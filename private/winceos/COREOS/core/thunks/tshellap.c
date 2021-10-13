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
#include <commdlg.h>
#include <shsdkstc.h>

/***************************
The current Gryphon PSL table is
const PFNVOID VTable[NUM_APIS] = {
    ShellNotifyCallback,            -- n/A
    ShellApiReservedForNK,          -- n/a
    XShellExecuteEx,                -- now internal to COREDLL (shexec component)
    XGetOpenFileName,               -- now internal to COREDLL (fileopen component)
    XGetSaveFileName,               -- now internal to COREDLL (fileopen component)
    XSHGetFileInfo,                 -- now internal to COREDLL (fileinfo component)
    Shell_NotifyIcon,               -- THUNK to SH_SHELL provided here (shellapis component)
    SHAddToRecentDocs (MERC ONLY)   -- THUNK to SH_SHELL provided here (shellapis component)
    SHCreateShortcut,               -- now internal to COREDLL (shortcut component)
    SHCreateExplorerInst(MERC ONLY) -- THUNK to SH_SHELL provided here (shellapis component)
    CeSetUserNotification,          -- now in its own driver
    CeClearUserNotification,        -- now in its own driver
    CeRunAppAtTime,                 -- now in its own driver
    CeRunAppAtEvent,                -- now in its own driver
    CeHandleAppNotifications,       -- now in its own driver
    CeGetUserNotificationPreferences-- now in its own driver
    CeEventHasOccurred,             -- now in its own driver
    PlaceHolder1,                   -- n/a
    SHGetShortcutTarget,            -- now internal to COREDLL (shortcut component)
    SHShowOutOfMemory,              -- now internal to COREDLL (shmisc component)
    ExitWindowsEx,                  -- now internal to COREDLL (NOP stub in shmisc component)
    XSHLoadDIBitmap,                -- now internal to COREDLL (shmisc component)
    SHSetDesktopPosition (OBSOLETE) -- n/a
    SHFileChange (internal to shell)-- n/a

----- Mercury table ends here -------------
    
    SHUnsupportedFunc,              -- n/a
    SHUnsupportedFunc,              -- n/a  
    SHUnsupportedFunc,              -- n/a  
    NotSystemParametersInfo,        -- Thunked in AYGSHELL.DLL
    SHGetAppKeyAssoc,               -- Thunked in AYGSHELL.DLL
    SHSetAppKeyWndAssoc,            -- Thunked in AYGSHELL.DLL
    PlaceHolder,                    -- n/a
    PlaceHolder,                    -- n/a
    PlaceHolder,                    -- n/a
    PlaceHolder,                    -- n/a
    PlaceHolder,                    -- n/a
    PlaceHolder,                    -- n/a
    PlaceHolder,                    -- n/a
    SHCloseAppsI,                   -- Thunked in AYGSHELL.DLL
    SHSipPreference,                -- Thunked in AYGSHELL.DLL
    PlaceHolder, //39
    PlaceHolder, //40
    SHSetNavBarText,                -- Thunked in AYGSHELL.DLL
    SHDoneButton,                   -- Thunked in AYGSHELL.DLL
    PlaceHolder, //43
    PlaceHolder, //44
    PlaceHolder, //45
    PlaceHolder, //46
    PlaceHolder, //47
    PlaceHolder, //48
    PlaceHolder, //49
    PlaceHolder, //50
    PlaceHolder, //51
    PlaceHolder, //52
    PlaceHolder, //53
    PlaceHolder, //54
    SHNotificationAdd,              -- Thunked in AYGSHELL.DLL
    SHNotificationUpdate,           -- Thunked in AYGSHELL.DLL
    SHNotificationRemove,           -- Thunked in AYGSHELL.DLL
    SHNotificationGetData, //58     -- Thunked in AYGSHELL.DLL

};
***************************/


// SDK exports
BOOL WINAPI xxx_Shell_NotifyIcon(DWORD dwMessage, PNOTIFYICONDATA lpData) {
    if(WAIT_OBJECT_0 == WaitForAPIReady(SH_SHELL, 0)) {
        return Shell_NotifyIcon(dwMessage,lpData, sizeof(NOTIFYICONDATA));
    } else {
        return FALSE;
    }
}


DWORD WINAPI xxx_SHCreateExplorerInstance(LPCTSTR pszPath, UINT uFlags) {
    if(WAIT_OBJECT_0 == WaitForAPIReady(SH_SHELL, 0)) {
        return SHCreateExplorerInstance(pszPath, uFlags);
    } else {
        return 0;
    }
}

/***
// SDK exports for commdlg
BOOL APIENTRY xxx_GetOpenFileNameW(LPOPENFILENAMEW lpofn) {
    return GetOpenFileNameW(lpofn);
}

BOOL APIENTRY xxx_GetSaveFileNameW(LPOPENFILENAMEW lpofn) {
    return GetSaveFileNameW(lpofn);
}
***/

BOOL xxx_SHDoneButton(HWND hwndRequester, DWORD dwState)
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHDoneButton_Trap( hwndRequester, dwState );
    }
    else
    {
        return FALSE;
    }
}

BYTE xxx_SHGetAppKeyAssoc(LPCTSTR ptszApp)
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHGetAppKeyAssoc_Trap( ptszApp );
    }
    else
    {
        return 0;
    }
}

BOOL xxx_SHSetAppKeyWndAssoc(BYTE bVk, HWND hwnd)
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHSetAppKeyWndAssoc_Trap( bVk, hwnd );
    }
    else
    {
        return FALSE;
    }
}

BOOL WINAPI xxx_SHSetNavBarText( HWND hwnd, LPCTSTR pszText )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHSetNavBarText_Trap( hwnd, pszText );
    }
    else
    {
        return FALSE;
    }
}

BOOL WINAPI xxx_NotSystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return NotSystemParametersInfo_Trap( uiAction, uiParam, pvParam, fWinIni );
    }
    else
    {
        return FALSE;
    }
}

BOOL WINAPI xxx_SHCloseApps( DWORD dwMemSought )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHCloseApps_Trap( dwMemSought );
    }
    else
    {
        return FALSE;
    }
}


LRESULT
WINAPI
xxx_SHNotificationAdd( SHNOTIFICATIONDATA *pndAdd )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHNotificationAdd_Trap( pndAdd, sizeof(SHNOTIFICATIONDATA), (LPTSTR) pndAdd->pszTitle, (LPTSTR) pndAdd->pszHTML );
    }
    else
    {
        return E_FAIL;
    }
}

LRESULT 
WINAPI 
xxx_SHNotificationUpdate(
    DWORD grnumUpdateMask,
    SHNOTIFICATIONDATA *pndNew
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHNotificationUpdate_Trap( grnumUpdateMask, pndNew, sizeof(SHNOTIFICATIONDATA), (LPTSTR) pndNew->pszTitle, (LPTSTR) pndNew->pszHTML );
    }
    else
    {
        return E_FAIL;
    }
}

LRESULT
WINAPI
xxx_SHNotificationRemove(
    const CLSID *pclsid,
    DWORD dwID
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) )
    {
        return SHNotificationRemove_Trap( pclsid, sizeof(CLSID), dwID );
    }
    else
    {
        return E_FAIL;
    }
}

LRESULT
WINAPI
xxx_SHNotificationGetData(
    const CLSID *pclsid,
    DWORD dwID,
    SHNOTIFICATIONDATA *pndBuffer
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady( SH_SHELL, 0 ) && pndBuffer && pclsid )
    {
        // get the strings separately (they don't handle the transition)
        DWORD dwSize;
        DWORD cbHTML;
        DWORD cbTitle;
        TCHAR *pszHTML;
        TCHAR *pszTitle;
        LRESULT lr;
        cbTitle = sizeof( TCHAR ) * MAX_PATH;
        pszTitle = (LPTSTR) LocalAlloc( LMEM_FIXED, cbTitle);
        if ( pszTitle )
        {
            pszTitle[0] = TEXT('\0');
            lr = SHNotificationGetData_Trap( pclsid, sizeof(CLSID), dwID, pndBuffer, sizeof(SHNOTIFICATIONDATA), pszTitle, cbTitle, NULL, 0, &dwSize );
            if ( lr == ERROR_SUCCESS )
            {
                if ( _tcslen( pszTitle ) == 0 )
                {
                    pndBuffer->pszTitle = NULL;
                    LocalFree( pszTitle );
                }
                else
                {
                    pndBuffer->pszTitle = pszTitle;
                }

                if ( dwSize > 0 )
                {
                    cbHTML = (dwSize + 1) * sizeof(TCHAR);
                    pszHTML = (LPTSTR) LocalAlloc( LMEM_FIXED, cbHTML);
                    if (pszHTML)
                    {   
                        pszHTML[0] = TEXT('\0');
                    }   
                    lr = SHNotificationGetData_Trap( pclsid, sizeof(CLSID), dwID, NULL, 0, NULL, 0, pszHTML, cbHTML, &dwSize );
                    pndBuffer->pszHTML = pszHTML;
                }
                else
                {
                    pndBuffer->pszHTML = NULL;
                }
            }
            return lr;
        }
    }

    return E_FAIL;
}
