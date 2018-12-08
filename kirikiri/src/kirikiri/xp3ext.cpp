
#include "xp3ext.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

bool kuranokunchi::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    for (size_t i = 0; i<unpackData.size(); ++i)
        unpackData[i] ^= (uint8_t)fe.checksum ^ 0xcd;
    return true;
}

bool amakoi::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    for (size_t i = 0; i<unpackData.size(); ++i)
        unpackData[i] ^= (uint8_t)fe.checksum;
    return true;
}

bool prettycation::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    for (size_t i = 5; i<unpackData.size(); ++i)
        unpackData[i] ^= (uint8_t)(fe.checksum >> 0xc);
    return true;
}

bool lovelycation::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    char* buf = unpackData.data();

    uint8_t key[5];
    key[0] = (uint8_t)(fe.checksum >> 8) & 0xff;
    key[1] = (uint8_t)(fe.checksum >> 8) & 0xff;
    key[2] = (uint8_t)(fe.checksum >> 1) & 0xff;
    key[3] = (uint8_t)(fe.checksum >> 7) & 0xff;
    key[4] = (uint8_t)(fe.checksum >> 5) & 0xff;

    for (size_t i = 0; i <= 0x64; ++i)
    {
        *buf++ ^= key[4];
    }
    for (size_t i = 0x65; i<unpackData.size(); ++i)
    {
        *buf++ ^= key[i & 4];
    }
    return true;
}

bool swansong::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    uint8_t ror = (uint8_t)fe.checksum & 7;
    uint8_t key = (uint8_t)(fe.checksum >> 8);
    for (size_t i = 0; i<unpackData.size(); ++i)
    {
        unpackData[i] ^= key;
        unpackData[i] = unpackData[i] >> ror | unpackData[i] << (8 - ror);
    }
    return true;
}

bool deai5bu::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    uint32_t key = 0x35353535;

    size_t dwordLen = unpackData.size() >> 2;
    uint32_t* ptr = (uint32_t*)unpackData.data();
    for (size_t i = 0; i<dwordLen; ++i)
        *ptr++ ^= key;

    size_t remain = unpackData.size() - (dwordLen << 2);
    for (; remain != 0; --remain)
    {
        unpackData[(dwordLen << 2) + remain - 1] ^= (uint8_t)(key >> (remain - 1));
    }
    return true;
}

bool kamiyabai::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    uint32_t key = 0xcdcdcdcd;

    size_t dwordLen = unpackData.size() >> 2;
    uint32_t* ptr = (uint32_t*)unpackData.data();
    for (size_t i = 0; i<dwordLen; ++i)
        *ptr++ ^= key;

    size_t remain = unpackData.size() - (dwordLen << 2);
    for (; remain != 0; --remain)
    {
        unpackData[(dwordLen << 2) + remain - 1] ^= (uint8_t)(key >> (remain - 1));
    }
    return true;
}

bool colorfulcure::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    xp3filter_decode("colorfulcure", fe.file_name.c_str(), (uint8_t*)unpackData.data(), unpackData.size(), 0, unpackData.size(), fe.checksum);
    return true;
}

bool sakurasaki::DoExtractData(const file_entry & fe, std::vector<char>& unpackData)
{
    xp3filter_decode("sakurasaki", fe.file_name.c_str(), (uint8_t*)unpackData.data(), unpackData.size(), 0, unpackData.size(), fe.checksum);
    return true;
}



///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////






void XP3Entrance(const wchar_t *packName, const wchar_t *curDirectory, const std::wstring& choosedGame)
{
    DWORD idx_size = 0;

    auto fucker = CreateXP3Handler(choosedGame);
    if (!fucker)
    {
        AppendMsg(wstring(L"�޷��������Ϸ[" + choosedGame + L"]\r\n").c_str());
        return;
    }

    bool success = fucker->Open(packName);
    assert(success);
    if (!success)
    {
        delete fucker;
        return;
    }

    wstring txt(packName);
    txt += L"_entries.txt";
    auto rawBytes = fucker->GetPlainIndexBytes();
    ofstream idx(txt + L".0.txt");
    assert(idx.is_open());
    idx.write(rawBytes.data(), rawBytes.size());
    idx.close();

    auto entries = fucker->ExtractEntries(rawBytes);
    fucker->DumpEntriesToFile(entries, txt.c_str());
    int saveFileCount = fucker->ExtractData(entries, curDirectory, wcout);
    fucker->Close();
    delete fucker;

    if (entries.size() == saveFileCount)
    {
        wstringstream wss;
        wss << L"[��ȡ���(" << saveFileCount << L"/" << saveFileCount << L")]" << packName << "\r\n";
        AppendMsg(wss.str().c_str());
    }
    else
    {
        wstringstream wss;
        wss << L"��ȡ" << saveFileCount << L"���ļ�����" << entries.size() << L"������"
            << entries.size() - saveFileCount << L"����������\r\n" << packName << L"\r\n";
        MessageBoxW(0, wss.str().c_str(), L"��ʾ", MB_ICONWARNING);
    }
}

std::map<std::wstring, GameInfomation> GameNameMap = {
    { L"<Ĭ�ϣ�ֱ����ȡ>",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"�����顢�D���ޤ�����",{ "sakurasaki", []() { return new sakurasaki; } } },
    { L"�}Ұ������Τդ�������",{ "kuranokunchi", []() { return new kuranokunchi; } } },
    { L"����������åץ� ���u���餦���Ĥǥ������ʤ���񘔤������ꡫ",{ "amakoi", []() { return new amakoi; } } },
    { L"���ߥĥ� ��7�ζ��\��˼�h��",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"�����`�餤�ȡ異���ɥ� -COLORFUL TOP STAGE��-",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"���ޤ��餹��ɥ륹���`",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"����ե��夢�� �ͷ���Ů",{ "colorfulcure", []() { return new colorfulcure; } } },
    { L"�Уңţԣԣ١��áģԣɣϣ�",{ "prettycation", []() { return new prettycation; } } },
    { L"your diary ��H",{ "kuranokunchi", []() { return new kuranokunchi; } } },
    { L"�Ԥ���2�ˤȥ������Τ��٤�",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"�Ԥ���2�ˤȥ������Τ��٤� LOVEHEAVEN300��",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"���ޤ��� �������äϥӥ��ӥ��ӥå���",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"���@�������饷�ߥ��`�� ver.MAKO",{ "Unencrypted", []() { return new EncryptedXP3; } } },
    { L"PRETTY��CATION2",{ "prettycation", []() { return new prettycation; } } },
    { L"������ΤƤƤ����֤Θ��Ӥ���������",{ "anioka", []() { return nullptr; } } },
    { L"SWANSONG",{ "swansong", []() { return new swansong; } } },
    { L"������������@�ɤ�",{ "koisakura", []() { return nullptr; } } },
    { L"���äȤ������� �������󤹤�����",{ "sukisuki", []() { return nullptr; } } },
    { L"����5�ˤμޤ��󤬥�֥�֤ʤΤϡ�δ�����餭��������Τ��������`���ʤ�����",{ "oreaka", []() { return nullptr; } } },
    { L"�äΥ�������(δ���)",{ "seiiki", []() { return nullptr; } } },
    { L"���ȥᣪ�ɥᥤ��",{ "Otomedomain", []() { return nullptr; } } },
    { L"LOVELY��CATION2",{ "lovelycation", []() { return new lovelycation; } } },
    { L"����ä�5�֤ϰ��Τ�Σ� �r�gֹͣ�Ȳ��ɱܤ��\��",{ "deai5bu", []() { return new deai5bu; } } },
    { L"���m�ߤ������ư���δ������Ф���",{ "kamiyabai", []() { return new kamiyabai; } } },
    { L"9-nine-�����ΤĤ����Τ������Τ���",{ "palette 9-nine", []() { return new palette_9_nine; } } },
};

EncryptedXP3 * CreateXP3Handler(const std::wstring & gameName)
{
    //static map<string, std::function<EncryptedXP3*()>> list{
    //    { "kuranokunchi",   []() { return new kuranokunchi; } },
    //    { "amakoi",         []() { return new amakoi;       } },
    //    { "prettycation",   []() { return new prettycation; } },
    //    { "lovelycation",   []() { return new lovelycation; } },
    //    { "swansong",       []() { return new swansong;     } },
    //    { "deai5bu",        []() { return new deai5bu;      } },
    //    { "kamiyabai",      []() { return new kamiyabai;    } },

    //    // cxdec
    //    { "colorfulcure",   []() { return new colorfulcure; } },
    //};

    //auto it = list.find(gameName);
    //if (it != list.end())
    //    return it->second();
    //else
    //    return new EncryptedXP3;

    auto it = GameNameMap.find(gameName);
    assert(it != GameNameMap.end());
    return it->second.Handler();
}
