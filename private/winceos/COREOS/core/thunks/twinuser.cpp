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
#define BUILDING_USER_STUBS

#include <windows.h>
#include <windev.h>
#define _OBJBASE_H_
#define __objidl_h__
#include "imm.h"
#include "window.hpp"   // gwes
#include "dlgmgr.h"
#include "touchgesture.h"
#ifndef RDPGWELIB
#include "KillThreadIfNeeded.hpp"
#endif

#include <commctrl.h>

#include <CePtr.hpp>
#include <twinuser.h>
#include <GweApiSet1.hpp>
#include <safeInt.hxx>
#include <IntSafe.h>
#include <tvout.h>
#include <gesture_priv.h>

#define ISKMODEADDR(Addr) ((int)Addr < 0)

const UINT32 CWindow::s_sigValid = 0x574e4457;
#define MSG_QUEUE_SIG   0x4d534751

typedef void*   HDWP;

#define DISABLEFAULTS() (UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT)
#define ENABLEFAULTS()  (UTlsPtr()[TLSSLOT_KERNEL] &= ~TLSKERN_NOFAULT)

class DisableFaults_t
{
public:

    DisableFaults_t(
        void
        )
    {
        (UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT);
    }

    ~DisableFaults_t(
        void
        )
    {
        (UTlsPtr()[TLSSLOT_KERNEL] &= ~TLSKERN_NOFAULT);
    }


};

extern GweApiSet1_t *pGweApiSet1Entrypoints;
extern GweApiSet2_t *pGweApiSet2Entrypoints;


//xref ApiTrapWrapper
extern "C"
BOOL
UnregisterClassW(
    __in LPCWSTR     pClassName,
    HINSTANCE   hInstance
    )
{
    ATOM ClassIdentifier = 0;
    BOOL ReturnValue = FALSE;

    if( !pClassName )
        {
        SetLastError(ERROR_CLASS_DOES_NOT_EXIST);
        goto leave;
        }

    if( !HIWORD(pClassName) )
        {
        ClassIdentifier = static_cast<ATOM>(LOWORD(pClassName));
        pClassName = NULL;
        }

  // Unlike desktop, in WinCE, we look up classes based on process (which is acquired within GWES)  
  // rather than HINSTANCE

    ReturnValue = pGweApiSet1Entrypoints->m_pUnregisterClassW( ClassIdentifier, pClassName);

leave:
    return ReturnValue;    
}



//xref ApiTrapWrapper
extern "C"
HWND
CreateWindowExW(
    DWORD       dwExStyle,
    __in LPCWSTR     lpClassName,
    __in_opt LPCWSTR     lpWindowName,
    DWORD       dwStyle,
    int         X,
    int         Y,
    int         nWidth,
    int         nHeight,
    HWND        hwndParent,
    HMENU       hMenu,
    HINSTANCE   hInstance,
    __in_opt LPVOID      lpParam
    )
{
    CREATESTRUCTW   createStruct = {0};

    createStruct.dwExStyle   = dwExStyle;
    createStruct.lpszClass   = lpClassName;
    createStruct.lpszName    = lpWindowName;
    createStruct.style       = dwStyle;
    createStruct.x           = X;
    createStruct.y           = Y;
    createStruct.cx          = nWidth;
    createStruct.cy          = nHeight;
    createStruct.hwndParent  = hwndParent;
    createStruct.hMenu       = hMenu;
    createStruct.hInstance   = hInstance;
    createStruct.lpCreateParams = lpParam;

    return pGweApiSet1Entrypoints->m_pCreateWindowExW(
                &createStruct,
                sizeof(CREATESTRUCTW)
                );

}


extern "C"
void
PostQuitMessage(
    int nExitCode
    )
{
//  PostQuitMessage_Trap(nExitCode);
    pGweApiSet1Entrypoints->m_pPostQuitMessage(nExitCode);
}



//xref ApiTrapWrapper
extern "C"
BOOL
TranslateMessage(
    __in CONST   MSG*    pMsg
    )
{
    if ( pMsg )
        {
        switch ( pMsg -> message )
            {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return pGweApiSet1Entrypoints->m_pTranslateMessage(pMsg,sizeof(MSG));
//              return TranslateMessage_Trap(pMsg);
            }
        }
    return FALSE;
}



//xref ApiTrapWrapper
extern "C"
BOOL
InSendMessage(
    void
    )
{
    return pGweApiSet2Entrypoints->m_pInSendMessage();
}



//xref ApiTrapWrapper
extern "C"
DWORD
GetQueueStatus(
    UINT    flags
    )
{
    return pGweApiSet2Entrypoints->m_pGetQueueStatus(flags);
}



//xref ApiTrapWrapper
extern "C"
HWND
GetCapture(
    void
    )
{
//  return GetCapture_Trap();
    return pGweApiSet1Entrypoints->m_pGetCapture();
}


extern "C"
HWND
SetCapture(
    HWND    hWnd
    )
{
//  return SetCapture_Trap(hWnd);
    return pGweApiSet1Entrypoints->m_pSetCapture(hWnd);
}


//xref ApiTrapWrapper
extern "C"
BOOL
ReleaseCapture(
    void
    )
{
//  return ReleaseCapture_Trap();
    return pGweApiSet1Entrypoints->m_pReleaseCapture();
}


//xref ApiTrapWrapper
extern "C"
HWND
WindowFromPoint(
    POINT   Point
    )
{
    return pGweApiSet1Entrypoints->m_pWindowFromPoint(Point);
}


//xref ApiTrapWrapper
extern "C"
HWND
ChildWindowFromPoint(
    HWND    hwndParent,
    POINT   Point
    )
{
    return pGweApiSet1Entrypoints->m_pChildWindowFromPoint(hwndParent, Point);
}


//xref ApiTrapWrapper
extern "C"
BOOL
ScreenToClient(
    HWND    hwnd,
    __inout LPPOINT lpPoint
    )
{
    return pGweApiSet1Entrypoints->m_pScreenToClient(hwnd, lpPoint, sizeof(POINT));
}


//xref ApiTrapWrapper
extern "C"
BOOL
SetWindowTextW(
    HWND    hwnd,
    __in LPCTSTR psz
    )
{
    return pGweApiSet1Entrypoints->m_pSetWindowTextW(hwnd, psz);
}


extern "C"
int
GetWindowTextW(
    HWND    hwnd,
    __out_ecount(cchMax) WCHAR * pString,
    int     cchMax
    )
{
    int ReturnValue = 0;

    unsigned int cchStringBufferCharCount = 0;
    size_t cbStringBufferSize = 0;
    
    if( FAILED( IntToUInt( cchMax, &cchStringBufferCharCount)) || 
          FAILED( UIntMult(cchStringBufferCharCount, sizeof(WCHAR), &cbStringBufferSize)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    ReturnValue = pGweApiSet1Entrypoints->m_pGetWindowTextW(hwnd, pString, cbStringBufferSize, cchMax, FALSE);

leave:
    return ReturnValue;
}




extern "C"
HDC
GetDCEx(
    HWND    hwnd,
    HRGN    hrgn,
    DWORD   flags
    )
{
    return pGweApiSet1Entrypoints->m_pGetDCEx(hwnd, hrgn, flags);
}


extern "C"
DWORD
GetClassLongW(
    HWND    hwnd,
    INT     nIndex
    )
{
    return pGweApiSet1Entrypoints->m_pGetClassLongW(hwnd, nIndex);
}

//xref ApiTrapWrapper
extern "C"
DWORD
SetClassLongW(
    HWND    hwnd,
    INT     nIndex,
    LONG    lValue
    )
{
    return pGweApiSet1Entrypoints->m_pSetClassLongW(hwnd, nIndex, lValue);
}

extern "C"
BOOL
DestroyWindow(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pDestroyWindow(hwnd);
}

extern "C"
BOOL
ShowWindow(
    HWND    hwnd,
    INT     nCmdShow
    )
{
    return pGweApiSet1Entrypoints->m_pShowWindow(hwnd, nCmdShow);
}

extern "C"
BOOL
UpdateWindow(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pUpdateWindow(hwnd);
}

//xref ApiTrapWrapper
extern "C"
BOOL
RedrawWindow(
    HWND    hwnd,
    __in_opt CONST RECT* lprcUpdate,
    HRGN     hrgn,
    UINT      flags
    )
{
    return pGweApiSet2Entrypoints->m_pRedrawWindow(hwnd,lprcUpdate,sizeof(RECT),hrgn,flags);
}

//xref ApiTrapWrapper
extern "C"
HWND
SetParent(
    HWND    hwnd,
    HWND    hwndParent
    )
{
    return pGweApiSet1Entrypoints->m_pSetParent(hwnd, hwndParent);
}


//xref ApiTrapWrapper
extern "C"
HWND
GetActiveWindow(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetActiveWindow();
}




//xref ApiTrapWrapper
extern "C"
BOOL
AdjustWindowRectEx( 
    __inout PRECT   prc,
    DWORD   dwStyle,
    BOOL    bMenu,
    DWORD   dwExStyle
    )
{
    return pGweApiSet1Entrypoints->m_pAdjustWindowRectEx(prc, sizeof(RECT), dwStyle, bMenu, dwExStyle);
}



//xref ApiTrapWrapper
extern "C"
HMENU
CreatePopupMenu(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pCreatePopupMenu();
}

//xref ApiTrapWrapper
extern "C"
BOOL
InsertMenuW(
    HMENU   hMenu,
    UINT    uPosition,
    UINT    uFlags,
    UINT    uIDNewItem,
    __in_opt LPCWSTR lpNewItem
    )
{
    DWORD dwNewItemData = 0;

    if (0 != (uFlags & (MFT_OWNERDRAW | MFT_SEPARATOR)))
        {
        dwNewItemData = (DWORD)lpNewItem;
        lpNewItem = NULL;
        }

    return pGweApiSet1Entrypoints->m_pInsertMenuW(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem, dwNewItemData);
}

//xref ApiTrapWrapper
extern "C"
BOOL
AppendMenuW(
    HMENU   hMenu,
    UINT    uFlags,
    UINT    uIDNewItem,
    __in_opt LPCWSTR lpNewItem
    )
{
    DWORD dwNewItemData = 0;

    if (0 != (uFlags & (MFT_OWNERDRAW | MFT_SEPARATOR)))
        {
        dwNewItemData = (DWORD)lpNewItem;
        lpNewItem = NULL;
        }

    return pGweApiSet1Entrypoints->m_pAppendMenuW(hMenu, uFlags, uIDNewItem, lpNewItem, dwNewItemData);
}

//xref ApiTrapWrapper
extern "C"
BOOL
RemoveMenu(
    HMENU   hMenu,
    UINT    uPosition,
    UINT    uFlags
    )
{
    return pGweApiSet1Entrypoints->m_pRemoveMenu(hMenu, uPosition, uFlags);
}

extern "C"
BOOL
DestroyMenu(
    HMENU   hMenu
    )
{
    return pGweApiSet1Entrypoints->m_pDestroyMenu(hMenu);
}

extern "C"
BOOL
TrackPopupMenuEx(
    HMENU       hMenu,
    UINT        uFlags,
    int         x,
    int         y,
    HWND        hWnd,
    __in_opt LPTPMPARAMS lptpm
    )
{
    return pGweApiSet1Entrypoints->m_pTrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm, sizeof(TPMPARAMS));
}



extern "C"
HMENU
LoadMenuW(
    HINSTANCE    hInstance,
    __in const   WCHAR*  pMenuName
    )
{
    DWORD MenuNameResourceID = 0;

    if( IS_RESOURCE_ID(pMenuName) )
        {
        MenuNameResourceID  = reinterpret_cast<DWORD>(pMenuName);
        pMenuName = NULL;
        }
    
    return pGweApiSet1Entrypoints->m_pLoadMenuW( hInstance, MenuNameResourceID, pMenuName);    
}



extern "C"
BOOL
EnableMenuItem(
    HMENU   hMenu,
    UINT    uIDEnableItem,
    UINT    uEnable
    )
{
    return pGweApiSet1Entrypoints->m_pEnableMenuItem(hMenu, uIDEnableItem, uEnable);
}

extern "C"
UINT
WINAPI
GetMenuDefaultItem(
    HMENU hMenu,
    UINT fByPos,
    UINT gmdiFlags)
{
    return pGweApiSet2Entrypoints->m_pGetMenuDefaultItem(hMenu, fByPos, gmdiFlags);
}

extern "C"
BOOL
WINAPI
SetMenuDefaultItem(
    HMENU hMenu,
    UINT uItem,
    UINT fByPos)
{
    return pGweApiSet2Entrypoints->m_pSetMenuDefaultItem(hMenu, uItem, fByPos);
}

extern "C"
BOOL
WINAPI
SetMenuItemBitmaps(
    HMENU hMenu,
    UINT uPosition,
    UINT uFlags,
    HBITMAP hBitmapUnchecked,
    HBITMAP hBitmapChecked)
{
    return pGweApiSet2Entrypoints->m_pSetMenuItemBitmaps(hMenu, uPosition, uFlags, hBitmapUnchecked, hBitmapChecked);
}

extern "C"
BOOL
MoveWindow(
    HWND    hWnd,
    int     X,
    int     Y,
    int     nWidth,
    int     nHeight,
    BOOL    bRepaint
    )
{
    return pGweApiSet1Entrypoints->m_pMoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
}


//xref ApiTrapWrapper
extern "C"
int
GetUpdateRgn(
    HWND    hWnd,
    HRGN    hRgn,
    BOOL    bErase
    )
{
    return pGweApiSet1Entrypoints->m_pGetUpdateRgn(hWnd, hRgn, bErase);
}

//xref ApiTrapWrapper
extern "C"
BOOL
GetUpdateRect(
    HWND    hWnd,
    __out_opt LPRECT  lpRect,
    BOOL    bErase
    )
{
    return pGweApiSet1Entrypoints->m_pGetUpdateRect(hWnd, lpRect, sizeof(RECT), bErase);
}


//xref ApiTrapWrapper
extern "C"
BOOL
BringWindowToTop(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pBringWindowToTop(hWnd);
}

//xref ApiTrapWrapper
extern "C"
int
GetWindowTextLengthW(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pGetWindowTextLengthW(hWnd);
}

//xref ApiTrapWrapper
extern "C"
BOOL
IsChild(
    HWND    hWndParent,
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pIsChild(hWndParent, hWnd);
}



//xref ApiTrapWrapper
extern "C"
HDC
GetWindowDC(
    HWND    hwnd
    )
{
    return  pGweApiSet1Entrypoints->m_pGetWindowDC(hwnd);
}

extern "C"
HWND
SetFocus(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pSetFocus(hwnd);
}



//xref ApiTrapWrapper
extern "C"
BOOL
ValidateRect(
    HWND    hWnd,
    __in_opt CONST   RECT*   lpRect
    )
{
    return pGweApiSet1Entrypoints->m_pValidateRect(hWnd, lpRect, sizeof(RECT));
}

//xref ApiTrapWrapper
extern "C"
BOOL
ValidateRgn(
            HWND    hWnd,
            HRGN    hrgn
    )
{
    return pGweApiSet2Entrypoints->m_pValidateRgn(hWnd, hrgn);
}

//xref ApiTrapWrapper
extern "C"
BOOL
InvalidateRgn(
            HWND    hWnd,
            HRGN    hRgn,
            BOOL    fErase
    )
{
    return pGweApiSet2Entrypoints->m_pInvalidateRgn(hWnd, hRgn, fErase);
}

//xref ApiTrapWrapper
extern "C"
HBITMAP
LoadBitmapW(
    HINSTANCE      hInstance,
    __in const WCHAR * pBitmapName
    )
{ 
    DWORD BitmapNameResourceID = 0;

    if( IS_RESOURCE_ID(pBitmapName) )
        {
        BitmapNameResourceID  = reinterpret_cast<DWORD>(pBitmapName);
        pBitmapName = NULL;
        }
    
    return pGweApiSet1Entrypoints->m_pLoadBitmapW( hInstance, BitmapNameResourceID, pBitmapName);
}


//xref ApiTrapWrapper
extern "C"
DWORD
CheckMenuItem(
    HMENU   hMenu,
    UINT    uIDCheckItem,
    UINT    uCheck
    )
{
    return pGweApiSet1Entrypoints->m_pCheckMenuItem(hMenu, uIDCheckItem, uCheck);
}


//xref ApiTrapWrapper
extern "C"
BOOL
CheckMenuRadioItem(
    HMENU   hMenu,
    UINT    wIDFirst,
    UINT    wIDLast,
    UINT    wIDCheck,
    UINT    flags
    )
{
    return pGweApiSet1Entrypoints->m_pCheckMenuRadioItem(hMenu, wIDFirst, wIDLast, wIDCheck, flags);
}



//xref ApiTrapWrapper
extern "C"
HICON
LoadIconW(
    HINSTANCE   hInstance,
    __in const WCHAR *  pIconName
    )
{
    DWORD IconNameResourceID = 0;

    if( IS_RESOURCE_ID(pIconName) )
        {
        IconNameResourceID  = reinterpret_cast<DWORD>(pIconName);
        pIconName = NULL;
        }
    
    return pGweApiSet1Entrypoints->m_pLoadIconW( hInstance, IconNameResourceID, pIconName);
}


extern "C"
BOOL
DrawIconEx(
    HDC     hDC,
    int     X,
    int     Y,
    HICON   hIcon,
    int     cxWidth,
    int     cyWidth,
    UINT    istepIfAniCur,
    HBRUSH  hbrFlickerFreeDraw,
    UINT    diFlags
    )
{
    return pGweApiSet1Entrypoints->m_pDrawIconEx(
                hDC,
                X,
                Y,
                hIcon,
                cxWidth,
                cyWidth,
                istepIfAniCur,
                hbrFlickerFreeDraw,
                diFlags
                );
}



extern "C"
BOOL
DestroyIcon(
    HICON   hicon
    )
{
    return pGweApiSet1Entrypoints->m_pDestroyIcon(hicon);
}



//xref ApiTrapWrapper
extern "C"
HDWP
BeginDeferWindowPos(
    int nNumWindows
    )
{
    return pGweApiSet1Entrypoints->m_pBeginDeferWindowPos(nNumWindows);
}



//xref ApiTrapWrapper
extern "C"
HDWP
DeferWindowPos(
    HDWP    hWinPosInfo,
    HWND    hWnd,
    HWND    hWndInsertAfter,
    int     x,
    int     y,
    int     cx,
    int     cy,
    UINT    uFlags
    )
{
    return pGweApiSet1Entrypoints->m_pDeferWindowPos(hWinPosInfo, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
}



//xref ApiTrapWrapper
extern "C"
BOOL
EndDeferWindowPos(
    HDWP    hWinPosInfo
    )
{
    return pGweApiSet1Entrypoints->m_pEndDeferWindowPos(hWinPosInfo);
}




//xref ApiTrapWrapper
extern "C"
BOOL
GetMonitorInfo(
    HMONITOR        hMonitor,
    __inout LPMONITORINFO   lpmi
    )
{
    BOOL fReturnValue = FALSE;

    if ( (NULL == lpmi) ||
            (sizeof(MONITORINFO) > lpmi->cbSize) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    fReturnValue = pGweApiSet2Entrypoints->m_pGetMonitorInfo(hMonitor, lpmi, lpmi->cbSize);

leave:
    return fReturnValue;
}



//xref ApiTrapWrapper
extern "C"
BOOL
EnumDisplayDevices(
    __in_opt LPCTSTR         lpDevice,
    DWORD           iDevNum,
    __inout PDISPLAY_DEVICE lpDisplayDevice,
    DWORD           dwFlags
    )
{
    BOOL fReturnValue = FALSE;

    if ( (NULL == lpDisplayDevice) ||
            (sizeof(DISPLAY_DEVICE) > lpDisplayDevice->cb) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    fReturnValue = pGweApiSet2Entrypoints->m_pEnumDisplayDevices(lpDevice, iDevNum, lpDisplayDevice, lpDisplayDevice->cb, dwFlags);

leave:
    return fReturnValue;
}



//xref ApiTrapWrapper
extern "C"
BOOL
EnumDisplaySettings(
    __in_opt LPCTSTR    lpszDeviceName,
    DWORD      iModeNum,
    __inout LPDEVMODEW lpDevMode
    )
{
    BOOL fReturnValue = FALSE;

    size_t cbDevMode = 0;

    if ( (NULL == lpDevMode) ||
         (sizeof(DEVMODEW) > lpDevMode->dmSize) ||
         FAILED(UIntAdd(lpDevMode->dmSize, lpDevMode->dmDriverExtra, &cbDevMode)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    fReturnValue = pGweApiSet2Entrypoints->m_pEnumDisplaySettings(lpszDeviceName, iModeNum, lpDevMode, cbDevMode);

leave:
    return fReturnValue;
}



//xref ApiTrapWrapper
extern "C"
BOOL
EnumDisplayMonitors(
    HDC             hdc,
    __in_opt LPCRECT         lprc,
    __in MONITORENUMPROC lpfnEnum,
    LPARAM          dwData
    )
{
    return pGweApiSet2Entrypoints->m_pEnumDisplayMonitors(hdc,lprc,sizeof(RECT),lpfnEnum,dwData);
}



//xref ApiTrapWrapper
extern "C"
HMONITOR
MonitorFromPoint(
    POINT   pt,
    DWORD   dwFlags
    )
{
    return pGweApiSet2Entrypoints->m_pMonitorFromPoint(pt, dwFlags);
}



//xref ApiTrapWrapper
extern "C"
HMONITOR
MonitorFromRect(
    __in LPCRECT lprc,
    DWORD   dwFlags
    )
{
    return pGweApiSet2Entrypoints->m_pMonitorFromRect(lprc, sizeof(RECT), dwFlags);
}



//xref ApiTrapWrapper
extern "C"
HMONITOR
MonitorFromWindow(
    HWND    hwnd,
    DWORD   dwFlags
    )
{
    return pGweApiSet2Entrypoints->m_pMonitorFromWindow(hwnd, dwFlags);
}

extern "C"
LONG
ChangeDisplaySettingsEx(
    __in_opt LPCTSTR lpszDeviceName,
    __inout_opt LPDEVMODE lpDevMode,
    HWND hwnd,
    DWORD dwflags,
    __in_opt LPVOID lParam)
{
    LONG lReturnValue = DISP_CHANGE_FAILED;

    size_t cbDevMode = sizeof(DEVMODE);

    if ( (NULL != lpDevMode) &&
            ((sizeof(DEVMODE) != lpDevMode->dmSize) ||
                FAILED(UIntAdd(lpDevMode->dmSize, lpDevMode->dmDriverExtra, &cbDevMode))
                )
            )
        {
        lReturnValue = DISP_CHANGE_BADMODE;
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    lReturnValue = pGweApiSet2Entrypoints->m_pChangeDisplaySettingsEx(
                                            lpszDeviceName,
                                            lpDevMode,
                                            cbDevMode,
                                            hwnd,
                                            dwflags,
                                            lParam,
                                            ((dwflags & CDS_VIDEOPARAMETERS) ? sizeof(VIDEOPARAMETERS) : 0) );

leave:
    return lReturnValue;
}

//xref ApiTrapWrapper
extern "C"
UINT
GetMessageSource(
    VOID
    )
{
    return pGweApiSet1Entrypoints->m_pGetMessageSource();
}



//xref ApiTrapWrapper
extern "C"
HHOOK
SetWindowsHookExW(
    int         idHook,
    HOOKPROC    lpfn,
    HINSTANCE   hmod,
    DWORD       dwThreadId
    )
{
    return pGweApiSet2Entrypoints->m_pSetWindowsHookExW(idHook, lpfn, hmod, dwThreadId);
}



//xref ApiTrapWrapper
extern "C"
BOOL
UnhookWindowsHookEx(
    HHOOK   hhk
    )
{
    return pGweApiSet2Entrypoints->m_pUnhookWindowsHookEx(hhk);
}


extern "C"
LRESULT
WINAPI
CallNextHookEx(
    HHOOK   hhk,
    int     nCode,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return 0;
}





//xref ApiTrapWrapper
extern "C"
BOOL
RegisterHotKey(
    HWND    hWnd,
    int     id,
    UINT    fsModifiers,
    UINT    vk
    )
{
    return pGweApiSet1Entrypoints->m_pRegisterHotKey(hWnd, id, fsModifiers, vk, FALSE); // fPostIMEProcessedHotkey = FALSE
}

//xref ApiTrapWrapper
extern "C"
BOOL
RegisterPostIMEProcessedHotkey(
    HWND    hWnd,
    int     id,
    UINT    fsModifiers,
    UINT    vk
    )
{
    return pGweApiSet1Entrypoints->m_pRegisterHotKey(hWnd, id, fsModifiers, vk, TRUE); // fPostIMEProcessedHotkey = TRUE
}

//xref ApiTrapWrapper
extern "C"
BOOL
UnregisterHotKey(
    HWND    hWnd,
    int     id
    )
{
    return pGweApiSet1Entrypoints->m_pUnregisterHotKey(hWnd, id);
}


//xref ApiTrapWrapper
extern "C"
BOOL
UnregisterFunc1(
    UINT    fsModifiers,
    UINT    vk
    )
{
    return pGweApiSet1Entrypoints->m_pUnregisterFunc1(fsModifiers,vk);
}


//xref ApiTrapWrapper
extern "C"
BOOL
AllKeys(
    BOOL    bAllKeys
    )
{
    return pGweApiSet2Entrypoints->m_pAllKeys(bAllKeys);
}


//xref ApiTrapWrapper
extern "C"
void
AccessibilitySoundSentryEvent(
    void
    )
{
    pGweApiSet2Entrypoints->m_pAccessibilitySoundSentryEvent();
}


//xref ApiTrapWrapper
extern "C"
int
SetWindowRgn(
    HWND    hwnd,
    HRGN    hrgn,
    BOOL    bRedraw
    )
{
    return pGweApiSet2Entrypoints->m_pSetWindowRgn(hwnd, hrgn, bRedraw);
}


//xref ApiTrapWrapper
extern "C"
int
GetWindowRgn(
    HWND    hwnd,
    HRGN    hrgn
    )
{
    return pGweApiSet2Entrypoints->m_pGetWindowRgn(hwnd, hrgn);
}


//xref ApiTrapWrapper
extern "C"
HWND
GetDesktopWindow(
    void
    )
{
    return pGweApiSet2Entrypoints->m_pGetDesktopWindow();
}


//xref ApiTrapWrapper
extern "C"
BOOL
DestroyCursor(
    HCURSOR hCursor
    )
{
    return pGweApiSet1Entrypoints->m_pDestroyCursor(hCursor);
}


//xref ApiTrapWrapper
extern "C"
HCURSOR
GetCursor(
    VOID
    )
{
    return pGweApiSet1Entrypoints->m_pGetCursor();
}


//xref ApiTrapWrapper
extern "C"
BOOL
GetCursorPos(
    __out LPPOINT lpPoint
    )
{
    return pGweApiSet1Entrypoints->m_pGetCursorPos(lpPoint, sizeof(POINT));
}

extern "C"
HCURSOR
LoadCursorW(
    HINSTANCE   hInstance,
    __in const WCHAR *  pCursorName
    )
{
    DWORD CursorNameResourceID = 0;

    if( IS_RESOURCE_ID(pCursorName) )
        {
        CursorNameResourceID  = (DWORD)pCursorName;
        pCursorName = NULL;
        }
    
    return pGweApiSet1Entrypoints->m_pLoadCursorW( hInstance, CursorNameResourceID, pCursorName);
}


//xref ApiTrapWrapper
extern "C"
HCURSOR
LoadAnimatedCursor(
    HINSTANCE   hInstance,
    DWORD       ResourceId,
    int         cFrames,
    int         FrameTimeInterval
    )
{
    return pGweApiSet2Entrypoints->m_pLoadAnimatedCursor(hInstance, ResourceId, cFrames, FrameTimeInterval);
}


//xref ApiTrapWrapper
extern "C"
BOOL
SetCursorPos(
    int X,
    int Y
    )
{
    return pGweApiSet1Entrypoints->m_pSetCursorPos(X, Y);
}


//xref ApiTrapWrapper
extern "C"
int
ShowCursor(
    BOOL bShow
    )
{
    return pGweApiSet1Entrypoints->m_pShowCursor(bShow);
}


//xref ApiTrapWrapper
extern "C"
HCURSOR
CreateCursor(
    HINSTANCE   hInst,
    int         xHotSpot,
    int         yHotSpot,
    int         nWidth,
    int         nHeight,
    __in CONST VOID* pvANDPlane,
    __in CONST VOID* pvXORPlane
    )
{
    HCURSOR hcursorReturnValue = NULL;

    unsigned int cbBytesPerPlane = 0;
    unsigned int cbPerLine = 0;
    unsigned int nHeightLocal = 0;

    if ( (nWidth != GetSystemMetrics(SM_CXCURSOR)) ||
         (nHeight != GetSystemMetrics(SM_CYCURSOR)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    if( FAILED(IntToUInt(nWidth >> 3, &cbPerLine)) ||
        FAILED(IntToUInt(nHeight, &nHeightLocal)) ||
        FAILED(UIntMult(cbPerLine, nHeightLocal, &cbBytesPerPlane)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    hcursorReturnValue = pGweApiSet1Entrypoints->m_pCreateCursor(
                                                        hInst,
                                                        xHotSpot,
                                                        yHotSpot,
                                                        nWidth,
                                                        nHeight,
                                                        pvANDPlane,
                                                        cbBytesPerPlane,
                                                        pvXORPlane,
                                                        cbBytesPerPlane
                                                        );

leave:
    return hcursorReturnValue;
}



extern "C" {





UINT
RegisterWindowMessageW(
    __in LPCTSTR lpString
    )
{
    return pGweApiSet1Entrypoints->m_pRegisterWindowMessageW(lpString);
}




HHOOK
QASetWindowsJournalHook(
    int         nFilterType,
    HOOKPROC    pfnFilterProc,
    EVENTMSG*   pfnEventMsg
    )
{
    return pGweApiSet1Entrypoints->m_pQASetWindowsJournalHook(nFilterType, pfnFilterProc, pfnEventMsg, sizeof(EVENTMSG));
}

BOOL
QAUnhookWindowsJournalHook(
    int nFilterType
    )
{
    return pGweApiSet1Entrypoints->m_pQAUnhookWindowsJournalHook(nFilterType);
}   

BOOL
GetMessageWNoWait(
    __out MSG*    pMsg,
    HWND    hWnd,
    UINT    wMsgFilterMin,
    UINT    wMsgFilterMax
    )
{
    return pGweApiSet1Entrypoints->m_pGetMessageWNoWait(pMsg, sizeof(MSG), hWnd, wMsgFilterMin, wMsgFilterMax );
}

BOOL
TouchCalibrate(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pTouchCalibrate();
}


int
DisableCaretSystemWide(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pDisableCaretSystemWide();
}

int
EnableCaretSystemWide(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pEnableCaretSystemWide();
}


BOOL
EnableHardwareKeyboard(
    BOOL    fEnable
    )
{
    return pGweApiSet1Entrypoints->m_pEnableHardwareKeyboard(fEnable);
}

KEY_STATE_FLAGS
GetAsyncShiftFlags(
    UINT    VKey
    )
{
    return pGweApiSet1Entrypoints->m_pGetAsyncShiftFlags(VKey);
}



DWORD MsgWaitForMultipleObjectsEx(
        DWORD nCount,
        __in_ecount(nCount) LPHANDLE pHandles,
        DWORD dwMilliseconds,
        DWORD dwWakeMask,
        DWORD dwFlags
        )
{
    DWORD dw = 0xFFFFFFFF;
    size_t cbHandles = 0;

    if (FAILED(UIntMult(nCount, sizeof(HANDLE), &cbHandles)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    dw = pGweApiSet1Entrypoints->m_pMsgWaitForMultipleObjectsEx(
                                      nCount,
                                      pHandles,
                                      cbHandles,
                                      dwMilliseconds,
                                      dwWakeMask,
                                      dwFlags);

leave:
    return dw;
}

void
WINAPI
SetAssociatedMenu(
    HWND    hwnd,
    HMENU   hmenu
    )
{
    pGweApiSet1Entrypoints->m_pSetAssociatedMenu(hwnd, hmenu);
}

HMENU
WINAPI
GetAssociatedMenu(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pGetAssociatedMenu(hwnd);
}


BOOL
DrawMenuBar(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pDrawMenuBar(hwnd);
}

BOOL
SetSysColors(
    int         cElements,
    __in_ecount(cElements) const   int*        lpaElements,
    __in_ecount(cElements) const   COLORREF*   lpaRgbValues
    )
{
    BOOL fReturnValue = FALSE;

    unsigned int cElementsLocal = 0;
    size_t cbaElements = 0;
    size_t cbaRgbValues = 0;

    if (FAILED(IntToUInt(cElements, &cElementsLocal)) ||
        FAILED(UIntMult(cElementsLocal, sizeof(int), &cbaElements)) ||
        FAILED(UIntMult(cElementsLocal, sizeof(COLORREF), &cbaRgbValues)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    fReturnValue = pGweApiSet1Entrypoints->m_pSetSysColors(cElements, lpaElements, cbaElements, lpaRgbValues, cbaRgbValues);

leave:
    return fReturnValue;
}

BOOL
DrawFrameControl(
    HDC     hdc,
    __in RECT*   lprc,
    UINT    uType,
    UINT    uState
    )
{
    return pGweApiSet1Entrypoints->m_pDrawFrameControl(hdc, lprc, sizeof(RECT), uType, uState);
}

BOOL
ClipCursor(
    __in_opt const   RECT*   lpRect
    )
{
    return pGweApiSet1Entrypoints->m_pClipCursor(lpRect, sizeof(RECT));
}

}


extern "C"
BOOL
EndDialog(
    HWND    hDlg,
    int     nResult
    )
{
    return pGweApiSet1Entrypoints->m_pEndDialog(hDlg, nResult);
}

extern "C"
HWND
GetDlgItem(
    HWND    hDlg,
    int     CtrlID
    )
{
    return pGweApiSet1Entrypoints->m_pGetDlgItem(hDlg, CtrlID);
}


int
SetScrollPos(
    HWND    hwnd,
    int     fnBar,
    int     nPos,
    BOOL    fRedraw
    )
{
    return pGweApiSet1Entrypoints->m_pSetScrollPos(hwnd, fnBar, nPos, fRedraw);
}

int
SetScrollRange(
    HWND    hwnd,
    int     fnBar,
    int     nMinPos,
    int     nMaxPos,
    BOOL    fRedraw
    )
{
    return pGweApiSet1Entrypoints->m_pSetScrollRange(hwnd, fnBar, nMinPos, nMaxPos, fRedraw);
}

extern "C"
int
GetScrollInfo(
    HWND        hwnd,
    int         fnBar,
    __inout SCROLLINFO* lpsi
    )
{
    return pGweApiSet1Entrypoints->m_pGetScrollInfo(hwnd, fnBar, lpsi, sizeof(SCROLLINFO));
}



UINT
MapVirtualKeyW(
    UINT    uCode,
    UINT    uMapType
    )
{
    if ( WAIT_OBJECT_0 != WaitForAPIReady(SH_WMGR, 0) )
        {
        SetLastError(ERROR_NOT_READY);
        return 0;
        }

    return pGweApiSet1Entrypoints->m_pMapVirtualKeyW(uCode, uMapType);
}

int
GetClassNameW(
    HWND    hWnd,
    __out_ecount(nMaxCount) WCHAR*  lpClassName,
    int     nMaxCount
    )
{
    int nReturnValue = 0;
    unsigned int nMaxCountLocal = 0;
    size_t  cbClassName = 0;

    if (FAILED(IntToUInt(nMaxCount, &nMaxCountLocal)) ||
        FAILED(UIntMult(nMaxCountLocal, sizeof(WCHAR), &cbClassName)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    nReturnValue = pGweApiSet1Entrypoints->m_pGetClassNameW(hWnd, lpClassName, cbClassName, nMaxCount);

leave:
    return nReturnValue;
}

extern "C"
int
MapWindowPoints(
    HWND    hwndFrom,
    HWND    hwndTo,
    __inout_ecount(cPoints) POINT*  ppt,
    UINT    cPoints
    )
{
    int nReturnValue = 0;
    size_t cbpt = 0;

    if (FAILED(UIntMult(cPoints, sizeof(POINT), &cbpt)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    nReturnValue = pGweApiSet1Entrypoints->m_pMapWindowPoints(hwndFrom, hwndTo, ppt, cbpt, cPoints);

leave:
    return nReturnValue;
}

extern "C"
HANDLE
LoadImageW(
    HINSTANCE       hInstance,
    __in const WCHAR*     pImageName,
    unsigned int    uType,
    int             cxDesired,
    int             cyDesired,
    unsigned int    fuLoad
    )
{
    DWORD ImageNameResourceID = 0;

    if( IS_RESOURCE_ID(pImageName) )
        {
        ImageNameResourceID  = reinterpret_cast<DWORD>(pImageName);
        pImageName = NULL;
        }
    
    return pGweApiSet1Entrypoints->m_pLoadImageW(hInstance, ImageNameResourceID, pImageName, uType, cxDesired, cyDesired, fuLoad);
}

HWND
GetForegroundWindow(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetForegroundWindow();
}

BOOL
SetForegroundWindow(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pSetForegroundWindow(hWnd);
}

HWND
SetActiveWindow(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pSetActiveWindow(hwnd);
}

BOOL
CreateCaret(
    HWND    hWnd,
    HBITMAP hBitmap,
    int     nWidth,
    int     nHeight
    )
{
    return pGweApiSet1Entrypoints->m_pCreateCaret(hWnd, hBitmap, nWidth, nHeight);
}

BOOL
DestroyCaret(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pDestroyCaret();
}

BOOL
HideCaret(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pHideCaret(hWnd);
}

BOOL
ShowCaret(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pShowCaret(hWnd);
}

BOOL
SetCaretPos(
    int X,
    int Y
    )
{
    return pGweApiSet1Entrypoints->m_pSetCaretPos(X,Y);
}

BOOL
GetCaretPos(
    __out POINT*  lpPoint
    )
{
    return pGweApiSet1Entrypoints->m_pGetCaretPos(lpPoint, sizeof(POINT));
}

BOOL
SetCaretBlinkTime(
    UINT    uMSeconds
    )
{
    return pGweApiSet1Entrypoints->m_pSetCaretBlinkTime(uMSeconds);
}

UINT
GetCaretBlinkTime(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetCaretBlinkTime();
}

extern "C"
BOOL
GetClassInfoW(
    HINSTANCE   hInstance,
    __in const   WCHAR*      pClassName,
    __out WNDCLASSW*  pWndClass
    )
{
    ATOM ClassIdentifier = 0;
    BOOL ReturnValue = FALSE;

    if( !pClassName )
        {
        SetLastError(ERROR_CLASS_DOES_NOT_EXIST);
        goto leave;
        }

    if( !HIWORD(pClassName) )
        {
        ClassIdentifier = static_cast<ATOM>(LOWORD(pClassName));
        pClassName = NULL;
        }

    ReturnValue = pGweApiSet1Entrypoints->m_pGetClassInfoW(hInstance, ClassIdentifier, pClassName, pWndClass, sizeof(WNDCLASSW));
    
leave:
    return ReturnValue;
}

HWND
GetNextDlgTabItem(
    HWND    hDlg,
    HWND    hCtl,
    BOOL    bPrevious
    )
{
    return pGweApiSet1Entrypoints->m_pGetNextDlgTabItem(hDlg, hCtl, bPrevious);
}

HWND
GetNextDlgGroupItem(
    HWND    hDlg,
    HWND    hCtl,
    BOOL    bPrevious
    )
{
    return pGweApiSet1Entrypoints->m_pGetNextDlgGroupItem(hDlg, hCtl, bPrevious);
}


BOOL
SetDlgItemInt(
    HWND    hDlg,
    int     idControl,
    UINT    uValue,
    BOOL    fSigned
    )
{
    return pGweApiSet1Entrypoints->m_pSetDlgItemInt(hDlg, idControl, uValue, fSigned);
}

UINT
GetDlgItemInt(
    HWND    hDlg,
    int     idControl,
    __out_opt BOOL*   lpSuccess,
    BOOL    fSigned
    )
{
    return pGweApiSet1Entrypoints->m_pGetDlgItemInt(hDlg, idControl, lpSuccess, fSigned);
}

BOOL
CheckRadioButton(
    HWND    hDlg,
    int     idFirst,
    int     idLast,
    int     idCheck
    )
{
    return pGweApiSet1Entrypoints->m_pCheckRadioButton(hDlg, idFirst, idLast, idCheck);
}

LONG
SendDlgItemMessageW(
    HWND    hDlg,
    int     nIDDlgItem,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return pGweApiSet1Entrypoints->m_pSendDlgItemMessageW(hDlg,nIDDlgItem,Msg,wParam,lParam);
}

BOOL
SetDlgItemTextW(
    HWND    hDlg,
    int     nIDDlgItem,
    __in_opt const   WCHAR*  lpString
    )
{
    return pGweApiSet1Entrypoints->m_pSetDlgItemTextW(hDlg,nIDDlgItem,lpString);
}

UINT
GetDlgItemTextW(
    HWND    hDlg,
    int     nIDDlgItem,
    __out_ecount(nMaxCount) WCHAR*  lpString,
    int     nMaxCount
    )
{
    UINT iResult = 0;

    unsigned int nMaxCountLocal = 0;
    size_t cbString = 0;

    if (FAILED(IntToUInt(nMaxCount, &nMaxCountLocal)) ||
        FAILED(UIntMult(nMaxCountLocal, sizeof(WCHAR), &cbString)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    iResult = pGweApiSet1Entrypoints->m_pGetDlgItemTextW(hDlg,nIDDlgItem,lpString,cbString,nMaxCount);

leave:
    return iResult;
}

HWND
FindWindowW(
    __in const   WCHAR*  pClassName,
    __in_opt const   WCHAR*  pWindowName
    )
{
    ATOM ClassIdentifier = 0;

    if( !HIWORD(pClassName) )
        {
        ClassIdentifier = static_cast<ATOM>(LOWORD(pClassName));
        pClassName = NULL;
        }

    return pGweApiSet1Entrypoints->m_pFindWindowW( ClassIdentifier, pClassName, pWindowName);
}

UINT
SetTimer(
    HWND        hwnd,
    UINT        idTimer,
    UINT        uTimeOut,
    TIMERPROC   pfnTimerProc
    )
{
    return pGweApiSet1Entrypoints->m_pSetTimer(hwnd, idTimer, uTimeOut, pfnTimerProc);
}

BOOL
KillTimer(
    HWND    hwnd,
    UINT    idEvent
    )
{
    return pGweApiSet1Entrypoints->m_pKillTimer(hwnd, idEvent);
}

extern "C"
HICON
ExtractIconExW(
    __in LPCWSTR lpszExeFileName,
    int     nIconIndex,
    __out_opt HICON*  phiconLarge,
    __out_opt HICON*  phiconSmall,
    UINT    nIcons
    )
{
    return pGweApiSet1Entrypoints->m_pExtractIconExW(
            lpszExeFileName,
            nIconIndex,
            phiconLarge,
            phiconSmall,
            nIcons
            );
}

HMENU
CreateMenu(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pCreateMenu();
}

extern "C"
HMENU
GetSubMenu(
    HMENU   hmenu,
    int     nPos
    )
{
    return pGweApiSet1Entrypoints->m_pGetSubMenu(hmenu, nPos);
}


extern "C"
BOOL
EnableWindow(
    HWND    hwnd,
    BOOL    bEnable
    )
{
    return pGweApiSet1Entrypoints->m_pEnableWindow(hwnd, bEnable);
}

BOOL
IsWindowEnabled(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pIsWindowEnabled(hwnd);
}



BOOL
ScrollDC(
    HDC hDC,
    int dx,
    int dy,
    __in_opt const   RECT*   lprcScroll,
    __in_opt const   RECT*   lprcClip,
    HRGN    hrgnUpdate,
    __out_opt RECT*   lprcUpdate
    )
{
    return pGweApiSet1Entrypoints->m_pScrollDC(hDC, dx, dy, lprcScroll, sizeof(RECT), lprcClip, sizeof(RECT), hrgnUpdate, lprcUpdate, sizeof(RECT));
}

int
ScrollWindowEx(
    HWND    hWnd,
    int     dx,
    int     dy,
    __in_opt const RECT*  prcScroll,
    __in const RECT*  prcClip,
    HRGN    hrgnUpdate,
    __out_opt RECT*       prcUpdate,
    UINT    flags
    )
{
    return pGweApiSet1Entrypoints->m_pScrollWindowEx(hWnd, dx, dy, prcScroll, sizeof(RECT), prcClip, sizeof(RECT), hrgnUpdate, prcUpdate, sizeof(RECT), flags);
}

BOOL
OpenClipboard(
    HWND    hWndNewOwner
    )
{
    return pGweApiSet1Entrypoints->m_pOpenClipboard(hWndNewOwner);
}

BOOL
CloseClipboard(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pCloseClipboard();
}

HWND
GetClipboardOwner(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetClipboardOwner();
}

HANDLE
SetClipboardData(
    UINT    uFormat,
    HANDLE  hMem
    )
{
    return pGweApiSet1Entrypoints->m_pSetClipboardData(uFormat, hMem, LocalSize(hMem));
}

HANDLE
GetClipboardData(
    UINT    uFormat
    )
{
    return pGweApiSet1Entrypoints->m_pGetClipboardDataGwe(
                uFormat,
                FALSE
                );
}



int
CountClipboardFormats(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pCountClipboardFormats();
}

UINT
EnumClipboardFormats(
    UINT    format
    )
{
    return pGweApiSet1Entrypoints->m_pEnumClipboardFormats(format);
}

int
GetClipboardFormatNameW(
    UINT    format,
    __out_ecount(cchMaxCount) WCHAR*  lpszFormatName,
    int     cchMaxCount
    )
{
    int nReturnValue = 0;
    unsigned int cchMaxCountLocal = 0;
    size_t cbszFormatName = 0;

    if (FAILED(IntToUInt(cchMaxCount, &cchMaxCountLocal)))
        {
        // Same error as desktop
        SetLastError(ERROR_NOACCESS);
        goto leave;
        }

    if (FAILED(UIntMult(cchMaxCountLocal, sizeof(WCHAR), &cbszFormatName)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    nReturnValue = pGweApiSet1Entrypoints->m_pGetClipboardFormatNameW(format, lpszFormatName, cbszFormatName, cchMaxCount);

leave:
    return nReturnValue;
}

BOOL
EmptyClipboard(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pEmptyClipboard();
}

BOOL
IsClipboardFormatAvailable(
    UINT    format
    )
{
    return pGweApiSet1Entrypoints->m_pIsClipboardFormatAvailable(format);
}

int
GetPriorityClipboardFormat(
    __in_ecount(cFormats) UINT*   paFormatPriorityList,
    int     cFormats
    )
{
    int nReturnValue = 0;
    unsigned int cFormatsLocal = 0;
    size_t cbFormatPriorityList = 0;

    if (FAILED(IntToUInt(cFormats, &cFormatsLocal)))
        {
        // Same error as desktop
        SetLastError(ERROR_NOACCESS);
        goto leave;
        }

    if (FAILED(UIntMult(cFormatsLocal, sizeof(UINT), &cbFormatPriorityList)))
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    nReturnValue = pGweApiSet1Entrypoints->m_pGetPriorityClipboardFormat(paFormatPriorityList, cbFormatPriorityList, cFormats);

leave:
    return nReturnValue;
}

HWND
GetOpenClipboardWindow(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetOpenClipboardWindow();
}

BOOL
MessageBeep(
    UINT    uType
    )
{
    return pGweApiSet1Entrypoints->m_pMessageBeep(uType);
}

HANDLE
GetClipboardDataAlloc(
    UINT    uFormat
    )
{
    return pGweApiSet1Entrypoints->m_pGetClipboardDataGwe(
                uFormat,
                TRUE
                );
}



extern "C"
BOOL
SetMenuItemInfoW(
    HMENU           hMenu,
    UINT            uPosition,
    BOOL            fByPosition,
    __in const   MENUITEMINFO*   lpmii
    )
{
    BOOL fSuccess = FALSE;
    
    if (NULL == lpmii)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
    }
    // lpmii->cbSize checked in GWES PSL
    fSuccess = pGweApiSet1Entrypoints->m_pSetMenuItemInfoW(hMenu, uPosition, fByPosition, lpmii, lpmii->cbSize);
    
leave:
    return fSuccess;
}

BOOL
GetMenuItemInfoW(
    HMENU           hMenu,
    UINT            uPosition,
    BOOL            fByPosition,
    __inout MENUITEMINFO*   lpmii
    )
{
    BOOL fSuccess = FALSE;
    
    if (NULL == lpmii)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
    }
    
    // lpmii->cbSize checked in GWES PSL
    fSuccess = pGweApiSet1Entrypoints->m_pGetMenuItemInfoW(hMenu, uPosition, fByPosition, lpmii, lpmii->cbSize);
    
leave:
    return fSuccess;
}

extern "C"
DWORD
GetMessagePos(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetMessagePos();
}

LRESULT
SendMessageTimeout(
    HWND        hWnd,
    UINT        Msg,
    WPARAM      wParam,
    LPARAM      lParam,
    UINT        fuFlags,
    UINT        uTimeout,
    __out PDWORD_PTR  lpdwResult
    )
{
    return pGweApiSet2Entrypoints->m_pSendMessageTimeout(
        hWnd,
        Msg,
        wParam,
        lParam,
        fuFlags,
        uTimeout,
        lpdwResult
        );
}

DWORD
GetMessageQueueReadyTimeStamp(
    HWND    hWnd
    )
{
    return pGweApiSet2Entrypoints->m_pGetMessageQueueReadyTimeStamp(hWnd);
}

BOOL
SendNotifyMessageW(
    HWND    hwnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return pGweApiSet1Entrypoints->m_pSendNotifyMessageW(hwnd, Msg, wParam, lParam);
}

BOOL
PostThreadMessageW(
    DWORD   idThread,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return pGweApiSet1Entrypoints->m_pPostThreadMessageW(idThread, Msg, wParam, lParam);
}

BOOL
EnumWindows(
    WNDENUMPROC pEnumFunc,
    LPARAM      lParam
    )
{
    return pGweApiSet1Entrypoints->m_pEnumWindows(
                pEnumFunc, 
                lParam
                );
}

BOOL
MapDialogRect(
    HWND    hwnd,
    __inout RECT*   prc
    )
{
    return pGweApiSet1Entrypoints->m_pMapDialogRect(hwnd, prc, sizeof(RECT));
}

LONG
GetDialogBaseUnits(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetDialogBaseUnits();
}

UINT
GetDoubleClickTime(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetDoubleClickTime();
}

extern "C"
DWORD
GetWindowThreadProcessId(
    HWND    hwnd,
    __out_opt DWORD*  lpdwProcessId
    )
{
    return pGweApiSet1Entrypoints->m_pGetWindowThreadProcessId(hwnd, lpdwProcessId);
}

HICON
CreateIconIndirect(
    __in ICONINFO*   piconinfo
    )
{
    return pGweApiSet1Entrypoints->m_pCreateIconIndirect(piconinfo, sizeof(ICONINFO));
}

extern "C"
HCURSOR
SetCursor(
    HCURSOR hCursor
    )
{
    return pGweApiSet1Entrypoints->m_pSetCursor(hCursor);
}

DWORD
GetKeyboardStatus(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetKeyboardStatus();
}

int
GetKeyboardType(
    int nTypeFlag
    )
{
    return pGweApiSet1Entrypoints->m_pGetKeyboardType(nTypeFlag);
}

HKL
GetKeyboardLayout(
    DWORD   idThread
    )
{
    return pGweApiSet1Entrypoints->m_pGetKeyboardLayout(idThread);
}

unsigned int
GetKeyboardLayoutList(
    int     KeybdLayoutCount,
    __out_ecount(KeybdLayoutCount) HKL *pKeybdLayoutList
    )
{
    size_t KeybdLayoutListBufferSize = 0;
    unsigned int ReturnValue = 0;

    if( FAILED(safeIntUMul(KeybdLayoutCount, sizeof(HKL), &KeybdLayoutListBufferSize)) )
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;       
        }

    ReturnValue = pGweApiSet1Entrypoints->m_pGetKeyboardLayoutList(KeybdLayoutCount, pKeybdLayoutList, KeybdLayoutListBufferSize);

leave:
    return ReturnValue;
}

HKL
LoadKeyboardLayoutW(
    __in const WCHAR * pKeybdLayoutID,
    UINT    Flags
    )
{
    return pGweApiSet1Entrypoints->m_pLoadKeyboardLayoutW(pKeybdLayoutID, Flags);
}

BOOL
GetKeyboardLayoutNameW(
    __out WCHAR*  pszNameBuf
    )
{
    // pNameBuf must be a buffer of at least KL_NAMELENGTH characters.
    return pGweApiSet1Entrypoints->m_pGetKeyboardLayoutNameW(pszNameBuf, KL_NAMELENGTH * sizeof(WCHAR));
}

HKL
ActivateKeyboardLayout(
    HKL     hkl,
    UINT    Flags
    )
{
    return pGweApiSet1Entrypoints->m_pActivateKeyboardLayout(hkl, Flags);
}

#ifndef RDPGWELIB
ATOM
GlobalAddAtomW(
    __in const   WCHAR*  pString
    )
{
    ATOM Atom = 0;

    if( !HIWORD(pString) )
        {
        Atom  = static_cast<ATOM>(LOWORD(pString));
        pString = NULL;
        }
    
    return pGweApiSet2Entrypoints->m_pGlobalAddAtomW( Atom, pString);
}

ATOM
GlobalDeleteAtom(
    ATOM    nAtom
    )
{
    return pGweApiSet2Entrypoints->m_pGlobalDeleteAtom(nAtom);
}

ATOM GlobalFindAtomW(
    __in const   WCHAR*  pString
    )
{
    ATOM Atom = 0;

    if( !HIWORD(pString) )
        {
        Atom  = static_cast<ATOM>(LOWORD(pString));
        pString = NULL;
        }
    
    return pGweApiSet2Entrypoints->m_pGlobalFindAtomW( Atom, pString);
}
#endif

/**********   OAK defines *****************/


BOOL
PostKeybdMessageEx(
    HWND    hwnd,
    unsigned int    VKey,
    unsigned int    KeyStateFlags,
    unsigned int    CharacterCount,
    __in_ecount(CharacterCount) unsigned int*  pShiftStateBuffer,
    __in_ecount(CharacterCount) unsigned int*  pCharacterBuffer,
    const GUID *pguidKeyboard
    )
{
    BOOL Success = FALSE;

    unsigned int ShiftStateBufferLen = 0;
    unsigned int CharacterBufferLen = 0;

    if( CharacterCount )
        {
        if( !pShiftStateBuffer || FAILED( safeIntUMul( sizeof(unsigned int), CharacterCount, &ShiftStateBufferLen) ) )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }

        if( !pCharacterBuffer )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }

        CharacterBufferLen = ShiftStateBufferLen;
        }

    Success =  pGweApiSet1Entrypoints->m_pPostKeybdMessage ( 
                    hwnd, 
                    VKey, 
                    KeyStateFlags, 
                    CharacterCount, 
                    pShiftStateBuffer, 
                    ShiftStateBufferLen,
                    pCharacterBuffer,
                    CharacterBufferLen,
                    pguidKeyboard,
                    sizeof(GUID)
                    );

leave:
    return Success;    
}

BOOL
PostKeybdMessage(
    HWND    hwnd,
    unsigned int    VKey,
    unsigned int    KeyStateFlags,
    unsigned int    CharacterCount,
    __in_ecount(CharacterCount) unsigned int*  pShiftStateBuffer,
    __in_ecount(CharacterCount) unsigned int*  pCharacterBuffer
    )
{
    const GUID GUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } }; 

    return PostKeybdMessageEx( hwnd, 
                                                VKey,
                                                KeyStateFlags, 
                                                CharacterCount, 
                                                pShiftStateBuffer, 
                                                pCharacterBuffer, 
                                                &GUID_NULL);
}


HRESULT
GetKeybdGuidFromKeybdMessage (
    HWND    hWnd,
    WPARAM  wParamOfWmChar,
    LPARAM  lParamOfWmChar,
    GUID    *pguidKeyboard
    )
{
    HRESULT hr =  pGweApiSet2Entrypoints->m_pGetKeybdGuidFromKeybdMessage(hWnd, wParamOfWmChar, lParamOfWmChar, pguidKeyboard, sizeof(GUID));

    return hr;
}

BOOL
GetLastInputType(
    DWORD *pdwEvents
    )
{
    return pGweApiSet2Entrypoints->m_pGetLastInputType(pdwEvents);
}

BOOL
SetLastInputType(
    DWORD dwEvents
    )
{
    return pGweApiSet2Entrypoints->m_pSetLastInputType(dwEvents);
}



HRESULT
SetCurrentInputLanguage(
    DWORD dwLCID
    )
{
    HRESULT hr =  pGweApiSet1Entrypoints->m_pSetCurrentInputLanguage(dwLCID);

    return hr;
}

HRESULT
GetCurrentInputLanguage(
    DWORD *pdwLCID
    )
{
    HRESULT hr =  pGweApiSet1Entrypoints->m_pGetCurrentInputLanguage(pdwLCID);

    return hr;
}

BOOL
GetMouseMovePoints(
    __out_ecount(BufferPointsCount) POINT*  pPointBuffer,
    unsigned int    BufferPointsCount,
    unsigned int  *pPointsRetrievedCount
    )
{
    BOOL Success = FALSE;

    size_t PointBufferSize = 0;

    if( FAILED(safeIntUMul(BufferPointsCount, sizeof(POINT), &PointBufferSize)) )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }
    
    Success = pGweApiSet1Entrypoints->m_pGetMouseMovePoints(pPointBuffer, PointBufferSize, BufferPointsCount, pPointsRetrievedCount);

leave:
    return Success;
}

void
SystemIdleTimerReset(
    void
    )
{
    pGweApiSet1Entrypoints->m_pSystemIdleTimerReset();
}

HWND
SetKeyboardTarget(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pSetKeyboardTarget(hwnd);
}

HWND
GetKeyboardTarget(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetKeyboardTarget();
}

void
NotifyWinUserSystem(
    UINT    uEvent
    )
{
    pGweApiSet1Entrypoints->m_pNotifyWinUserSystem(uEvent);
}

BOOL
RegisterTaskBar(
    HWND    hwndTaskBar
    )
{
    return pGweApiSet1Entrypoints->m_pRegisterTaskBar(hwndTaskBar);
}

BOOL
RegisterTaskBarEx(
    HWND    hwndTaskBar,
    BOOL    bTaskBarOnTop
    )
{
    return pGweApiSet2Entrypoints->m_pRegisterTaskBarEx(hwndTaskBar, bTaskBarOnTop);
}

BOOL
RegisterDesktop(
    HWND    hwndDesktop
    )
{
    return pGweApiSet2Entrypoints->m_pRegisterDesktop(hwndDesktop);
}

BOOL
RegisterSIPanel(
    HWND    hwndTaskBar
    )
{
    return pGweApiSet1Entrypoints->m_pRegisterSIPanel(hwndTaskBar);
}


BOOL
RectangleAnimation(
    HWND    hWnd,
    __in const RECT*   prc,
    BOOL    fMaximize
    )
{
    return pGweApiSet1Entrypoints->m_pRectangleAnimation(hWnd, prc, sizeof(RECT), fMaximize);
}

void
ShellModalEnd(
    void
    )
{
    pGweApiSet1Entrypoints->m_pShellModalEnd();
}

void
GwesPowerOffSystem(
    void
    )
{
    pGweApiSet1Entrypoints->m_pGwesPowerOff();
}

BOOL
GwesPowerDown(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGwesPowerDown();
}

void
GwesPowerUp(
    BOOL    fWantsStartupScreen
    )
{
    pGweApiSet1Entrypoints->m_pGwesPowerUp(fWantsStartupScreen);
}

BOOL
KeybdGetDeviceInfo(
    int     iIndex,
    __out void*   lpOutput
    )
{
    return pGweApiSet1Entrypoints->m_pKeybdGetDeviceInfo(iIndex, lpOutput);
}

BOOL
KeybdInitStates(
    __out KEY_STATE   KeyState,
    __out void*       pKeybdDriverState
    )
{
    return pGweApiSet1Entrypoints->m_pKeybdInitStates( KeyState, sizeof(KEY_STATE), pKeybdDriverState );
}   
    
unsigned int
KeybdVKeyToUnicode(
    unsigned int            VKey,
    KEY_STATE_FLAGS  KeyEventFlags,
    __in KEY_STATE             KeyState,
    __in void*               KeybdDriverState,
    unsigned int      BufferSizeCount,
    __out unsigned int*    pcCharacters,
    __out_ecount(BufferSizeCount) KEY_STATE_FLAGS*    pShiftStateBuffer,
    __out_ecount(BufferSizeCount) unsigned int*    pCharacterBuffer
    )
{
    unsigned int ReturnValue = 0;
    unsigned int ShiftStateBufferLen = 0;
    unsigned int CharacterBufferLen = 0;

    if( BufferSizeCount )
        {
        if( !pShiftStateBuffer || FAILED( safeIntUMul(sizeof(KEY_STATE_FLAGS), BufferSizeCount, &ShiftStateBufferLen) ) )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }

        if( !pCharacterBuffer || FAILED( safeIntUMul(sizeof(DWORD), BufferSizeCount, &CharacterBufferLen) ) )
            {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto leave;
            }
        }

    ReturnValue =  pGweApiSet1Entrypoints->m_pKeybdVKeyToUnicode(
            VKey,
            KeyEventFlags,
            KeyState,
            sizeof(KEY_STATE),
            KeybdDriverState,
            BufferSizeCount,
            pcCharacters,            
            pShiftStateBuffer,
            ShiftStateBufferLen,
            pCharacterBuffer,
            CharacterBufferLen
            );

leave:

    return ReturnValue;
}

BOOL
SetProp(
    HWND     hWnd,
    __in const WCHAR * pPropertyName,
    HANDLE   hData
    )
{
    ATOM PropertyIdentifier = 0;

    if( !HIWORD(pPropertyName) )
        {
        PropertyIdentifier = static_cast<ATOM>(LOWORD(pPropertyName));
        pPropertyName = NULL;
        }
    
    return pGweApiSet2Entrypoints->m_pSetProp(hWnd, PropertyIdentifier, pPropertyName, hData);
}


HANDLE
GetProp(
    HWND     hWnd,
    __in const WCHAR * pPropertyName
    )
{
    ATOM PropertyIdentifier = 0;

    if( !HIWORD(pPropertyName) )
        {
        PropertyIdentifier  = static_cast<ATOM>(LOWORD(pPropertyName));
        pPropertyName = NULL;
        }
    
    return pGweApiSet2Entrypoints->m_pGetProp(hWnd, PropertyIdentifier, pPropertyName);
}

HANDLE
RemoveProp(
    HWND     hWnd,
    __in const WCHAR * pPropertyName
    )
{
    ATOM PropertyIdentifier = 0;

    if( !HIWORD(pPropertyName) )
        {
        PropertyIdentifier  = static_cast<ATOM>(LOWORD(pPropertyName));
        pPropertyName = NULL;
        }
    
    return pGweApiSet2Entrypoints->m_pRemoveProp(hWnd, PropertyIdentifier, pPropertyName);
}

int
EnumPropsEx(
    HWND            hWnd,
    PROPENUMPROCEX  lpEnumFunc,
    LPARAM          lParam
    )
{
    return pGweApiSet2Entrypoints->m_pEnumPropsEx(hWnd, lpEnumFunc, lParam);
}


extern "C"
BOOL
PostMessageW(
    HWND    hwnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return pGweApiSet1Entrypoints->m_pPostMessageW(hwnd, Msg, wParam, lParam);
}

extern "C"
LRESULT
CallWindowProcW(
    WNDPROC WndProc,
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return pGweApiSet1Entrypoints->m_pCallWindowProcW(
            WndProc,
            hWnd,
            Msg,
            wParam,
            lParam
            );
}


MsgQueue*
WINAPI
GetCurrentThreadMessageQueue(
    void
    )
{
    return pGweApiSet2Entrypoints->m_pGetCurrentThreadMessageQueue();
}


LRESULT
SendMessageW(
    HWND    hwnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    return pGweApiSet1Entrypoints->m_pSendMessageW(hwnd,Msg,wParam,lParam);
}

BOOL
GetMessageW(
    __out MSG*    pMsgr,
    HWND    hwnd,
    UINT    wMsgFilterMin,
    UINT    wMsgFilterMax
    )
{
    return pGweApiSet1Entrypoints->m_pGetMessageW(pMsgr,sizeof(MSG),hwnd,wMsgFilterMin,wMsgFilterMax);
}



LONG
DispatchMessageW(
    __in CONST   MSG*    pMsg
    )
{
    LRESULT lRet = 0;

    if ( !pMsg || !pMsg->hwnd )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    lRet = pGweApiSet1Entrypoints->m_pDispatchMessageW(pMsg,sizeof(MSG));

leave:
    return lRet;
}

extern "C"
BOOL
RegisterForwardKeys(
    HWND hwnd,
    UINT vkStart,
    UINT vkEnd,
    BOOL fInherited
    )
{
    return pGweApiSet2Entrypoints->m_pRegisterForwardKeys(hwnd, vkStart, vkEnd, fInherited);
}

extern "C"
BOOL
UnregisterForwardKeys(
    HWND hwnd,
    UINT vkStart,
    UINT vkEnd
    )
{
    return pGweApiSet2Entrypoints->m_pUnregisterForwardKeys(hwnd, vkStart, vkEnd);
}

extern "C"
BOOL
IsForwardKey(
    HWND hwnd,
    UINT vk
    )
{
    return pGweApiSet2Entrypoints->m_pIsForwardKey(hwnd, vk);
}

extern "C"
BOOL
ForwardKeyMessages(
    HWND  hwnd
    )
{
    return pGweApiSet2Entrypoints->m_pForwardKeyMessages(hwnd);
}

extern "C"
BOOL
WINAPI
StartKeyForwarding(
    VOID
    )
{
    return pGweApiSet2Entrypoints->m_pStartKeyForwarding();
}

extern "C"
BOOL
WINAPI
StopKeyForwarding (
    VOID
    )
{
    return pGweApiSet2Entrypoints->m_pStopKeyForwarding();
}

extern "C"
BOOL
SetWindowPos(
    HWND    hwnd,
    HWND    hwndInsertAfter,
    int     x,
    int     y,
    int     dx,
    int     dy,
    UINT    fuFlags
    )
{
    return pGweApiSet1Entrypoints->m_pSetWindowPos(hwnd,hwndInsertAfter,x,y,dx,dy,fuFlags);
}

extern "C"
BOOL
GetWindowRect(
    HWND    hwnd,
    __out RECT*   prc
    )
{
    return pGweApiSet1Entrypoints->m_pGetWindowRect(hwnd,prc,sizeof(RECT));
}

extern "C"
BOOL
GetClientRect(
    HWND    hwnd,
    __out RECT*   prc
    )
{
    return pGweApiSet1Entrypoints->m_pGetClientRect(hwnd,prc,sizeof(RECT));
}

BOOL
InvalidateRect(
    HWND    hwnd,
    __in_opt const RECT*   prc,
    BOOL    fErase
    )
{
    return pGweApiSet1Entrypoints->m_pInvalidateRect(hwnd,prc,sizeof(RECT),fErase);
}

HWND
GetWindow(
    HWND    hwnd,
    UINT    uCmd
    )
{
    return pGweApiSet1Entrypoints->m_pGetWindow(hwnd,uCmd);
}


extern "C"
int
GetSystemMetrics(
    int nIndex
    )
{
static const    UINT8*  s_prgDefSystemMetrics = NULL;

    int nRet = 0;

    //  Store the pointer in a local variable to make this code reentrant-safe
    if ( !s_prgDefSystemMetrics )
        {
        pGweApiSet1Entrypoints->m_pGweCoreDllInfoFn( GWEINFO_DEFAULT_SYSTEM_METRICS , reinterpret_cast<void*>(&s_prgDefSystemMetrics));
        }

//REVIEW this shouldn't be defined here.
    #define SM_LASTMETRIC 82

    if ( ( nIndex < 0 ) ||
         ( nIndex >= SM_LASTMETRIC ) )
        {
        goto leave;
        }

    if ( NULL != s_prgDefSystemMetrics )
        {
        nRet = s_prgDefSystemMetrics[nIndex];
        }
    else
        {
        nRet = 255;
        }

    //  255 means don't know the value, need to check elsewhere.
    if ( nRet == 255 )
        {
        nRet = pGweApiSet1Entrypoints->m_pGetSystemMetrics(nIndex);
        }

leave:
    return nRet;
}

BOOL
ClientToScreen(
    HWND    hwnd,
    __inout POINT*  lpPoint
    )
{
    return pGweApiSet1Entrypoints->m_pClientToScreen(hwnd, lpPoint, sizeof(POINT));
}



extern "C"
LONG
SetWindowLongW(
    HWND    hwnd,
    int     Index,
    LONG    NewValue
    )
{
    return pGweApiSet1Entrypoints->m_pSetWindowLongW(hwnd,Index,NewValue);
}

extern "C"
LONG
WINAPI
GetWindowLongW(
    HWND    hwnd,
    int     nIndex
    )
{
    return pGweApiSet1Entrypoints->m_pGetWindowLongW(hwnd,nIndex);
}

extern "C"
HDC
BeginPaint(
    HWND            hwnd,
    __out PAINTSTRUCT*    pps
    )
{
    return pGweApiSet1Entrypoints->m_pBeginPaint(hwnd,pps,sizeof(PAINTSTRUCT));
}


extern "C"
BOOL
EndPaint(
    HWND            hwnd,
    __in PAINTSTRUCT*    pps
    )
{
    return pGweApiSet1Entrypoints->m_pEndPaint(hwnd,pps,sizeof(PAINTSTRUCT));
}

extern "C"
HDC
GetDC(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pGetDC(hwnd);
}



extern "C"
BOOL
ReleaseDC(
    HWND    hwnd,
    HDC     hdc
    )
{
    return pGweApiSet1Entrypoints->m_pReleaseDC(hwnd,hdc);
}


LRESULT
DefWindowProcW(
    HWND    hwnd,
    UINT    msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    switch ( msg )
        {
        case WM_ACTIVATE:
            if ( wParam )
                {
                SetFocus(hwnd);
                }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_STYLECHANGED:
            SetWindowPos(hwnd,0,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            return 0;

        case WM_WINDOWPOSCHANGED:
        case WM_SETREDRAW:
        case WM_QUERYNEWPALETTE:
        case WM_GETTEXT:
        case WM_SETTEXT:
        case WM_GETICON:
        case WM_SETICON:
        case WM_GETTEXTLENGTH:
        case WM_ERASEBKGND:
        case WM_PAINT:
        case WM_CANCELMODE:
        case WM_NCPAINT:
        case WM_NCCREATE:
        case WM_NCDESTROY:
        case WM_NCACTIVATE:
        case WM_NCCALCSIZE:
        case WM_NCHITTEST:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCLBUTTONUP:
        case WM_NCMOUSEMOVE:
        case WM_CAPTURECHANGED:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_FORWARDKEYDOWN:
        case WM_FORWARDKEYUP:
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
        case WM_SYSCHAR:
        case WM_SYSCOMMAND:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT:
        case WM_SETCURSOR:
        case WM_IME_KEYDOWN:
        case WM_IME_KEYUP:
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_NOTIFY:
        case WM_IME_SETCONTEXT:
        case WM_IME_COMPOSITIONFULL:
        case WM_IME_SYSTEM:
        case WM_SYSCOPYDATA:
        case WM_INPUTLANGCHANGE:
        case WM_INPUTLANGCHANGEREQUEST:
        case WM_THEMECHANGED:
        case WM_GESTURE:
            return pGweApiSet1Entrypoints->m_pDefWindowProcW(hwnd,msg,wParam,lParam);
        }
    return 0;
}


extern "C"
HWND
GetParent(
    HWND    hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pGetParent(hwnd);
}

extern "C"
HWND
GetFocus(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetFocus();
}

extern "C"
DWORD
GetSysColor(
    int nIndex
    )
{
static  DWORD*  prgDefSysColor = NULL;

    DWORD dwReturnValue = 0;

    // As all the required data is in shared heap which can be read from user side we read it  
    // directly instead of instead of trapping into pGweApiSet1Entrypoints->m_pGetSysColor

    if ( !prgDefSysColor &&
            !pGweApiSet1Entrypoints->m_pGweCoreDllInfoFn( GWEINFO_DEFAULT_SYSTEM_COLORS, reinterpret_cast<void*>(&prgDefSysColor)) )
        {
        goto leave;    
        }

    if ( ( nIndex < SYS_COLOR_INDEX_FLAG ) ||
         ( nIndex >= (SYS_COLOR_INDEX_FLAG | C_SYS_COLOR_TYPES)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    dwReturnValue = prgDefSysColor[nIndex & ~SYS_COLOR_INDEX_FLAG];

leave:
    return dwReturnValue;
}

extern "C"
BOOL
IsWindow(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pIsWindow(hWnd);
}

BOOL
DeleteMenu(
    HMENU   hmenu,
    UINT    uPosition,
    UINT    uFlags
    )
{
    return pGweApiSet1Entrypoints->m_pDeleteMenu(hmenu,uPosition,uFlags);
}


BOOL
IsWindowVisible(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pIsWindowVisible(hWnd);
}





#ifdef x86
#pragma optimize("", off)
#endif

SHORT
GetAsyncKeyState(
    INT vKey
    )
{
    return pGweApiSet1Entrypoints->m_pGetAsyncKeyState(vKey);
}


extern "C"
SHORT
GetKeyState(
    INT vKey
    )
{
    return pGweApiSet1Entrypoints->m_pGetKeyState(vKey);
}

#ifdef x86
#pragma optimize("", on)
#endif

int
GetDlgCtrlID(
    HWND    hWnd
    )
{
    return pGweApiSet1Entrypoints->m_pGetDlgCtrlID(hWnd);
}

void
keybd_event(
    BYTE    Vk,
    BYTE    Scan,
    DWORD   dwFlags,
    DWORD   dwExtraInfo
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0) )
        {
        pGweApiSet1Entrypoints->m_pkeybd_event(Vk, Scan, dwFlags, dwExtraInfo);
        }
    return;
}

void
keybd_eventEx(
    BYTE    Vk,
    BYTE    Scan,
    DWORD   dwFlags,
    const GUID *pguidPDD
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0) )
        {
        pGweApiSet1Entrypoints->m_pkeybd_eventEx(Vk, Scan, dwFlags,  pguidPDD, sizeof(GUID));
        }
    return;
}

int
SetScrollInfo(
    HWND            hwnd,
    int             fnBar,
    __in LPCSCROLLINFO   lpsi,
    BOOL            fRedraw
    )
{
    return pGweApiSet1Entrypoints->m_pSetScrollInfo(hwnd,fnBar,lpsi,sizeof(SCROLLINFO),fRedraw);
}


BOOL
PeekMessageW(
    __out MSG*    pMsg,
    HWND    hWnd,
    UINT    wMsgFilterMin,
    UINT    wMsgFilterMax,
    UINT    wRemoveMsg
    )
{
    return pGweApiSet1Entrypoints->m_pPeekMessageW(pMsg,sizeof(MSG),hWnd,wMsgFilterMin,wMsgFilterMax,wRemoveMsg);
}


extern "C"
BOOL
GetForegroundInfo(
    __out GET_FOREGROUND_INFO*    pgfi
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0) )
        {
        return pGweApiSet2Entrypoints->m_pGetForegroundInfo(pgfi, sizeof(GET_FOREGROUND_INFO));
        }
    return FALSE;
}


extern "C"
HWND
GetForegroundKeyboardTarget(
    void
    )
{
    GET_FOREGROUND_INFO gfi;

    if ( GetForegroundInfo(&gfi) )
        {
        return gfi.hwndKeyboardDest;
        }
    return NULL;
}


HKL
GetForegroundKeyboardLayoutHandle(
    void
    )
{
    GET_FOREGROUND_INFO gfi;

    if ( GetForegroundInfo(&gfi) )
        {
        return gfi.KeyboardLayoutHandle;
        }
    return NULL;
}




BOOL
IsDialogMessageW(
    HWND    hDlg,
    __in MSG*    lpMsg
    )
{
    return pGweApiSet1Entrypoints->m_pIsDialogMessageW(hDlg, lpMsg, sizeof(MSG));
}

LRESULT
DefDlgProcW(
    HWND hDlg,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
    )
{
    return pGweApiSet1Entrypoints->m_pDefDlgProcW(hDlg,msg,wParam,lParam);
}

int
TranslateAcceleratorW(
    HWND    hWnd,
    HACCEL  hAccTable,
    MSG*    lpMsg
    )
{
    if ( lpMsg )
        {
        switch ( lpMsg->message )
            {
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            case WM_SYSCHAR:
            case WM_CHAR:
                return pGweApiSet1Entrypoints->m_pTranslateAcceleratorW(hWnd, hAccTable, lpMsg, sizeof(MSG));
            }
        }
    return 0;
}



UINT
RegisterClipboardFormatW(
    __in const   WCHAR*  lpszFormat
    )
{
    return pGweApiSet1Entrypoints->m_pRegisterClipboardFormatW(lpszFormat);
}



void
mouse_event(
    DWORD   dwFlags,
    DWORD   dx,
    DWORD   dy,
    DWORD   cButtons,
    DWORD   dwExtraInfo
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0) )
        {
        pGweApiSet1Entrypoints->m_pmouse_event(dwFlags, dx, dy, cButtons, dwExtraInfo);
        }
    return;
}


BOOL
GetClipCursor(
    __out RECT*   lpRect
    )
{
    return pGweApiSet1Entrypoints->m_pGetClipCursor(lpRect, sizeof(RECT));
}


unsigned int
SendInput(
    unsigned int    nInputs,
    __in_ecount(nInputs) INPUT*  pInputs,
    int     cbSize
    )
{
    size_t InputsBufferSize = 0;
    unsigned int ReturnValue = 0;
    unsigned int cbSizeLocal = 0;

    if ( WAIT_OBJECT_0 != WaitForAPIReady(SH_WMGR, 0) )
        {
        SetLastError(ERROR_NOT_READY);
        goto leave;
        }

    if ( (sizeof(INPUT) > cbSize) ||
         FAILED(IntToUInt(cbSize, &cbSizeLocal)) ||
         FAILED(safeIntUMul(nInputs, cbSizeLocal, &InputsBufferSize)) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }
    
     ReturnValue = pGweApiSet1Entrypoints->m_pSendInput(nInputs, pInputs, InputsBufferSize, cbSize);

leave:
    return ReturnValue;
}

BOOL
SetWindowAttribute(
    HWND hWnd,
    UINT uIndex,
    DWORD dwAttribute
)
{
    return pGweApiSet1Entrypoints->m_pSetWindowAttribute(hWnd, uIndex, dwAttribute);
}

BOOL
EnableGestures(
    HWND hwnd,
    ULONGLONG ullFlags,
    UINT uScope
    )
{
    return pGweApiSet1Entrypoints->m_pEnableGestures(hwnd, &ullFlags, uScope);
}

BOOL
DisableGestures(
    HWND hwnd,
    ULONGLONG ullFlags,
    UINT uScope
    )
{
    return pGweApiSet1Entrypoints->m_pDisableGestures(hwnd, &ullFlags, uScope);
}

BOOL
QueryGestures(
    HWND hwnd,
    UINT uScope,
    __out PULONGLONG pullFlags
    )
{
    return pGweApiSet1Entrypoints->m_pQueryGestures(hwnd, uScope, pullFlags);
}

extern "C"
BOOL
WINAPI
GetGestureInfo(
    __in HGESTUREINFO hGestureInfo,
    __out PGESTUREINFO pCeGestureInfo
    )
{
    // This API is not currently exported, but if CEGESTUREINFO ever differs
    // from GESTUREINFO, it will be trivial to export this API

    if (NULL == hGestureInfo ||
        NULL == pCeGestureInfo ||
        sizeof(CEGESTUREINFO) != pCeGestureInfo->cbSize)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // We make a local copy of the GESTUREINFODATA using a safe copy mechanism
    // so that if the HGESTUREINFO points to invalid memory we don't crash. We try to
    // ensure that any exception resulting from accesing the memory backing the HGESTUREINFO
    // is turned into a failure, but that we propogate exceptions resulting from accessing
    // the output buffer. Note that we do not copy the extra arguments here - we do not touch
    // them.
    GESTUREINFODATA localGestureInfoData;
    if (!CeSafeCopyMemory(&localGestureInfoData, reinterpret_cast<GESTUREINFODATA*>(hGestureInfo), MIN_GESTUREINFODATA_SIZE_CB) ||
        !IsValidGESTUREINFODATA(&localGestureInfoData))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // This is copying the result into the caller buffer. Any exception here
    // is due to the callers output buffer being invalid.
    memcpy(pCeGestureInfo, &localGestureInfoData.gestureInfo, sizeof(CEGESTUREINFO));

    return TRUE;
}


extern "C"
BOOL
WINAPI
GetGestureExtraArguments(
    __in HGESTUREINFO hGestureInfo,
    __in UINT cbExtraArguments,
    __out_bcount(cbExtraArguments) PBYTE pExtraArguments
    )
{
    if (NULL == hGestureInfo ||
        NULL == pExtraArguments)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // We make a local copy of the GESTUREINFODATA using a safe copy mechanism
    // so that if the HGESTUREINFO points to invalid memory we don't crash.
    GESTUREINFODATA localGestureInfoData;
    if (!CeSafeCopyMemory(&localGestureInfoData, reinterpret_cast<GESTUREINFODATA*>(hGestureInfo), MIN_GESTUREINFODATA_SIZE_CB) ||
        !IsValidGESTUREINFODATA(&localGestureInfoData))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Check that there is data and that the buffer is big enough before trying to copy
    // anything
    if (localGestureInfoData.gestureInfo.cbExtraArguments == 0)
    {
        SetLastError(ERROR_NO_DATA);
        return FALSE;
    }

    if (cbExtraArguments < localGestureInfoData.gestureInfo.cbExtraArguments)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    // Attempt to copy the data into our local buffer
    if (!CeSafeCopyMemory(pExtraArguments, reinterpret_cast<GESTUREINFODATA*>(hGestureInfo)->rgExtraArguments, localGestureInfoData.gestureInfo.cbExtraArguments))
    {
        // The handle is invalid or the output buffer is invalid - we can't tell.
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return TRUE;
}

extern "C"
BOOL
WINAPI
Gesture(
    __in HWND hwnd,
    __in const GESTUREINFO* pGestureInfo,
    __in_bcount_opt(pGestureInfo->cbExtraArguments) const BYTE* pExtraArguments)
{
    if (NULL == pGestureInfo || sizeof(GESTUREINFO) != pGestureInfo->cbSize)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return pGweApiSet2Entrypoints->m_pGesture(hwnd, pGestureInfo, sizeof(*pGestureInfo), pExtraArguments, pGestureInfo->cbExtraArguments);
}

    
extern "C"
BOOL
WINAPI
GestureBeginSession(
    __inout CEGESTUREINFO* pGestureInfo)
{
    if (NULL == pGestureInfo || sizeof(GESTUREINFO) != pGestureInfo->cbSize)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return pGweApiSet2Entrypoints->m_pGestureSession(TRUE, pGestureInfo, sizeof(*pGestureInfo));
}

extern "C"
BOOL
WINAPI
GestureEndSession(
    __in const CEGESTUREINFO* pGestureInfo)
{
    if (NULL == pGestureInfo || sizeof(GESTUREINFO) != pGestureInfo->cbSize)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return pGweApiSet2Entrypoints->m_pGestureSession(FALSE, const_cast<CEGESTUREINFO *>(pGestureInfo), sizeof(*pGestureInfo));
}

extern "C"
DWORD
WINAPI
SetWindowCursorPreference(
    HWND hwnd,
    DWORD dwFlags
    )
{
    return pGweApiSet1Entrypoints->m_pSetWindowCursorPreference(hwnd, dwFlags);
}


extern "C"
DWORD
WINAPI
GetWindowCursorPreference(
    HWND hwnd
    )
{
    return pGweApiSet1Entrypoints->m_pGetWindowCursorPreference(hwnd);
}


extern "C"
DWORD
WINAPI
SetSystemCursorPreference(
    DWORD dwFlags
    )
{
    return pGweApiSet1Entrypoints->m_pSetSystemCursorPreference(dwFlags);
}


extern "C"
DWORD
WINAPI
GetSystemCursorPreference(
    void
    )
{
    return pGweApiSet1Entrypoints->m_pGetSystemCursorPreference();
}

#ifndef RDPGWELIB

extern "C"
int
GetWindowTextWDirect(
    HWND    hwnd,
    __out_ecount(cchMax) WCHAR * pString,
    int     cchMax
    )
{
    int ReturnValue = 0;

    unsigned int cchStringBufferCharCount = 0;
    size_t cbStringBufferSize = 0;
    
    if( FAILED( IntToUInt( cchMax, &cchStringBufferCharCount)) || 
          FAILED( UIntMult(cchStringBufferCharCount, sizeof(WCHAR), &cbStringBufferSize)) )
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
    }

    ReturnValue = pGweApiSet1Entrypoints->m_pGetWindowTextW(hwnd, pString, cbStringBufferSize, cchMax, TRUE);

leave:
    return ReturnValue;
}

#endif

//xref ApiTrapWrapper
extern "C"
BOOL
ShowStartupWindow(
    void
    )
{
    return pGweApiSet2Entrypoints->m_pShowStartupWindow(FALSE);
}

//xref ApiTrapWrapper
extern "C"
BOOL
ShowStartupWindowBypassWantStartupScreen(
    void
    )
{
    return pGweApiSet2Entrypoints->m_pShowStartupWindow(TRUE);
}


extern "C"
BOOL
GetIconInfo(
    HICON hIcon,
    __out PICONINFO piconinfo
    )
{
    return pGweApiSet2Entrypoints->m_pGetIconInfo(hIcon, piconinfo, sizeof(ICONINFO));
}

BOOL
IsGwesLoaded(
    void
    )
{
    static BOOL s_bIsGwesLoaed = FALSE;

    if ( !s_bIsGwesLoaed )
        {
        if ( WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0) )
            {
            s_bIsGwesLoaed = TRUE;
            }
        else
            {
            //  As good an error as any.
            SetLastError(ERROR_INVALID_WINDOW_HANDLE);
            }
        }
    
    return s_bIsGwesLoaed;
}

// TODO: (yama perf) Need to check if we need to provide alternate mechanism to 
// TODO: (yama perf) calling GetCurrentThreadMessageQueue so that we can avoid a trap.
// TODO: (yama perf) - as of not only sipapi uses this method. The code is left here so that 
// TODO: (yama perf) all the usages of GWE intenal structures are done only from thunks dir.

BOOL CoreDllNeedsPixelDoubling()
{
    MsgQueue*   pMsgQueueThisThread;
    BOOL bRet = TRUE;                   // assume that we need it. Better safe than sorry.

    if ( !IsGwesLoaded() )
        return TRUE;

    //  Get the message queue for this thread.
    pMsgQueueThisThread = GetCurrentThreadMessageQueue();
    if ( !pMsgQueueThisThread )
        {
        bRet = TRUE;
        }
    else
        {
        // Since there is a valid MsgQueue, its m_NeedsPixelDoubling should
        // have already been initialized.
        ASSERT( pMsgQueueThisThread -> m_NeedsPixelDoubling != MsgQueue::DoNotKnow );
        if ( pMsgQueueThisThread -> m_NeedsPixelDoubling == MsgQueue::Yes )
            {
            bRet = TRUE;
            }
        else if ( pMsgQueueThisThread -> m_NeedsPixelDoubling == MsgQueue::No )
            {
            bRet = FALSE;
            }
        }

    return bRet;
}

BOOL
SetDialogAutoScrollBar(
    HWND    hDlg
    )
{
    return pGweApiSet2Entrypoints->m_pSetDialogAutoScrollBar(hDlg);
}

BOOL
SetWindowPosOnRotate(
    HWND hWnd,
    HWND hWndInsertAfter,
    int X,
    int Y,
    int cx,
    int cy,
    UINT uFlags,
    BOOL fNoScroll
    )
{
    return pGweApiSet2Entrypoints->m_pSetWindowPosOnRotate(hWnd, hWndInsertAfter,
                                 X, Y, cx, cy,
                                 uFlags, fNoScroll);
}

HRESULT 
AnimateRects(
    ANIMATERECTSINFO *pari
    )
{
    return pGweApiSet2Entrypoints->m_pAnimateRects(pari, sizeof(ANIMATERECTSINFO));
}


BYTE
GetWindowOpacity(
    HWND hwnd
    )
{
    return pGweApiSet2Entrypoints->m_pGetWindowOpacity(hwnd);
}

BOOL
SetWindowOpacity(
    HWND hwnd,
    BYTE bAlpha
    )
{
    return pGweApiSet2Entrypoints->m_pSetWindowOpacity(hwnd, bAlpha);
}

BOOL
UpdateWindowSurface(
    HWND    hwnd,
    int     iEscape,
    int     cjInput,
    __in_bcount_opt(cjInput) const   char*   lpInData,
    int     cjOutput,
    __out_bcount_opt(cjOutput) char*   lpOutData
    )
{
    BOOL fReturnValue = FALSE;

    if ( (cjInput < 0) || (cjOutput < 0) )
        {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto leave;
        }

    fReturnValue = pGweApiSet2Entrypoints->m_pUpdateWindowSurface(hwnd, iEscape, lpInData, cjInput, lpOutData, cjOutput);

leave:
    return fReturnValue;
}

DWORD
GetWindowCompositionFlags(
    HWND hwnd
    )
{
    return pGweApiSet2Entrypoints->m_pGetWindowCompositionFlags(hwnd);
}

BOOL
SetWindowCompositionFlags(
    HWND hwnd,
    DWORD dwFlags
    )
{
    return pGweApiSet2Entrypoints->m_pSetWindowCompositionFlags(hwnd, dwFlags);
}

void
MinimizeFullScreenExclusive(
    void
    )
{
    return pGweApiSet2Entrypoints->m_pMinimizeFullScreenExclusive();
}

//Configuration info APIs for assets management
extern "C"
DWORD
OpenDisplayConfiguration(
    HDC hDC
    )
{
    return pGweApiSet2Entrypoints->m_pOpenDisplayConfiguration(hDC);
}

extern "C"
BOOL
CloseDisplayConfiguration(
    DWORD dwDisplayConfigurationID
    )
{
    return pGweApiSet2Entrypoints->m_pCloseDisplayConfiguration(dwDisplayConfigurationID);
}

extern "C"
BOOL
GetFallbackModuleNamesForDisplayConfiguration(
    const __in_z LPCWSTR pwszModuleBase,
    __deref_out_ecount(*pcszModuleNames) LPWSTR **pprgwszModuleNames,
    __out DWORD *pcszModuleNames,
    const __in_z LPCWSTR pwszType,
    DWORD dwDisplayConfigurationID
    )
{
    return pGweApiSet2Entrypoints->m_pGetFallbackModuleNamesForDisplayConfiguration(pwszModuleBase, pprgwszModuleNames, pcszModuleNames, pwszType, dwDisplayConfigurationID);
}

extern "C"
HRESULT
WINAPI
GetMemoryWaitEvents(
    __deref_out HANDLE*   phLowMemoryEvent,
    __deref_out HANDLE*   phHighMemoryEvent
   )
{
    return pGweApiSet2Entrypoints->m_pGetMemoryWaitEvents(phLowMemoryEvent, phHighMemoryEvent);
}

BOOL
RegisterGesture(
    const __in_z LPCWSTR pszName,
    __out PDWORD_PTR pdwID
    )
{
    return pGweApiSet2Entrypoints->m_pRegisterGesture(pszName, pdwID);
}

