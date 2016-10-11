
#include <algorithm>
#include <assert.h>
#include <locale>
#include <math.h>
#include "merge2.h"
#include <strsafe.h>


Merge::PictureInfo::PictureInfo()
{
    id = 0;
    x = 0;
    y = 0;
    width = 0;
    height = 0;
    mPixels = nullptr;
    mLines = nullptr;
}

Merge::PictureInfo::PictureInfo(const PictureInfo & pi)
{
    id = pi.id;
    x = pi.x;
    y = pi.y;
    width = pi.width;
    height = pi.height;
    mLines = nullptr;
    mPixels = nullptr;
        
    if (pi.mPixels)
    {
        int size = width * height * sizeof(COLORREF);
        mPixels = (COLORREF*)malloc(size);
        memcpy_s(mPixels, size, pi.mPixels, size);

        // ������
        size = height * sizeof(COLORREF*);
        mLines = (COLORREF**)malloc(size);
        for (int i = 0; i < height; ++i)
            mLines[i] = &mPixels[(height - 1 - i) * width];
    }
}

Merge::PictureInfo & Merge::PictureInfo::operator=(const PictureInfo & pi)
{
    if (this != &pi)
    {
        id = pi.id;
        x = pi.x;
        y = pi.y;
        width = pi.width;
        height = pi.height;
        mLines = nullptr;
        mPixels = nullptr;

        if (pi.mLines)
        {
            int size = height * sizeof(COLORREF*);
            mLines = (COLORREF**)malloc(size);
            memcpy_s(mLines, size, pi.mLines, size);
        }

        if (pi.mPixels)
        {
            int size = width * height * sizeof(COLORREF);
            mPixels = (COLORREF*)malloc(size);
            memcpy_s(mPixels, size, pi.mPixels, size);
        }
    }    
    return *this;
}

Merge::PictureInfo::~PictureInfo()
{
    if (mPixels)
        free((void *)mPixels);
    if (mLines)
        free((void *)mLines);
}

Merge::Merge()
{
    mGroupInitialized = false;

    setlocale(LC_ALL, "");
}

Merge::~Merge()
{

}

bool Merge::Initialize(const wchar_t *txtfilename, int groupnum, std::vector<int> &group)
{
    bool result = true;

    do
    {
        mGroupInitialized = false;
        
        // ��ȡ������Ϣ
        mGroup.assign(group.begin(), group.end());
        mInfo.resize(groupnum);

        // ��ȡtxt�ļ�
        result = LoadFileList(txtfilename);
        if (!result)
            break;
        
        // ��ȡpng�ļ���Ϣ
        // ȥ��txt��׺��
        mTxtFileName.assign(txtfilename);
        std::size_t pos = mTxtFileName.rfind(L'.');
        mTxtFileName.assign(mTxtFileName.substr(0, pos));
        
        for (int i = 0; i<mInfo.size(); ++i)
        {
            for (int k = 0; k<mInfo[i].size(); ++k)
            {
                wchar_t tmp[32];
                StringCchPrintf(tmp, _countof(tmp), L"%02d.png", mInfo[i][k].id);
                std::wstring name(mTxtFileName);
                name += L'_';
                name += tmp;
                //StringCchPrintf(name, _countof(name), L"%s_%02d.png", mTxtFileName, mInfo[i][k].id);
                result = OpenPng(name.c_str(), &mInfo[i][k]);
                if (!result)
                {
                    assert(0);
                    return false;
                }
            }
        }

        mGroupInitialized = true;
        return true;
    } while (0);

    return false;
}

bool Merge::Process()
{
    std::vector<int> cur(mGroup.size(), -1);        // ��i��ʹ�õ�cur[i]��ͼƬ, -1��ʾ��ʹ��
    cur[0] = 0;

    int count = 0;
    std::wstring name;
    do
    {
        // �Ե�һ��(��ͼ)�Ĵ�СΪ���Χ
        PictureInfo newImage(mInfo[0][cur[0]]);
        for (int i = 1; i < mInfo.size(); ++i)
        {
            if (cur[i] == -1)
                continue;
            PictureInfo *pi = &mInfo[i][cur[i]];

            //
            // (��͸��)���ظ���
            //
            PixelsOverWrite(&newImage, pi);
        }

        wchar_t tmp[32];
        StringCchPrintf(tmp, _countof(tmp), L"%04d.png", count++);

        std::size_t pos = mTxtFileName.rfind(L'\\');
        name.assign(mTxtFileName.substr(0, pos+1));
        name += L"Merge_";
        name += mTxtFileName.substr(pos+1);
        name += L'_';
        name += tmp;
        //StringCchPrintf(name, _countof(name), L"Merge_%s_%04d.png", mTxtFileName, count++);
        SaveToPng(name.c_str(), &newImage);
        printf("%S saved.\n", name.c_str());
    } while (NextPermutation(cur));

    int total = mInfo[0].size();
    for (int i=1; i<mInfo.size(); ++i)
        total *= (mInfo[i].size() + 1);
    printf("%d pictures saved, %d in total.\n", count, total);

    return true;
}

void Merge::Release()
{
    mGroup.clear();
    std::vector<int>().swap(mGroup);
    mInfo.clear();
    std::vector<std::vector<PictureInfo>>().swap(mInfo);
}

bool Merge::LoadFileList(const wchar_t *txtFileName)
{
    if (!txtFileName)
    {
        assert(0);
        return false;
    }

    HANDLE hTxt = CreateFile(txtFileName,
        GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hTxt == INVALID_HANDLE_VALUE)
    {
        assert(0);
        return false;
    }

    DWORD size = GetFileSize(hTxt, NULL);
    DWORD bytesRead = 0;
    char* data = new char[size];
    char* dataEnd = data + size;
    bool result = ReadFile(hTxt, data, size, &bytesRead, 0);
    assert(result && size == bytesRead);
    CloseHandle(hTxt);

    if (*data > '9' || *data < '0')  // ����˵����
        GotoNextLine(&data, dataEnd);

    PictureInfo pi;
    int length;
    while (data < dataEnd)
    {
        if (*data == '\n' || *data == '\r')
            ++data;
        else
        {
            pi.id = atoi(data);
            length = max(0, (int)log10((float)pi.id)) + 1;
            data += length;
            SkipBlank(&data, dataEnd);

            pi.x = atoi(data);
            length = max(0, (int)log10((float)pi.x)) + 1;
            data += length;
            SkipBlank(&data, dataEnd);

            pi.y = atoi(data);
            length = max(0, (int)log10((float)pi.y)) + 1;
            data += length;
            SkipBlank(&data, dataEnd);

            pi.width = atoi(data);
            length = max(0, (int)log10((float)pi.width)) + 1;
            data += length;
            SkipBlank(&data, dataEnd);

            pi.height = atoi(data);
            length = max(0, (int)log10((float)pi.height)) + 1;
            data += length;
            SkipBlank(&data, dataEnd);

            SaveToGroup(&pi);
        }
    }

    return true;
}

void Merge::GotoNextLine(char **ptr, char *limit)
{
    while (*ptr < limit && **ptr != '\n')
        ++(*ptr);
    if (*ptr < limit)   // ptr��������
        ++(*ptr);
}

void Merge::SkipBlank(char ** ptr, char * limit)
{
    while (*ptr < limit && (**ptr == ' ' || **ptr == '\t'))
        ++(*ptr);
}

void Merge::SaveToGroup(PictureInfo *pi)
{
    for (int i = 0; i < mGroup.size(); ++i)
    {
        if (pi->id <= mGroup[i])
        {
            mInfo[i].push_back(*pi);
            return;
        }
    }
    assert(0);
}

bool Merge::OpenPng(const wchar_t *filename, PictureInfo *pi)
{
    assert(pi);
    
    png_structp png_ptr;     //libpng�Ľṹ��
    png_infop   info_ptr;    //libpng����Ϣ

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        goto Error;
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        goto Error;
    }
    int iRetVal = setjmp(png_jmpbuf(png_ptr));//��װ��������ת��
                                             //��libpng�ڲ����ִ����ʱ��libpng�����longjmpֱ����ת���������С�
    if (iRetVal)//setjmp�ķ���ֵ����libpng��ת���ṩ�Ĵ�����루ò������1�����ǻ������ҿ�libpng�Ĺٷ��ĵ���
    {
        fprintf(stderr, "�����룺%d\n", iRetVal);
        goto Error;
    }

    assert(png_ptr && info_ptr);

    FILE *ptrFile = NULL;
    errno_t err = _wfopen_s(&ptrFile, filename, L"rb");
    assert(err == 0 && ptrFile);

    //
    // ��libpng���ļ���
    //
    png_init_io(png_ptr, ptrFile);
    png_read_info(png_ptr, info_ptr);

    //
    // ��ȡ�ļ�ͷ��Ϣ
    //    
    int bit_depth, color_type;
    png_uint_32 width, height;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    //
    // ����ɫ��ʽ��ȡΪRGBA
    //

    //Ҫ��ת��������ɫ��RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    //Ҫ��λ���ǿ��8bit
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth<8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    //Ҫ��λ���ǿ��8bit
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    //�Ҷȱ���ת����RGB
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    //
    // �������ػ�����
    //
    COLORREF* pPixels = (COLORREF*)malloc(width * height * sizeof(COLORREF));
    COLORREF** lines = (COLORREF**)malloc(height * sizeof(COLORREF*));//��ָ��
    if (!lines)
        goto Error;
    png_int_32 h = height - 1;
    png_int_32 i = 0;
    while (h >= 0)//�������ȡ����Ϊλͼ�ǵ׵�����
    {
        lines[i] = (COLORREF*)&pPixels[h * width];
        --h;
        ++i;
    }

    //
    // ��ȡ����
    //
    png_read_image(png_ptr, (png_bytepp)lines);

    //
    // �ͷ���Դ
    //
    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(ptrFile);

    pi->mLines = lines;
    pi->mPixels = pPixels;

    return true;

Error:
    assert(0);
    return false;
}

bool Merge::SaveToPng(const wchar_t *filename, PictureInfo *pi)
{
    png_structp png_ptr;     //libpng�Ľṹ��
    png_infop   info_ptr;    //libpng����Ϣ

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        goto Error;
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        goto Error;
    }

    //��װ��������ת��
    //��libpng�ڲ����ִ����ʱ��libpng�����longjmpֱ����ת���������С�
    int iRetVal = setjmp(png_jmpbuf(png_ptr));

    //setjmp�ķ���ֵ����libpng��ת���ṩ�Ĵ�����루ò������1�����ǻ������ҿ�libpng�Ĺٷ��ĵ���
    if (iRetVal)
    {
        fprintf(stderr, "�����룺%d\n", iRetVal);
        goto Error;
    }

    assert(png_ptr && info_ptr);

    FILE *ptrFile = NULL;
    errno_t err = _wfopen_s(&ptrFile, filename, L"wb");
    assert(err == 0);
    png_init_io(png_ptr, ptrFile);

    //
    //����PNG�ļ�ͷ
    //
    png_set_IHDR(
        png_ptr,
        info_ptr,
        pi->width,
        pi->height,
        8,                              //��ɫ���,
        PNG_COLOR_TYPE_RGBA,            //��ɫ����, PNG_COLOR_TYPE_RGBA��ʾ32λ��͸��ͨ�����ɫ
        PNG_INTERLACE_NONE,             //����������: PNG_INTERLACE_ADAM7
        PNG_COMPRESSION_TYPE_DEFAULT,   //ѹ����ʽ
        PNG_FILTER_TYPE_DEFAULT         //ʲô����? Ĭ���� PNG_FILTER_TYPE_DEFAULT
    );
    //���ô����Ϣ
    png_set_packing(png_ptr);
    //д���ļ�ͷ
    png_write_info(png_ptr, info_ptr);

    //
    // д������
    //
    png_write_image(png_ptr, (png_bytepp)pi->mLines);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(ptrFile);

    return true;

Error:
    assert(0);
    return false;
}

bool Merge::NextPermutation(std::vector<int>& v)
{
    // ��� mGroup mInfo �����ж�
    for (int i = v.size() - 1; i >= 0; --i)
    {
        if (v[i] < (int)(mInfo[i].size() - 1))
        {
            ++v[i];
            return true;
        }
        else
        {
            v[i] = -1;
            // i��1, �ж���һ��
        }
    }
    return false;
}

void Merge::PixelsOverWrite(PictureInfo * dst, PictureInfo * src)
{
    for (int h = 0; h < src->height; ++h)
        for (int w = 0; w < src->width; ++w)
        {
            COLORREF srcPixel = src->mPixels[h * src->width + w];
            if ((srcPixel & 0xff000000))
            {
                // dst(��ͼ)Ҳ��ƫ��, ��ʱҪ����
                int dstW = -dst->x + src->x + w;
                // y�᷽��Ҫ������, ������Ϊy���ǵ��ŵ�����dst->yҪ��
                int dstH = dst->y + dst->height - src->y - (src->height - h);

                // Alpha���
                union {
                    unsigned char p[4];
                    COLORREF pixel;
                } srcP, dstP, blend;
                srcP.pixel = srcPixel;
                dstP.pixel = dst->mPixels[dstH * dst->width + dstW];

                float r[4], s[4], d[4];
                float as = (float)srcP.p[3] / 0xff, ad = (float)dstP.p[3] / 0xff;
                float fs = 1, fd = 1-as;    // SRC OVER
                s[0] = (float)srcP.p[0]; s[1] = (float)srcP.p[1];
                s[2] = (float)srcP.p[2]; s[3] = (float)srcP.p[3];
                d[0] = (float)dstP.p[0]; d[1] = (float)dstP.p[1];
                d[2] = (float)dstP.p[2]; d[3] = (float)dstP.p[3];

                for (int k = 0; k < 3; ++k)
                    r[k] = s[k]*fs + d[k]*fd;
                r[3] = (as + ad * (1 - as)) * 0xff;

                blend.p[0] = (unsigned char)r[0];
                blend.p[1] = (unsigned char)r[1];
                blend.p[2] = (unsigned char)r[2];
                blend.p[3] = (unsigned char)r[3];
                
                dst->mPixels[dstH * dst->width + dstW] = blend.pixel;
            }
        }
}
