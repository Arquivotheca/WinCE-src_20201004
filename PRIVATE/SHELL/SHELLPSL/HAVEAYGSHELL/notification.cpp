//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <aygshell.h>
#include <shlwapi.h>
#include <dbt.h>
#include <Msgqueue.h>
#include <Pnp.h>
#include <Cardserv.h>

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

/*
 * local defines
 */

#define MEDIA_EVENT   WAIT_OBJECT_0+1
#define NETWORK_EVENT MEDIA_EVENT+1
#define WAITOFFSET (DWORD) (m_bHaveNetworkDirectory ? 3 : 2)
#define WAITTOENTRY( dwWait ) (dwWait - WAITOFFSET)
#define ENTRYTOWAIT( dwEntry ) (dwEntry + WAITOFFSET)
#define GETEVENTMASK( dwWait ) m_entries[ WAITTOENTRY( dwWait ) ].shcne.dwEventMask
#define NEEDS_EVENT( dwWait, dwEvent ) ( ( GETEVENTMASK( dwWait ) & dwEvent ) || !dwEvent )
#define lengthof(x) ( (sizeof((x))) / (sizeof(*(x))) )

#define MAX_WAITOBJECTS     32
#define MAX_ENTRIES         29
#define NOTIFY_BUFFER_MAX   0x0FFF

// stolen from extfile.h
#define AFS_ROOTNUM_STORECARD 0
#define AFS_ROOTNUM_NETWORK 1
#define SYSTEM_MNTVOLUME		0xe
#define OIDFROMAFS(iAFS)        ((CEOID)((SYSTEM_MNTVOLUME<<28)|((iAFS)&0xfffffff)))

/*
 * Convenience Functions
 */

TCHAR * W2T( WCHAR *pwszIn, HPROCESS proc = NULL )
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
    if ( wcscmp( pwszDir + ( wcslen( pwszDir ) - 1 ), L"//" ) != 0 )
    {
        pwszPath = (WCHAR *) malloc( sizeof( WCHAR ) * ( wcslen( pwszDir ) + wcslen( pwszFile ) + 2 ) );
        if ( !pwszPath )
        {
            goto exit;
        }
        wcscpy( pwszPath, pwszDir );
        wcscat( pwszPath, L"\\" );
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

BOOL GetRemotePath( WCHAR *pwszLocalPath, WCHAR *pwszRemotePath )
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
    wcsncat( pwszRemotePath, shfi.szDisplayName, (pos - shfi.szDisplayName) / sizeof( WCHAR ) );
    wcscat( pwszRemotePath, L"\\" );
    wcscat( pwszRemotePath, pos+4 );

    return TRUE;
}

FILECHANGENOTIFY * CreateFileChangeNotify( LONG wEventId, WCHAR *pwszFilename, NOTIFYENTRY entry )
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
            WCHAR *pwszPath = ConstructFullPathW( (TCHAR *) pNotify->fci.dwItem1, entry.shcne.pszWatchDir );
            WIN32_FIND_DATA wfd;
            if ( pwszPath && ( FindFirstFile( pwszPath, &wfd ) != INVALID_HANDLE_VALUE ) )
            {
                pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                pNotify->fci.ftModified = wfd.ftLastWriteTime;
                pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
            }
            if ( pwszPath )
            {
                free( pwszPath );
            }
        }
        pNotify->fci.dwItem2 = 0;
    }

    CloseHandle( hProc );
    return pNotify;
}

void CopyChangeNotifyEntry( SHCHANGENOTIFYENTRY *pDest, SHCHANGENOTIFYENTRY *pSrc )
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

class ChangeWatcher
{
protected:
    static const DWORD  NOTIFY_FILTER;
    HANDLE              m_hThread;
    CRITICAL_SECTION    m_cs;
    DWORD               m_dwWaitObjects;
    HANDLE              m_waitObjects[ MAX_WAITOBJECTS ];
    NOTIFYENTRY         m_entries[ MAX_ENTRIES ];
    TCHAR               m_pszNetworkDir[ MAX_PATH ];

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

    DWORD       m_dwWaitingForCallbackId;
    DWORD       m_dwWaitingForCallbackProcessID;
    WCHAR       m_szLocalPath[MAX_PATH];
    WCHAR       m_szRemoteSharePath[MAX_PATH];
    DWORD       m_dwCurrentEvent;
    BOOL        m_bSentMessage;

    BOOL        m_bHaveNetworkDirectory;

public:
    ChangeWatcher();
    ~ChangeWatcher();

    BOOL AddWatch( HWND hwnd, SHCHANGENOTIFYENTRY *pschne, DWORD dwProcID, LPCTSTR pszEventName, BOOL *pbReplace );
    BOOL RemoveWatch( HWND hwnd );
    BOOL SendMessage( HPROCESS proc );
};

/*
 * ChangeWatcher Class Methods
 */

const DWORD ChangeWatcher::NOTIFY_FILTER =  FILE_NOTIFY_CHANGE_FILE_NAME |
                                            FILE_NOTIFY_CHANGE_DIR_NAME |
                                            FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                            FILE_NOTIFY_CHANGE_SIZE |
                                            FILE_NOTIFY_CHANGE_LAST_WRITE |
                                            FILE_NOTIFY_CHANGE_LAST_ACCESS |
                                            FILE_NOTIFY_CHANGE_CREATION |
                                            FILE_NOTIFY_CHANGE_SECURITY |
                                            FILE_NOTIFY_CHANGE_CEGETINFO;


ChangeWatcher::ChangeWatcher() :
    m_hThread(NULL),
    m_dwWaitObjects(0)
{
    ::InitializeCriticalSection( &m_cs );
    
    for ( int i = 0; i < MAX_WAITOBJECTS; i++ )
    {
        m_waitObjects[ i ] = NULL;
    }

    m_waitObjects[ m_dwWaitObjects++ ] = ::CreateEvent( NULL, TRUE, FALSE, NULL );
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


ChangeWatcher::~ChangeWatcher()
{
    if (m_hThread)
       ShutdownWatch();

    if (m_waitObjects[0])
       ::CloseHandle(m_waitObjects[0]);
 
    // starts either at the first file notification
    // or the network directory watcher
    for ( DWORD i = 2; i < MAX_WAITOBJECTS; i++ )
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

    ::DeleteCriticalSection(&m_cs);
}

DWORD WINAPI ChangeWatcher::ThreadProc(LPVOID lpParameter)
{
    ChangeWatcher * pChangeWatcher = (ChangeWatcher *) lpParameter;
    pChangeWatcher->ProcessEvents();
    return 0;
}

LPBYTE ChangeWatcher::GetNotificationBuffer( UINT dwWaitIndex )
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


void ChangeWatcher::DoPostMessage( DWORD dwWaitIndex, DWORD dwEventID, LPTSTR pszFileName )
{
    if ( NEEDS_EVENT( dwWaitIndex, dwEventID ) ) 
    {
        FILECHANGENOTIFY *pNotify = CreateFileChangeNotify( dwEventID, pszFileName, m_entries[ WAITTOENTRY( dwWaitIndex ) ] );
        // using Post will avoid Deadlock, but might offer unexpected results
        if ( pNotify )
        {
            ::PostMessage( m_entries[ WAITTOENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );
        }
    }
}

void ChangeWatcher::OnChange(UINT dwWaitIndex)
{
    LPBYTE pBuffer = NULL;

    pBuffer = GetNotificationBuffer( dwWaitIndex );
    if ( !pBuffer )
    {
        return;
    }

    if ( !IsWindow( m_entries[ WAITTOENTRY( dwWaitIndex ) ].hwnd ) )
    {
        // should be in a critical section here in the main loop
//        RemoveFromList( dwWaitIndex );
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
            if ( ::_tcscmp( _T( "\\" ), pNotifyInfo->FileName ) != 0 )
            {
                WCHAR *pwszFullPath = ConstructFullPathW( pNotifyInfo->FileName, m_entries[ WAITTOENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
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
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEDIR, m_entries[ WAITTOENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
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
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEDIR, m_entries[ WAITTOENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
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

                        WCHAR *pwszNewFullPath = ConstructFullPathW( pNotifyInfo->FileName, m_entries[ WAITTOENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                        if ( !PathIsDirectory( pwszNewFullPath ) )
                        {
                            if ( NEEDS_EVENT( dwWaitIndex, SHCNE_RENAMEITEM ) )
                            {
                                FILECHANGENOTIFY *pNotify = CreateFileChangeNotify( SHCNE_RENAMEITEM, szOldName, m_entries[ WAITTOENTRY( dwWaitIndex ) ] );

                                if ( pNotify )
                                {
                                    HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ WAITTOENTRY( dwWaitIndex ) ].dwProcID );
                                    if ( hProc )
                                    {
                                        pNotify->fci.dwItem2 = (DWORD) W2T( pwszNewFullPath, hProc );
                                        WIN32_FIND_DATA wfd;
                                        if ( pwszNewFullPath && ( FindFirstFile( pwszNewFullPath, &wfd ) != INVALID_HANDLE_VALUE ) )
                                        {
                                            pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                                            pNotify->fci.ftModified = wfd.ftLastWriteTime;
                                            pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
                                        }
    
                                        ::PostMessage( m_entries[ WAITTOENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );

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
                                FILECHANGENOTIFY *pNotify = CreateFileChangeNotify( SHCNE_RENAMEFOLDER, szOldName, m_entries[ WAITTOENTRY( dwWaitIndex ) ] );

                                if ( pNotify )
                                {
                                    HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ WAITTOENTRY( dwWaitIndex ) ].dwProcID );
                                    if ( hProc )
                                    {
                                        pNotify->fci.dwItem2 = (DWORD) W2T( pwszNewFullPath, hProc );
                                        WIN32_FIND_DATA wfd;
                                        if ( pwszNewFullPath && ( FindFirstFile( pwszNewFullPath, &wfd ) != INVALID_HANDLE_VALUE ) )
                                        {
                                            pNotify->fci.dwAttributes = wfd.dwFileAttributes;
                                            pNotify->fci.ftModified = wfd.ftLastWriteTime;
                                            pNotify->fci.nFileSize = (wfd.nFileSizeHigh * MAXDWORD) + wfd.nFileSizeLow;
                                        }
    
                                        ::PostMessage( m_entries[ WAITTOENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );

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
                        DoPostMessage( dwWaitIndex, SHCNE_UPDATEDIR, m_entries[ WAITTOENTRY( dwWaitIndex ) ].shcne.pszWatchDir );
                        break;
                    }
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
            RETAILMSG( 1, (TEXT("ChangeWatcher::OnChange - Got a FILE_NOTIFICATION_INFORMATION structure with a zero file name length, don't know how to handle.  Ignoring\r\n")));
        }

        cbOffset += pNotifyInfo->NextEntryOffset;
    } while(pNotifyInfo->NextEntryOffset);

    free(pBuffer);
}

void ChangeWatcher::OnDeviceChange( UINT dwWaitIndex )
{
    if ( !IsWindow( m_entries[ WAITTOENTRY( dwWaitIndex ) ].hwnd ) )
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
            pNotify = CreateFileChangeNotify( m_dwCurrentEvent, m_szLocalPath, m_entries[ WAITTOENTRY( dwWaitIndex ) ] );

            if ( pNotify )
            {
                HPROCESS hProc = ::OpenProcess( 0, FALSE, m_entries[ WAITTOENTRY( dwWaitIndex ) ].dwProcID );
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

                    ::PostMessage( m_entries[ WAITTOENTRY( dwWaitIndex ) ].hwnd, WM_FILECHANGEINFO, 0, (LPARAM) pNotify );

                    ::CloseHandle( hProc );
                }
            }
            break;
        default:
            ASSERT( FALSE );
            break;
    }
}

BOOL ChangeWatcher::SendMessage( HPROCESS hProc )
{
    if ( ( (DWORD) hProc != m_dwWaitingForCallbackProcessID ) || ( m_dwWaitingForCallbackId == -1 ) || ( m_bSentMessage ) )
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

DWORD ChangeWatcher::FindNextInterestedEntry( DWORD dwStartIndex, DWORD dwEventId )
{
    for ( DWORD i = dwStartIndex; i < WAITTOENTRY( m_dwWaitObjects ); i++ )
    {
        if ( m_entries[ i ].shcne.dwEventMask & dwEventId )
        {
            return i;
        }
    }

    return -1;
}

void ChangeWatcher::SetNetAndCardEvents( DWORD dwEventID, WCHAR *pwszFullPath )
{
    if ( !PathIsDirectory( pwszFullPath ) )
    {
        return;
    }

    // new share
    DWORD dwEntryID = FindNextInterestedEntry( 0, dwEventID );
    
    if ( dwEntryID == -1 )
    {
        return;
    }

    ::EnterCriticalSection( &m_cs );
    
    // store the local path
    wcscpy( m_szLocalPath, pwszFullPath );
    m_dwCurrentEvent = dwEventID;

    while( dwEntryID != -1 )
    {
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
        m_dwWaitingForCallbackProcessID = -1;
        m_bSentMessage = TRUE;

        CloseHandle( hProc );
        CloseHandle( hEvent );

        // send message and block
        dwEntryID = FindNextInterestedEntry( dwEntryID + 1, dwEventID );
    }
    ::LeaveCriticalSection( &m_cs );
}

void ChangeWatcher::ProcessEvents()
{
    BOOL fContinue = TRUE;
    DWORD dwWait;

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
                                                                        FILE_NOTIFY_CHANGE_CREATION );
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
        else if (WAIT_OBJECT_0 + m_dwWaitObjects > dwWait)
        {
            m_dwWaitingForCallbackId = dwWait-WAIT_OBJECT_0;

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
                    if ( ::_tcscmp( _T( "\\" ), pNotifyInfo->FileName ) != 0 )
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
                ::EnterCriticalSection(&m_cs);
                // set members
                DWORD dwEntryID = WAITTOENTRY(m_dwWaitingForCallbackId);
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
                ::LeaveCriticalSection(&m_cs);

                // done blocking
                m_dwWaitingForCallbackId = -1;
                m_dwWaitingForCallbackProcessID = -1;
                m_bSentMessage = TRUE;

                CloseHandle( hProc );
                CloseHandle( hEvent );
            }
        }
        else
        {
            ASSERT(WAIT_OBJECT_0+m_dwWaitObjects > dwWait);
            fContinue = FALSE;
        }
    }

    // close the queue
    ::StopDeviceNotifications( hNotifications );
    ::CloseMsgQueue( hQueue );
    m_waitObjects[ MEDIA_EVENT ] = NULL;

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
void ChangeWatcher::CleanHouse()
{
    for ( int i = (int) WAITTOENTRY( m_dwWaitObjects ); i >= 0; i-- )
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
            RemoveFromList( dwEntryID );
        }

        CloseHandle( hProc );
        CloseHandle( hEvent );
    }
}

void ChangeWatcher::BeginWatch()
{
    ASSERT(m_waitObjects[0]);

    if (!m_hThread && (WAITOFFSET < m_dwWaitObjects))
    {
        m_hThread = ::CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
        ASSERT(m_hThread);
    }
}


void ChangeWatcher::ShutdownWatch()
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
}

BOOL ChangeWatcher::AddWatch( HWND hwnd, SHCHANGENOTIFYENTRY *pshcne, DWORD procID, LPCTSTR pszEventName, BOOL *pbReplace )
{
    ASSERT(hwnd);
    ASSERT(pszEventName);
    ASSERT(pbReplace);
    if ( !hwnd || !procID || !pszEventName || !pbReplace )
        return 0;

    UINT uReturn = 0;

    if (lengthof(m_waitObjects) <= m_dwWaitObjects)
        return 0;

    SHCHANGENOTIFYENTRY shcne = { 0 };
    SHCHANGENOTIFYENTRY *pUseShcne = pshcne;

    if ( !pUseShcne )
    {
        shcne.dwEventMask = SHCNE_ALLEVENTS;
        shcne.pszWatchDir = TEXT("\\");
        shcne.fRecursive = TRUE;
        
        pUseShcne = &shcne;
    }

    ::EnterCriticalSection(&m_cs);
    ShutdownWatch();
    
    CleanHouse();


    // see if there is already a watch for this window
    DWORD dwWatch = (DWORD) -1;
    for (DWORD i = 0; i < WAITTOENTRY(m_dwWaitObjects); i++ )
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
        if ( WAITTOENTRY( m_dwWaitObjects ) + 1 >= MAX_ENTRIES )
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
        m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].hwnd = hwnd;
        m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].dwProcID = procID;
		if ( pszEventName )
		{
			if ( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName )
			{
				LocalFree( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName );
				m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName = NULL;
			}
			m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName 
								= (LPTSTR) LocalAlloc( LPTR, sizeof( TCHAR ) * ( _tcslen( pszEventName ) + 1 ) );
			if ( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName )
			{
				_tcscpy( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName, pszEventName );
			}
		}
        CopyChangeNotifyEntry( &(m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].shcne), pUseShcne );
        uReturn = (UINT) m_waitObjects[m_dwWaitObjects];
        m_dwWaitObjects++;
    }

    BeginWatch();
    ::LeaveCriticalSection(&m_cs);

    return uReturn;
}

void ChangeWatcher::RemoveFromList( DWORD dwWatchIndex )
{
    if ( (-1 == dwWatchIndex) || ( 0 == dwWatchIndex ) )
    {
        return;
    }

    // Free the objects
    ::FindCloseChangeNotification( m_waitObjects[dwWatchIndex] );
    m_waitObjects[dwWatchIndex] = NULL;
    if ( m_entries[ WAITTOENTRY( dwWatchIndex ) ].shcne.pszWatchDir )
    {
        free( m_entries[ WAITTOENTRY( dwWatchIndex ) ].shcne.pszWatchDir );
        m_entries[ WAITTOENTRY( dwWatchIndex ) ].shcne.pszWatchDir = NULL;
    }
	if ( m_entries[ WAITTOENTRY( dwWatchIndex ) ].pstrEventName )
	{
		LocalFree( m_entries[ WAITTOENTRY( dwWatchIndex ) ].pstrEventName );
		m_entries[ WAITTOENTRY( dwWatchIndex ) ].pstrEventName = NULL;
	}

    // Copy the exiting handles down one slot
    m_dwWaitObjects--;
      ::memcpy(m_waitObjects+dwWatchIndex, m_waitObjects+dwWatchIndex+1,
               (m_dwWaitObjects-dwWatchIndex)*sizeof(*m_waitObjects));
    
    if ( m_dwWaitObjects > WAITOFFSET )
    {
        for( DWORD i = WAITTOENTRY(dwWatchIndex); i <  WAITTOENTRY( m_dwWaitObjects ) - 1; i++ )
        {
            CopyChangeNotifyEntry( &m_entries[ i ].shcne, &m_entries[ i + 1 ].shcne );
            m_entries[ i ].hwnd = m_entries[ i + 1 ].hwnd;
            m_entries[ i ].dwProcID = m_entries[ i + 1 ].dwProcID;
			m_entries[ i ].pstrEventName = m_entries[ i + 1 ].pstrEventName;
			m_entries[ i + 1 ].pstrEventName = NULL;
        }
    }

    m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].hwnd = NULL;
    m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].dwProcID = 0;

    if ( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].shcne.pszWatchDir )
    {
        free( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].shcne.pszWatchDir );
        m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].shcne.pszWatchDir = NULL;
    }

    if ( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName )
    {
        LocalFree( m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName );
        m_entries[ WAITTOENTRY( m_dwWaitObjects ) ].pstrEventName = NULL;
    }
}

BOOL ChangeWatcher::RemoveWatch( HWND hwnd )
{
    if ( !hwnd )
    {
        return FALSE;
    }

    ::EnterCriticalSection(&m_cs);
    ShutdownWatch();

    DWORD dwEntry = (DWORD) -1;
    
    for (DWORD i = 0; i < WAITTOENTRY(m_dwWaitObjects); i++ )
    {
        if ( hwnd == m_entries[ i ].hwnd )
        {
            dwEntry = i;
            break;
        }
    }

    RemoveFromList( ENTRYTOWAIT( dwEntry ) );

    BeginWatch();
    ::LeaveCriticalSection(&m_cs);

    return (-1 != dwEntry);
}

/*
 * Local Variables
 */

ChangeWatcher *g_pWatcher = NULL;

/*
 * External-available calls for explorer.exe only
 */

BOOL InitializeChangeWatcher()
{
    g_pWatcher = new ChangeWatcher();

    return (g_pWatcher != NULL);
}

BOOL ReleaseChangeWatcher()
{
    if ( g_pWatcher )
    {
        delete g_pWatcher;
        g_pWatcher = NULL;
    }

    return TRUE;
}


/*
 * PSL Call Implementations
 */

extern "C" BOOL SHChangeNotifyRegisterII( HWND hwnd, LPCTSTR pszWatch, SHCHANGENOTIFYENTRY *pshcne, DWORD procID, LPCTSTR pszEventName, BOOL *pReplace )
{
    if ( g_pWatcher )
    {
        // fix up the pointers
		if ( pshcne )
		{
			pshcne->pszWatchDir = (LPTSTR) pszWatch;
		}
        return g_pWatcher->AddWatch( hwnd, pshcne, procID, pszEventName, pReplace );
    }
    return FALSE;
}

extern "C" BOOL SHFileNotifyRemoveII( HWND hwnd )
{
    if ( g_pWatcher )
    {
        return g_pWatcher->RemoveWatch( hwnd );
    }

    return FALSE;
}

extern "C" void SHFileNotifyFreeII( FILECHANGENOTIFY *pfcn, HPROCESS proc )
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

extern "C" BOOL SendChangeNotificationToWindowI( HPROCESS proc )
{
    if ( g_pWatcher )
    {
        return g_pWatcher->SendMessage( proc );
    }

    return FALSE;
}
