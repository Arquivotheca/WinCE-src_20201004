//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "fsnotify.h"

HANDLE g_hNotifyAPI = NULL;
CRITICAL_SECTION g_csMain;
NOTVOLENTRY *g_pRootVolume = NULL;

#ifdef UNDER_CE

#define ARRAY_SIZE(a)           (sizeof(a)/sizeof(a[0]))


CONST PFNVOID apfnFindNotify[3] = {
        (PFNVOID)NotifyCloseEvent,
        (PFNVOID)NULL,
        (PFNVOID)NotifyReset,
};

CONST DWORD asigFindNotify[3] = {
        FNSIG1(DW),                                     // FindClose
        FNSIG0(),                                       //
        FNSIG2(DW, PTR)                                 // FindNextFileW
};
#endif

#ifndef UNDER_CE
#define FILE_NOTIFY_CHANGE_CEGETINFO 0x80000000
#endif

void AddFileToEvent(NOTEVENTENTRY *pEvent, WCHAR *szFileName, DWORD dwFlags, DWORD dwAction);
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
        g_hNotifyAPI = CreateAPISet("FNOT", ARRAY_SIZE(apfnFindNotify), (const PFNVOID * )apfnFindNotify, asigFindNotify);
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
#ifdef UNDER_CE        
        SetHandleOwner( pEvent->hEvent, GetCurrentProcess());
#endif        
//      NKDbgPrintfW( L"Deleting event %08X\r\n", pEvent->hEvent);
        CloseHandle( pEvent->hEvent);
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
                    if (pbSubTree && *szTemp)
                        *pbSubTree  = FALSE;    
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
    } else {
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

HANDLE NotifyCreateEvent(HANDLE hVolume, HANDLE hProc,const WCHAR *szPath, BOOL bWatchSubTree, DWORD dwFlags)
{
//  NKDbgPrintfW( L"CreateNotificationEvent %s Flags=%ld\r\n", szPath, dwFlags);
    ENTERNOTIFY( hVolume);
    NOTDIRENTRY *pCurrent = InsertOrFindDirEntry(hVolume, szPath, NULL, TRUE, FALSE);
    NOTEVENTENTRY *pEvent = new NOTEVENTENTRY;
    if (!pEvent) {
        return INVALID_HANDLE_VALUE;
    }   
    NOTVOLENTRY *pVolume = (NOTVOLENTRY *)hVolume;
    pEvent->dwSig = NOT_EVENT_SIG;
    pEvent->dwFlags = dwFlags; 
    pEvent->dwCount = 0;
    pEvent->hProc = hProc;
    pEvent->bWatchSubTree = bWatchSubTree;
    pEvent->hEvent = CreateEvent( NULL, TRUE, FALSE, NULL);
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
    } else {
        pEvent->pNext = pVolume->pEvents;
        if (pVolume->pEvents) {
            pVolume->pEvents->pPrevious = pEvent;
        } 
        pVolume->pEvents = pEvent;
    }
    EXITNOTIFY(hVolume);
    pEvent->pVolume = pVolume;
#ifdef UNDER_CE    
    pEvent->hNotify = CreateAPIHandle( g_hNotifyAPI, pEvent);
    SetEventData( pEvent->hEvent, (DWORD)pEvent->hNotify);
    SetHandleOwner( pEvent->hNotify, hProc);
    return pEvent->hEvent;
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
    } else {
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
#ifdef UNDER_CE    
    SetHandleOwner( pEvent->hEvent, GetCurrentProcess());
#endif    
    CloseHandle( pEvent->hEvent);
    delete pEvent;
    return TRUE;
}

void AddFileToEvent(NOTEVENTENTRY *pEvent, WCHAR *szFileName, DWORD dwFlags, DWORD dwAction)
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
                    wcscpy( pFileInfo->szFileName, szFileName);
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
                SetEvent( pEvent->hEvent);
            }
            pEvent->dwCount++;
        }    
        pEvent = pEvent->pNext;
    }
}

void NotifyPathChange(HANDLE hVolume,const  WCHAR *szPath, BOOL bPath, DWORD dwAction)
{
    BOOL bSubTree = TRUE;
    WCHAR *szFileName = NULL;
    NOTDIRENTRY *pCurrent = NULL;
//    NKDbgPrintfW( L"NotifyPathChange on path %s bPath=%s\r\n", szPath , bPath ? L"TRUE" : L"FALSE");
    ENTERNOTIFY(hVolume);
    if (bPath && ((FILE_ACTION_REMOVED == dwAction) || (FILE_ACTION_RENAMED_OLD_NAME == dwAction))) {
        pCurrent = InsertOrFindDirEntry(hVolume, szPath, &szFileName, bPath, TRUE);
        if (pCurrent && szFileName && bSubTree && (wcscmp(szFileName, pCurrent->szDirectory) ==0)) {
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
        bPath = FALSE;            
        pCurrent = InsertOrFindDirEntry(hVolume, szPath, &szFileName, bPath, TRUE, &bSubTree);
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
        bPath = TRUE;
    } else {
        pCurrent = InsertOrFindDirEntry(hVolume, szPath, &szFileName, bPath, TRUE, &bSubTree);
        if (bSubTree) {
            AddFileToEvent( pCurrent ? pCurrent->pEvents : ((NOTVOLENTRY *)hVolume)->pEvents, 
                            szFileName, 
                            bPath ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME, 
                            dwAction);
        }    
        if (szFileName) {
            delete [] szFileName;
            szFileName = NULL;
        }    
    }    
    while(pCurrent) {
        CheckEventOnPath( pCurrent->pEvents, bSubTree, bPath);
        bSubTree = FALSE;
        pCurrent = pCurrent->pParent;
    }
    CheckEventOnPath( ((NOTVOLENTRY *)hVolume)->pEvents, bSubTree, bPath);
    EXITNOTIFY(hVolume);
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

void    NotifyMoveFile( HANDLE hVolume, LPCWSTR lpszExisting, LPCWSTR lpszNew)
{
    BOOL bPath = FALSE;
    DWORD dwAttr = 0;
    DWORD dwLen1 = GetFileNameOffset( lpszExisting);
    DWORD dwLen2 = GetFileNameOffset( lpszNew);
    BOOL bSameDir = TRUE;
    WCHAR szPath[MAX_PATH];
    if ((dwLen1 != dwLen2) || (wcsncmp( lpszExisting, lpszNew, dwLen1) != 0)) {
        bSameDir = FALSE;
    }    
    DWORD dwVolLen = ((NOTVOLENTRY *)hVolume)->szVolumeName ? wcslen(((NOTVOLENTRY *)hVolume)->szVolumeName) : 0;
    if ((dwVolLen > 1) && (wcslen(lpszNew)+dwVolLen+1 < MAX_PATH)) {
        wcscpy( szPath, ((NOTVOLENTRY *)hVolume)->szVolumeName);
        wcscat( szPath, lpszNew);
        dwAttr = GetFileAttributesW( szPath);
    }  else {
        dwAttr = GetFileAttributesW( lpszNew);
    }   
    if ((dwAttr != -1) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        bPath = TRUE;
    }     
    ENTERNOTIFY(hVolume);
    NotifyPathChange( hVolume, lpszExisting, bPath, bSameDir ? FILE_ACTION_RENAMED_OLD_NAME : FILE_ACTION_REMOVED);
    NotifyPathChange( hVolume, lpszNew, bPath, bSameDir ? FILE_ACTION_RENAMED_NEW_NAME : FILE_ACTION_ADDED);
    EXITNOTIFY(hVolume);
}


BOOL NotifyReset(HANDLE h, NOTGETNEXTDATA *pData)
{
    BOOL fRet = TRUE;
    NOTEVENTENTRY *pEvent = (NOTEVENTENTRY *)h;
    NOTVOLENTRY *pVolume = pEvent->pVolume;
    ResetEvent( pEvent->hEvent);
    ENTERNOTIFY(pVolume);
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
                DWORD dwLen = pFileInfo->dwFileNameLen + sizeof(WCHAR) + sizeof(FILE_NOTIFY_INFORMATION);
                if (dwLen & (sizeof(DWORD)-1)) {
                    dwLen += (sizeof(DWORD)-(dwLen & sizeof(DWORD)-1));
                }    
                if (pData->nBufferLength >= dwLen + dwBytesReturned) {
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
            pEvent->dwCount -= dwCount;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);   
            fRet = FALSE;
        }
    } else {
        if (pEvent->dwCount)
            pEvent->dwCount--;
        fRet = TRUE;    
    }    
    if (fRet) {
        if (pEvent->dwCount) {
            SetEvent(pEvent->hEvent);
        }
    }    
#ifdef UNDER_CE    
    SetHandleOwner( pEvent->hNotify, pEvent->hProc);
#endif    
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

void NotifyHandleChange(HANDLE hVolume, HANDLE h, DWORD dwFlags)
{
    NotifyHandleChangeEx( hVolume, h, dwFlags, FILE_ACTION_MODIFIED);
}

BOOL    NotifyGetNextChange(HANDLE h, DWORD dwFlags, LPVOID lpBuffer, DWORD nBufferLength, LPDWORD lpBytesReturned, LPDWORD lpBytesAvailable)
{
    BOOL bRet;
#ifdef UNDER_CE    
    HANDLE hNotify = (HANDLE)GetEventData(h);
    if (!hNotify && !(hNotify = GetNotifiyHandle(h))) {
        return FALSE;
    }     
#else
    HANDLE hNotify = h;
#endif    
    __try {
#ifdef UNDER_CE    
        SetHandleOwner( hNotify, GetCurrentProcess());
#endif        
        if ((dwFlags == 0) && (lpBuffer == NULL) && (nBufferLength == 0) && (lpBytesReturned == NULL) && (lpBytesAvailable == NULL)) {
#ifdef UNDER_CE        
            bRet = FindNextFile( hNotify, NULL);
#else
            NotifyReset( hNotify, NULL);
#endif            
        } else {
            NOTGETNEXTDATA Data;
#ifdef UNDER_CE            
            if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
                Data.lpBuffer = MapCallerPtr(lpBuffer, nBufferLength);
                Data.lpBytesReturned = (LPDWORD)MapCallerPtr( lpBytesReturned, sizeof(DWORD));
                Data.lpBytesAvailable = (LPDWORD)MapCallerPtr( lpBytesAvailable, sizeof(DWORD));
            } else {
#endif            
                Data.lpBuffer = lpBuffer;
                Data.lpBytesReturned = lpBytesReturned;
                Data.lpBytesAvailable = lpBytesAvailable;
#ifdef UNDER_CE                
            }  
#endif            
            Data.dwFlags = dwFlags;
            Data.nBufferLength = nBufferLength;
            bRet = FindNextFile( hNotify, (WIN32_FIND_DATA *)&Data);
        }    
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
    return bRet;
}

BOOL    NotifyCloseChangeHandle(HANDLE h)
{
#ifdef UNDER_CE    
    HANDLE hNotify = (HANDLE)GetEventData(h);
    if (!hNotify && !(hNotify = GetNotifiyHandle(h))) {
        return(FALSE);
    }
    SetHandleOwner( hNotify, GetCurrentProcess());
    FindClose( hNotify);
#else
    NotifyCloseEvent( h);
#endif    
    return TRUE;
}

