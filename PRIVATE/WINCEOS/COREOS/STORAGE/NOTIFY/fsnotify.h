//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _FSNOTIFY_H_
#define _FSNOTIFY_H_


#define HANDLE_FLAG_FILEADDED		0x000000001
#define HANDLE_FLAG_FILECHANGED		0x000000002

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
    DWORD			dwFlags;
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
    NOTDIRENTRY         *pDirEntry;        
    NOTEVENTENTRY       *pEvents;
    NOTHANDLEENTRY      *pHandles;
    NOTVOLENTRY 		*pNextVolume;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


HANDLE  NotifyCreateVolume( WCHAR *szNotifyPoint);
void    NotifyDeleteVolume(HANDLE hVolume);
HANDLE  NotifyCreateFile( HANDLE hVolume, const WCHAR *szFileName);
HANDLE  NotifyCreateEvent(HANDLE hVolume, HANDLE hProc, const WCHAR *szPath, BOOL bWatchSubTree, DWORD dwFlags);
void    NotifyCloseHandle(HANDLE hVolume, HANDLE h);
BOOL    NotifyReset(HANDLE h, NOTGETNEXTDATA *pData);
void    NotifyPathChange(HANDLE hVolume, LPCWSTR szPath, BOOL bPath, DWORD dwAction);
void    NotifyMoveFile( HANDLE hFindNotifyHandle, LPCWSTR lpszExisting, LPCWSTR lpszNew);
void    NotifyHandleChange(HANDLE hVolume, HANDLE h, DWORD dwFlags);
void    NotifyHandleChange(HANDLE hVolume, HANDLE h, DWORD dwFlags);
BOOL    NotifyCloseEvent(HANDLE h);
BOOL    NotifyGetNextChange(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable);
BOOL    NotifyCloseChangeHandle(HANDLE h);

#define ENTERNOTIFY(h) if (h) EnterCriticalSection(&(((NOTVOLENTRY *)h)->cs)) 
#define EXITNOTIFY(h) if (h) LeaveCriticalSection(&(((NOTVOLENTRY *)h)->cs))

#ifdef __cplusplus
}
#endif

#endif // _FSNOTIFY_H_
