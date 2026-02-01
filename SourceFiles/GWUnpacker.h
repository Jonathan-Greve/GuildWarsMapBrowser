#pragma once
#include <vector>

struct MainHeader
{
	unsigned char ID[4];
	int HeaderSize;
	int SectorSize;
	int CRC1;
	__int64 MFTOffset;
	int MFTSize;
	int Flags;
};

struct MFTHeader
{
	unsigned char ID[4];
	int Unk1;
	int Unk2;
	int EntryCount;
	int Unk4;
	int Unk5;
};

enum FileType
{
	NONE,
	AMAT,
	AMP,
	ATEXDXT1,
	ATEXDXT2,
	ATEXDXT3,
	ATEXDXT4,
	ATEXDXT5,
	ATEXDXTN,
	ATEXDXTA,
	ATEXDXTL,
	ATTXDXT1,
	ATTXDXT3,
	ATTXDXT5,
	ATTXDXTN,
	ATTXDXTA,
	ATTXDXTL,
	DDS,
	FFNA_Type2,
	FFNA_Type3,
	FFNA_Unknown,
	MFTBASE,
	NOTREAD,
	SOUND,
	TEXT,
	UNKNOWN
};

struct MFTEntry
{
	__int64 Offset;
	__int32 Size;
	unsigned short a;
	unsigned char b;
	unsigned char c;
	__int32 ID;
	__int32 CRC;
	__int32 type;
	__int32 uncompressedSize;
	__int32 Hash;
	uint32_t murmurhash3;
	std::vector<uint32_t> chunk_ids;  // Chunk IDs found in FFNA files
};

struct MFTExpansion
{
	int FileNumber;
	int FileOffset;
};

class GWDat
{
public:
	unsigned int readDat(const TCHAR* file);
	unsigned char* readFile(HANDLE file_handle, unsigned int n, bool translate = true);

	MFTEntry& operator[](const int n) { return MFT[n]; }

	MFTEntry* get_MFT_entry_ptr(const int n)
	{
		if (n < MFT.size())
			return &MFT[n];
		return nullptr;
	}
	std::vector<MFTEntry>& get_MFT() { return MFT; }

	int getSectorSize() { return GWHead.SectorSize; }

	unsigned int getNumFiles() { return (unsigned int)MFT.size(); }

	void sort(unsigned int* index, int column, bool ascending);

	unsigned int getFilesRead() const { return filesRead; }
	unsigned int getTextureFiles() const { return textureFiles; }
	unsigned int getSoundFiles() const { return soundFiles; }
	unsigned int getFfnaFiles() const { return ffnaFiles; }
	unsigned int getUnknownFiles() const { return unknownFiles; }
	unsigned int getTextFiles() const { return textFiles; }
	unsigned int getMftBaseFiles() const { return mftBaseFiles; }
	unsigned int getAmatFiles() const { return amatFiles; }

	HANDLE get_dat_filehandle(const TCHAR* file);

protected:
	MainHeader GWHead;
	MFTHeader MFTH;
	std::vector<MFTExpansion> MFTX;
	std::vector<MFTEntry> MFT;

	//Critical Section to make readFile thread safe

	//Counters for statistics
	unsigned int filesRead;
	unsigned int textureFiles;
	unsigned int soundFiles;
	unsigned int ffnaFiles;
	unsigned int unknownFiles;
	unsigned int textFiles;
	unsigned int mftBaseFiles;
	unsigned int amatFiles;

protected:
	//wrappers for the OS seek and read functions
	void seek(HANDLE file_handle, __int64 offset, int origin);
	void read(HANDLE file_handle, void* buffer, int size, int count);
};

inline std::string typeToString(int type)
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

inline std::wstring typeToWString(int type)
{
	switch (type)
	{
	case NONE:
		return L" ";
	case AMAT:
		return L"AMAT";
	case AMP:
		return L"Amp";
	case ATEXDXT1:
		return L"ATEXDXT1";
	case ATEXDXT2:
		return L"ATEXDXT2";
	case ATEXDXT3:
		return L"ATEXDXT3";
	case ATEXDXT4:
		return L"ATEXDXT4";
	case ATEXDXT5:
		return L"ATEXDXT5";
	case ATEXDXTL:
		return L"ATEXDXTL";
	case ATEXDXTN:
		return L"ATEXDXTN";
	case ATEXDXTA:
		return L"ATEXDXTA";
	case ATTXDXT1:
		return L"ATTXDXT1";
	case ATTXDXT3:
		return L"ATTXDXT3";
	case ATTXDXT5:
		return L"ATTXDXT5";
	case ATTXDXTN:
		return L"ATTXDXTN";
	case ATTXDXTA:
		return L"ATTXDXTA";
	case ATTXDXTL:
		return L"ATTXDXTL";
	case DDS:
		return L"DDS";
	case FFNA_Type2:
		return L"FFNA - Model";
	case FFNA_Type3:
		return L"FFNA - Map";
	case FFNA_Unknown:
		return L"FFNA - Unknown";
	case MFTBASE:
		return L"MFTBase";
	case NOTREAD:
		return L"";
	case SOUND:
		return L"Sound";
	case TEXT:
		return L"Text";
	case UNKNOWN:
		return L"Unknown";
	default:
		return L"Unknown";
	}
}
