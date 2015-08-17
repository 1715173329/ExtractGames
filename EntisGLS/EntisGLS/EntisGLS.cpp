#include <Windows.h>
#include <strsafe.h>
#include <vector>
#include <string>
#include <iostream>
#include "EntisGLS.h"

using std::string;
using std::vector;

static const int MAXPATH = 350;

int SplitFileNameAndSave(const wchar_t *cur_dir, const wchar_t *file_name, void* unpack, unsigned long file_length)
{
	DWORD ByteWrite;
	wchar_t buf[MAXPATH] = {0}, buf2[MAXPATH];

	if (!unpack)
	{
		StringCchPrintf(buf2, MAXPATH, L"[������Ϊ�գ���ֹд��]%s\r\n", file_name);
		AppendMsg(buf2);
		return -4;
	}

	StringCchCopy(buf, MAXPATH, cur_dir);
	StringCchCat(buf, MAXPATH, L"\\");
	StringCchCat(buf, MAXPATH, file_name);

	int len = lstrlenW(buf);
	int i = lstrlenW(cur_dir) + 1;
	wchar_t *p = buf, *end = buf + len;
	while (p <= end && i < len)
	{
		while(buf[i] != '\\' && buf[i] != '/' && buf[i] != '\0') ++i;
	
		if (i<len)
		{
			wchar_t tmp = buf[i];
			buf[i] = '\0';

			CreateDirectoryW(p, 0);
			buf[i] = tmp;
			++i;
			p = buf + i;
		}
	}

	HANDLE hFile;
	int ret = 0;
	do{
		hFile = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			StringCchPrintf(buf2, MAXPATH, L"[�ļ���������]%s\r\n", file_name);
			ret = -1;
			break;
		}

		WriteFile(hFile, unpack, file_length, &ByteWrite, NULL);

		if (ByteWrite != file_length)
		{
			StringCchPrintf(buf2, MAXPATH, L"[�ļ�д�����]%s\r\n", file_name);
			ret = -2;
			break;
		}
		int t = GetLastError();
		if (!t || t == ERROR_ALREADY_EXISTS)
			StringCchPrintf(buf2, MAXPATH, L"[�ѱ���]%s\r\n", file_name);
		else
		{
			StringCchPrintf(buf2, MAXPATH, L"[�޷�����]%s,������%d\r\n", file_name, GetLastError());
			ret = -3;
		}
	}while(0);

	AppendMsg(buf2);
	CloseHandle(hFile);
	return ret;
}

bool IsEntisArchiveFile(PBYTE buf)
{
	return !memcmp(buf, "Entis\x1A\x00\x00\x00\x04\x00\x02\x00\x00\x00\x00", 16);
}

int GetPackageIndex(HANDLE hPack, vector<FILEENTRY>& Index, char *path)
{
	DWORD R;
	NOADIRENTRY nd;
	QWORD StartOffset = SetFilePointer(hPack, 0, 0, FILE_CURRENT);

	ReadFile(hPack, &nd, sizeof(nd), &R, 0);
	if (memcmp(nd.DirEntryMagic, "DirEntry", 8))
	{
		AppendMsg(L"DirEntry��־����\r\n");
		return -1;
	}

	PBYTE Idx = (PBYTE)VirtualAlloc(NULL, (DWORD)nd.IndexLen, MEM_COMMIT, PAGE_READWRITE);
	if (!Idx)
	{
		AppendMsg(L"�ڴ��޷�����\r\n");
		return -2;
	}

	ReadFile(hPack, Idx, (DWORD)nd.IndexLen, &R, 0);

	DWORD IdxNum = *(PDWORD)Idx;
	PBYTE p		 = Idx + 4;
	int ret = 0;
	for (DWORD i=0; i<IdxNum; ++i)
	{
		FILEENTRY fe;
		char PathBuf[MAXPATH];
		fe.qBytes = *(QWORD*)p;
		p += 8;
		fe.nAttribute = *(PDWORD)p;
		p += 4;
		fe.nEncodeType = *(PDWORD)p;
		p += 4;
		fe.qOffset = *(QWORD*)p + StartOffset;	// ����ƫ��
		p += 8;
		p += 8;		// ����ʱ���
		fe.nExtraInfoLen = *(PDWORD)p;
		p += 4;
		if (fe.nExtraInfoLen)
		{
			if (!(fe.pExtraInfo = new BYTE[fe.nExtraInfoLen]))
			{
				AppendMsg(L"�ڴ��޷�����\r\n");
				return -2;
			}
		} else fe.pExtraInfo = 0;
		p += fe.nExtraInfoLen;
		StringCchCopyA(PathBuf, MAXPATH, path);
		StringCchCatA(PathBuf, MAXPATH, (char*)(p+4));
		MultiByteToWideChar(932, 0, PathBuf, MAXPATH, fe.wName, MAXPATH);
		p += 4 + *(PDWORD)p;

		if (fe.nAttribute == attrDirectory)
		{
			LONG OffsetHign = (LONG)(fe.qOffset >> 32);
			NOAFILEDATA nfd;
			SetFilePointer(hPack, (DWORD)fe.qOffset, &OffsetHign, FILE_BEGIN);
			ReadFile(hPack, &nfd, sizeof(nfd), (PDWORD)&OffsetHign, 0);

			ret = GetPackageIndex(hPack, Index, PathBuf);
			if (ret) break;
		} else if (fe.nAttribute == attrEndOfDirectory) {
			AppendMsg(L"attrEndOfDirectory!\r\n"); break;
		} else if (fe.nAttribute == attrNextDirectory) {
			AppendMsg(L"attrNextDirectory!\r\n"); break;
		}
		else
			Index.push_back(fe);
	}
	VirtualFree(Idx, 0, MEM_RELEASE);
	return ret;
}
const BYTE RemoteCode [10] = {0x64, 0xa1, 0x30, 0x0, 0x0, 0x0, 0x8b, 0x40, 0x08, 0xcc};
const BYTE RemoteINT3 = 0xcc;
/*
int __declspec(naked) RemoteCode()
{
	__asm{
		mov eax, fs:[0x30]
		mov eax, dword ptr [eax+0x8]
		_emit 0xcc
	}
}*/
bool FindPassword(HANDLE hp, PVOID ImgBase, PVOID SearchEnd, string& out)
{
	DWORD R;
	MEMORY_BASIC_INFORMATION mbi;
	BYTE Buf[0x1000];

	PBYTE p = (PBYTE)ImgBase + 0x1000;
	for (;p < SearchEnd; p += 0x1000)
	{
		VirtualQueryEx(hp, p, &mbi, 0x1000);
		if (mbi.State != MEM_COMMIT)
			continue;
		ReadProcessMemory(hp, p, Buf, 0x1000, &R);

		DWORD i = 0;
		for (i=0; i<0x1000-14; ++i)
			if (!memcmp(Buf+i, "<archive path=", 14))
			{
				string str = (char*)(Buf+i);
				DWORD begin = 0, end;
				while (string::npos != (begin = str.find("<archive path=", begin)))
				{
					end = str.find("/>", begin);
					if (end == string::npos)
						break;

					out += string(str.begin()+begin, str.begin()+end+2) + "\r\n";
					begin = end+1;
				}
				if (out.size())
				{
					out.push_back('\0');
					return true;
				}
			}
	}
	return false;
}

int GetGamePassword(string& Password)
{
	DWORD R;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	OPENFILENAME ofn = {0};
	DEBUG_EVENT de;
	wchar_t ExeName[MAXPATH] = {0};
	//----------------------------------------------------------------------------
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize	= sizeof(ofn);
	ofn.hwndOwner	= 0;
	ofn.lpstrFilter	= L"��ִ���ļ�(*.exe)\0*.exe\0\0";
	ofn.lpstrFile	= ExeName;
	ofn.nMaxFile	= MAXPATH;
	ofn.Flags		= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	GetOpenFileName(&ofn);
	//----------------------------------------------------------------------------
	DWORD StartTime, ImageBase;				// exe�Ļ�ַ
	HMODULE hKernel, hUser;					// kernel32��user32�ľ��
	BYTE cOriByteLR, cOriByteFW;			// ����FindResourceA��FindWindowA���������ֽ�
	FARPROC fnFindRes = 0, fnFindWin = 0;	// ���������ĵ�ַ
	int dwTimes = -1, ret = 0;
	bool bResLoaded = false;				// ȷ���Ƿ��Ѽ���xml��Դ
	CONTEXT ctx, BackUp;
	PBYTE pResIDAddr;						// ��ʱ����ָ���ӽ��̶�������ԴID��ַ
	char ResName[128];

	if (ExeName[0])
	{
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);
		if (!CreateProcess(0, ExeName, 0, 0, 0, DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | NORMAL_PRIORITY_CLASS, 0, 0, &si, &pi))
		{
			MessageBox(0, L"���̴���ʧ��", 0, MB_ICONERROR);
			return -1;
		}
		BackUp.ContextFlags = ctx.ContextFlags = CONTEXT_FULL;
		StartTime = GetTickCount();			// ��¼ʱ���Ա㳬ʱ�˳�
		while (1)
		{
			WaitForDebugEvent(&de, 10000);	// 10s
			if (GetTickCount() - StartTime > 10000)
				break;
			if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
			{
				++dwTimes;
				if (!dwTimes)	// ��1�Σ�����dll�����öϵ�
				{
					hKernel = GetModuleHandle(L"kernel32.dll");
					hUser = GetModuleHandle(L"user32.dll");
					if (hKernel == INVALID_HANDLE_VALUE || hUser == INVALID_HANDLE_VALUE)
					{
						ret = -2;
						break;
					}
					if (!(fnFindRes = GetProcAddress(hKernel, "FindResourceA")) ||
						!(fnFindWin = GetProcAddress(hUser, "FindWindowA")))
					{
						ret = -2;
						break;
					}
					ReadProcessMemory(pi.hProcess, fnFindRes, &cOriByteLR, 1, &R);
					ReadProcessMemory(pi.hProcess, fnFindWin, &cOriByteFW, 1, &R);

					WriteProcessMemory(pi.hProcess, fnFindRes, &RemoteINT3, 1, &R);
					WriteProcessMemory(pi.hProcess, fnFindWin, &RemoteINT3, 1, &R);

				} else if (dwTimes == -1) {	// FindWindow��ȷ�Ͼ�������ȡexe��ַ��׼������key
					GetThreadContext(pi.hThread, &ctx);
					ImageBase = ctx.Eax;
					SetThreadContext(pi.hThread, &BackUp);

					if (FindPassword(pi.hProcess, (PVOID)ImageBase, hKernel, Password))
						break;

				} else {
					if (!bResLoaded)
					{	// ȷ���Ƿ��Ѽ���xml��Դ
						
						GetThreadContext(pi.hThread, &ctx);
						ReadProcessMemory(pi.hProcess, (PVOID)(ctx.Esp + 8), &pResIDAddr, 4, &R);
						ReadProcessMemory(pi.hProcess, pResIDAddr, ResName, 128, &R);

						--ctx.Eip;
						if (!strcmp(ResName, "IDR_COTOMI"))
							bResLoaded = true;
						else
						{ // ���صĲ���xml��Դ���򵥲��������öϵ�
							ctx.EFlags |= 0x100;
						}
						WriteProcessMemory(pi.hProcess, fnFindRes, &cOriByteLR, 1, &R);
						SetThreadContext(pi.hThread, &ctx);
					}
					else
					{ // xml��Դ�Ѿ������룬׼����ȡ
						GetThreadContext(pi.hThread, &BackUp);
						--BackUp.Eip;
						WriteProcessMemory(pi.hProcess, fnFindWin, &cOriByteFW, 1, &R);

						PVOID RemoteMem = VirtualAllocEx(pi.hProcess, NULL, 10, MEM_COMMIT, PAGE_READWRITE);
						if (!RemoteMem) return false;
						DWORD WW = WriteProcessMemory(pi.hProcess, RemoteMem, (PBYTE)RemoteCode, 10, &R);
						ctx = BackUp;
						ctx.Eip = (DWORD)RemoteMem;
						SetThreadContext(pi.hThread, &ctx);
						dwTimes = -2;
					}
				}
			} else if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP) {
					WriteProcessMemory(pi.hProcess, fnFindRes, &RemoteINT3, 1, &R);
			} else if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
				break;
			ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
		} // while (1)
	} // if (ExeName[0])
	TerminateProcess(pi.hProcess, 0);
	
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return ret;
}

int Enterence(const wchar_t *PackName, const wchar_t *CurDir)
{
	DWORD R, FileSaveNum = 0;
	string Password;
	wchar_t MsgBuf[MAXPATH];
	HANDLE hPack = CreateFile(PackName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hPack == INVALID_HANDLE_VALUE)
	{
		AppendMsg(L"�޷����ļ�\r\n");
		return -1;
	}
	NOAHEADER nh;
	ReadFile(hPack, &nh, sizeof(nh), &R, 0);

	if (!IsEntisArchiveFile((PBYTE)(&nh)))
	{
		StringCchPrintf(MsgBuf, MAXPATH, L"%s����EntisGLS��ʽ�ļ�\r\n", PackName);
		AppendMsg(MsgBuf);
		return -3;
	}

	vector<FILEENTRY> Index;
	if (GetPackageIndex(hPack, Index, ""))
	{
		StringCchPrintf(MsgBuf, MAXPATH, L"%s�ļ�Ŀ¼����\r\n", PackName);
		AppendMsg(MsgBuf);
		return -4;
	}

	// �����ļ�������ȡ�ļ����ݲ����ж�Ӧ����
	for (DWORD i=0; i<Index.size(); ++i)
	{
		NOAFILEDATA nf;
		LONG Hign = (LONG)(Index[i].qOffset >> 32);
		SetFilePointer(hPack, (DWORD)Index[i].qOffset, &Hign, FILE_BEGIN);
		// ����֮ǰ��һ��filedata�ṹ
		ReadFile(hPack, &nf, sizeof(nf), &R, 0);
		if (memcmp(nf.FileDataMagic, "filedata", 8))
		{
			AppendMsg(L"filedata��־��������\r\n");
			continue;
		}
		PBYTE Untreated = (PBYTE)VirtualAlloc(NULL, (DWORD)nf.FileRecordLen, MEM_COMMIT, PAGE_READWRITE);
		if (!Untreated)
		{
			AppendMsg(L"�ڴ��޷�����\r\n");
			return -2;
		}

		ReadFile(hPack, Untreated, (DWORD)nf.FileRecordLen, &R, 0);

		PBYTE DataToSave = 0;
		switch (Index[i].nEncodeType)
		{
		case encodeRaw:
			DataToSave = Untreated;
			break;

		case etBSHFCrypt:
			StringCchPrintf(MsgBuf, MAXPATH, L"%s���ļ����ݼ��ܣ���ѡ����Ϸ��exe�ļ���ȡ����\r\n", PackName);
			if (IDYES == MessageBox(0, MsgBuf, L"��ʾ", MB_YESNO | MB_ICONINFORMATION))
			{
				if (!GetGamePassword(Password))
				{
					char *Psw = new char[Password.size()];
					wchar_t *PswUni = new wchar_t[Password.size()+MAXPATH];
					if (!Psw || !PswUni)
					{
						AppendMsg(L"�ڴ��޷�����\r\n");
						return -2;
					}
					for (DWORD i=0; i<Password.size(); ++i)
						Psw[i] = Password[i];
					MultiByteToWideChar(932, 0, Psw, Password.size(), PswUni, Password.size()+MAXPATH);
					StringCchCat(PswUni, Password.size()+MAXPATH,
							L"\nkey=���漴Ϊ��Ӧ���������(��������)����Ctrl+C���Ƶ������壬�������ͽ���Noa32�����¡�Player��");
					MessageBox(0, PswUni, L"����Get��chu", MB_ICONINFORMATION);
					delete[] Psw;
					delete[] PswUni;
				} else {
					MessageBox(0, L"�޷���ȡ������Դ", L"������", MB_ICONWARNING);
				}
				return 0;
			}
			else
			{
				StringCchPrintf(MsgBuf, MAXPATH, L"%s���ļ����ݼ��ܣ���ȡ��ֹ\r\n", PackName);
				AppendMsg(MsgBuf);
				return -3;
			}
			break;

		case encodeERISA:
		case encodeCrypt32:
		case encodeERISACrypt32:
			AppendMsg(L"�ļ�������Ҫ���룬����Noa32�����¡�Player������ȡ\r\n");
			break;

		default:
			AppendMsg(L"δ֪��������\r\n");
			break;
		}

		if (!SplitFileNameAndSave(CurDir, Index[i].wName, DataToSave, (DWORD)Index[i].qBytes))
			++FileSaveNum;
		VirtualFree(Untreated, 0, MEM_RELEASE);
	}

	if (FileSaveNum == Index.size())
	{
		StringCchPrintf(MsgBuf, MAXPATH, L"[��ȡ���(%d/%d)] %s\r\n", FileSaveNum, Index.size(), PackName);
		AppendMsg(MsgBuf);
	} else {
		StringCchPrintf(MsgBuf, MAXPATH, L"[��ȡ����(%d/%d)] %s\r\n��%d���ļ���ȡʧ��",
							FileSaveNum, Index.size(), PackName, Index.size() - FileSaveNum);
		MessageBox(0, MsgBuf, 0, MB_ICONWARNING);
	}
	return 0;
}