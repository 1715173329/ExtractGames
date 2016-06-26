
#pragma once

#include <vector>
#include "FileOperator.h"


class TpkOperator
{
public:
    struct TpkHeader
    {
        char Magic[8];
        U32 IdxBaseSize;
    };

#pragma pack(4)
    struct TpkEntry
    {
        U64 Id;               // У����������
        U64 Offset;
        U32 PkgLen;           // ��������û��ѹ�������ߵ�ֵ����ͬ��
        U32 RawLen;
        U32 OffsetToMainBody; // ���ݿ�ǰ���ļ������ȼ��ļ�����ռ�ֽ�
        U32 Flag;             // 0x80000000, δѹ��
        U32 Crc32;
    };
#pragma pack()

    enum {
        Data_Compress_Flag = 0x80000000,
        Magic_Entry = 0x28,
        Magic_ID_1 = 0x7fed7fed,
        Magic_ID_2 = 0xeeeeeeee,

        TpkEncryptionBlock_Offset_1 = 0x400,    // in bytes 
        TpkEncryptionBlock_Offset_2 = 0x800,    // in bytes
    };

    static const char TpkHeaderMagic[8];
    static const unsigned long TpkEncryptionBlock[0x500];

    std::wstring OutputDirectoy;

public:
    TpkOperator();
    ~TpkOperator();

    bool OpenTpk(std::wstring filename);
    bool IsValidTpk();
    bool GetFileEntries();
    bool ExtractFiles();
    bool CheckData(PU8 data, U32 crc);
    void CloseTpk();

    void CreatePrepare(char *filename, const std::wstring & directory, std::wstring & fn);

    // ��Ϸ��ͨ���ļ��������ļ���������λ��(���㷨), ������¼, ��ȻҲ�����õ�
    U64 GetEntryOffsetByFileName(const char *filename, PU64 pId);

private:
    ReadOperator rfile;
    WriteOperator wfile;

    std::vector<TpkEntry> mEntries;
    U32 IndexBaseSize;
};