#include <Windows.h>
#include <strsafe.h>

struct PNA_HEADER
{
	char magic[4];
	DWORD unkonwn1;
	DWORD unkonwn2;
	DWORD unkonwn3;
	DWORD file_num_fake;
	DWORD next_type;
};

struct PNA_IDX
{
	int file_serial;
	int h_offset;
	int v_offset;
	int width;
	int height;
	int unknown1;
	int unknown2;
	int unknown3;
	int file_size;
	int next_type;	// 00:�¸����������ļ� 01:�¸�����Ч��FFFF 02:�پ���һ��0x24��idx(FFFF)����
};

const char pos_title[] = "ID\th_offset\tv_offset\twidth\theight\r\n";
const char pos_format[] = "%d\t\t\t%d\t\t\t%d\t\t\t%d\t\t\t%d\r\n";

extern void AppendMsg(const wchar_t *Msg);

bool IsPnaFile(const unsigned char *Data)
{
	if (*(unsigned long*)Data == 0x50414e50)	// "PNAP"
		return true;
	return false;
}

int ExtractPNAPFile(unsigned char *Data, const wchar_t *CurDir, const wchar_t *FileName)
{
	DWORD dwFileProcessed = 0, ByteRead;
	wchar_t Format[MAX_PATH], szBuffer[MAX_PATH];

	// �����ļ���
	StringCchCopy(Format, MAX_PATH, CurDir);
	StringCchCat (Format, MAX_PATH, L"\\");
	StringCchCat (Format, MAX_PATH, FileName);

	int llen = lstrlen(Format);
	while(llen && Format[llen] != '.') --llen;
	Format[llen] = '\0';

	StringCchCat(Format, MAX_PATH, TEXT("_%02d.png"));


	// ������Ϣ
	struct PNA_HEADER *pph = (struct PNA_HEADER*)Data;
	struct PNA_IDX *pna_idx = (struct PNA_IDX*)(Data + sizeof(struct PNA_HEADER)), *p;
	DWORD  png_ptr = sizeof(PNA_HEADER) + sizeof(PNA_IDX) * pph->file_num_fake - 4;


	// ����������txt�ļ�
	wchar_t TxtName[MAX_PATH];							
	StringCchCopy(TxtName, MAX_PATH, CurDir);
	StringCchCat (TxtName, MAX_PATH, L"\\");
	StringCchCat (TxtName, MAX_PATH, FileName);
	TxtName[llen+1] = TxtName[llen+3] = 't'; TxtName[llen+2] = 'x';


	// д�����
	HANDLE hIdx = CreateFile(TxtName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	WriteFile(hIdx, pos_title, sizeof(pos_title)/sizeof(pos_title[0]) - 1, &ByteRead, 0);		// ����\0����д

	p = pna_idx;
	for (DWORD i=0; i<pph->file_num_fake; ++i, ++p)
	{
		if (p->file_serial == -1) continue;

		// д��ͼƬ��ƫ�Ƶ���Ϣ��txt
		StringCbPrintfA((char*)szBuffer, MAX_PATH, pos_format,
						p->file_serial, p->h_offset, p->v_offset, p->width, p->height);
		WriteFile(hIdx, szBuffer, strlen((char*)szBuffer), &ByteRead, 0);


		// ͼƬ����
		PBYTE FileBuf = (PBYTE)((PBYTE)Data + png_ptr);

		StringCchPrintf(szBuffer, MAX_PATH, Format, p->file_serial);
		HANDLE hSave = CreateFile(szBuffer, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		WriteFile(hSave, FileBuf, p->file_size, &ByteRead, 0);
		CloseHandle(hSave);

		++dwFileProcessed;


		png_ptr += p->file_size;
	}
	CloseHandle(hIdx);

	StringCchPrintf(szBuffer, MAX_PATH, L"[�������(%d)] %s\r\n", dwFileProcessed, FileName);
	AppendMsg(szBuffer);

	return 0;
}
