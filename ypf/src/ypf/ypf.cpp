
#include <cassert>
#include "ypf.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <iostream>

using namespace std;

bool YPF::Open(const std::string & path)
{
    assert(mPath.empty());
    assert(!mStream.is_open());

    mStream.open(path, ios::binary);
    if (!mStream.is_open())
    {
        cout << "���ļ�" << path << "ʧ��\n";
        return false;
    }

    mStream.read((char*)&mHeader, sizeof(mHeader));
    if (mHeader.mMagic != YpfHeader::MAGIC)
    {
        cout << "������Ч�� YPF ���\n";
        mStream.close();
        return false;
    }

    mPath = path;
    return true;
}

void YPF::Close()
{
    mStream.close();
    mPath.clear();
}

bool YPF::ExtractEntries()
{
    if (mPath.empty())
    {
        cout << "δ�򿪷���ļ�\n";
        return false;
    }

    string packetDir;
    
    assert(mStream.is_open());
    vector<char> data(mHeader.mDataStart - sizeof(mHeader));
    mStream.seekg(sizeof(YpfHeader), ios::beg);
    mStream.read(data.data(), data.size());
    vector<char> dataInv(data.begin(), data.end());
    for (char& c : dataInv) c = ~c;

    mEntries.clear();
    size_t dataIndex = 0;
    while (dataIndex < data.size() && mEntries.size() < mHeader.mFileCount)
    {
        NormalizedEntry e;

        dataIndex += 5;
        uint32_t old = dataIndex;
        while (dataIndex < dataInv.size() && dataInv[dataIndex] != '.')
            ++dataIndex;
        dataIndex += 4;        // ��׺��
        e.filename = string(&dataInv[old], &dataInv[dataIndex]);

        if (PathFindExtension(e.filename.c_str()) == e.filename.c_str() + e.filename.size())
        {
            cout << "�����ļ�" << e.filename << "ʱ��������\n";
            return false;
        }

        if (packetDir.empty())
        {
            packetDir = e.filename.substr(0, e.filename.find('\\'));
        }
        else
        {
            if (strncmp(packetDir.c_str(), e.filename.c_str(), packetDir.size()))
            {
                cout << "�����ļ�" << e.filename << "ʱ��������\n";
                return false;
            }
        }

        e.filetype = data[dataIndex];
        ++dataIndex;
        e.zlib_compressed = data[dataIndex] == 1;
        ++dataIndex;
        e.original_length = *(uint32_t*)&data[dataIndex];
        dataIndex += 4;
        e.compressd_length = *(uint32_t*)&data[dataIndex];
        dataIndex += 4;
        e.offset = *(uint32_t*)&data[dataIndex];
        dataIndex += 4;
        dataIndex += 4;
        
        mEntries.push_back(e);
    }

    if (dataIndex != data.size() || mEntries.size() != mHeader.mFileCount)
    {
        cout << "�в����ļ���ȡʧ��(" << mEntries.size() << "/" << mHeader.mFileCount << ")\n";
        return false;
    }
    else
    {
        return true;
    }
}

bool YPF::ExtractResource(const std::string & saveDir)
{
    if (mPath.empty())
    {
        cout << "δ�򿪷���ļ�\n";
        return false;
    }

    if (mEntries.empty())
    {
        cout << "û����Ч���ļ��б�\n";
        return false;
    }

    if (saveDir.empty())
    {
        cout << "δָ������Ŀ¼\n";
        return false;
    }
    
    typedef int(*UNCOMPRESS)(char* dst, uint32_t* dstLength, char* src, uint32_t srcLength);

    HMODULE hZlib = LoadLibrary("zlib.dll");
    if (!hZlib)
    {
        cout << "zlib.dll not found\n";
        return false;
    }
    UNCOMPRESS Uncompress = (UNCOMPRESS)GetProcAddress(hZlib, "uncompress");
    if (!Uncompress)
    {
        cout << "uncompress function not found in zlib.dll\n";
        CloseHandle(hZlib);
        return false;
    }

    assert(saveDir.back() == '\\');
    size_t saved = 0;
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        vector<char> packed(mEntries[i].compressd_length);
        vector<char> unpacked(mEntries[i].original_length);
        mStream.seekg(mEntries[i].offset);
        mStream.read(packed.data(), mEntries[i].compressd_length);

        if (mEntries[i].zlib_compressed)
        {
            size_t dstLength = unpacked.size();
            int ret = Uncompress(unpacked.data(), &dstLength, packed.data(), packed.size());
            if (ret)
            {
                cout << "zlib ��ѹ" << mEntries[i].filename << "ʧ��\n";
                continue;
            }
        }
        else
        {
            unpacked.assign(packed.begin(), packed.end());
        }


        string fullPath = ConvJPToChs(saveDir + mEntries[i].filename);
        string folder = GetFolderOfPath(fullPath);
        int ret = SHCreateDirectoryExA(NULL, folder.c_str(), NULL);
        if (ret != ERROR_SUCCESS && ret != ERROR_ALREADY_EXISTS)
        {
            cout << "����Ŀ¼" << folder << "ʧ��\n";
            continue;
        }
        ofstream save(fullPath, ios::binary);
        if (!save.is_open())
        {
            cout << "�����ļ�" << fullPath << "ʧ��\n";
            continue;
        }
        save.write(unpacked.data(), unpacked.size());
        save.close();
        cout << fullPath << "�ѱ���\n";
        ++saved;
    }

    cout << "�����ȡ(" << saved << "/" << mHeader.mFileCount << ")\n";
    FreeLibrary(hZlib);
    return true;
}

std::string YPF::GetFolderOfPath(const std::string & fullPath)
{
    vector<char> tmp;
    if (PathIsRelative(fullPath.c_str()))
    {
        tmp.resize(GetFullPathName(fullPath.c_str(), 0, NULL, NULL));
        GetFullPathName(fullPath.c_str(), tmp.size(), tmp.data(), NULL);
    }
    else
    {
        tmp.assign(fullPath.begin(), fullPath.end());
        tmp.push_back('\0');
    }
    if (PathRemoveFileSpecA(tmp.data()))
        return string(tmp.data()) + "\\";
    else
        return std::string();
}

std::string YPF::ConvJPToChs(const std::string & str)
{
    vector<wchar_t> wc(MultiByteToWideChar(932, 0, str.c_str(), -1, NULL, 0));
    MultiByteToWideChar(932, 0, str.c_str(), -1, wc.data(), wc.size());

    vector<char> mcb(WideCharToMultiByte(CP_ACP, 0, wc.data(), -1, NULL, 0, NULL, NULL));
    WideCharToMultiByte(CP_ACP, 0, wc.data(), -1, mcb.data(), mcb.size(), NULL, NULL);
    return mcb.data();
}
