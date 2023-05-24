#pragma once
#pragma comment(lib, "Shlwapi.lib")
#include "resource.h"
#include <tchar.h>
#include <Shlwapi.h>

inline bool extract_bass_dll_resource()
{
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_DLL1), _T("DLL"));
    if (hRes == NULL)
    {
        // Handle error - resource not found
        return false;
    }

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (hData == NULL)
    {
        // Handle error - resource data couldn't be loaded
        return false;
    }

    void* pData = LockResource(hData);
    if (pData == NULL)
    {
        // Handle error - resource couldn't be locked
        return false;
    }

    DWORD dwSize = SizeofResource(NULL, hRes);
    if (dwSize == 0)
    {
        // Handle error - resource size is zero
        return false;
    }

    // Get the path of the executable
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, _T("bass.dll"));

    // Create the file and write the resource data to it
    HANDLE hFile = CreateFile(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // Handle error - file could not be created
        return false;
    }

    DWORD dwBytesWritten = 0;
    WriteFile(hFile, pData, dwSize, &dwBytesWritten, NULL);
    CloseHandle(hFile);

    return true;
}

inline bool extract_bass_fx_dll_resource()
{
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_DLL2), _T("DLL"));
    if (hRes == NULL)
    {
        // Handle error - resource not found
        return false;
    }

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (hData == NULL)
    {
        // Handle error - resource data couldn't be loaded
        return false;
    }

    void* pData = LockResource(hData);
    if (pData == NULL)
    {
        // Handle error - resource couldn't be locked
        return false;
    }

    DWORD dwSize = SizeofResource(NULL, hRes);
    if (dwSize == 0)
    {
        // Handle error - resource size is zero
        return false;
    }

    // Get the path of the executable
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, _T("bass_dx.dll"));

    // Create the file and write the resource data to it
    HANDLE hFile = CreateFile(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // Handle error - file could not be created
        return false;
    }

    DWORD dwBytesWritten = 0;
    WriteFile(hFile, pData, dwSize, &dwBytesWritten, NULL);
    CloseHandle(hFile);

    return true;
}
