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
#ifndef _FSNOTIFY_H_
#define _FSNOTIFY_H_

#ifndef UNDER_CE
#define FILE_ACTION_CHANGE_COMPLETED        0x00010000
#endif

#define HANDLE_FLAG_FILEADDED               0x000000001
#define HANDLE_FLAG_FILECHANGED             0x000000002

typedef struct tagNOTIFYCOUNT NOTIFYCOUNT,*PNOTIFYCOUNT;
typedef struct tagNOTEVENTENTRY NOTEVENTENTRY,*PNOTEVENTENTRY;
typedef struct tagNOTHANDLEENTRY NOTHANDLEENTRY, *PNOTHANDLEENTRY;
typedef struct tagNOTDIRENTRY NOTDIRENTRY, *PNOTDIRENTRY;
typedef struct tagNOTVOLENTRY NOTVOLENTRY, *PNOTVOLENTRY;
typedef struct tagNOTFILEINFO NOTFILEINFO, *PNOTFILEINFO;
typedef struct tagNOTGETNEXTDATA NOTGETNEXTDATA, *PNOTGETNEXTDATA;

#define NOT_VOL_SIG 0x47544754
#define NOT_DIR_SIG 0x47444744
#define NOT_EVENT_SIG 0x44474447
#define NOT_HANDLE_SIG 0x54475447

#define MAX_NOTIFICATIONS 0x100

//
// Note that there is no explicit cleanup for this in the volume
// notification structure.  When the first NOTFILEINFO is added for process
// it will create a new NOTIFYCOUNT for that process.  When the last
// notification is removed for a process it will delete the NOTIFYCOUNT
// structure.  Each volume entry already has a critical section that we will
// use to synchronize this.
//
struct tagNOTIFYCOUNT {
    DWORD dwCount;
    HANDLE hProc;
    BOOL fOverflow;
    NOTIFYCOUNT* pNext;
};

struct tagNOTGETNEXTDATA {
    DWORD dwFlags;
    LPVOID lpBuffer;
    DWORD nBufferLength;
    LPDWORD lpBytesReturned;
    LPDWORD lpBytesAvailable;
};

struct tagNOTFILEINFO
{
    WCHAR       *szFileName;
    DWORD       dwFileNameLen;
    DWORD       dwAction;
    NOTFILEINFO *pNext;                
};

struct tagNOTEVENTENTRY{
    DWORD           dwSig;
    HANDLE          hProc;
    DWORD           dwFlags;
    BOOL            bWatchSubTree;
    HANDLE          hEvent;
    HANDLE          hNotify;
    DWORD           dwCount;
    NOTVOLENTRY    *pVolume;
    NOTFILEINFO    *pFileInfo;
    NOTFILEINFO    *pFileInfoLast;
    NOTDIRENTRY    *pDirectory;
    NOTEVENTENTRY  *pNext;
    NOTEVENTENTRY  *pPrevious;
};

struct tagNOTHANDLEENTRY{   
    DWORD           dwSig;
    DWORD           dwFlags;
    NOTDIRENTRY    *pDirectory;
    WCHAR          *szFileName;
    NOTHANDLEENTRY *pNext;
    NOTHANDLEENTRY *pPrevious;
};

struct tagNOTDIRENTRY{
    DWORD           dwSig;
    WCHAR           *szDirectory;
    HANDLE          hVolume;
    NOTEVENTENTRY   *pEvents;
    NOTHANDLEENTRY  *pHandles;
    NOTDIRENTRY     *pChild;
    NOTDIRENTRY     *pParent;
    NOTDIRENTRY     *pNext;
    NOTDIRENTRY     *pPrevious;
    BYTE  bDepth;
};

struct tagNOTVOLENTRY
{
    DWORD               dwSig;
    CRITICAL_SECTION    cs;
    WCHAR               *szVolumeName;
    NOTIFYCOUNT         *pNotifyCount;
    NOTDIRENTRY         *pDirEntry;        
    NOTEVENTENTRY       *pEvents;
    NOTHANDLEENTRY      *pHandles;
    NOTVOLENTRY         *pNextVolume;
};

#define ENTERNOTIFY(h) if (h) EnterCriticalSection(&(((NOTVOLENTRY *)h)->cs)) 
#define EXITNOTIFY(h) if (h) LeaveCriticalSection(&(((NOTVOLENTRY *)h)->cs))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Notification PSLs.
BOOL    NotifyReset(HANDLE h, NOTGETNEXTDATA *pData, DWORD DataSize);
BOOL    NotifyCloseEvent(HANDLE h);

#ifdef __cplusplus
}
#endif

#endif // _FSNOTIFY_H_
