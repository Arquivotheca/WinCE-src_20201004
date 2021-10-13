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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include <fsnotify_api.h>
#include "fsnotify.h"

HANDLE g_hNotifyAPI = NULL;
CRITICAL_SECTION g_csMain;
NOTVOLENTRY *g_pRootVolume = NULL;

#ifdef UNDER_CE

#define ARRAY_SIZE(a)           (sizeof(a)/sizeof(a[0]))

// An internal handle-based API set is used for the notification engine. 
// Handles created based on this API set are not returned to the caller, but
// are owned by the caller so that they will be cleaned up on process exit.
// Handles returnd to the caller are event handles, and notification handles
// stored as event data.

#include <psyscall.h>
#define WIN32_FIND_CALL(type, api, args)    (*(type (*) args)PRIV_IMPLICIT_CALL(HT_FIND, api))
#define NotifyReset_Trap                    WIN32_FIND_CALL(BOOL, 2, (HANDLE hf, PNOTGETNEXTDATA pfd, DWORD cb))

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

void AddFileToEvent(NOTEVENTENTRY *pEvent, const WCHAR *szFileName, DWORD dwFlags, DWORD dwAction);
void CheckEventOnPath(NOTEVENTENTRY *pEvent, BOOL bSubTree, BOOL bPath);
void NotifyHandleChangeEx(HANDLE hVolume, HANDLE h, DWORD dwFlags, DWORD dwAction);

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
        if (hNotify || (pDirectory->pChild && (hNotify = FindNotifyHandleOnDirectory(pDirectory->pChild, hEvent)))) {
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
    NOTVOLENTRY *pVolume = g_pRootVolume;
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
        if (hNotify || (pVolume->pDirEntry && (hNotify = FindNotifyHandleOnDirectory( pVolume->pDirEntry, hEvent)))) {
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
    if (pVolume->szVolumeName) {
        memcpy( pVolume->szVolumeName, szNotifyPoint, dwSize);
    }   
#ifdef UNDER_CE    
    if (!g_hNotifyAPI) {
        InitializeCriticalSection( &g_csMain);
        g_hNotifyAPI = CreateAPISet("FNOT", ARRAY_SIZE(asigFindNotify), (const PFNVOID * )apfnFindNotifyExt, asigFindNotify);
        RegisterDirectMethods (g_hNotifyAPI, (const PFNVOID * )apfnFindNotifyInt);
        RegisterAPISet(g_hNotifyAPI, HT_FIND | REGISTER_APISET_TYPE);
    }
    EnterCriticalSection( &g_csMain);
    if (g_pRootVolume) {
        pVolume->pNextVolume = g_pRootVolume;
    }
    g_pRootVolume = pVolume;
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

void DeleteFileInfos(NOTEVENTENTRY *pEvent)
{
    while (pEvent->pFileInfo) {
        NOTFILEINFO *pFileInfo;
        pFileInfo = pEvent->pFileInfo;
        pEvent->pFileInfo = pFileInfo->pNext;
        delete [] pFileInfo->szFileName;
        delete pFileInfo;
    }
}

void DeleteEvents(NOTEVENTENTRY *pEvents)
{
    while(pEvents) {
        NOTEVENTENTRY *pEvent;
        pEvent = pEvents;
        DeleteFileInfos(pEvent);
        pEvents = pEvent->pNext;
        VERIFY( CloseHandle( pEvent->hEvent));
        pEvent->hEvent = NULL;
        delete pEvent;
    }
}

void ClearEvents(NOTEVENTENTRY *pEvents)
{
    NOTEVENTENTRY *pEvent = pEvents;
    while(pEvent) {
        pEvent->pVolume = NULL;
        pEvent = pEvent->pNext;
    }
    AddFileToEvent( pEvents, 
                    L"\\", 
                    FILE_NOTIFY_CHANGE_DIR_NAME, 
                    FILE_ACTION_REMOVED);
    CheckEventOnPath(pEvents, TRUE, TRUE);
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
        if (g_pRootVolume == pVolume) {
            g_pRootVolume = pVolume->pNextVolume;
        } else {
            NOTVOLENTRY * pCurVolume = g_pRootVolume;
            while(pCurVolume) {
                if (pCurVolume->pNextVolume == pVolume) {
                    pCurVolume->pNextVolume = pVolume->pNextVolume;
                    break;
                }
                pCurVolume = pCurVolume->pNextVolume;
            }
        }    
        LeaveCriticalSection( &g_csMain);
        DeleteCriticalSection(&pVolume->cs);
        if (pVolume->szVolumeName) {
            delete [] pVolume->szVolumeName;
        }   
        delete pVolume;
    }
}


NOTDIRENTRY *InsertOrFindDirEntry(HANDLE hVolume,const WCHAR *szFullPath, WCHAR **pszFileName, BOOL bPath, BOOL bFind, BOOL *pbSubTree = NULL) 
{
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
    NOTDIRENTRY *pDirectory = pVolume->pDirEntry;
    WCHAR *szPath = NULL;
    while(*szFullPath && (*szFullPath == L'\\'))
        szFullPath++;
    szPath = new WCHAR[wcslen(szFullPath)+1];    
    if (!szPath) {
        return NULL;
    }   
    wcscpy( szPath, szFullPath);                
    WCHAR *szCurrent = szPath;
    NOTDIRENTRY *pParent = NULL;
    NOTDIRENTRY *pCurrent = NULL;
    if (pszFileName)
        *pszFileName = NULL;
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
            while(pDirectory && pDirectory->szDirectory && ( (nCmp = wcsicmp(szCurrent, pDirectory->szDirectory)) > 0)) {
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
                            *pszFileName = new WCHAR[wcslen(szTemp)+1];
                            if (*pszFileName) {
                                wcscpy( *pszFileName, szTemp);
                            }    
                        }
                    } else {
                        if (szCurrent && *szCurrent && pszFileName && pbSubTree) {
                            *pszFileName = new WCHAR[wcslen(szCurrent)+1];
                            if (*pszFileName) {
                                wcscpy( *pszFileName, szCurrent);
                            }    
                        }
                    }    
                    if (szPath) {
                        delete [] szPath;
                    }   
                    return pCurrent;
                }
                if (!(pCurrent = CreateDirEntry(szCurrent, (HANDLE)pVolume))) 
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
                *pszFileName = new WCHAR[wcslen(szCurrent)+1];
                if (*pszFileName) {
                    wcscpy( *pszFileName, szCurrent);
                }    
            }
        }
        szCurrent = szTemp;
        bDepth++;
    }
    if (pCurrent && pszFileName && !*pszFileName && pCurrent->szDirectory) {
        *pszFileName = new WCHAR[wcslen(pCurrent->szDirectory)+1];
        if (*pszFileName) {
            wcscpy( *pszFileName, pCurrent->szDirectory);
        }    
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
    WCHAR *szFileName;
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
    ENTERNOTIFY( hVolume);
    NOTDIRENTRY *pCurrent = InsertOrFindDirEntry(hVolume, szPath, NULL, TRUE, FALSE);
    NOTEVENTENTRY *pEvent = new NOTEVENTENTRY;
    if (!pEvent) {
        EXITNOTIFY(hVolume);
        return INVALID_HANDLE_VALUE;
    }   
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
    pEvent->dwSig = NOT_EVENT_SIG;
    pEvent->dwFlags = dwFlags; 
    pEvent->dwCount = 0;
    pEvent->hProc = hProc;
    pEvent->bWatchSubTree = bWatchSubTree;
    pEvent->hEvent = CreateEvent( NULL, TRUE, FALSE, NULL);
    if (!pEvent->hEvent) {
        delete pEvent;
        EXITNOTIFY(hVolume);
        return INVALID_HANDLE_VALUE;
    }
    pEvent->pDirectory = pCurrent;
    pEvent->pPrevious = NULL;
    pEvent->pFileInfo = NULL;
    pEvent->pFileInfoLast = NULL;
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
    EXITNOTIFY(hVolume);
    pEvent->pVolume = pVolume;
#ifdef UNDER_CE    
    // Create an API handle using our internal notification API set. This API
    // set is only used internally and the handle is not returned to the caller.
    HANDLE hNotify = CreateAPIHandle( g_hNotifyAPI, pEvent);

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
    HANDLE hUserEventHandle;
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
    return hUserEventHandle;
#else
    pEvent->hNotify = pEvent;
    return (HANDLE)pEvent;
#endif
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
        EXITNOTIFY(pEvent->pVolume);        
    }        
    DeleteFileInfos( pEvent);
    VERIFY( CloseHandle( pEvent->hEvent)); // Close our event handle.
    delete pEvent;
    return TRUE;
}

void AddFileToEvent(NOTEVENTENTRY *pEvent, const WCHAR *szFileName, DWORD dwFlags, DWORD dwAction)
{
    if (szFileName) {
        DWORD dwLen = wcslen(szFileName);
        while(pEvent) {
            BOOL bNotify = FALSE;
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
                    pFileInfo->dwFileNameLen = dwLen*sizeof(WCHAR);
                    pFileInfo->szFileName = new WCHAR[dwLen+1];
                    if (pFileInfo->szFileName) {
                        wcscpy( pFileInfo->szFileName, szFileName);
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
                }
            }    
            pEvent = pEvent->pNext;
        }
    }    
}


void CheckEventOnPath(NOTEVENTENTRY *pEvent, BOOL bSubTree, BOOL bPath)
{
    while(pEvent) {
        BOOL bNotify = FALSE;
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
            if (!pEvent->dwCount) {
                VERIFY( SetEvent( pEvent->hEvent));
            }
            pEvent->dwCount++;
        }    
        pEvent = pEvent->pNext;
    }
}

// Called by a file system to indicate that path-based change has
// occurred on the volume, such as file or directory creation.
void NotifyPathChangeEx(HANDLE hVolume, const WCHAR *szPath, const WCHAR *szFile, BOOL bPath, DWORD dwAction)
{
    BOOL bSubTree = TRUE;
    WCHAR *szFileName = NULL;
    NOTDIRENTRY *pCurrent = NULL;

    DEBUGCHK (!szFile || !bPath);

    ENTERNOTIFY(hVolume);
    if (bPath && ((FILE_ACTION_REMOVED == dwAction) || (FILE_ACTION_RENAMED_OLD_NAME == dwAction))) {
        // Removing or renaming a directory.

        // First, determine if there is already a directory entry watching the directory
        // being removed or renamed so we can signal that the "current directory" has been
        // removed.
        pCurrent = InsertOrFindDirEntry(hVolume, szPath, &szFileName, bPath, TRUE);
        if (pCurrent && szFileName && bSubTree && (wcscmp(szFileName, pCurrent->szDirectory) == 0)) {
            AddFileToEvent( pCurrent->pEvents, 
                            L"\\", 
                            FILE_NOTIFY_CHANGE_DIR_NAME, 
                            FILE_ACTION_REMOVED);
            CheckEventOnPath( pCurrent->pEvents, bSubTree, bPath);
        }    
        if (szFileName) {
            delete [] szFileName;
            szFileName = NULL;
        }

        // Search again, this time with bPath=FALSE so it won't match "current directory."
        pCurrent = InsertOrFindDirEntry(hVolume, szPath, &szFileName, FALSE, TRUE, &bSubTree);
        if (bSubTree) {
            AddFileToEvent( pCurrent ? pCurrent->pEvents : ((NOTVOLENTRY *)hVolume)->pEvents, 
                            szFileName, 
                            FILE_NOTIFY_CHANGE_DIR_NAME, 
                            dwAction);
        }    
        if (szFileName) {
            delete [] szFileName;
            szFileName = NULL;
        }    
    } else {

        // Search for an existing watch directory entry where this change is occuring.
        // Always pass bPath=FALSE because we don't want to match the current directory.
        // We DO NOT notify when the watched path is created; we DO notify when the 
        // watched path is removed.
        if (szFile) {
            // don't need the file name because we already have it
            pCurrent = InsertOrFindDirEntry(hVolume, szPath, NULL, FALSE, TRUE, &bSubTree);
        } else {            
            pCurrent = InsertOrFindDirEntry(hVolume, szPath, &szFileName, FALSE, TRUE, &bSubTree);
            szFile = szFileName;
        }
        
        if (bSubTree) {
            // Add notification info.
            AddFileToEvent( pCurrent ? pCurrent->pEvents : ((NOTVOLENTRY *)hVolume)->pEvents, 
                            szFile, 
                            bPath ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME, 
                            dwAction);
        }    
        if (szFileName) {
            delete [] szFileName;
            szFileName = NULL;
        }    
    }    
    while(pCurrent) {
        // Signal all events up the directory chain.
        CheckEventOnPath( pCurrent->pEvents, bSubTree, bPath);
        bSubTree = FALSE;
        pCurrent = pCurrent->pParent;
    }
    if (hVolume) {
        // Finally, signal events on the root.
        CheckEventOnPath( ((NOTVOLENTRY *)hVolume)->pEvents, bSubTree, bPath);
    }
    EXITNOTIFY(hVolume);
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
    DWORD dwAttr = 0;
    DWORD dwLen1 = GetFileNameOffset( lpszExisting);
    DWORD dwLen2 = GetFileNameOffset( lpszNew);
    BOOL bSameDir = TRUE;
    if ((dwLen1 != dwLen2) || (wcsncmp( lpszExisting, lpszNew, dwLen1) != 0)) {
        bSameDir = FALSE;
    }    
    ENTERNOTIFY(hVolume);
    NotifyPathChange( hVolume, lpszExisting, bPath, bSameDir ? FILE_ACTION_RENAMED_OLD_NAME : FILE_ACTION_REMOVED);
    NotifyPathChange( hVolume, lpszNew, bPath, bSameDir ? FILE_ACTION_RENAMED_NEW_NAME : FILE_ACTION_ADDED);
    EXITNOTIFY(hVolume);
}

void NotifyMoveFile (HANDLE hVolume, LPCWSTR lpszExisting, LPCWSTR lpszNew)
{
    NotifyMoveFileEx (hVolume, lpszExisting, lpszNew, FALSE);
}

// Internal PSL used by FindNextChangeNotification. Can ONLY be invoked by kernel,
// so there is no need to verify access to embedded pointers in NOTGETNEXTDATA.
BOOL NotifyReset(HANDLE h, NOTGETNEXTDATA *pData)
{
    BOOL fRet = TRUE;
    NOTEVENTENTRY *pEvent = (NOTEVENTENTRY *)h;
    NOTVOLENTRY *pVolume = pEvent->pVolume;

    SetLastError (ERROR_SUCCESS);

    ENTERNOTIFY(pVolume);
    ResetEvent( pEvent->hEvent);
    if (pData && pEvent->dwCount) {
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
                } else {
                    if (dwCount) {
                        SetLastError(ERROR_MORE_DATA);
                    } else {
                        SetLastError(ERROR_INSUFFICIENT_BUFFER);
                    }    
                    break;
                }
                pEvent->pFileInfo = pFileInfo->pNext;
                delete [] pFileInfo->szFileName;
                delete pFileInfo;
            }
            if (!pEvent->pFileInfo) {
                pEvent->pFileInfoLast = NULL;
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
            if (pEvent->dwCount > dwCount) {
                pEvent->dwCount -= dwCount;
            } else {
                // do not let this value roll-back past zero
                pEvent->dwCount = 0;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);   
            fRet = FALSE;
        }

    } else if (pEvent->dwCount) {

        // Data not requested but there are pending notifications.
        pEvent->dwCount --;

    } else if (pData) {

        // Data requested, but there are no pending notifications.
        // In this case, return failure because we are being called
        // from the CeGetNotificationInfo API.
        __try {
            *(pData->lpBytesReturned) = 0;
            *(pData->lpBytesAvailable) = 0;
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

void CheckEventOnHandle(NOTEVENTENTRY *pEvent, DWORD dwFlags, BOOL bSubTree)
{
    while(pEvent) {
        BOOL bNotify = FALSE;
        if ((bSubTree || pEvent->bWatchSubTree) &&(pEvent->dwFlags & dwFlags)) {
#if 0
            NKDbgPrintfW( L"Signaling a change on handle (%08X) Count=%ld\r\n", pEvent, pEvent->dwCount);
            NOTDIRENTRY *pTemp= pEvent->pDirectory;
            while( pTemp) {
                NKDbgPrintfW(L"%s<-", pTemp->szDirectory);
                pTemp= pTemp->pPrevious;
            }
#endif
            if (!pEvent->dwCount) {
                SetEvent( pEvent->hEvent);
            }
            pEvent->dwCount++;
        }    
        pEvent = pEvent->pNext;
    }
}

// Called by a file system to indicate that an open file
// has been modified.
void NotifyHandleChangeEx(HANDLE hVolume, HANDLE h, DWORD dwFlags, DWORD dwAction)
{
    NOTHANDLEENTRY *pHandle = (NOTHANDLEENTRY *)h;
    NOTDIRENTRY *pCurrent = pHandle->pDirectory;
    BOOL bSubTree = TRUE;
//  NKDbgPrintfW( L"NotifyHandleChange on handle=%08X Flags=%08X\r\n", h, dwFlags);
    if (!IsValidData(h, NOT_HANDLE_SIG)) {
        return;
    }
    ENTERNOTIFY(hVolume);
    pHandle->dwFlags |= HANDLE_FLAG_FILECHANGED;
    if (bSubTree) {
        if (pCurrent) {
            AddFileToEvent( pCurrent->pEvents, pHandle->szFileName, dwFlags, dwAction);
        } else {
            AddFileToEvent( ((NOTVOLENTRY *)hVolume)->pEvents, pHandle->szFileName, dwFlags, dwAction);
        }
    }    
    while(pCurrent) {
        CheckEventOnHandle(pCurrent->pEvents, dwFlags, bSubTree);
        bSubTree = FALSE;
        pCurrent = pCurrent->pParent;
    }
    CheckEventOnHandle(((NOTVOLENTRY *)hVolume)->pEvents, dwFlags, bSubTree);
    EXITNOTIFY(hVolume);
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

