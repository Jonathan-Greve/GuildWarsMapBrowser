//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <winsdkver.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <sdkddkver.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>

#include <wrl/client.h>

#include <d3d11_1.h>
#include <dxgi1_6.h>

#include <DirectXMath.h>
#include <DirectXColors.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <string>
#include <format>
#include <span>
#include <unordered_set>
#include <variant>
#include <optional>
#include <filesystem>

// Dear ImGui
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imconfig.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Dear ImGui File Dialog
#include "ImGuiFileDialog.h"

#include "GWUnpacker.h"
#include "AtexDecompress.h"
#include "AtexReader.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "bass.h"
typedef BOOL(__stdcall* LPFNBASSINIT)(int, DWORD, DWORD, HWND, const void*);
typedef HSTREAM(WINAPI* LPFNBASSSTREAMCREATEFILE)(BOOL mem, const void* file, QWORD offset, QWORD length,
                                                  DWORD flags);
typedef BOOL(WINAPI* LPFNBASSCHANNELPLAY)(DWORD handle, BOOL restart);
typedef BOOL(WINAPI* LPFNBASSCHANNELPAUSE)(DWORD handle);
typedef BOOL(WINAPI* LPFNBASSCHANNELSTOP)(DWORD handle);
typedef double(WINAPI* LPFNBASSCHANNELBYTES2SECONDS)(DWORD handle, QWORD bytes);
typedef QWORD(WINAPI* LPFNBASSCHANNELGETLENGTH)(DWORD handle, DWORD mode);
typedef DWORD(WINAPI* LPFNBASSSTREAMGETFILEPOSITION)(HSTREAM handle, DWORD mode);
typedef BOOL(WINAPI* LPFNBASSCHANNELGETINFO)(DWORD handle, BASS_CHANNELINFO* info);
typedef DWORD(WINAPI* LPFNBASSCHANNELFLAGS)(DWORD handle, DWORD flags, DWORD mask);
typedef BOOL(WINAPI* LPFNBASSSTREAMFREE)(DWORD handle);
typedef BOOL(WINAPI* LPFNBASSCHANNELSETPOSITION)(DWORD handle, QWORD pos, DWORD mode);
typedef QWORD(WINAPI* LPFNBASSCHANNELGETPOSITION)(DWORD handle, DWORD mode);
typedef QWORD(WINAPI* LPFNBASSCHANNELSECONDS2BYTES)(DWORD handle, double seconds);
typedef BOOL(WINAPI* LPFNBASSCHANNELSETATTRIBUTE)(DWORD handle, DWORD attrib, float value);

#include "bass_fx.h"
typedef DWORD(WINAPI* LPFNBASSFXTMPOCREATE)(DWORD chan, DWORD flags);

namespace DX
{
// Helper class for COM exceptions
class com_exception : public std::exception
{
public:
    com_exception(HRESULT hr) noexcept
        : result(hr)
    {
    }

    const char* what() const noexcept override
    {
        static char s_str[64] = {};
        sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
        return s_str;
    }

private:
    HRESULT result;
};

// Helper utility converts D3D API failures into exceptions.
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw com_exception(hr);
    }
}
}

static const char* type_strings[26] = {
    " ", "AMAT", "Amp", "ATEXDXT1", "ATEXDXT2", "ATEXDXT3", "ATEXDXT4",
    "ATEXDXT5", "ATEXDXTN", "ATEXDXTA", "ATEXDXTL", "ATTXDXT1", "ATTXDXT3", "ATTXDXT5",
    "ATTXDXTN", "ATTXDXTA", "ATTXDXTL", "DDS", "FFNA - Model", "FFNA - Map", "FFNA - Unknown",
    "MFTBase", "NOT_READ", "Sound", "Text", "Unknown"
};

inline int decode_filename(int id0, int id1) { return (id0 - 0xff00ff) + (id1 * 0xff00); }

inline void encode_filehash(uint32_t filehash, int& id0_out, int& id1_out) {
    id0_out = static_cast<wchar_t>(((filehash - 1) % 0xff00) + 0x100);
    id1_out = static_cast<wchar_t>(((filehash - 1) / 0xff00) + 0x100);
}

inline std::optional<std::filesystem::path> get_executable_directory() {
    WCHAR path[MAX_PATH];
    HMODULE hModule = GetModuleHandle(NULL);
    if (hModule != NULL) {
        // Get the path to the executable
        if (GetModuleFileName(hModule, path, MAX_PATH) > 0) {
            // Convert to std::filesystem::path and return the directory part
            return std::filesystem::path(path).parent_path();
        }
    }
    return std::nullopt; // Return nullopt if unable to retrieve the path
}

inline std::optional<std::filesystem::path> load_last_filepath(const std::string& filename) {
    auto exe_dir_opt = get_executable_directory();
    if (exe_dir_opt) {
        if (std::filesystem::exists(*exe_dir_opt / filename)) {
            std::ifstream infile(*exe_dir_opt / filename);
            if (infile.is_open()) {
                std::string line;
                std::getline(infile, line);
                if (std::filesystem::exists(line)) {
                    return line;
                }
            }
        }
    }

    return std::nullopt;
}

inline std::optional<std::filesystem::path> save_last_filepath(const std::filesystem::path& filepath, const std::string& filename) {
    auto exe_dir_opt = get_executable_directory();
    if (exe_dir_opt) {
        std::ofstream outfile(*exe_dir_opt / filename, std::ios::trunc);
        outfile << filepath.string();

        return filepath;
    }

    return std::nullopt;
}

enum class LODQuality : uint8_t {
    High, // Best quality
    Medium, // Medium quality
    Low, // Lowset quality
};

inline std::vector<FileType> GetAllFileTypes() {
    return { NONE, AMAT, AMP, ATEXDXT1, ATEXDXT2, ATEXDXT3, ATEXDXT4, ATEXDXT5, ATEXDXTN, ATEXDXTA, ATEXDXTL,
            ATTXDXT1, ATTXDXT3, ATTXDXT5, ATTXDXTN, ATTXDXTA, ATTXDXTL, DDS, FFNA_Type2, FFNA_Type3, FFNA_Unknown,
            MFTBASE, NOTREAD, SOUND, TEXT, UNKNOWN };
}

#include <commdlg.h>
#include <shobjidl.h>

inline std::wstring OpenFileDialog(std::wstring filename, std::wstring fileType)
{
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH];
    wcsncpy(fileName, filename.c_str(), MAX_PATH);
    fileName[MAX_PATH - 1] = L'\0'; // Ensure null termination
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = nullptr;

    // Build filter string with embedded nulls properly
    // Format: "Description\0Pattern\0Description2\0Pattern2\0\0"
    std::wstring filterDesc = fileType + L" Files (*." + fileType + L")";
    std::wstring filterPattern = L"*." + fileType;

    std::vector<wchar_t> filterNullTerm;
    filterNullTerm.insert(filterNullTerm.end(), filterDesc.begin(), filterDesc.end());
    filterNullTerm.push_back(L'\0');
    filterNullTerm.insert(filterNullTerm.end(), filterPattern.begin(), filterPattern.end());
    filterNullTerm.push_back(L'\0');
    filterNullTerm.push_back(L'\0'); // Double null terminator at end

    ofn.lpstrFilter = filterNullTerm.data();
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = fileType.c_str();

    if (GetSaveFileName(&ofn))
    {
        std::wstring wFileName(fileName);
        return wFileName;
    }

    return L"";
}

inline std::wstring OpenDirectoryDialog()
{
    IFileDialog* pfd;
    std::wstring wDirName;

    // CoCreate the dialog object.
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (SUCCEEDED(hr))
    {
        DWORD dwOptions;
        // Get the options for the dialog.
        hr = pfd->GetOptions(&dwOptions);
        if (SUCCEEDED(hr))
        {
            // Set the options to pick folders only and don't change the working directory.
            hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_NOCHANGEDIR);
            if (SUCCEEDED(hr))
            {
                // Show the dialog.
                hr = pfd->Show(nullptr);
                if (SUCCEEDED(hr))
                {
                    // Get the folder selected by the user.
                    IShellItem* psi;
                    hr = pfd->GetFolder(&psi);
                    if (SUCCEEDED(hr))
                    {
                        PWSTR pszPath;
                        hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);

                        if (SUCCEEDED(hr))
                        {
                            wDirName = pszPath;
                            CoTaskMemFree(pszPath);
                        }
                        psi->Release();
                    }
                }
            }
        }
        pfd->Release();
    }
    return wDirName;
}

inline bool is_type_texture(FileType type) {
    switch (type)
    {
    case ATEXDXT1:
    case ATEXDXT2:
    case ATEXDXT3:
    case ATEXDXT4:
    case ATEXDXT5:
    case ATEXDXTN:
    case ATEXDXTA:
    case ATEXDXTL:
    case ATTXDXT1:
    case ATTXDXT3:
    case ATTXDXT5:
    case ATTXDXTN:
    case ATTXDXTA:
    case ATTXDXTL:
    case DDS:
        return true;
    default:
        break;
    }

    return false;
}