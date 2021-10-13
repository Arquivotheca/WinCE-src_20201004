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

#ifndef __GWE_API_SET_1_HPP_INCLUDED__
#define __GWE_API_SET_1_HPP_INCLUDED__

#include <CePtr.hpp>

// If MAKEINTRESOURCE is used to make a resource name string, 
// the LOWORD has the resource ID while the HIWORD is 0.
#define IS_RESOURCE_ID(str) ( str && !HIWORD((DWORD)str))

class   ImageList;
struct  _IMAGEINFO;
struct  _IMAGELISTDRAWPARAMS;

class GweApiSet1_t;
class GweApiSet2_t;
class MsgQueue;

class GweApiSet1_t
{
public:

    typedef
    void
    (*Unimplemented_t)(
        void
        );

    typedef
    void
    (*NotifyCallback_t)(
        DWORD       cause,
        HPROCESS    hprc,
        HTHREAD     hthd
        );

    typedef
    void
    (*GweCoreDllInfoFn_t)(
        DWORD   dwIndex,
        void*   pData
        );

    typedef
    ATOM
    (*RegisterClassW_t)(
        const   WNDCLASSW*  pWndcls,
                size_t      WndclsSize
        );


    typedef
    BOOL
    (*UnregisterClassW_t)(
              ATOM   ClassIdentifier,
        const WCHAR* pClassName
        );

    typedef
    HWND
    (*CreateWindowExW_t)(
                UINT32          grfExStyle,
        const   WCHAR*          szClassName,
        const   WCHAR*          szName,
                UINT32          grfStyle,
                INT32           x,
                INT32           y,
                UINT32          cx,
                UINT32          cy,
                HWND            pwndParent,
                HMENU           hmenu,
                HINSTANCE       hinst,
                void*           pvParam,
                CREATESTRUCTW*  pcrs
                );

    typedef
    BOOL
    (*PostMessageW_t)(
        HWND            hWnd,
        unsigned int    Msg,
        WPARAM          wParam,
        LPARAM          lParam
        );

    typedef
    void
    (*PostQuitMessage_t)(
        int     nExitCode
        );

    typedef
    LRESULT
    (*SendMessageW_t)(
        HWND            hWnd,
        unsigned int    uMsg,
        WPARAM          wParam,
        LPARAM          lParam
        );

    typedef
    BOOL
    (*GetMessageW_t)(
        MSG*            pMsg,
        HWND            hWnd,
        unsigned int    wMsgFilterMin,
        unsigned int    wMsgFilterMax
        );

    typedef
    BOOL
    (*TranslateMessage_t)(
        const   MSG*    pMsg
        );


    typedef
    LRESULT
    (*DispatchMessageW_t)(
        const   MSG*    pMsg
        );


    typedef
    HWND
    (*GetCapture_t)(
        void
        );

    typedef
    HWND
    (*SetCapture_t)(
        HWND    hwnd
        );


    typedef
    BOOL
    (*ReleaseCapture_t)(
        void
        );


    typedef
    BOOL
    (*SetWindowPos_t)(
        HWND    hwndThis,
        HWND    hwndInsertAfter,
        int     x,
        int     y,
        int     cx,
        int     cy,
        UINT    fuFlags
        );


    typedef
    BOOL
    (*GetWindowRect_t)(
        HWND    hwndThis,
        RECT*   prc
        );

    typedef
    BOOL
    (*GetClientRect_t)(
        HWND    hwndThis,
        RECT*   prc
        );


    typedef
    BOOL
    (*InvalidateRect_t)(
                HWND    hwndThis,
        const   RECT*   prc,
                BOOL    fErase
        );

    typedef
    HWND
    (*GetWindow_t)(
        HWND    hwndThis,
        UINT32  relation
        );


    typedef
    int
    (*GetSystemMetrics_t)(
        int nIndex
        );


    typedef
    ImageList*
    (*GetDragImage_t)(
        POINT*  ppt,
        POINT*  pptHotspot
        );

    typedef
    BOOL
    (*GetIconSize_t)(
        ImageList*  piml,
        int*        cx,
        int*        cy
        );

    typedef
    BOOL
    (*SetIconSize_t)(
        ImageList*  piml,
        int         cx,
        int         cy
        );


    typedef
    BOOL
    (*GetImageInfo_t)(
        ImageList*  piml,
        int         i,
        _IMAGEINFO* pImageInfo
        );

    typedef
    ImageList*
    (*Merge_t)(
        ImageList*  piml1,
        int         i1,
        ImageList*  piml2,
        int         i2,
        int         dx,
        int         dy
        );


    typedef
    int
    (*ShowCursor_t)(
        BOOL bShow
        );

    typedef
    BOOL
    (*SetCursorPos_t)(
        int x,
        int y
        );

    typedef
    void
    (*CopyDitherImage_t)(
        ImageList*  pimlDst,
        WORD        iDst,
        int         xDst,
        int         yDst,
        ImageList*  pimlSrc,
        int         iSrc,
        UINT        fStyle
        );

    typedef
    BOOL
    (*DrawIndirect_t)(
        _IMAGELISTDRAWPARAMS* pimldp
        );

    typedef
    BOOL
    (*DragShowNolock_t)(
        BOOL    fShow
    );


    typedef
    HWND
    (*WindowFromPoint_t)(
        POINT   pt
    );


    typedef
    HWND
    (*ChildWindowFromPoint_t)(
        HWND    hwndThis,
        POINT   pt
        );


    typedef
    BOOL
    (*ClientToScreen_t)(
        HWND    hwndThis,
        POINT*  pPoint
        );

    typedef
    BOOL
    (*ScreenToClient_t)(
        HWND    hwndThis,
        POINT*  pPoint
        );

    typedef
    BOOL
    (*SetWindowTextW_t)(
                HWND    hwndThis,
        const   WCHAR*  psz
        );

    typedef
    INT
    (*GetWindowTextW_t)(
        HWND    hwndThis,
        WCHAR*  pString,
        size_t  StringBufferSize,
        int     cchMax
        );

    typedef
    LONG
    (*SetWindowLongW_t)(
        HWND    hwndThis,
        int     nIndex,
        LONG    lNewLong
        );

    typedef
    LONG
    (*GetWindowLongW_t)(
        HWND    hwndThis,
        int     nIndex
        );


    typedef
    HDC
    (*BeginPaint_t)(
        HWND            hwndThis,
        PAINTSTRUCT*    pps
        );

    typedef
    BOOL
    (*EndPaint_t)(
        HWND            hwndThis,
        PAINTSTRUCT*    pps
        );








    typedef
    HDC
    (*GetDC_t)(
        HWND    hwndThis
        );

    typedef
    int
    (*ReleaseDC_t)(
        HWND    hwndThis,
        HDC     hdc
        );

    typedef
    LRESULT
    (*DefWindowProcW_t)(
        HWND    hwndThis,
        UINT    msg,
        WPARAM  wParam,
        LPARAM  lParam
        );

    typedef
    DWORD
    (*GetClassLongW_t)(
        HWND            hwndThis,
        INT             nIndex
        );

    typedef
    DWORD
    (*SetClassLongW_t)(
        HWND    hwndThis,
        INT     nIndex,
        LONG    lNewValue
        );

    typedef
    BOOL
    (*DestroyWindow_t)(
        HWND    hwndThis
        );

    typedef
    BOOL
    (*ShowWindow_t)(
        HWND    hwndThis,
        INT     nCmdShow
        );

    typedef
    BOOL
    (*UpdateWindow_t)(
        HWND    hwndThis
        );

    typedef
    HWND
    (*SetParent_t)(
        HWND    hwndThis,
        HWND    hwndNewParent
        );

    typedef
    HWND
    (*GetParent_t)(
        HWND    hwndThis
        );

    typedef
    int
    (*MessageBoxW_t)(
                HWND    hwnd,
        const   WCHAR*  szText,
        const   WCHAR*  szCaption,
                UINT    uType,
                MSG*    pMsg
        );

    typedef
    HWND
    (*SetFocus_t)(
        HWND hWnd
        );

    typedef
    HWND
    (*GetFocus_t)(
        void
        );

    typedef
    HWND
    (*GetActiveWindow_t)(
        void
        );

    typedef
    HDC
    (*GetWindowDC_t)(
        HWND    hwndThis
        );

    typedef
    DWORD
    (*GetSysColor_t)(
        int nIndex
        );

    typedef
    BOOL
    (*AdjustWindowRectEx_t)(
        LPRECT  prc,
        DWORD   dwStyle,
        BOOL    fMenu,
        DWORD   dwExStyle
        );


    typedef
    BOOL
    (*IsWindow_t)(
        HWND    hwndThis
        );

    typedef
    HMENU
    (*CreatePopupMenu_t)(
        void
        );

    typedef
    BOOL
    (*InsertMenuW_t)(
        HMENU   hmenu,
        UINT    uPosition,
        UINT    uFlags,
        UINT    uIDNewItem,
        LPCWSTR lpNewItem
        );


    typedef
    BOOL
    (*AppendMenuW_t)(
        HMENU   hMenu,
        UINT    uFlags,
        UINT    uIDNewItem,
        LPCWSTR lpNewItem
        );

    typedef
    BOOL
    (*RemoveMenu_t)(
        HMENU   hmenu,
        UINT    uPosition,
        UINT    uFlags
        );

    typedef
    BOOL
    (*DestroyMenu_t)(
        HMENU   hmenu
        );

    typedef
    BOOL
    (*TrackPopupMenuEx_t)(
        HMENU       hmenu,
        UINT        uFlags,
        int         x,
        int         y,
        HWND        hWnd,
        LPTPMPARAMS lptpm
        );

    typedef
    HMENU
    (*LoadMenuW_t)(
        HINSTANCE   hinst,
        DWORD         MenuNameResourceID,
    const   WCHAR* pMenuName
        );

    typedef
    BOOL
    (*EnableMenuItem_t)(
        HMENU   hmenu,
        UINT    uPosition,
        UINT    uFlags
        );

    typedef
    BOOL
    (*MoveWindow_t)(
        HWND    hwndThis,
        int     X,
        int     Y,
        int     nWidth,
        int     nHeight,
        BOOL    bRepaint
        );

    typedef
    int
    (*GetUpdateRgn_t)(
        HWND    hwndThis,
        HRGN    hRgn,
        BOOL    bErase
        );

    typedef
    BOOL
    (*GetUpdateRect_t)(
        HWND    hwndThis,
        LPRECT  lpRect,
        BOOL    bErase
        );

    typedef
    BOOL
    (*BringWindowToTop_t)(
        HWND    hwndThis
        );

    typedef
    int
    (*GetWindowTextLengthW_t)(
        HWND    hwndThis
        );

    typedef
    BOOL
    (*IsChild_t)(
        HWND    hwndThis,
        HWND    hWnd
        );

    typedef
    BOOL
    (*IsWindowVisible_t)(
        HWND    hwndThis
        );

    typedef
    BOOL
    (*ValidateRect_t)(
                HWND    hwndThis,
        CONST   RECT*   lpRect
        );

    typedef
    HBITMAP
    (*LoadBitmapW_t)(
        HINSTANCE    hinst,
        DWORD          BitmapResourceID,        
    const   WCHAR*  pBitmapName
        );

    typedef
    DWORD
    (*CheckMenuItem_t)(
        HMENU   hmenu,
        UINT    uPosition,
        UINT    uFlags
        );

    typedef
    BOOL
    (*CheckMenuRadioItem_t)(
        HMENU   hMenu,
        UINT    wIDFirst,
        UINT    wIDLast,
        UINT    wIDCheck,
        UINT    flags
        );

    typedef
    BOOL
    (*DeleteMenu_t)(
        HMENU   hmenu,
        UINT    uPosition,
        UINT    uFlags
        );

    typedef
    HICON
    (*LoadIconW_t)(
        HINSTANCE   hinst,
        DWORD         IconNameResourceID,
    const   WCHAR* pIconName
        );

    typedef
    BOOL
    (*DrawIconEx_t)(
        HDC     hdc,
        int     X,
        int     Y,
        HICON   hicon,
        int     cx,
        int     cy,
        UINT    istepIfAniCur,
        HBRUSH  hbrFlickerFreeDraw,
        UINT    diFlags
        );

    typedef
    BOOL
    (*DestroyIcon_t)(
        HICON   hicon
        );

    typedef
    SHORT
    (*GetAsyncKeyState_t)(
        int nVirtKey
        );


    typedef
    int
    (*DialogBoxIndirectParamW_t)(
                HINSTANCE           hInstance,
        const   DLGTEMPLATE*        lpdt,
                HWND                hwndOwner,
                DLGPROC          pDialogProc,
                LPARAM              lParam,
                CREATESTRUCTW*      pcrs,
                MSG*                pmsg
                );


    typedef
    BOOL
    (*EndDialog_t)(
        HWND    hwnd,
        int     lresult
        );

    typedef
    HWND
    (*GetDlgItem_t)(
        HWND    hDlg,
        int     iCtrlID
        );

    typedef
    int
    (*GetDlgCtrlID_t)(
        HWND    hwnd
        );

    typedef
    SHORT
    (*GetKeyState_t)(
        int nVirtKey
        );


    typedef
    BOOL
    (*KeybdGetDeviceInfo_t)(
        int     iIndex,
        void*   lpOutput
        );

    typedef
    BOOL
    (*KeybdInitStates_t)(
        KEY_STATE   KeyState,
        size_t           KeyStateSize, 
        void        *pKeybdDriverState
        );

    typedef
    BOOL
    (*PostKeybdMessage_t)(
        HWND            hwnd,                    //    hwnd to send to.
        unsigned int    VKey,                    //    VKey to send.
        KEY_STATE_FLAGS    KeyStateFlags,            //    Flags to determine event and other key states.
        unsigned int    CharacterCount,            //    Count of characters.
        unsigned int  *pShiftStateBuffer,     //    Shift state buffer.
        unsigned int   ShiftStateBufferLen,
        unsigned int  *pCharacterBuffer,       //    Character buffer.
        unsigned int   CharacterBufferLen
        );

    typedef
    unsigned int
    (*KeybdVKeyToUnicode_t)(
        unsigned int             VKey,
        KEY_STATE_FLAGS   KeyEvent,
        KEY_STATE       KeyState,
        size_t               KeyStateSize,
        void                *pKeybdDriverState,
        unsigned int       BufferSizeCount,
        unsigned int     *pCharacterCount,
        KEY_STATE_FLAGS *pShiftStateBuffer,
        unsigned int       ShiftStateBufferLen,
        unsigned int     *pCharacterBuffer,
        unsigned int       CharacterBufferLen
        );

    typedef
    void
    (*keybd_event_t)(
        BYTE    Vk,
        BYTE    Scan,
        DWORD   dwFlags,
        DWORD   dwExtraInfo
        );

    typedef
    void
    (*mouse_event_t)(
        DWORD   dwFlags,
        DWORD   dx,
        DWORD   dy,
        DWORD   dwData,
        DWORD   dwExtraInfo
        );

    typedef
    int
    (*SetScrollInfo_t)(
                HWND        hwnd,
                int         fnBar,
        const   SCROLLINFO* psi,
                BOOL        bRedraw
        );

    typedef
    int
    (*SetScrollPos_t)(
        HWND    hwnd,
        int     fnBar,
        int     nPos,
        BOOL    bRedraw
        );

    typedef
    BOOL
    (*SetScrollRange_t)(
        HWND    hwnd,
        int     fnBar,
        int     nMinPos,
        int     nMaxPos,
        BOOL    bRedraw
        );

    typedef
    BOOL
    (*GetScrollInfo_t)(
        HWND        hwnd,
        int         fnBar,
        SCROLLINFO* psi
        );

    typedef
    BOOL
    (*PeekMessageW_t)(
        MSG*            pMsg,
        HWND            hWnd,
        unsigned int    wMsgFilterMin,
        unsigned int    wMsgFilterMax,
        unsigned int    wRemoveMsg
        );

    typedef
    UINT
    (*MapVirtualKeyW_t)(
        UINT    uCode,
        UINT    uMapType
        );

    typedef
    BOOL
    (*GetMessageWNoWait_t)(
        MSG*            pMsg,
        HWND            hWnd,
        unsigned int    wMsgFilterMin,
        unsigned int    wMsgFilterMax
        );

    typedef
    int
    (*GetClassNameW_t)(
        HWND    hwndThis,
        WCHAR*  szClassName,
        int     cchClassName
        );

    typedef
    int
    (*MapWindowPoints_t)(
        HWND        hwndFrom,
        HWND        hwndTo,
        POINT*      ppt,
        UINT        cPoints
        );

    typedef
    HANDLE
    (*LoadImageW_t)(
            HINSTANCE   hinst,
            DWORD        ImageNameResourceID,
    const   WCHAR*    pImageName,
            unsigned int  uType,
            int                cxDesired,
            int                cyDesired,
            unsigned int  fuLoad
            );

    typedef
    HWND
    (*GetForegroundWindow_t)(
        void
        );



    typedef
    BOOL
    (*SetForegroundWindow_t)(
        HWND    hwnd
        );

    typedef
    BOOL
    (*RegisterTaskBar_t)(
        HWND    hwndTaskBar
        );

    typedef
    HWND
    (*SetActiveWindow_t)(
        HWND    hWnd
        );

    typedef
    LRESULT
    (*CallWindowProcW_t)(
        WNDPROC        pWindowProc,
        HWND                hwnd,
        UINT                uMsg,
        WPARAM              wParam,
        LPARAM              lParam
        );

    typedef
    BOOL
    (*GetClassInfoW_t)(
                HINSTANCE   hInstance,
                ATOM        ClassIdentifier,
        const   WCHAR*      pClassName,
                WNDCLASS*   pwndcls
        );


    typedef
    HWND
    (*GetNextDlgTabItem_t)(
        HWND    hwndDlg,
        HWND    hwnd,
        BOOL    fPrev
        );


    typedef
    HWND
    (*CreateDialogIndirectParamW_t)(
                HINSTANCE           hInstance,
        const   DLGTEMPLATE*        lpDT,
                HWND                hwndOwner,
                DLGPROC           pDialogProc,
                LPARAM              lParam,
                CREATESTRUCTW*      pcrs
        );

    typedef
    BOOL
    (*IsDialogMessageW_t)(
        HWND    hwndDlg,
        MSG*    lpmsg
        );

    typedef
    BOOL
    (*SetDlgItemInt_t)(
        HWND    hwnd,
        int     lid,
        UINT    lValue,
        BOOL    fSigned
        );

    typedef
    UINT
    (*GetDlgItemInt_t)(
        HWND    hwnd,
        int     lid,
        BOOL*   lpfValOK,
        BOOL    fSigned
        );

    typedef
    HWND
    (*FindWindowW_t)(
              ATOM   ClassIdentifier,
        const WCHAR* pClassName,
        const WCHAR* pWindowName
        );

    typedef
    BOOL
    (*CreateCaret_t)(
        HWND    hWnd,
        HBITMAP hBitmap,
        int     nWidth,
        int     nHeight
        );

    typedef
    BOOL
    (*DestroyCaret_t)(
        void
        );

    typedef
    BOOL
    (*HideCaret_t)(
        HWND    hWnd
        );

    typedef
    BOOL
    (*ShowCaret_t)(
        HWND    hWnd
        );

    typedef
    BOOL
    (*SetCaretPos_t)(
        int X,
        int Y
        );

    typedef
    BOOL
    (*GetCaretPos_t)(
        POINT*  lpPoint
        );

    typedef
    BOOL
    (*GetCursorPos_t)(
        POINT*  pPoint,
        size_t     PointBufferSize
        );


    typedef
    BOOL
    (*ClipCursor_t)(
        const   RECT*   pClipRect
        );


    typedef
    BOOL
    (*GetClipCursor_t)(
        RECT*   pRect
        );

    typedef
    HCURSOR
    (*GetCursor_t)(
        void
        );

    typedef
    HICON
    (*ExtractIconExW_t)(
        const   WCHAR*  pszExeName,
                int     nIconIndex,
                HICON*  phiconLarge,
                HICON*  phiconSmall,
                UINT    nIcons
        );


    typedef
    unsigned int
    (*SetTimer_t)(
        HWND            hwnd,
        unsigned int    idTimer,
        unsigned int    uTimeout,
        TIMERPROC       tmprc,
        HPROCESS        hProcCallingContext
        );

    typedef
    BOOL
    (*KillTimer_t)(
        HWND            hwnd,
        unsigned int    uIDEvent
        );


    typedef
    HWND
    (*GetNextDlgGroupItem_t)(
        HWND    hwndDlg,
        HWND    hwnd,
        BOOL    fPrev
        );


    typedef
    BOOL
    (*CheckRadioButton_t)(
        HWND    hwnd,
        int     idFirst,
        int     idLast,
        int     idCheck
        );

    typedef
    BOOL
    (*EnableWindow_t)(
        HWND    hwndThis,
        BOOL    bEnable
        );

    typedef
    BOOL
    (*IsWindowEnabled_t)(
        HWND    hwndThis
        );


    typedef
    HMENU
    (*CreateMenu_t)(
        void
        );

    typedef
    HMENU
    (*GetSubMenu_t)(
        HMENU   hmenu,
        int     nPos
        );


    typedef
    LRESULT
    (*DefDlgProcW_t)(
        HWND    hwnd,
        UINT    message,
        WPARAM  wParam,
        LPARAM  lParam
        );

    typedef
    BOOL
    (*SendNotifyMessageW_t)(
        HWND            hWnd,
        unsigned int    uMsg,
        WPARAM          wParam,
        LPARAM          lParam
        );

    typedef
    BOOL
    (*PostThreadMessageW_t)(
        DWORD           idThread,
        unsigned int    Msg,
        WPARAM          wParam,
        LPARAM          lParam
        );

    typedef
    int
    (*TranslateAcceleratorW_t)(
        HWND    hWnd,
        HACCEL  hAccTable,
        MSG*    lpMsg
        );


    typedef
    HKL
    (*GetKeyboardLayout_t)(
        DWORD   idThread
        );


    typedef
    unsigned int
    (*GetKeyboardLayoutList_t)(
        int     KeybdLayoutCount,
        HKL *pKeybdLayoutList,
        size_t  KeybdLayoutListBufferSize 
        );

    typedef
    int
    (*GetKeyboardType_t)(
        int nTypeFlag
        );

    typedef
    ImageList*
    (*Create_t)(
        int     cx,
        int     cy,
        UINT    flags,
        int     cInitial,
        int     cGrow
        );

    typedef
    BOOL
    (*Destroy_t)(
        ImageList* piml
        );


    typedef
    int
    (*GetImageCount_t)(
        ImageList* piml
        );

    typedef
    int
    (*Add_t)(
        ImageList*  piml,
        HBITMAP     hbmImage,
        HBITMAP     hbmMask
        );

    typedef
    int
    (*ReplaceIcon_t)(
        ImageList*  piml,
        int         i,
        HICON       hIcon
        );

    typedef
    COLORREF
    (*SetBkColor_t)(
        ImageList*  piml,
        COLORREF    clrBk
        );

    typedef
    COLORREF
    (*GetBkColor_t)(
        ImageList*  piml
        );

    typedef
    BOOL
    (*SetOverlayImage_t)(
        ImageList*  piml,
        int         iImage,
        int         iOverlay
        );


    typedef
    BOOL
    (*Draw_t)(
        ImageList*  piml,
        int         i,
        HDC         hdcDst,
        int         x,
        int         y,
        UINT        fStyle
        );

    typedef
    BOOL
    (*Replace_t)(
        ImageList*  piml,
        int         i,
        HBITMAP     hbmImage,
        HBITMAP     hbmMask
        );

    typedef
    int
    (*AddMasked_t)(
        ImageList*  piml,
        HBITMAP     hbmImage,
        COLORREF    crMask
        );

    typedef
    BOOL
    (*DrawEx_t)(
        ImageList*  piml,
        int         i,
        HDC         hdcDst,
        int         x,
        int         y,
        int         cx,
        int         cy,
        COLORREF    rgbBk,
        COLORREF    rgbFg,
        UINT        fStyle
        );

    typedef
    BOOL
    (*Remove_t)(
        ImageList*  piml,
        int         i
        );

    typedef
    HICON
    (*GetIcon_t)(
        ImageList*  piml,
        int         i,
        UINT        flags
        );

    typedef
    ImageList*
    (*LoadImage_t)(
                HINSTANCE   hi,
                DWORD        ImageNameResourceID,                
        const   WCHAR*    pImageName,
                int                cx,
                int                cGrow,
                COLORREF    crMask,
                unsigned int  uType,
                unsigned int  uFlags
                );

    typedef
    BOOL
    (*BeginDrag_t)(
        ImageList*  pimlTrack,
        int         iTrack,
        int         dxHotspot,
        int         dyHotspot
        );

    typedef
    void
    (*EndDrag_t)(
        void
        );

    typedef
    BOOL
    (*DragEnter_t)(
        HWND    hwndLock,
        int     x,
        int     y
        );

    typedef
    BOOL
    (*DragLeave_t)(
        HWND hwndLock
        );

    typedef
    BOOL
    (*DragMove_t)(
        int x,
        int y
        );


    typedef
    BOOL
    (*SetDragCursorImage_t)(
        ImageList*  piml,
        int         i,
        int         dxHotspot,
        int         dyHotspot
        );


    typedef
    BOOL
    (*ScrollDC_t)(
                HDC     hdc,
                int     dx,
                int     dy,
        const   RECT*   lprcScroll,
        const   RECT*   lprcClip ,
                HRGN    hrgnUpdate,
                RECT*   lprcUpdate
        );

    typedef
    int
    (*ScrollWindowEx_t)(
                HWND    hwndThis,
                int     dx,
                int     dy,
        const   RECT*   prcScroll,
        const   RECT*   prcClip ,
                HRGN    hrgnUpdate,
                RECT*   prcUpdate,
                UINT    flags
        );

    typedef
    BOOL
    (*OpenClipboard_t)(
        HWND    hWndNewOwner
        );

    typedef
    BOOL
    (*CloseClipboard_t)(
        void
        );


    typedef
    HWND
    (*GetClipboardOwner_t)(
        void
        );

    typedef
    HANDLE
    (*SetClipboardData_t)(
        UINT    uFormat,
        HANDLE  hMem
        );

    typedef
    HANDLE
    (*GetClipboardDataGwe_t)(
        UINT        uFormat,
        HPROCESS    hprocGetter,
        BOOL        fAlloc
        );

    typedef
    UINT
    (*RegisterClipboardFormatW_t)(
        const   WCHAR*  pFormatName
        );

    typedef
    int
    (*CountClipboardFormats_t)(
        void
        );

    typedef
    UINT
    (*EnumClipboardFormats_t)(
        UINT    Format
        );

    typedef
    int
    (*GetClipboardFormatNameW_t)(
        UINT    format,
        WCHAR*  pFormatName,
        int     cchMaxCount
        );

    typedef
    BOOL
    (*EmptyClipboard_t)(
        void
        );

    typedef
    BOOL
    (*IsClipboardFormatAvailable_t)(
        UINT    uFormat
        );

    typedef
    int
    (*GetPriorityClipboardFormat_t)(
        UINT*   paFormatPriorityList,
        int     cFormats
        );

    typedef
    HWND
    (*GetOpenClipboardWindow_t)(
        void
        );

    typedef
    BOOL
    (*MessageBeep_t)(
        UINT    uType
        );

    typedef
    void
    (*SystemIdleTimerReset_t)(
        void
        );

    typedef
    void
    (*SystemIdleTimerUpdateMax_t)(
        void
        );


    typedef
    HWND
    (*SetKeyboardTarget_t)(
        HWND    hwnd
        );

    typedef
    HWND
    (*GetKeyboardTarget_t)(
        void
        );

    typedef
    void
    (*NotifyWinUserSystem_t)(
        UINT    uEvent
        );

    typedef
    BOOL
    (*SetMenuItemInfoW_t)(
                HMENU           hmenu,
                UINT            uPosition,
                BOOL            fByPosition,
        const   MENUITEMINFO*   pmii
        );

    typedef
    BOOL
    (*GetMenuItemInfoW_t)(
        HMENU           hmenu,
        UINT            uPosition,
        BOOL            fByPosition,
        MENUITEMINFO*   pmii
        );

    typedef
    BOOL
    (*SetCaretBlinkTime_t)(
        unsigned int    uMSeconds
        );

    typedef
    unsigned int
    (*GetCaretBlinkTime_t)(
        void
        );

    typedef
    DWORD
    (*GetMessagePos_t)(
        void
        );

    typedef
    HHOOK
    (*QASetWindowsJournalHook_t)(
        int         nFilterType,
        HOOKPROC    pfnFilterProc,
        EVENTMSG*   pfnEventMsg
        );

    typedef
    BOOL
    (*QAUnhookWindowsJournalHook_t)(
        int nFilterType
        );

    typedef
    BOOL
    (*EnumWindows_t)(
        WNDENUMPROC      pEnumFunc,
        LPARAM                  lParam
        );

    typedef
    BOOL
    (*RectangleAnimation_t)(
                HWND    hwndThis,
        const   RECT*   prc,
                BOOL    fMaximize
        );

    typedef
    BOOL
    (*MapDialogRect_t)(
        HWND    hwnd,
        RECT*   lprc
        );

//  typedef
//  BOOL
//  (*GetSystemPowerStatusEx_t)(
//      SYSTEM_POWER_STATUS_EX* pstatus,
//      BOOL                    fUpdate
//      );

    typedef
    LONG
    (*GetDialogBaseUnits_t)(
        void
        );

    typedef
    UINT
    (*GetDoubleClickTime_t)(
        void
        );

    typedef
    DWORD
    (*GetWindowThreadProcessId_t)(
        HWND    hwndThis,
        DWORD*  lpdwProcessId
        );


    typedef
    HICON
    (*CreateIconIndirect_t)(
        ICONINFO*   pii
        );


    typedef
    void
    (*ShellModalEnd_t)(
        void
        );

    typedef
    BOOL
    (*TouchCalibrate_t)(
        void
        );


//  typedef
//  void
//  (*BatteryGetLifeTimeInfo_t)(
//      SYSTEMTIME* pstLastChange,
//      DWORD*      pcmsCpuUsage,
//      DWORD*      pcmsPreviousCpuUsage
//      );

    typedef
    void
    (*GwesPowerOff_t)(
        void
        );


    typedef
    HCURSOR
    (*LoadCursorW_t)(
        HINSTANCE      hInstance,
        DWORD           CursorNameResourceID,
    const   WCHAR*    pCursorName
        );

    typedef
    HCURSOR
    (*SetCursor_t)(
        HCURSOR hCursor
        );

    typedef
    BOOL
    (*DestroyCursor_t)(
        HCURSOR hCursor
        );

    typedef
    int
    (*DisableCaretSystemWide_t)(
        void
        );

    typedef
    int
    (*EnableCaretSystemWide_t)(
        void
        );

    typedef
    BOOL
    (*GetMouseMovePoints_t)(
        POINT * pPointBuffer,
        size_t      PointBufferSize,
        unsigned int    BufferPointsCount,
        unsigned int    *pPointsRetrievedCount
        );


    typedef
    BOOL
    (*EnableHardwareKeyboard_t)(
        BOOL    bEnable
        );

    typedef
    DWORD
    (*GetKeyboardStatus_t)(
        void
        );


    typedef
    BOOL
    (*RegisterSIPanel_t)(
        HWND    hwndSIPanel
        );

    typedef
    KEY_STATE_FLAGS
    (*GetAsyncShiftFlags_t)(
        UINT    VKey
        );

    typedef
    DWORD
    (*MsgWaitForMultipleObjectsEx_t)(
        DWORD   nCount,
        HANDLE* pHandles,
        DWORD   dwMilliseconds,
        DWORD   dwWakeMask,
        DWORD   dwFlags
        );

    typedef
    void
    (*SetAssociatedMenu_t)(
        HWND    hwndThis,
        HMENU   hmenu
        );

    typedef
    HMENU
    (*GetAssociatedMenu_t)(
        HWND    hwndThis
        );


    typedef
    BOOL
    (*DrawMenuBar_t)(
        HWND    hwnd
        );

    typedef
    BOOL
    (*SetSysColors_t)(
                int         cElements,
        const   int*        rgcolor,
        const   COLORREF*   rgrgb
        );

    typedef
    BOOL
    (*ExposedDrawFrameControl_t)(
        HDC     hdc,
        RECT*   prc,
        UINT    wType,
        UINT    wState
        );

    typedef
    HCURSOR
    (*CreateCursor_t)(
                HINSTANCE   hInst,
                int         xHotSpot,
                int         yHotSpot,
                int         nWidth,
                int         nHeight,
        const   void*       pvANDPlane,
        const   void*       pvXORPlane
        );

    typedef
    UINT
    (*RegisterWindowMessageW_t)(
        const   WCHAR*  pString
        );

    typedef
    BOOL
    (*SystemParametersInfoW_t)(
        UINT    uiAction,
        UINT    uiParam,
        void*   pvParam,
        UINT    fWinIni
        );

    typedef
    unsigned int
    (*SendInput_t)(
        unsigned int  InputsCount,
        INPUT * pInputs,
        size_t    InputsBufferSize,
        int     cbSize
        );

    typedef
    LONG
    (*SendDlgItemMessageW_t)(
        HWND    hDlg,
        int     nIDDlgItem,
        UINT    Msg,
        WPARAM  wParam,
        LPARAM  lParam
        );


    typedef
    BOOL
    (*SetDlgItemTextW_t)(
                HWND    hDlg,
                int     nIDDlgItem,
        const   WCHAR*  lpString
        );

    typedef
    UINT
    (*GetDlgItemTextW_t)(
        HWND    hDlg,
        int     nIDDlgItem,
        WCHAR*  lpString,
        int     nMaxCount
        );

    typedef
    unsigned int
    (*GetMessageSource_t)(
        void
        );

    typedef
    BOOL
    (*RegisterHotKey_t)(
        HWND    hWnd,
        int     id,
        UINT    fsModifiers,
        UINT    vk
        );

    typedef
    BOOL
    (*UnregisterHotKey_t)(
        HWND    hWnd,
        int     id
        );

    typedef
    BOOL
    (*Copy_t)(
        ImageList*  pimlDst,
        int         iDst,
        ImageList*  pimlSrc,
        int         iSrc,
        UINT        uFlags
        );

    typedef
    ImageList*
    (*Duplicate_t)(
        ImageList*  piml
        );

    typedef
    BOOL
    (*SetImageCount_t)(
        ImageList*  piml,
        UINT        uAlloc
        );

    typedef
    BOOL
    (*UnregisterFunc1_t)(
        UINT    fsModifiers,
        UINT    vk
        );

    typedef
    HIMC
    (*ImmGetContextFromWindowGwe_t)(
        HWND    hwnd
        );

    typedef
    BOOL
    (*ImmAssociateContextWithWindowGwe_t)(
        HWND    hwndThis,
        HIMC    himc,
        DWORD   dwFlags,
        HIMC*   phimcPrev,
        HWND*   phwndFocus,
        HIMC*   phimcFocusPrev,
        HIMC*   phimcFocusNew
        );

    typedef
    BOOL
    (*ImmSetHotKey_t)(
        DWORD   dwHotKeyId,
        UINT    uModifiers,
        UINT    uVkey,
        HKL     hkl
        );

    typedef
    HDWP
    (*BeginDeferWindowPos_t)(
        int nNumWindows
        );

    typedef
    HDWP
    (*DeferWindowPos_t)(    
        HDWP    hWinPosInfo,
        HWND    hWnd,
        HWND    hWndInsertAfter,
        int     x,
        int     y,
        int     cx,
        int     cy,
        UINT    uFlags
        );

    typedef
    BOOL
    (*EndDeferWindowPos_t)(
        HDWP    hWinPosInfo
        );

    typedef
    BOOL
    (*ImmGetHotKey_t)(
        DWORD   dwHotKeyId,
        UINT*   puModifiers,
        UINT*   puVkey,
        HKL*    phkl
        );



    typedef
    HDC
    (*GetDCEx_t)(
        HWND    hwndThis,
        HRGN    hrgnClip,
        DWORD   flags
        );

    typedef
    BOOL
    (*GwesPowerDown_t)(
        void
        );

    typedef
    void
    (*GwesPowerUp_t)(
        BOOL
        );

    typedef
    HKL
    (*LoadKeyboardLayoutW_t)(
    const WCHAR         *pKeybdLayoutID,
             size_t             KeybdLayoutIDBufferSize,
             unsigned int    Flags
        );


    typedef
    HKL
    (*ActivateKeyboardLayout_t)(
        HKL     hkl,
        unsigned int Flags
        );

//  typedef
//  DWORD
//  (*GetSystemPowerStatusEx2_t)(
//      SYSTEM_POWER_STATUS_EX2*    pstatus,
//      DWORD                       dwLen,
//      BOOL                        fUpdate
//      );

    typedef
    BOOL
    (*DisableGestures_t)(
        HWND hwnd,
        ULONGLONG *pullFlags,
        UINT uScope
        );

    typedef
    BOOL
    (*QueryGestures_t)(
        HWND hwnd,
        UINT uScope,
        PULONGLONG pullFlags);

    typedef
    BOOL
    (*EnableGestures_t)(
        HWND hwnd,
        ULONGLONG *pullFlags,
        UINT uScope
        );

    typedef
    BOOL
    (*GetKeyboardLayoutNameW_t)(
        WCHAR*  pszNameBuf
        );



    NotifyCallback_t                m_pNotifyCallback;              //  0
    GweCoreDllInfoFn_t              m_pGweCoreDllInfoFn;            //  1
    RegisterClassW_t              m_pRegisterClassW;   //  2
    UnregisterClassW_t              m_pUnregisterClassW;            //  3
    CreateWindowExW_t               m_pCreateWindowExW;             //  4
    PostMessageW_t                  m_pPostMessageW;                //  5
    PostQuitMessage_t               m_pPostQuitMessage;             //  6
    SendMessageW_t                  m_pSendMessageW;                //  7
    GetMessageW_t                   m_pGetMessageW;                 //  8
    TranslateMessage_t              m_pTranslateMessage;            //  9

    DispatchMessageW_t              m_pDispatchMessageW;            //  10
    GetCapture_t                    m_pGetCapture;                  //  11
    SetCapture_t                    m_pSetCapture;                  //  12
    ReleaseCapture_t                m_pReleaseCapture;              //  13
    SetWindowPos_t                  m_pSetWindowPos;                //  14
    GetWindowRect_t                 m_pGetWindowRect;               //  15
    GetClientRect_t                 m_pGetClientRect;               //  16
    InvalidateRect_t                m_pInvalidateRect;              //  17
    GetWindow_t                     m_pGetWindow;                   //  18
    GetSystemMetrics_t              m_pGetSystemMetrics;            //  19

    GetDragImage_t                  m_pGetDragImage;                //  20
    GetIconSize_t                   m_pGetIconSize;                 //  21
    SetIconSize_t                   m_pSetIconSize;                 //  22
    GetImageInfo_t                  m_pGetImageInfo;                //  23
    Merge_t                         m_pMerge;                       //  24
    ShowCursor_t                    m_pShowCursor;                  //  25
    SetCursorPos_t                  m_pSetCursorPos;                //  26
    CopyDitherImage_t               m_pCopyDitherImage;             //  27
    DrawIndirect_t                  m_pDrawIndirect;                //  28
    DragShowNolock_t                m_pDragShowNolock;              //  29

    WindowFromPoint_t               m_pWindowFromPoint;             //  30
    ChildWindowFromPoint_t          m_pChildWindowFromPoint;        //  31
    ClientToScreen_t                m_pClientToScreen;              //  32
    ScreenToClient_t                m_pScreenToClient;              //  33
    SetWindowTextW_t                m_pSetWindowTextW;              //  34
    GetWindowTextW_t                m_pGetWindowTextW;              //  35
    SetWindowLongW_t                m_pSetWindowLongW;              //  36
    GetWindowLongW_t                m_pGetWindowLongW;              //  37
    BeginPaint_t                    m_pBeginPaint;                  //  38
    EndPaint_t                      m_pEndPaint;                    //  39

    GetDC_t                         m_pGetDC;                       //  40
    ReleaseDC_t                     m_pReleaseDC;
    DefWindowProcW_t                m_pDefWindowProcW;
    GetClassLongW_t                 m_pGetClassLongW;
    SetClassLongW_t                 m_pSetClassLongW;
    DestroyWindow_t                 m_pDestroyWindow;
    ShowWindow_t                    m_pShowWindow;
    UpdateWindow_t                  m_pUpdateWindow;
    SetParent_t                     m_pSetParent;
    GetParent_t                     m_pGetParent;                   //  49

    MessageBoxW_t                   m_pMessageBoxW;                 //  50
    SetFocus_t                      m_pSetFocus;
    GetFocus_t                      m_pGetFocus;
    GetActiveWindow_t               m_pGetActiveWindow;
    GetWindowDC_t                   m_pGetWindowDC;
    GetSysColor_t                   m_pGetSysColor;
    AdjustWindowRectEx_t            m_pAdjustWindowRectEx;
    IsWindow_t                      m_pIsWindow;
    CreatePopupMenu_t               m_pCreatePopupMenu;
    InsertMenuW_t                   m_pInsertMenuW;                 //  59

    AppendMenuW_t                   m_pAppendMenuW;                 //  60
    RemoveMenu_t                    m_pRemoveMenu;
    DestroyMenu_t                   m_pDestroyMenu;
    TrackPopupMenuEx_t              m_pTrackPopupMenuEx;
    LoadMenuW_t                     m_pLoadMenuW;
    EnableMenuItem_t                m_pEnableMenuItem;
    MoveWindow_t                    m_pMoveWindow;
    GetUpdateRgn_t                  m_pGetUpdateRgn;
    GetUpdateRect_t                 m_pGetUpdateRect;
    BringWindowToTop_t              m_pBringWindowToTop;            //  69

    GetWindowTextLengthW_t          m_pGetWindowTextLengthW;        //  70
    IsChild_t                       m_pIsChild;
    IsWindowVisible_t               m_pIsWindowVisible;
    ValidateRect_t                  m_pValidateRect;
    LoadBitmapW_t                   m_pLoadBitmapW;
    CheckMenuItem_t                 m_pCheckMenuItem;
    CheckMenuRadioItem_t            m_pCheckMenuRadioItem;
    DeleteMenu_t                    m_pDeleteMenu;
    LoadIconW_t                     m_pLoadIconW;
    DrawIconEx_t                    m_pDrawIconEx;                  //  79

    DestroyIcon_t                   m_pDestroyIcon;             //  80
    GetAsyncKeyState_t              m_pGetAsyncKeyState;
    Unimplemented_t                 m_pUnimplemented82;          //  82

    DialogBoxIndirectParamW_t       m_pDialogBoxIndirectParamW;
    EndDialog_t                     m_pEndDialog;
    GetDlgItem_t                    m_pGetDlgItem;
    GetDlgCtrlID_t                  m_pGetDlgCtrlID;
    GetKeyState_t                   m_pGetKeyState;
    KeybdGetDeviceInfo_t            m_pKeybdGetDeviceInfo;
    KeybdInitStates_t               m_pKeybdInitStates;         //  89

    PostKeybdMessage_t              m_pPostKeybdMessage;            //  90
    KeybdVKeyToUnicode_t            m_pKeybdVKeyToUnicode;
    keybd_event_t                   m_pkeybd_event;
    mouse_event_t                   m_pmouse_event;
    SetScrollInfo_t                 m_pSetScrollInfo;
    SetScrollPos_t                  m_pSetScrollPos;
    SetScrollRange_t                m_pSetScrollRange;
    GetScrollInfo_t                 m_pGetScrollInfo;
    PeekMessageW_t                  m_pPeekMessageW;
    MapVirtualKeyW_t                m_pMapVirtualKeyW;              //  99

    GetMessageWNoWait_t             m_pGetMessageWNoWait;           //  100
    GetClassNameW_t                 m_pGetClassNameW;
    MapWindowPoints_t               m_pMapWindowPoints;
    LoadImageW_t                    m_pLoadImageW;
    GetForegroundWindow_t           m_pGetForegroundWindow;
    SetForegroundWindow_t           m_pSetForegroundWindow;
    RegisterTaskBar_t               m_pRegisterTaskBar;
    SetActiveWindow_t               m_pSetActiveWindow;
    CallWindowProcW_t            m_pCallWindowProcW;
    GetClassInfoW_t                 m_pGetClassInfoW;               //  109

    GetNextDlgTabItem_t             m_pGetNextDlgTabItem;           //  110
    CreateDialogIndirectParamW_t    m_pCreateDialogIndirectParamW;
    IsDialogMessageW_t              m_pIsDialogMessageW;
    SetDlgItemInt_t                 m_pSetDlgItemInt;
    GetDlgItemInt_t                 m_pGetDlgItemInt;
    FindWindowW_t                   m_pFindWindowW;
    CreateCaret_t                   m_pCreateCaret;
    DestroyCaret_t                  m_pDestroyCaret;
    HideCaret_t                     m_pHideCaret;
    ShowCaret_t                     m_pShowCaret;                   //  119

    SetCaretPos_t                   m_pSetCaretPos;             //  120
    GetCaretPos_t                   m_pGetCaretPos;
    GetCursorPos_t                  m_pGetCursorPos;
    ClipCursor_t                    m_pClipCursor;
    GetClipCursor_t                 m_pGetClipCursor;
    GetCursor_t                     m_pGetCursor;
    ExtractIconExW_t                m_pExtractIconExW;
    SetTimer_t                      m_pSetTimer;
    KillTimer_t                     m_pKillTimer;
    GetNextDlgGroupItem_t           m_pGetNextDlgGroupItem;     //  129

    CheckRadioButton_t              m_pCheckRadioButton;            //  130
    EnableWindow_t                  m_pEnableWindow;
    IsWindowEnabled_t               m_pIsWindowEnabled;
    CreateMenu_t                    m_pCreateMenu;
    GetSubMenu_t                    m_pGetSubMenu;
    DefDlgProcW_t                   m_pDefDlgProcW;
    SendNotifyMessageW_t            m_pSendNotifyMessageW;
    PostThreadMessageW_t            m_pPostThreadMessageW;
    TranslateAcceleratorW_t         m_pTranslateAcceleratorW;
    GetKeyboardLayout_t             m_pGetKeyboardLayout;           //  139

    GetKeyboardLayoutList_t         m_pGetKeyboardLayoutList;       //  140
    GetKeyboardType_t               m_pGetKeyboardType;
    Create_t                        m_pCreate;
    Destroy_t                       m_pDestroy;
    GetImageCount_t                 m_pGetImageCount;
    Add_t                           m_pAdd;
    ReplaceIcon_t                   m_pReplaceIcon;
    SetBkColor_t                    m_pSetBkColor;
    GetBkColor_t                    m_pGetBkColor;
    SetOverlayImage_t               m_pSetOverlayImage;         //  149

    Draw_t                          m_pDraw;                        //  150
    Replace_t                       m_pReplace;
    AddMasked_t                     m_pAddMasked;
    DrawEx_t                        m_pDrawEx;
    Remove_t                        m_pRemove;
    GetIcon_t                       m_pGetIcon;
    LoadImage_t                     m_pLoadImage;
    BeginDrag_t                     m_pBeginDrag;
    EndDrag_t                       m_pEndDrag;
    DragEnter_t                     m_pDragEnter;                   //  159

    DragLeave_t                     m_pDragLeave;                   //  160
    DragMove_t                      m_pDragMove;
    SetDragCursorImage_t            m_pSetDragCursorImage;
    Unimplemented_t                 m_pUnimplementedEntry163;
    ScrollDC_t                      m_pScrollDC;
    ScrollWindowEx_t                m_pScrollWindowEx;
    OpenClipboard_t                 m_pOpenClipboard;
    CloseClipboard_t                m_pCloseClipboard;
    GetClipboardOwner_t             m_pGetClipboardOwner;
    SetClipboardData_t              m_pSetClipboardData;            //  169

    GetClipboardDataGwe_t           m_pGetClipboardDataGwe;     //  170
    RegisterClipboardFormatW_t      m_pRegisterClipboardFormatW;
    CountClipboardFormats_t         m_pCountClipboardFormats;
    EnumClipboardFormats_t          m_pEnumClipboardFormats;
    GetClipboardFormatNameW_t       m_pGetClipboardFormatNameW;
    EmptyClipboard_t                m_pEmptyClipboard;
    IsClipboardFormatAvailable_t    m_pIsClipboardFormatAvailable;
    GetPriorityClipboardFormat_t    m_pGetPriorityClipboardFormat;
    GetOpenClipboardWindow_t        m_pGetOpenClipboardWindow;
    MessageBeep_t                   m_pMessageBeep;             //  179

    SystemIdleTimerReset_t                  m_pSystemIdleTimerReset;    //  180
    SystemIdleTimerUpdateMax_t              m_pSystemIdleTimerUpdateMax;
    Unimplemented_t                         m_pUnimplemented182;
    SetKeyboardTarget_t                     m_pSetKeyboardTarget;
    GetKeyboardTarget_t                     m_pGetKeyboardTarget;
    NotifyWinUserSystem_t                   m_pNotifyWinUserSystem;
    SetMenuItemInfoW_t                      m_pSetMenuItemInfoW;
    GetMenuItemInfoW_t                      m_pGetMenuItemInfoW;
    SetCaretBlinkTime_t                     m_pSetCaretBlinkTime;
    GetCaretBlinkTime_t                     m_pGetCaretBlinkTime;       //  189

    GetMessagePos_t                         m_pGetMessagePos;           //  190
    QASetWindowsJournalHook_t               m_pQASetWindowsJournalHook;
    QAUnhookWindowsJournalHook_t            m_pQAUnhookWindowsJournalHook;
    Unimplemented_t                         m_pUnimplemented193;  // 193

    DisableGestures_t                       m_pDisableGestures;         //  194
    EnumWindows_t                           m_pEnumWindows;
    RectangleAnimation_t                    m_pRectangleAnimation;
    MapDialogRect_t                         m_pMapDialogRect;
    Unimplemented_t                                 m_pUnimplemented198;
    GetDialogBaseUnits_t                    m_pGetDialogBaseUnits;      //  199

    GetDoubleClickTime_t                    m_pGetDoubleClickTime;      //  200
    GetWindowThreadProcessId_t              m_pGetWindowThreadProcessId;
    CreateIconIndirect_t                    m_pCreateIconIndirect;
    ShellModalEnd_t                         m_pShellModalEnd;
    TouchCalibrate_t                        m_pTouchCalibrate;
    QueryGestures_t                         m_pQueryGestures;           // 205
    EnableGestures_t                        m_pEnableGestures;
    GwesPowerOff_t                          m_pGwesPowerOff;
    Unimplemented_t                                 m_pUnimplemented208;
    LoadCursorW_t                           m_pLoadCursorW;             //  209

    SetCursor_t                             m_pSetCursor;               //  210
    DestroyCursor_t                         m_pDestroyCursor;
    DisableCaretSystemWide_t                m_pDisableCaretSystemWide;
    EnableCaretSystemWide_t                 m_pEnableCaretSystemWide;
    GetMouseMovePoints_t                    m_pGetMouseMovePoints;
    Unimplemented_t                         m_pUnimplemented215;  // 215
    EnableHardwareKeyboard_t                m_pEnableHardwareKeyboard;
    GetKeyboardStatus_t                     m_pGetKeyboardStatus;
    RegisterSIPanel_t                       m_pRegisterSIPanel;
    GetAsyncShiftFlags_t                    m_pGetAsyncShiftFlags;      //  219

    MsgWaitForMultipleObjectsEx_t           m_pMsgWaitForMultipleObjectsEx; //  220
    SetAssociatedMenu_t                     m_pSetAssociatedMenu;
    GetAssociatedMenu_t                     m_pGetAssociatedMenu;
    DrawMenuBar_t                           m_pDrawMenuBar;
    SetSysColors_t                          m_pSetSysColors;
    ExposedDrawFrameControl_t               m_pExposedDrawFrameControl;
    CreateCursor_t                          m_pCreateCursor;
    RegisterWindowMessageW_t                m_pRegisterWindowMessageW;
    SystemParametersInfoW_t                 m_pSystemParametersInfoW;
    SendInput_t                             m_pSendInput;                   //  229

    SendDlgItemMessageW_t                   m_pSendDlgItemMessageW;         //  230
    SetDlgItemTextW_t                       m_pSetDlgItemTextW;
    GetDlgItemTextW_t                       m_pGetDlgItemTextW;
    GetMessageSource_t                      m_pGetMessageSource;
    RegisterHotKey_t                        m_pRegisterHotKey;
    UnregisterHotKey_t                      m_pUnregisterHotKey;
    Copy_t                                  m_pCopy;
    Duplicate_t                             m_pDuplicate;
    SetImageCount_t                         m_pSetImageCount;
    UnregisterFunc1_t                       m_pUnregisterFunc1;             //  239

    ImmGetContextFromWindowGwe_t            m_pImmGetContextFromWindowGwe;  //  240
    ImmAssociateContextWithWindowGwe_t      m_pImmAssociateContextWithWindowGwe;
    ImmSetHotKey_t                          m_pImmSetHotKey;
    BeginDeferWindowPos_t                   m_pBeginDeferWindowPos;
    DeferWindowPos_t                        m_pDeferWindowPos;
    EndDeferWindowPos_t                     m_pEndDeferWindowPos;
    ImmGetHotKey_t                          m_pImmGetHotKey;
    GetDCEx_t                               m_pGetDCEx;
    GwesPowerDown_t                         m_pGwesPowerDown;
    GwesPowerUp_t                           m_pGwesPowerUp;                 //  249

    Unimplemented_t                                 m_pUnimplemented250;            //  250
    Unimplemented_t                                 m_pUnimplemented251;
    LoadKeyboardLayoutW_t                   m_pLoadKeyboardLayoutW;
    ActivateKeyboardLayout_t                m_pActivateKeyboardLayout;
    Unimplemented_t                         m_pUnimplemented254;
    GetKeyboardLayoutNameW_t                m_pGetKeyboardLayoutNameW;

};

class GweApiSet2_t
{
public:

    typedef
    void
    (*Unimplemented_t)(
        void
        );


    typedef
    void
    (*GDINotifyCallback_t)(
        DWORD   cause,
        DWORD   proc,
        DWORD   thread
        );

    typedef
    int
    (*AddFontResourceW_t)(
        const   WCHAR*  lpszFilename
        );

    typedef
    BOOL
    (*BitBlt_t)(
        HDC,
        int,
        int,
        int,
        int,
        HDC,
        int,
        int,
        DWORD
        );

    typedef
    int
    (*CombineRgn_t)(
        HRGN,
        HRGN,
        HRGN,
        int
        );

    typedef
    HDC
    (*CreateCompatibleDC_t)(
        HDC hdcIn
        );

    typedef
    HBRUSH
    (*CreateDIBPatternBrushPt_t)(
        const   void*,
                unsigned int
        );

    typedef
    HBITMAP
    (*CreateDIBSectionStub_t)(
                HDC             hdc,
                BOOL            fAllowNullDC,
        const   BITMAPINFO*     pbmi,
                void*           pvBits,
                unsigned int    iUsage
        );


    typedef
    HFONT
    (*CreateFontIndirectW_t)(
        const   LOGFONTW*
        );


    typedef
    HRGN
    (*CreateRectRgnIndirect_t)(
        const   RECT*
        );

    typedef
    HPEN
    (*CreatePenIndirect_t)(
        const LOGPEN*   lplgpn
        );

    typedef
    HBRUSH
    (*CreateSolidBrush_t)(
        COLORREF    crColor
        );

    typedef
    BOOL
    (*DeleteDC_t)(
        HDC hdc
        );

    typedef
    BOOL
    (*DeleteObject_t)(
        HGDIOBJ hgdiobj,
        LPVOID *ppvDIBUserMem
        );

    typedef
    BOOL
    (*DrawEdge_t)(
        HDC,
        RECT*,
        unsigned int,
        unsigned int
        );

    typedef
    BOOL
    (*DrawFocusRect_t)(
                HDC,
        const   RECT*
    );

    typedef
    int
    (*DrawTextW_t)(
                HDC,
        const   WCHAR*,
                int,
                RECT*,
                unsigned int
        );

    typedef
    BOOL
    (*Ellipse_t)(
        HDC,
        int,
        int,
        int,
        int
        );

    typedef
    int
    (*EnumFontFamiliesW_t)(
                HDC,
        const   WCHAR*,
                FONTENUMPROC,
                LPARAM
        );

    typedef
    int
    (*EnumFontsW_t)(
                HDC             hdc,
        const   WCHAR*          lpFaceName,
                FONTENUMPROC    lpFontFunc,
                LPARAM          lParam
        );



    typedef
    int
    (*ExcludeClipRect_t)(
        HDC,
        int,
        int,
        int,
        int
        );


    typedef
    BOOL
    (*ExtTextOutW_t)(
                HDC             hdc,
                int             X,
                int             Y,
                unsigned int    fuOptions,
        const   RECT*           lprc,
        const   WCHAR*          lpszString,
                unsigned int    cbCount,
        const   int*            lpDx
        );

    typedef
    int
    (*FillRect_t)(
                HDC,
        const   RECT*,
                HBRUSH
        );


    typedef
    COLORREF
    (*GetBkColor_t)(
        HDC hdc
        );

    typedef
    int
    (*GetBkMode_t)(
        HDC hdc
        );

    typedef
    int
    (*GetClipRgn_t)(
        HDC,
        HRGN
        );


    typedef
    HGDIOBJ
    (*GetCurrentObject_t)(
        HDC             hdc,
        unsigned int    uObjectType
        );


    typedef
    int
    (*GetDeviceCaps_t)(
        HDC hdc,
        int nIndex
        );

    typedef
    COLORREF
    (*GetNearestColor_t)(
        HDC,
        COLORREF
        );


    typedef
    int
    (*GetObjectW_t)(
        HGDIOBJ hgdiobj,
        int     cbBuffer,
        void*   lpvObject
        );

    typedef
    DWORD
    (*GetObjectType_t)(
        HGDIOBJ hgdiobj
        );

    typedef
    COLORREF
    (*GetPixel_t)(
        HDC,
        int,
        int
        );


    typedef
    DWORD
    (*GetRegionData_t)(
        HRGN,
        DWORD,
        RGNDATA*
        );

    typedef
    int
    (*GetRgnBox_t)(
        HRGN,
        RECT*
        );

    typedef
    HGDIOBJ
    (*GetStockObject_t)(
        int IdxObject
        );

    typedef
    BOOL
    (*PatBlt_t)(
        HDC,
        int,
        int,
        int,
        int,
        DWORD
        );

    typedef
    COLORREF
    (*GetTextColor_t)(
        HDC hdc
        );

    typedef
    BOOL
    (*GetTextExtentExPointW_t)(
                HDC     hdc,
        const   WCHAR*  lpszStr,
                int     cchString,
                int     nMaxExtent,
                int*    lpnFit,
                int*    alpDx,
                SIZE*   lpSize
                );

    typedef
    int
    (*GetTextFaceW_t)(
        HDC,
        int,
        WCHAR*
        );

    typedef
    BOOL
    (*GetTextMetricsW_t)(
        HDC         hdc,
        TEXTMETRIC* lptm
        );

    typedef
    UINT
    (*GetOutlineTextMetricsW_t)(
        HDC         hdc,
        UINT        cbData,
        LPOUTLINETEXTMETRICW lpOTM
        );

    typedef
    BOOL
    (*MaskBlt_t)(
        HDC,
        int,
        int,
        int,
        int,
        HDC,
        int,
        int,
        HBITMAP,
        int,
        int,
        DWORD
        );

    typedef
    int
    (*OffsetRgn_t)(
        HRGN,
        int,
        int
        );

    typedef
    BOOL
    (*Polygon_t)(
                HDC,    
        const   POINT*,
                int
        );

    typedef
    BOOL
    (*Polyline_t)(
                HDC,
        const   POINT*,
                int
                );

    typedef
    BOOL
    (*PtInRegion_t)(
        HRGN,
        int,
        int
        );

    typedef
    BOOL
    (*Rectangle_t)(
        HDC,
        int,
        int,
        int,
        int
        );

    typedef
    BOOL
    (*RectInRegion_t)(
                HRGN,
        const   RECT*
        );

    typedef
    BOOL
    (*RemoveFontResourceW_t)(
        const   WCHAR*  lpFileName
        );

    typedef
    BOOL
    (*RestoreDC_t)(
        HDC,
        int
        );

    typedef
    BOOL
    (*RoundRect_t)(
        HDC,
        int,
        int,
        int,
        int,
        int,
        int
        );

    typedef
    int
    (*SaveDC_t)(
        HDC
        );

    typedef
    int
    (*SelectClipRgn_t)(
        HDC     hdc,
        HRGN    hrgn
        );

    typedef
    HGDIOBJ
    (*SelectObject_t)(
        HDC     hdc,
        HGDIOBJ hgdiobj
        );

    typedef
    COLORREF
    (*SetBkColor_t)(
        HDC         hdc,
        COLORREF    crColor
        );

    typedef
    int
    (*SetBkMode_t)(
        HDC hdc,
        int iBkMode
        );


    typedef
    BOOL
    (*SetBrushOrgEx_t)(
        HDC,
        int,
        int,
        POINT*
        );

    typedef
    COLORREF
    (*SetPixel_t)(
        HDC,
        int,
        int,
        COLORREF
        );

    typedef
    COLORREF
    (*SetTextColor_t)(
        HDC         hdc,
        COLORREF    crColor
        );

    typedef
    BOOL
    (*StretchBlt_t)(
        HDC,
        int,
        int,
        int,
        int,
        HDC,
        int,
        int,
        int,
        int,
        DWORD
        );

    typedef
    HBITMAP
    (*CreateBitmap_t)(
                int,
                int,
                unsigned int,
                unsigned int,
        const   void*
        );

    typedef
    HBITMAP
    (*CreateCompatibleBitmap_t)(
        HDC,
        int,
        int
        );

    typedef
    HBRUSH
    (*GetSysColorBrush_t)(
        int
        );

    typedef
    int
    (*IntersectClipRect_t)(
        HDC,
        int,
        int,
        int,
        int
        );

    typedef
    int
    (*GetClipBox_t)(
        HDC,
        RECT*
        );

    typedef
    BOOL
    (*CeRemoveFontResource_t)(
        CEOID   oid
        );

    typedef
    BOOL
    (*EnableEUDC_t)(
        BOOL    fEnableEUDC
        );

    typedef
    HENHMETAFILE
    (*CloseEnhMetaFile_t)(
        HDC
        );

    typedef
    HDC
    (*CreateEnhMetaFileW_t)(
                HDC,
        const   WCHAR*,
        const   RECT*,
        const   WCHAR*
        );

    typedef
    BOOL
    (*DeleteEnhMetaFile_t)(
        HENHMETAFILE
        );

    typedef
    BOOL
    (*PlayEnhMetaFile_t)(
                HDC,
                HENHMETAFILE,
        const   RECT*
        );

    typedef
    HPALETTE
    (*CreatePalette_t)(
        const   LOGPALETTE*
        );

    typedef
    HPALETTE
    (*SelectPalette_t)(
        HDC,
        HPALETTE,
        BOOL
        );

    typedef
    unsigned int
    (*RealizePalette_t)(
        HDC
        );

    typedef
    unsigned int
    (*GetPaletteEntries_t)(
        HPALETTE,
        unsigned int,
        unsigned int,
        PALETTEENTRY*
        );

    typedef
    unsigned int
    (*SetPaletteEntries_t)(
                HPALETTE,
                unsigned int,
                unsigned int,
        const   PALETTEENTRY*
        );

    typedef
    unsigned int
    (*GetSystemPaletteEntries_t)(
        HDC,
        unsigned int,
        unsigned int,
        PALETTEENTRY*
        );

    typedef
    unsigned int
    (*GetNearestPaletteIndex_t)(
        HPALETTE,
        COLORREF
        );

    typedef
    HPEN
    (*CreatePen_t)(
        int,
        int,
        COLORREF
        );

    typedef
    int
    (*StartDocW_t)(
                HDC,    
        const   DOCINFOW*
        );

    typedef
    int
    (*EndDoc_t)(
        HDC
        );


    typedef
    int
    (*StartPage_t)(
        HDC
        );

    typedef
    int
    (*EndPage_t)(
        HDC
        );


    typedef
    int
    (*AbortDoc_t)(
        HDC
        );

    typedef
    int
    (*SetAbortProc_t)(
        HDC,
        ABORTPROC
        );

    typedef
    HDC
    (*CreateDCW_t)(
        const   WCHAR*,
        const   WCHAR*,
        const   WCHAR*,
        const   DEVMODEW*
        );

    typedef
    HRGN
    (*CreateRectRgn_t)(
        int,
        int,
        int,
        int
        );

    typedef
    BOOL
    (*FillRgn_t)(
        HDC,
        HRGN,
        HBRUSH
        );

    typedef
    int
    (*SetROP2_t)(
        HDC,
        int
        );


    typedef
    BOOL
    (*SetRectRgn_t)(
        HRGN,
        int,
        int,
        int,
        int
        );


    typedef
    BOOL
    (*RectVisible_t)(
                HDC,
        const   RECT*
        );

    typedef
    HBRUSH
    (*CreatePatternBrush_t)(
        HBITMAP
        );


    typedef
    HBITMAP
    (*CreateBitmapFromPointer_t)(
        const   BITMAPINFO*,
                int,
                void*
        );

    typedef
    BOOL
    (*SetViewportOrgEx_t)(
        HDC     hdc,
        int     nXOrg,
        int     nYOrg,
        POINT*  lppt
        );

    typedef
    BOOL
    (*TransparentImage_t)(
        HDC,
        int,
        int,
        int,
        int,
        HANDLE,
        int,
        int,
        int,
        int,
        COLORREF
        );

    typedef
    BOOL
    (*GdiSetObjectOwner_t)(
        HGDIOBJ,
        HPROCESS
        );

    typedef
    BOOL
    (*TranslateCharsetInfo_t)(
        DWORD*          lpSrc,
        CHARSETINFO*    lpCs,
        DWORD           dwFlags
        );


    typedef
    int
    (*ExtEscape_t)(
                HDC,
                int,
                int,
        const   char*,
                int,
                char*
        );


    typedef
    HHOOK
    (*SetWindowsHookExW_t)(
        int         idHook,
        HOOKPROC    lpfn,
        HINSTANCE   hmod,
        DWORD       dwThreadId
        );

    typedef
    BOOL
    (*UnhookWindowsHookEx_t)(
        HHOOK   hhk
        );


    typedef
    BOOL
    (*GetForegroundInfo_t)(
        GET_FOREGROUND_INFO*    pgfi
        );



    typedef
    int
    (*SetWindowRgn_t)(
        HWND    hwndThis,
        HRGN    hrgn,
        BOOL    bRedraw
        );


    typedef
    int
    (*GetWindowRgn_t)(
        HWND    hwndThis,
        HRGN    hrgn
        );

    typedef
    HWND
    (*GetDesktopWindow_t)(
        void
        );

    typedef
    BOOL
    (*InSendMessage_t)(
        void
        );

    typedef
    DWORD
    (*GetQueueStatus_t)(
        unsigned int    flags
        );

    typedef
    BOOL
    (*AllKeys_t)(
        BOOL    bAllKeys
        );

    typedef
    HCURSOR
    (*LoadAnimatedCursor_t)(
        HINSTANCE   hinst,
        DWORD       ResourceId,
        int         cFrames,
        int         FrameTimeInterval
        );

    typedef
    LRESULT
    (*SendMessageTimeout_t)(
        HWND            hWnd,
        unsigned int    uMsg,
        WPARAM          wParam,
        LPARAM          lParam,
        unsigned int    fuFlags,
        unsigned int    uTimeout,
        DWORD_PTR*      lpdwResult
        );

    typedef
    BOOL
    (*SetProp_t)(
              HWND   hWnd,
              ATOM   PropertyIdentifier,
        const WCHAR* pPropertyName,
              HANDLE hData
        );

    typedef
    HANDLE
    (*GetProp_t)(
              HWND   hWnd,
              ATOM   PropertyIdentifier,
        const WCHAR* pPropertyName
        );

    typedef
    HANDLE
    (*RemoveProp_t)(
              HWND   hWnd,
              ATOM   PropertyIdentifier,
        const WCHAR* pPropertyName
        );

    typedef
    int
    (*EnumPropsEx_t)(
        HWND            hWnd,
        PROPENUMPROCEX  lpEnumFunc,
        LPARAM          lParam
        );


    typedef
    DWORD
    (*GetMessageQueueReadyTimeStamp_t)(
        HWND    hWnd
        );

    typedef
    BOOL
    (*RegisterTaskBarEx_t)(
        HWND    hwndTaskBar,
        BOOL    bTaskBarOnTop
        );

    typedef
    BOOL
    (*RegisterDesktop_t)(
        HWND    hwndDesktop
        );

    typedef
    ATOM
    (*GlobalAddAtomW_t)(
        ATOM           Atom,
    const WCHAR*  pString
        );


    typedef
    ATOM
    (*GlobalDeleteAtom_t)(
        ATOM    nAtom
        );

    typedef
    ATOM
    (*GlobalFindAtomW_t)(
        ATOM           Atom,
    const WCHAR*  pString
        );

    typedef
    HMONITOR
    (*MonitorFromPoint_t)(
        POINT   pt,
        DWORD   dwFlag
        );

    typedef
    HMONITOR
    (*MonitorFromRect_t)(
        const   RECT*   lprc,
                DWORD   dwFlag
        );


    typedef
    HMONITOR
    (*MonitorFromWindow_t)(
        HWND    hwnd,
        DWORD   dwFlag
        );

    typedef
    BOOL
    (*GetMonitorInfo_t)(
        HMONITOR        hMonitor,
        MONITORINFO*    lpmi
        );


    typedef
    BOOL
    (*EnumDisplayMonitors_t)(
                HDC             hdcPaint,
        const   RECT*           lprcPaint,
                MONITORENUMPROC lpfnEnum,
                LPARAM          lData,
                HPROCESS        hProcCallingContext
        );

    typedef
    void
    (*AccessibilitySoundSentryEvent_t)(
        void
        );


    typedef
    LONG
    (*ChangeDisplaySettingsEx_t)(
        const   WCHAR*      lpDeviceName,
                DEVMODE*    lpDevMode,
                HWND        hwnd,
                DWORD       dwflags,
                void*       lParam
        );

    typedef
    BOOL
    (*InvalidateRgn_t)(
        HWND    hwndThis,
        HRGN    hRgn,
        BOOL    fErase
        );

    typedef
    BOOL
    (*ValidateRgn_t)(
        HWND    hwndThis,
        HRGN    hrgn
        );

    typedef
    HRGN
    (*ExtCreateRegion_t)(
        const   XFORM*,
                DWORD,
        const   RGNDATA*
        );

    typedef
    BOOL
    (*MoveToEx_t)(
        HDC,
        int,
        int,
        POINT*
        );

    typedef
    BOOL
    (*LineTo_t)(
        HDC,
        int,
        int
        );

    typedef
    BOOL
    (*GetCurrentPositionEx_t)(
        HDC,
        POINT*
        );

    typedef
    UINT
    (*SetTextAlign_t)(
        HDC         hdc,
        UINT        fMode
        );

    typedef
    UINT
    (*GetTextAlign_t)(
        HDC         hdc
        );

    typedef
    BOOL
    (*GetCharWidth32_t)(
        HDC     hdc,
        UINT    iFirstChar,
        UINT    iLastChar,
        int*    lpBuffer
        );

    typedef
    UINT
    (*GetDIBColorTable_t)(
        HDC             hdc,
        UINT            uStartIndex,
        UINT            cEntries,
        RGBQUAD*        pColors
        );

    typedef
    UINT
    (*SetDIBColorTable_t)(
                HDC         hdc,
                UINT        uStartIndex,
                UINT        cEntries,
        const   RGBQUAD*    pColors
        );

    typedef
    int
    (*StretchDIBits_t)(
                HDC         hdc,
                int         XDest,
                int         YDest,
                int         nDestWidth,
                int         nDestHeight,
                int         XSrc,
                int         YSrc,
                int         nSrcWidth,
                int         nSrcHeight,
        const   void*       lpBits,
        const   BITMAPINFO* lpBitsInfo,
                UINT        iUsage,
                DWORD       dwRop
        );


    typedef
    BOOL
    (*RedrawWindow_t)(
                HWND    hWnd,
        const   RECT*   lprcUpdate,
                HRGN    hrgnUpdate,
                UINT    flags
        );

    typedef
    LONG
    (*SetBitmapBits_t)(
                HBITMAP,
                DWORD,
        const   void*
        );

    typedef
    int
    (*SetDIBitsToDevice_t)(
                HDC             hdc,
                int             XDest,
                int             YDest,
                DWORD           dwWidth,
                DWORD           dwHeight,
                int             XSrc,
                int             YSrc,
                UINT            uStartScan,
                UINT            cScanLines,
        const   void*           lpvBits,
        const   BITMAPINFO*     lpbmi,
                UINT            fuColorUse
        );

    typedef
    BOOL
    (*GradientFill_t)(
        HDC         hdc,
        TRIVERTEX*  pVertex,
        ULONG       nVertex,
        void*       pMesh,
        ULONG       nCount,
        ULONG       ulMode
    );

    typedef
    BOOL
    (*InvertRect_t)(
                HDC,
        const   RECT*
        );


    typedef
    BOOL
    (*EnumDisplaySettings_t)(
        const   WCHAR*      lpszDeviceName,
                DWORD       iModeNum,
                DEVMODEW*   lpDevMode
        );

    typedef
    BOOL
    (*EnumDisplayDevices_t)(
        const   WCHAR*          lpDevice,
                DWORD           iModeNum,
                DISPLAY_DEVICE* lpDevMode,
                DWORD           dwFlags
        );

    typedef
    BOOL
    (*GetCharABCWidths_t)(
        HDC     hdc,
        UINT    uFirstChar,
        UINT    uLastChar,
        ABC*    lpabc
        );

    typedef
    BOOL
    (*GetCharABCWidthsI_t)(
        HDC hdc,         
        UINT giFirst,    
        UINT cgi,        
        LPWORD pgi,      
        LPABC lpabc      
        );

    typedef
    BOOL
    (*ShowStartupWindow_t)(
        void
        );

    typedef
    DWORD
    (*GetFontData_t)(
        HDC hdc,
        DWORD   dwTable,
        DWORD   dwOffset,
        LPVOID  lpvBuffer,
        DWORD   cbData
        );

    typedef
    BOOL
    (*GetGweApiSetTables_t)(
        GweApiSet1_t *pApiSet1,
        GweApiSet2_t *pApiSet2
        );

    typedef
    int
    (*GetStretchBltMode_t)(
        HDC
        );

    typedef
    int
    (*SetStretchBltMode_t)(
        HDC,
        int
        );
    typedef
    BOOL
    (*AlphaBlend_t)(
        HDC,
        int,
        int,
        int,
        int,
        HDC,
        int,
        int,
        int,
        int,
        BLENDFUNCTION
        );

    typedef
    BOOL
    (*GetIconInfo_t)(
        HICON,
        PICONINFO,
        size_t
        );

    typedef
    int
    (*EnumFontFamiliesExW_t)(
        HDC,
        LPLOGFONTW,
        FONTENUMPROC,
        LPARAM,
        DWORD
        );

    typedef
    DWORD
    (*SetLayout_t)(
        HDC,
        DWORD
        );

    typedef
    DWORD
    (*GetLayout_t)(
        HDC
        );

    typedef
    int
    (*SetTextCharacterExtra_t)(
        HDC,
        int
        );

    typedef
    int
    (*GetTextCharacterExtra_t)(
        HDC
        );
    
    typedef
        DWORD
    (*ImmAssociateValueWithGwesMessageQueue_t)(
        DWORD,
        UINT
        );

    typedef
    BOOL
    (*GetViewportOrgEx_t)(
        HDC     hdc,
        POINT*  lppt
        );

    typedef
    BOOL
    (*GetViewportExtEx_t)(
        HDC     hdc,
        SIZE*   lpSize
        );

    typedef
    BOOL
    (*OffsetViewportOrgEx_t)(        
        HDC hdc,         // handle to device context
        int nXOffset,    // horizontal offset
        int nYOffset,    // vertical offset
        POINT* lpPoint   // original origin
        );

    typedef
    int
    (*GetROP2_t)(
        HDC
        );

    typedef
    BOOL
    (*SetWindowOrgEx_t)(        
        HDC hdc,
        int X,
        int Y,
        POINT* lpPoint  
        );

    typedef
    BOOL
    (*GetWindowOrgEx_t)(        
        HDC hdc,
        POINT* lpPoint  
        );

    typedef
    BOOL
    (*GetWindowExtEx_t)(        
        HDC hdc,
        SIZE* lpSize  
        );

    typedef
    MsgQueue*
    (*GetCurrentThreadMessageQueue_t)(
        void
        );

    typedef
    HANDLE
    (*AddFontMemResourceEx_t)(
        PVOID,
        DWORD,
        PVOID,
        DWORD*
        );

    typedef
    BOOL
    (*RemoveFontMemResourceEx_t)(
        HANDLE
        );

    typedef
    BOOL
    (*SetDialogAutoScrollBar_t)(
        HWND hDlg
        );

    typedef
    BOOL
    (*SetWindowPosOnRotate_t)(
        HWND hWnd, 
        HWND hWndInsertAfter,
        int X,
        int Y,
        int cx,
        int cy,
        UINT uFlags,
        BOOL fNoScroll
        );

    typedef
    HRESULT
    (*AnimateRects_t)(
        ANIMATERECTSINFO *pari,
        DWORD cbari
        );

    typedef
    BOOL
    (*Gesture_t)(
        __in HWND,
        __in_bcount(cbInfo) const GESTUREINFO*,
        __in UINT cbInfo,
        __in_bcount(cbExtraArguments) const BYTE*,
        __in UINT cbExtraArguments
        );

    typedef
    BOOL
    (*GetWindowAutoGesture_t)(
        __in HWND,
        __out_bcount(cbInfo) LPWAGINFO,
        __in UINT cbInfo
        );

    typedef
    BOOL
    (*SetWindowAutoGesture_t)(
        __in HWND,
        __in_bcount(cbInfo) LPCWAGINFO,
        __in UINT cbInfo
        );

    typedef
    BOOL
    (*RegisterGesture_t)(
        const __nullterminated LPCWSTR pszName,
        __out PDWORD_PTR lpdwID
        );

    typedef
    BOOL
    (*RegisterDefaultGestureHandler_t)(
        HWND hwnd,
        BOOL fRegister
        );

    GDINotifyCallback_t         m_pGDINotifyCallback;       //  0
    Unimplemented_t                     m_pReserved2;
    AddFontResourceW_t          m_pAddFontResourceW;
    BitBlt_t                    m_pBitBlt;
    CombineRgn_t                m_pCombineRgn;
    CreateCompatibleDC_t        m_pCreateCompatibleDC;
    CreateDIBPatternBrushPt_t   m_pCreateDIBPatternBrushPt;
    CreateDIBSectionStub_t      m_pCreateDIBSectionStub;
    CreateFontIndirectW_t       m_pCreateFontIndirectW;
    CreateRectRgnIndirect_t     m_pCreateRectRgnIndirect;   //  9

    CreatePenIndirect_t         m_pCreatePenIndirect;       //  10
    CreateSolidBrush_t          m_pCreateSolidBrush;
    DeleteDC_t                  m_pDeleteDC;
    DeleteObject_t              m_pDeleteObject;
    DrawEdge_t                  m_pDrawEdge;
    DrawFocusRect_t             m_pDrawFocusRect;
    DrawTextW_t                 m_pDrawTextW;
    Ellipse_t                   m_pEllipse;
    EnumFontFamiliesW_t         m_pEnumFontFamiliesW;
    EnumFontsW_t                m_pEnumFontsW;              //  19

    ExcludeClipRect_t           m_pExcludeClipRect;         //  20
    ExtTextOutW_t               m_pExtTextOutW;
    FillRect_t                  m_pFillRect;
    Unimplemented_t                     m_pReserved23;
    GetBkColor_t                m_pGetBkColor;
    GetBkMode_t                 m_pGetBkMode;
    GetClipRgn_t                m_pGetClipRgn;
    GetCurrentObject_t          m_pGetCurrentObject;
    GetDeviceCaps_t             m_pGetDeviceCaps;
    GetNearestColor_t           m_pGetNearestColor;         //  29

    GetObjectW_t                m_pGetObjectW;              //  30
    GetObjectType_t             m_pGetObjectType;
    GetPixel_t                  m_pGetPixel;
    GetRegionData_t             m_pGetRegionData;
    GetRgnBox_t                 m_pGetRgnBox;
    GetStockObject_t            m_pGetStockObject;
    PatBlt_t                    m_pPatBlt;
    GetTextColor_t              m_pGetTextColor;
    GetTextExtentExPointW_t     m_pGetTextExtentExPointW;
    GetTextFaceW_t              m_pGetTextFaceW;            //  39

    GetTextMetricsW_t           m_pGetTextMetricsW;         //  40
    MaskBlt_t                   m_pMaskBlt;
    OffsetRgn_t                 m_pOffsetRgn;
    Polygon_t                   m_pPolygon;
    Polyline_t                  m_pPolyline;
    PtInRegion_t                m_pPtInRegion;
    Rectangle_t                 m_pRectangle;
    RectInRegion_t              m_pRectInRegion;
    RemoveFontResourceW_t       m_pRemoveFontResourceW;
    RestoreDC_t                 m_pRestoreDC;               //  49

    RoundRect_t                 m_pRoundRect;               //  50
    SaveDC_t                    m_pSaveDC;
    SelectClipRgn_t             m_pSelectClipRgn;
    SelectObject_t              m_pSelectObject;
    SetBkColor_t                m_pSetBkColor;
    SetBkMode_t                 m_pSetBkMode;
    SetBrushOrgEx_t             m_pSetBrushOrgEx;
    SetPixel_t                  m_pSetPixel;
    SetTextColor_t              m_pSetTextColor;
    StretchBlt_t                m_pStretchBlt;              //  59

    CreateBitmap_t              m_pCreateBitmap;            //  60
    CreateCompatibleBitmap_t    m_pCreateCompatibleBitmap;
    GetSysColorBrush_t          m_pGetSysColorBrush;
    IntersectClipRect_t         m_pIntersectClipRect;
    GetClipBox_t                m_pGetClipBox;
    CeRemoveFontResource_t      m_pCeRemoveFontResource;
    EnableEUDC_t                m_pEnableEUDC;
    CloseEnhMetaFile_t          m_pCloseEnhMetaFile;
    CreateEnhMetaFileW_t        m_pCreateEnhMetaFileW;
    DeleteEnhMetaFile_t         m_pDeleteEnhMetaFile;       //  69

    PlayEnhMetaFile_t           m_pPlayEnhMetaFile;         //  70
    CreatePalette_t             m_pCreatePalette;
    SelectPalette_t             m_pSelectPalette;
    RealizePalette_t            m_pRealizePalette;
    GetPaletteEntries_t         m_pGetPaletteEntries;
    SetPaletteEntries_t         m_pSetPaletteEntries;
    GetSystemPaletteEntries_t   m_pGetSystemPaletteEntries;
    GetNearestPaletteIndex_t    m_pGetNearestPaletteIndex;
    CreatePen_t                 m_pCreatePen;
    StartDocW_t                 m_pStartDocW;               //  79

    EndDoc_t                            m_pEndDoc;          //  80
    StartPage_t                         m_pStartPage;
    EndPage_t                           m_pEndPage;
    AbortDoc_t                          m_pAbortDoc;
    SetAbortProc_t                      m_pSetAbortProc;
        CreateDCW_t                         m_pCreateDCW;
    CreateRectRgn_t                     m_pCreateRectRgn;
    FillRgn_t                           m_pFillRgn;
    SetROP2_t                           m_pSetROP2;
    SetRectRgn_t                        m_pSetRectRgn;      //  89

    RectVisible_t                       m_pRectVisible;             //  90
    CreatePatternBrush_t                m_pCreatePatternBrush;
    CreateBitmapFromPointer_t           m_pCreateBitmapFromPointer;
    SetViewportOrgEx_t                  m_pSetViewportOrgEx;
    TransparentImage_t                  m_pTransparentImage;
    GdiSetObjectOwner_t                 m_pGdiSetObjectOwner;
    TranslateCharsetInfo_t              m_pTranslateCharsetInfo;
    ExtEscape_t                         m_pExtEscape;
    SetWindowsHookExW_t                 m_pSetWindowsHookExW;
    UnhookWindowsHookEx_t               m_pUnhookWindowsHookEx;     //  99

    GetForegroundInfo_t                 m_pGetForegroundInfo;                   //  100
    void                    *m_lpUnused1;
    void                    *m_lpUnused2;
    void                    *m_lpUnused3;
    void                    *m_lpUnused4;
    void                    *m_lpUnused5;
    void                    *m_lpUnused6;
    void                    *m_lpUnused7;
    void                    *m_lpUnused8;
    SetWindowRgn_t                      m_pSetWindowRgn;                        //  109

    void                    *m_lpUnused9;           //  110
    GetWindowRgn_t                      m_pGetWindowRgn;
    void                    *m_lpUnused10;
    GetDesktopWindow_t                  m_pGetDesktopWindow;
    InSendMessage_t                     m_pInSendMessage;
    GetQueueStatus_t                    m_pGetQueueStatus;
    AllKeys_t                           m_pAllKeys;
    LoadAnimatedCursor_t                m_pLoadAnimatedCursor;
    SendMessageTimeout_t                m_pSendMessageTimeout;
    SetProp_t                           m_pSetProp;                     //  119

    GetProp_t                           m_pGetProp;                     //  120
    RemoveProp_t                        m_pRemoveProp;
    EnumPropsEx_t                       m_pEnumPropsEx;
    GetMessageQueueReadyTimeStamp_t     m_pGetMessageQueueReadyTimeStamp;
    RegisterTaskBarEx_t                 m_pRegisterTaskBarEx;
    RegisterDesktop_t                   m_pRegisterDesktop;
    GlobalAddAtomW_t                    m_pGlobalAddAtomW;
    GlobalDeleteAtom_t                  m_pGlobalDeleteAtom;
    GlobalFindAtomW_t                   m_pGlobalFindAtomW;
    MonitorFromPoint_t                  m_pMonitorFromPoint;            //  129

    MonitorFromRect_t                   m_pMonitorFromRect;             //  130
    MonitorFromWindow_t                 m_pMonitorFromWindow;
    GetMonitorInfo_t                    m_pGetMonitorInfo;
    EnumDisplayMonitors_t               m_pEnumDisplayMonitors;
    AccessibilitySoundSentryEvent_t     m_pAccessibilitySoundSentryEvent;
    ChangeDisplaySettingsEx_t           m_pChangeDisplaySettingsEx;
    InvalidateRgn_t                     m_pInvalidateRgn;
    ValidateRgn_t                       m_pValidateRgn;
    ExtCreateRegion_t                   m_pExtCreateRegion;
    MoveToEx_t                          m_pMoveToEx;                //  139

    LineTo_t                            m_pLineTo;                  //  140
    GetCurrentPositionEx_t              m_pGetCurrentPositionEx;
    SetTextAlign_t                      m_pSetTextAlign;
    GetTextAlign_t                      m_pGetTextAlign;
    GetCharWidth32_t                    m_pGetCharWidth32;
    GetDIBColorTable_t                  m_pGetDIBColorTable;
    SetDIBColorTable_t                  m_pSetDIBColorTable;
    StretchDIBits_t                     m_pStretchDIBits;
    RedrawWindow_t                      m_pRedrawWindow;
    SetBitmapBits_t                     m_pSetBitmapBits;               //  149

    SetDIBitsToDevice_t                 m_pSetDIBitsToDevice;           //  150
    GradientFill_t                      m_pGradientFill;
    InvertRect_t                        m_pInvertRect;
    EnumDisplaySettings_t               m_pEnumDisplaySettings;
    EnumDisplayDevices_t                m_pEnumDisplayDevices;
    GetCharABCWidths_t                  m_pGetCharABCWidths;
    ShowStartupWindow_t                 m_pShowStartupWindow;
    GetGweApiSetTables_t                m_pGetGweApiSetTables;
    GetStretchBltMode_t                 m_pGetStretchBltMode;
    SetStretchBltMode_t                 m_pSetStretchBltMode;           //  159

    AlphaBlend_t                        m_pAlphaBlend;              //  160
    GetIconInfo_t                       m_pGetIconInfo;
    EnumFontFamiliesExW_t               m_pEnumFontFamiliesExW;
    GetFontData_t                       m_pGetFontData;             
    GetCharABCWidthsI_t                 m_pGetCharABCWidthsI;
    GetOutlineTextMetricsW_t            m_pGetOutlineTextMetricsW;
    SetLayout_t                         m_pSetLayout;
    GetLayout_t                         m_pGetLayout;
    SetTextCharacterExtra_t             m_pSetTextCharacterExtra;
    GetTextCharacterExtra_t             m_pGetTextCharacterExtra;           //  169
    
    ImmAssociateValueWithGwesMessageQueue_t     m_pImmAssociateValueWithGwesMessageQueue;       /* 170 */
    GetViewportOrgEx_t                  m_pGetViewportOrgEx;    
    GetViewportExtEx_t                  m_pGetViewportExtEx;        
    OffsetViewportOrgEx_t               m_pOffsetViewportOrgEx;            
    GetROP2_t                           m_pGetROP2;    
    SetWindowOrgEx_t                    m_pSetWindowOrgEx;                 // 175
    GetWindowOrgEx_t                    m_pGetWindowOrgEx;
    GetWindowExtEx_t                    m_pGetWindowExtEx;

    GetCurrentThreadMessageQueue_t      m_pGetCurrentThreadMessageQueue;

    AddFontMemResourceEx_t              m_pAddFontMemResourceEx;          // 179
    RemoveFontMemResourceEx_t           m_pRemoveFontMemResourceEx;       // 180    

    SetDialogAutoScrollBar_t            m_pSetDialogAutoScrollBar;        // 181
    SetWindowPosOnRotate_t              m_pSetWindowPosOnRotate;          // 182
    AnimateRects_t                      m_pAnimateRects;                  // 183

    Unimplemented_t                     m_pUnimplemented184;        // 184
    Gesture_t                           m_pGesture;                         // 185
    GetWindowAutoGesture_t              m_pGetWindowAutoGesture;            // 186
    SetWindowAutoGesture_t              m_pSetWindowAutoGesture;            // 187
    RegisterGesture_t                   m_pRegisterGesture;                 // 188
    RegisterDefaultGestureHandler_t     m_pRegisterDefaultGestureHandler;   // 189
};


#endif

