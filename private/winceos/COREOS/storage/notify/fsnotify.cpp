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
#include <windows.h>
#include <fsnotify_api.h>
#include "fsnotify.h"

HANDLE g_hNotifyAPI = NULL;
CRITICAL_SECTION g_csMain;
NOTVOLENTRY *g_pVolumeList = NULL;
NOTVOLENTRY *g_pRootVolume = NULL;

#ifdef UNDER_CE

// An internal handle-based API set is used for the notification engine. 
// Handles created based on this API set are not returned to the caller, but
// are owned by the caller so that they will be cleaned up on process exit.
// Handles returnd to the caller are event handles, and notification handles
// stored as event data.

#define NotifyReset_Trap(hf, pfd, cb) \
            _DIRECT_HANDLE_CALL(BOOL, HT_FIND, 2, FALSE, (HANDLE, PNOTGETNEXTDATA, DWORD), 2, (hf, pfd, cb))

// Internal interface only.
CONST PFNVOID apfnFindNotifyInt[] = {
        (PFNVOID)NotifyCloseEvent,
        (PFNVOID)NULL,
        (PFNVOID)NotifyReset
};

// External interface is not exposed by this API.
CONST PFNVOID apfnFindNotifyExt[] = {
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)NULL
};

CONST ULONGLONG asigFindNotify[] = {
        FNSIG1(DW),                                     // FindClose
        FNSIG0(),                                       //
        FNSIG3(DW, IO_PTR, DW)                          // FindNextFileW
};

#else // UNDER_CE


#define FILE_NOTIFY_CHANGE_CEGETINFO 0x80000000
#define VERIFY(a) a
#define DEBUGCHK(a)

#define NotifyReset_Trap(hf, pfd, cb)   NotifyReset(hf, pfd, cb)

#endif // !UNDER_CE


struct FileEvent
{
    const WCHAR *szFileName ;
    DWORD dwFlags ;
    DWORD dwAction ;

    FileEvent() : szFileName(NULL), dwFlags(0), dwAction(0) {}
} ;


void AddFileToEvent(NOTEVENTENTRY *pEvent, const WCHAR *szFileName, DWORD dwFlags, DWORD dwAction);
void CheckEventOnPath(NOTEVENTENTRY *pEvent, BOOL bSubTree, BOOL bPath, const FileEvent *);
void NotifyHandleChangeEx(HANDLE hVolume, HANDLE h, DWORD dwFlags, DWORD dwAction);

inline BOOL StrAllocCopy(WCHAR **pszDest, const WCHAR *szSource)
{
    DWORD cchDest = wcslen(szSource)+1;
    if (cchDest == 0) return FALSE;
    *pszDest = new WCHAR[cchDest];
    if (!*pszDest)
        return FALSE;    
    wcscpy_s(*pszDest,cchDest,szSource);
    return TRUE;
}

HANDLE FindNotifyHandleOnDirectory( NOTDIRENTRY *pDirectory, HANDLE hEvent) 
{
    HANDLE hNotify = NULL;
    while(pDirectory) {
        NOTEVENTENTRY *pEvent = pDirectory->pEvents;
        while(pEvent) {
            if (pEvent->hEvent == hEvent) {
                hNotify = pEvent->hNotify;
                break;
            }
            pEvent = pEvent->pNext;
        }
        if (hNotify || (pDirectory->pChild && (NULL != (hNotify = FindNotifyHandleOnDirectory(pDirectory->pChild, hEvent))))) {
            break;
        }   
        pDirectory = pDirectory->pNext;
    }
    return hNotify;
}

HANDLE GetNotifiyHandle(HANDLE hEvent) 
{
    HANDLE hNotify = NULL;
    EnterCriticalSection( &g_csMain);
    NOTVOLENTRY *pVolume = g_pVolumeList;
    while(pVolume) {
        ENTERNOTIFY( pVolume);
        NOTEVENTENTRY *pEvent = pVolume->pEvents;
        while(pEvent) {
            if (pEvent->hEvent == hEvent) {
                hNotify = pEvent->hNotify;
                break;
            }
            pEvent = pEvent->pNext;
        }
        if (hNotify || (pVolume->pDirEntry && (NULL != (hNotify = FindNotifyHandleOnDirectory( pVolume->pDirEntry, hEvent))))) {
            EXITNOTIFY(pVolume);
            break;
        } 
        EXITNOTIFY(pVolume);
        pVolume = pVolume->pNextVolume;
    }
    LeaveCriticalSection( &g_csMain);
    return hNotify;
}


inline BOOL IsValidData(HANDLE h, DWORD dwSig)
{
    BOOL bRet = TRUE;
    NOTHANDLEENTRY *pHandle = (NOTHANDLEENTRY *)h;
    if (!h || (h == INVALID_HANDLE_VALUE)) {
        bRet = FALSE;
    }
    if (bRet) {
        __try {
            if (pHandle->dwSig != dwSig) 
                bRet = FALSE;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            bRet = FALSE;
        }
    }
    return bRet;
}

NOTDIRENTRY *CreateDirEntry(WCHAR *szName, HANDLE hVolume)
{
    NOTDIRENTRY *pDirEntry = new NOTDIRENTRY;
    if (!pDirEntry) { 
        return NULL;
    }   
    DWORD dwSize = (wcslen(szName)+1)*sizeof(WCHAR);
    pDirEntry->dwSig = NOT_DIR_SIG;
    pDirEntry->pChild = NULL;
    pDirEntry->pParent = NULL;
    pDirEntry->pNext = NULL;
    pDirEntry->pPrevious = NULL;
    pDirEntry->pHandles = NULL;
    pDirEntry->hVolume = hVolume;
    pDirEntry->pEvents = NULL;
    pDirEntry->szDirectory = new WCHAR[dwSize];
    if (pDirEntry->szDirectory) {
        memcpy( pDirEntry->szDirectory, szName, dwSize);
    }     
    return pDirEntry;
}   

// Called by a file system during mount to create the base 
// notification object associated with a mounted volume.
HANDLE NotifyCreateVolume( WCHAR *szNotifyPoint)
{
    DWORD dwSize = (wcslen(szNotifyPoint)+1)*sizeof(WCHAR);
    NOTVOLENTRY *pVolume = new NOTVOLENTRY;
    if (!pVolume) { 
        return INVALID_HANDLE_VALUE;
    }   
    InitializeCriticalSection(&pVolume->cs);
    pVolume->dwSig = NOT_VOL_SIG;
    pVolume->pDirEntry = NULL;
    pVolume->szVolumeName = new WCHAR[dwSize];
    pVolume->pHandles = NULL;
    pVolume->pEvents = NULL;
    pVolume->pNextVolume = NULL;
    pVolume->pNotifyCount = NULL;
    if (pVolume->szVolumeName) {
        memcpy( pVolume->szVolumeName, szNotifyPoint, dwSize);
    }   
#ifdef UNDER_CE    
    if (!g_hNotifyAPI) {
        InitializeCriticalSection( &g_csMain);
        g_hNotifyAPI = CreateAPISet("FNOT", _countof(asigFindNotify), (const PFNVOID * )apfnFindNotifyExt, asigFindNotify);
        RegisterDirectMethods (g_hNotifyAPI, (const PFNVOID * )apfnFindNotifyInt);
        RegisterAPISet(g_hNotifyAPI, HT_FIND | REGISTER_APISET_TYPE);
    }
    EnterCriticalSection( &g_csMain);
    
    if (!g_pVolumeList) {
        g_pVolumeList = pVolume;
    }else
    {
        pVolume->pNextVolume = g_pVolumeList->pNextVolume ;
        g_pVolumeList->pNextVolume = pVolume ;
    }

    if (wcsncmp(pVolume->szVolumeName,L"",1) == 0)
    {
        ASSERT(g_pRootVolume == NULL) ;
        g_pRootVolume = pVolume ;
    }
    
    LeaveCriticalSection( &g_csMain);
#endif    
    return (HANDLE)pVolume;
}

void DeleteHandles(NOTHANDLEENTRY *pHandles)
{
    while(pHandles) {
        NOTHANDLEENTRY *pHandle;
        pHandle = pHandles;
        pHandles = pHandle->pNext;
        if (pHandle->szFileName)
            delete [] pHandle->szFileName;
        delete pHandle;
    }
}

void deleteNOTFILEINFO(NOTEVENTENTRY *pEvent, PNOTIFYCOUNT *ppCount, PNOTIFYCOUNT pPrevCount, BOOL fDeleteCount = TRUE)
{
    //
    // Lock the volume before calling deleteNOTFILEINFO
    //

    ASSERT(ppCount);
    PNOTIFYCOUNT & pCount(*ppCount);
    NOTFILEINFO *pFileInfo = pEvent->pFileInfo;

    if (!pFileInfo) {
        ASSERT(pEvent->dwCount == 0);
        ASSERT(pEvent->pFileInfoLast == NULL);
        return;
    }

    pEvent->pFileInfo = pFileInfo->pNext;
    pEvent->dwCount-- ;

    if (pEvent->pFileInfoLast == pFileInfo) {
        ASSERT(pFileInfo->pNext == NULL);
        ASSERT(pEvent->dwCount == 0);
        pEvent->pFileInfoLast = NULL;
    }

    delete [] pFileInfo->szFileName;
    delete pFileInfo;

    if (pCount) {
        ASSERT(pCount->dwCount > 0);
        if (pCount->dwCount == 0) return;
        pCount->dwCount -= 1;
        if (pCount->dwCount == 0 && fDeleteCount) {
            if (pPrevCount) {
                pPrevCount->pNext = pCount->pNext;
            } else {
                pEvent->pVolume->pNotifyCount = pCount->pNext;
            }
            delete pCount;
            pCount = NULL;
        }
    }

}

void DeleteFileInfos(NOTEVENTENTRY *pEvent, BOOL fDeleteCount = TRUE)
{
    //
    // Lock the volume before calling DeleteFileInfos.
    //
    PNOTIFYCOUNT pCount = NULL;
    PNOTIFYCOUNT pPrevCount = NULL;

    if (pEvent->pVolume) {
        pCount = pEvent->pVolume->pNotifyCount;
        while (pCount && pCount->hProc != pEvent->hProc) {
            pPrevCount = pCount;
            pCount = pCount->pNext;
        }
    }

    if (!pEvent->pFileInfo) pEvent->dwCount = 0;

    while (pEvent->pFileInfo) {
        deleteNOTFILEINFO(pEvent,&pCount,pPrevCount,fDeleteCount);
    }

    ASSERT(pEvent->dwCount == 0);
    ASSERT(pEvent->pFileInfoLast == NULL);
    pEvent->dwCount = 0;
    pEvent->pFileInfoLast = NULL;
}

void ClearEvents(NOTEVENTENTRY *pEvents)
{
    FileEvent FE ;
    FE.szFileName = L"\\" ;
    FE.dwFlags    = FILE_NOTIFY_CHANGE_DIR_NAME ;
    FE.dwAction   = FILE_ACTION_REMOVED ;

    CheckEventOnPath(pEvents, TRUE, TRUE, &FE);

    NOTEVENTENTRY *pEvent = pEvents;
    while(pEvent) {
        pEvent->pVolume = NULL;
        pEvent = pEvent->pNext;
    }
}


void DeleteNotDirEntry(NOTDIRENTRY *pNotify)
{
    pNotify->pParent = NULL; 
    while(pNotify) {
        DeleteHandles( pNotify->pHandles);
        ClearEvents( pNotify->pEvents);
        NOTDIRENTRY *pNext;
        if (pNotify->pChild) {
            DeleteNotDirEntry(pNotify->pChild);
        }
        pNext = pNotify->pNext;
        pNotify->pPrevious = NULL; 
        if (pNotify->szDirectory) {
            delete [] pNotify->szDirectory;
        }   
        delete pNotify;     
        pNotify = pNext;
    }   
}

#if 0
WCHAR g_szDumpPath[MAX_PATH];
void DumpDirEntry(NOTDIRENTRY *pNotify)
{
    pNotify->pParent = NULL; 
    while(pNotify) {
        if (!pNotify->bDepth) {
            wcscpy( g_szDumpPath, L"\\");
            wcscat( g_szDumpPath, pNotify->szDirectory);
        } else {
            wcscat(g_szDumpPath, L"\\");
            wcscat(g_szDumpPath, pNotify->szDirectory);
        }    
        if (pNotify->pChild) {
            DumpDirEntry(pNotify->pChild);
        }
        if (!pNotify->bDepth) {
//          NKDbgPrintfW( L"%s\r\n",szPath);
        }    
        pNotify = pNotify->pNext;
    }   
}
#endif

// Called by a file system during dismount to free the
// notification object associated with a mounted volume.
void NotifyDeleteVolume(HANDLE hVolume)
{
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
    if (pVolume) {
        ENTERNOTIFY(hVolume);
        if (pVolume->pDirEntry) {
#if 0        
            DumpDirEntry(pVolume->pDirEntry);
#endif            
            DeleteNotDirEntry( pVolume->pDirEntry);
        }   
        DeleteHandles(pVolume->pHandles);
        ClearEvents(pVolume->pEvents);
        EXITNOTIFY(hVolume);
        EnterCriticalSection( &g_csMain);
        if (g_pVolumeList == pVolume) {
            g_pVolumeList = pVolume->pNextVolume;
        } else {
            NOTVOLENTRY * pCurVolume = g_pVolumeList;
            while(pCurVolume) {
                if (pCurVolume->pNextVolume == pVolume) {
                    pCurVolume->pNextVolume = pVolume->pNextVolume;
                    break;
                }
                pCurVolume = pCurVolume->pNextVolume;
            }
        }    
        if (wcsncmp(pVolume->szVolumeName,L"",1) == 0)
        {
            ASSERT(g_pRootVolume) ;
            ASSERT(g_pRootVolume == pVolume) ;
            g_pRootVolume = NULL ;
        }
    
        LeaveCriticalSection( &g_csMain);
        DeleteCriticalSection(&pVolume->cs);
        if (pVolume->szVolumeName) {
            delete [] pVolume->szVolumeName;
        }   
        delete pVolume;
    }
}


// Given a NOTDIRENTRY entry and a path, this function returns the
// trailing part of the path starting beyond the directory that is
// pointed to by pCurrent
// DO NOT Free the memory this function returns

const WCHAR *GetTrailingName(NOTDIRENTRY * pCurrent, const WCHAR *szFullPath)
{
    if (pCurrent->pParent)
    {
        szFullPath = GetTrailingName(pCurrent->pParent, szFullPath) ;        
    }
    else
    {
        if (*szFullPath == L'\\') ++szFullPath ; // Strip off any leading slash
        if (*szFullPath == L'\\') ++szFullPath ; // UNC paths can have two leading slashes
    }

    const WCHAR * dirname = pCurrent->szDirectory ;
    while(*dirname)
    {
        ASSERT(tolower(*dirname) == tolower(*szFullPath)) ;
        ++dirname ;
        ++szFullPath ;
    }

    if (*szFullPath)
    {
        ASSERT(*szFullPath == L'\\') ;
        ++szFullPath ;
        ASSERT(*szFullPath) ;
    }

    return szFullPath ;
}


// bPath - if TRUE, the last file/dirname in the path is not ignored
//         Typically filename creation APIs set bPath to FALSE as
//         FindFirstChangeNotification listens on directories and we
//         do not want to trigger events if a file of the same name
//         created. Directory creation APIs set bPath to TRUE to match
//         the whole path
//
// bFind - TRUE implies we want to find the entry
//       - FALSE implies we want to insert an entry

NOTDIRENTRY *InsertOrFindDirEntry(HANDLE hVolume,const WCHAR *szFullPath, WCHAR **pszFileName, BOOL bPath, BOOL bFind, BOOL *pbSubTree = NULL) 
{
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
    NOTDIRENTRY *pDirectory = pVolume->pDirEntry;
    WCHAR *szPath = NULL;
    while(*szFullPath && (*szFullPath == L'\\'))
        szFullPath++;
    if (!StrAllocCopy(&szPath, szFullPath)) {
        return NULL;
    }
    WCHAR *szCurrent = szPath;
    NOTDIRENTRY *pParent = NULL;
    NOTDIRENTRY *pCurrent = NULL;
    if (pszFileName) {
       //If this ASSERT fires it means the caller didnt initialize pszFileName
       ASSERT(*pszFileName == NULL);  
       *pszFileName = NULL;
    }
    BYTE bDepth = 0;
    while(*szCurrent) {
        WCHAR *szTemp = szCurrent;
        while(*szTemp && (*szTemp != L'\\'))
            szTemp++;
        if (*szTemp || bPath) {                 
            if (*szTemp) {
                *szTemp = L'\0';
                szTemp++;
            } else {
                bPath = FALSE;
            }    
            NOTDIRENTRY *pPrevious = NULL;
#if 0            
            if (wcscmp( szCurrent, L".") == 0) {
                szCurrent = szTemp;
                continue;
            }
            if (wcscmp( szCurrent, L"..") == 0) {
                szCurrent = szTemp;
                if (pCurrent && pCurrent->pPrevious)
                    pCurrent = pCurrent->pPrevious;
                continue;   
            }
#endif            
            int nCmp = -1;
            while(pDirectory && pDirectory->szDirectory && ( (nCmp = _wcsicmp(szCurrent, pDirectory->szDirectory)) > 0)) {
                pPrevious = pDirectory;
                pDirectory = pDirectory->pNext;
                nCmp = -1;
            }
            if (nCmp != 0) {
                if (bFind) {
                    if (pbSubTree && (*szTemp || !pszFileName)) {
                        *pbSubTree  = FALSE;    
                    }
                    if (*szTemp) {
                        if (szTemp && *szTemp && pszFileName) {
                            StrAllocCopy(pszFileName, szTemp);
                        }
                    } else {
                        if (szCurrent && *szCurrent && pszFileName && pbSubTree) {
                            StrAllocCopy(pszFileName, szCurrent);
                        }
                    }    
                    if (szPath) {
                        delete [] szPath;
                    }   
                    return pCurrent;
                }
                if (NULL == (pCurrent = CreateDirEntry(szCurrent, (HANDLE)pVolume))) 
                    return NULL; // Out of Memory
                pCurrent->bDepth = bDepth;
                if (pPrevious) {
                    pPrevious->pNext = pCurrent;
                } else {
                    if (pParent) {
                        pParent->pChild = pCurrent;
                    } else {
                        pVolume->pDirEntry = pCurrent;
                    }
                }   
                if (pDirectory) {
                    pDirectory->pPrevious = pCurrent;
                } 
                pCurrent->pPrevious = pPrevious;
                pCurrent->pNext = pDirectory;
                pCurrent->pParent = pParent;
                pDirectory = NULL;
                if (!pVolume->pDirEntry)
                    pVolume->pDirEntry = pCurrent;
            } else {
                pCurrent = pDirectory;
                pDirectory = pDirectory->pChild;
            }
            pParent = pCurrent;
        } else {
            if (szCurrent && *szCurrent && pszFileName) {
                StrAllocCopy(pszFileName, szCurrent);
            }
        }
        szCurrent = szTemp;
        bDepth++;
    }
    if (pCurrent && pszFileName && !*pszFileName && pCurrent->szDirectory) {
        StrAllocCopy(pszFileName, pCurrent->szDirectory);
    }
    if (szPath) {
        delete [] szPath;
    }   
    return pCurrent;
}

// Called by a file system during file handle creation to create
// a notification object associated with an open file.
HANDLE NotifyCreateFile( HANDLE hVolume,const WCHAR *szFullPath) 
{
//  NKDbgPrintfW(L"****************************************************************************\r\n");
//  NKDbgPrintfW( L"ProcessCreateFile %s \r\n", szFullPath);
    WCHAR *szFileName = NULL;
    ENTERNOTIFY(hVolume);
    NOTDIRENTRY *pCurrent = InsertOrFindDirEntry(hVolume, szFullPath, &szFileName, FALSE, FALSE);
    NOTHANDLEENTRY *pHandle = new NOTHANDLEENTRY;
    if (!pHandle) { 
        return INVALID_HANDLE_VALUE;
    }   
    pHandle->dwSig = NOT_HANDLE_SIG;
    pHandle->pDirectory = pCurrent;
    pHandle->szFileName = szFileName;
    pHandle->pPrevious = NULL;
    pHandle->dwFlags = 0;
    if (pCurrent) {
        pHandle->pNext = pCurrent->pHandles;
        if (pCurrent->pHandles) {
            pCurrent->pHandles->pPrevious = pHandle;
        } 
        pCurrent->pHandles = pHandle;
    } else if (hVolume) {
        NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
        pHandle->pNext = pVolume->pHandles;
        if (pVolume->pHandles) {
            pVolume->pHandles->pPrevious = pHandle;
        }
        pVolume->pHandles = pHandle;
    }
#if 0
    DumpDirEntry(((NOTVOLENTRY *)hVolume)->pDirEntry);
#endif    
    EXITNOTIFY(hVolume);
    return (HANDLE)pHandle;
}

// Called by the FindFirstChangeNotification API to create a 
// notification event handle.
HANDLE NotifyCreateEvent(HANDLE hVolume, HANDLE hProc,const WCHAR *szPath, BOOL bWatchSubTree, DWORD dwFlags)
{
//  NKDbgPrintfW( L"CreateNotificationEvent %s Flags=%ld\r\n", szPath, dwFlags);
    
    HANDLE hUserEventHandle = INVALID_HANDLE_VALUE;
    NOTEVENTENTRY *pEvent = new NOTEVENTENTRY;
    if (!pEvent) {
        return INVALID_HANDLE_VALUE;
    }   

    pEvent->hEvent = CreateEvent( NULL, TRUE, FALSE, NULL);
    if (!pEvent->hEvent) {
        delete pEvent;
        return INVALID_HANDLE_VALUE;
    }

    pEvent->pVolume = NULL;
    pEvent->pDirectory = NULL;
    pEvent->dwSig = NOT_EVENT_SIG;
    pEvent->dwFlags = dwFlags; 
    pEvent->dwCount = 0;
    pEvent->hProc = hProc;
    pEvent->bWatchSubTree = bWatchSubTree;
    pEvent->pPrevious = NULL;
    pEvent->pFileInfo = NULL;
    pEvent->pFileInfoLast = NULL;


#ifdef UNDER_CE    
    // Create an API handle using our internal notification API set. This API
    // set is only used internally and the handle is not returned to the caller.
    HANDLE hNotify = CreateAPIHandle( g_hNotifyAPI, pEvent);
    if ((hNotify == NULL) || (hNotify == INVALID_HANDLE_VALUE)) {
        NotifyCloseEvent((HANDLE)pEvent);
        return INVALID_HANDLE_VALUE;
    }

    // Duplicate the notification handle into the caller's process and close our
    // copy. This is done to make sure proper cleanup is performed when the caller
    // process exits.
    if (!DuplicateHandle( (HANDLE)GetCurrentProcessId(), hNotify, hProc,
        &pEvent->hNotify, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        DEBUGCHK( 0);
        // On failure, notify handle was already closed, so pEvent was cleaned
        // up by NotifyCloseEvent.
        return INVALID_HANDLE_VALUE;
    }

    //
    // NOTE: pEvent->hNotify is owned by pEvent->hProc. 
    //

    // Store the internal notification API handle as event data belonging to
    // the notification event.

    // Instead of returning the internal notification handle, duplicate the 
    // notification event handle for the requesting process. We must return an
    // event handle so it can be passed to a wait function.
    if (!SetEventData( pEvent->hEvent, (DWORD)pEvent->hNotify) ||
        !DuplicateHandle( (HANDLE)GetCurrentProcessId(), pEvent->hEvent, hProc, 
        &hUserEventHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {

        DEBUGCHK( 0); // We should never fail to duplicate this event handle.

        // Duplicate the notification back into our process, closing the caller's
        // copy, so we can close it completely. 
        // NOTE: We could use CloseHandleInProcess here if it were a public API.
        VERIFY(DuplicateHandle( hProc, pEvent->hNotify, (HANDLE)GetCurrentProcessId(), &hNotify,
            0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE));

        // NotifyCloseEvent will cleanup everything.
        CloseHandle (hNotify);
        return INVALID_HANDLE_VALUE;
    }
#else
    pEvent->hNotify = pEvent;
    hUserEventHandle = (HANDLE)pEvent;
#endif


    ENTERNOTIFY( hVolume);
    NOTDIRENTRY *pCurrent = InsertOrFindDirEntry(hVolume, szPath, NULL, TRUE, FALSE);
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
    pEvent->pDirectory = pCurrent;

    if (pCurrent) {
        pEvent->pNext = pCurrent->pEvents;
        if (pCurrent->pEvents) {
            pCurrent->pEvents->pPrevious = pEvent;
        } 
        pCurrent->pEvents = pEvent;
    } else if (pVolume) {
        pEvent->pNext = pVolume->pEvents;
        if (pVolume->pEvents) {
            pVolume->pEvents->pPrevious = pEvent;
        } 
        pVolume->pEvents = pEvent;
    }
    pEvent->pVolume = pVolume;
    EXITNOTIFY(hVolume);

    return hUserEventHandle;




}

void CheckForCleanup(NOTVOLENTRY *pVolume, NOTDIRENTRY *pDirectory)
{
    NOTDIRENTRY *pTemp = NULL;
    while (pDirectory && !pDirectory->pHandles && !pDirectory->pEvents && !pDirectory->pChild) {
        pTemp = pDirectory->pParent;
        if (pTemp){
            if (pTemp->pChild == pDirectory) {
                pTemp->pChild = pDirectory->pNext;
            }   
        } else {
            if (pVolume->pDirEntry == pDirectory) {
                pVolume->pDirEntry = pDirectory->pNext;
            }
        }   
        if (pDirectory->pPrevious) {
            pDirectory->pPrevious->pNext = pDirectory->pNext;
        }   
        if (pDirectory->pNext) {
            pDirectory->pNext->pPrevious = pDirectory->pPrevious;
        }   
        if (pDirectory->szDirectory) {
            delete [] pDirectory->szDirectory;
        }   
        delete pDirectory;
        pDirectory = pTemp;
    }
}

// Called from a file system to indicate a file handle is being closed 
// to generate notifications accordingly.
void NotifyCloseHandle(HANDLE hVolume, HANDLE h)
{
    NOTHANDLEENTRY *pHandle = (NOTHANDLEENTRY *)h;
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
//  NKDbgPrintfW(L"****************************************************************************\r\n");
//  NKDbgPrintfW( L"CloseHandle on Handle=%08X\r\n", h);
    if (!IsValidData(h, NOT_HANDLE_SIG)) {
        return;
    }
    if (pHandle->dwFlags & HANDLE_FLAG_FILECHANGED) {
        NotifyHandleChangeEx( hVolume, 
                              h, 
                              FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS | FILE_NOTIFY_CHANGE_CREATION, 
                              FILE_ACTION_CHANGE_COMPLETED);
    }
    ENTERNOTIFY( hVolume);
    if (pHandle->pDirectory) {
        if (pHandle->pDirectory->pHandles == pHandle) {
            pHandle->pDirectory->pHandles = pHandle->pNext;
        }
    } else if (pVolume) {
        if (pVolume->pHandles == pHandle) {
            pVolume->pHandles = pHandle->pNext;
        }
    }    
    if (pHandle->pPrevious)
        pHandle->pPrevious->pNext = pHandle->pNext;
    if (pHandle->pNext)
        pHandle->pNext->pPrevious = pHandle->pPrevious;
    if (IsValidData(pHandle->pDirectory, NOT_DIR_SIG))
        CheckForCleanup(pVolume, pHandle->pDirectory);          
    EXITNOTIFY(hVolume);        
    if (pHandle->szFileName)
        delete [] pHandle->szFileName;
    delete pHandle;
}

// Internal PSL used by FindCloseChangeNotification.
BOOL NotifyCloseEvent(HANDLE h)
{
    NOTEVENTENTRY *pEvent = (NOTEVENTENTRY *)h;
    if (pEvent->pVolume) {
        ENTERNOTIFY(pEvent->pVolume);
        if (pEvent->pDirectory) {
            if (pEvent->pDirectory->pEvents == pEvent) {
                pEvent->pDirectory->pEvents = pEvent->pNext;
            }
        } else {
            if (pEvent->pVolume->pEvents == pEvent) {
                pEvent->pVolume->pEvents = pEvent->pNext;
            }
        }    
        if (pEvent->pPrevious)
            pEvent->pPrevious->pNext = pEvent->pNext;
        if (pEvent->pNext)
            pEvent->pNext->pPrevious = pEvent->pPrevious;
        if (pEvent->pDirectory)
            CheckForCleanup(pEvent->pVolume, pEvent->pDirectory);
        DeleteFileInfos( pEvent);
        EXITNOTIFY(pEvent->pVolume);        
    }        
    VERIFY( CloseHandle( pEvent->hEvent)); // Close our event handle.
    delete pEvent;
    return TRUE;
}

void AddFileToEvent(NOTEVENTENTRY *pEvent, const WCHAR *szFileName, DWORD dwFlags, DWORD dwAction)
{
    if (szFileName) {
        DWORD dwLen = wcslen(szFileName);
        if (dwLen > INT_MAX) return;
        if(pEvent) {
            if ((pEvent->dwFlags & FILE_NOTIFY_CHANGE_CEGETINFO) && (pEvent->dwFlags & dwFlags)) {
#if 0
                WCHAR szAction[MAX_PATH];
                switch(dwAction) {
                    case FILE_ACTION_ADDED:
                        wcscpy( szAction, L"The file was added to the directory.");
                        break;
                    case FILE_ACTION_REMOVED:
                        wcscpy( szAction, L"The file was removed from the directory."); 
                        break;
                    case FILE_ACTION_MODIFIED:
                        wcscpy( szAction, L"The file was modified. This can be a change in the time stamp or attributes."); 
                        break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                        wcscpy( szAction, L"The file was renamed and this is the old name."); 
                        break;
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        wcscpy( szAction, L"The file was renamed and this is the new name."); 
                        break;
                }
                NKDbgPrintfW( L"Add file %s Action is %s\r\n", szFileName, szAction);
#endif            
                NOTFILEINFO *pFileInfo = new NOTFILEINFO;
                if (pFileInfo) {
                    //
                    // Make sure we can add a count to this volume before
                    // we go any farther.
                    //
                    PNOTIFYCOUNT pCount = NULL;
                    if( pEvent->pVolume )
                    {
                        ENTERNOTIFY(pEvent->pVolume);

                        pCount = pEvent->pVolume->pNotifyCount;
                        while (pCount && pCount->hProc != pEvent->hProc) {
                            pCount = pCount->pNext;
                        }
                        
                        if (pCount) {
                            if (pCount->dwCount < MAX_NOTIFICATIONS && !pCount->fOverflow) {
                                pCount->dwCount += 1;
                            } else {
                                //
                                // We've surpassed the number of notifications we 
                                // will allow.
                                //
                                if (!pCount->fOverflow) {
                                    //
                                    // This is addition actually caused the 
                                    // overflow, so go through and remove all
                                    // notifications for this process.
                                    //
                                    PNOTEVENTENTRY pTemp = pEvent->pVolume->pEvents;
                                    while (pTemp) {
                                        if (pTemp->hProc == pEvent->hProc) {
                                            DeleteFileInfos( pTemp, FALSE );
                                        }
                                        pTemp = pTemp->pNext;
                                    }
                                }

                                pCount->fOverflow = TRUE;
                                pCount = NULL;
                            }
                        } else {
                            pCount = new NOTIFYCOUNT;
                            if (pCount) {
                                pCount->fOverflow = FALSE;
                                pCount->dwCount = 1;
                                pCount->hProc = pEvent->hProc;
                                pCount->pNext = pEvent->pVolume->pNotifyCount;
                                pEvent->pVolume->pNotifyCount = pCount;
                            }
                        }
                        EXITNOTIFY(pEvent->pVolume);
                    }

                    if ((pEvent->pVolume && pCount) || !pEvent->pVolume) {
                        pFileInfo->dwFileNameLen = dwLen*sizeof(WCHAR);
                        pFileInfo->szFileName = new WCHAR[dwLen+1];
                        if (pFileInfo->szFileName) {
                            wcscpy_s( pFileInfo->szFileName, dwLen+1, szFileName);
                        }
                        pFileInfo->dwAction = dwAction;
                        pFileInfo->pNext = NULL;
                        if (!pEvent->pFileInfo) {
                            pEvent->pFileInfo = pFileInfo;
                        } 
                        if (pEvent->pFileInfoLast) {
                            pEvent->pFileInfoLast->pNext = pFileInfo;
                        }
                        pEvent->pFileInfoLast = pFileInfo;
                        pEvent->dwCount++;
                    } else {
                        delete pFileInfo;
                    }

                }
            } else if(pEvent->dwFlags & dwFlags) {
                pEvent->dwCount++;
            }
        }
    }    
}


void CheckEventOnPath(NOTEVENTENTRY *pEvent, BOOL bSubTree, BOOL bPath, const FileEvent *pFileEvent)
{
    while(pEvent) {
        if ((bSubTree || pEvent->bWatchSubTree) &&
           ((bPath && (pEvent->dwFlags & FILE_NOTIFY_CHANGE_DIR_NAME)) ||
            (!bPath && (pEvent->dwFlags & FILE_NOTIFY_CHANGE_FILE_NAME)))) {
#if 0
            NKDbgPrintfW( L"Signaling a change on path (%08X) Count=%ld ", pEvent, pEvent->dwCount);
            NOTDIRENTRY *pTemp= pEvent->pDirectory;
            while( pTemp) {
                NKDbgPrintfW(L"%s<-", pTemp->szDirectory);
                pTemp= pTemp->pPrevious;
            }
#endif
            DWORD dwoldCount = pEvent->dwCount;
            AddFileToEvent(pEvent,pFileEvent->szFileName, pFileEvent->dwFlags, pFileEvent->dwAction) ;
            if (dwoldCount < pEvent->dwCount) {
                VERIFY( SetEvent( pEvent->hEvent));
            }
        }    
        pEvent = pEvent->pNext;
    }
}


void CheckDeleteEventOnTree(NOTDIRENTRY *pDir, BOOL bPath, DWORD dwFlags, DWORD dwAction)
{
    if (pDir)
    {
        FileEvent FE;
        FE.dwAction   = dwAction;
        FE.dwFlags    = dwFlags;
        FE.szFileName = L"\\";
        CheckEventOnPath(pDir->pEvents,TRUE,bPath,&FE);

        NOTDIRENTRY *pChild = pDir->pChild;
        while (pChild)
        {
            CheckDeleteEventOnTree(pChild,bPath,dwFlags,dwAction);
            pChild = pChild->pNext;
        }
    }
}


// Called by a file system to indicate that path-based change has
// occurred on the volume, such as file or directory creation.
void NotifyPathChangeEx(HANDLE hVolume, const WCHAR *szPath, const WCHAR *szFile, BOOL bPath, DWORD dwAction)
{
    BOOL bSubTree = TRUE, bNotifyRoot = FALSE ;
    WCHAR *szFileName = NULL, *sztemp = (WCHAR *)szPath;
    NOTDIRENTRY *pCurrent = NULL;
    FileEvent FE ;
    DWORD len1 = 0;
    DWORD len2 = 0;
    DEBUGCHK (!szFile || !bPath);

    ENTERNOTIFY(hVolume);
    
    if ((szFile) && (szPath)) {
        len1 = wcslen(szPath) ;
        len2 = wcslen(szFile) ;

        DWORD dwstrlentemp = len1 + len2 + 2 ;
        if ((dwstrlentemp <= len1) || (dwstrlentemp <= len2) || (dwstrlentemp < 2)) {
            DEBUGCHK(0);
            EXITNOTIFY(hVolume);
            return;
        }

        sztemp = new TCHAR [len1 + len2 + 2] ;
        if(sztemp) {
            wcscpy_s(sztemp, len1+len2+2, szPath) ;
            if ((len1 == 0) || (szPath[len1 - 1] != L'\\')) {
                wcscat_s(sztemp, len1+len2+2, L"\\") ;
            }
        } else {
            // This notification may not be sent correctly. 
            sztemp = (WCHAR *)szPath ;
            DEBUGCHK(0) ;
        }
        
    }
    if (bPath && ((FILE_ACTION_REMOVED == dwAction) || (FILE_ACTION_RENAMED_OLD_NAME == dwAction))) {
        // Removing or renaming a directory.

        // First, determine if there is already a directory entry watching the directory
        // being removed or renamed so we can signal that the "current directory" has been
        // removed.
        pCurrent = InsertOrFindDirEntry(hVolume, sztemp, &szFileName, bPath, TRUE);
        if (pCurrent && szFileName && bSubTree && (wcscmp(szFileName, pCurrent->szDirectory) == 0)) {
            FE.dwFlags    = FILE_NOTIFY_CHANGE_DIR_NAME ;
            FE.dwAction   = FILE_ACTION_REMOVED ;
            FE.szFileName = L"\\" ;
            CheckDeleteEventOnTree(pCurrent,bPath,FE.dwFlags,FE.dwAction) ;
        }    

        if (szFileName)
        {
           //free up szFileName as we are going to search again.
           delete [] szFileName;
           szFileName = NULL;
        }
        // Search again, this time with bPath=FALSE so it won't match "current directory."
        pCurrent = InsertOrFindDirEntry(hVolume, sztemp, &szFileName, FALSE, TRUE, &bSubTree);
        FE.dwFlags    = FILE_NOTIFY_CHANGE_DIR_NAME ;
        FE.dwAction   = dwAction ;

    } else {

        // Search for an existing watch directory entry where this change is occuring.
        // Always pass bPath=FALSE because we don't want to match the current directory.
        // We DO NOT notify when the watched path is created; we DO notify when the 
        // watched path is removed.
        if (szFile) {
            // don't need the file name because we already have it
            pCurrent = InsertOrFindDirEntry(hVolume, sztemp, NULL, FALSE, TRUE, &bSubTree);
        } else {            
            pCurrent = InsertOrFindDirEntry(hVolume, sztemp, &szFileName, FALSE, TRUE, &bSubTree);
        }
        
        FE.dwFlags    = bPath ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME ;
        FE.dwAction   = dwAction ;

    }

    if (szFile) {
        if (sztemp != szPath) {
            wcscat_s(sztemp,len1+len2+2,szFile) ;
        }
    }
    
    while(pCurrent && sztemp) {
        // Signal all events up the directory chain.
        FE.szFileName = GetTrailingName(pCurrent,sztemp) ;
        CheckEventOnPath( pCurrent->pEvents, bSubTree, bPath, &FE);
        bSubTree = FALSE;
        pCurrent = pCurrent->pParent;
    }

    ASSERT(hVolume) ;
    
    if (hVolume && sztemp) {
        // Finally, signal events on the root.
        FE.szFileName = (*sztemp == L'\\') ? sztemp + 1 : sztemp;
        CheckEventOnPath( ((NOTVOLENTRY *)hVolume)->pEvents, bSubTree, bPath, &FE);
    }

    bNotifyRoot = FALSE ;
    if (g_pRootVolume && (hVolume != g_pRootVolume) && sztemp)
    {
        // Build the pathname when you have the hVolume lock
        // Notify the root volume after releasing the lock
        
        DWORD pathsize = wcslen(sztemp) + wcslen(((NOTVOLENTRY *)hVolume)->szVolumeName) + 1 ;
        FE.szFileName =  new WCHAR[pathsize] ;
        if (FE.szFileName)
        {
            const WCHAR *szbuf = (*sztemp == L'\\') ? sztemp + 1 : sztemp ;
            StringCchPrintfW((STRSAFE_LPWSTR)FE.szFileName,pathsize,L"%s\\%s",((NOTVOLENTRY *)hVolume)->szVolumeName,szbuf) ;
            bNotifyRoot = TRUE ;
        }
    }

    EXITNOTIFY(hVolume);

    if (bNotifyRoot)
    {
        EnterCriticalSection( &g_csMain);

        if(g_pRootVolume && (hVolume != g_pRootVolume))
        {
            ENTERNOTIFY(g_pRootVolume) ;
            CheckEventOnPath(g_pRootVolume->pEvents, FALSE, bPath, &FE);        
            EXITNOTIFY(g_pRootVolume) ;
        }
        
        LeaveCriticalSection( &g_csMain);
        delete [] FE.szFileName ;            
    }

    if (szFileName) {
        delete [] szFileName;
        szFileName = NULL;
    }

    if (sztemp != szPath) {
        delete [] sztemp ;
        sztemp = NULL;
    }
        

}

// Called by a file system to indicate that a file or directory
// on the volume has changed.
void NotifyPathChange(HANDLE hVolume, const WCHAR *szPath, BOOL bPath, DWORD dwAction)
{
    NotifyPathChangeEx(hVolume, szPath, NULL, bPath, dwAction);
}

int GetFileNameOffset(PCWSTR pszFileName)
{
    int cch;
    cch = wcslen(pszFileName);
    while( cch && (pszFileName[cch] != '\\')) {
        cch--;
    }
    return(cch);
}

// Called by a file system to indicate that a file or directory
// on the volume has moved or been renamed.
void    NotifyMoveFileEx (HANDLE hVolume, LPCWSTR lpszExisting, LPCWSTR lpszNew, BOOL bPath)
{
    DWORD dwLen1 = GetFileNameOffset( lpszExisting);
    DWORD dwLen2 = GetFileNameOffset( lpszNew);
    BOOL bSameDir = TRUE;
    if ((dwLen1 != dwLen2) || (wcsncmp( lpszExisting, lpszNew, dwLen1) != 0)) {
        bSameDir = FALSE;
    }    
    ENTERNOTIFY(hVolume);
    if (bSameDir) {
        // When moving a file within the same directory, send rename-notifications
        // since any client watching the affected directory will always receive both.
        NotifyPathChange( hVolume, lpszExisting, bPath, FILE_ACTION_RENAMED_OLD_NAME);
        NotifyPathChange( hVolume, lpszNew, bPath, FILE_ACTION_RENAMED_NEW_NAME);
    } else {
        // When moving a file between directories, send removed/added/completed
        // notifications. This is the same set of notifications that would be
        // generated when moving between volumes, which is effectively a copy
        // followed by a delete.
        NotifyPathChange( hVolume, lpszExisting, bPath, FILE_ACTION_REMOVED);
        NotifyPathChange( hVolume, lpszNew, bPath, FILE_ACTION_ADDED);
        NotifyPathChange( hVolume, lpszNew, bPath, FILE_ACTION_CHANGE_COMPLETED);
    }
    EXITNOTIFY(hVolume);
}

void NotifyMoveFile (HANDLE hVolume, LPCWSTR lpszExisting, LPCWSTR lpszNew)
{
    NotifyMoveFileEx (hVolume, lpszExisting, lpszNew, FALSE);
}

// Internal PSL used by FindNextChangeNotification. Can ONLY be invoked by kernel,
// so there is no need to verify access to embedded pointers in NOTGETNEXTDATA.
BOOL NotifyReset(HANDLE h, NOTGETNEXTDATA *pData, DWORD DataSize)
{
    if (pData && (sizeof(NOTGETNEXTDATA) != DataSize)) {
        DEBUGCHK(0);
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    BOOL fRet = TRUE;
    NOTEVENTENTRY *pEvent = (NOTEVENTENTRY *)h;
    NOTVOLENTRY *pVolume = pEvent->pVolume;

    SetLastError (ERROR_SUCCESS);

    ENTERNOTIFY(pVolume);
    ResetEvent( pEvent->hEvent);

    PNOTIFYCOUNT pCount = NULL;
    PNOTIFYCOUNT pPrevCount = NULL;

    if (pEvent->pVolume) {
        pCount = pEvent->pVolume->pNotifyCount;
        while (pCount && pCount->hProc != pEvent->hProc) {
            pPrevCount = pCount;
            pCount = pCount->pNext;
        }
    }

//    ASSERT(pData || pEvent->dwCount); // Turn on this ASSERT to catch bugs in the caller. ASSERT(pEvent->dwCount) is also valid
//                                      // Ideally NotifyReset shouldn't be called if pEvent->dwCount is zero.
    ASSERT(!pEvent->pFileInfo ||  pEvent->dwCount);
    ASSERT(!pData || (!!pEvent->pFileInfo ==  !!pEvent->dwCount));

    if (pCount && pCount->fOverflow) {
        //
        // Once we've reported the error we will start receiving new 
        // notifications.
        //
        fRet = FALSE;
        SetLastError(ERROR_ALLOTTED_SPACE_EXCEEDED);

        if (pPrevCount) {
            pPrevCount->pNext = pCount->pNext;
        } else {
            pEvent->pVolume->pNotifyCount = pCount->pNext;
        }
        delete pCount;
        pCount = NULL;
    } else if (pData && pEvent->dwCount && pEvent->pFileInfo) {
        __try {
            DWORD dwCount = 0;
            DWORD dwBytesAvail = 0;
            DWORD dwBytesReturned = 0;
            DWORD dwPrevLen = 0;
            NOTFILEINFO *pFileInfo=NULL;
            FILE_NOTIFY_INFORMATION *pNotInfo = NULL;

            while( pEvent->pFileInfo) {

                pFileInfo = pEvent->pFileInfo;

                if ((pFileInfo->dwFileNameLen + sizeof(WCHAR) < sizeof(WCHAR)) ||
                    (pFileInfo->dwFileNameLen + sizeof(WCHAR) + sizeof(FILE_NOTIFY_INFORMATION) < sizeof(FILE_NOTIFY_INFORMATION))) {
                    // Integer overflow should never happen or we have an unexpectedly large file name.
                    DEBUGCHK(0);
                    SetLastError (ERROR_ARITHMETIC_OVERFLOW);
                    fRet = FALSE;
                    break;
                }

                DWORD dwLen = pFileInfo->dwFileNameLen + sizeof(WCHAR) + sizeof(FILE_NOTIFY_INFORMATION);

                if (dwLen & (sizeof(DWORD)-1)) {
                    dwLen += (sizeof(DWORD)-(dwLen & sizeof(DWORD)-1));
                }
                if( dwLen == 0 )
                {
                    DEBUGCHK(0);
                    SetLastError (ERROR_ARITHMETIC_OVERFLOW);
                    fRet = FALSE;
                    break;
                }
                if ((pData->nBufferLength >= dwLen + dwBytesReturned) &&
                    (dwLen + dwBytesReturned >= dwLen)) { // check for integer overflow
                    if (pNotInfo) {
                        pNotInfo->NextEntryOffset = dwPrevLen;
                    }    
                    pNotInfo = (FILE_NOTIFY_INFORMATION *)((BYTE *)pData->lpBuffer+dwBytesReturned);
                    pNotInfo->NextEntryOffset = 0; 
                    pNotInfo->Action = pFileInfo->dwAction;
                    pNotInfo->FileNameLength = pFileInfo->dwFileNameLen;
                    memcpy( (BYTE *)pNotInfo->FileName, (BYTE *)pFileInfo->szFileName, pFileInfo->dwFileNameLen+sizeof(WCHAR));
                    dwBytesReturned += dwLen;
                    dwPrevLen = dwLen;
                    dwCount++;
                    deleteNOTFILEINFO(pEvent,&pCount,pPrevCount);
                } else {
                    if (dwCount) {
                        SetLastError(ERROR_MORE_DATA);
                    } else {
                        SetLastError(ERROR_INSUFFICIENT_BUFFER);
                    }    
                    break;
                }
        
            }

            if (pData->lpBytesAvailable) {
                pFileInfo = pEvent->pFileInfo;
                while(pFileInfo) {
                    dwBytesAvail += pFileInfo->dwFileNameLen + sizeof(WCHAR) + sizeof(FILE_NOTIFY_INFORMATION);
                    if (dwBytesAvail & (sizeof(DWORD)-1)) {
                        dwBytesAvail += (sizeof(DWORD)-(dwBytesAvail & sizeof(DWORD)-1));
                    }    
                    pFileInfo = pFileInfo->pNext;
                }
                *(pData->lpBytesAvailable) = dwBytesAvail;
            }    
            if (pData->lpBytesReturned) {
                *(pData->lpBytesReturned) = dwBytesReturned;
            }

        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);   
            fRet = FALSE;
        }

    } else if (pEvent->dwCount) {

        // Data not requested but there are pending notifications.
        // (Or possibly data requested, and we have a count but no DATA!!! Internal Error)

        if (pEvent->pFileInfo) {
            deleteNOTFILEINFO(pEvent,&pCount,pPrevCount);
        } else {
            pEvent->dwCount --;
        }

    } else if (pData) {

        // Data requested, but there are no pending notifications.
        // In this case, return failure because we are being called
        // from the CeGetNotificationInfo API.
        __try {
            if (pData->lpBytesReturned) {
                *(pData->lpBytesReturned) = 0;
            }
            if (pData->lpBytesAvailable) {
                *(pData->lpBytesAvailable) = 0;
            }
        }__except (EXCEPTION_EXECUTE_HANDLER) {
        }
        SetLastError (ERROR_NO_MORE_ITEMS);
        fRet = FALSE;
    }

    if (fRet) {

        // If there are still outstanding events, re-trigger the event
        // so the caller will know to retreive them.
        if (pEvent->dwCount) {
            SetEvent(pEvent->hEvent);
        }
    }    
    EXITNOTIFY(pVolume);
    return fRet;
}

void CheckEventOnHandle(NOTEVENTENTRY *pEvent, DWORD dwFlags, BOOL bSubTree, const FileEvent *pFileEvent)
{
    while(pEvent) {
        if ((bSubTree || pEvent->bWatchSubTree) &&(pEvent->dwFlags & dwFlags)) {
#if 0
            NKDbgPrintfW( L"Signaling a change on handle (%08X) Count=%ld\r\n", pEvent, pEvent->dwCount);
            NOTDIRENTRY *pTemp= pEvent->pDirectory;
            while( pTemp) {
                NKDbgPrintfW(L"%s<-", pTemp->szDirectory);
                pTemp= pTemp->pPrevious;
            }
#endif
            DWORD dwoldCount = pEvent->dwCount;
            AddFileToEvent(pEvent,pFileEvent->szFileName, pFileEvent->dwFlags, pFileEvent->dwAction) ;
            if (dwoldCount < pEvent->dwCount) {
                SetEvent( pEvent->hEvent);
            }
        }    
        pEvent = pEvent->pNext;
    }
}


// This function takes a filename and a NOTDIRENTRY and builds the complete
// path by traversing the pDir parameter towards its parent root
// Free the memory returned by this function

WCHAR * BuildPath(NOTDIRENTRY *pDir, const WCHAR *szFilename)
{
    DWORD size = 0 ;
    NOTDIRENTRY *pCurrent ;
    WCHAR *szPath , *sztemp ;

    ASSERT(szFilename) ;
    size = wcslen(szFilename) + 1 ;
    ASSERT(size > 1) ;
    pCurrent = pDir ;    
    while(pCurrent)
    {
        size += wcslen(pCurrent->szDirectory) + 1 ;
        pCurrent = pCurrent->pParent ;
    }

    szPath = new WCHAR[size+1] ;
    if (!szPath) return NULL ;
    
    szPath[size] = 0 ;

    sztemp = szPath + size ;
    {
        DWORD len = wcslen(szFilename) ;
        sztemp -= len ;        
        ASSERT(sztemp > szPath) ;
        memcpy(sztemp,szFilename,len*sizeof(WCHAR)) ;
        --sztemp ;
        *sztemp = L'\\' ;
    }
    
    pCurrent = pDir ;
    while(pCurrent)
    {
        DWORD len = wcslen(pCurrent->szDirectory) ;
        sztemp -= len ;        
        ASSERT(sztemp > szPath) ;
        memcpy(sztemp,pCurrent->szDirectory,len*sizeof(WCHAR)) ;
        pCurrent = pCurrent->pParent ;
        --sztemp ;
        *sztemp = L'\\' ;
    }

    ASSERT(sztemp == szPath) ;
    return szPath ;
    
}

// Called by a file system to indicate that an open file
// has been modified.
void NotifyHandleChangeEx(HANDLE hVolume, HANDLE h, DWORD dwFlags, DWORD dwAction)
{
    NOTHANDLEENTRY *pHandle = (NOTHANDLEENTRY *)h;
    NOTDIRENTRY *pCurrent = pHandle->pDirectory;
    BOOL bSubTree = TRUE, bNotifyRoot = FALSE;
    FileEvent FE ;
    WCHAR *szPath = NULL ;
    
//  NKDbgPrintfW( L"NotifyHandleChange on handle=%08X Flags=%08X\r\n", h, dwFlags);
    if ((!IsValidData(h, NOT_HANDLE_SIG))
        || (!hVolume)) {
        return;
    }
    ENTERNOTIFY(hVolume);
    pHandle->dwFlags |= HANDLE_FLAG_FILECHANGED;

    FE.dwFlags    = dwFlags ;
    FE.dwAction   = dwAction ;

    szPath = BuildPath(pCurrent,pHandle->szFileName) ;
    if (!szPath) goto Exit ;
    
    while(pCurrent) {
        FE.szFileName = GetTrailingName(pCurrent,szPath) ;
        CheckEventOnHandle(pCurrent->pEvents, dwFlags, bSubTree,&FE);
        bSubTree = FALSE;
        pCurrent = pCurrent->pParent;
    }

    ASSERT(*szPath == L'\\') ;
    FE.szFileName = szPath + 1 ;
    CheckEventOnHandle(((NOTVOLENTRY *)hVolume)->pEvents, dwFlags, bSubTree,&FE);

    bNotifyRoot = FALSE ;
    if (g_pRootVolume && (hVolume != g_pRootVolume))
    {
        // Build the pathname when you have the hVolume lock
        // Notify the root volume after releasing the lock
        
        DWORD pathsize = wcslen(szPath) + wcslen(((NOTVOLENTRY *)hVolume)->szVolumeName) + 1 ;
        FE.szFileName =  new WCHAR[pathsize] ;
        if (FE.szFileName)
        {
            StringCchPrintfW((STRSAFE_LPWSTR)FE.szFileName,pathsize,L"%s%s",((NOTVOLENTRY *)hVolume)->szVolumeName,szPath) ;
            bNotifyRoot = TRUE ;
        }
    }

Exit:

    EXITNOTIFY(hVolume);

    if (bNotifyRoot)
    {
        EnterCriticalSection( &g_csMain);

        if (g_pRootVolume && (hVolume != g_pRootVolume))
        {
            ENTERNOTIFY(g_pRootVolume) ;
            // Finally, signal events on the root volume.
            CheckEventOnHandle(g_pRootVolume->pEvents, dwFlags, FALSE, &FE);        
            EXITNOTIFY(g_pRootVolume) ;
        }

        LeaveCriticalSection( &g_csMain);
        delete [] FE.szFileName ;            
    }

    delete [] szPath ;

}

// Called by a file system to indicate that an open file
// has been modified.
void NotifyHandleChange(HANDLE hVolume, HANDLE h, DWORD dwFlags)
{
    NotifyHandleChangeEx( hVolume, h, dwFlags, FILE_ACTION_MODIFIED);
}

// Called by the FindNextChangeNotification and CeGetFileNotificationInfo
// APIs to get file change info and reset the notification event.
BOOL INT_NotifyGetNextChange(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable)
{
    BOOL bRet = FALSE;
#ifdef UNDER_CE
    // Retrieve the internal notification handle stored as event data in the
    // notification event handle.
    HANDLE hNotify = (HANDLE)GetEventData( h);
    if (!hNotify) {
        // If the handle has no data, it must not be a notification handle.
        SetLastError (ERROR_INVALID_HANDLE);
        return FALSE;
    }
#else
    HANDLE hNotify = h;
#endif
    __try { 
        if ((dwFlags == 0) && (lpBuffer == NULL) && (nBufferLength == 0) && (lpBytesReturned == NULL) && (lpBytesAvailable == NULL)) {

            bRet = NotifyReset_Trap( hNotify, NULL, 0);

        } else {
            NOTGETNEXTDATA Data;
            Data.lpBuffer = lpBuffer;
            Data.lpBytesReturned = lpBytesReturned;
            Data.lpBytesAvailable = lpBytesAvailable;

            Data.dwFlags = dwFlags;
            Data.nBufferLength = nBufferLength;

            bRet = NotifyReset_Trap( hNotify, &Data, sizeof(NOTGETNEXTDATA));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        bRet = FALSE;
    }

    return bRet;
}

// Called by the FindNextChangeNotification and CeGetFileNotificationInfo
// APIs to get file change info and reset the notification event.
BOOL NotifyGetNextChange(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable)
{
    BOOL bRet = FALSE;
#ifdef UNDER_CE
    // Make a copy of the caller's handle so we can access it.
    if (!DuplicateHandle((HANDLE)GetCallerVMProcessId(), h, GetCurrentProcess(), &h, 0, 
                FALSE, DUPLICATE_SAME_ACCESS)) {
        // The caller either passed a bad handle or does not have access to 
        // it. Preserve the error code set by DuplicateHandle.
        return FALSE;
    }
    
    // Retrieve the internal notification handle stored as event data in the
    // notification event handle.
    HANDLE hUserNotify = (HANDLE)GetEventData( h);
    if (!hUserNotify) {
        // If the handle has no data, it must not be a notification handle.
        VERIFY( CloseHandle( h)); // Close the duplicated event handle.
        SetLastError (ERROR_INVALID_HANDLE);
        return FALSE;
    }

    // Duplicate the notification handle, owned by the caller, into our process.
    HANDLE hNotify;
    if (!DuplicateHandle((HANDLE)GetCallerVMProcessId(), hUserNotify, GetCurrentProcess(), &hNotify, 0, 
        FALSE, DUPLICATE_SAME_ACCESS)) {
        // The caller either passed a bad handle or does not have access to 
        // it. Preserve the error code set by DuplicateHandle.
        VERIFY( CloseHandle( h)); // Close the duplicated event handle.
        return FALSE;
    }
    
#else
    HANDLE hNotify = h;
#endif
    __try { 
        if ((dwFlags == 0) && (lpBuffer == NULL) && (nBufferLength == 0) && (lpBytesReturned == NULL) && (lpBytesAvailable == NULL)) {

            bRet = NotifyReset_Trap( hNotify, NULL, 0);

        } else {
            NOTGETNEXTDATA Data;
            Data.lpBuffer = lpBuffer;
            Data.lpBytesReturned = lpBytesReturned;
            Data.lpBytesAvailable = lpBytesAvailable;

            Data.dwFlags = dwFlags;
            Data.nBufferLength = nBufferLength;

            bRet = NotifyReset_Trap( hNotify, &Data, sizeof(NOTGETNEXTDATA));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        bRet = FALSE;
    }

#ifdef UNDER_CE
    VERIFY( CloseHandle( h)); // Close the duplicated event handle.
    VERIFY( CloseHandle( hNotify)); // Close the duplicated notification handle).
#endif
    
    return bRet;
}

// Called by the FindCloseChangeNotification API to close the
// notification event.
BOOL INT_NotifyCloseChangeHandle(HANDLE h)
{
#ifdef UNDER_CE
    HANDLE hNotify = (HANDLE)GetEventData(h);
    if (!hNotify) {
        // If the handle has no data, it must not be a notification handle.
        // If we get here, the caller's handle has already been closed
        // and there isn't anything we can do about it, but since it was
        // not a notification handle we fail the call.
        SetLastError (ERROR_INVALID_HANDLE);
        return FALSE;
    }

    // Traps directly into NotifyCloseEvent PSL
    return CloseHandle( hNotify);
#else
    return NotifyCloseEvent( h);
#endif
}

// Called by the FindCloseChangeNotification API to close the
// notification event.
BOOL NotifyCloseChangeHandle(HANDLE h)
{
#ifdef UNDER_CE    
    // Make a copy of the caller's notification event handle so we can access
    // it, and close the caller's handle.
    if (!DuplicateHandle((HANDLE)GetCallerVMProcessId(), h, GetCurrentProcess(), &h, 0,
                FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        // The caller either passed a bad handle or does not have access to 
        // it. Preserve the error code set by DuplicateHandle.
        return FALSE;
    }
    
    HANDLE hUserNotify = (HANDLE)GetEventData(h);
    if (!hUserNotify) {
        // If the handle has no data, it must not be a notification handle.
        VERIFY( CloseHandle( h)); // Close the duplicated event handle.
        // If we get here, the caller's handle has already been closed
        // and there isn't anything we can do about it, but since it was
        // not a notification handle we fail the call.
        SetLastError (ERROR_INVALID_HANDLE);
        return FALSE;
    }

    VERIFY( CloseHandle ( h)); // Close the duplicated event handle.

    // Duplicate the notification handle, owned by the caller, into our process,
    // and close the caller's copy.
    HANDLE hNotify;
    if (!DuplicateHandle((HANDLE)GetCallerVMProcessId(), hUserNotify, GetCurrentProcess(), &hNotify, 0, 
        FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        // The data associated with the event must not have been a valid handle
        // or was not owned by the caller process.
        return FALSE;
    }

    // Traps directly into NotifyCloseEvent PSL
    return CloseHandle( hNotify);
#else
    return NotifyCloseEvent( h);
#endif    
}

