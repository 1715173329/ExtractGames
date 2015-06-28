#include <Windows.h>
#include "types.h"
#include "error.h"
#include "resource.h"
#include "xp3.h"

#define THREAD_NUM 4

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

#define MESSAGE(x) MessageBox(0, x, TEXT("��ʾ"), MB_ICONINFORMATION|MB_OK)

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
	lstrcat(szBuffer, TEXT("\r\n"));
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
		hZlib = LoadLibrary(TEXT("zlib.dll"));
		if (!hZlib)
		{
			MESSAGE(TEXT("ȱ��zlib.dll�ļ���"));
			EndDialog(hDlg, 0);
		}
		
		if (!(tmp = (UNCOM)GetProcAddress(hZlib, "uncompress")))
		{
			MESSAGE(TEXT("���뺯����ȡʧ�ܣ�"));
			EndDialog(hDlg, 0);
		}
//----------------------------------------------------------
		hEdit = GetDlgItem(hDlg, IDC_EDIT);
		SendMessage(hEdit, EM_LIMITTEXT, -1, 0);
		AppendMsg(TEXT("ѡ���Ӧ��Ϸ���Ϸ�xp3�ļ����˴�..."));
//----------------------------------------------------------
		hCombo = GetDlgItem(hDlg, IDC_COMBO);
		for (int i=IDS_STRING100; i<=IDS_STRING107; ++i)	// ��Ϊ��Ӧ��Ϸ����
		{
			LoadString((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), i, szBuffer, MAX_PATH);
			SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)szBuffer);
		}
//----------------------------------------------------------
		for (int i=0; i<THREAD_NUM; ++i)
		{
			if (!(tp[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
			{
				AppendMsg(TEXT("�¼���ʼ������"));
				EndDialog(hDlg, 0);
			}
			if (!(tp[i].queue = (wchar_t**)VirtualAlloc(NULL, sizeof(wchar_t*), MEM_COMMIT, PAGE_READWRITE)))
			{
				AppendMsg(TEXT("�ڴ�������"));
				EndDialog(hDlg, 0);
			}
			if (!(*(tp[i].queue) = (wchar_t*)VirtualAlloc(NULL, tp[i].QUEUE_SIZE*MAX_PATH*sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE)))
			{
				AppendMsg(TEXT("�ڴ�������"));
				EndDialog(hDlg, 0);
			}
			if (!(tp[i].hThread = CreateThread(NULL, 0, Thread, &tp[i], 0, NULL)))
			{
				AppendMsg(TEXT("�̴߳���ʧ�ܣ�"));
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
				SetDlgItemText(hDlg, IDC_PAUSE, TEXT("��ͣ(&P)"));
			}
			else
			{
				for (int i=0; i<THREAD_NUM; ++i)
					SuspendThread(tp[i].hThread);
				SetDlgItemText(hDlg, IDC_PAUSE, TEXT("����(&R)"));
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
	PTSTR filter;
};

int callback(struct CB* pcb, PTSTR path)
{
	int len = lstrlen(path);
	while(len>=0 && path[len-1] != '.') --len;

	if (!pcb->filter || !lstrcmp(&path[len], pcb->filter))
	{
		while (pcb->ptp[pcb->cnt].front == pcb->ptp[pcb->cnt].tail+1)		// ��������ת��һ��
			pcb->cnt = (pcb->cnt+1) % THREAD_NUM;

		EnterCriticalSection(&cs);
		{
			lstrcpy((PTSTR)(*pcb->ptp[pcb->cnt].queue + pcb->ptp[pcb->cnt].tail*MAX_PATH), path);
		
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
//	static const DWORD MemAllocStep = 1024*MAX_PATH;
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
		do{
			// �����ļ���
			lstrcpy(lpFind, lpPath);
			lstrcat(lpFind, FindData.cFileName);

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

DWORD AppendFileToQueue(PTSTR pInBuf, CallBack callback, struct CB *pcb)
{	
	if (FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(pInBuf))
		ExpandDirectory(pInBuf, callback, pcb);
	else callback(pcb, pInBuf);

	return 0;
}

void OnDropFiles(HDROP hDrop, HWND hDlg, thread_param* ptp)
{
	struct CB cb;
	TCHAR FileName[MAX_PATH];
	char szBuffer[128];
	DWORD i;
	DWORD FileNum;

	cb.cnt	  = 0;
	cb.filter = 0;
	cb.ptp	  = ptp;

	u32 idx = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
	if (idx == CB_ERR)
	{
		MessageBox(hDlg, TEXT("����ѡ���Ӧ����Ϸ"), TEXT("��ʾ"), MB_ICONINFORMATION);
		return;
	}
	LoadStringA((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), idx+500, szBuffer, 128);
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
	wchar_t cur_dir[MAX_PATH], *CurrentFile;
	TCHAR szBuffer[MAX_PATH];
	thread_param *ptp = (thread_param*) pv;
	
	while (1)
	{
		WaitForSingleObject(ptp->hEvent, INFINITE);

		if (ptp->thread_exit) break;

		CurrentFile = *ptp->queue + ptp->front*MAX_PATH;

		lstrcpyW(cur_dir, CurrentFile);

		int l = lstrlenW(cur_dir);
		while(l && cur_dir[l-1] != '\\') --l;
		cur_dir[l] = '\0';

		lstrcatW(cur_dir, TEXT("[extract] "));
		lstrcatW(cur_dir, &CurrentFile[l]);
		CreateDirectory(cur_dir, 0);
		
		u32 file_num = 0;
		u32 idx_size = 0;
		u8 *uncompress_idx;
		do{
			hFile = CreateFile(CurrentFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
			if (hFile == INVALID_HANDLE_VALUE)
			{
				wsprintf(szBuffer, TEXT("�޷����ļ�, ����\r\n%s"), CurrentFile);
				AppendMsg(szBuffer);
				break;
			}

			if (!is_xp3_file(hFile))
			{
				wsprintf(szBuffer, TEXT("�����xp3�ļ�:%s"), CurrentFile);
				AppendMsg(szBuffer);
				break;
			}
						
			uncompress_idx = uncompress_xp3_idx(hFile, &idx_size, ptp->unCom);
			
			if (!uncompress_idx)
			{
				AppendMsg(TEXT("xp3������ȡʧ��"));
				break;
			}
			u32 save_file = xp3_extract_file_save(hFile, uncompress_idx, idx_size, &file_num,
												  ptp->ChooseGame, ptp->unCom, cur_dir);

			if (file_num == save_file)
			{
				wsprintf(szBuffer, TEXT("[��ȡ���(%d/%d)]%s"), save_file, save_file, CurrentFile);
				AppendMsg(szBuffer);
			}
			else
			{
				wsprintf(szBuffer, TEXT("��ȡ%d���ļ�����%d������%d����������\r\n%s"),
								save_file, file_num, file_num-save_file, CurrentFile);
				MessageBox(0, szBuffer, TEXT("��ʾ"), MB_ICONWARNING);
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