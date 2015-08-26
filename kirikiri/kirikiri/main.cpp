#include <Windows.h>
#include "types.h"
#include "error.h"
#include "resource.h"
#include "xp3.h"
#include <strsafe.h>

const int THREAD_NUM = 4;

struct thread_param
{
	enum {QUEUE_SIZE = 1500};
	wchar_t **queue;
	DWORD front, tail;
	HANDLE hEvent;
	HANDLE hThread;
	char ChooseGame[32];
	UNCOM unCom;
	bool thread_exit;
};

HWND hEdit, hCombo;
HANDLE hThread;
CRITICAL_SECTION cs;

DWORD WINAPI Thread(PVOID pv);
void OnDropFiles(HDROP hDrop, HWND hwnd, thread_param* ptp);
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

#define MESSAGE(x) MessageBox(0, x, L"��ʾ", MB_ICONINFORMATION|MB_OK)

void AppendMsg(const wchar_t *szBuffer)
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
	SendMessage(hEdit, EM_GETSEL, 0, (LPARAM)&dwPos);
	return;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInstance,
					PSTR pCmdLine, int iCmdShow)
{
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, DlgProc, 0);
	return 0;
}

BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static thread_param tp[THREAD_NUM];
	static HMODULE hZlib;
	static bool thread_paused;
	UNCOM tmp = 0;
	TCHAR szBuffer[MAX_PATH];

	switch (msg)
	{
	case WM_INITDIALOG:
		hZlib = LoadLibrary(L"zlib.dll");
		if (!hZlib)
		{
			MESSAGE(L"ȱ��zlib.dll�ļ���");
			EndDialog(hDlg, 0);
		}
		
		if (!(tmp = (UNCOM)GetProcAddress(hZlib, "uncompress")))
		{
			MESSAGE(L"���뺯����ȡʧ�ܣ�");
			EndDialog(hDlg, 0);
		}
//----------------------------------------------------------
		hEdit = GetDlgItem(hDlg, IDC_EDIT);
		SendMessage(hEdit, EM_LIMITTEXT, -1, 0);
		AppendMsg(L"ѡ���Ӧ��Ϸ���Ϸ�xp3�ļ����˴�...\r\n");
//----------------------------------------------------------
		hCombo = GetDlgItem(hDlg, IDC_COMBO);
		for (int i=IDS_STRING099; i<=IDS_STRING116; ++i)	// ��Ϊ��Ӧ��Ϸ(�ַ���)����
		{
			LoadString((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), i, szBuffer, MAX_PATH);
			SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)szBuffer);
		}
//----------------------------------------------------------
		for (int i=0; i<THREAD_NUM; ++i)
		{
			if (!(tp[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
			{
				AppendMsg(L"�¼���ʼ������\r\n");
				EndDialog(hDlg, 0);
			}
			if (!(tp[i].queue = (wchar_t**)VirtualAlloc(NULL, sizeof(wchar_t*), MEM_COMMIT, PAGE_READWRITE)))
			{
				AppendMsg(L"�ڴ�������\r\n");
				EndDialog(hDlg, 0);
			}
			if (!(*(tp[i].queue) = (wchar_t*)VirtualAlloc(NULL, tp[i].QUEUE_SIZE*MAX_PATH*sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE)))
			{
				AppendMsg(L"�ڴ�������\r\n");
				EndDialog(hDlg, 0);
			}
			if (!(tp[i].hThread = CreateThread(NULL, 0, Thread, &tp[i], 0, NULL)))
			{
				AppendMsg(L"�̴߳���ʧ�ܣ�\r\n");
				EndDialog(hDlg, 0);
			}
			tp[i].front = tp[i].tail = 0;
			tp[i].thread_exit = false;
			tp[i].unCom = tmp;
		}
		InitializeCriticalSection(&cs);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_PAUSE)
		{
			if (thread_paused)
			{
				for (int i=0; i<THREAD_NUM; ++i)
					ResumeThread(tp[i].hThread);
				SetDlgItemText(hDlg, IDC_PAUSE, L"��ͣ(&P)");
			}
			else
			{
				for (int i=0; i<THREAD_NUM; ++i)
					SuspendThread(tp[i].hThread);
				SetDlgItemText(hDlg, IDC_PAUSE, L"����(&R)");
			}
			thread_paused ^= 1;
		}
		return TRUE;
				
	case WM_DROPFILES:
		OnDropFiles((HDROP)wParam, hDlg, tp);
		return TRUE;

	case WM_CLOSE:
		for (int i=0; i<THREAD_NUM; ++i)
		{
			tp[i].thread_exit = true;
			SetEvent(tp[i].hEvent);
		}
		FreeLibrary(hZlib);
		EndDialog(hDlg, 0);
		return TRUE;
	}
	return FALSE;
}

typedef int (*CallBack)(struct CB* pcb, PTSTR path);

struct CB
{
	int cnt;
	thread_param* ptp;
	wchar_t *filter;
};

int callback(struct CB* pcb, wchar_t *path)
{
	int len = wcslen(path);
	while(len>=0 && path[len-1] != '.') --len;

	if (!pcb->filter || !wcscmp(&path[len], pcb->filter))
	{
		while (pcb->ptp[pcb->cnt].front == pcb->ptp[pcb->cnt].tail+1)		// ��������ת��һ��
			pcb->cnt = (pcb->cnt+1) % THREAD_NUM;

		EnterCriticalSection(&cs);
		{
			StringCchCopy(*pcb->ptp[pcb->cnt].queue + pcb->ptp[pcb->cnt].tail*MAX_PATH, MAX_PATH, path);
		
			if (pcb->ptp[pcb->cnt].tail == pcb->ptp[pcb->cnt].front)		// ԭ�ȶ���Ϊ�գ���λ
				SetEvent(pcb->ptp[pcb->cnt].hEvent);

			pcb->ptp[pcb->cnt].tail = (pcb->ptp[pcb->cnt].tail + 1) % pcb->ptp[pcb->cnt].QUEUE_SIZE;// ���¶���
		}
		LeaveCriticalSection(&cs);

		pcb->cnt = (pcb->cnt+1) % THREAD_NUM;	// ת��һ���߳�
	}
	return 0;
}

int ExpandDirectory(PTSTR lpszPath, CallBack callback, struct CB* pcb)
{
	wchar_t			lpFind[MAX_PATH], lpSearch[MAX_PATH], lpPath[MAX_PATH];
	HANDLE			hFindFile;
	WIN32_FIND_DATA FindData;
	int				cnt = 0;

	// Path\*.*
	StringCchCopy(lpPath,   MAX_PATH, lpszPath);
	StringCchCat (lpPath,   MAX_PATH, L"\\");
	StringCchCopy(lpSearch, MAX_PATH, lpPath);
	StringCchCat (lpSearch, MAX_PATH, L"*.*");

	if (INVALID_HANDLE_VALUE != (hFindFile = FindFirstFile(lpSearch, &FindData)))
	{
		do{
			// �����ļ���
			StringCchCopy(lpFind, MAX_PATH, lpPath);
			StringCchCat(lpFind, MAX_PATH, FindData.cFileName);

			if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (FindData.cFileName[0] != '.')
					ExpandDirectory(lpFind, callback, pcb);
			}
			else callback(pcb, lpFind);
		}while(FindNextFile(hFindFile, &FindData));
		FindClose(hFindFile);
		return 0;
	}
	return -2;
}

DWORD AppendFileToQueue(wchar_t *pInBuf, CallBack callback, struct CB *pcb)
{	
	if (FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(pInBuf))
		ExpandDirectory(pInBuf, callback, pcb);
	else callback(pcb, pInBuf);

	return 0;
}

void OnDropFiles(HDROP hDrop, HWND hDlg, thread_param* ptp)
{
	struct CB cb;
	wchar_t FileName[MAX_PATH];
	char szBuffer[128];
	DWORD i, FileNum;

	cb.cnt	  = 0;
	cb.filter = 0;
	cb.ptp	  = ptp;

	u32 idx = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
	if (idx == CB_ERR)
	{
		MessageBox(hDlg, L"����ѡ���Ӧ����Ϸ", L"��ʾ", MB_ICONINFORMATION);
		return;
	}
	LoadStringA((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), idx+499, szBuffer, 128);
	for (int i=0; i<THREAD_NUM; ++i)
		strcpy(ptp[i].ChooseGame, szBuffer);

	FileNum  = DragQueryFile(hDrop, -1, NULL, 0);

	for (i=0; i<FileNum; ++i)
	{
		DragQueryFile(hDrop, i, (LPTSTR)FileName, MAX_PATH);
		AppendFileToQueue(FileName, callback, &cb);
	}
	DragFinish(hDrop);

	return;
}

DWORD WINAPI Thread(PVOID pv)
{
	DWORD dwNowProcess = 0;
	HANDLE hFile;
	wchar_t cur_dir[MAX_PATH], szBuffer[MAX_PATH], *CurrentFile;
	thread_param *ptp = (thread_param*) pv;
	
	while (1)
	{
		WaitForSingleObject(ptp->hEvent, INFINITE);

		if (ptp->thread_exit) break;

		CurrentFile = *ptp->queue + ptp->front*MAX_PATH;

		StringCchCopy(cur_dir, MAX_PATH, CurrentFile);

		DWORD l = wcslen(cur_dir);
		while(l && cur_dir[l-1] != '\\') --l;
		cur_dir[l] = '\0';

		StringCchCat(cur_dir, MAX_PATH, L"[extract] ");
		StringCchCat(cur_dir, MAX_PATH, &CurrentFile[l]);
		CreateDirectory(cur_dir, 0);
		
		u32 file_num = 0;
		u32 idx_size = 0;
		u8 *uncompress_idx;
		do{
			hFile = CreateFile(CurrentFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
			if (hFile == INVALID_HANDLE_VALUE)
			{
				StringCchPrintf(szBuffer, MAX_PATH, L"�޷����ļ�, ����\r\n%s\r\n", CurrentFile);
				AppendMsg(szBuffer);
				break;
			}

			if (!is_xp3_file(hFile))
			{
				StringCchPrintf(szBuffer, MAX_PATH, L"�����xp3�ļ�:%s\r\n", CurrentFile);
				AppendMsg(szBuffer);
				break;
			}
						
			uncompress_idx = uncompress_xp3_idx(hFile, &idx_size, ptp->unCom);
			
			if (!uncompress_idx)
			{
				AppendMsg(L"xp3������ȡʧ��\r\n");
				break;
			}
			u32 save_file = xp3_extract_file_save(hFile, uncompress_idx, idx_size, &file_num,
												  ptp->ChooseGame, ptp->unCom, cur_dir);

			if (file_num == save_file)
			{
				StringCchPrintf(szBuffer, MAX_PATH, 
						L"[��ȡ���(%d/%d)]%s\r\n", save_file, save_file, CurrentFile);
				AppendMsg(szBuffer);
			}
			else
			{
				StringCchPrintf(szBuffer, MAX_PATH, L"��ȡ%d���ļ�����%d������%d����������\r\n%s\r\n",
								save_file, file_num, file_num-save_file, CurrentFile);
				MessageBox(0, szBuffer, L"��ʾ", MB_ICONWARNING);
			}
		}while(0);

		if (uncompress_idx)
		{
			VirtualFree(uncompress_idx, idx_size, MEM_DECOMMIT);
			VirtualFree(uncompress_idx, 0, MEM_RELEASE);
		}
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

		EnterCriticalSection(&cs);
		{
			ptp->front = (ptp->front + 1) % ptp->QUEUE_SIZE;
		
			if (ptp->front == ptp->tail)
				ResetEvent(ptp->hEvent);
		}
		LeaveCriticalSection(&cs);
	}
	return 0;
}