//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <commctrl.h>
#include <aygshell.h>

// from mysip.cpp
BOOL pfnSipShowIM( DWORD dwFlag );
BOOL pfnSipGetCurrentIM( CLSID* pClsid );
BOOL pfnSipSetCurrentIM( CLSID* pClsid );
BOOL pfnSipGetInfo( SIPINFO *pSipInfo );
BOOL pfnSipSetInfo( SIPINFO *pSipInfo );

/*------------------------------------------------------------------------------
    Send Bubble Notification Message to App
------------------------------------------------------------------------------*/
LRESULT SendBubbleNotificationMessage( HWND hwnd, NMSHN *pNMSHN, TCHAR *pszTarget )
{
    DWORD dwProcessID;
    HPROCESS hProcess;
    LRESULT lRes = 0;

    ASSERT( hwnd );
    if ( !hwnd || !IsWindow( hwnd ) )
    {
        return lRes;
    }

    GetWindowThreadProcessId(hwnd, &dwProcessID);
    hProcess = OpenProcess(0, FALSE, dwProcessID);
    if (NULL != hProcess)
    {
        DWORD dwOldPerms = SetProcPermissions((DWORD)-1);
        __try
        {
            DWORD dwStrMem = 0;
            NMSHN *pnmshn = NULL;

            if ( pszTarget )
            {
                dwStrMem = sizeof(TCHAR) * ( _tcslen( pszTarget ) + 1 );
            }
            pnmshn = (NMSHN *)LocalAllocInProcess(0, sizeof(NMSHN) + dwStrMem, hProcess);

            if (NULL != pnmshn)
            {
                if ( pszTarget )
                {
                    LPTSTR pszMapped = NULL;
                    pszMapped = (LPTSTR)(pnmshn + 1);
                    _tcscpy(pszMapped, pszTarget);
                    pnmshn->hdr.hwndFrom = pNMSHN->hdr.hwndFrom;
                    pnmshn->hdr.idFrom = pNMSHN->hdr.idFrom;
                    pnmshn->hdr.code = pNMSHN->hdr.code;
                    if ( pszMapped )
                    {
                        pnmshn->pszLink = pszMapped;
                    }
                    pnmshn->lParam = pNMSHN->lParam;
                    pnmshn->dwReturn = pNMSHN->dwReturn;
                }
                else
                {
                    *pnmshn = *pNMSHN;
                }

                lRes = SendMessage( hwnd, WM_NOTIFY, (WPARAM)pNMSHN->hdr.idFrom, (LPARAM)pnmshn );
                
                LocalFreeInProcess(pnmshn, hProcess);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // We can't let the shell crash in this case
        }

        SetProcPermissions(dwOldPerms);
        CloseHandle(hProcess);
    }

    return lRes;
}

/*------------------------------------------------------------------------------
    FindWindowChildOfClass - needed for SHDoneButton
------------------------------------------------------------------------------*/
BOOL FindWindowChildOfClass(HWND hwndParent, LPCTSTR szChildWindowClass, HWND *phwndChild)
{
    HWND hwndChild = NULL;

    hwndChild = GetWindow(hwndParent, GW_CHILD);
    while(hwndChild != NULL)
    {
        // Is the child window of the proper type?
        TCHAR szWindowClassName[MAX_PATH];
        if(GetClassName(hwndChild, szWindowClassName, MAX_PATH))
        {
            if(!_tcsicmp(szWindowClassName, szChildWindowClass))
            {
                // Yes; return it
                *phwndChild = hwndChild;
                return TRUE;
            }
        }

        // No; keep searching
        hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
    }

    // Didn't find it
    *phwndChild = NULL;
    return FALSE;
}

// REVIEW: STOLEN FROM AYGSHELL, TEMPORARY - used for FindMenuBar
#ifndef ARRAYSIZE
#define ARRAYSIZE(a)   (sizeof(a)/sizeof(a[0]))
#endif
/*------------------------------------------------------------------------------
    FindMenuBar - Used for SHDoneButton
------------------------------------------------------------------------------*/
HWND FindMenuBar(
    HWND hwnd       // In: Window whose menubar is to be found
)
{
    LPTSTR pszClass;
    HWND hwndSearch;
    HWND hwndMenuBar = NULL;

    pszClass = TEXT("menu_worker");

    // The menu bar is an owned window
    // Owned windows are always before their owner in the Z order
    // There may be other windows between the window and its owner (like the IME)
    // So we search for this window this way...

    hwndSearch = hwnd;

    while(NULL != (hwndSearch = GetWindow(hwndSearch, GW_HWNDPREV)))
    {
        if(GetWindow(hwndSearch, GW_OWNER) == hwnd)
        {
            TCHAR szBuff[100];
            
            if(GetClassName(hwndSearch, szBuff, ARRAYSIZE(szBuff)) &&
                (0 == lstrcmp(szBuff, pszClass)) &&
                IsWindowVisible(hwndSearch))
            {
                // we found the MenuBar!
                hwndMenuBar = hwndSearch;
                break;
            }
        }
    }

    return hwndMenuBar;
}


#define ADORNMENTS_SEPARATOR_ID 0xCCDEADCC // in cmdbar.cpp

/*------------------------------------------------------------------------------
    SHDoneButton
------------------------------------------------------------------------------*/
extern "C" BOOL SHDoneButton(HWND hwndRequester, DWORD dwState)
{
    DWORD dwStyle;
    BOOL bReturnVal = TRUE;
    HWND hwndChild = NULL;
    HWND hwndCB = NULL;

    if ( !hwndRequester || !IsWindow( hwndRequester ) || ( ( dwState != SHDB_SHOW ) && ( dwState != SHDB_HIDE ) ) )
    {
        return FALSE;
    }

    // check for WS_NONAVDONEBUTTON
    // from the docs
    // "If you need to create a top-level window without the Smart Minimize button in the upper right of the Navigation
    // Bar, do not use the WS_CAPTION style, and add the WS_NONAVDONEBUTTON to the styles when calling CreateWindow"
    dwStyle = GetWindowLong( hwndRequester, GWL_STYLE );
    if ( ( dwStyle & WS_CAPTION ) != WS_CAPTION )
    {
        // changing the rules a bit for adding OK button to the caption bar
        DWORD dwExStyle = GetWindowLong( hwndRequester, GWL_EXSTYLE );
        if ( ( dwExStyle & WS_NONAVDONEBUTTON ) == WS_NONAVDONEBUTTON )
        {
            return TRUE;
        }
    }
  
    // check for menu bar
    if ( bReturnVal )
    {
        // This function will only work if the window requesting the done button owns
        // a popup command bar.
        hwndChild = FindMenuBar( hwndRequester );
        bReturnVal = ( hwndChild != NULL );
    }
    if ( bReturnVal )
    {
        bReturnVal = FindWindowChildOfClass(hwndChild, TOOLBARCLASSNAME, &hwndCB);
    }
    if ( bReturnVal )
    {
        TBBUTTON tbbutton;
        int cButtons,n;

        cButtons = SendMessage(hwndCB, TB_BUTTONCOUNT, 0, 0);

        // Got the command bar; add the appropriate adornments.
        if(dwState == SHDB_SHOW)
        {
            BOOL bFound = FALSE;
            int iCloseButtonIndex = -1;
            int iSepIndex = -1;
            // check to see if the adornments are there, but hidden
            for (n = cButtons - 1; (n >= 0 && n >= cButtons - 3) & !bFound; n--)
            {
                if (SendMessage(hwndCB, TB_GETBUTTON, (WPARAM)n, (LPARAM)&tbbutton))
                {
                    if ( tbbutton.idCommand == IDOK ) 
                    {
                        // show the ok button, hide the close button
                        bReturnVal = SendMessage( hwndCB, TB_HIDEBUTTON, (WPARAM)tbbutton.idCommand, (LPARAM)MAKELONG(FALSE,0) );
                        SendMessage( hwndCB, TB_HIDEBUTTON, (WPARAM)WM_CLOSE, (LPARAM)MAKELONG(TRUE,0) );
                        bFound = TRUE;
                    }
                    else if ( tbbutton.idCommand == WM_CLOSE )
                    {
                        iCloseButtonIndex = n;
                    }
                    else if ( ( tbbutton.fsStyle & TBSTYLE_SEP ) && ( tbbutton.idCommand == ADORNMENTS_SEPARATOR_ID ) )
                    {
                        iSepIndex = n;
                    }

                }
            }

            if ( !bFound )
            {
                // delete the close button so we can re-add it
                if ( iCloseButtonIndex > -1 )
                {
                    SendMessage( hwndCB, TB_DELETEBUTTON, (WPARAM)iCloseButtonIndex, (LPARAM)0 );
                }

                if ( iSepIndex > -1 )
                {
                    SendMessage( hwndCB, TB_DELETEBUTTON, (WPARAM)iSepIndex, (LPARAM)0 );
                }

                // add the ok button
                bReturnVal = CommandBar_AddAdornments(hwndCB, CMDBAR_OK, 0);
                // hide the close button
                SendMessage( hwndCB, TB_HIDEBUTTON, (WPARAM)WM_CLOSE, (LPARAM)MAKELONG(TRUE,0) );
            }
        }
        else
        {
            // hide the OK button, show the close button
            // Start from the right most button and move left a max of 3
            for (n = cButtons - 1; n >= 0 && n >= cButtons - 3; n--)
            {
                if ( SendMessage(hwndCB, TB_GETBUTTON, (WPARAM)n, (LPARAM)&tbbutton) )
                {
                    if ( ( tbbutton.idCommand == IDOK ) ||
                         ( ( ( tbbutton.fsStyle & TBSTYLE_SEP ) == TBSTYLE_SEP ) &&
                           ( tbbutton.idCommand != ADORNMENTS_SEPARATOR_ID ) ) )
                    {
                        bReturnVal = SendMessage( hwndCB, TB_HIDEBUTTON, (WPARAM)tbbutton.idCommand, (LPARAM)MAKELONG(TRUE,0) );
                    }
                    else if ( tbbutton.idCommand == WM_CLOSE )
                    {
                        SendMessage( hwndCB, TB_HIDEBUTTON, (WPARAM)tbbutton.idCommand, (LPARAM)MAKELONG(FALSE,0) );
                    }
                }
            }
        }
    }

    //dwStyle = GetWindowLong( hwndRequester, GWL_STYLE );
    // didn't find a menu bar, probably a dialog
    if ( !bReturnVal && ( hwndRequester != GetDesktopWindow() ) && ( ( dwStyle & WS_CAPTION ) == WS_CAPTION ) )
    {
        // changing the rules a bit for adding OK button to the caption bar
        DWORD dwExStyle = GetWindowLong( hwndRequester, GWL_EXSTYLE );

        if ( dwState == SHDB_SHOW )
        {
            // need to add the OK button to the caption
            SetWindowLong( hwndRequester, GWL_EXSTYLE, WS_EX_CAPTIONOKBTN | dwExStyle );
            // need to remove the sysmenu
            // REVIEW: #54220, no longer removing the System menu, so that dialogs can have a close
            // button in addition to a ok button
            //SetWindowLong( hwndRequester, GWL_STYLE, ~WS_SYSMENU & dwStyle );
        }
        else
        {
            // need to remove the OK button from the caption
            SetWindowLong( hwndRequester, GWL_EXSTYLE, ~WS_EX_CAPTIONOKBTN & dwExStyle );
            // add back the sysmenu
            // REVIEW: #54220, no longer removing the System menu, so that dialogs can have a close
            // button in addition to a ok button
            //SetWindowLong( hwndRequester, GWL_STYLE, WS_SYSMENU | dwStyle );
        }

        bReturnVal = TRUE;
    }

    return bReturnVal;
}

extern BOOL Taskbar_ChangeItemText( HWND hwndApp, LPCTSTR pszText );

extern "C" BOOL WINAPI SHSetNavBarText( HWND hwnd, LPCTSTR pszText )
{
    if ( !hwnd || !IsWindow( hwnd ) )
    {
        RETAILMSG(1, (TEXT("SHELL32: SHSetNavBarText bogus hwnd %x passed in!.\r\n"),hwnd));
        return FALSE;
    }

    return Taskbar_ChangeItemText( hwnd, pszText );
}

/* Moved to public
extern "C" BOOL WINAPI SHCloseApps( DWORD dwMemSought )
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return TRUE;
}
*/

/* Support code for SHSipPreference */
// thread proc
HANDLE  g_hEvent = NULL;
HANDLE  g_hSipWaitThread = NULL;
BOOL    g_bSipGoingUp = FALSE;
#define SIP_WAIT_TIME 1000


// input dialogs
// using an array is better than the MDPG implementation
// but could maybe smarter than this
#define MAX_INPUT_DIALOGS   32
HWND    g_arrhwndInputDialogs[ MAX_INPUT_DIALOGS ];
BOOL    g_bInitializedInputDialogs = FALSE; 

//
// from wpcpriv.h
// Private SHSipPreference flags.  These are continuations of the SIPSTATE enum.
// that's defined in the public AYGSHELL.H file.  Careful here.
//
#define  SIP_INPUTDIALOGINIT ((SIPSTATE)5)
#define  SIP_DOWN_NOVALIDATE ((SIPSTATE)6)


static DWORD HandleSipWait( LPVOID lpParameter )
{
    ASSERT( g_hEvent );
    if ( !g_hEvent )
    {
        return 1;
    }

    DWORD dwWaitCode = WaitForSingleObject( g_hEvent, SIP_WAIT_TIME );

    if ( dwWaitCode == WAIT_TIMEOUT )
    {
        if ( g_bSipGoingUp )
        {
            pfnSipShowIM( SIPF_ON );
        }
        else
        {
            pfnSipShowIM( SIPF_OFF );
        }
    }

    // release the event handle
    CloseHandle( g_hEvent );
    g_hEvent = NULL;

    return 0;
}

extern "C" BOOL SHSipPreference(HWND hwnd, SIPSTATE st)
{

    BOOL fRet = TRUE;
    BOOL bCreateThread = FALSE;
    BOOL bKillThread = FALSE;
    BOOL bNewValue = FALSE;
    int  i;


    // validate SIPSTATE
    if ( ( st < SIP_UP ) || ( st > SIP_DOWN_NOVALIDATE ) || !hwnd )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if ( !g_bInitializedInputDialogs )
    {
        for ( i = 0; i < MAX_INPUT_DIALOGS; i++ )
        {
            g_arrhwndInputDialogs[ i ] = NULL;
        }

        g_bInitializedInputDialogs = TRUE;
    }

    // get rid of any bogus input dialogs, check to see if
    // hwnd is a child of any of them
    for ( i = 0; i < MAX_INPUT_DIALOGS; i++ )
    {
        if ( g_arrhwndInputDialogs[ i ] )
        {
            if ( !IsWindow( g_arrhwndInputDialogs[ i ] ) )
            {
                g_arrhwndInputDialogs[ i ] = NULL;
            }
            else
            {
                if ( IsChild( g_arrhwndInputDialogs[ i ], hwnd ) )
                {
                    return FALSE;
                }
            }
        }
    }

    switch ( st )
    {
        case SIP_UP:
            if ( g_bSipGoingUp && g_hEvent )
            {
                fRet = true;
            }
            else
            {
                bNewValue = TRUE;
                bCreateThread = TRUE;
            }
            break;
        case SIP_DOWN_NOVALIDATE: // private
        case SIP_DOWN:
            if ( !g_bSipGoingUp && g_hEvent )
            {
                fRet = TRUE;
            }
            else
            {
                bNewValue = FALSE;
                bCreateThread = TRUE;
            }
            break;
        case SIP_FORCEDOWN:
            fRet = pfnSipShowIM( SIPF_OFF );
            bKillThread = TRUE;
            break;
        case SIP_UNCHANGED:
            // kill the thread (if it exists)
            bKillThread = TRUE;
            break;
        case SIP_INPUTDIALOGINIT:
            if ( g_bSipGoingUp && g_hEvent )
            {
                fRet = true;
            }
            else
            {
                bNewValue = TRUE;
                bCreateThread = TRUE;
            }
            // fall through
        case SIP_INPUTDIALOG:
            // find a space in the input dialog array and add this
            for ( i = 0; i < MAX_INPUT_DIALOGS; i++ )
            {
                if ( !g_arrhwndInputDialogs[ i ] )
                {
                    g_arrhwndInputDialogs[ i ] = hwnd;
                    break;
                }
            }
            break;
        default:
            fRet = FALSE;
            break;
    }

    if ( bKillThread || bCreateThread )
    {
        if ( g_hEvent )
        {
            SetEvent( g_hEvent );
            if ( bCreateThread )
            {
                DWORD dwExitCode;
                while( GetExitCodeThread( g_hSipWaitThread, &dwExitCode ) && ( dwExitCode == STILL_ACTIVE ) )
                {
                    Sleep(0);
                }
            }
        }
    }

    if ( bCreateThread )
    {
        g_bSipGoingUp = bNewValue;
        g_hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
        if ( g_hEvent )
        {
            g_hSipWaitThread = CreateThread( NULL, 0, HandleSipWait, NULL, 0, NULL );
            if ( !g_hSipWaitThread )
            {
                fRet = FALSE;
            }
        }
        else
        {
            fRet = FALSE;
        }
    }

    return fRet;
}

//
// Magic number to send to SIP to get past no-app-change restriction.
//
//#define MAGIC_BYPASS                0x10921B39


// REVIEW: we have no g_sipinfo, what to do?  Can it be ignored?
extern "C" BOOL WINAPI NotSystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni )
{
/*    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return TRUE; */
    BOOL fRes = FALSE, fZeroPtr = FALSE;
    SIPINFO si, *psi;

    switch( uiAction ) {

    case SPI_SETCOMPLETIONINFO:
// REVIEW: We don't have the IME yet, can't use this
    /*        ImmEscape(                               // Suggest_GetRegValues();
            0,
            ImmGetContext(g_hwndSipFloater),
            IME_ESC_PRIVATE_FIRST+42,
            NULL ); */
        fRes = TRUE;
        break;

    case SPI_GETWORKAREA:
        fRes = SystemParametersInfo( SPI_GETWORKAREA, 0, pvParam, 0 );
        break;

    case SPI_SETSIPINFO:
    {
        BOOL fNotifyCompIME = FALSE;
        si = *(SIPINFO *)pvParam;   

        // REVIEW: Don't need
/*        if( MAGIC_BYPASS != uiParam ) {
            si.rcSipRect = g_sipinfo.rcSipRect;
            si.fdwFlags = (si.fdwFlags & (SIPF_ON|SIPF_DISABLECOMPLETION)) | (g_sipinfo.fdwFlags & ~(SIPF_ON|SIPF_DISABLECOMPLETION));
        }
*/
        if( si.cbSize == sizeof( SIPINFO ) &&
            si.dwImDataSize && si.pvImData &&
            ZeroPtr( si.pvImData ) == (DWORD)si.pvImData )
        {
            si.pvImData = MapPtrToProcess(
                                si.pvImData,
                                (HANDLE)GetCallerProcess() );
        }

        // if the completion state changes, we need to notify the completion ime.
        // with the optimizations now done in the OS, this doesn't always happen
        // from the SipSetInfo call if that doesn't change the sip up/down state
        // REVIEW: don't have auto complete yet
/*
        if ((si.fdwFlags ^ g_sipinfo.fdwFlags) & SIPF_DISABLECOMPLETION)
        {
            fNotifyCompIME = TRUE;
        }
*/
        // save this now, because as we're making this call, another psl can can come in
        // and want to use the g_sipinfo  to set yet another state, and we want them to use
        // the new info so they won't inadvertantly back our change out
        // REVIEW
//        g_sipinfo.fdwFlags = si.fdwFlags;
        fRes = pfnSipSetInfo( &si );

        // REVIEW: don't have IME
/*        if (fNotifyCompIME)
        {
            NotifyFakeIME(WM_SETTINGCHANGE, SPI_SETSIPINFO, NULL);
        } */
        break;
    }

    case SPI_GETSIPINFO:
    {
        psi = (SIPINFO *)pvParam;
        if (psi)
        {
            if( 
                psi->cbSize == sizeof( SIPINFO ) &&
                psi->dwImDataSize && psi->pvImData &&
                ZeroPtr( psi->pvImData ) == (DWORD)psi->pvImData )
            {
                psi->pvImData = MapPtrToProcess(
                                                psi->pvImData,
                                                (HANDLE)GetCallerProcess() );
                fZeroPtr = TRUE;
            }

            fRes = pfnSipGetInfo( (SIPINFO *)pvParam );

            // since this is a special flag of our own, and not one the OS knows about, we need to fill this bit in ourselves;
            // REVIEW: don't have auto complete yet
//            psi->fdwFlags = (g_sipinfo.fdwFlags & SIPF_DISABLECOMPLETION) | (psi->fdwFlags & ~SIPF_DISABLECOMPLETION);

            if (!fRes)
            {
                // trouble in toon town.  Let's lie a little...
                RETAILMSG(1, (TEXT("SHELL32: XNotSystemParametersInfo, SPI_GETSIPINFO, SipGetInfo failed!  Defaulting...\r\n")));
                SystemParametersInfo(SPI_GETWORKAREA, 0, &(((SIPINFO *)pvParam)->rcVisibleDesktop), FALSE);
                ((SIPINFO *)pvParam)->fdwFlags = SIPF_DOCKED;
            }
            else
            {
                // workaround for McKendrick #59867
                // a sip docked to the top of the workarea returns an invalid
                // visible desktop rect.
                if ( psi->rcVisibleDesktop.top == psi->rcVisibleDesktop.bottom )
                {
                    RECT rcWorkArea;
                    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
                    if ( ( psi->rcVisibleDesktop.bottom != psi->rcSipRect.bottom ) &&
                         ( psi->rcVisibleDesktop.top != psi->rcSipRect.top ) )
                    {
                        // docked in the middle?
                        psi->rcVisibleDesktop.bottom = rcWorkArea.bottom;
                        psi->rcVisibleDesktop.top = rcWorkArea.top;
                    }                        
                    else if ( psi->rcVisibleDesktop.bottom != psi->rcSipRect.bottom )
                    {
                        // docked on top
                        psi->rcVisibleDesktop.top = psi->rcSipRect.bottom;
                        psi->rcVisibleDesktop.bottom = rcWorkArea.bottom;
                    }
                    else
                    {
                        // docked on bottom
                        psi->rcVisibleDesktop.top = rcWorkArea.top;
                        psi->rcVisibleDesktop.bottom = psi->rcSipRect.top;
                    }
                }
            }

            if( fZeroPtr ) {
                psi->pvImData = (void *)ZeroPtr( psi->pvImData );
            }
        }
        break;
    }

    case SPI_SETCURRENTIM:
        fRes = pfnSipSetCurrentIM( (CLSID *)pvParam );
        break; 

    case SPI_GETCURRENTIM:
        fRes = pfnSipGetCurrentIM( (CLSID *)pvParam );
        break;

    default:
        SetLastError( ERROR_INVALID_PARAMETER );
        break;

    } // switch( uiAction )

    return fRes;

    UNREFERENCED_PARAMETER( fWinIni );
}
