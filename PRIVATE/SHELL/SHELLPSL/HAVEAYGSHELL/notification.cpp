//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __WINDOWS__
#include <windows.h>
#endif //__WINDOWS__

#ifndef _AYGSHELL_H_
#include <aygshell.h>
#endif //_AYGSHELL_H_

#ifndef _INC_SHLWAPI
#include <shlwapi.h>
#endif //_INC_SHLWAPI

#ifndef _DBT_H
#include <dbt.h>
#endif //_DBT_H

#ifndef __MSGQUEUE_H__
#include <Msgqueue.h>
#endif //__MSGQUEUE_H__



#ifdef USE_CARDSERVER_API

#include <Pnp.h>

#ifndef __CARDSERV_H__
#include <Cardserv.h>
#endif //__CARDSERV_H__

#endif //USE_CARDSERVER_API

/*
 * local structs
 */

typedef struct _NOTIFYENTRY
{
    SHCHANGENOTIFYENTRY shcne;
    HWND                hwnd;
    DWORD               dwProcID;
    LPTSTR              pstrEventName;
} NOTIFYENTRY, * LPNOTIFYENTRY;

#ifndef USE_CARDSERVER_API
typedef struct _STORAGECARDDIRECTORYENTRY
{
    LPWSTR pstrDirectoryName;
    struct _STORAGECARDDIRECTORYENTRY * pNext;
} STORAGECARDDIRECTORYENTRY, * LPSTORAGECARDDIRECTORYENTRY;
#endif // USE_CARDSERVER_API

/*
 * local defines
 */

#define MEDIA_EVENT   WAIT_OBJECT_0+1
#define NETWORK_EVENT MEDIA_EVENT+1
#define WAIT_OFFSET (DWORD) (m_bHaveNetworkDirectory ? 3 : 2)
#define WAIT_TO_ENTRY( dwWait ) (dwWait - WAIT_OFFSET)
#define ENTRY_TO_WAIT( dwEntry ) (dwEntry + WAIT_OFFSET)
#define GET_EVENT_MASK( dwWait ) m_entries[ WAIT_TO_ENTRY( dwWait ) ].shcne.dwEventMask
#define NEEDS_EVENT( dwWait, dwEvent ) ( ( GET_EVENT_MASK( dwWait ) & dwEvent ) || !dwEvent )
#define LENGTH_OF(x) ( (sizeof((x))) / (sizeof(*(x))) )

#define MAX_WAITOBJECTS     32
#define MAX_ENTRIES         29
#define NOTIFY_BUFFER_MAX   0x0FFF

#define ROOT_DIRECTORY      TEXT("\\")
#define DIRECTORY_SEPARATOR TEXT("\\")

#define NEEDS_ATTRIBUTES_EVENT_MASK ( SHCNE_CREATE | SHCNE_MKDIR | SHCNE_ATTRIBUTES | SHCNE_UPDATEITEM )
#define DOES_THIS_EVENT_NEED_ATTRIBUTES( dwEvent ) ( ( dwEvent & NEEDS_ATTRIBUTES_EVENT_MASK ) == dwEvent )

#define NEEDS_PATH_COMPOSED_EVENT_MASK ( NEEDS_ATTRIBUTES_EVENT_MASK | SHCNE_DELETE | SHCNE_RMDIR | SHCNE_RENAMEITEM | SHCNE_RENAMEFOLDER )
#define DOES_THIS_FILE_NEED_A_COMPOSED_PATH( dwEvent ) ( ( dwEvent & NEEDS_PATH_COMPOSED_EVENT_MASK ) == dwEvent )

// stolen from extfile.h
#define AFS_ROOTNUM_STORECARD 0
#define AFS_ROOTNUM_NETWORK 1
#define SYSTEM_MNTVOLUME		0xe
#define OIDFROMAFS(iAFS)        ((CEOID)((SYSTEM_MNTVOLUME<<28)|((iAFS)&0xfffffff)))

/*
 * Convenience Functions
 */

TCHAR *
W2T( WCHAR *pwszIn, HPROCESS proc = NULL )
{
    TCHAR *pszOut = NULL;

#ifdef _UNICODE
    if ( !proc )
    {
        pszOut = (TCHAR *) malloc( sizeof( WCHAR ) * ( wcslen( pwszIn ) + 1 ) );
    }
    else
    {
        HLOCAL hMem = 0;
        hMem = LocalAllocInProcess( LMEM_FIXED, sizeof( WCHAR ) * ( wcslen( pwszIn ) + 1 ), proc );
        if ( !hMem )
        {
            return NULL;
        }

        pszOut = reinterpret_cast<WCHAR *>(hMem);
    }

    if ( pszOut )
    {
        wcscpy( pszOut, pwszIn );
    }
#else
    size_t size = wcstombs( NULL, pwszIn, 1 );
    if ( size > 0 )
    {
        if ( !proc )
        {
            pszOut = (TCHAR *) malloc( size );
        }
        else
        {
            HLOCAL hMem = 0;
            hMem = LocalAllocInProcess( LMEM_FIXED, size, proc );
            if ( !hMem )
            {
                return NULL;
            }

            pszOut = reinterpret_cast<WCHAR *>(hMem);
        }
    }
    if ( pszOut )
    {
        wcstombs( pszOut, pwszIn, size );
    }
#endif

    return pszOut;
}

WCHAR * ConstructFullPathW( TCHAR *pszFile, TCHAR *pszDir )
{
    WCHAR *pwszFile = NULL;
    WCHAR *pwszDir = NULL;
    WCHAR *pwszPath = NULL;
#ifdef _UNICODE
    // already have wide string inputs
    pwszFile = pszFile;
    pwszDir = pszDir;
#else
    pwszFile = T2W( pszFile );
    pwszDir = T2W( pszDir );
#endif // _UNICODE

    if ( !pwszDir || !pwszFile )
    {
        goto exit;
    }

    // check to see if the last char of the dir is a /
    if ( wcscmp( pwszDir + ( wcslen( pwszDir ) - 1 ), ROOT_DIRECTORY ) != 0 )
    {
        pwszPath = (WCHAR *) malloc( sizeof( WCHAR ) * ( wcslen( pwszDir ) + wcslen( pwszFile ) + 2 ) );
        if ( !pwszPath )
        {
            goto exit;
        }
        wcscpy( pwszPath, pwszDir );
        wcscat( pwszPath, DIRECTORY_SEPARATOR );
        wcscat( pwszPath, pwszFile );
    }
    else
    {
        pwszPath = (WCHAR *) malloc( sizeof( WCHAR ) * ( wcslen( pwszDir ) + wcslen( pwszFile ) + 1 ) );
        if ( !pwszPath )
        {
            goto exit;
        }
        wcscpy( pwszPath, pwszDir );
        wcscat( pwszPath, pwszFile );
    }

exit:
#ifndef _UNICODE
    if ( pwszFile )
    {
        free( pwszFile );
    }

    if ( pwszDir )
    {
        free( pwszDir );
    }
#endif // !_UNICODE

    return pwszPath;
}

BOOL
GetRemotePath(
    WCHAR *pwszLocalPath,
    WCHAR *pwszRemotePath
    )
{
    SHFILEINFO shfi;
    if ( !::SHGetFileInfo( pwszLocalPath, 0, &shfi, sizeof( shfi ), SHGFI_DISPLAYNAME ) )
    {
        return FALSE;
    }

    if ( _tcslen( shfi.szDisplayName ) < 1 )
    {
        return FALSE;
    }

    // find the last " on "
    TCHAR *pos = _tcsstr( shfi.szDisplayName, _T( " on " ) );
    if ( pos != NULL )
    {
        TCHAR *pos2 = pos;
        while( pos2 != NULL )
        {
            pos2 = _tcsstr( pos2+1, _T( " on " ) );
            if ( pos2 )
            {
                pos = pos2;
            }
        }
    }
    else
    {
        return FALSE;
    }

    // do tricks because we know we're not MBCS
    //int iFirstLen = (pos - shfi.szDisplayName) / sizeof( WCHAR );
    swprintf( pwszRemotePath, L"\\\\" );
    wcscat( pwszRemotePath, pos+4 );
    wcscat( pwszRemotePath, DIRECTORY_SEPARATOR );
    wcsncat( pwszRemotePath, shfi.szDisplayName, (pos - shfi.szDisplayName) );

    return TRUE;
}

FILECHANGENOTIFY *
CreateFileChangeNotify(
    LONG wEventId,
    WCHAR *pwszFilename,
    NOTIFYENTRY entry
    )
{
    HPROCESS hProc = ::OpenProcess( 0, FALSE, entry.dwProcID );
    if ( !hProc )
    {
        return NULL;
    }

    FILECHANGENOTIFY *pNotify = NULL;
    HLOCAL hMem = 0;
    hMem = LocalAllocInProcess( LMEM_FIXED, sizeof( FILECHANGENOTIFY ), hProc );
    if ( !hMem )
    {
        CloseHandle( hProc );
        return NULL;
    }

    pNotify = reinterpret_cast<FILECHANGENOTIFY *>(hMem);

    if ( pNotify != NULL )
    {
        pNotify->dwRefCount = 1;
        pNotify->fci.cbSize = sizeof( FILECHANGEINFO );
        pNotify->fci.wEventId = wEventId;
        pNotify->fci.uFlags = 0;
        pNotify->fci.dwItem1 = 0;
        pNotify->fci.dwAttributes = 0;
        pNotify->fci.ftModified.dwLowDateTime = 0;
        pNotify->fci.ftModified.dwHighDateTime = 0;
        pNotify->fci.nFileSize = 0;
        if ( ( wEventId != SHCNE_UPDATEIMAGE ) && ( pwszFilename != NULL ) )
        {
            pNotify->fci.uFlags = SHCNF_PATH;
            pNotify->fci.dwItem1 = (DWORD) W2T( pwszFilename, hProc );
            if ( DOES_THIS_EVENT_NEED_ATTRIBUTES( wEventId ) )
            {
                WCHAR *pwszPath = NULL;
                if ( DOES_THIS_FILE_NEED_A_COMPOSED_PATH( wEventId ) )
                {
                    pwszPath = ConstructFullPathW( (TCHAR *) pNotify->fci.dwItem1, entry.shcne.pszWatchDir );
                }
                else
                {
                    pwszPath = pwszFilename;
                }
                WIN32_FIND_DATA wfd;
                if ( pwszPath && ( FindFirstFile( pwszPath, &wfd ) != INVALID_HANDLE_VALUE ) )
                {
                    pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                    pNotify->fci.ftModified = wfd.ftLastWriteTime;
                    pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
                }
                if ( pwszPath && ( pwszPath != pwszFilename ) )
                {
                    free( pwszPath );
                }
            }
        }
        pNotify->fci.dwItem2 = 0;
    }

    CloseHandle( hProc );
    return pNotify;
}

void
CopyChangeNotifyEntry(
    SHCHANGENOTIFYENTRY *pDest,
    SHCHANGENOTIFYENTRY *pSrc
    )
{
    pDest->dwEventMask = pSrc->dwEventMask;
    pDest->fRecursive = pSrc->fRecursive;
    if ( pDest->pszWatchDir )
    {
        LocalFree( pDest->pszWatchDir );
        pDest->pszWatchDir = NULL;
    }
    pDest->pszWatchDir = (TCHAR *) LocalAlloc( LPTR, sizeof(TCHAR) * ( _tcslen( pSrc->pszWatchDir ) + 1 ) );
    if ( pDest->pszWatchDir )
    {
        _tcscpy( pDest->pszWatchDir, pSrc->pszWatchDir );
    }
}


/*
 * Change Watcher Class Definition
 */

class ChangeWatcher_t
{
private:
    static const DWORD  NOTIFY_FILTER;

    HANDLE              m_hThread;
    CRITICAL_SECTION    m_cs;
    DWORD               m_dwWaitObjects;
    HANDLE              m_waitObjects[ MAX_WAITOBJECTS ];
    NOTIFYENTRY         m_entries[ MAX_ENTRIES ];
    TCHAR               m_pszNetworkDir[ MAX_PATH ];
    DWORD               m_dwWaitingForCallbackId;
    DWORD               m_dwWaitingForCallbackProcessID;
    WCHAR               m_szLocalPath[MAX_PATH];
    WCHAR               m_szRemoteSharePath[MAX_PATH];
    DWORD               m_dwCurrentEvent;
    BOOL                m_bSentMessage;

    BOOL                m_bInCriticalSection;
    BOOL                m_bCriticalSectionInitialized;

    BOOL                m_bHaveNetworkDirectory;

#ifndef USE_CARDSERVER_API
    LPSTORAGECARDDIRECTORYENTRY         m_pStorageCardDirectoryEntries;
#endif //USE_CARDSERVER_API

    static DWORD WINAPI ThreadProc( LPVOID lpParameter );
    void    OnChange( UINT dwWaitIndex );
    void    OnDeviceChange( UINT dwWaitIndex );
    void    ProcessEvents();
    void    BeginWatch();
    void    ShutdownWatch();
    LPBYTE  GetNotificationBuffer( UINT uiWaitIndex );
    void    RemoveFromList( DWORD dwWaitIndex );
    DWORD   FindNextInterestedEntry( DWORD dwStartIndex, DWORD dwEventId );
    void    DoPostMessage( DWORD dwWaitIndex, DWORD dwEventID, LPTSTR pszFilename );
    void    SetNetAndCardEvents( DWORD dwEventID, WCHAR *pwszFullPath );
    void    CleanHouse();
    BOOL    TryToEnterCriticalSection();
    BOOL    ExitTheCriticalSection();

#ifndef USE_CARDSERVER_API
    void    AddStorageCardDirectoryEntry( WCHAR *pwszPath );
    void    RemoveStorageCardDirectoryEntry( WCHAR *pwszPath );
    BOOL    IsInStorageCardDirectoryEntries( WCHAR *pwszPath );
    BOOL    IsStorageCardDirectory( WCHAR *pwszPath );
    void    CleanStorageCardDirectoryEntriesList();
#endif //USE_CARDSERVER_API

public:
    ChangeWatcher_t();
    ~ChangeWatcher_t();

    BOOL AddWatch( HWND hwnd, SHCHANGENOTIFYENTRY *pschne, DWORD dwProcID, LPCTSTR pszEventName, BOOL *pbReplace );
    BOOL RemoveWatch( HWND hwnd );
    BOOL SendMessage( HPROCESS proc );
};

/*
 * ChangeWatcher_t Class Methods
 */

const
DWORD
ChangeWatcher_t::
NOTIFY_FILTER = FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES |
                FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_LAST_ACCESS |
                FILE_NOTIFY_CHANGE_CREATION |
                FILE_NOTIFY_CHANGE_SECURITY |
                FILE_NOTIFY_CHANGE_CEGETINFO;


ChangeWatcher_t::
ChangeWatcher_t() :
    m_hThread(NULL),
    m_dwWaitObjects(0)
{
    __try
    {
        ::InitializeCriticalSection( &m_cs );
        m_bCriticalSectionInitialized = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        m_bCriticalSectionInitialized = FALSE;
    }

    ASSERT( m_bCriticalSectionInitialized );

    m_bInCriticalSection = FALSE;
    
    for ( int i = 0; i < MAX_WAITOBJECTS; i++ )
    {
        m_waitObjects[ i ] = NULL;
    }

    m_waitObjects[ m_dwWaitObjects++ ] = ::CreateEvent( NULL, TRUE, FALSE, NULL );

#ifndef USE_CARDSERVER_API
    m_pStorageCardDirectoryEntries = NULL;
#endif // USE_CARDSERVER_API

    // increment for Media Insert/Delete
    m_dwWaitObjects++;

    // do we have networking installed
    m_bHaveNetworkDirectory = FALSE;
    CEOIDINFO ceOidInfo;

    // Load the localized name of the \NETWORK folder
    if (CeOidGetInfo(OIDFROMAFS(AFS_ROOTNUM_NETWORK), &ceOidInfo)) {
        // name in ceOidInfo.infDirectory.szDirName
        _tcscpy( m_pszNetworkDir, ceOidInfo.infDirectory.szDirName );
        DWORD dwFileAttribs = ::GetFileAttributes( m_pszNetworkDir );
        if ( dwFileAttribs != -1 )
        {
            m_bHaveNetworkDirectory = (dwFileAttribs & FILE_ATTRIBUTE_DIRECTORY);
        }
    }

    if ( m_bHaveNetworkDirectory )
    {
        // increment for network watcher
        m_dwWaitObjects++;
    }

    for ( i = 0; i < MAX_ENTRIES; i++ )
    {
        m_entries[ i ].hwnd = NULL;
        m_entries[ i ].dwProcID = 0;
		m_entries[ i ].pstrEventName = NULL;
        m_entries[ i ].shcne.dwEventMask = 0;
        m_entries[ i ].shcne.pszWatchDir = NULL;
        m_entries[ i ].shcne.fRecursive = FALSE;
    }
    BeginWatch();
}


ChangeWatcher_t::
~ChangeWatcher_t()
{
    if (m_hThread)
       ShutdownWatch();

    if (m_waitObjects[0])
       ::CloseHandle(m_waitObjects[0]);
 
    // starts either at the first file notification
    // or the network directory watcher
#ifdef USE_CARDSERVER_API
    for ( DWORD i = 2; i < MAX_WAITOBJECTS; i++ )
#else //USE_CARDSERVER_API
    // now using file notification for device changes
    for ( DWORD i = 1; i < MAX_WAITOBJECTS; i++ )
#endif //USE_CARDSERVER_API
    {
        if ( m_waitObjects[ i ] )
        {
            ::FindCloseChangeNotification( m_waitObjects[ i ] );
        }
    }

    for ( i = 0; i < MAX_ENTRIES; i++ )
    {
        if ( m_entries[ i ].shcne.pszWatchDir )
        {
            LocalFree( m_entries[ i ].shcne.pszWatchDir );
        }

		if ( m_entries[ i ].pstrEventName )
		{
			LocalFree( m_entries[ i ].pstrEventName );
		}
    }

    if ( m_bCriticalSectionInitialized )
    {
        ::DeleteCriticalSection(&m_cs);
    }

#ifndef USE_CARDSERVER_API
    while( m_pStorageCardDirectoryEntries != NULL )
    {
        free( m_pStorageCardDirectoryEntries->pstrDirectoryName );
        LPSTORAGECARDDIRECTORYENTRY pNext = m_pStorageCardDirectoryEntries->pNext;
        free( m_pStorageCardDirectoryEntries );
        m_pStorageCardDirectoryEntries = pNext;
    }
#endif // USE_CARDSERVER_API
}

#ifndef USE_CARDSERVER_API
void
ChangeWatcher_t::
AddStorageCardDirectoryEntry( WCHAR *pwszPath )
{
    if ( IsInStorageCardDirectoryEntries( pwszPath ) )
    {
        // already in the list
        return;
    }

    LPSTORAGECARDDIRECTORYENTRY pNew = (LPSTORAGECARDDIRECTORYENTRY) malloc( sizeof( STORAGECARDDIRECTORYENTRY ) );
    if( !pNew )
    {
        // out of memory
        return;
    }

    pNew->pstrDirectoryName = (LPWSTR) malloc( ( wcslen( pwszPath ) + 1 ) * sizeof( WCHAR ) );
    wcscpy( pNew->pstrDirectoryName, pwszPath );
    pNew->pNext = m_pStorageCardDirectoryEntries;
    m_pStorageCardDirectoryEntries = pNew;
}

void
ChangeWatcher_t::
RemoveStorageCardDirectoryEntry( WCHAR *pwszPath )
{
    LPSTORAGECARDDIRECTORYENTRY pPrevious = NULL;
    LPSTORAGECARDDIRECTORYENTRY pCurrent = NULL;

    pPrevious = m_pStorageCardDirectoryEntries;
    if ( !pPrevious )
    {
        return;
    }
    
    if ( wcscmp( pPrevious->pstrDirectoryName, pwszPath ) == 0 )
    {
        m_pStorageCardDirectoryEntries = m_pStorageCardDirectoryEntries->pNext;
        free( pPrevious->pstrDirectoryName );
        free( pPrevious );
        return;
    }

    pCurrent = pPrevious->pNext;
    while( pCurrent != NULL )
    {
        if ( wcscmp( pCurrent->pstrDirectoryName, pwszPath ) == 0 )
        {
            pPrevious->pNext = pCurrent->pNext;
            free( pCurrent->pstrDirectoryName );
            free( pCurrent );
            return;
        }

        pPrevious = pCurrent;
        pCurrent = pCurrent->pNext;
    }

}

BOOL
ChangeWatcher_t::
IsInStorageCardDirectoryEntries( WCHAR *pwszPath )
{
    LPSTORAGECARDDIRECTORYENTRY pCurrent = m_pStorageCardDirectoryEntries;

    while( pCurrent )
    {
        if ( wcscmp( pCurrent->pstrDirectoryName, pwszPath ) == 0 )
        {
            return TRUE;
        }

        pCurrent = pCurrent->pNext;
    }

    return FALSE;
}

BOOL
ChangeWatcher_t::
IsStorageCardDirectory( WCHAR *pwszPath )
{
    DWORD dwAttrib = GetFileAttributes( pwszPath );
    if ( ( dwAttrib != -1 ) && ( dwAttrib & FILE_ATTRIBUTE_DIRECTORY ) &&
         ( dwAttrib & FILE_ATTRIBUTE_TEMPORARY ) )
    {
        return TRUE;
    }

    return FALSE;
}

void
ChangeWatcher_t::
CleanStorageCardDirectoryEntriesList()
{
    LPSTORAGECARDDIRECTORYENTRY pCurrent = m_pStorageCardDirectoryEntries;

    while( pCurrent )
    {
        WCHAR *pszDirName = pCurrent->pstrDirectoryName;
        if ( !IsStorageCardDirectory( pszDirName ) )
        {
            pCurrent = pCurrent->pNext;
            RemoveStorageCardDirectoryEntry( pszDirName );
        }
        else
        {
            pCurrent = pCurrent->pNext;
        }
    }
}
#endif // USE_CARDSERVER_API

BOOL
ChangeWatcher_t::
TryToEnterCriticalSection()
{
    if ( m_bInCriticalSection || !m_bCriticalSectionInitialized )
    {
        return FALSE;
    }

    ::EnterCriticalSection(&m_cs);
    m_bInCriticalSection = TRUE;

    return TRUE;
}

BOOL
ChangeWatcher_t::
ExitTheCriticalSection()
{
    if ( !m_bCriticalSectionInitialized )
    {
        return FALSE;
    }

    ASSERT( m_bInCriticalSection );
    ::LeaveCriticalSection( &m_cs );
    m_bInCriticalSection = FALSE;

    return TRUE;
}

DWORD
WINAPI
ChangeWatcher_t::
ThreadProc(
    LPVOID lpParameter
    )
{
    ChangeWatcher_t * pChangeWatcher = (ChangeWatcher_t *) lpParameter;
    pChangeWatcher->ProcessEvents();
    return 0;
}

LPBYTE
ChangeWatcher_t::
GetNotificationBuffer(
    UINT dwWaitIndex
    )
{
    DWORD cbBuffer = 0;
    LPBYTE pBuffer = NULL;
    UINT dwAdjustedWaitIndex = dwWaitIndex;

    if (!::CeGetFileNotificationInfo(m_waitObjects[dwAdjustedWaitIndex], 0, NULL,
                                    0, NULL, &cbBuffer))
    {
        return NULL;
    }
    if ( cbBuffer == 0 )
    {
        // In McKendrick this can happen in fRecursive is true and
        // a subdirectory has changed
        return NULL;
    }
    ASSERT(0 < cbBuffer);

    if ( NOTIFY_BUFFER_MAX < cbBuffer )
    {
        cbBuffer = NOTIFY_BUFFER_MAX;
    }

    pBuffer = (LPBYTE) malloc(cbBuffer);
    if (!pBuffer)
        return NULL;

    if (!::CeGetFileNotificationInfo(m_waitObjects[dwAdjustedWaitIndex], 0, pBuffer,
                                    cbBuffer, NULL, NULL))
    {
        free(pBuffer);
        return NULL;
    }

    return pBuffer;
}


void
ChangeWatcher_t::
DoPostMessage(
    DWORD dwWaitIndex,
    DWORD dwEventID,
    LPTSTR pszFileName
    )
{
    if ( NEEDS_EVENT( dwWaitIndex, dwEventID ) ) 
    {
        FILECHANGENOTIFY *pNotify = CreateFileChangeNotify( dwEventID, pszFileName, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ] );
        // using Post will avoid Deadlock, but might offer unexpected results
        if ( pNotify )
        {
            ::PostMessage( m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );
        }
    }
}

void
ChangeWatcher_t::
OnChange(
    UINT dwWaitIndex
    )
{
    LPBYTE pBuffer = NULL;

    pBuffer = GetNotificationBuffer( dwWaitIndex );
    if ( !pBuffer )
    {
        return;
    }

    if ( !IsWindow( m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].hwnd ) )
    {
        // should be in a critical section here in the main loop
//        RemoveFromList( WAIT_TO_ENTRY(dwWaitIndex) );
        free( pBuffer );
        return;
    }

    FILE_NOTIFY_INFORMATION * pNotifyInfo = NULL;
    DWORD cbOffset = 0;
    do
    {
        pNotifyInfo = (FILE_NOTIFY_INFORMATION *) (pBuffer+cbOffset);
        if ( pNotifyInfo && ( pNotifyInfo->FileNameLength > 0 ) )
        {
            if ( ::_tcscmp( ROOT_DIRECTORY, pNotifyInfo->FileName ) != 0 )
            {
                WCHAR *pwszFullPath = ConstructFullPathW( pNotifyInfo->FileName, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                switch (pNotifyInfo->Action)
                {
                    case FILE_ACTION_ADDED:
                    {
                        if ( !PathIsDirectory( pwszFullPath ) )
                        {
                            DoPostMessage( dwWaitIndex, SHCNE_CREATE, pNotifyInfo->FileName );
                        }
                        else
                        {
                            DoPostMessage( dwWaitIndex, SHCNE_MKDIR, pNotifyInfo->FileName );
                        }

                        // DoPostMessage checks the event type
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEDIR, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                    }
                    break;

                    case FILE_ACTION_REMOVED:
                    {
                        if ( !PathIsDirectory( pwszFullPath ) )
                        {
                            DoPostMessage( dwWaitIndex, SHCNE_DELETE, pNotifyInfo->FileName );
                        }
                        else
                        {
                            DoPostMessage( dwWaitIndex, SHCNE_RMDIR, pNotifyInfo->FileName );
                        }
                    
                        // DoPostMessage checks the event type
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEDIR, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                    }
                    break;

                    case FILE_ACTION_MODIFIED:
                    {
                        // don't know what the difference between these two events is
                        DoPostMessage( dwWaitIndex, SHCNE_ATTRIBUTES, pNotifyInfo->FileName );
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEITEM, pNotifyInfo->FileName );
                    }
                    break;

                    case FILE_ACTION_RENAMED_OLD_NAME:
                    {
                        TCHAR szOldName[ MAX_PATH ];
                        _tcscpy( szOldName, pNotifyInfo->FileName );
                        cbOffset += pNotifyInfo->NextEntryOffset;
                        pNotifyInfo = (FILE_NOTIFY_INFORMATION *) (pBuffer+cbOffset);
                        ASSERT(pNotifyInfo);
                        ASSERT(FILE_ACTION_RENAMED_NEW_NAME == pNotifyInfo->Action);

                        WCHAR *pwszNewFullPath = ConstructFullPathW( pNotifyInfo->FileName, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                        if ( !PathIsDirectory( pwszNewFullPath ) )
                        {
                            if ( NEEDS_EVENT( dwWaitIndex, SHCNE_RENAMEITEM ) )
                            {
                                FILECHANGENOTIFY *pNotify = CreateFileChangeNotify( SHCNE_RENAMEITEM, szOldName, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ] );

                                if ( pNotify )
                                {
                                    HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].dwProcID );
                                    if ( hProc )
                                    {
                                        pNotify->fci.dwItem2 = (DWORD) W2T( pNotifyInfo->FileName, hProc );
                                        WIN32_FIND_DATA wfd;
                                        if ( pwszNewFullPath && ( FindFirstFile( pwszNewFullPath, &wfd ) != INVALID_HANDLE_VALUE ) )
                                        {
                                            pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                                            pNotify->fci.ftModified = wfd.ftLastWriteTime;
                                            pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
                                        }
    
                                        ::PostMessage( m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );

                                        ::CloseHandle( hProc );
                                    }
                                
                                    // REVIEW: Leaks Memory if hProc is invalid, but the CreateFileChangeNotify should return NULL in that
                                    // case anyway, so only time that will happen is if process exits between CreateFilechangeNotify and the next
                                    // OpenProcess
                                }
                            }
                        }
                        else
                        {
                            if ( NEEDS_EVENT( dwWaitIndex, SHCNE_RENAMEFOLDER ) )
                            {
                                FILECHANGENOTIFY *pNotify = CreateFileChangeNotify( SHCNE_RENAMEFOLDER, szOldName, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ] );

                                if ( pNotify )
                                {
                                    HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].dwProcID );
                                    if ( hProc )
                                    {
                                        pNotify->fci.dwItem2 = (DWORD) W2T( pNotifyInfo->FileName, hProc );
                                        WIN32_FIND_DATA wfd;
                                        if ( pwszNewFullPath && ( FindFirstFile( pwszNewFullPath, &wfd ) != INVALID_HANDLE_VALUE ) )
                                        {
                                            pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                                            pNotify->fci.ftModified = wfd.ftLastWriteTime;
                                            pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
                                        }
    
                                        ::PostMessage( m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );

                                        ::CloseHandle( hProc );
                                    }
                                
                                    // REVIEW: Leaks Memory if hProc is invalid, but the CreateFileChangeNotify should return NULL in that
                                    // case anyway, so only time that will happen is if process exits between CreateFilechangeNotify and the next
                                    // OpenProcess
                                }
                            }
                        }
                        free( pwszNewFullPath );

                        // DoPostMessage checks the event type
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEDIR, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                        break;
                    }
                    default:
                        break;
                }
                free( pwszFullPath );
            }
            else
            {
                // Special notification which means this directory no longer exists
                //         m_ShellChangeNotify[dwWaitIndex-1]->OnChange(SHCNE_RMDIR, NULL, NULL);
            }
        }
        else
        {
            // don't know how to handle this case
            DEBUGCHK( FALSE );
            RETAILMSG( 1, (TEXT("ChangeWatcher_t::OnChange - Got a FILE_NOTIFICATION_INFORMATION structure with a zero file name length, don't know how to handle.  Ignoring\r\n")));
        }

        if (!pNotifyInfo) break;

        cbOffset += pNotifyInfo->NextEntryOffset;
    } while(pNotifyInfo->NextEntryOffset);

    free(pBuffer);
}

void
ChangeWatcher_t::
OnDeviceChange(
    UINT dwWaitIndex
    )
{
    if ( !IsWindow( m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].hwnd ) )
    {
        return;
    }

    FILECHANGENOTIFY *pNotify;
    switch( m_dwCurrentEvent )
    {
        case SHCNE_MEDIAINSERTED:
        case SHCNE_MEDIAREMOVED:
            DoPostMessage( dwWaitIndex, m_dwCurrentEvent, m_szLocalPath );
            break;
        case SHCNE_NETSHARE:
        case SHCNE_NETUNSHARE:
            pNotify = CreateFileChangeNotify( m_dwCurrentEvent, m_szLocalPath, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ] );

            if ( pNotify )
            {
                HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].dwProcID );
                if ( hProc )
                {
                    pNotify->fci.dwItem2 = (DWORD) W2T( m_szRemoteSharePath, hProc );
                    WIN32_FIND_DATA wfd;
                    if ( FindFirstFile( m_szRemoteSharePath, &wfd ) != INVALID_HANDLE_VALUE )
                    {
                        pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                        pNotify->fci.ftModified = wfd.ftLastWriteTime;
                        pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
                    }

                    ::PostMessage( m_entries[ WAIT_TO_ENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );

                    ::CloseHandle( hProc );
                }
            }
            break;
        default:
            ASSERT( FALSE );
            break;
    }
}

BOOL
ChangeWatcher_t::
SendMessage(
    HPROCESS hProc
    )
{
    if ( ( (DWORD) hProc != m_dwWaitingForCallbackProcessID ) || ( m_dwWaitingForCallbackId == -1 ) || ( m_bSentMessage ) || !m_bCriticalSectionInitialized )
    {
        return FALSE;
    }

    if ( m_dwCurrentEvent == -1 )
    {
        OnChange( m_dwWaitingForCallbackId );
    }
    else
    {
        OnDeviceChange( m_dwWaitingForCallbackId );
    }

    m_bSentMessage = TRUE;
    return TRUE;
}

DWORD
ChangeWatcher_t::
FindNextInterestedEntry(
    DWORD dwStartIndex,
    DWORD dwEventId
    )
{
    for ( DWORD i = dwStartIndex; i < WAIT_TO_ENTRY( m_dwWaitObjects ); i++ )
    {
        if ( m_entries[ i ].shcne.dwEventMask & dwEventId )
        {
            return i;
        }
    }

    return -1;
}

void
ChangeWatcher_t::
SetNetAndCardEvents(
    DWORD dwEventID,
    WCHAR *pwszFullPath
    )
{
    if ( !pwszFullPath )
    {
        return;
    }

    // new share
    DWORD dwEntryID = FindNextInterestedEntry( 0, dwEventID );
    
    if ( dwEntryID == -1 )
    {
        return;
    }

    // if we can't enter the critical section, we're probably
    // trying to shut down the thread.  Blow off these events
    // so that we don't deadlock
    if ( TryToEnterCriticalSection() )
    {
        // store the local path
        wcscpy( m_szLocalPath, pwszFullPath );
        m_dwCurrentEvent = dwEventID;

        while( dwEntryID != -1 )
        {
            m_dwWaitingForCallbackId = ENTRY_TO_WAIT( dwEntryID );
            m_dwWaitingForCallbackProcessID = m_entries[ dwEntryID ].dwProcID;
            m_bSentMessage = FALSE;
            // set Change Event
            HANDLE hEvent = NULL;
		    if ( m_entries[ dwEntryID ].pstrEventName != NULL )
		    {
			    hEvent = ::OpenEvent( EVENT_ALL_ACCESS, FALSE, m_entries[ dwEntryID ].pstrEventName );
		    }
            BOOL bEventSet = ( hEvent && ::SetEvent( hEvent ));

            // block until callback happens or process exits
            DWORD dwExitCode = STILL_ACTIVE;
            HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ dwEntryID ].dwProcID );
            while( bEventSet && !m_bSentMessage && hProc &&
                   GetExitCodeProcess( hProc, &dwExitCode ) &&
                   ( dwExitCode == STILL_ACTIVE ) )
            {
            }

            // done blocking
            m_dwWaitingForCallbackId = -1;
            m_dwWaitingForCallbackProcessID = -1;
            m_bSentMessage = TRUE;

            CloseHandle( hProc );
            CloseHandle( hEvent );

            // send message and block
            dwEntryID = FindNextInterestedEntry( dwEntryID + 1, dwEventID );
        }
        ExitTheCriticalSection();
    }
}

void
ChangeWatcher_t::
ProcessEvents()
{
    BOOL fContinue = TRUE;
    DWORD dwWait;

#ifdef USE_CARDSERVER_API
    if ( m_waitObjects[ MEDIA_EVENT ] != NULL )
    {
        ::CloseMsgQueue( m_waitObjects[ MEDIA_EVENT ] );
        m_waitObjects[ MEDIA_EVENT ] = NULL;
    }

    // create the QUEUE for Media events
	BYTE pbMsgBuf[sizeof(DEVDETAIL) + (sizeof(TCHAR) * MAX_DEVCLASS_NAMELEN)];
	PDEVDETAIL pdd = (PDEVDETAIL) pbMsgBuf;
	MSGQUEUEOPTIONS msgopts;
	memset(&msgopts, 0, sizeof(msgopts));
    msgopts.dwSize = sizeof(msgopts);
    msgopts.dwFlags = 0;
    msgopts.dwMaxMessages = 0;
    msgopts.cbMaxMessage = sizeof(pbMsgBuf);
    msgopts.bReadAccess = TRUE;

    HANDLE hQueue = ::CreateMsgQueue(NULL, &msgopts);
    ASSERT( hQueue );
    if ( !hQueue )
    {
        // ERROR!
        return;
    }

    GUID guid = DEVCLASS_CARDSERV_GUID;
    HANDLE hNotifications = ::RequestDeviceNotifications( &guid, hQueue, FALSE );
    m_waitObjects[ MEDIA_EVENT ] = hQueue;
    ASSERT( hNotifications );
    if ( !hNotifications )
    {
        // ERROR!
        return;
    }
#else //USE_CARDSERVER_API
    if ( m_waitObjects[ MEDIA_EVENT ] != NULL )
    {
        ::FindCloseChangeNotification( m_waitObjects[ MEDIA_EVENT ] );
        m_waitObjects[ MEDIA_EVENT ] = NULL;
    }

    m_waitObjects[ MEDIA_EVENT ] = ::FindFirstChangeNotification( ROOT_DIRECTORY,
                                                                  FALSE,
                                                                  NOTIFY_FILTER );
    ASSERT( m_waitObjects[ MEDIA_EVENT ] );

    if ( !m_waitObjects[ MEDIA_EVENT ] )
    {
        // ERROR
        return;
    }
#endif //USE_CARDSERVER_API

    // create the file notifications for the network directory
    if ( m_bHaveNetworkDirectory )
    {
        if ( m_waitObjects[ NETWORK_EVENT ] != NULL )
        {
            ::FindCloseChangeNotification( m_waitObjects[ NETWORK_EVENT ] );
            m_waitObjects[ NETWORK_EVENT ] = NULL;
        }

        m_waitObjects[ NETWORK_EVENT ] = ::FindFirstChangeNotification( m_pszNetworkDir,
                                                                        FALSE,
                                                                        NOTIFY_FILTER );
        ASSERT( m_waitObjects[ NETWORK_EVENT ] );

        if ( !m_waitObjects[ NETWORK_EVENT ] )
        {
            // ERROR
            return;
        }
    }


    while (fContinue)
    {
        dwWait = ::WaitForMultipleObjects(m_dwWaitObjects, m_waitObjects,
                                        FALSE, INFINITE);
        if (WAIT_OBJECT_0 == dwWait)
        {
            fContinue = FALSE;
            ::ResetEvent(m_waitObjects[0]);
        }
#ifdef USE_CARDSERVER_API
        else if ( MEDIA_EVENT == dwWait )
        {
            // card added or removed ( from message queue )
			DWORD dwSize, dwFlags;

            if (::ReadMsgQueue(m_waitObjects[MEDIA_EVENT], pdd, sizeof(pbMsgBuf), &dwSize, 0, &dwFlags))
            {
                m_dwWaitingForCallbackId = dwWait-WAIT_OBJECT_0;
                if ( pdd->fAttached )
                {
                    // card added
                    SetNetAndCardEvents( SHCNE_MEDIAINSERTED, pdd->szName );
                    
                }
                else
                {
                    // card removed
                    SetNetAndCardEvents( SHCNE_MEDIAREMOVED, pdd->szName );
                }
                m_dwWaitingForCallbackId = -1;
            }
        }
#endif // USE_CARDSERVER_API
        else if (WAIT_OBJECT_0 + m_dwWaitObjects > dwWait)
        {
            m_dwWaitingForCallbackId = dwWait-WAIT_OBJECT_0;

#ifndef USE_CARDSERVER_API
            if ( dwWait == MEDIA_EVENT )
            {
                LPBYTE pBuffer = GetNotificationBuffer( m_dwWaitingForCallbackId );
                if ( !pBuffer )
                {
                    break;
                }

                FILE_NOTIFY_INFORMATION * pNotifyInfo = NULL;
                DWORD cbOffset = 0;
                do
                {
                    pNotifyInfo = (FILE_NOTIFY_INFORMATION *) (pBuffer+cbOffset);
                    if ( ::_tcscmp( ROOT_DIRECTORY, pNotifyInfo->FileName ) != 0 )
                    {
                        WCHAR *pwszFullPath = ConstructFullPathW( pNotifyInfo->FileName, ROOT_DIRECTORY );
                        
                        switch (pNotifyInfo->Action)
                        {
                            case FILE_ACTION_ADDED:
                                if ( IsStorageCardDirectory( pwszFullPath ) )
                                {
                                    AddStorageCardDirectoryEntry( pwszFullPath );
                                    SetNetAndCardEvents( SHCNE_MEDIAINSERTED, pwszFullPath );
                                }
                                break;

                            case FILE_ACTION_REMOVED:

                                SetNetAndCardEvents( SHCNE_MEDIAREMOVED, pwszFullPath );

                                if ( IsInStorageCardDirectoryEntries( pwszFullPath ) )
                                {
                                    RemoveStorageCardDirectoryEntry( pwszFullPath );
                                }
                                break;

                        }
                        free( pwszFullPath );
                        m_dwWaitingForCallbackId = -1;
                    }
                    else
                    {
                        // Special notification which means this directory no longer exists
                        // is this even possible with the root directory?
                        ASSERT( FALSE );
                    }

                    cbOffset += pNotifyInfo->NextEntryOffset;
                } while(pNotifyInfo->NextEntryOffset);


                if ( pBuffer )
                {
                    free( pBuffer );
                }
            } else
#endif //USE_CARDSERVER_API
            if ( m_bHaveNetworkDirectory && ( dwWait == NETWORK_EVENT ) )
            {
                // net shared or unshared
                LPBYTE pBuffer = GetNotificationBuffer( m_dwWaitingForCallbackId );
                if ( !pBuffer )
                {
                    break;
                }

                FILE_NOTIFY_INFORMATION * pNotifyInfo = NULL;
                DWORD cbOffset = 0;
                do
                {
                    pNotifyInfo = (FILE_NOTIFY_INFORMATION *) (pBuffer+cbOffset);
                    if ( ::_tcscmp( ROOT_DIRECTORY, pNotifyInfo->FileName ) != 0 )
                    {
                        WCHAR *pwszFullPath = ConstructFullPathW( pNotifyInfo->FileName, m_pszNetworkDir );
                        GetRemotePath( pwszFullPath, m_szRemoteSharePath );
                        switch (pNotifyInfo->Action)
                        {
                            case FILE_ACTION_ADDED:
                                SetNetAndCardEvents( SHCNE_NETSHARE, pwszFullPath );
                                break;

                            case FILE_ACTION_REMOVED:
                                SetNetAndCardEvents( SHCNE_NETUNSHARE, pwszFullPath );
                                break;

                        }
                        free( pwszFullPath );
                        m_dwWaitingForCallbackId = -1;
                    }
                    else
                    {
                        // Special notification which means this directory no longer exists
                        // is this even possible with the network directory?
                        ASSERT( FALSE );
                    }

                    cbOffset += pNotifyInfo->NextEntryOffset;
                } while(pNotifyInfo->NextEntryOffset);


                if ( pBuffer )
                {
                    free( pBuffer );
                }
            }
            else
            {
                // if we can't enter the critical section, it probably means that
                // we're trying to shutdown the thread, so blow off this event so
                // that we don't deadlock
                if ( TryToEnterCriticalSection() )
                {
                    // set members
                    DWORD dwEntryID = WAIT_TO_ENTRY(m_dwWaitingForCallbackId);
                    m_dwWaitingForCallbackProcessID = m_entries[ dwEntryID ].dwProcID;
                    m_bSentMessage = FALSE;
                    m_dwCurrentEvent = -1;
                    // set Change Event
                    HANDLE hEvent = NULL;
				    if ( m_entries[ dwEntryID ].pstrEventName != NULL )
				    {
					    hEvent = ::OpenEvent( EVENT_ALL_ACCESS, FALSE, m_entries[ dwEntryID ].pstrEventName );
				    }
                    BOOL bEventSet = ( hEvent && ::SetEvent( hEvent ));

                    // block until callback happens or process exits
                    DWORD dwExitCode = STILL_ACTIVE;
                    HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ dwEntryID ].dwProcID );
                    while( bEventSet && !m_bSentMessage && hProc &&
                           GetExitCodeProcess( hProc, &dwExitCode ) &&
                           ( dwExitCode == STILL_ACTIVE ) )
                    {
                    }

                    if ( !hEvent || !hProc )
                    {
                        // process is dead, clear the notification, remove the item from the watch
                        LPBYTE pBuffer = GetNotificationBuffer( m_dwWaitingForCallbackId );
                        if ( pBuffer )
                        {
                            free( pBuffer );
                        }

                        //RemoveFromList( m_dwWaitingForCallbackId );

                        fContinue = ( m_dwWaitObjects > 1 );
                    }
                    ExitTheCriticalSection();
                    CloseHandle( hProc );
                    CloseHandle( hEvent );
                }

                // done blocking
                m_dwWaitingForCallbackId = -1;
                m_dwWaitingForCallbackProcessID = -1;
                m_bSentMessage = TRUE;
            }
        }
        else
        {
            ASSERT(WAIT_OBJECT_0+m_dwWaitObjects > dwWait);
            fContinue = FALSE;
        }
    }


#ifdef USE_CARDSERVER_API
    // close the queue
    ::StopDeviceNotifications( hNotifications );
    ::CloseMsgQueue( hQueue );
    m_waitObjects[ MEDIA_EVENT ] = NULL;
#else // USE_CARDSERVER_API
    if ( m_waitObjects[ MEDIA_EVENT ] != NULL )
    {
        ::FindCloseChangeNotification( m_waitObjects[ MEDIA_EVENT ] );
        m_waitObjects[ MEDIA_EVENT ] = NULL;
    }
#endif // USE_CARDSERVER_API

    if ( m_bHaveNetworkDirectory )
    {
        if ( m_waitObjects[ NETWORK_EVENT ] != NULL )
        {
            ::FindCloseChangeNotification( m_waitObjects[ NETWORK_EVENT ] );
            m_waitObjects[ NETWORK_EVENT ] = NULL;
        }
    }
}

// remove any invalid entries from the watch
void
ChangeWatcher_t::
CleanHouse()
{
    for (DWORD i = 0; i < WAIT_TO_ENTRY( m_dwWaitObjects ); i++ )
    {
        DWORD dwEntryID = (DWORD) i;
        HANDLE hEvent = NULL;
		if ( m_entries[ dwEntryID ].pstrEventName )
		{
			hEvent = ::OpenEvent( EVENT_ALL_ACCESS, FALSE, m_entries[ dwEntryID ].pstrEventName );
		}

        DWORD dwExitCode = STILL_ACTIVE;
        HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ dwEntryID ].dwProcID );
        BOOL bActive = (hProc && GetExitCodeProcess( hProc, &dwExitCode ) && ( dwExitCode == STILL_ACTIVE ));

        if ( !bActive || !hEvent || !IsWindow( m_entries[ dwEntryID ].hwnd ) )
        {
            RemoveFromList(dwEntryID);
        }

        CloseHandle( hProc );
        CloseHandle( hEvent );
    }
}

void
ChangeWatcher_t::
BeginWatch()
{
    ASSERT(m_waitObjects[0]);

    if (!m_hThread && (WAIT_OFFSET < m_dwWaitObjects))
    {
        m_hThread = ::CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
        ASSERT(m_hThread);
    }
}


void
ChangeWatcher_t::
ShutdownWatch()
{
    ASSERT(m_waitObjects[0]);

    if (m_hThread )
    {
        DWORD dwExitCode;
        if ( GetExitCodeThread( m_hThread, &dwExitCode ) && ( dwExitCode == STILL_ACTIVE ) )
        {
            MSG msg = {0};
            ::SetEvent(m_waitObjects[0]);

            while (WAIT_OBJECT_0 != ::MsgWaitForMultipleObjects(1, &m_hThread,
                                                          FALSE, INFINITE,
                                                          QS_ALLINPUT))
            {
                if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
            }
        }

        ::CloseHandle(m_hThread);
        m_hThread = NULL;
    }

#ifndef USE_CARDSERVER_API
    CleanStorageCardDirectoryEntriesList();
#endif // USE_CARDSERVER_API
}

BOOL
ChangeWatcher_t::
AddWatch(
    HWND hwnd,
    SHCHANGENOTIFYENTRY *pshcne,
    DWORD procID,
    LPCTSTR pszEventName,
    BOOL *pbReplace
    )
{
    ASSERT(hwnd);
    ASSERT(pszEventName);
    ASSERT(pbReplace);
    ASSERT(m_bCriticalSectionInitialized);
    if ( !hwnd || !procID || !pszEventName || !pbReplace || !m_bCriticalSectionInitialized )
        return 0;

    UINT uReturn = 0;

    if (LENGTH_OF(m_waitObjects) <= m_dwWaitObjects)
        return 0;

    SHCHANGENOTIFYENTRY shcne = { 0 };
    SHCHANGENOTIFYENTRY *pUseShcne = pshcne;

    if ( !pUseShcne )
    {
        shcne.dwEventMask = SHCNE_ALLEVENTS;
        shcne.pszWatchDir = ROOT_DIRECTORY;
        shcne.fRecursive = TRUE;
        
        pUseShcne = &shcne;
    }

    if ( !pUseShcne->pszWatchDir )
    {
        shcne.dwEventMask = pUseShcne->dwEventMask;
        shcne.pszWatchDir = ROOT_DIRECTORY;
        shcne.fRecursive = pUseShcne->fRecursive;

        pUseShcne = &shcne;
    }

    // need to block on the critical section
    // if it is in use it is because we are processing
    // an event
    while( !TryToEnterCriticalSection() )
    {
        Sleep(0);
    }

    ShutdownWatch();
    
    CleanHouse();


    // see if there is already a watch for this window
    DWORD dwWatch = (DWORD) -1;
    for (DWORD i = 0; i < WAIT_TO_ENTRY(m_dwWaitObjects); i++ )
    {
        if ( hwnd == m_entries[ i ].hwnd )
        {
            dwWatch = i;
            break;
        }
    }

    if ( dwWatch != -1 )
    {
        *pbReplace = TRUE;
        RemoveFromList( dwWatch );
    }
    else
    {
        *pbReplace = FALSE;
        if ( WAIT_TO_ENTRY( m_dwWaitObjects ) + 1 >= MAX_ENTRIES )
        {
            // we're full
            return FALSE;
        }
    }


    // Move all the handles after the split point back one
    m_waitObjects[m_dwWaitObjects] = ::FindFirstChangeNotification( pUseShcne->pszWatchDir,
                                                                    //pUseShcne->fRecursive, - IN MCKENDRICK, THIS ISN'T SUPPORTED TO THE EXTENT THAT WE NEED IT TO BE
                                                                    FALSE,
                                                                    NOTIFY_FILTER );

    if ( INVALID_HANDLE_VALUE != m_waitObjects[ m_dwWaitObjects ] )
    {
        m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].hwnd = hwnd;
        m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].dwProcID = procID;
		if ( pszEventName )
		{
			if ( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName )
			{
				LocalFree( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName );
				m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName = NULL;
			}
			m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName 
								= (LPTSTR) LocalAlloc( LPTR, sizeof( TCHAR ) * ( _tcslen( pszEventName ) + 1 ) );
			if ( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName )
			{
				_tcscpy( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName, pszEventName );
			}
		}
        CopyChangeNotifyEntry( &(m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].shcne), pUseShcne );
        uReturn = (UINT) m_waitObjects[m_dwWaitObjects];
        m_dwWaitObjects++;
    }

    BeginWatch();
    ExitTheCriticalSection();

    return uReturn;
}

void
ChangeWatcher_t::
RemoveFromList(
    DWORD dwEntryIndex
    )
{
    DWORD dwWatchIndex = ENTRY_TO_WAIT(dwEntryIndex);

    if ((-1 == dwWatchIndex) || ( 0 == dwWatchIndex ) || (dwWatchIndex > MAX_WAITOBJECTS))
    {
        return;
    }

    // Free the objects
    ::FindCloseChangeNotification( m_waitObjects[dwWatchIndex] );
    m_waitObjects[dwWatchIndex] = NULL;
    if ( m_entries[dwEntryIndex].shcne.pszWatchDir )
    {
        free( m_entries[dwEntryIndex].shcne.pszWatchDir );
        m_entries[dwEntryIndex].shcne.pszWatchDir = NULL;
    }
	if ( m_entries[dwEntryIndex].pstrEventName )
	{
		LocalFree( m_entries[dwEntryIndex].pstrEventName );
		m_entries[dwEntryIndex].pstrEventName = NULL;
	}

    // Copy the exiting handles down one slot
    m_dwWaitObjects--;
      ::memmove(m_waitObjects+dwWatchIndex, m_waitObjects+dwWatchIndex+1,
               (m_dwWaitObjects-dwWatchIndex)*sizeof(*m_waitObjects));

    m_waitObjects[m_dwWaitObjects] = NULL;
    
    if ( m_dwWaitObjects > WAIT_OFFSET )
    {
        for( DWORD i = dwEntryIndex; i <  WAIT_TO_ENTRY( m_dwWaitObjects ); i++ )
        {
            CopyChangeNotifyEntry( &m_entries[ i ].shcne, &m_entries[ i + 1 ].shcne );
            m_entries[ i ].hwnd = m_entries[ i + 1 ].hwnd;
            m_entries[ i ].dwProcID = m_entries[ i + 1 ].dwProcID;
			m_entries[ i ].pstrEventName = m_entries[ i + 1 ].pstrEventName;
			m_entries[ i + 1 ].pstrEventName = NULL;
        }
    }

    m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].hwnd = NULL;
    m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].dwProcID = 0;

    if ( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].shcne.pszWatchDir )
    {
        free( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].shcne.pszWatchDir );
        m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].shcne.pszWatchDir = NULL;
    }

    if ( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName )
    {
        LocalFree( m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName );
        m_entries[ WAIT_TO_ENTRY( m_dwWaitObjects ) ].pstrEventName = NULL;
    }


}

BOOL
ChangeWatcher_t::
RemoveWatch(
    HWND hwnd
    )
{
    if ( !hwnd || !m_bCriticalSectionInitialized )
    {
        return FALSE;
    }

    // need to block on the critical section
    while( !TryToEnterCriticalSection() )
    {
        Sleep(0);
    }
    
    ShutdownWatch();

    DWORD dwEntry = (DWORD) -1;
    
    for (DWORD i = 0; i < WAIT_TO_ENTRY(m_dwWaitObjects); i++ )
    {
        if ( hwnd == m_entries[ i ].hwnd )
        {
            dwEntry = i;
            break;
        }
    }

    if ( dwEntry != -1 )
    {
        RemoveFromList( dwEntry );
    }

    BeginWatch();
    ExitTheCriticalSection();

    return (-1 != dwEntry);
}

/*
 * Local Variables
 */

ChangeWatcher_t *f_pWatcher = NULL;

/*
 * External-available calls for explorer.exe only
 */

BOOL
InitializeChangeWatcher()
{
    ASSERT( !f_pWatcher );
    f_pWatcher = new ChangeWatcher_t();

    return (f_pWatcher != NULL);
}

BOOL
ReleaseChangeWatcher()
{
    if ( f_pWatcher )
    {
        delete f_pWatcher;
        f_pWatcher = NULL;
    }

    return TRUE;
}


/*
 * PSL Call Implementations
 */

extern "C"
BOOL
SHChangeNotifyRegisterII(
    HWND hwnd,
    LPCTSTR pszWatch,
    SHCHANGENOTIFYENTRY *pshcne,
    DWORD procID,
    LPCTSTR pszEventName,
    BOOL *pReplace
    )
{
    __try
    {
        if ( f_pWatcher )
        {
            // fix up the pointers
		    if ( pshcne )
		    {
			    pshcne->pszWatchDir = (LPTSTR) pszWatch;
		    }
            return f_pWatcher->AddWatch( hwnd, pshcne, procID, pszEventName, pReplace );
        }
        return FALSE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
        return FALSE;
    }
}

extern "C"
BOOL
SHFileNotifyRemoveII(
    HWND hwnd
    )
{
    __try
    {
        if ( f_pWatcher )
        {
            return f_pWatcher->RemoveWatch( hwnd );
        }

        return FALSE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
        return FALSE;
    }
}

extern "C"
void
SHFileNotifyFreeII(
    FILECHANGENOTIFY *pfcn,
    HPROCESS proc
    )
{
    __try
    {
        if ( !pfcn )
        {
            return;
        }
        
        pfcn->dwRefCount--;

        if ( pfcn->dwRefCount < 1 )
        {
            if ( ( pfcn->fci.dwItem1 ) && ( pfcn->fci.uFlags & SHCNF_PATH ) )
            {
                if( proc )
                {
                    LocalFreeInProcess((HLOCAL)(void *) pfcn->fci.dwItem1, proc);
                }
                else
                {
                    free( (void *) pfcn->fci.dwItem1 );
                }
            }

            if ( ( ( pfcn->fci.wEventId == SHCNE_RENAMEITEM ) || ( pfcn->fci.wEventId == SHCNE_RENAMEFOLDER ) ) && ( pfcn->fci.dwItem2 ) )
            {
                if ( proc )
                {
                    LocalFreeInProcess((HLOCAL)(void *) pfcn->fci.dwItem2, proc);
                }
                else
                {
                    free( (void *) pfcn->fci.dwItem2 );
                }
            }

            if( proc )
            {
                LocalFreeInProcess((HLOCAL) pfcn, proc);
            }
            else
            {
                free( pfcn );
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
    }
}

extern "C"
BOOL
SendChangeNotificationToWindowI(
    HPROCESS proc
    )
{
    __try
    {
        if ( f_pWatcher )
        {
            return f_pWatcher->SendMessage( proc );
        }

        return FALSE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
        return FALSE;
    }
}
