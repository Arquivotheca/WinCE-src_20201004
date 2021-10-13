//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "storeincludes.hpp"

BOOL FS_DevCloseFileHandle(BlockDevice_t *pDevice)
{
    return pDevice->Close();
}

BOOL FS_DevDeviceIoControl(BlockDevice_t *pDevice, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) 
{
    return pDevice->Control(dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}

BOOL FS_DevReadFile(BlockDevice_t* /* pDevice */, LPVOID /* buffer */, DWORD /* nBytesToRead */, LPDWORD /* lpNumBytesRead */, LPOVERLAPPED /* lpOverlapped */)
{
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevWriteFile(BlockDevice_t* /* pDevice */, LPCVOID /* buffer */, DWORD /* nBytesToWrite */, LPDWORD /* lpNumBytesWritten */, LPOVERLAPPED /* lpOverlapped */)
{
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

DWORD FS_DevSetFilePointer(BlockDevice_t* /* pDevice */, LONG /* lDistanceToMove */, PLONG /* lpDistanceToMoveHigh */, DWORD /* dwMoveMethod */) 
{
    SetLastError(ERROR_INVALID_FUNCTION);
    return INVALID_SET_FILE_POINTER;
}


DWORD FS_DevGetFileSize(BlockDevice_t* /* pDevice */, LPDWORD /* lpFileSizeHigh */) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return INVALID_FILE_SIZE;
}

BOOL FS_DevGetFileInformationByHandle(BlockDevice_t* /* pDevice */, LPBY_HANDLE_FILE_INFORMATION /* lpFileInfo */) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevFlushFileBuffers(BlockDevice_t* /* pDevice */) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevGetFileTime(BlockDevice_t* /* pDevice */, LPFILETIME /* lpCreation */, LPFILETIME /* lpLastAccess */, LPFILETIME /* lpLastWrite */) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevSetFileTime(BlockDevice_t* /* pDevice */, CONST FILETIME* /* lpCreation */ , CONST FILETIME* /* lpLastAccess */, CONST FILETIME* /* lpLastWrite */) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevSetEndOfFile(BlockDevice_t* /* pDevice */) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

#ifdef UNDER_CE

const PFNVOID DevFileDirectApiMethods[NUM_FILE_APIS] = {
    (PFNVOID)FS_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)FS_DevReadFile,
    (PFNVOID)FS_DevWriteFile,
    (PFNVOID)FS_DevGetFileSize,
    (PFNVOID)FS_DevSetFilePointer,
    (PFNVOID)FS_DevGetFileInformationByHandle,
    (PFNVOID)FS_DevFlushFileBuffers,
    (PFNVOID)FS_DevGetFileTime,
    (PFNVOID)FS_DevSetFileTime,
    (PFNVOID)FS_DevSetEndOfFile,
    (PFNVOID)FS_DevDeviceIoControl,
};

const PFNVOID DevFileApiMethods[NUM_FILE_APIS] = {
    (PFNVOID)FS_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
};


const ULONGLONG DevFileApiSigs[NUM_FILE_APIS] = {
    FNSIG1(DW),                                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),           // ReadFile
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),           // WriteFile
    FNSIG2(DW,O_PDW),                           // GetFileSize
    FNSIG4(DW,DW,IO_PDW,DW),                    // SetFilePointer
    FNSIG2(DW,IO_PDW),                          // GetFileInformationByHandle
    FNSIG1(DW),                                 // FlushFileBuffers
    FNSIG4(DW,O_PI64,O_PI64,O_PI64),            // GetFileTime
    FNSIG4(DW,IO_PI64,IO_PI64,IO_PI64),         // SetFileTime
    FNSIG1(DW),                                 // SetEndOfFile,
    FNSIG8(DW,DW,IO_PTR,DW,IO_PTR,DW,O_PDW,IO_PDW), // DeviceIoControl
    FNSIG7(DW,O_PTR,DW,O_PDW,IO_PDW,DW,DW),     // ReadFileWithSeek
    FNSIG7(DW,I_PTR,DW,O_PDW,IO_PDW,DW,DW),     // WriteFileWithSeek
    FNSIG6(DW,DW,DW,DW,DW,IO_PDW),              // LockFileEx
    FNSIG5(DW,DW,DW,DW,IO_PDW),                 // UnlockFileEx
};

#endif // UNDER_CE

static HANDLE hBlockDevApi  = NULL;

LRESULT InitializeBlockDeviceAPI ()
{
#ifdef UNDER_CE
    // Initialize the handle-based file API.
    hBlockDevApi = CreateAPISet (const_cast<CHAR*> ("BDEV"), NUM_FILE_APIS, DevFileApiMethods, DevFileApiSigs);
    RegisterAPISet (hBlockDevApi, HT_FILE | REGISTER_APISET_TYPE);
    RegisterDirectMethods(hBlockDevApi, DevFileDirectApiMethods);
#endif
    return ERROR_SUCCESS;
}

/*
        "Profile"="RamDisk"
        "FriendlyName"="Windows CE Ramdisk Driver"
        "Dll"="ramdisk.dll"
        "Prefix"="DSK"
*/

PFNVOID BlockDevice_t::LoadFunction(const TCHAR *szPrefix, const TCHAR *szSuffix)
{
    TCHAR szFunction[32+DEVICENAMESIZE];
    if (szPrefix[0]) {
        VERIFY(SUCCEEDED(StringCchPrintf(szFunction, 32+DEVICENAMESIZE, L"%s_%s", szPrefix, szSuffix)));
    }
    else {
        VERIFY(SUCCEEDED(StringCchCopy(szFunction, 32+DEVICENAMESIZE, szSuffix)));
    }
    return (PFNVOID)FsdGetProcAddress(m_hBlockDevice, szFunction);
}

#ifdef UNDER_CE
#define WRITE_REG_SZ(Name, Value, Max) RegSetValueEx( hKey, Name, 0, REG_SZ, (LPBYTE)Value, (wcsnlen(Value, Max)+1)*sizeof(WCHAR))
#define WRITE_REG_DWORD(Name, Value) RegSetValueEx( hKey, Name, 0, REG_DWORD, (LPBYTE)Value, sizeof(DWORD));
#else
// NT stubs
#define WRITE_REG_SZ(Name, Value, Max)
#define WRITE_REG_DWORD(Name, Value)
#endif

HANDLE BlockDevice_t::OpenBlockDevice()
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HKEY hKey;
    TCHAR szModule[MAX_PATH];
    TCHAR szPrefix[DEVICENAMESIZE];

    if (ERROR_SUCCESS == FsdRegOpenKey( m_szDriverPath, &hKey)) {
        if (FsdGetRegistryString(hKey, L"Dll", szModule, sizeof(szModule)/sizeof(WCHAR))) {
            m_hBlockDevice = LoadDriver( szModule);
            if (m_hBlockDevice) {
                if (!FsdGetRegistryString(hKey, L"Prefix", szPrefix, sizeof(szPrefix)/sizeof(WCHAR))) {
                    VERIFY(SUCCEEDED(StringCchCopy( szPrefix, DEVICENAMESIZE, L"")));
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
                WRITE_REG_SZ( L"Key", m_szDriverPath, _countof(m_szDriverPath));
                m_hDevice = m_pInit(m_szDriverPath);
                if (m_hDevice) {
                    m_hOpenHandle = m_pOpen( m_hDevice, GENERIC_READ | GENERIC_WRITE, 0);
                    if (!m_hOpenHandle) {
                        m_pDeinit(m_hDevice);
                        m_hDevice = NULL;
                    }
                } 
            } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
                m_hOpenHandle = NULL;
            }

            if (m_hOpenHandle && m_hDevice) {
                hFile = CreateAPIHandle(hBlockDevApi, this); 
                if (!hFile) {
                    hFile = INVALID_HANDLE_VALUE;
                }

                GetDeviceIClass(hKey);                
            }   
        }
        DWORD dwFlag = 4;
        WRITE_REG_DWORD( L"Flags", &dwFlag);
    }   
    return hFile;
}

void BlockDevice_t::GetDeviceIClass(HKEY hKey)
{
#ifdef UNDER_CE
    DWORD cbSize = 0;
    if (RegQueryValueEx(hKey, L"IClass", NULL, NULL, NULL, &cbSize) == ERROR_SUCCESS) {

        // Validate the size
        if ((cbSize % sizeof(WCHAR)) != 0) {
            return;
        }

        DWORD cchSize = cbSize / sizeof(WCHAR);
        m_pszIClass = new WCHAR[cchSize+1];
        m_pszIClass[0] = 0;

        if (m_pszIClass) {
            if (RegQueryValueEx(hKey, L"IClass", NULL, NULL, (LPBYTE)m_pszIClass, &cbSize) == ERROR_SUCCESS) {
                m_pszIClass[cchSize - 1] = 0;  // enforce null termination
                m_pszIClass[cchSize] = 0;      // enforce multi-sz double termination
            }
        }
    }
#endif // UNDER_CE
}

void BlockDevice_t::AdvertiseInterface(LPCWSTR szDeviceName, BOOL fAdd)
{
    if (!m_pszIClass) {
        return;
    }

    WCHAR szDevicePath[MAX_PATH];
    VERIFY(SUCCEEDED(StringCchPrintf(szDevicePath, MAX_PATH, L"\\StoreMgr\\%s", szDeviceName)));
    
    // Parse the IClass data, which consists of multiple null-terminated strings
    const WCHAR* pszTempClass = m_pszIClass;
    
    while(*pszTempClass != 0) {

        GUID guidClass;
        size_t nTempLength = 0;

        VERIFY(SUCCEEDED(StringCchLength (pszTempClass, MAX_PATH, &nTempLength)));

        if (!FsdGuidFromString(pszTempClass, &guidClass)) {
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR: Invalid interface class GUID '%s'\r\n", pszTempClass));
        } else if (0 != memcmp(&guidClass, &BLOCK_DRIVER_GUID, sizeof(GUID))) {
            // Advertise any non-block driver interfaces specified in the
            // registry for this driver. We don't advertise the block-driver
            // interface because we don't want FSDMGR's PNPThread to pick it
            // up and try to mount the device a 2nd time.
            FSDMGR_AdvertiseInterface(&guidClass, szDevicePath, fAdd);
        }
        pszTempClass += nTempLength + 1;
    }    
}
