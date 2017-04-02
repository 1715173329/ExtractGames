#include <Windows.h>
#include "resource.h"
#include "pfs.h"
#include <assert.h>

HWND hEdit;

struct thread_param
{
	enum {ARR_SIZE = 0x10};
	PTSTR *file_name;
	int file_num;
	bool thread_run;
};

DWORD WINAPI Thread(PVOID pv);
BOOL CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInstance,
					PSTR pCmdLine, int iCmdShow)
{
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, DlgProc, 0);
	return 0;
}

void AppendMsg(PTSTR szBuffer)
{
	static DWORD dwPos;
	if (0 == szBuffer)
	{
		dwPos = 0;
		SendMessage(hEdit, EM_SETSEL, 0, -1);
		SendMessage(hEdit, EM_REPLACESEL, FALSE, 0);
		return;
	}
	SendMessage(hEdit, EM_SETSEL, (WPARAM)&dwPos, (LPARAM)&dwPos);
	SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)szBuffer);
	SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)TEXT("\r\n"));
	SendMessage(hEdit, EM_GETSEL, 0, (LPARAM)&dwPos);
	return;
}

int mycmp(PTSTR src, PTSTR dst)
{
	int i = 0;
	while (src[i]) tolower(src[i++]);
	return lstrcmp(src, dst);
}
//////////////////////////////////////////////////////////////////////////////////
// ����չ����Ŀ¼
// lpszPath		 - ��չ��Ŀ¼
// callback		 - �ص�����
// pcb			 - �ص���������
//////////////////////////////////////////////////////////////////////////////////
typedef int (*CallBack)(struct CB* pcb, PTSTR path);

struct CB
{
	thread_param* ptp;
};

int callback(struct CB* pcb, PTSTR path)
{
	if (pcb->ptp->file_num < pcb->ptp->ARR_SIZE)
	{
		lstrcpy((PTSTR)((PBYTE)pcb->ptp->file_name + pcb->ptp->file_num*MAX_PATH), path);
		++pcb->ptp->file_num;
	}
	else
	{
		TCHAR msg[64];
		wsprintf(msg, TEXT("һ���Ϸ�����%d���ļ���"), pcb->ptp->ARR_SIZE);
		AppendMsg(msg);
	}
	return 0;
}
/*
int ExpandDirectory(PTSTR lpszPath, CallBack callback, struct CB* pcb)
{
	static const DWORD MemAllocStep = 1024*MAX_PATH;
	TCHAR			lpFind[MAX_PATH], lpSearch[MAX_PATH], lpPath[MAX_PATH];
	HANDLE			hFindFile;
	WIN32_FIND_DATA FindData;
	int				cnt = 0;

	// Path\*.*
	lstrcpy(lpPath, lpszPath);
	lstrcat(lpPath, TEXT("\\"));
	lstrcpy(lpSearch, lpPath);
	lstrcat(lpSearch, TEXT("*.*"));

	if (INVALID_HANDLE_VALUE != (hFindFile = FindFirstFile(lpSearch, &FindData)))
	{
		do
		{
			// �����ļ���
			lstrcpy(lpFind, lpPath);
			lstrcat(lpFind, FindData.cFileName);

			if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (FindData.cFileName[0] != '.')
					ExpandDirectory(lpFind, callback, pcb);
			}
			else
			{
				callback(pcb, lpFind);
			}
		}while(FindNextFile(hFindFile, &FindData));
		FindClose(hFindFile);
		return 0;
	}
	return -2;
}

//////////////////////////////////////////////////////////////////////////////////
// ���pInBuf(�������ļ�)/pInBuf�µ��������ļ�(������Ŀ¼)������
// pInBuf		 - ������ļ�(չ��Ŀ¼)
// callback		 - �ص�����
// pcb			 - �ص���������
//////////////////////////////////////////////////////////////////////////////////
DWORD AppendFileToQueue(PTSTR pInBuf, CallBack callback, struct CB *pcb)
{	
	if (FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(pInBuf))
	{
		ExpandDirectory(pInBuf, callback, pcb);
	}
	else
	{
		callback(pcb, pInBuf);
	}
	return 0;
}*/

void OnDropFiles(HDROP hDrop, thread_param* ptp)
{
	struct CB cb;
	TCHAR FileName[MAX_PATH];
	DWORD i;
	DWORD FileNum;

	cb.ptp = ptp;

	FileNum  = DragQueryFile(hDrop, -1, NULL, 0);

	for (i=0; i<FileNum; ++i)
	{
		DragQueryFile(hDrop, i, (LPTSTR)FileName, MAX_PATH);
		if (FILE_ATTRIBUTE_DIRECTORY != GetFileAttributes(FileName))
			callback(&cb, FileName);
	}
	DragFinish(hDrop);

	if (!ptp->thread_run)
	{
		CreateThread(NULL, 0, Thread, (PVOID)ptp, 0, NULL);
		ptp->thread_run = true;
	}
	else
		MessageBox(0, TEXT("�����"), TEXT("��ʾ"), MB_ICONINFORMATION);

	return;
}

BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static thread_param tp;

	switch (msg)
	{
	case WM_INITDIALOG:
		hEdit = GetDlgItem(hDlg, IDC_EDIT);
		SendMessage(hEdit, EM_LIMITTEXT, -1, 0);
		AppendMsg(TEXT("�Ϸ��ļ����˴�..."));

		tp.file_num = 0;
		if (!(tp.file_name = (PTSTR*)VirtualAlloc(NULL, sizeof(PTSTR*), MEM_COMMIT, PAGE_READWRITE)))
		{
			AppendMsg(TEXT("�ڴ�������"));
			EndDialog(hDlg, 0);
		}
		if (!(*(tp.file_name) = (PTSTR)VirtualAlloc(NULL, tp.ARR_SIZE*MAX_PATH, MEM_COMMIT, PAGE_READWRITE)))
		{
			AppendMsg(TEXT("�ڴ�������"));
			EndDialog(hDlg, 0);
		}

		return TRUE;

	case WM_DROPFILES:
		OnDropFiles((HDROP)wParam, &tp);
		return TRUE;

	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;
	}
	return FALSE;
}

int SplitFileNameAndSave(PTSTR cur_dir, PCTSTR file_name, PVOID unpack, DWORD file_length)
{
	int i = 0;
	int len = lstrlen(file_name);
	DWORD ByteWrite;
	TCHAR buf[MAX_PATH], buf2[MAX_PATH], curdir[MAX_PATH];

	GetCurrentDirectory(MAX_PATH, curdir);
	lstrcpy(buf, file_name);
	LPTSTR p = buf, end = buf + len;
	while (p <= end && i < len)
	{
		while(buf[i] != '\\' && buf[i] != '/' && buf[i] != '\0') ++i;
	
		if (i<len)
		{
			buf[i++] = '\0';

			CreateDirectory(p, 0);
//			int err = GetLastError();
			SetCurrentDirectory(p);
//			err = GetLastError();
			p = buf + i;
		}
	}
	HANDLE hFile = CreateFile(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		goto SaveErr;

	WriteFile(hFile, unpack, file_length, &ByteWrite, NULL);
	if (ByteWrite != file_length)
	{
		wsprintf(buf2, TEXT("[�ļ�д�����]%s"), p);
		goto Append;
	}
	if (!GetLastError())
		wsprintf(buf2, TEXT("[�ѱ���]%s"), p);
	else
SaveErr:
	wsprintf(buf2, TEXT("[�޷�����]%s"), p);
Append:
	AppendMsg(buf2);
	CloseHandle(hFile);

	SetCurrentDirectory(curdir);
	return 1;
}


bool ReadPackgeEntriesV1(HANDLE hFile, std::vector<PFXEntry> & vecEntries)
{
    DWORD ReadBytes;
    BOOL  bSuccess;
    PFXHeaderV1 Header;
    LARGE_INTEGER liFileSize;

    liFileSize.LowPart = GetFileSize(hFile, (PDWORD)&liFileSize.HighPart);
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    bSuccess = ReadFile(hFile, &Header, sizeof(PFXHeaderV1), &ReadBytes, NULL);
    assert(bSuccess);

    if (Header.FileNumber == 0 || Header.RelationFileDataOffset > liFileSize.QuadPart)
    {
        return false;
    }

    unsigned long auBuffer[4];
    char Buffer[MAX_PATH];
    wchar_t wBuffer[MAX_PATH];
    // ˳�����¶�ȡ
    for (size_t i = 0; i < Header.FileNumber; ++i)
    {
        PFXEntry Entry;
        unsigned long uNameLen;
        bSuccess = ReadFile(hFile, &uNameLen, sizeof(unsigned long), &ReadBytes, NULL);
        assert(bSuccess);
        assert(uNameLen < MAX_PATH);

        memset(Buffer, 0, sizeof(Buffer));
        memset(wBuffer, 0, sizeof(wBuffer));
        bSuccess = ReadFile(hFile, Buffer, uNameLen, &ReadBytes, NULL);
        assert(bSuccess);
        MultiByteToWideChar(932, 0, Buffer, uNameLen, wBuffer, MAX_PATH);   // 	shift_jis
        Entry.FileName.assign(wBuffer);

        bSuccess = ReadFile(hFile, auBuffer, 12, &ReadBytes, NULL);
        assert(bSuccess);
        if (auBuffer[0] != 0x10 || auBuffer[1] != 0x0 || auBuffer[1] != 0x0)
        {
            vecEntries.clear();
            return false;
        }

        bSuccess = ReadFile(
            hFile, &Entry.Offset, sizeof(unsigned long), &ReadBytes, NULL
        );
        assert(bSuccess);

        bSuccess = ReadFile(
            hFile, &Entry.Length, sizeof(unsigned long), &ReadBytes, NULL
        );
        assert(bSuccess);
        if (Entry.Offset + Entry.Length > liFileSize.QuadPart)
        {
            assert(0);
            vecEntries.clear();
            return false;
        }

        vecEntries.push_back(Entry);
    }
    return true;
}

bool ReadPackgeEntriesV2(HANDLE hFile, std::vector<PFXEntry> & vecEntries)
{
    DWORD ReadBytes;
    BOOL  bSuccess;
    PFXHeaderV2 Header;
    LARGE_INTEGER liFileSize;

    liFileSize.LowPart = GetFileSize(hFile, (PDWORD)&liFileSize.HighPart);
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    bSuccess = ReadFile(hFile, &Header, sizeof(PFXHeaderV2), &ReadBytes, NULL);
    assert(bSuccess);

    if (Header.FileNumber == 0 || Header.RelationFileDataOffset > liFileSize.QuadPart)
    {
        return false;
    }

    unsigned long uBuffer;
    char Buffer[MAX_PATH];
    wchar_t wBuffer[MAX_PATH];
    // ˳�����¶�ȡ
    for (size_t i = 0; i < Header.FileNumber; ++i)
    {
        unsigned long uNameLen;
        PFXEntry Entry;
        bSuccess = ReadFile(hFile, &uNameLen, sizeof(unsigned long), &ReadBytes, NULL);
        assert(bSuccess);
        assert(uNameLen < MAX_PATH);

        memset(Buffer, 0, sizeof(Buffer));
        memset(wBuffer, 0, sizeof(wBuffer));
        bSuccess = ReadFile(hFile, Buffer, uNameLen, &ReadBytes, NULL);
        assert(bSuccess);
        MultiByteToWideChar(932, 0, Buffer, uNameLen, wBuffer, MAX_PATH);   // 	shift_jis
        Entry.FileName.assign(wBuffer);

        bSuccess = ReadFile(hFile, &uBuffer, sizeof(unsigned long), &ReadBytes, NULL);
        assert(bSuccess);
        if (uBuffer != 0x0)
        {
            vecEntries.clear();
            return false;
        }

        bSuccess = ReadFile(hFile, &Entry.Offset, sizeof(unsigned long), &ReadBytes, NULL);
        assert(bSuccess);

        bSuccess = ReadFile(hFile, &Entry.Length, sizeof(unsigned long), &ReadBytes, NULL);
        assert(bSuccess);
        
        if (Entry.Offset + Entry.Length > liFileSize.QuadPart)
        {
            assert(0);
            vecEntries.clear();
            return false;
        }

        vecEntries.push_back(Entry);
    }
    return true;
}

int ExtractDataViaEntry(HANDLE hFile, std::vector<PFXEntry> & vecEntries, PWSTR strCurDir)
{
    DWORD ReadBytes, iExtractNum = 0;

    for (size_t i = 0; i < vecEntries.size(); ++i)
    {
        PFXEntry & Entry = vecEntries[i];
        void *pData = VirtualAlloc(NULL, Entry.Length, MEM_COMMIT, PAGE_READWRITE);
        SetFilePointer(hFile, Entry.Offset, NULL, FILE_BEGIN);
        ReadFile(hFile, pData, Entry.Length, &ReadBytes, NULL);
        assert(ReadBytes == Entry.Length);
        if (SplitFileNameAndSave(strCurDir, Entry.FileName.c_str(), pData, Entry.Length))
            ++iExtractNum;
        VirtualFree(pData, 0, MEM_RELEASE);
    }
    return iExtractNum;
}

DWORD WINAPI Thread(PVOID pv)
{
	HANDLE hFile;
	TCHAR szBuffer[MAX_PATH], cur_dir[MAX_PATH];
	LPTSTR CurrentFile;
	thread_param * ptp = (thread_param*) pv;
	DWORD dwNowProcess = 0;

	while (dwNowProcess < ptp->file_num)
	{
		CurrentFile = (PTSTR)((PBYTE)ptp->file_name + dwNowProcess*MAX_PATH);

		lstrcpy(cur_dir, CurrentFile);

		int llen = lstrlen(cur_dir);
		while(llen && CurrentFile[llen] != '\\') --llen;
		cur_dir[llen] = '\0';

		lstrcat(cur_dir, TEXT("\\[extract] "));
		lstrcat(cur_dir, CurrentFile + llen + 1);
		CreateDirectory(cur_dir, 0);
		SetCurrentDirectory(cur_dir);

        ///////////////////////////////////////////////////////////////////
        // ���

		hFile = CreateFile(CurrentFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			wsprintf(szBuffer, TEXT("�޷����ļ�, ����\r\n%s"), CurrentFile);
			AppendMsg(szBuffer);
			++dwNowProcess;
			continue;
		}

        std::vector<PFXEntry> vecEntries;
        bool bSuccess = false;
        for (int i = 0; i < sizeof(PFXEntryExtracterMap); ++i)
        {
            if (bSuccess = (PFXEntryExtracterMap[i])(hFile, vecEntries))
                break;
        }
        if (!bSuccess)
        {
			wsprintf(szBuffer, TEXT("�ļ���ʽ����, ����\r\n%s"), CurrentFile);
			AppendMsg(szBuffer);
        }
        else
        {
            ExtractDataViaEntry(hFile, vecEntries, cur_dir);
        }
		CloseHandle(hFile);
		++dwNowProcess;
    }
    return 0;
}
