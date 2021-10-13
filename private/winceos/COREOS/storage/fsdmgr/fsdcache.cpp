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
#include "fsdcache.hpp"

HINSTANCE hCacheDll = NULL;
DWORD dwNumCaches = 0;

PFN_CREATECACHE pfnCreateCache;
PFN_DELETECACHE pfnDeleteCache;
PFN_RESIZECACHE pfnResizeCache;
PFN_CACHEDREAD pfnCachedRead;
PFN_CACHEDWRITE pfnCachedWrite;
PFN_FLUSHCACHE pfnFlushCache;
PFN_SYNCCACHE pfnSyncCache;
PFN_INVALIDATECACHE pfnInvalidateCache;
PFN_CACHEIOCONTROL pfnCacheIoControl;

extern HANDLE g_hResizingEvent;
extern CRITICAL_SECTION g_csAddRemove;

VOID LoadCacheDll(PDSK pDsk)
{
    TCHAR szCacheDll[MAX_PATH];
    
    if (FSDMGR_GetRegistryString(pDsk, L"CacheDll", szCacheDll, MAX_PATH)) {
               
        hCacheDll = LoadDriver (szCacheDll);
        
        if (!hCacheDll) {
            DEBUGMSG(ZONE_ERRORS,  (L"FSDMGR!LoadCacheDll: LoadLibrary failed on %s \r\n", szCacheDll));
        } else {
        
            pfnCreateCache = (PFN_CREATECACHE)FsdGetProcAddress(hCacheDll, TEXT("CreateCache"));
            pfnDeleteCache = (PFN_DELETECACHE)FsdGetProcAddress(hCacheDll, TEXT("DeleteCache"));
            pfnResizeCache = (PFN_RESIZECACHE)FsdGetProcAddress(hCacheDll, TEXT("ResizeCache"));
            pfnCachedRead = (PFN_CACHEDREAD)FsdGetProcAddress(hCacheDll, TEXT("CachedRead"));
            pfnCachedWrite = (PFN_CACHEDWRITE)FsdGetProcAddress(hCacheDll, TEXT("CachedWrite"));
            pfnFlushCache = (PFN_FLUSHCACHE)FsdGetProcAddress(hCacheDll, TEXT("FlushCache"));
            pfnSyncCache = (PFN_SYNCCACHE)FsdGetProcAddress(hCacheDll, TEXT("SyncCache"));
            pfnInvalidateCache = (PFN_INVALIDATECACHE)FsdGetProcAddress(hCacheDll, TEXT("InvalidateCache"));
            pfnCacheIoControl = (PFN_CACHEIOCONTROL)FsdGetProcAddress(hCacheDll, TEXT("CacheIoControl"));

            // These standard functions are required to be implemented.
            if (!pfnCreateCache || !pfnDeleteCache || !pfnResizeCache || !pfnCachedRead ||
                !pfnCachedWrite || !pfnFlushCache || !pfnSyncCache || !pfnCacheIoControl ||
                !pfnInvalidateCache) {
                DEBUGMSG(ZONE_ERRORS,  (L"FSDMGR!LoadCacheDll: Required entry points in driver not found\r\n"));
                FreeLibrary(hCacheDll);       
                hCacheDll = NULL;
            }
        }
    }
    
    // If cache dll not found/valid, use null cache as stubs
    if (!hCacheDll) {
        pfnCreateCache = CreateCache;
        pfnDeleteCache = DeleteCache;
        pfnResizeCache = ResizeCache;
        pfnCachedRead = CachedRead;
        pfnCachedWrite = CachedWrite;
        pfnFlushCache = FlushCache;
        pfnSyncCache = SyncCache;
        pfnInvalidateCache = InvalidateCache;
        pfnCacheIoControl = CacheIoControl;

        // Initialize null cache objects
        InitNullCache();
    }

}

DWORD FSDMGR_CreateCache( PDSK pDsk, DWORD dwStart, DWORD dwEnd, DWORD dwCacheSize, DWORD dwBlockSize, DWORD dwCreateFlags)
{
    if (!dwNumCaches) {
        LoadCacheDll(pDsk);
    }

    DEBUGCHK (pfnCreateCache);
    DWORD dwRet;
    __try {
        dwRet = pfnCreateCache (pDsk, dwStart, dwEnd, dwCacheSize, dwBlockSize, dwCreateFlags);
    } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
        dwRet = INVALID_CACHE_ID;
        return dwRet;
    }    

    if (dwRet != INVALID_CACHE_ID)
        dwNumCaches++;

    return dwRet;
}

DWORD FSDMGR_DeleteCache( DWORD dwCacheId)
{   
    if (!pfnDeleteCache) {
        return ERROR_NOT_SUPPORTED;
    }
    
    DWORD dwRet = pfnDeleteCache (dwCacheId);

    if (--dwNumCaches == 0) {
        
        if (hCacheDll) {
            FreeLibrary(hCacheDll);       
        } else {
            DeInitNullCache();
        }
        
        hCacheDll = NULL;
    }    

    return dwRet;
}

DWORD FSDMGR_ResizeCache( DWORD dwCacheId, DWORD dwSize, DWORD dwResizeFlags)
{
    if (pfnResizeCache) {
        __try {
            return pfnResizeCache (dwCacheId, dwSize, dwResizeFlags);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}

DWORD FSDMGR_CachedRead(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwReadFlags)
{
    if (pfnCachedRead) {
        __try {
            return pfnCachedRead (dwCacheId, dwBlockNum, dwNumBlocks, pBuffer, dwReadFlags);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}

DWORD FSDMGR_CachedWrite(DWORD dwCacheId, DWORD dwBlockNum, DWORD dwNumBlocks, PVOID pBuffer, DWORD dwWriteFlags)
{
    if (pfnCachedWrite) {
        __try {
            return pfnCachedWrite (dwCacheId, dwBlockNum, dwNumBlocks, pBuffer, dwWriteFlags);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}

DWORD FSDMGR_FlushCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlushFlags)
{
    if (pfnFlushCache) {
        __try {
            return pfnFlushCache (dwCacheId, pSectorList, dwNumEntries, dwFlushFlags);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}

DWORD FSDMGR_SyncCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwSyncFlags)
{
    if (pfnSyncCache) {
        __try {
            return pfnSyncCache (dwCacheId, pSectorList, dwNumEntries, dwSyncFlags);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}

DWORD FSDMGR_InvalidateCache(DWORD dwCacheId, PSECTOR_LIST_ENTRY pSectorList, DWORD dwNumEntries, DWORD dwFlags)
{
    if (pfnInvalidateCache) {
        __try {
            return pfnInvalidateCache (dwCacheId, pSectorList, dwNumEntries, dwFlags);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}


BOOL FSDMGR_CacheIoControl(DWORD dwCacheId, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    if (pfnCacheIoControl) {
        __try {
            return pfnCacheIoControl (dwCacheId, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    } else {
        return ERROR_NOT_SUPPORTED;
    }    
}
