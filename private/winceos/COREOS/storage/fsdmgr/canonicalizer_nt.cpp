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

#ifndef UNDER_CE

#include "storeincludes.hpp"
#include "canonicalizer.hpp"
#include "bufferpool.hpp"
#include <intsafe.h>

#ifdef UNDER_CE
#include "iriutil.hpp"
using namespace IriUtil;
#endif // UNDER_CE

typedef BufferPool<WCHAR, MAX_PATH> MaxPathPool;
MaxPathPool g_MaxPathPool (5);

const WCHAR DEVICE_PREFIX[] = L"$device\\";

#ifdef UNDER_CE

typedef HANDLE (*PFN_FIND_FIRST_DEVICE) (DeviceSearchType, LPCVOID, PDEVMGR_DEVICE_INFORMATION);

static PFN_FIND_FIRST_DEVICE g_pfnFindFirstDevice;

static LRESULT TranslateLegacyDeviceName (const WCHAR* pLegacyName,
    __out_ecount(NameChars) WCHAR* pName, size_t NameChars)
{
    if (!g_pfnFindFirstDevice) {
        return ERROR_DEV_NOT_EXIST;
    }

    if (*pLegacyName == L'\\') {
        pLegacyName++;
    }

    DEVMGR_DEVICE_INFORMATION DeviceInfo;
    DeviceInfo.dwSize = sizeof (DEVMGR_DEVICE_INFORMATION);

    // Use the FindFirstDevice API to translate a legacy device name into
    // its full device name.
    HANDLE hFind = g_pfnFindFirstDevice (DeviceSearchByLegacyName, pLegacyName, &DeviceInfo);
    if (INVALID_HANDLE_VALUE == hFind) {
        return ERROR_DEV_NOT_EXIST;
    }
    VERIFY (FindClose (hFind));

    // Use the canonical version of the name returned by FindFirstDevice.
    if (!CeGetCanonicalPathNameW(DeviceInfo.szDeviceName, pName, NameChars, 0)) {
        // Preseve the error code set by CeGetCanonicalPathNameW
        return FsdGetLastError ();
    }

    DEBUGMSG (ZONE_APIS, (L"FSDMGR!TranslateLegacyDeviceName: \"%s\" --> \"%s\"", pLegacyName, pName));

    return ERROR_SUCCESS;
}

#else

static LRESULT TranslateLegacyDeviceName (const WCHAR* pLegacyName, WCHAR* pName,
    size_t NameChars)
{
    return ERROR_DEV_NOT_EXIST;
}

#endif // UNDER_CE

// Returns newly allocated path if successful, NULL if probe or
// canonicalization failed.  If lpszPathName is NULL this function will
// return NULL.  Free the buffer using SafeFreeCanonicalPath.
LPWSTR SafeGetCanonicalPathW (
    __in_z LPCWSTR lpszPathName,            // Caller buffer, NULL-terminated with MAX_PATH maximum length
    __out_opt size_t* pcchCanonicalPath
    )
{
    LPWSTR lpszCanonicalPathName = NULL;
    LRESULT lResult = ERROR_SUCCESS;

    if (!lpszPathName) {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    lpszCanonicalPathName = g_MaxPathPool.AllocateBuffer ();  // Always MAX_PATH long
    if (!lpszCanonicalPathName) {
        lResult = ERROR_OUTOFMEMORY;
        goto exit;
    }

    __try {
        if (!CeGetCanonicalPathNameW(lpszPathName, lpszCanonicalPathName, MAX_PATH, 0)) {
            // Preseve the error code set by CeGetCanonicalPathNameW
            lResult = FsdGetLastError ();
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        lResult = ERROR_INVALID_PARAMETER;
    }

    if (ERROR_SUCCESS != lResult) {
        goto exit;
    }

    if (IsLegacyDeviceName (lpszCanonicalPathName)) {

        // After canonicalization, perform a legacy device name translation
        // if this appears to be a legacy device name.

        LPWSTR lpszDeviceName = g_MaxPathPool.AllocateBuffer ();
        if (!lpszDeviceName) {
            lResult = ERROR_OUTOFMEMORY;
            goto exit;
        }

        lResult = TranslateLegacyDeviceName (lpszCanonicalPathName, lpszDeviceName, MAX_PATH);
        if (ERROR_SUCCESS != lResult) {
            // Failed TranslateLegacyDeviceName. A device with this name
            // probably doesn't exist on the system.
            SafeFreeCanonicalPath (lpszDeviceName);
            goto exit;
        }

        // Free the original canonical name and use the device name instead.
        SafeFreeCanonicalPath(lpszCanonicalPathName);
        lpszCanonicalPathName = lpszDeviceName;
    }

    if (pcchCanonicalPath) {
        
        // Return the length of the canonical path in characters, if requested.
        
        VERIFY(SUCCEEDED(StringCchLength(lpszCanonicalPathName, MAX_PATH, pcchCanonicalPath)));
    }

    lResult = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != lResult) {
        if (lpszCanonicalPathName) {
            SafeFreeCanonicalPath (lpszCanonicalPathName);
            lpszCanonicalPathName = NULL;
        }
        SetLastError (lResult);
    }
    return lpszCanonicalPathName;
}


// Symmetric function to free the path from SafeGetCanonicalPathW.
VOID SafeFreeCanonicalPath (LPWSTR lpszCPathName)
{
    DEBUGCHK(lpszCPathName);  // Catch cases where we free NULL
    if (lpszCPathName) {
        g_MaxPathPool.FreeBuffer (lpszCPathName);
    }
}

// Returns true if the string passed in is equivalent to L"\\"
BOOL IsRootPathName (const WCHAR* pPath)
{
    return (L'\\' == pPath[0]) && (L'\0' == pPath[1]);
}

void SimplifySearchSpec (__inout_z WCHAR* pSearchSpec)
{
    // Search for the last path token in the string.
    WCHAR* pLastToken = wcsrchr (pSearchSpec, L'\\');
    if (pLastToken &&
        (0 == wcscmp (L"\\*.*", pLastToken))) {
        // This search string ends in \*.*, truncate it to \* for DOS
        // behavior/compatibility.
        pLastToken[2] = L'\0';
    }
}

// determine if a path is, or is a sub-dir of, the root directory
BOOL IsPathInSystemDir(LPCWSTR lpszPath)
{
    BOOL fRet = FALSE;

    while ((*lpszPath == L'\\') || (*lpszPath == L'/')) {
        lpszPath++;
    }

    if (wcsnlen_s(lpszPath, MAX_PATH) > SYSDIRLEN) {
        if ((_wcsnicmp(lpszPath, SYSDIRNAME, SYSDIRLEN) == 0) &&
                ((*(lpszPath+SYSDIRLEN) == L'\\') ||
                (*(lpszPath+SYSDIRLEN) == L'/')) ||
                (*lpszPath == L'\0')) {
            // matches the system directory
            fRet = TRUE;
        }
    }
    return fRet;
}

// determine if a path is in the root directory, but not a sub-dir
BOOL IsPathInRootDir(LPCWSTR lpszPath)
{
    // skip all preceding slashes
    while ((*lpszPath == L'\\') || (*lpszPath == L'/')) {
        lpszPath++;
    }

    // make sure there are no more slashes in the string
    while ((L'\0' != *lpszPath) && (*lpszPath != L'\\') && (*lpszPath != L'/')) {
        lpszPath++;
    }

    // if this were not canonicalized, this would fail for paths ending in \

    // if we reached the end of the string, then this is not a sub-dir
    return (L'\0' == *lpszPath);
}

BOOL IsConsoleName (const WCHAR* pName)
{
    // If a file name is named "reg:" or "con" we assume it is a console
    // device. This is for legacy resons with relfsd.
    if ((0 == _wcsicmp (pName, L"\\reg:")) ||
        (0 == _wcsicmp (pName, L"\\con"))) {
        return TRUE;
    }
    return FALSE;
}

BOOL IsPsuedoDeviceName (const WCHAR* pName)
{
    // If a file name ends in a colon, assume it is a pseudo device.
    // This is typically something like L"\\Storage Card\\VOL:"
    size_t NameChars;
    if (FAILED (StringCchLengthW (pName, MAX_PATH, &NameChars))) {
        return FALSE;
    }
    return pName[NameChars-1] == L':';
}

BOOL IsLegacyDeviceName (const WCHAR* pName)
{
    if (*pName == L'\\') {
        pName++;
    }

    // Five char names where the 5th char is ':' are
    // considered legacy device names and are passed to device
    // manager for processing.
    size_t NameChars;
    if (FAILED (StringCchLengthW (pName, MAX_PATH, &NameChars))) {
        return FALSE;
    }
    return (NameChars == 5) && (pName[4] == L':');
}

BOOL IsFullDeviceName(const WCHAR* pName)
{
    size_t nCharCount = 0;

    if( pName[0] == L'\\' )
    {
        pName += 1;
    }

    if( FAILED(StringCchLength( pName, MAX_PATH, &nCharCount )) )
    {
        return FALSE;
    }

    if( nCharCount > _countof(DEVICE_PREFIX) )
    {
        if( _wcsnicmp( pName, DEVICE_PREFIX, _countof(DEVICE_PREFIX) - 1 ) == 0 )
        {
            if( (wcsrchr( pName, L'\\' ) - pName) == (_countof(DEVICE_PREFIX) - 2) )
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOL GetStoreNameFromDeviceName(const WCHAR* pName, __out_ecount(dwSizeInChars) WCHAR* strDeviceName, DWORD dwSizeInChars)
{
    size_t nCharCount = 0;

    if( FAILED(StringCchLength( pName, MAX_PATH, &nCharCount )) )
    {
        return FALSE;
    }

    if( IsFullDeviceName( pName ) )
    {
        if( pName[0] == L'\\' )
        {
            pName += 1;
        }

        if( SUCCEEDED(StringCchCopy( strDeviceName,
                                     dwSizeInChars,
                                     pName + _countof(DEVICE_PREFIX) - 1 )) )
        {
            if( SUCCEEDED(StringCchCat( strDeviceName,
                                        dwSizeInChars,
                                        L":" )) )
            {
                return TRUE;
            }
        }

        ASSERT(!"The device name is too long to fit in a storage name.");
    }
    else if( IsLegacyDeviceName( pName ) )
    {
        VERIFY(SUCCEEDED(StringCchCopy( strDeviceName, dwSizeInChars, pName )));
        return TRUE;
    }
    else
    {
        //
        // This executes the Autoload from Registry case.  It just needs to
        // be truncated.
        //
        StringCchCopy( strDeviceName, dwSizeInChars, pName );
        return TRUE;
    }

    return FALSE;
}

BOOL GetShortDeviceName(const WCHAR* pName, __out_ecount(dwSizeInChars) WCHAR* pShortName, DWORD dwSizeInChars)
{
    size_t nCharCount = 0;

    if( FAILED(StringCchLength( pName, MAX_PATH, &nCharCount )) )
    {
        return FALSE;
    }

    if( IsFullDeviceName( pName ) )
    {
        if( SUCCEEDED(StringCchCopy( pShortName,
                                     dwSizeInChars,
                                     pName + _countof(DEVICE_PREFIX) - 1 )) )
        {
            if( SUCCEEDED(StringCchCat( pShortName,
                                        dwSizeInChars,
                                        L":" )) )
            {
                return TRUE;
            }
        }

        ASSERT(!"The device name is too long to fit in a storage name.");
    }
    else if( IsLegacyDeviceName( pName ) )
    {
        VERIFY(SUCCEEDED(StringCchCopy( pShortName, dwSizeInChars, pName )));
        return TRUE;
    }
    else
    {
        //
        // This executes the Autoload from Registry case.  It just needs to
        // be truncated.
        //
        StringCchCopy( pShortName, dwSizeInChars, pName );
        return TRUE;
    }

    return FALSE;
}

BOOL IsSecondaryStreamName (const WCHAR* /* pName */)
{
    // There is currently no native stream support in FSDMGR.
    return FALSE;
}


// NOTENOTE: This API cannot be called during DllMain because it calls
// GetModuleHandle.
LRESULT InitializeCanonicalizerAPI ()
{
#ifdef UNDER_CE
    HMODULE hCoreDll = GetModuleHandle (L"coredll.dll");
    DEBUGCHK (hCoreDll);
    if (hCoreDll)
        g_pfnFindFirstDevice = (PFN_FIND_FIRST_DEVICE)FsdGetProcAddress(hCoreDll, L"FindFirstDevice");
#endif

    return NO_ERROR;
}

#endif /* !UNDER_CE */
