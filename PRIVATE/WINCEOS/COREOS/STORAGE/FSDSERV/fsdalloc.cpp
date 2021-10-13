//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    alloc.c

Abstract:

    This file handles the creation/destruction of all FSDMGR data
    structures (ie, FSDs, DSKs, VOLs and HDLs).

--*/

#include "fsdmgrp.h"


/*  AllocFSD - Allocate FSD structure and obtain FSD entry points
 *
 *  ENTRY
 *      hFSD == handle to new FSD
 *
 *  EXIT
 *      Pointer to internal FSD structure, or NULL if out of memory or
 *      FSD entry points could not be obtained (use GetLastError if you really
 *      want to know the answer).
 */

PFSD AllocFSD(HMODULE hFSD, HANDLE hDsk)
{
    PFSD pFSD;
    WCHAR baseName[MAX_FSD_NAME_SIZE];

    pFSD = dlFSDList.pFSDNext;
    while (pFSD != (PFSD)&dlFSDList) {
        if (CompareFSDs(pFSD->hFSD, hFSD))
            break;
        pFSD = pFSD->dlFSD.pFSDNext;
    }

    if (GetProcAddress(hFSD, L"FSD_CreateFileW")) {
        /* this guy uses simply FSD_ as a function prefix */
        wcscpy(baseName, L"FSD_");
    } else {
        DWORD cch, i;
        WCHAR wsTmp[MAX_PATH];

        /* derive the function prefix from the module's name. This is
         * a tricky step, since the letter case of the module name must
         * match the function prefix within the DLL.
         */

        cch = GetModuleFileName(hFSD, wsTmp, ARRAY_SIZE(wsTmp));
        if (cch) {
            PWSTR pwsTmp = &wsTmp[cch];
            while (pwsTmp != wsTmp)
            {
                pwsTmp--;
                if (*pwsTmp == '\\' || *pwsTmp == '/')
                {
                    pwsTmp++;
                    break;
                }
            }

            i = 0;
            while (*pwsTmp && *pwsTmp != '.' && i < ARRAY_SIZE(baseName)-2)
                baseName[i++] = *pwsTmp++;

            baseName[i]   = L'_';
            baseName[i+1] = 0;
        } else {
            return NULL;
        }
    }


    pFSD = (PFSD)LocalAlloc(LPTR, sizeof(FSD));

    if (pFSD) {
        INITSIG(pFSD, FSD_SIG);
        AddListItem((PDLINK)&dlFSDList, (PDLINK)&pFSD->dlFSD);
        pFSD->hFSD = hFSD;
        pFSD->hDsk = hDsk;
        wcscpy( pFSD->wsFSD, baseName);
    } else {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FSDMGR!AllocFSD: out of memory!\n")));
    }    

    if (pFSD) {
        if (!pFSD->pfnMountDisk || !pFSD->pfnUnmountDisk) {
            pFSD->pfnMountDisk = (PFNMOUNTDISK)GetFSDProcAddress(pFSD, TEXT("MountDisk"));
            pFSD->pfnUnmountDisk = (PFNMOUNTDISK)GetFSDProcAddress(pFSD, TEXT("UnmountDisk"));
        }
        if (!pFSD->pfnMountDisk || !pFSD->pfnUnmountDisk) {
            // CHeck to see if this is a filter
            if (!pFSD->pfnHookVolume|| !pFSD->pfnUnhookVolume) {
                pFSD->pfnHookVolume = (PFNHOOKVOLUME)GetFSDProcAddress(pFSD, TEXT("HookVolume"));
                pFSD->pfnUnhookVolume = (PFNUNHOOKVOLUME)GetFSDProcAddress(pFSD, TEXT("UnhookVolume"));
            }
            if (!pFSD->pfnHookVolume || !pFSD->pfnUnhookVolume) {
                // It doesn't have FSD entrypoints or FILTER entrypoints
                DeallocFSD(pFSD);
                pFSD = NULL;
            }    
        }
    }
    if (pFSD) {
        if (GetFSDProcArray(pFSD, pFSD->apfnAFS,  apfnAFSStubs,  apwsAFSAPIs,  ARRAY_SIZE(apwsAFSAPIs)) &&
            GetFSDProcArray(pFSD, pFSD->apfnFile, apfnFileStubs, apwsFileAPIs, ARRAY_SIZE(apwsFileAPIs)) &&
            GetFSDProcArray(pFSD, pFSD->apfnFind, apfnFindStubs, apwsFindAPIs, ARRAY_SIZE(apwsFindAPIs))) {
            pFSD->pRefreshVolume = (PREFRESHVOLUME)GetFSDProcAddress(pFSD, L"RefreshVolume");
        } else {
            DeallocFSD(pFSD);
            pFSD = NULL;
        }
    }    
    return pFSD;
}


/*  DeallocFSD - Deallocate FSD structure
 *
 *  ENTRY
 *      pFSD -> FSD
 *
 *  EXIT
 *      TRUE is FSD was successfully deallocated, FALSE if not (eg, if the FSD
 *      structure still has some DSK structures attached to it).
 */

BOOL DeallocFSD(PFSD pFSD)
{
    ASSERT(VALIDSIG(pFSD, FSD_SIG));

    RemoveListItem((PDLINK)&pFSD->dlFSD);
    LocalFree((HLOCAL)pFSD);
    return TRUE;
}


/*  AllocDisk - Allocate DSK structure for an FSD
 *
 *  ENTRY
 *      pFSD -> FSD structure
 *      pwsDsk -> disk name
 *      dwFlags == intial disk flags
 *
 *  EXIT
 *      Pointer to internal DSK structure, or NULL if out of memory
 */

PDSK AllocDisk(PFSD pFSD, PCWSTR pwsDsk, HANDLE hDsk, PDEVICEIOCONTROL pIOControl)
{
    DWORD dwFlags = DSK_NONE;
 
    if (pFSD == NULL)
        return NULL;


    if (hDsk == INVALID_HANDLE_VALUE) {
        hDsk = CreateFileW(pwsDsk,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL, OPEN_EXISTING, 0, NULL);

        if (hDsk != INVALID_HANDLE_VALUE)
            dwFlags &= ~DSK_READONLY;
        else if (GetLastError() == ERROR_ACCESS_DENIED) {
            hDsk = CreateFileW(pwsDsk,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
            if (hDsk != INVALID_HANDLE_VALUE)
                dwFlags |= DSK_READONLY;
        }
        pwsDsk = NULL;
    }

    if (hDsk == INVALID_HANDLE_VALUE) {
        DEBUGMSGW(ZONE_INIT || ZONE_ERRORS, (DBGTEXTW("FSDMGR!AllocDisk: CreateFile(%s) failed (%d)\n"), pwsDsk, GetLastError()));
        return NULL;
    }
    
   
    PDSK pDsk = (PDSK)LocalAlloc(LPTR, sizeof(DSK));

    if (pDsk) {
        INITSIG(pDsk, DSK_SIG);
        pDsk->pFSD = pFSD;
        pDsk->pDeviceIoControl = pIOControl;
        pDsk->pVol = NULL;
        pDsk->dwFilterVol = 0;
        pDsk->hPartition = NULL;
        if (pwsDsk) {
            wcscpy( pDsk->wsDsk, pwsDsk);
        } else {
            wcscpy( pDsk->wsDsk, L"");
        }   
    }

    if (pDsk) {
        pDsk->hDsk = hDsk;
        pDsk->dwFlags = dwFlags;
    }

    return pDsk;
}




/*  MarkDisk - Mark a DSK structure with one or more flags
 *
 *  ENTRY
 *      pDsk -> DSK structure
 *      dwFlags == one or flags to mark (see DSK_*)
 *
 *  EXIT
 *      None
 */

void MarkDisk(PDSK pDsk, DWORD dwFlags)
{
    ASSERT(VALIDSIG(pDsk, DSK_SIG));

    pDsk->dwFlags |= dwFlags;
    if ((pDsk->dwFlags & DSK_CLOSED) && (pDsk->hDsk)){
        CloseHandle(pDsk->hDsk);
        pDsk->hDsk = INVALID_HANDLE_VALUE;
    }
}


/*  UnmarkDisk - Unmark a DSK structure with one or more flags
 *
 *  ENTRY
 *      pDsk -> DSK structure
 *      dwFlags == one or flags to unmark (see DSK_*)
 *
 *  EXIT
 *      None
 */

void UnmarkDisk(PDSK pDsk, DWORD dwFlags)
{
    ASSERT(VALIDSIG(pDsk, DSK_SIG));

    pDsk->dwFlags &= ~dwFlags;
}


/*  DeallocDisk - Deallocate DSK structure
 *
 *  ENTRY
 *      pDsk -> DSK structure
 *
 *  EXIT
 *      TRUE is DSK was successfully deallocated, FALSE if not (eg, if the DSK
 *      structure still has some VOL structures attached to it).
 */

BOOL DeallocDisk(PDSK pDsk)
{
    ASSERT(VALIDSIG(pDsk, DSK_SIG));

    if (!pDsk->pVol) {
        if ((pDsk->hDsk != INVALID_HANDLE_VALUE) && (pDsk->hDsk))
            CloseHandle(pDsk->hDsk);
        LocalFree((HLOCAL)pDsk);
        return TRUE;
    }
    return FALSE;
}


/*  AllocVolume - Allocate VOL structure for a DSK
 *
 *  ENTRY
 *      pDSK -> DSK structure
 *      dwVolData == FSD-defined volume data
 *
 *  EXIT
 *      Pointer to internal VOL structure, or NULL if out of memory
 */

PVOL AllocVolume(PDSK pDsk, DWORD dwVolData)
{
    PVOL pVol;

    ASSERT(OWNCRITICALSECTION(&csFSD));

    if (pDsk == NULL)
        return NULL;

    pVol = (PVOL)LocalAlloc(LPTR, sizeof(VOL));

    if (pVol) {
        INITSIG(pVol, VOL_SIG);
        InitList((PDLINK)&pVol->dlHdlList);
        pDsk->pVol = pVol;
        pVol->dwFlags = 0;
        pVol->pDsk = pDsk;
        pVol->iAFS = INVALID_AFS;
        pVol->hThreadExitEvent = NULL;
        if (pDsk->pPrevDisk) {
            FILTERHOOK fh;
            PDSK pTemp = pDsk->pPrevDisk;
            while(pTemp) {
                fh.hVolume = dwVolData;
                fh.cbSize=(sizeof(FILTERHOOK));
                fh.pCloseVolume = (PCLOSEVOLUME)pDsk->pFSD->apfnAFS[AFSAPI_CLOSEVOLUME];
                fh.pCreateDirectoryW = (PCREATEDIRECTORYW)pDsk->pFSD->apfnAFS[AFSAPI_CREATEDIRECTORYW];  
                fh.pRemoveDirectoryW = (PREMOVEDIRECTORYW)pDsk->pFSD->apfnAFS[AFSAPI_REMOVEDIRECTORYW];  
                fh.pGetFileAttributesW = (PGETFILEATTRIBUTESW)pDsk->pFSD->apfnAFS[AFSAPI_GETFILEATTRIBUTESW];  
                fh.pSetFileAttributesW = (PSETFILEATTRIBUTESW)pDsk->pFSD->apfnAFS[AFSAPI_SETFILEATTRIBUTESW];  
                fh.pDeleteFileW = (PDELETEFILEW)pDsk->pFSD->apfnAFS[AFSAPI_DELETEFILEW];  
                fh.pMoveFileW = (PMOVEFILEW)pDsk->pFSD->apfnAFS[AFSAPI_MOVEFILEW];  
                fh.pDeleteAndRenameFileW = (PDELETEANDRENAMEFILEW)pDsk->pFSD->apfnAFS[AFSAPI_PRESTOCHANGOFILENAME];
                fh.pGetDiskFreeSpaceW = (PGETDISKFREESPACEW)pDsk->pFSD->apfnAFS[AFSAPI_GETDISKFREESPACE];
                fh.pNotify = (PNOTIFY)pDsk->pFSD->apfnAFS[AFSAPI_NOTIFY];
                fh.pRegisterFileSystemFunction = (PREGISTERFILESYSTEMFUNCTION)pDsk->pFSD->apfnAFS[AFSAPI_REGISTERFILESYSTEMFUNCTION];
                fh.pFindFirstFileW = (PFINDFIRSTFILEW)pDsk->pFSD->apfnAFS[AFSAPI_FINDFIRSTFILEW];
                fh.pCreateFileW = (PCREATEFILEW)pDsk->pFSD->apfnAFS[AFSAPI_CREATEFILEW];
                fh.pFsIoControl = (PFSIOCONTROL)pDsk->pFSD->apfnAFS[AFSAPI_FSIOCONTROL];

                fh.pFindNextFileW = (PFINDNEXTFILEW)pDsk->pFSD->apfnFind[FINDAPI_FINDNEXTFILEW];
                fh.pFindClose = (PFINDCLOSE)pDsk->pFSD->apfnFind[FINDAPI_FINDCLOSE];
                
                fh.pReadFile = (PREADFILE)pDsk->pFSD->apfnFile[FILEAPI_READFILE];
                fh.pReadFileWithSeek = (PREADFILEWITHSEEK)pDsk->pFSD->apfnFile[FILEAPI_READFILEWITHSEEK];
                fh.pWriteFile = (PWRITEFILE)pDsk->pFSD->apfnFile[FILEAPI_WRITEFILE];
                fh.pWriteFileWithSeek = (PWRITEFILEWITHSEEK)pDsk->pFSD->apfnFile[FILEAPI_WRITEFILEWITHSEEK];
                fh.pSetFilePointer = (PSETFILEPOINTER)pDsk->pFSD->apfnFile[FILEAPI_SETFILEPOINTER];
                fh.pGetFileSize = (PGETFILESIZE)pDsk->pFSD->apfnFile[FILEAPI_GETFILESIZE];
                fh.pGetFileInformationByHandle = (PGETFILEINFORMATIONBYHANDLE)pDsk->pFSD->apfnFile[FILEAPI_GETFILEINFORMATIONBYHANDLE];
                fh.pFlushFileBuffers = (PFLUSHFILEBUFFERS)pDsk->pFSD->apfnFile[FILEAPI_FLUSHFILEBUFFERS];
                fh.pGetFileTime = (PGETFILETIME)pDsk->pFSD->apfnFile[FILEAPI_GETFILETIME];
                fh.pSetFileTime = (PSETFILETIME)pDsk->pFSD->apfnFile[FILEAPI_SETFILETIME];
                fh.pSetEndOfFile = (PSETENDOFFILE)pDsk->pFSD->apfnFile[FILEAPI_SETENDOFFILE];
                fh.pDeviceIoControl = (PDEVICEIOCONTROL)pDsk->pFSD->apfnFile[FILEAPI_DEVICEIOCONTROL];
                fh.pCloseFile = (PCLOSEFILE)pDsk->pFSD->apfnFile[FILEAPI_CLOSEFILE];
                
                fh.pRefreshVolume = pDsk->pFSD->pRefreshVolume;
                if (pTemp->pFSD->pfnHookVolume) {
                    dwVolData = pTemp->pFSD->pfnHookVolume(pTemp, &fh);
                    pTemp->dwFilterVol = dwVolData;
                }   
                pVol->pDsk = pTemp;
                pTemp->pVol = pVol;
                pDsk = pTemp;
                pTemp = pTemp->pPrevDisk;
            }
        }    
        pVol->dwVolData = dwVolData;
    } else {
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FSDMGR!AllocVolume: out of memory!\n")));
    }    

    return pVol;
}


/*  DeallocVolume - Deallocate VOL structure
 *
 *  ENTRY
 *      pVol -> VOL structure
 *
 *  EXIT
 *      TRUE is VOL was successfully deallocated, FALSE if not (eg, if the VOL
 *      structure still has some HDL structures attached to it).
 *
 *      Currently, we never fail this function.  If handles are still attached to
 *      the volume, we forcibly close them;  that decision may need to be revisited....
 */

void UnHookAllFilters(PVOL pVol)
{
    if (pVol->pDsk) {
        while(pVol->pDsk->pNextDisk) {
            if (pVol->pDsk->dwFilterVol)
                pVol->pDsk->pFSD->pfnUnhookVolume(pVol->pDsk->dwFilterVol);
            pVol->pDsk = pVol->pDsk->pNextDisk;
        }
    }    
}

BOOL DeallocVolumeHandles(PVOL pVol)
{
    ASSERT(VALIDSIG(pVol, VOL_SIG) && OWNCRITICALSECTION(&csFSD));
        
    PHDL pHdl;
    pHdl = pVol->dlHdlList.pHdlNext;
    while (pHdl != (PHDL)&pVol->dlHdlList) {
        if (!(pHdl->dwFlags & HDL_INVALID)) {
            __try {
                pHdl->dwFlags |= HDL_INVALID;
                if (pHdl->dwFlags & HDL_FILE) {
                    ((PCLOSEFILE)pVol->pDsk->pFSD->apfnFile[FILEAPI_CLOSEFILE])(pHdl->dwHdlData);
                } else
                if (pHdl->dwFlags & HDL_SEARCH) {
                    ((PFINDCLOSE)pVol->pDsk->pFSD->apfnFind[FINDAPI_FINDCLOSE])(pHdl->dwHdlData);
                }   
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_ACCESS_DENIED);
            }
            pHdl->pVol = NULL;
        }    
        pHdl = pHdl->dlHdl.pHdlNext;
    }

    RemoveListItem((PDLINK)&pHdl->dlHdl);
    return TRUE;
}

BOOL DeallocVolume(PVOL pVol)
{
    ASSERT(VALIDSIG(pVol, VOL_SIG) && OWNCRITICALSECTION(&csFSD));

    if (pVol->iAFS != INVALID_AFS)
        return FALSE;

    if (pVol->pDsk) {
        while(pVol->pDsk->pNextDisk) {
            pVol->pDsk->pVol = NULL;
            pVol->pDsk = pVol->pDsk->pNextDisk;
        }
        pVol->pDsk->pVol = NULL; // Get the main FSD's Disk
    }    

    LocalFree((HLOCAL)pVol);

    return TRUE;
}


/*  AllocFSDHandle - Allocate HDL structure for a VOL
 *
 *  ENTRY
 *      pVol -> VOL structure
 *      hProc == originating process handle
 *      dwHldData == value from FSD's call to FSDMGR_CreateXXXXHandle
 *      dwFlags == initial flags for HDL (eg, HDL_FILE or HDL_FIND)
 *
 *  EXIT
 *      An application handle, or INVALID_HANDLE_VALUE if error
 */

HANDLE AllocFSDHandle(PVOL pVol, HANDLE hProc, HANDLE hNotify, DWORD dwHdlData, DWORD dwFlags)
{
    PHDL pHdl;
    HANDLE h = INVALID_HANDLE_VALUE;

    if (pVol == NULL)
        return NULL;

    pHdl = (PHDL)LocalAlloc(LPTR, sizeof(HDL));

    if (pHdl) {
        EnterCriticalSection(&csFSD);

        INITSIG(pHdl, (dwFlags & (HDL_FILE | HDL_PSUEDO))? HFILE_SIG : HSEARCH_SIG);
        AddListItem((PDLINK)&pVol->dlHdlList, (PDLINK)&pHdl->dlHdl);
        pHdl->pVol = pVol;
        pHdl->dwFlags = dwFlags;
        pHdl->dwHdlData = dwHdlData;
        pHdl->hNotify = hNotify;
        pHdl->h = CreateAPIHandle((dwFlags & (HDL_FILE | HDL_PSUEDO))? hFileAPI : hFindAPI, pHdl);
        if (pHdl->h) {
            h = pHdl->h;
            if (hProc == NULL) {
                if (hProc = GetCurrentProcess()) {
                    BOOL f = SetHandleOwner(h, hProc);
                    DEBUGMSG(ZONE_INIT,(DBGTEXT("FSDMGR!AllocFSDHandle: hProc switched to 0x%08x (%d)\n"), hProc, f));
                }
            }
            pHdl->hProc = hProc;
        }
        else
            DeallocFSDHandle(pHdl);

        LeaveCriticalSection(&csFSD);
    }
    else
        DEBUGMSG(ZONE_INIT || ZONE_ERRORS,(DBGTEXT("FSDMGR!AllocFSDHandle: out of memory!\n")));

    return h;
}


/*  DeallocFSDHandle - Deallocate HDL structure
 *
 *  ENTRY
 *      pHdl -> HDL structure
 *
 *  EXIT
 *      None
 */

void DeallocFSDHandle(PHDL pHdl)
{
    EnterCriticalSection(&csFSD);

    ASSERT(VALIDSIG(pHdl, (pHdl->dwFlags & (HDL_FILE | HDL_PSUEDO))? HFILE_SIG : HSEARCH_SIG));

    RemoveListItem((PDLINK)&pHdl->dlHdl);
    LeaveCriticalSection(&csFSD);

    LocalFree((HLOCAL)pHdl);
}



PDSK HookFilters(PDSK pDsk, PFSD pFSD)
{
    extern const TCHAR *g_szSTORAGE_PATH;
    extern const TCHAR *g_szFILE_SYSTEM_MODULE_STRING;
    WCHAR szRegKey[MAX_PATH];
    FSDLOADLIST *pFSDLoadList=NULL;
    HKEY hKey = NULL;
    // Filter search path is the following
    // 1) System\Storage Manager\AutoLoad\FileSystem\Filters 
    //              or
    //    System\Storage Manager\Profiles\ProfileName\FileSystem\Filters
    // 2) System\Storage Manager\FileSystem\Filters
    // 3) System\Storage Manager\Filters
    wsprintf( szRegKey, L"%s\\%s\\Filters", pFSD->szRegKey, pFSD->szFileSysName);
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey)) {
        pFSDLoadList = LoadFSDList( hKey, LOAD_FLAG_SYNC | LOAD_FLAG_ASYNC, szRegKey, pFSDLoadList, TRUE /* Reverse Order */);
        RegCloseKey(hKey);
    }   
    wsprintf( szRegKey, L"%s\\%s\\Filters", g_szSTORAGE_PATH, pFSD->szFileSysName);
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey)) {

        pFSDLoadList = LoadFSDList( hKey, LOAD_FLAG_SYNC | LOAD_FLAG_ASYNC, szRegKey, pFSDLoadList, TRUE /* Reverse Order */);
        RegCloseKey(hKey);
    }
    wsprintf( szRegKey, L"%s\\Filters", g_szSTORAGE_PATH);
    if (ERROR_SUCCESS ==  RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey)) {
        pFSDLoadList = LoadFSDList( hKey, LOAD_FLAG_SYNC | LOAD_FLAG_ASYNC, szRegKey, pFSDLoadList, TRUE /* Reverse Order */);
        RegCloseKey(hKey);
    }
    if (pFSDLoadList) {
        PFSDLOADLIST pTemp = NULL;
        while(pFSDLoadList) {
            HKEY hKeyFilter;
            TCHAR szDllName[MAX_PATH];
            PDSK pDskNew;
            HANDLE hFilter;
            // TODO: If we fail the load what happens ?
            if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, pFSDLoadList->szPath, 0, 0, &hKeyFilter)) {
                if (FsdGetRegistryString(hKeyFilter, g_szFILE_SYSTEM_MODULE_STRING, szDllName, sizeof(szDllName)/sizeof(WCHAR))) {
                    hFilter = LoadLibrary( szDllName);
                    if (hFilter) {
                        // TODO: In the process of hooking if any of the API's point to a stub then we should bypass ?
                        FSDINITDATA fid;
                        fid.dwFlags = pDsk->dwFlags;
                        fid.hDsk = pDsk->hDsk;
                        fid.hFSD = hFilter;
                        fid.pIoControl = pDsk->pDeviceIoControl;
                        fid.pNextDisk = NULL;
                        wcscpy( fid.szDiskName, L"Filter");
                        wcsncpy( fid.szFileSysName, pFSDLoadList->szName, 31); // FILESYSNAMESIZE = 32
                        fid.szFileSysName[31] = 0;
                        wcscpy( fid.szRegKey, szRegKey);
                        fid.pNextDisk = pDsk;
                        pDskNew = FSDLoad( &fid);
                        pDsk->pPrevDisk = pDskNew;
                        pDsk = pDskNew;
                    }
                }
                RegCloseKey( hKeyFilter);
            }
            pTemp = pFSDLoadList;
            pFSDLoadList = pFSDLoadList->pNext;
            delete pTemp;
        }
    }
    return pDsk;
}


PFSDLOADLIST LoadFSDList( HKEY hKey, DWORD dwFlags, TCHAR *szPath, PFSDLOADLIST pExisting, BOOL bReverse)
{
    extern const TCHAR *g_szFSD_ORDER_STRING;
    extern const TCHAR *g_szFSD_LOADFLAG_STRING;
    TCHAR szRegKey[MAX_PATH];
    DWORD cbSize = MAX_PATH;
    DWORD dwIndex = 0;
    DWORD dwItemFlag = 0;
    PFSDLOADLIST pFSDLoadList = pExisting;
    while(ERROR_SUCCESS == RegEnumKeyEx( hKey, dwIndex, szRegKey, &cbSize, NULL, NULL, 0, NULL)) {
        HKEY hKeyFS;
        if (ERROR_SUCCESS == RegOpenKeyEx( hKey, szRegKey, 0, 0, &hKeyFS)) {
            DWORD dwOrder;
            if (!FsdGetRegistryValue( hKeyFS, g_szFSD_ORDER_STRING, &dwOrder)) {
                dwOrder = 0xffffffff;
            }   
            FsdGetRegistryValue( hKeyFS, g_szFSD_LOADFLAG_STRING, &dwItemFlag);
            if (!(dwItemFlag & LOAD_FLAG_ASYNC) && !(dwItemFlag & LOAD_FLAG_SYNC)) {
                dwItemFlag |= LOAD_FLAG_ASYNC;
            }   
            if (dwItemFlag & dwFlags) {
                PFSDLOADLIST pTemp = pFSDLoadList;
                PFSDLOADLIST pPrev = NULL;
                while(pTemp) {
                    if (bReverse) {
                        if (pTemp->dwOrder < dwOrder) {
                            break;
                        }
                    } else {    
                        if (pTemp->dwOrder > dwOrder) {
                            break;
                        }   
                    }   
                    pPrev = pTemp;                  
                    pTemp = pTemp->pNext;
                }
                pTemp = new FSDLOADLIST;
                if (pTemp) {
                    if (szPath) {
                        wcscpy( pTemp->szPath, szPath);
                        wcscat( pTemp->szPath, L"\\");
                        wcscat( pTemp->szPath, szRegKey);
                    }
                    pTemp->dwOrder = dwOrder;
                    wcscpy( pTemp->szName, szRegKey);
                    if (pPrev) {
                        pTemp->pNext = pPrev->pNext;
                        pPrev->pNext = pTemp;
                    } else {
                        pTemp->pNext = pFSDLoadList;
                        pFSDLoadList = pTemp;
                    }
                }    
            }   
            RegCloseKey( hKeyFS);
        }
        dwIndex++;
        cbSize = MAX_PATH;
    }
    return pFSDLoadList;
}
