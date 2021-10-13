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
#include <winreg.h>
#include <commctrl.h>
#include <aygshell.h>
#include <shellapip.h>
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

// REVIEW: we have no g_sipinfo, what to do?  Can it be ignored?
extern "C" BOOL WINAPI NotSystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT cbpvParam, UINT fWinIni )
{
    __try
    {
        HRESULT hr = NOERROR;
        SIPINFO *pMarshalledSipInfo = NULL;
        VOID *pMarshalledIMData = NULL;
        VOID *pvIMData;
        DWORD  cbIMData;
        BOOL fRes = FALSE, fZeroPtr = FALSE;
        LPRECT  pMarshalledRect = NULL;
        CLSID *pMarshalledCLSID = NULL;

        switch( uiAction ) {

        case SPI_SETCOMPLETIONINFO:
            fRes = TRUE;
            break;

        case SPI_GETWORKAREA:
            if (cbpvParam == sizeof(RECT) && NULL != pvParam)
            {
                pMarshalledRect = (LPRECT)pvParam;
                fRes = SystemParametersInfo( SPI_GETWORKAREA, 0, pMarshalledRect, 0 );
            }
            break;

        case SPI_SETSIPINFO:
        {
            pMarshalledSipInfo = (SIPINFO*)pvParam;
            if (pMarshalledSipInfo)
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
            }

            break;
        }

        case SPI_GETSIPINFO:
        {
            pMarshalledSipInfo = (SIPINFO*)pvParam;
            if (pMarshalledSipInfo)
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
            }
        }
        break;

        case SPI_SETCURRENTIM:
            pMarshalledCLSID = (CLSID*)pvParam;
            if (NULL != pMarshalledCLSID && cbpvParam == sizeof(CLSID))
            {
                fRes = pfnSipSetCurrentIM( (CLSID *)pMarshalledCLSID);
            }
            break; 

        case SPI_GETCURRENTIM:
            pMarshalledCLSID = (CLSID*)pvParam;
            if (NULL != pMarshalledCLSID && cbpvParam == sizeof(CLSID))
            {
                fRes = pfnSipGetCurrentIM( (CLSID *)pMarshalledCLSID );
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
