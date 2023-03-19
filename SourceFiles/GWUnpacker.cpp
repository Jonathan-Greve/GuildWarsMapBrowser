#include "pch.h"
#include <stdio.h>
#include "Xentax.h"
#include "GWUnpacker.h"
#include <algorithm>
#include <map>

using namespace std;

std::string typeToString(int type)
{
    switch (type)
    {
    case NONE:
        return " ";
    case AMAT:
        return "AMAT";
    case AMP:
        return "Amp";
    case ATEXDXT1:
        return "ATEXDXT1";
    case ATEXDXT2:
        return "ATEXDXT2";
    case ATEXDXT3:
        return "ATEXDXT3";
    case ATEXDXT4:
        return "ATEXDXT4";
    case ATEXDXT5:
        return "ATEXDXT5";
    case ATEXDXTL:
        return "ATEXDXTL";
    case ATEXDXTN:
        return "ATEXDXTN";
    case ATEXDXTA:
        return "ATEXDXTA";
    case ATTXDXT1:
        return "ATTXDXT1";
    case ATTXDXT3:
        return "ATTXDXT3";
    case ATTXDXT5:
        return "ATTXDXT5";
    case ATTXDXTN:
        return "ATTXDXTN";
    case ATTXDXTA:
        return "ATTXDXTA";
    case ATTXDXTL:
        return "ATTXDXTL";
    case DDS:
        return "DDS";
    case FFNA_Type2:
        return "FFNA - Model";
    case FFNA_Type3:
        return "FFNA - Map";
    case FFNA_Unknown:
        return "FFNA - Unknown";
    case MFTBASE:
        return "MFTBase";
    case NOTREAD:
        return "";
    case SOUND:
        return "Sound";
    case TEXT:
        return "Text";
    case UNKNOWN:
        return "Unknown";
    default:
        return "Unknown";
    }
}

HANDLE GWDat::get_dat_filehandle(const TCHAR* file)
{
    auto file_handle = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (file_handle == INVALID_HANDLE_VALUE)
    {
        TCHAR text[2048];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, text, 2048, NULL);

        std::wstring s;
        s = std::format(L"Error while opening \"{}\": {}", file, text);
        MessageBox(NULL, s.c_str(), L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    return file_handle;
}

void GWDat::seek(HANDLE file_handle, __int64 offset, int origin)
{
    LARGE_INTEGER i;
    i.QuadPart = offset;
    SetFilePointerEx(file_handle, i, NULL, FILE_BEGIN);
}

void GWDat::read(HANDLE file_handle, void* buffer, int size, int count)
{
    DWORD bread;
    int errcde = ReadFile(file_handle, buffer, size * count, &bread, NULL);
    if (! errcde)
    {
        TCHAR text[2048];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, text, 1024, NULL);
        printf("%s\n", text);
    }
}

unsigned char* GWDat::readFile(HANDLE file_handle, unsigned int n, bool translate /* = true*/)
{
    MFTEntry& m = MFT[n];

    if (m.type == NOTREAD)
        filesRead += 1;

    //Don't read files that were already read if we just need the type
    if (m.type != NOTREAD && ! translate)
    {
        return NULL;
    }

    if (! m.b)
    {
        m.type = MFTBASE;
        m.uncompressedSize = 0;
        mftBaseFiles += 1;
        return NULL;
    }

    unsigned char* Input = new unsigned char[m.Size];
    unsigned char* Output = NULL;
    int OutSize = 0;

    seek(file_handle, m.Offset, 0);
    read(file_handle, Input, m.Size, 1);

    if (m.a)
        UnpackGWDat(Input, m.Size, Output, OutSize);
    else
    {
        Output = new unsigned char[m.Size];
        memcpy(Output, Input, m.Size);
        OutSize = m.Size;
    }
    delete[] Input;

    if (Output)
    {
        if (m.type == NOTREAD)
        {
            int type = 0;
            auto sub_type = ((unsigned char*)Output)[4];
            unsigned int i = ((unsigned int*)Output)[0];
            unsigned int k = ((unsigned int*)Output)[1];
            int i2 = i & 0xffff;
            int i3 = i & 0xffffff;

            switch (i)
            {
            case 'XTTA':
                textureFiles += 1;
                switch (k)
                {
                case '1TXD':
                    type = ATTXDXT1;
                    break;
                case '3TXD':
                    type = ATTXDXT3;
                    break;
                case '5TXD':
                    type = ATTXDXT5;
                    break;
                case 'NTXD':
                    type = ATTXDXTN;
                    break;
                case 'ATXD':
                    type = ATTXDXTA;
                    break;
                case 'LTXD':
                    type = ATTXDXTL;
                    break;
                }
                break;
            case 'XETA':
                textureFiles += 1;
                switch (k)
                {
                case '1TXD':
                    type = ATEXDXT1;
                    break;
                case '2TXD':
                    type = ATEXDXT2;
                    break;
                case '3TXD':
                    type = ATEXDXT3;
                    break;
                case '4TXD':
                    type = ATEXDXT4;
                    break;
                case '5TXD':
                    type = ATEXDXT5;
                    break;
                case 'NTXD':
                    type = ATEXDXTN;
                    break;
                case 'ATXD':
                    type = ATEXDXTA;
                    break;
                case 'LTXD':
                    type = ATEXDXTL;
                    break;
                }
                break;
            case '===;':
            case '***;':
                type = TEXT;
                textFiles += 1;
                break;
            case 'anff':
                if (sub_type == 2)
                {
                    type = FFNA_Type2;
                }
                else if (sub_type == 3)
                {
                    type = FFNA_Type3;
                }
                else
                {
                    type = FFNA_Unknown;
                }
                ffnaFiles += 1;
                break;
            case ' SDD':
                type = DDS;
                textureFiles += 1;
                break;
            case 'TAMA':
                type = AMAT;
                amatFiles += 1;
                break;
            default:
                type = UNKNOWN;
            }
            switch (i2)
            {
            case 0xFAFF:
            case 0xFBFF:
                type = SOUND;
                break;
            default:
                break;
            }

            switch (i3)
            {
            case 'PMA':
                type = AMP;
                break;
            case 0x334449:
                type = SOUND;
                break;
            default:
                break;
            }

            if (type == AMP || type == SOUND)
                soundFiles += 1;
            else if (type == UNKNOWN)
                unknownFiles += 1;

            m.type = type;
            m.uncompressedSize = OutSize;
        }
    }
    return Output;
}

bool compareH(MFTExpansion& a, MFTExpansion b) { return a.FileOffset < b.FileOffset; }

unsigned int GWDat::readDat(const TCHAR* file)
{
    auto file_handle = get_dat_filehandle(file);

    read(file_handle, &GWHead, sizeof(GWHead), 1);

    if (! (GWHead.ID[0] == 0x33 && GWHead.ID[1] == 0x41 && GWHead.ID[2] == 0x4e && GWHead.ID[3] == 0x1a))
    {
        std::wstring s;
        s = std::format(L"The input file \"{}\"is not a Guild Wars datafile!", file);
        MessageBox(NULL, s.c_str(), L"Error", MB_ICONERROR | MB_OK);
        CloseHandle(file_handle);
        return 0;
    }

    //read reserved MFT entries
    seek(file_handle, GWHead.MFTOffset, SEEK_SET);
    read(file_handle, &MFTH, sizeof(MFTH), 1);
    for (int x = 0; x < 15; ++x)
    {
        MFTEntry ME;
        read(file_handle, &ME, 0x18, 1);
        ME.type = NOTREAD;
        ME.uncompressedSize = -1;
        ME.Hash = 0;
        MFT.push_back(ME);
    }

    //read Hashlist
    seek(file_handle, MFT[1].Offset, SEEK_SET);
    int mftxsize = MFT[1].Size / sizeof(MFTExpansion);
    for (unsigned int x = 0; x < MFT[1].Size / sizeof(MFTExpansion); x++)
    {
        MFTExpansion _MFTX;
        read(file_handle, &_MFTX, sizeof(MFTExpansion), 1);
        MFTX.push_back(_MFTX);
    }

    std::sort(MFTX.begin(), MFTX.end(), compareH);

    //read MFT entries
    unsigned int hashcounter = 0;
    while (MFTX[hashcounter].FileOffset < 16)
        ++hashcounter;

    seek(file_handle, GWHead.MFTOffset + 24 * 16, SEEK_SET);
    for (int x = 16; x < MFTH.EntryCount - 1; ++x)
    {
        MFTEntry ME;
        read(file_handle, &ME, 0x18, 1);
        ME.type = NOTREAD;
        ME.uncompressedSize = -1;

        if (hashcounter < MFTX.size() && x == MFTX[hashcounter].FileOffset)
        {
            ME.Hash = MFTX[hashcounter].FileNumber;
            MFT.push_back(ME);

            while (hashcounter + 1 < MFTX.size() &&
                   MFTX[hashcounter].FileOffset == MFTX[hashcounter + 1].FileOffset)
            {
                ++hashcounter;
                ME.Hash = MFTX[hashcounter].FileNumber;
                MFT.push_back(ME);
            }

            ++hashcounter;
        }
        else
        {
            ME.Hash = 0;
            MFT.push_back(ME);
        }
    }
    CloseHandle(file_handle);

    filesRead = 0;
    textureFiles = 0;
    soundFiles = 0;
    ffnaFiles = 0;
    unknownFiles = 0;
    textFiles = 0;
    mftBaseFiles = 0;
    amatFiles = 0;

    return (unsigned int)MFT.size();
}

class compareNAsc
{
public:
    compareNAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return a > b; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareNDsc
{
public:
    compareNDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return b > a; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareIDAsc
{
public:
    compareIDAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[a].ID > MFT[b].ID; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareIDDsc
{
public:
    compareIDDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[b].ID > MFT[a].ID; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareOffsetAsc
{
public:
    compareOffsetAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[a].Offset > MFT[b].Offset; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareOffsetDsc
{
public:
    compareOffsetDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[b].Offset > MFT[a].Offset; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareSizeAsc
{
public:
    compareSizeAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[a].Size > MFT[b].Size; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareSizeDsc
{
public:
    compareSizeDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[b].Size > MFT[a].Size; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareUncompressedSizeAsc
{
public:
    compareUncompressedSizeAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[a].uncompressedSize > MFT[b].uncompressedSize; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareUncompressedSizeDsc
{
public:
    compareUncompressedSizeDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[b].uncompressedSize > MFT[a].uncompressedSize; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareFlagsAsc
{
public:
    compareFlagsAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b)
    {
        if (MFT[a].a != MFT[b].a)
            return MFT[a].a > MFT[b].a;
        else if (MFT[a].b != MFT[b].b)
            return MFT[a].b > MFT[b].b;
        else
            return MFT[a].c > MFT[b].c;
    }

protected:
    const vector<MFTEntry>& MFT;
};

class compareFlagsDsc
{
public:
    compareFlagsDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b)
    {
        if (MFT[b].a != MFT[a].a)
            return MFT[b].a > MFT[a].a;
        else if (MFT[b].b != MFT[a].b)
            return MFT[b].b > MFT[a].b;
        else
            return MFT[b].c > MFT[a].c;
    }

protected:
    const vector<MFTEntry>& MFT;
};

class compareTypeAsc
{
public:
    compareTypeAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[a].type > MFT[b].type; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareTypeDsc
{
public:
    compareTypeDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return MFT[b].type > MFT[a].type; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareHashAsc
{
public:
    compareHashAsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return (unsigned int)MFT[a].Hash > (unsigned int)MFT[b].Hash; }

protected:
    const vector<MFTEntry>& MFT;
};

class compareHashDsc
{
public:
    compareHashDsc(const vector<MFTEntry>& MFT)
        : MFT(MFT)
    {
    }

    bool operator()(int a, int b) { return (unsigned int)MFT[b].Hash > (unsigned int)MFT[a].Hash; }

protected:
    const vector<MFTEntry>& MFT;
};

void GWDat::sort(unsigned int* index, int column, bool ascending)
{
    switch (column)
    {
    case 0:
        if (ascending)
            std::sort(index, index + MFT.size(), compareNAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareNDsc(MFT));
        break;
    case 1:
        if (ascending)
            std::sort(index, index + MFT.size(), compareIDAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareIDDsc(MFT));
        break;
    case 2:
    case 3:
        if (ascending)
            std::sort(index, index + MFT.size(), compareOffsetAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareOffsetDsc(MFT));
        break;
    case 4:
        if (ascending)
            std::sort(index, index + MFT.size(), compareSizeAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareSizeDsc(MFT));
        break;
    case 5:
        if (ascending)
            std::sort(index, index + MFT.size(), compareUncompressedSizeAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareUncompressedSizeDsc(MFT));
        break;
    case 6:
        if (ascending)
            std::sort(index, index + MFT.size(), compareFlagsAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareFlagsDsc(MFT));
        break;
    case 7:
        if (ascending)
            std::sort(index, index + MFT.size(), compareTypeAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareTypeDsc(MFT));
        break;
    case 8:
        if (ascending)
            std::sort(index, index + MFT.size(), compareHashAsc(MFT));
        else
            std::sort(index, index + MFT.size(), compareHashDsc(MFT));
        break;
    }
}
