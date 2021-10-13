//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <storemain.h>

BOOL FS_DevCloseFileHandle(CBlockDevice *pDevice)
{
    return pDevice->Close();
}

BOOL FS_DevDeviceIoControl(CBlockDevice *pDevice, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) 
{
    return pDevice->Control(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}

BOOL FS_DevReadFile(CBlockDevice *pDevice, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,    LPOVERLAPPED lpOverlapped)
{
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevWriteFile(CBlockDevice *pDevice, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,    LPOVERLAPPED lpOverlapped)
{
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

DWORD FS_DevSetFilePointer(CBlockDevice *pDevice, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) 
{
    SetLastError(ERROR_INVALID_FUNCTION);
    return -1;
}


DWORD FS_DevGetFileSize(CBlockDevice *pDevice, LPDWORD lpFileSizeHigh) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return 0xffffffff;
}

BOOL FS_DevGetFileInformationByHandle(CBlockDevice *pDevice, LPBY_HANDLE_FILE_INFORMATION lpFileInfo) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevFlushFileBuffers(CBlockDevice *pDevice) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevGetFileTime(CBlockDevice *pDevice, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevSetFileTime(CBlockDevice *pDevice, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevSetEndOfFile(CBlockDevice *pDevice) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

/*
        "Profile"="RamDisk"
        "FriendlyName"="Windows CE Ramdisk Driver"
        "Dll"="ramdisk.dll"
        "Prefix"="DSK"
*/

PFNVOID CBlockDevice::LoadFunction(TCHAR *szPrefix, TCHAR *szSuffix)
{
    TCHAR szFunction[32+DEVICENAMESIZE];
    if (wcslen(szPrefix)) {
        wsprintf( szFunction, L"%s_%s", szPrefix, szSuffix);
    } else {
        wcscpy( szFunction, szSuffix);
    }
    return (PFNVOID)GetProcAddress( m_hBlockDevice, szFunction);
}

#define WRITE_REG_SZ(Name, Value) RegSetValueEx( hKey, Name, 0, REG_SZ, (LPBYTE)Value, (wcslen(Value)+1)*sizeof(WCHAR))
#define WRITE_REG_DWORD(Name, Value) RegSetValueEx( hKey, Name, 0, REG_DWORD, (LPBYTE)Value, sizeof(DWORD));

HANDLE CBlockDevice::OpenBlockDevice()
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HKEY hKey;
    TCHAR szModule[MAX_PATH];
    TCHAR szPrefix[DEVICENAMESIZE];

    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, m_szDriverPath, 0, 0, &hKey)) {
        if (FsdGetRegistryString(hKey, L"Dll", szModule, sizeof(szModule)/sizeof(WCHAR))) {
            m_hBlockDevice = LoadDriver( szModule);
            if (m_hBlockDevice) {
                if (!FsdGetRegistryString(hKey, L"Prefix", szPrefix, sizeof(szPrefix)/sizeof(WCHAR))) {
                    wcscpy( szPrefix, L"");
                }
                m_pInit      = (PINITFN)LoadFunction( szPrefix, L"Init");
                m_pDeinit    = (PDEINITFN)LoadFunction( szPrefix, L"Deinit");
                m_pOpen      = (POPENFN)LoadFunction( szPrefix, L"Open");
                m_pClose     = (PCLOSEFN)LoadFunction( szPrefix, L"Close");
                m_pControl   = (PCONTROLFN)LoadFunction( szPrefix, L"IOControl");
                m_pPowerdown = (PPOWERDNFN)LoadFunction( szPrefix, L"PowerDown");
                m_pPowerup   = (PPOWERUPFN)LoadFunction( szPrefix, L"PowerUp");
            }   
        }
        if (m_pInit && m_pOpen && m_pControl) {
            __try {
                WRITE_REG_SZ( L"Key", m_szDriverPath);
                if (m_hDevice = m_pInit(m_szDriverPath)) {
                    if (!(m_hOpenHandle = m_pOpen( m_hDevice, GENERIC_READ | GENERIC_WRITE, 0))) {
                        m_pDeinit( m_hDevice);
                        m_hDevice = NULL;
                    }
                } 
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                if (m_pInit)
                    m_pDeinit(m_hDevice);
            }
            if (m_hOpenHandle && m_hDevice) {
                hFile = CreateAPIHandle( g_hBlockDevApi, this); 
            }   
        }
        DWORD dwFlag = 4;
        WRITE_REG_DWORD( L"Flags", &dwFlag);
    }   
    return hFile;
}

