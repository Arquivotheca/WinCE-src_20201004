//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "relfsd.h"
#include "cefs.h"

HANDLE RELFSD_FindFirstFileW(VolumeState *pVolume, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{
    HANDLE      hSearch = INVALID_HANDLE_VALUE;
    int         nFileId;
    SearchState   *pSearch;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : FindFirstFile ")));

    EnterCriticalSectionMM(g_pcsMain);

    nFileId = rfindfirst(pwsFileSpec, pfd);

    
    if (nFileId != -1)
    {
        pSearch = (SearchState *)LocalAlloc(0, sizeof(SearchState));
        if (pSearch)
        {
            pSearch->fs_Handle = nFileId;
            pSearch->fs_Volume = pVolume;
            pSearch->pFindCurrent = pSearch->abFindDataBuf;
            pSearch->dwFindBytesRemaining = 0;
            pSearch->bFindDone = FALSE;

            hSearch = FSDMGR_CreateSearchHandle(pVolume->vs_Handle, hProc,(ulong)pSearch);

            DEBUGMSG(ZONE_APIS, (TEXT("Handle: %d "), hSearch));


            if (hSearch == INVALID_HANDLE_VALUE)
            {
                LocalFree(pSearch);
                DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : Invalid Handle ")));

            }
        }
    }
    else
    {
        SetLastError(ERROR_NO_MORE_FILES);
    }

    LeaveCriticalSectionMM(g_pcsMain);

    return hSearch;
}


BOOL RELFSD_FindNextFileW(SearchState *pSearch, PWIN32_FIND_DATAW pfd)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : FindNextFile")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rfindnext(pSearch, pfd); 
    if (!fSuccess) {
        SetLastError( ERROR_NO_MORE_FILES);
    }    
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}

BOOL RELFSD_FindClose(SearchState *pSearch)
{
    BOOL fSuccess = TRUE;

    DEBUGMSG(ZONE_APIS, (TEXT("ReleaseFSD : FindClose")));

    EnterCriticalSectionMM(g_pcsMain);
    fSuccess = rfindclose((int)pSearch->fs_Handle);
    LocalFree( pSearch);
    LeaveCriticalSectionMM(g_pcsMain);

    return fSuccess;
}


