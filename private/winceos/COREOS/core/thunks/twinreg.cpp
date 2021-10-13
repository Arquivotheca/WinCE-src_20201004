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
#define BUILDING_FS_STUBS
#include <windows.h>
#include <fsioctl.h>

#include "KillThreadIfNeeded.hpp"



// SDK exports
extern "C"
LONG
xxx_RegCloseKey(
    HKEY    hKey
    )
{
    return RegCloseKey_Trap(hKey);
}

extern "C"
LONG
xxx_RegFlushKey(
    HKEY    hKey
    )
{
    return RegFlushKey_Trap(hKey);
}


//
// RegCreateKeyEx takes 9 parameter and we need to limit it to 8 for parameter validation.
// combine the 2 out parameters (phkey, lpdwDisposition) into 1 and take care of it in 
// the thunk layer.
//
extern "C"
LONG
xxx_RegCreateKeyExW(
    HKEY                    hKey,
    LPCWSTR                 lpSubKey,
    DWORD                   Reserved,
    LPWSTR                  lpClass,
    DWORD                   dwOptions,
    REGSAM                  samDesired,
    LPSECURITY_ATTRIBUTES   lpSecurityAttributes,
    PHKEY                   phkResult,
    LPDWORD                 lpdwDisposition
    )
{
    return RegCreateKeyExW_Trap(hKey,lpSubKey,Reserved,lpClass,dwOptions,samDesired,lpSecurityAttributes,phkResult,lpdwDisposition);    
}

extern "C"
LONG
xxx_RegDeleteKeyW(
    HKEY    hKey,
    LPCWSTR lpSubKey
    )
{
    return RegDeleteKeyW_Trap(hKey,lpSubKey);
}

extern "C"
LONG
xxx_RegDeleteValueW(
    HKEY    hKey,
    LPCWSTR lpValueName
    )
{
    return RegDeleteValueW_Trap(hKey,lpValueName);
}

extern "C"
LONG
xxx_RegEnumValueW(
    HKEY    hKey,
    DWORD   dwIndex,
    LPWSTR  lpValueName,
    LPDWORD lpcchValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE  lpData,
    LPDWORD lpcbData
    )
{
    // Convert the IN/OUT lpcchValueName to separate IN and OUT parameters.
    DWORD cchValueNameIn = (lpValueName && lpcchValueName) ? *lpcchValueName : 0;
    DWORD cbDataIn = (lpData && lpcbData) ? *lpcbData: 0;
    if (cchValueNameIn > MAXDWORD / sizeof(WCHAR)) {
        return ERROR_ARITHMETIC_OVERFLOW;
    }
    DWORD cbValueNameIn = cchValueNameIn * sizeof(WCHAR);
    return RegEnumValueW_Trap(hKey,dwIndex,lpValueName,cbValueNameIn,lpcchValueName,
            lpReserved,lpType,lpData,cbDataIn,lpcbData);
}

extern "C"
LONG
xxx_RegEnumKeyExW(
    HKEY        hKey,
    DWORD       dwIndex,
    LPWSTR      lpName,
    LPDWORD     lpcchName,
    LPDWORD     lpReserved,
    LPWSTR      lpClass,
    LPDWORD     lpcchClass,
    PFILETIME   lpftLastWriteTime
    )
{
    // Convert the IN/OUT lpcchName and lpcchClass values to separate IN and OUT paramters.
    DWORD cchNameIn = (lpName && lpcchName) ? *lpcchName : 0;
    DWORD cchClassIn = (lpClass && lpcchClass) ? *lpcchClass : 0;
    if ((cchNameIn > MAXDWORD / sizeof(WCHAR)) ||
        (cchClassIn > MAXDWORD / sizeof(WCHAR))) {
        return ERROR_ARITHMETIC_OVERFLOW;
    }
    DWORD cbNameIn = cchNameIn * sizeof(WCHAR);
    DWORD cbClassIn = cchClassIn * sizeof(WCHAR);
    return RegEnumKeyExW_Trap(hKey,dwIndex,lpName,cbNameIn,lpcchName,lpReserved,lpClass,
             cbClassIn,lpcchClass,lpftLastWriteTime);
}

extern "C"
LONG
xxx_RegOpenKeyExW(
    HKEY    hKey,
    LPCWSTR lpSubKey,
    DWORD   ulOptions,
    REGSAM  samDesired,
    PHKEY   phkResult
    )
{
    return RegOpenKeyExW_Trap(hKey,lpSubKey,ulOptions,samDesired,phkResult);
}

extern "C"
LONG
xxx_RegQueryInfoKeyW(
    HKEY        hKey,
    LPWSTR      lpClass,
    LPDWORD     lpcchClass,
    LPDWORD     lpReserved,
    LPDWORD     lpcSubKeys,
    LPDWORD     lpcchMaxSubKeyLen,
    LPDWORD     lpcchMaxClassLen,
    LPDWORD     lpcValues,
    LPDWORD     lpcchMaxValueNameLen,
    LPDWORD     lpcbMaxValueLen,
    LPDWORD     lpcbSecurityDescriptor,
    PFILETIME   lpftLastWriteTime
    )
{
    DWORD cchClassIn = (lpClass && lpcchClass) ? *lpcchClass : 0;
    if (cchClassIn > MAXDWORD / sizeof(WCHAR)) {
        return ERROR_ARITHMETIC_OVERFLOW;
    }
    DWORD cbClassIn = cchClassIn * sizeof(WCHAR);
    return RegQueryInfoKeyW_Trap (hKey, lpClass, cbClassIn, lpcchClass,
                lpReserved, lpcSubKeys, lpcchMaxSubKeyLen, lpcchMaxClassLen,
                lpcValues, lpcchMaxValueNameLen, lpcbMaxValueLen,
                lpcbSecurityDescriptor,lpftLastWriteTime);
}

extern "C"
LONG
xxx_RegQueryValueExW(
    HKEY    hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE  lpData,
    LPDWORD lpcbData
    )
{
    DWORD cbData = lpcbData ? *lpcbData : 0;
    return RegQueryValueExW_Trap(hKey,lpValueName,lpReserved,lpType,lpData,cbData,lpcbData);
}

extern "C"
LONG
xxx_RegSetValueExW(
            HKEY    hKey,
            LPCWSTR lpValueName,
            DWORD   Reserved,
            DWORD   dwType,
    CONST   BYTE*   lpData,
            DWORD   cbData
    )
{
    return RegSetValueExW_Trap(hKey,lpValueName,Reserved,dwType,lpData,cbData);
}

// OAK exports
extern "C"
BOOL
xxx_RegCopyFile(
    LPCWSTR lpszFile
    )
{
    return RegCopyFile_Trap(lpszFile);
}

extern "C"
BOOL
xxx_RegRestoreFile(
    LPCWSTR lpszFile
    )
{
    return RegRestoreFile_Trap(lpszFile);
}

extern "C"
LONG
xxx_RegSaveKey(
    HKEY                    hKey,
    LPCWSTR                 lpszFile,
    LPSECURITY_ATTRIBUTES   lpSecurityAttributes
    )
{
    return RegSaveKey_Trap(hKey, lpszFile, lpSecurityAttributes);
}

extern "C"
LONG
xxx_RegReplaceKey(
    HKEY    hKey,
    LPCWSTR lpszSubKey,
    LPCWSTR lpszNewFile,
    LPCWSTR lpszOldFile
    )
{
    return RegReplaceKey_Trap(hKey, lpszSubKey, lpszNewFile, lpszOldFile);
}

extern "C"
LONG xxx_RegSetKeySecurity(
    HKEY hKey, 
    SECURITY_INFORMATION SecurityInformation, 
    PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** RegSetKeySecurity() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    return S_OK;
}

extern "C"
LONG xxx_RegGetKeySecurity(
    HKEY hKey, SECURITY_INFORMATION SecurityInformation, 
    PSECURITY_DESCRIPTOR pSecurityDescriptor, 
    LPDWORD lpcbSecurityDescriptor)
{
    RETAILMSG(1, (TEXT("\r\n\r\n*** RegSetKeySecurity() called - NOT SUPPORTED IN CE7.\r\n\r\n")));
    ASSERT(0);
    if (lpcbSecurityDescriptor)
        *lpcbSecurityDescriptor = KEY_ALL_ACCESS;
    return S_OK;
}


extern "C"
BOOL
xxx_SetCurrentUser(
    LPCWSTR lpszUserName,
    LPBYTE  lpbUserData,
    DWORD   dwDataSize,
    BOOL    bCreateIfNew
    )
{
    return SetCurrentUser_Trap(lpszUserName, lpbUserData, dwDataSize, bCreateIfNew);
}

extern "C"
BOOL
xxx_SetUserData(
    LPBYTE  lpbUserData,
    DWORD   dwDataSize
    )
{
    return SetUserData_Trap(lpbUserData, dwDataSize);
}

extern "C"
BOOL
xxx_GetUserDirectory(
    LPWSTR  lpszBuffer,
    LPDWORD lpdwSize
    )
{
    return GetUserInformation_Trap(lpszBuffer, lpdwSize, USERINFO_DIRECTORY);
}

extern "C"
BOOL xxx_CryptProtectData(
    __in              const DATA_BLOB*              pDataIn,
    __in_opt          LPCWSTR                       szDataDescr,
    __in_opt          const DATA_BLOB*              pOptionalEntropy,
    __reserved        PVOID                         pvReserved,
    __in_opt          struct _CRYPTPROTECT_PROMPTSTRUCT*    pPromptStruct,
                      DWORD                         dwFlags,
    __out             DATA_BLOB*                    pDataOut            // out encr blob
    )
{
    if (!pDataIn || !pDataOut)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    BOOL fRet = PSLCryptProtectData(pDataIn->pbData,
                                   pDataIn->cbData,
                                   szDataDescr,
                                   pOptionalEntropy?pOptionalEntropy->pbData:NULL,
                                   pOptionalEntropy?pOptionalEntropy->cbData:0,
                                   dwFlags,
                                   pDataOut->pbData,
                                   pDataOut->cbData,
                                   &pDataOut->cbData);

    if ( !fRet &&
         ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) &&
         ( pDataOut->pbData == NULL ) )
    {
        // allocate output buffers from the local heap
        pDataOut->pbData = (unsigned char*)LocalAlloc(0, pDataOut->cbData);
        if (NULL == pDataOut->pbData)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return false;
        }

        fRet = PSLCryptProtectData(pDataIn->pbData,
                                   pDataIn->cbData,
                                   szDataDescr,
                                   pOptionalEntropy?pOptionalEntropy->pbData:NULL,
                                   pOptionalEntropy?pOptionalEntropy->cbData:0,
                                   dwFlags,
                                   pDataOut->pbData,
                                   pDataOut->cbData,
                                   &pDataOut->cbData);
        if (!fRet)
        {
            LocalFree(pDataOut->pbData);
            pDataOut->pbData = NULL;
            pDataOut->cbData = 0;
        }
    }
    return fRet;
}

extern "C"
BOOL
xxx_CryptUnprotectData(
    __in                const DATA_BLOB*            pDataIn,             // in encr blob
    __deref_opt_out_opt LPWSTR*                     ppszDataDescr,       // out
    __in_opt            const DATA_BLOB*            pOptionalEntropy,
    __reserved          PVOID                       pvReserved,
    __in_opt            struct _CRYPTPROTECT_PROMPTSTRUCT*  pPromptStruct,
                        DWORD                       dwFlags,
    __out               DATA_BLOB*                  pDataOut
    )
{
    if (!pDataIn || !pDataOut)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    DWORD dwDataDescrPresent = 0;
    DWORD dwDataDescrOffset = 0;
    BOOL fRet = PSLCryptUnprotectData(pDataIn->pbData,
                                     pDataIn->cbData,
                                     &dwDataDescrPresent,
                                     &dwDataDescrOffset,
                                     pOptionalEntropy?pOptionalEntropy->pbData:NULL,
                                     pOptionalEntropy?pOptionalEntropy->cbData:0,
                                     dwFlags,
                                     pDataOut->pbData,
                                     pDataOut->cbData,
                                     &pDataOut->cbData);

    if (!fRet && GetLastError() == ERROR_INSUFFICIENT_BUFFER && pDataOut->pbData == NULL)
    {
        // allocate output buffers from the local heap
        pDataOut->pbData = (unsigned char*)LocalAlloc(0, pDataOut->cbData);
        if (NULL == pDataOut->pbData)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return false;
        }

        fRet = PSLCryptUnprotectData(pDataIn->pbData,
                                     pDataIn->cbData,
                                     &dwDataDescrPresent,
                                     &dwDataDescrOffset,
                                     pOptionalEntropy?pOptionalEntropy->pbData:NULL,
                                     pOptionalEntropy?pOptionalEntropy->cbData:0,
                                     dwFlags,
                                     pDataOut->pbData,
                                     pDataOut->cbData,
                                     &pDataOut->cbData);
        if (!fRet)
        {
            LocalFree(pDataOut->pbData);
            pDataOut->pbData = NULL;
            pDataOut->cbData = 0;
        }
    }

    if (fRet && ppszDataDescr)
    {
        if (dwDataDescrPresent)
        {
            LPWSTR pszTmp = reinterpret_cast<LPWSTR>(pDataIn->pbData + dwDataDescrOffset);
            int    nLen   = wcslen (pszTmp);
            // The docs for this API say that the caller of this API is supposed to LocalFree any string buffer returned in
            // ppszDataDescr. So we need to LocalAlloc a copy of the string buffer.
            LPWSTR pszDataDescr = (WCHAR*)LocalAlloc(0, (nLen+1)*sizeof(WCHAR));
            if (pszDataDescr) {
                memcpy(pszDataDescr, pszTmp, nLen*sizeof(WCHAR));
                pszDataDescr[nLen] = 0; // EOS
            }
            *ppszDataDescr = pszDataDescr;
        }
    }
    return fRet;
}

extern "C"
BOOL
xxx_GenRandom(
    IN  DWORD   dwLen,
        PBYTE   pbBuffer
    )
{
    return PSLGenRandom(pbBuffer, dwLen);
}


extern "C"
HANDLE  
xxx_CeFindFirstRegChange(
    IN HKEY hKey, 
    IN BOOL bWatchSubTree, 
    IN DWORD dwNotifyFilter
    )
{
    return CeFindFirstRegChange_Trap( hKey, bWatchSubTree, dwNotifyFilter);
}

extern "C"
BOOL    
xxx_CeFindNextRegChange(
    HANDLE hNotify
    )
{
    return CeFindNextRegChange_Trap( hNotify);
}

extern "C"
BOOL    
xxx_CeFindCloseRegChange(
    HANDLE hNotify
    )
{
    return CeFindCloseRegChange_Trap( hNotify);
}

extern "C"
LONG 
xxx_CeRegTestSetValueW(
    HKEY hKey, 
    LPCWSTR lpValueName, 
    DWORD dwType, 
    CONST BYTE *lpOldData, 
    DWORD cbOldData, 
    CONST BYTE *lpNewData, 
    DWORD cbNewData, 
    DWORD dwFlags)
{
    return CeRegTestSetValueW_Trap(hKey, lpValueName, dwType, lpOldData, cbOldData, lpNewData, cbNewData, dwFlags);
}

extern "C"
LONG
xxx_CeRegGetInfo(
    HKEY hKey, 
    PCE_REGISTRY_INFO pInfo 
)
{
    DWORD dwRet;
    
    if (CeFsIoControlW_Trap( NULL, FSCTL_GET_REGISTRY_INFO,  &hKey,  sizeof(HKEY),  pInfo,  sizeof(CE_REGISTRY_INFO),  NULL, NULL)) {
        return ERROR_SUCCESS;
    }
    dwRet = GetLastError();
    return dwRet;
}   

extern "C"
LONG xxx_CeRegGetNotificationInfo(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable)
{
    if (CeGetFileNotificationInfo_Trap(h, dwFlags, lpBuffer, nBufferLength, lpBytesReturned, lpBytesAvailable)) {
        return ERROR_SUCCESS;
    } 
    return ERROR_GEN_FAILURE;
}


