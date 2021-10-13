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
#include <commctrl.h>
#include <aygshell.h>
#include <intsafe.h>

// from mysip.cpp
BOOL pfnSipShowIM( DWORD dwFlag );
BOOL pfnSipGetCurrentIM( CLSID* pClsid );
BOOL pfnSipSetCurrentIM( CLSID* pClsid );
BOOL pfnSipGetInfo( SIPINFO *pSipInfo );
BOOL pfnSipSetInfo( SIPINFO *pSipInfo );
//
// Magic number to send to SIP to get past no-app-change restriction.
//
#define MAGIC_BYPASS                0x06d00000


/*------------------------------------------------------------------------------
    Send Bubble Notification Message to App
------------------------------------------------------------------------------*/
LRESULT SendBubbleNotificationMessage( HWND hwnd, NMSHN *pNMSHN, TCHAR *pszTarget )
{
    HRESULT hr = NOERROR;
    DWORD dwProcessID;
    HPROCESS hProcess;
    LRESULT lRes = 0;
    NMSHN *pnmshnRemote = NULL;
    NMSHN *pnmshn = NULL;

    ASSERT( hwnd );
    if ( !hwnd || !IsWindow( hwnd ) )
    {
        return lRes;
    }

    GetWindowThreadProcessId(hwnd, &dwProcessID);
    hProcess = OpenProcess(0, FALSE, dwProcessID);
    if (NULL == hProcess)
    {
        return lRes;
    }

    __try
    {
        DWORD dwStrMem = 0;
        DWORD cch;
        DWORD cbWrite;
        DWORD cbWritten;

        if ( pszTarget )
        {
            // allocate space at the end of the structure if there is a link. This makes marshalling easier.
            if (FAILED(DWordAdd(_tcslen(pszTarget), 1, &cch)))
            {
                goto Exit;
            }

            if (FAILED(DWordMult(cch, sizeof(TCHAR), &dwStrMem)))
            {
                goto Exit;
            }
        }

        if (FAILED(DWordAdd(dwStrMem, sizeof(NMSHN), &cbWrite)))
        {
            goto Exit;
        }

        pnmshn = (NMSHN *)LocalAlloc(LMEM_FIXED, cbWrite);
        if (NULL != pnmshn)
        {

            if ( pszTarget )
            {
                LPTSTR pszMapped;
                pszMapped = (LPTSTR)(pnmshn + 1);
                StringCbCopy(pszMapped, dwStrMem, pszTarget);

                pnmshn->hdr.hwndFrom = pNMSHN->hdr.hwndFrom;
                pnmshn->hdr.idFrom = pNMSHN->hdr.idFrom;
                pnmshn->hdr.code = pNMSHN->hdr.code;
                pnmshn->pszLink = pszMapped;
                pnmshn->lParam = pNMSHN->lParam;
                pnmshn->dwReturn = pNMSHN->dwReturn;
            }
            else
            {
                *pnmshn = *pNMSHN;
            }

            // need to alloc memory so the receiving application can read the data.
            pnmshnRemote = (NMSHN*)VirtualAllocEx(hProcess, NULL, cbWrite, MEM_COMMIT, PAGE_READWRITE);
            if (NULL != pnmshnRemote)
            {
                // If there was a link point past the header where the string actually is located in the remoted allocation.
                if (pszTarget)
                {
                    pnmshn->pszLink = (LPTSTR)(pnmshnRemote + 1);
                }
                if (WriteProcessMemory(hProcess, pnmshnRemote, pnmshn, cbWrite, &cbWritten))
                {
                    lRes = SendMessage( hwnd, WM_NOTIFY, (WPARAM)pNMSHN->hdr.idFrom, (LPARAM)pnmshnRemote );
                }
                VirtualFreeEx(hProcess, pnmshnRemote, 0, MEM_RELEASE);
             }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // We can't let the shell crash in this case
        lRes = 0;
    }

    LocalFree(pnmshn);

Exit:
    if (NULL != hProcess)
    {
        CloseHandle(hProcess);
    }

    return lRes;
}


extern BOOL Taskbar_ChangeItemText( HWND hwndApp, LPCTSTR pszText );

extern "C" BOOL WINAPI SHSetNavBarText( HWND hwnd, LPCTSTR pszText )
{
    __try
    {
        if ( !hwnd || !IsWindow( hwnd ) )
        {
            RETAILMSG(1, (TEXT("SHELL32: SHSetNavBarText bogus hwnd %x passed in!.\r\n"),hwnd));
            return FALSE;
        }

        return Taskbar_ChangeItemText( hwnd, pszText );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
        return FALSE;
    }
}

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
        SIPINFO sipinfo = {0};
        sipinfo.cbSize = sizeof( SIPINFO );
        if ( g_bSipGoingUp )
        {
            if( pfnSipGetInfo( &sipinfo ) )
            {
                sipinfo.fdwFlags &= ~SIPF_OFF;
                sipinfo.fdwFlags |= SIPF_ON | MAGIC_BYPASS;
                pfnSipSetInfo( &sipinfo );
            }
        }
        else
        {
            if( pfnSipGetInfo( &sipinfo ) )
            {
                sipinfo.fdwFlags &= ~SIPF_ON;
                sipinfo.fdwFlags |= SIPF_OFF | MAGIC_BYPASS;
                pfnSipSetInfo( &sipinfo );
            }
        }
    }

    // release the event handle
    HANDLE localEvent = g_hEvent;
    g_hEvent = NULL;
    CloseHandle( localEvent );

    return 0;
}

// This function is not thread safe. It needs to be rewritten.
extern "C" BOOL SHSipPreference(HWND hwnd, SIPSTATE st)
{
    __try
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

        if (g_hSipWaitThread && (bKillThread || bCreateThread))
        {
            if (g_hEvent)
            {
                SetEvent( g_hEvent );
            }
            WaitForSingleObject(g_hSipWaitThread, INFINITE);

            // release the thread handle
            CloseHandle( g_hSipWaitThread );
            g_hSipWaitThread = NULL;
        }

        if ( bCreateThread )
        {
            g_bSipGoingUp = bNewValue;

            ASSERT(g_hEvent == NULL);

            g_hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
            if ( g_hEvent )
            {
                ASSERT(g_hSipWaitThread == NULL);

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
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
        return FALSE;
    }
}


// REVIEW: we have no g_sipinfo, what to do?  Can it be ignored?
extern "C" BOOL WINAPI NotSystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni )
{
    __try
    {
        HRESULT hr = NOERROR;
        SIPINFO *pMarshalledSipInfo = NULL;
        VOID *pMarshalledIMData = NULL;
        VOID *pvIMData;
        DWORD  cbIMData;
        BOOL fRes = FALSE, fZeroPtr = FALSE;
        SIPINFO *psi;
        LPRECT  pMarshalledRect = NULL;
        CLSID *pMarshalledCLSID = NULL;

        switch( uiAction ) {

        case SPI_SETCOMPLETIONINFO:
            fRes = TRUE;
            break;

        case SPI_GETWORKAREA:

            hr = CeOpenCallerBuffer((void **)&pMarshalledRect, pvParam, sizeof(RECT), ARG_O_PTR, FALSE);
            if (SUCCEEDED(hr))
            {
                fRes = SystemParametersInfo( SPI_GETWORKAREA, 0, pMarshalledRect, 0 );
                VERIFY(SUCCEEDED(CeCloseCallerBuffer(pMarshalledRect, pvParam, sizeof(RECT), ARG_O_PTR)));
            }
            break;

        case SPI_SETSIPINFO:
        {
            hr = CeOpenCallerBuffer((void **)&pMarshalledSipInfo, pvParam, sizeof(SIPINFO), ARG_IO_PTR, TRUE);
            if (SUCCEEDED(hr) && pMarshalledSipInfo)
            {
                pvIMData = pMarshalledSipInfo->pvImData;
                cbIMData = pMarshalledSipInfo->dwImDataSize;

                if ((pMarshalledSipInfo->cbSize == sizeof(SIPINFO)) &&
                    cbIMData && pvIMData)
                {
                    hr = CeOpenCallerBuffer((void **)&pMarshalledIMData, pvIMData, 
                                cbIMData, ARG_IO_PTR, TRUE);
                }
                
                if (SUCCEEDED(hr))
                {
                    pMarshalledSipInfo->pvImData = pMarshalledIMData;
                    fRes = pfnSipSetInfo(pMarshalledSipInfo);
                    pMarshalledSipInfo->pvImData = pvIMData;

                    if (pMarshalledIMData)
                    {
                        VERIFY(SUCCEEDED(hr = CeCloseCallerBuffer(pMarshalledIMData, pvIMData, cbIMData, ARG_IO_PTR)));
                    }
                }
                VERIFY(SUCCEEDED(hr = CeCloseCallerBuffer(pMarshalledSipInfo, pvParam, sizeof(SIPINFO), ARG_IO_PTR)));
            }

            break;
        }

        case SPI_GETSIPINFO:
        {
            
            psi = (SIPINFO *)pvParam;
            if (psi)
            {
        
                hr = CeOpenCallerBuffer((void **)&pMarshalledSipInfo, psi, sizeof(SIPINFO), ARG_IO_PTR, TRUE);
                if (SUCCEEDED(hr) && pMarshalledSipInfo)
                {
                    pvIMData = pMarshalledSipInfo->pvImData;
                    cbIMData = pMarshalledSipInfo->dwImDataSize;

                    if ((pMarshalledSipInfo->cbSize == sizeof(SIPINFO)) &&
                        cbIMData && pvIMData)
                    {
                        hr = CeOpenCallerBuffer((void **)&pMarshalledIMData, pvIMData, 
                                    cbIMData, ARG_IO_PTR, TRUE);
                    }
                
                    if (SUCCEEDED(hr))
                    {
                        pMarshalledSipInfo->pvImData = pMarshalledIMData;
                        fRes = pfnSipGetInfo(pMarshalledSipInfo);

                        // since this is a special flag of our own, and not one the OS knows about, we need to fill this bit in ourselves;
                        // REVIEW: don't have auto complete yet
                        //            psi->fdwFlags = (g_sipinfo.fdwFlags & SIPF_DISABLECOMPLETION) | (psi->fdwFlags & ~SIPF_DISABLECOMPLETION);
                        if (FALSE == fRes)
                        {
                            SystemParametersInfo(SPI_GETWORKAREA, 0, &(pMarshalledSipInfo->rcVisibleDesktop), FALSE);
                            pMarshalledSipInfo->fdwFlags = SIPF_DOCKED;
                        }
                        pMarshalledSipInfo->pvImData = pvIMData;
                        
                        if (pMarshalledIMData)
                        {
                            VERIFY(SUCCEEDED(hr = CeCloseCallerBuffer(pMarshalledIMData, pvIMData, cbIMData, ARG_IO_PTR)));
                        }
                    }
                    VERIFY(SUCCEEDED(hr = CeCloseCallerBuffer(pMarshalledSipInfo, psi, sizeof(SIPINFO), ARG_IO_PTR)));
                }
            }
            break;
        }

        case SPI_SETCURRENTIM:
            hr = CeOpenCallerBuffer((void **)&pMarshalledCLSID, pvParam, sizeof(CLSID), ARG_I_PTR, TRUE);
            if (SUCCEEDED(hr))
            {
                fRes = pfnSipSetCurrentIM( (CLSID *)pMarshalledCLSID);
                VERIFY(SUCCEEDED(CeCloseCallerBuffer(pMarshalledCLSID, pvParam, sizeof(CLSID), ARG_I_PTR)));
            }
            break; 

        case SPI_GETCURRENTIM:
            hr = CeOpenCallerBuffer((void **)&pMarshalledCLSID, pvParam, sizeof(CLSID), ARG_O_PTR, TRUE);
            if (SUCCEEDED(hr))
            {
                fRes = pfnSipGetCurrentIM( (CLSID *)pMarshalledCLSID );
                VERIFY(SUCCEEDED(CeCloseCallerBuffer(pMarshalledCLSID, pvParam, sizeof(CLSID), ARG_O_PTR)));
            }
            break;

        default:
            SetLastError( ERROR_INVALID_PARAMETER );
            break;

        } // switch( uiAction )

        return fRes;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ASSERT(FALSE);
        return FALSE;
    }

    UNREFERENCED_PARAMETER( fWinIni );
}
