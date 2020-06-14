#include <Windows.h>
#include <strsafe.h>
#include "resource.h"
#include "QLIE.h"

#define MAXPATH 350

#define THREADNUM 1		// ���̶߳����ƺ�������

HWND hEdit;
CRITICAL_SECTION cs;

struct thread_param
{
    enum {QUEUE_SIZE = 1500};
    wchar_t **queue;
    DWORD front, tail;
    HANDLE hEvent;
    HANDLE hThread;
    bool thread_exit;
};

wchar_t g_ExePath[MAX_PATH];
wchar_t g_KeyPath[MAX_PATH];

// ����
DWORD WINAPI Thread(PVOID pv);
BOOL CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern int Entrance(const wchar_t *CurDir, const wchar_t *PackName);




int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR pCmdLine, int iCmdShow)
{
    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, DlgProc, 0);
    return 0;
}

void AppendMsg(const wchar_t *szBuffer)
{
    static DWORD dwPos;
    if (0 == szBuffer)
    {
        dwPos = 0;
        SendMessage(hEdit, EM_SETSEL, 0, -1);
        SendMessage(hEdit, EM_REPLACESEL, FALSE, 0);
    } else {
        SendMessage(hEdit, EM_SETSEL, (WPARAM)&dwPos, (LPARAM)&dwPos);
        SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)szBuffer);
        SendMessage(hEdit, EM_GETSEL, 0, (LPARAM)&dwPos);
    }
    return;
}

int mycmp(const wchar_t* src, const wchar_t* dst)
{
    int i = 0;
    while (src[i]) tolower(src[i++]);
    return wcscmp(src, dst);
}
//////////////////////////////////////////////////////////////////////////////////
// ����չ����Ŀ¼
// lpszPath		 - ��չ��Ŀ¼
// callback		 - �ص�����
// pcb			 - �ص���������
//////////////////////////////////////////////////////////////////////////////////
typedef int (*CallBack)(struct CB* pcb, wchar_t* path);

struct CB
{
    int cnt;
    thread_param* ptp;
    wchar_t* filter;
};

int callback(struct CB* pcb, wchar_t* path)
{
    int len = lstrlen(path);
    while(len>=0 && path[len-1] != '.') --len;
    if (!pcb->filter || !wcscmp(&path[len], pcb->filter))
    {
        while (pcb->ptp[pcb->cnt].front == pcb->ptp[pcb->cnt].tail+1)		// ��������ת��һ��
            pcb->cnt = (pcb->cnt+1) % THREADNUM;

        EnterCriticalSection(&cs);
        {
            StringCchCopy(*pcb->ptp[pcb->cnt].queue + pcb->ptp[pcb->cnt].tail*MAXPATH, MAXPATH, path);

            if (pcb->ptp[pcb->cnt].tail == pcb->ptp[pcb->cnt].front)		// ԭ�ȶ���Ϊ�գ���λ
                SetEvent(pcb->ptp[pcb->cnt].hEvent);

            pcb->ptp[pcb->cnt].tail = (pcb->ptp[pcb->cnt].tail + 1) % thread_param::QUEUE_SIZE;// ���¶���
        }
        LeaveCriticalSection(&cs);

        pcb->cnt = (pcb->cnt+1) % THREADNUM;	// ת��һ���߳�
    }
    return 0;
}

int ExpandDirectory(PTSTR lpszPath, CallBack callback, struct CB* pcb)
{
    wchar_t			lpFind[MAXPATH], lpSearch[MAXPATH], lpPath[MAXPATH];
    HANDLE			hFindFile;
    WIN32_FIND_DATA FindData;
    int				cnt = 0;

    // Path\*.*
    StringCchCopy(lpPath, MAXPATH, lpszPath);
    StringCchCat(lpPath, MAXPATH, L"\\");
    StringCchCopy(lpSearch, MAXPATH, lpPath);
    StringCchCat(lpSearch, MAXPATH, L"*.*");

    if (INVALID_HANDLE_VALUE != (hFindFile = FindFirstFile(lpSearch, &FindData)))
    {
        do
        {
            // �����ļ���
            StringCchCopy(lpFind, MAXPATH, lpPath);
            StringCchCat(lpFind, MAXPATH, FindData.cFileName);

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
}

void OnDropFiles(HDROP hDrop, HWND hDlg, thread_param* ptp)
{
    struct CB cb;
    TCHAR FileName[MAXPATH];
    DWORD i;
    DWORD FileNum;

    cb.cnt = 0;
    cb.filter = 0;
    cb.ptp = ptp;


    GetDlgItemText(hDlg, IDC_EXEPATH, g_ExePath, MAX_PATH);
    GetDlgItemText(hDlg, IDC_KEYPATH, g_KeyPath, MAX_PATH);

    if (!g_ExePath[0] && !g_KeyPath[0])
    {
        AppendMsg(L"·�����Ϸ�\r\n");
        return;
    }

    FileNum  = DragQueryFile(hDrop, -1, NULL, 0);

    for (i=0; i<FileNum; ++i)
    {
        DragQueryFile(hDrop, i, (LPTSTR)FileName, MAXPATH);
        AppendFileToQueue(FileName, callback, &cb);
    }
    DragFinish(hDrop);

    return;
}

BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static thread_param tp[4];

    switch (msg)
    {
    case WM_INITDIALOG:
        SendMessage(hDlg, WM_SETICON, ICON_BIG,
                    (LPARAM)LoadIcon((HINSTANCE)GetWindowLong(hDlg, GWL_HINSTANCE), MAKEINTRESOURCE(IDI_ICON2)));

        hEdit = GetDlgItem(hDlg, IDC_EDIT);
        SendMessage(hEdit, EM_LIMITTEXT, -1, 0);
        AppendMsg(TEXT("�Ϸŷ���ļ����˴�...\r\n��ע�⡿�ļ�·����С��260���ַ�\r\n"));


        for (int i=0; i<THREADNUM; ++i)
        {
            if (!(tp[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
            {
                AppendMsg(TEXT("�¼���ʼ������"));
                EndDialog(hDlg, 0);
            }
            if (!(tp[i].queue = (wchar_t**)VirtualAlloc(NULL, sizeof(wchar_t**), MEM_COMMIT, PAGE_READWRITE)))
            {
                AppendMsg(TEXT("�ڴ�������"));
                EndDialog(hDlg, 0);
            }
            if (!(*(tp[i].queue) = (wchar_t*)VirtualAlloc(NULL, thread_param::QUEUE_SIZE * MAXPATH * sizeof(wchar_t),
                                                                MEM_COMMIT, PAGE_READWRITE)))
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
        }
        InitializeCriticalSection(&cs);
        return TRUE;

    case WM_DROPFILES:
        OnDropFiles((HDROP)wParam, hDlg, tp);
        return TRUE;

    case WM_CLOSE:
        for (int i=0; i<THREADNUM; ++i)
        {
            tp[i].thread_exit = true;
            SetEvent(tp[i].hEvent);
        }
        Sleep(200);
        EndDialog(hDlg, 0);
        return TRUE;
    }
    return FALSE;
}

DWORD WINAPI Thread(PVOID pv)
{
    wchar_t cur_dir[MAXPATH], *CurrentFile;
    DWORD dwNowProcess = 0;
    thread_param * ptp = (thread_param*) pv;

    while (1)
    {
        WaitForSingleObject(ptp->hEvent, INFINITE);

        if (ptp->thread_exit) break;

        CurrentFile = (wchar_t*)(*ptp->queue + ptp->front * MAXPATH);

        StringCchCopy(cur_dir, MAXPATH, CurrentFile);

        int l = wcslen(cur_dir);
        while(l && cur_dir[l-1] != '\\') --l;
        cur_dir[l] = '\0';

        StringCchCat(cur_dir, MAXPATH, L"[extract] ");
        StringCchCat(cur_dir, MAXPATH, &CurrentFile[l]);
        CreateDirectory(cur_dir, 0);

        if (wcslen(g_KeyPath) != 0)
        {
            Mangekyoo_1_2_3 m;
            m.Entrance(cur_dir, CurrentFile);
        }
        else
        {
            Mangekyoo_4_5 m;
            m.Entrance(cur_dir, CurrentFile);
        }

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