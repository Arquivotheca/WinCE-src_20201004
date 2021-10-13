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
#ifndef __DLGMGR_H__
#define __DLGMGR_H__

#ifndef _DLGMGR_
#define _DLGMGR_

#define DIALOGCLASSNAME TEXT("Dialog")

#ifndef __CE_PTR_HPP_INCLUDED__
#include <CePtr.hpp>
#endif

#pragma once
#include <AnimationBase.h>
#include <TouchUtil.h>

/* Dialog structure (dlg) */
//IMPORTANT: sizeof this structure must be == DLGWINDOWEXTRA.
typedef struct
{
    LPARAM  result;          // contains result code for a processed message

    DLGPROC pDialogProc;

    LONG    lUser;           // user space

    HANDLE  hProc;           // Process owning dlgproc   
    HWND    hwndFocus;       // control with current focus
    LONG    lresult;         // final return value for a modal dialog
    WORD    wDefID;          // Original DefID for the dialog
    WORD    fEnd       : 1;
    WORD    fPixels    : 1;
    HFONT   hUserFont;
} DLG, *PDLG;

#define DLGEX_SIGNATURE 0xFFFF
typedef struct tagDLGNEWTEMPLATE
{
    WORD    wDlgVer;
    WORD    wSignature;
    DWORD   dwHelpID;
    DWORD   dwExStyle;
    DWORD   dwStyle;
    BYTE    cDlgItems; // NOTE: if 32, this comes in as a WORD but is converted by cvtres32.asm in win32\win32c
    short   x;
    short   y;
    short   cx;
    short   cy;
}   DLGNEWTEMPLATE, FAR *LPDLGNEWTEMPLATE;
    /* MenuName
    ** ClassName
    ** Title
    ** if DS_SETFONT:  pointsize, weight, bItalic, bCharSet, FontName
    */

typedef struct tagNEWDLGITEMTEMPLATE
{
    DWORD   dwExStyle;
    DWORD   dwStyle;
    short   x;
    short   y;
    short   cx;
    short   cy;
    DWORD   wID;
} NEWDLGITEMTEMPLATE, FAR *LPNEWDLGITEMTEMPLATE;
    /* ClassName
    ** Title
    ** length word
    */

/*
 * Special case string token codes.  These must be the same as in the resource
 * compiler's RC.H file.
 */
/*
 * NOTE: Order is assumed and much be this way for applications to be
 * compatable with windows 2.0
 */
#define CODEBIT         0x80
#define BUTTONCODE      0x80
#define EDITCODE        0x81
#define STATICCODE      0x82
#define LISTBOXCODE     0x83
#define SCROLLBARCODE   0x84
#define COMBOBOXCODE    0x85

// Maximum number of iterations for
// keyboard navigation functions that
// are vulnerable to infinite loops.
#define KEYBDNAV_LOOP_MAX     512

// Flags to specify options for NextControl
#define DLG_NC_PREVIOUS                 0x00000001L // Iterate to previous control.
#define DLG_NC_LOOP                     0x00000002L // Root and last descendent are connected to form a loop.
#define DLG_NC_HWNDSTART_VALID          0x00000004L // Don't call GetLookupWindow on the start HWND.
#define DLG_NC_RETURN_VISIBLE_ENABLED   0x00000008L // Return value must have WS_VISIBLE set and WS_DISABLED unset.

void    CheckDefPushButton(HWND, HWND, HWND);
HWND    GetParentDialog(HWND hwndDlg);
HWND    NextControl(HWND hwndParent, HWND hwndStart, UINT uFlags);
HWND    GetLookupWindow(HWND hwndRoot, HWND hwnd);
HWND    FindDlgItem(HWND hwndParent, int id);
void    DlgSetFocus(HWND hwnd);
HWND    GotoNextMnem(HWND hwndDlg, HWND hwnd, WCHAR ch);

#define IsDefPushButton(hwnd)   \
    (MsgQueue::SendMessageW_I(hwnd,WM_GETDLGCODE,0,0L)&DLGC_DEFPUSHBUTTON)
#define IsUndefPushButton(hwnd)      \
    (MsgQueue::SendMessageW_I(hwnd,WM_GETDLGCODE,0,0L)&DLGC_UNDEFPUSHBUTTON)
#define IsPushButton(hwnd)      \
    (MsgQueue::SendMessageW_I(hwnd,WM_GETDLGCODE,0,0L)&(DLGC_UNDEFPUSHBUTTON|DLGC_DEFPUSHBUTTON))

// Flags to specify movement across dialog items
#define CHANGEDLGITEM_TYPE_TAB      0x00000001 // Change the tabbing position
#define CHANGEDLGITEM_TYPE_GRP      0x00000002 // Change the position within a group of controls
#define CHANGEDLGITEM_TYPE_ALL      0x00000004 // Change the position within the group of all 
                                               // visible and enabled controls in dialog
#define CHANGEDLGITEM_TYPE_SCROLL_ONLY  0x00000008 // Scroll only. Do not change position

#define CHANGEDLGITEM_TYPE_MASK     (CHANGEDLGITEM_TYPE_TAB | \
                                    CHANGEDLGITEM_TYPE_GRP | \
                                    CHANGEDLGITEM_TYPE_ALL | \
                                    CHANGEDLGITEM_TYPE_SCROLL_ONLY)

#define CHANGEDLGITEM_DIR_PREV      0x80000000 // Move to the previous item


// Thranslate a key press code into a dialog item change code
BOOL GetChangeDlgItemCode(
    LPMSG lpmsg,            // In: message
    UINT *pnChangeCode,     // Out: dialog item change code
    BOOL bRTLDlg            // In: True if the Dialog is RTL
);

// Translate a key press code into the corresponding command code, if any
BOOL VK2Command(
    HWND hwnd,          // IN: Window corresponding to message
    DWORD dwKey,        // IN: key code
    WORD wDefID,        // IN: default dialog ID
    WORD code,          // IN: code returned from WM_GETDLGCODE
    HWND *phwnd2,       // IN/OUT: Temp window, NULL by default
    DWORD *piCmd        // OUT: Command code returned
);


// Initialize dialog scrolling
BOOL InitScrolling(
    HWND hwndDlg,           // In: Dialog
    BOOL fResetScrolling,   // In: Should scrolling be reset?
    BOOL fResetScrollInfo   // In: Should scroll info be reset?
);

// Implements dialog scrolling
void ProcessScrolling(
    HWND hwndOld,       // In: Old control with focus
    HWND hwndNew,       // In: New control with focus
    HWND hwndDlg,       // In: Dialog
    BOOL fDown          // In: TRUE if scrolling down. FALSE if scrolling up.
);

// Uninitialize scrolling
BOOL UnInitDialog(
    HWND hwndDlg        // In: Dialog
);

// Process DM_RESETSCROLL to reset scrolling
LRESULT OnResetScroll(
    HWND hwnd,          // In: Dialog
    WPARAM wParam,      // In: wParam of DM_RESETSCROLL
    LPARAM lParam       // In: lParam of DM_RESETSCROLL
);

// Message handling preprocessing
BOOL DefDlgProcWPreproc(
    PDLG pdlg,         // In: dialog
    PDLG pdlgParent,   // In: parent dialog
    HWND hwnd,          // In: dialog window
    UINT nMsg,          // In: message to handle
    WPARAM wParam,      // In: wparam for message
    LPARAM lParam,      // In: lparam for message
    LRESULT *plRet      // Out: Value returned from handling
);

#pragma warning(push)
#pragma warning(disable:4512) // assignment operator could not be generated

// Touch - support pan messages
// Pan state - this is used to store the overstretch state that can occur when 
//             a user pan's beyond the end of the scroll range.
class PanState : public AnimationStateBase
{
public:
    PanState()
        : AnimationStateBase(AnimationStateBase::Dialog)
        , nXPanPosition(0)
        , nYPanPosition(0)
        , fRunningAnimation(0)
        , hPhysicsEngine(0)
        , uTimerId(0)
        , eSupportedPanLockDir(PanLocking::PANLOCKDIR_NONE)
        , fIsXGFSceneDialog(FALSE)
    {
        ptsPrevPanLocation.x = 0;
        ptsPrevPanLocation.y = 0;
    }

    int nXPanPosition; 
    int nYPanPosition;
    POINTS ptsPrevPanLocation;
    BOOL fRunningAnimation;
    HANDLE hPhysicsEngine;
    UINT uTimerId;
    PanLocking::PANLOCKDIR eSupportedPanLockDir;
    PanLocking panLock;

    BOOL fIsXGFSceneDialog;
};

#pragma warning(pop)

typedef PanState* LPPANSTATE;

LPPANSTATE GetDialogPanState(HWND hDlg);
HRESULT SetDialogPanState(HWND hDlg, LPPANSTATE);
HRESULT RemoveDialogPanState(HWND hDlg);

HRESULT ProcessPanGesture( 
    HWND    hwndDlg,                    // In: Dialog window handle
    __in GESTUREINFO const *pgi         // In: Gesture information
);

HRESULT ProcessEndPan( 
    HWND    hwndDlg,                    // In: Dialog window handle
    int     nTransitionSpeed,           // In: start speed of animation (relevant for flick end gesture)
    DWORD   nTransitionAngle            // In: Angle for start speed (relevant for flick end gesture)
);

HRESULT 
CheckCancelPanAnimation(
    HWND hwndDlg                        // In: Dialog window handle
);

HRESULT SnapBackToFrame(
    HWND hwndDlg,                       // In: Dialog window handle
    __inout LPPANSTATE pPanState        // In: pointer to the pan state for the window
);

VOID CALLBACK AnimationTimerProc(          
    HWND hwnd,                          // In: Dialog window handle
    UINT uMsg,                          // In: Set to WM_TIMER from the original message
    UINT_PTR idEvent,                   // In: id of the timer that caused this function to be called
    DWORD dwTime                        // In: tick count
);

HRESULT PanAxis (
    HWND hwndDlg,                       // In: Dialog window handle
    BOOL fHorizontal,                   // In: TRUE - x axis will be moved, FALSE - y axis
    int nOffset                         // In: How far should the form be moved - signed!
);


BOOL ChangeDlgTabItem(HWND hwndDlg, LPMSG lpMsg, WORD nDlgCode, BOOL fPrev);
BOOL ChangeDlgGroupItem(HWND hwndDlg, LPMSG lpMsg, WORD nDlgCode, BOOL fPrev);
BOOL HandleOnGroupChangeFailed(HWND hwndDlg, LPMSG lpMsg, WORD nDlgCode, BOOL fPrev);
BOOL ShouldSendBtnClickOnGroupChange(HWND hwndOldFocus, HWND hwndNewFocus);

BOOL WmKeydown(
            HWND    hwnd,
            HWND    hwnd2,
            HWND    hwndDlg,
            LPMSG   lpmsg,
            WORD    code
            );

// REVIEW: move the following definition to winuser.h

// Dialog message extensions
//
// Reset scroll info. To be sent by a dialog if it does a relayout of child
// controls after WM_INITDIALOG. Supported only if the dialog returns DLGC_SCROLL
// on WM_GETDLGCODE.
// wParam: TRUE to scroll back to top if user scrolled dialog. FALSE to leave
// dialog in position that user scrolled to.
// lParam: TRUE to recompute scroll information. FALSE to use previously computed
// scroll information.
#define DM_RESETSCROLL          DM_RESERVED0x2

// Dialog scroll info
struct DIALOGSCROLLINFO
{
    int nMin;        // Min client rect coordinate
    int nMax;        // Max client rect coordinate
    int nTop;        // Top of viewport (in client coordinates)
    int nPage;       // Page height
    int nLineHeight; // Line height
};

// Dialog scroll states
enum DLGSCROLLSTATE
{
    DSS_NONE = 0,   // No scroll bar (initial state)
    DSS_AUTOMATIC,  // Scroll bar is automatically added as needed
    DSS_ACTIVE      // Scroll bar is active (scroll bar was added)
};

// Scrollbar property string
static const TCHAR gc_szScrollable[] = TEXT("MSShellDlgScroll");

static const WCHAR* const g_szMSDlgScrollBar    = L"MSDlgScrollBar";

UINT GetDialogScrollState(HWND hDlg);
void SetDialogScrollState(HWND hDlg, UINT uState);

BOOL
NotifyDialogManager(
    HWND    hwndDlg,
    DWORD   oldStyle,
    DWORD   newStyle,
    DWORD   ScrollbarStateFlag
    );

// string used to store dialog base units in window property
static const WCHAR* const g_szBaseUnit    = L"bu";

class DialogManager_t
{
public:

    static
    BOOL
    Initialize(
        HINSTANCE    hInstance
        );

    static
    int
    WINAPI
    DialogBoxIndirectParamW_E(
        HINSTANCE       hInstance,
        __in_bcount(cbDialogTemplate) LPCDLGTEMPLATE  pDialogTemplate,
        size_t          cbDialogTemplate,
        HWND            hwndOwner,
        DLGPROC         pDialogProc,
        LPARAM          lParam,
        __out_bcount(cbCreateStruct) CREATESTRUCTW*  pCreateStruct,
        size_t          cbCreateStruct,
        __out_bcount(cbMSG) MSG*            pMSG,
        size_t              cbMSG
        );

    static
    int
    WINAPI
    DialogBoxIndirectParamW_IW(
        HINSTANCE       hInstance,
        __in_bcount(cbDialogTemplate) LPCDLGTEMPLATE  pDialogTemplate,
        size_t          cbDialogTemplate,
        HWND            hwndOwner,
        DLGPROC         pDialogProc,
        LPARAM          lParam,
        __out_bcount(cbCreateStruct) CREATESTRUCTW*  pCreateStruct,
        size_t          cbCreateStruct,
        __out_bcount(cbMSG) MSG*            pMSG,
        size_t              cbMSG
        );


    static
    int
    DialogBoxIndirectParamW_I(
        HINSTANCE        hInstance,
        __in LPCDLGTEMPLATE   pDialogTemplate,
        HWND             hwndOwner,
        CePtr_t<DLGPROC> DialogProcPtr,
        LPARAM           lParam,
        __out CREATESTRUCTW*   pCreateStruct,
        __out MSG*             pMSG
        );

    static
    HWND
    WINAPI
    CreateDialogIndirectParamW_E(
        HINSTANCE       hInstance,
        __in_bcount(cbDialogTemplate) LPCDLGTEMPLATEW  pDialogTemplate,
        size_t          cbDialogTemplate,
        HWND            hwndOwner,
        DLGPROC         pDialogProc,
        LPARAM          lParam,
        __out_bcount(cbCreateStruct) CREATESTRUCTW*   pCreateStruct,
        size_t          cbCreateStruct
        );

    static
    HWND
    WINAPI
    CreateDialogIndirectParamW_IW(
        HINSTANCE       hInstance,
        __in_bcount(cbDialogTemplate) LPCDLGTEMPLATEW  pDialogTemplate,
        size_t          cbDialogTemplate,
        HWND            hwndOwner,
        DLGPROC         pDialogProc,
        LPARAM          lParam,
        __out_bcount(cbCreateStruct) CREATESTRUCTW*   pCreateStruct,
        size_t          cbCreateStruct
        );
    
    static
    HWND
    CreateDialogIndirectParamW_I(
        HINSTANCE        hInstance,
        __in LPCDLGTEMPLATEW  pDialogTemplate,
        HWND             hwndOwner,
        CePtr_t<DLGPROC> DialogProcPtr,
        LPARAM           lParam,
        __out CREATESTRUCTW*   pCreateStruct
        );

    static
    BOOL
    SetDlgItemInt_I(
        HWND    hwnd,
        int     lid,
        UINT    lValue,
        BOOL    fSigned
        );

    static
    UINT
    WINAPI
    GetDlgItemInt_I(
        HWND    hwnd,
        int     lid,
        __out_opt BOOL*   lpfValOK,
        BOOL    fSigned
        );

    static
    BOOL
    WINAPI
    SetDlgItemTextW_W(
        HWND    hDlg,
        int     nIDDlgItem,
        __in_opt LPCWSTR lpString
        );

    static
    BOOL
    SetDlgItemTextW_I(
        HWND    hDlg,
        int     nIDDlgItem,
        __in_opt LPCWSTR lpString
        );

    static
    UINT
    GetDlgItemTextW_I(
        HWND    hDlg,
        int     nIDDlgItem,
        __out_ecount(nMaxCount) LPWSTR  lpString,
        int     nMaxCount
        );

    static
    UINT
    WINAPI
    GetDlgItemTextW_E(
        HWND    hDlg,
        int     nIDDlgItem,
        __out_bcount(cbString) LPWSTR  lpString,
        size_t  cbString,
        int     nMaxCount
        );

    static
    LONG
    GetDialogBaseUnits_I(
        void
        );

    static
    BOOL
    MapDialogRect_I(
        HWND    hwnd,
        __inout LPRECT  lprc
        );

    static
    BOOL
    MapDialogRect_E(
        HWND    hwnd,
        __inout_bcount(cbrc) LPRECT  lprc,
        size_t  cbrc
        );

    static
    BOOL
    WINAPI
    EndDialog_E(
        HWND    hwnd,
        int     lresult
        );

    static
    BOOL
    EndDialog_I(
        HWND    hwnd,
        int     lresult
        );

    static
    BOOL
    IsDialogMessageW_I(
        HWND    hwndDlg,
        __in LPMSG   lpmsg
        );

    static
    BOOL
    IsDialogMessageW_E(
        HWND    hwndDlg,
        __in_bcount(cbmsg) LPMSG   lpmsg,
        size_t  cbmsg
        );

    static
    int
    GetDlgCtrlID_I(
        HWND    hwnd
        );


    static
    HWND
    GetDlgItem_I(
        HWND    hDlg,
        int     iCtrlID
        );

    static
    BOOL
    WINAPI
    CheckRadioButton_E(
        HWND    hwnd,
        int     idFirst,
        int     idLast,
        int     idCheck
        );

    static
    BOOL
    CheckRadioButton_I(
        HWND    hwnd,
        int     idFirst,
        int     idLast,
        int     idCheck
        );

    static
    LONG
    WINAPI
    SendDlgItemMessageW_I(
        HWND    hDlg,
        int     nIDDlgItem,
        UINT    Msg,
        WPARAM  wParam,
        LPARAM  lParam
        );

    static
    LONG
    WINAPI
    SendDlgItemMessageW_E(
        HWND    hDlg,
        int     nIDDlgItem,
        UINT    Msg,
        WPARAM  wParam,
        LPARAM  lParam
        );

    static
    LRESULT
    WINAPI
    DefDlgProcW_E(
        HWND    hwnd,
        UINT    message,
        WPARAM  wParam,
        LPARAM  lParam
        );

    static
    LRESULT
    DefDlgProcW_I(
        HWND    hwnd,
        UINT    message,
        WPARAM  wParam,
        LPARAM  lParam
        );

    static
    HWND
    GetNextDlgItem_I(
        HWND    hwndDlg,
        HWND    hwnd,
        BOOL    fPrev
        );

    static
    HWND
    GetNextDlgTabItem_I(
        HWND    hwndDlg,
        HWND    hwnd,
        BOOL    fPrev
        );

    static
    HWND
    GetNextDlgGroupItem_I(
        HWND    hwndDlg,
        HWND    hwnd,
        BOOL    fPrev
        );

    static 
    BOOL 
    SetDialogAutoScrollBar_I(
        HWND    hDlg
        );

    static
    BOOL 
    SetWindowPosOnRotate_I(
        HWND    hWnd, 
        HWND    hWndInsertAfter,
        int X, 
        int Y, 
        int cx, 
        int cy,
        UINT uFlags, 
        BOOL fNoScroll
        );    
};

#endif // _DLGMGR_

#endif // __DLGMGR_H__
