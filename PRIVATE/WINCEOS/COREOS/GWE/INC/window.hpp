//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*


*/
#ifndef __WINDOW_HPP__
#define __WINDOW_HPP__


class CWindow;

struct ScrollBarInfoInternal;
class GestureStateManager;

#include <windows.h>
#include <gwe_s.h>
#include <cmsgque.h>
#include <class.hpp>
#include <gdipriv.h>
#include <gdi.hpp>
#include <CePtr.hpp>

extern "C" LPTSTR GetCaption(HWND hwnd);


typedef void GdiWindow;
class MsgQueue;
struct  ScrollBarInfoInternal;
class   CWindowClass;
class   SendMsgEntry_t;

// BUGBUG: We only support disabling a scroll bar in its entirity.
#define WF_NC_SBH_PRESENT       0x00000020  //  Horizontal scroll bar present
#define WF_NC_SBV_PRESENT       0x00000010  //  Vertical scroll bar present
#define WF_NC_SBV_SCROLLING     0x00000080  //  Vertical non-client scrollbar
#define WF_SBV_DISABLED         0x00000200  //  Draw vertical scroll bar disabled
#define WF_SBH_DISABLED         0x00000400  //  Draw horz scroll bar disabled
#define WF_NC_DISABLE_CLOSE     0x00000100  //  Draw caption close box disabled
#define WF_NC_SBV_SYSCOPYDATA   0x00000800  //  This flag is set when the window has a non client vertical scrollbar that was
                                            //  created by a sending a WM_SYSCOPYDATA w/ WMSCD_DIALOGSCROLLBAR message
                                            //  (only dialog windows can have this type of scrollbar)

#define WF_WIN_IS_SB_CTL         0x00000040      // Scroll bar control window

#define GWE_SENT_WM_COMMAND 0xabcd  // Dialog sent WM_COMMAND to message box

// constants used by GweCoreDllInfoFn
#define GWEINFO_VIRTUAL_MEM_BASE                     1
#define GWEINFO_DEFAULT_SYSTEM_METRICS         2
#define GWEINFO_PRIMARY_DISPLAY_SCREEN_RES  3
#define GWEINFO_DEFAULT_SYSTEM_COLORS           4


//    Param for ModifyZOrder
enum MzoCommand
    {
    mzoModifyOrder,
    mzoAdoptOnly,
    mzoDisownOnly,
    };

class CWindow
{
private:

    CWindow     *m_pcwndNext;
    CWindow     *m_pcwndParent;
    CWindow     *m_pcwndChild;

    static const UINT32 s_sigValid;
    UINT32      m_sig;          /* signature */

    CWindow     *m_pcwndOwner;
    CWindow     *m_pcwndOwned;
    CWindow     *m_pcwndNextOwned;

    static const CWindow  *s_pcwndDesktopOwner;

    CWindow     *m_pcwndRestore;

    RECT        m_rc;           /* rect for entire window, screen coords */
    RECT        m_rcClient;     /* client area rect, screen coords */

    /* A GDIWINDOW tracks the location and clip region of a window and is used
       to create a DC. These three correspond to the: */
    GdiWindow*  m_pgdiwnd;              /* whole window */
    GdiWindow*  m_pgdiwndClient;        /* client area */
    GdiWindow*  m_pgdiwndClientUpdate;  /* client update region */

    RECT        m_rcRestore;            /* used when restoring the size of a window */
    GENTBL*        m_ptblProperties;        // used to store window properties (GET/SetProp)

    CWindow(const CWindow&);            //    Default copy constructor.  Not implemented.
    CWindow& operator=(const CWindow&);    //    Default assignment operator.  Not implemented.
    void*    operator new(size_t cb);    //    Default operator new.  Not implemented


    void*
    operator new(
        size_t    cb,
        size_t    cbExtra
        );

    void
    operator delete(
        void*
        );


    CWindow(
        size_t cbExtra
        );

    ~CWindow(
        void
        );

    static
    void
    AddToDestroyedList(
        CWindow* pcwnd
        );

    inline
    static
    bool
    CanDefaultIMEControlWindowBeDestroyed(
        CWindow* pcwnd
        )
    {
        // Only sibling threads of the thread which created an IME control window can destroy it.     
        return (pcwnd->m.fThreadDead ||
                (GetOwnerProcess() == (HANDLE)GetProcessIdOfThread(pcwnd->m_hthdCreator)));
    }

    inline
    bool
    IsDefaultIMEControlWindow(
        void
        )
    {
        return ((m_pwc->m_style & (CS_SYSTEMCLASS | CS_IME)) == (CS_SYSTEMCLASS | CS_IME));
    }

    int
    MarkOwned(
        int n
        );

    LRESULT
    Dwp_SendToDefaultImeWnd(
        HWND    hwnd,
        UINT    msg,
        WPARAM    wParam,
        LPARAM    lParam
        );

    LRESULT
    Dwp_SendToTopLevel(
        HWND    hwnd,
        UINT    msg,
        WPARAM    wParam,
        LPARAM    lParam
        );

    LRESULT
    Dwp_OnWmImeNotify(
        HWND    hwnd,
        UINT    msg,
        WPARAM    wParam,
        LPARAM    lParam
        );

    LRESULT
    Dwp_OnWmImeComposition(
        HWND    hwnd,
        UINT    msg,
        WPARAM    wParam,
        LPARAM    lParam
        );



public:

    static
    CWindow*
    NewUniquePcwnd(
        int    cbExtra
        );


    static
    BOOL
    WINAPI
    SetWindowPos_I(
        HWND hwndThis,
        HWND hwndInsertAfter,
        int             x,
        int             y,
        int             cx,
        int             cy,
        UINT            fuFlags
        );

    static
    BOOL
    WINAPI
    GetWindowRect_I(
        HWND            hwndThis,
        PRECT           prc
        );

    static
    BOOL
    WINAPI
    GetClientRect_I(
        HWND            hwndThis,
        LPRECT          prc
        );

    static
    BOOL
    WINAPI
    InvalidateRect_I(
        HWND            hwndThis,
        LPCRECT         prc,
        BOOL            fErase
        );


    static
    BOOL
    WINAPI
    InvalidateRgn_I(
        HWND            hwndThis,
        HRGN            hRgn,
        BOOL            fErase
        );

    static
    BOOL
    WINAPI
    ValidateRgn_I(
        HWND            hwndThis,
        HRGN            hrgn
        );

    static
    HWND
    WINAPI
    GetWindow_I(
        HWND            hwndThis,
        UINT32          relation
        );

    static
    HWND
    WINAPI
    ChildWindowFromPoint_I(
        HWND            hwndThis,
        POINT           pt
        );

    static
    BOOL
    WINAPI
    ClientToScreen_I(
        HWND            hwndThis,
        POINT *         pPoint
        );


    static
    BOOL
    WINAPI
    ScreenToClient_I(
        HWND            hwndThis,
        POINT *         pPoint
        );

    static
    BOOL
    WINAPI
    SetWindowTextW_I(
                HWND    hwndThis,
        const    WCHAR*    psz
        );

    static
    int
    WINAPI
    GetWindowTextW_E(
        HWND   hwndThis,
        __out_bcount(StringBufferSize) WCHAR * pString,
        size_t StringBufferSize,
        int    cchMax
        );

    static
    int
    WINAPI
    GetWindowTextW_I(
        HWND    hwndThis,
        __out_ecount(cchMax) WCHAR* pString,
        int     cchMax
        );

    static
    LONG
    WINAPI
    SetWindowLongW_I(
        HWND            hwndThis,
        int             nIndex,
        LONG            lNewLong
        );

    static
    LONG
    WINAPI
    GetWindowLongW_I(
        HWND            hwndThis,
        int             nIndex
        );

    static
    HDC
    WINAPI
    BeginPaint_I(
        HWND            hwndThis,
        PAINTSTRUCT*    pps
        );

    static
    BOOL
    WINAPI
    EndPaint_I(
        HWND            hwndThis,
        PAINTSTRUCT*    pps
        );

    static
    HDC
    WINAPI
    GetDC_I(
        HWND    hwndThis
        );

    static
    int
    WINAPI
    ReleaseDC_I(
        HWND            hwndThis,
        HDC             hdc
        );

    static
    LRESULT
    WINAPI
    DefWindowProcW_I(
        HWND            hwndThis,
        UINT            msg,
        WPARAM          wParam,
        LPARAM          lParam
        );

    static
    DWORD
    WINAPI
    GetClassLongW_I(
        HWND            hwndThis,
        INT             nIndex
        );

    static
    DWORD
    WINAPI
    SetClassLongW_I(
        HWND            hwndThis,
        INT             nIndex,
        LONG            lNewValue
        );

    static
    BOOL
    WINAPI
    DestroyWindow_I(
        HWND            hwndThis
        );

    static
    BOOL
    WINAPI
    ShowWindow_I(
        HWND            hwndThis,
        INT             nCmdShow
        );

    static
    BOOL
    WINAPI
    UpdateWindow_I(
        HWND    hwndThis
        );


    BOOL
    RedrawWindowInvalidateHelper(
      HRGN hrgn,
      UINT flags
    );

    BOOL
    RedrawWindowValidateHelper(
      HRGN hrgn,
      UINT flags
    );
    
    BOOL
    RedrawWindowRecursivePaint(
    BOOL bChildren
    );
    
    void
    ClientToScreen(
    RECT *lprc
    );

    static
    BOOL
    WINAPI
    RedrawWindow_I(
      HWND hWnd,
      CONST RECT *lprcUpdate,
      HRGN hrgnUpdate,
      UINT flags
    );
    
    static
    HWND
    WINAPI
    SetParent_I(
        HWND            hwndThis,
        HWND            hwndNewParent
        );
    
    static
    HWND
    WINAPI
    GetParent_I(
        HWND    hwndThis
        );

    static
    HDC
    WINAPI
    GetWindowDC_I(
        HWND    hwndThis
        );

    static
    BOOL
    WINAPI
    IsWindow_I(
        HWND  hwndThis
        );

    static
    BOOL
    WINAPI
    IsWindow_I(
        HWND hwnd,
        CWindow **ppwin);

    static
    BOOL
    WINAPI
    MoveWindow_I(
        HWND            hwndThis,
        int             X,
        int             Y,
        int             nWidth,
        int             nHeight,
        BOOL            bRepaint
        );

    static
    int
    WINAPI
    GetUpdateRgn_I(
        HWND            hwndThis,
        HRGN            hRgn,
        BOOL            bErase
        );

    static
    BOOL
    WINAPI
    GetUpdateRect_I(
        HWND            hwndThis,
        LPRECT          lpRect,
        BOOL            bErase
        );

    static
    BOOL
    WINAPI
    BringWindowToTop_I(
        HWND  hwndThis
        );

    static
    int
    WINAPI
    GetWindowTextLengthW_I(
        HWND hwndThis
        );

    static
    BOOL
    WINAPI
    IsChild_I(
        HWND            hwndThis,
        HWND            hWnd
        );

    static
    BOOL
    WINAPI
    IsWindowVisible_I(
        HWND            hwndThis
        );

    static
    BOOL
    WINAPI
    ValidateRect_I(
        HWND            hwndThis,
        CONST RECT      *lpRect
        );
    
    static
    int
    WINAPI
    GetClassNameW_I(
        HWND    hwndThis,
        WCHAR*    szClassName,
        int     cchClassName
        );

    static
    BOOL
    WINAPI
    EnableWindow_I(
        HWND hwndThis,
        BOOL bEnable
        );

    static
    BOOL
    WINAPI
    IsWindowEnabled_I(
        HWND hwndThis
        );

    static
    int
    WINAPI
    ScrollWindowEx_I(
        HWND            hwndThis,
        int             dx,
        int             dy,
        CONST RECT *    prcScroll,
        CONST RECT *    prcClip ,
        HRGN            hrgnUpdate,
        LPRECT          prcUpdate,
        UINT            flags
        );

    static
    BOOL
    WINAPI
    RectangleAnimation_I(
        HWND                hwndThis,
        LPCRECT             prc,
        BOOL                    fMaximize
        );

    static
    DWORD
    WINAPI
    GetWindowThreadProcessId_I(
        HWND                hwndThis,
        LPDWORD             lpdwProcessId
        );

    static
    void
    WINAPI
    SetAssociatedMenu_I(
        HWND  hwndThis,
        HMENU hmenu
        );

    static
    HMENU
    WINAPI
    GetAssociatedMenu_I(
        HWND    hwndThis
        );

    static
    BOOL
    WINAPI
    ImmAssociateContextWithWindowGwe_I(
        HWND    hwndThis,
        HIMC    himc,
        DWORD    dwFlags,
        HIMC    *phimcPrev,
        HWND    *phwndFocus,
        HIMC    *phimcFocusPrev,
        HIMC    *phimcFocusNew
        );

    static
    HDC
    WINAPI
    GetDCEx_I(
        HWND    hwndThis,
        HRGN    hrgnClip,
        DWORD    flags
        );

    static
    int
    WINAPI
    SetWindowRgn_I (
        HWND hwndThis,
        HRGN hrgn,
        BOOL bRedraw
        );

    static
    int
    WINAPI
    GetWindowRgn_I (
        HWND hwndThis,
        HRGN hrgn
        );
    
    static
    ScrollBarInfoInternal *
    GetSBInfoInternal(
        HWND    hwnd,
        BOOL    fVert
        );

    static
    BOOL
    WINAPI    
    SetPropW_I(
                HWND   hWnd,
                ATOM   PropertyIdentifier,
        const   WCHAR* pPropertyName,
                HANDLE hData
        );
        
    static
    HANDLE
    WINAPI    
    GetPropW_I(
                HWND   hWnd,
                ATOM   PropertyIdentifier,
        const   WCHAR* pPropertyName
        );

    static
    HANDLE
    WINAPI    
    RemovePropW_I(
                HWND   hWnd,
                ATOM   PropertyIdentifier,
        const   WCHAR* pPropertyName
        );
    
    static
    int
    EnumPropsEx_I(
        HWND            hWnd,
        PROPENUMPROCEX    lpEnumFunc,
        LPARAM            lParam,
        HPROCESS        hProcCallingContext
        );

    static
    int
    WINAPI    
    EnumPropsEx_E(
        HWND            hWnd,
        PROPENUMPROCEX    lpEnumFunc,
        LPARAM            lParam
        );

     HWND
    WINAPI
    GetTopLevelInfo(
        MsgQueue**    pmsgqWindow,
        MsgQueue**    pmsgqTopLevel
        );


    static
    CWindow*
    coredllPwndFromHwnd(
        HWND    hwnd
        );


    static
    BOOL
    WINAPI
    coredllIsWindow(
        HWND    hwnd    
        );

     BOOL
    WINAPI
    IsZoomed(
        void
        );

    bool
    WINAPI
    IsRestorable(
        void
        );

    BOOL
    WINAPI
    SetWindowPos1(
        HWND        hwndInsertAfter,
        CWindow*    pcwndControl,
        int            x,
        int            y,
        int            cx,
        int            cy,
        UINT        fuFlags
        );

    BOOL
    WINAPI
    ChildWindowExists(
        CWindow*    pcwnd
        );

    HWND
    WINAPI
    GetEnabledParent(
        void
        );

    static
    BOOL
    WINAPI
    CheckFlagsChain_I(
        HWND    hwndThis,
        UINT32    grfMask,
        UINT32    grfResult,
        HWND    root
        );


    bool
    CheckWsExChain(
        UINT32    grfMask,
        UINT32    grfResult,
        CWindow    *pcwndRoot
        );


        VOID WINAPI LeafLevelHitDetect(
                INT32       ScreenX,
                INT32       ScreenY,
                MsgQueue    *pmsgqRestrict,
                BOOL        fNCCapture,
                HWND        *phwndHit,
                MsgQueue    **ppmsgqHit,
                HWND        *phwndHitTopLevel,
                INT32       *pxWindow,
                INT32       *pyWindow,
                LRESULT     *pHitCode,
                BOOL        *pfDoubleClicks,
                UINT32        *grfStyleChild,
                UINT32        *grfExStyleChild,
                UINT32        *grfStyleTopLevel,
                UINT32        *grfExStyleChain
                );

        VOID WINAPI HitWindowInfo(
                BOOL            *pfDoubleClicks,
                HWND            *phwndTopLevel
                );


    CWindow* WINAPI GetTopLevelWindow(
        void
        );




//    FAREAST // FE_IME
    CWindow* WINAPI GetTopLevelWindowWithInTask(
        void
        );
//    #endif // FAREAST


    friend
        HIMC
        WINAPI
        ImmGetContextFromWindowGwe(
            HWND    hwnd
            );


    static LPBYTE WINAPI GetExtraBytePtr_I(HWND hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return ((LPBYTE)&(This.m_rgdwExtraBytes[0]));
    }

    static BOOL WINAPI GetfDialog_I(HWND hwndThis)
    {
       CWindow&    This = *((CWindow*)hwndThis);
       return This.m.fIsDialog;
    }

    static
    void
    ScreenToWindow(
        HWND    hwnd,
        LPPOINT    ppt
        );
    
    static
    void
    WindowToScreen(
        HWND    hwnd,
        LPPOINT    ppt
        );
    
    bool
    IsMessageBox(
        void
        );

    static bool IsListBox(HWND hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return This.m.m_IsListBox;
    }

    static void ListBoxSet(
        HWND hwndThis
        )
    {
        CWindow&    This = *((CWindow*)hwndThis);
        This.m.m_IsListBox = true;
        return;
    }

    void
    IsMessageBox(
        bool    torf
        );

    static VOID WINAPI SetfDialog_I(HWND hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);

        This.m.fIsDialog = TRUE;
    }

    void WINAPI UsingHotKeys(void)
    {
        m.fUsingHotKeys = TRUE;
    }

    BOOL WINAPI SetRedraw(BOOL);


    static HWND WINAPI GetRealParent_I(HWND hwndThis);

    CWindow* WINAPI pcwndHighestOwnerDesktop(CWindow *pcwndNoOwner, bool bNoPseudoOwner);

    CWindow* WINAPI pcwndHighestOwnerNonDesktop(CWindow *pcwndNoOwner);

    void WINAPI RestoreWindowSet(CWindow  *pcwnd);

    CWindow* WINAPI RestoreWindowGet();

    static UINT32 WINAPI GetStyle_I(HWND  hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return This.m_grfStyle;
    }

    VOID WINAPI SetStyle(UINT32 grfStyle)
    {
        m_grfStyle = grfStyle;
    }

    UINT32 WINAPI GetExStyle(void)
    {
        return m_grfExStyle;
    }

    BOOL WINAPI IsVisibleEnabled_I(void)
    {
        return (m_grfStyle&(WS_VISIBLE|WS_DISABLED)) == WS_VISIBLE;
    }

    static CWindow* WINAPI GetChild_I(HWND hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return This.m_pcwndChild;
    }

    static CWindow* WINAPI GetNext_I(HWND hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return This.m_pcwndNext;
    }

    static LONG WINAPI GetControlID_I(HWND  hwndThis)
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return This.m_lID;
    }

    static BOOL DeleteGweRegion(HRGN hrgn);

        #if CHECK_UNUSED
        BOOL WINAPI Unused(
                HGDIOBJ h
                );
        #endif



    UINT32      m_dwState;  /* Window state flags. Nonclient area and controls */

    ScrollBarInfoInternal    *m_psbii;

    GestureStateManager*     m_pGestureStateManager;

//private:


    BOOL
    WINAPI
    Initialize(
                CWindow         *pcwndParent,
                UINT32          cy,
                UINT32          cx,
                INT32           y,
                INT32           x,
                UINT32          grfStyle,
        const    WCHAR*    szName,
                UINT32          grfExStyle,
                MsgQueue*        pmsgq,
                CWindowClass*    pwc,
                LONG            lID
        );

    void WINAPI ModifyZOrder(
        CWindow     *pcwndPos,
        CWindow     *pcwndControl,
        MzoCommand    mzoCmd,
        int            *piDelta
        );

    UINT32
    WINAPI
    GetText(
        __out_ecount(cchTextBuffer) WCHAR* pszTextBuffer,
        UINT32 cchTextBuffer
        );

    BOOL
    WINAPI
    SetText(
        const    WCHAR*    pszWindowText
        );



    VOID
    WINAPI
    MarkOwnedGroup(
        void
        );


    VOID
    WINAPI
    Slide(
        INT32    dx,
        INT32    dy
        );

    CWindow*
    WINAPI
    WindowFromScreenPoint(
        INT32    x,
        INT32    y,
        BOOL    fNoHiddenOrDisabled
        );

    VOID
    WINAPI
    FromScreenRect(
        RECT*    prc
        );

    HRGN WINAPI GetClientUpdateRgn(
        VOID
        );

    VOID WINAPI ReleaseClientUpdateRgn(
        VOID
        );

    HDC WINAPI GetClientUpdateDC(
        VOID
        );

    VOID WINAPI ReleaseClientUpdateDC(
        HDC hdc
        );

    void
    WINAPI
    ThreadOrProcessCleanup(
        HPROCESS    hprc,
        HTHREAD        hthd
        );

    void
    WINAPI
    MarkDeadWindows(
        HPROCESS    hprc,
        HTHREAD        hthd
        );

    VOID WINAPI RedrawCaption(
        VOID
        );


    VOID WINAPI SetVisibleRegion(
        int fnCombineMode,
        HRGN hrgn
        );

    VOID WINAPI SetUpdateRegion(
        int fnCombineMode,
        HRGN hrgn
        );

    VOID WINAPI GiveVisRgnToBehind(
        LPRECT prc
        );

    VOID WINAPI DistributeVisRgn(
        HRGN hrgnGift,
        BOOL fUpdateFromScratch,
        BOOL fIncludeThis
        );

    BOOL WINAPI GiveUpVisRgn(
        HRGN hrgnTotalStolen,
        LPRECT prc,
        BOOL fRecursive,
        BOOL fTotallyObscured
        );

    BOOL WINAPI GiveUpVisRgn(
        HRGN hrgnTotalStolen,
        HRGN hrgnNeed,
        BOOL fRecursive,
        BOOL fTotallyObscured
        );

    HRGN WINAPI StealVisRgnFromBehind(
        VOID
        );

    VOID WINAPI CheckSaveBits(
        HRGN hrgnCheck
        );

    BOOL WINAPI CheckForOpenDC(
        LPRECT prc
        );

    VOID WINAPI ResetWindowDimension(
        LPRECT prc
        );
        
    VOID WINAPI ClearVisibleAndUpdateRegions(
        VOID
        );

    VOID WINAPI RecursiveClearVisibleAndUpdateRegions(
        VOID
        );

    VOID WINAPI CalcVisRgn(
        HRGN hrgnStolen,
        BOOL fUpdateFromScratch
        );

    void WINAPI AddChildrensVisibleRegion(
        HRGN    hrgn,
        CWindow    *pcwndStop
        );


    void WINAPI TopOwnedWindows(
        void
        );

    void WINAPI SendSizeMoveMsgs(
        void
        );

    void
    WINAPI
    SendDestroyMessages1(
        void
        );

    BOOL WINAPI RecursiveInvalidateWorker(
        DWORD dwFlags
        );

    void WINAPI RecursiveInvalidate(
        DWORD dwFlags
        );

    CWindow * WINAPI FindNewForegroundActive(
                        MsgQueue    *pmsgq,
                        BOOL        bSkipOwners,
                        UINT32        *pfPickedBySystem
                        );

    void WINAPI ForegroundActiveMaybeLost(
                    BOOL            bSkipOwners                
                    );

    void WINAPI FocusMaybeLost(
                    void
                    );

    HPROCESS WINAPI GetWndProcProcess(void)
    {
        return m_WindowProcPtr.GetProcHandle();
    }

//    WNDPROC
//    WINAPI
//    GetWndProc(
//        void
//        )
//    {
//        return m_pfnWndProc;
//    }

    MsgQueue* WINAPI GetMessageQueue(void)
    {
        return m_pmsgq;
    }

    void WINAPI GetMsgQueuesForTree(
        class MsgQueue    **ppmsgqTreeList
        );


    HWND WINAPI GetHwnd(void)
    {
        return (HWND)this;
    }

    static CWindowClass* GetWindowClass(
        HWND hwnd
        )
    {
        return ((CWindow*)hwnd)->m_pwc;
    }

    static
    UINT32
    GetScrollbarStateFlags(
        HWND  hwndThis
        )
    {
        CWindow&    This = *((CWindow*)hwndThis);
        return This.m_dwState;
    }

    static
    void
    VerticalScrollBarStyle(
        HWND    hwndThis,
        bool    bAddScrollBar
        );

    #ifdef DEBUG
    VOID Dump(void);
    VOID DumpRecursive(void);
    #endif /* DEBUG */

    #define DUMP_WINDOW_SIZES 0
    #if DUMP_WINDOW_SIZES
    void DumpWindowSizes(int *pcUsed, int * pcUnused, int * pcCV, int *pcTotal);
    friend void DumpSizes(void);
    #endif

    friend class CWindowManager;
    friend class MsgQueueMgr;


    static
    bool
    HalveCoordMessageIfNecessary(
        CWindow*    pWindow,
        UINT        Message,
        WPARAM*        pwParam,
        LPARAM*        plParam
        );


    static
    void
    RestoreCoordMessage(
        UINT        Message,
        WPARAM*        pwParam,
        LPARAM*        plParam
        );

    static
    LRESULT
    WINAPI
    CallWindowProcW_E(
        WNDPROC  pWindowProc,
        HWND     hwnd,
        UINT     uMsg,
        WPARAM   wParam,
        LPARAM   lParam
        );

    static
    LRESULT
    CallWindowProcW_I(
        CePtr_t<WNDPROC>    WindowProcPtr,
        HWND                hwnd,
        UINT                uMsg,
        WPARAM              wParam,
        LPARAM              lParam,
        SendMsgEntry_t*     pSendMsgEntry
        );

    friend
    BOOL
    HitInVisRgn(
        INT32        ScreenX,
        INT32        ScreenY,
        CWindow*    pcwnd
        );

    friend
    MsgQueue*
    HwndMessageQueue(
        HWND    hwnd
        );


//    friend void GetIndices(CWindow *pcwnd, CWindow *pcwndThis, CWindow *pcwndInsertAfter,
//                            LONG * piThis, LONG * piInsertAfter);


    friend
    WCHAR*
    GetCaption(
        HWND    hwnd
        );


    friend
    HANDLE
    LookupProperty(
                HWND    hWnd,
                ATOM     Atom,
        const    WCHAR*    pString,
                bool    bDeleteProp
        );
    

    #ifdef MEMTRACKING
    friend void MemTrackerPrintWindow(DWORD dwFlags, DWORD dwType, HANDLE hItem,
                            DWORD dwProc, BOOL fDeleted, DWORD dwSize,
                            DWORD dw1, DWORD dw2);
    #endif

    const    WCHAR*    m_pszName;      /* Name.  Allowed to be NULL. */


    MsgQueue*    m_pmsgq;
    HIMC        m_himc;
    HPROCESS    m_hprcHimcOwner;

//    Secret static control style.  Windows has this.
#define SS_EDITCONTROL      0x00001000L
    UINT32      m_grfStyle;

    UINT32      m_grfExStyle;
    CWindowClass *m_pwc;
    LONG        m_lID;          // ID associated with the window
    LONG        m_lUserData;


    HPROCESS    m_hprcCreator;  /* Process that created this window. */
    HTHREAD     m_hthdCreator;  /* Thread that created this window. */

   /* The WNDPROC.  Not necessarily same as m_pwc->m_WindowProcPtr because of subclassing. */
    CePtr_t<WNDPROC>    m_WindowProcPtr;

    HRGN        m_hrgnWindowRgn;/* The window region */
    HRGN        m_hrgnVisible;  /* The currently visible region */
    HRGN        m_hrgnUpdate;   /* Update == Invalid /\ Visible */

    HRGN        m_hrgnClientVisible;/* The currently visible client region */
    HRGN        m_hrgnClientUpdate;   /* ClientUpdate == Update /\ Client */
                                /* m_hrgnClientUpdate is only valid as long as
                                   a DC using it exists. */


    union {
    HMENU       m_hmenu;        /* the menu associated with this app window */
    HPROCESS    m_hprcDestroyer;/* the process destroying this window */
    };
    union {
        struct {
            UINT32          fHasUpdateRegion            : 1; /* Is m_hrgnUpdate meaningful? */
            UINT32          fHasVisibleRegion           : 1; /* Is m_hrgnVisible meaningful? */
            UINT32          fEraseBkgnd                 : 1; /* Should WM_ERASEBKGND be sent? */
            UINT32          fThreadDead                 : 1; /* Did this wnd's thread go away out
                                                                from underneath it? */
            UINT32          fBeingDestroyed             : 1; /* Inside DestroyWindow()? */
            UINT32          fIsDialog                   : 1; /* Is this part of a dialog? */
            UINT32          fSentWmCreate               : 1; /* Did we send WM_CREATE yet? */
            UINT32          fSentWmDestroy              : 1; /* Did we send WM_DESTROY yet? */

            UINT32          fPendingSizeMove            : 1; /* We are waiting to send WM_SIZE and
                                                                WM_MOVE msgs when a ShowWindow
                                                                happens. */
            UINT32          fAltPressed                 : 1; /* Was ALT pressed by itself? */
            UINT32          fRedraw                     : 1;
            UINT32          fRedrawOffToOn              : 1;
            UINT32          fOwnedGroup                 : 2;
            UINT32          fUsingHotKeys               : 1;
            unsigned int    fWasWsSizeBox               : 1;
            unsigned int    TopLevelPositionPriority    : 4;
            UINT32          fRemovedTopmost             : 1; /* Removed WS_EX_TOPMOST flag? */
            UINT32          fPreCedarApp                : 1; /* was this window created by a precedar app */
            UINT32          fDontAddToDestroyedList     : 1; /* used to generate unique HWND */
            bool            m_IsMessageBox              : 1;
            bool            m_IsListBox                 : 1; /* Is this a listbox ? */
            bool            fPreTaliskerApp             : 1; /* was this window created by a pretalisker app */
        } m;
        UINT32 m_grfBitFields;      /* Note Note access all bitfields at once */
    };

    // The following state is used to protect from stack faults caused by a parent window sending a gesture command to a child
    // that doesn't handle the gesture: defwindowproc propagates unhandled gesture messages to the windows parent, so sending a
    // gesture from parent to child, where the child control doesnt handle the message, causes a tight(ish) loop and generally
    // ends in a stack fault.
    // This protection will only work for the first 64 gesture command values - user defined values above 64 are not protected
    // by this code.
    ULONGLONG   m_ullGuardGestureFlags;

#pragma warning( disable : 4200 )
    DWORD       m_rgdwExtraBytes[0];
#pragma warning( default : 4200 )

};

//#define CALL(s) do { if(!(fRet =(s))) goto errRtn; } while(0)

/* Validate the "this" pointer */
#define _CHECKTHIS(STM, GOTO) \
                        do { if(!CWindow::IsWindow_I(GetHwnd())) { \
                            SetLastError(ERROR_INVALID_WINDOW_HANDLE); \
                            ASSERT(Ret==FALSE); \
                            STM \
                            GOTO; }} while(0)

#define CHECKTHIS()             _CHECKTHIS(;, goto Exit)
#define CHECK_THIS_FINALLY()    _CHECKTHIS(;, LEAVE)

/* Parameters to CWindow::InvalidateRegion */
typedef UINT32 INVALIDATE;
#define INVALIDATE_SUBTRACT             1
#define INVALIDATE_NOERASEBKGND         2   // don't erase background


HRGN
CreateNullRegion(
    void
    );

HDWP WINAPI BeginDeferWindowPos_I(
    int nNumWindows
    );

HDWP WINAPI DeferWindowPos_I(    
   HDWP hWinPosInfo,
   HWND hWnd,
   HWND hWndInsertAfter,
   int x,
   int y,
   int cx,
   int cy,
   UINT uFlags
   );

BOOL WINAPI EndDeferWindowPos_I(
   HDWP hWinPosInfo
   );


const
UINT8*
DefSystemMetrics(
    void
    );



typedef struct tagPROP
{
    WCHAR*    lpString;
    HANDLE    hData;
} PROP;

#endif // __WINDOW_HPP__

