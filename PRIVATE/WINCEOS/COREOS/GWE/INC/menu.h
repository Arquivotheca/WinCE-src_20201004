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
#ifndef __MENU_H_INCLUDED__
#define __MENU_H_INCLUDED__

typedef UINT32 MENU_POSITION;
typedef int MNC;


// when to open or close a cascading menu
typedef enum { WHEN_NEVER, WHEN_NOW, WHEN_ONTIMER } WHEN;

// where is an item in a scrolling menu
typedef enum {WHERE_VISIBLE, WHERE_OFFBOTTOM, WHERE_OFFTOP} WHERE;


//    Candidate for the position of a cascading menu
class CItem;
class CMenuControl;



class MenuPositionCandidate_t
{
public:

    int x;                          // x-coordinate of this candidate
    BOOL fDropLeft;                 // does this candidate involve dropping left?
    int cxOverlapParent;            // how much does this candidate overlap parent?
};

class CMenu
{

public:

    static BOOL s_fSupportsCascade;
    static BOOL s_fSupportsMultiColumn;
    static BOOL s_fSupportsScrolling;
    static BOOL s_fUseTapOnlyUI;    // Use the "tap only" UI?

    long m_cx;                      // width
    long m_cy;                      // height
    SIZE m_sizeScreen;              // Screen size at time of menu size computation.
    HDC m_hdc;
    HPROCESS m_hprc;                // owner process
    UINT32 m_sig;                   // signature
    HWND m_hwnd;                    // The window that is this menu
    HWND m_hwndNotify;              // Who gets notifications
    CItem* m_rgitem;                // Resizable array of items
    CItem* m_pitemSelected;         // The selected menu item
    CItem* m_pitemLastButtonDown;   // The last item to get a LBUTTONDOWN
    BOOL m_fPopup:1;                // Is it a popup?
    BOOL m_fBeingDestroyed:1;       // Have we called DestroyWindow or DestroyMenu?
    BOOL m_fWMDestroy:1;            // Used by DestroyMenu to determine if called from WM_DESTROY handler
    BOOL m_fRTL:1;                  // Is it RTL menu?
    DWORD m_dwLastGesture;          // Gesture command currently being handled
    DWORD m_bMeasureGestureLatency; // Flag to determine whether to use the current frame to measure gesture latency

    BOOL
    Initialize(
        void
        );

    MENU_POSITION
    PositionFromCommandID(
        UINT32 uID
        );

    CItem*
    LookUpItem(
        UINT wID,
        BOOL fByPosition,
        HMENU* phmenuFound,
        UINT* piFound = NULL
        );

    int
    FindNextValidItem(
        int i,
        int dir,
        WCHAR chMnemonic
        );

    void
    Select(
        CItem* pitem,
        WHEN when = WHEN_NEVER
        );

    MNC
    HandleChar(
        WCHAR ch,
        CItem** ppitem
        );

    BOOL
    Valid(
        void
        );

    void*
    operator new(
        size_t cb
        );

    void
    Delete(
        void
        );

    HMENU
    GetHmenu(
        void
        );

    long
    FullWidth(
        void
        );

    long
    FullHeight(
        void
        );

    BOOL
    PtInMenu(
        const POINT& pt
        );

    void
    HandlePaint(
        HDC hdc,
        RECT rcPaint
        );

    bool
    SizeIsKnown(
        void
        );

    void
    SizeIsKnownFalse(
        void
        );

    virtual
    void
    ComputeSize(
        void
        ) = 0;

    virtual
    CItem*
    ItemFromPoint(
        POINT* ppt
        ) = 0;

    virtual
    LRESULT
    WndProc(
        UINT msg,
        WPARAM wParam,
        LPARAM lParam
        );

};


class CPopupMenu : public CMenu
{
public:

    void ComputeSize(void);
    void SingleColumnComputeSize(void);
    CItem* ItemFromPoint(PPOINT ppt);
    CItem* SingleColumnItemFromPoint(PPOINT ppt);
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void ExitMenuLoop(void);
    void Execute(CItem* pitem, BOOL fSelectFirst);
    void Prepare(HWND hWnd, UINT uPos, UINT uFlags);
    void PrepareToShow(CMenuControl* pmenuControl, BOOL fSelectFirst);
    bool PrepareToShow1(CMenuControl* pmenuControl, BOOL fSelectFirst);
    void DestroyMenuWindow();

    // for cascading popup menus
    BOOL CascadeProcessKey(WPARAM& wParam);
    void ComputeCascadingMenuPosition(CItem* pitemParent);

    void ComputeCascadingCandidatePositions(
        POINT& ptItem,
        MenuPositionCandidate_t& candLeft,
        MenuPositionCandidate_t& candRight,
        CItem* pitemParent
        );

    void OpenCascadingMenu(CPopupMenu* pmenuParent, CItem* pitemParent,
                    BOOL fSelectFirst);
    void CloseCascadingMenu(void);
    void CascadeExitMenuLoop(void);
    BOOL IsCascade(void) { return !!m_pmenuParent; }
    void SetOpenTimer(void);
    void KillOpenTimer(void);
    void SetCloseTimer(void);
    void KillCloseTimer(void);
    void ForceCloseTimer(void);
    BOOL AnotherMenuWantsClick(UINT msg, WPARAM wParam, POINT pt, BOOL fScreen);
    BOOL IsOpen(void) { return !!m_hwnd; }
    BOOL CascadeHandleMouseMessage(CItem* pitem, UINT msg);
    static void InitializeScrollingMenus(void);

    // for multi-column menus
    void MultiColumnComputeSize(void);
    void DetermineMultiColumnness(void);
    CItem* MultiColumnItemFromPoint(PPOINT ppt);
    BOOL MultiColumnProcessKey(WPARAM& wParam);

    // for scrolling menus
    void ScrollingPrepareToShow(void);
    CItem* ScrollingItemFromPoint(PPOINT ppt);
    void ScrollingComputeSize(void);
    void KillScrollTimers(void);
    void FixedScroll(UINT dy);

    // For tooltip
    void DisplayToolTip(void);

	bool
	DrawAnimatedMenu(
		BOOL fSelectFirst
		);      

    long m_x;                           // screen coordinates
    long m_y;                           // screen coordinates
    RECT m_rcNonDismiss;                // Rectangle you can touch w/o dismissing menu
    HWND m_hwndToolTip;                 // Handle to the tooltip window
    CMenuControl* m_pmenuControl;       // the menu control that "owns" this popup
    LPARAM m_lPendingButtonDown;        // The lParam of the button down that dismissed the menu
    WPARAM m_wPendingButtonDown;        // The wParam of the button down that dismissed the menu
    BOOL m_fPendingLeftButtonDown:1;    // True if the left button dismissed the menu
    BOOL m_fInsideMenuLoop:1;           // Processing msgs inside TrackPopupMenu?
    BOOL m_fReturnCmd:1;                // Was TPM_RETURNCMD specified?
    BOOL m_fOpenTimerRunning:1;         // Is there an Open timer running?
    BOOL m_fCloseTimerRunning:1;        // Is there a Close timer running?
    BOOL m_fDroppedLeft:1;              // Was this menu dropped to left of its parent?
    BOOL m_fIsMultiColumn:1;            // Does it have multiple columns?
    BOOL m_fScrolling:1;                // Is it a scrolling menu?
    BOOL m_fUpArrowEnabled:1;           // In a scrolling menu, is up arrow enabled?
    BOOL m_fDownArrowEnabled:1;         // In a scrolling menu, is down arrow enabled?
    BOOL m_fScrollTimerRunning:1;       // Is there a scroll timer running?
    BOOL m_fTimerSet:1;                 // True if the timer is active.
    CPopupMenu* m_pmenuParent;          // the parent in a cascade
    static UINT s_cmd;                  // command returned when TPM_RETURNCMD is on

#ifndef WPC_NEW
#define WPC_NEW
#endif
#ifdef WPC_NEW
    LPRECT m_prcClipExclude;
#endif

private:
    BOOL OnGesture(WPARAM wParam, LPARAM lParam);

    SHORT m_previousPanY;

};

class CMenuControl : public CMenu
{
public:

    void ComputeSize(void);
    CItem* ItemFromPoint(PPOINT ppt);
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void Open(CItem* pitem, BOOL fSelectFirst);
};

class CItem
{
public:
    UINT32 m_grf;
    long m_x;           /* location in client coordinates */
    long m_y;           /* location in client coordinates */
    long m_cx;          /* These two fields must be      */
    long m_cy;          /* adjacent and look like a SIZE */
    long m_ichAccel;    /* index of first character after tab */
    long m_cxAccel;     /* width of the accelerator text */
    UINT32 m_cchString; /* not including NULL */
    DWORD m_dwItemData; /* for the application's use */
    long m_xAccel;      /* where the accelerator text starts */
    PTSTR m_pszTT;      /* text for the tooltip */

    union
    {
    UINT32 m_uID;
    CPopupMenu*  m_pmenuPopup;/* the other menu this item pops up. May be NULL */
    };
    union
    {
    PTSTR m_pszString;
    DWORD m_dwTypeSpecificData;
    };

/*
REVIEW: We should use this member variable to handle non-prefix mnemonics, instead of having the
        additional MFMN_USENONPREFIX flag. Right now, the mnemonic is just stuffed into the
        item string.
#ifdef WPC
    TCHAR m_chNonPrefixMnemo; // Special mnemonic separate from item text. May be NULL.
#endif // WPC
*/

    void ComputeSize(CMenu* pmenu);
    void Cleanup(BOOL fFreeSubmenus);
    void Draw(CMenu* pmenu, UINT uDrawAction, HDC hdc);
    void Select(CMenu* pmenu, WHEN when = WHEN_NEVER);
    void Deselect(CMenu* pmenu, BOOL fRedraw = TRUE, WHEN when = WHEN_NOW);

    void CascadeSelect(CMenu* pmenu, WHEN when);
    void CascadeDeselect(CMenu* pmenu, WHEN when);
    void ScrollingSelect(CPopupMenu* pmenu);
    WHERE GetWhere(CPopupMenu* pmenu);
    BOOL IsItemVisible(CPopupMenu* pmenu);
    BOOL InUpdateRect(RECT * prc);

    void FreeTypeData();
    BOOL UseTypeDataAsString(void);

    BOOL IsString(void)
    {
        return !(m_grf &(MFT_OWNERDRAW | /*MFT_BITMAP |*/ MFT_SEPARATOR));
    }

    BOOL IsPopup(void)
    {
        return m_grf & MF_POPUP;
    }

    BOOL IsSeparator(void)
    {
        return m_grf & MF_SEPARATOR;
    }

    BOOL IsBreak(void)
    {
        return m_grf &(MF_MENUBREAK | MF_MENUBARBREAK);
    }

};


void
InternalMenuKeyFilter(
    HMENU hmenu,
    WORD ch
    );

BOOL IsMenuCommandDisabled(
    HWND hwnd,
    HMENU hmenu,
    DWORD cmd
    );


class MenuControlCompatibility_t
{
public:

    static
    bool
    IsTpc(
        void
        );

    static
    bool
    IsPpcOrTpc(
        void
        );
};


// Flags to specify the style of automatic mnemonics
#define AMF_NPRE_ASCENDNUM  0x00000001 // Non-prefix ascending numeric layout
#define AMF_PRE_TABLELOOKUP 0x00000002 // Use lookup table to assign prefix mnemonics
#define AMF_PRE_ITEMSCAN    0x00000004 // Scan item text for possible prefix mnemonics, if none are present

#endif /* __MENU_H_INCLUDED__ */
