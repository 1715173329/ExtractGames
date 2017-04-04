
#include "tpk.h"

int wmain(int argc, wchar_t **argv)
{
    std::wstring tpkname, outdir;
    if (argc < 2)
    {
        printf("ʹ�÷�ʽ:\n    Tpk.exe <Tpk File Name> [<Output Directory>]\n");
        return 0;
    }
    if (argc >= 2)
        tpkname.assign(argv[1]);
    if (argc >= 3)
        outdir.assign(argv[2]);

    TpkOperator to;
    to.OutputDirectoy.assign(outdir);

    to.OpenTpk(tpkname);
    if (!to.IsValidTpk())
    {
        printf("%ws������Ч��Tpk�ļ�.\n", tpkname.c_str());
        return 0;
    }

    printf("������ȡ����...\n");

    to.GetFileEntries();
    to.ExtractFiles();

    printf("��ȡ���.\n");

#ifdef _DEBUG
    
    // ���⼸��
    unsigned long long offset = 0, id = 0;
    if (tpkname.find(L"client.tpk") != std::wstring::npos)
    {
        offset = to.GetEntryOffsetByFileName("init.cmp", &id);
        if (offset != 0xdebc0 || id != (unsigned long long)0x672fb441d6c0445e)
            throw "Papapa1";

        offset = to.GetEntryOffsetByFileName("main.cmp", &id);
        if (offset != 0x2338E0 || id != (unsigned long long)0x524a20aff1551307)
            throw "Papapa2";

        offset = to.GetEntryOffsetByFileName("client.cmp", &id);
        if (offset != 0x188850 || id != (unsigned long long)0xcfa562dc24b9d77a)
            throw "Papapa3";

        offset = to.GetEntryOffsetByFileName("scripts\\marriage.cmp", &id);
        if (offset != 0x7cc90 || id != (unsigned long long)0x0a31cb1444577897)
            throw "Papapa4";
    }

#endif // DEBUG    

    return 0;
}