//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#define DIALOGCLASSNAME TEXT("Dialog")

#ifndef __CE_PTR_HPP_INCLUDED__
#include <CePtr.hpp>
#endif

/* Dialog structure (dlg) */
//IMPORTANT: sizeof this structure must be == DLGWINDOWEXTRA.
typedef struct
{
    LPARAM  result;          // contains result code for a processed message
	CePtr_t<DLGPROC>	m_DialogProcPtr;
    LONG    lUser;           // user space
    HANDLE  hProc;           // Process owning dlgproc
    HWND    hwndFocus;       // control with current focus
    LONG	lresult;		 // final return value for a modal dialog
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

void    CheckDefPushButton(HWND, HWND, HWND);
HWND    GetParentDialog(HWND hwndDlg);
HWND    NextControl(HWND hwndParent, HWND hwnd, UINT uSkip);
HWND    PrevControl(HWND hwndParent, HWND hwnd, UINT uSkip);
HWND    FindDlgItem(HWND hwndParent, int id);
void    DlgSetFocus(HWND hwnd);
HWND 	GotoNextMnem(HWND hwndDlg, HWND hwnd, WCHAR ch);

#define IsDefPushButton(hwnd)   \
	(MsgQueue::SendMessageW_I(hwnd,WM_GETDLGCODE,0,0L)&DLGC_DEFPUSHBUTTON)
#define IsUndefPushButton(hwnd)      \
	(MsgQueue::SendMessageW_I(hwnd,WM_GETDLGCODE,0,0L)&DLGC_UNDEFPUSHBUTTON)
#define IsPushButton(hwnd)      \
	(MsgQueue::SendMessageW_I(hwnd,WM_GETDLGCODE,0,0L)&(DLGC_UNDEFPUSHBUTTON|DLGC_DEFPUSHBUTTON))

// Flags to specify movement across dialog items
#define CHANGEDLGITEM_TYPE_TAB      0x00000001 // Change the tabbing position
#define CHANGEDLGITEM_TYPE_GRP      0x00000002 // Change the position within a group of controls
#define CHANGEDLGITEM_TYPE_MASK     (CHANGEDLGITEM_TYPE_TAB | CHANGEDLGITEM_TYPE_GRP)

#define CHANGEDLGITEM_DIR_PREV      0x80000000 // Move to the previous item


// Thranslate a key press code into a dialog item change code
static BOOL GetChangeDlgItemCode(
    WPARAM wKeyPressed,     // In: Key press code from WM_KEYDOWN
    UINT *pnChangeCode,      // Out: dialog item change code
    BOOL bRTLDlg	                // In: True if the Dialog is RTL
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


BOOL ChangeDlgTabItem(HWND hwndDlg, LPMSG lpMsg, WORD nDlgCode, BOOL fPrev);
BOOL ChangeDlgGroupItem(HWND hwndDlg, LPMSG lpMsg, WORD nDlgCode, BOOL fPrev);
BOOL WmKeydown(
			HWND	hwnd,
			HWND	hwnd2,
			HWND	hwndDlg,
			WORD    wDefID,
			LPMSG 	lpmsg,
			WORD	code
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

BOOL
NotifyDialogManager(
    HWND				hwndDlg,
	DWORD				oldStyle,
	DWORD				newStyle,
	DWORD				ScrollbarStateFlag
	);

// string used to store dialog base units in window property
static const WCHAR* const g_szBaseUnit	= L"bu";




class DialogManager_t
{

public:

	static
	BOOL
	Initialize(
		HINSTANCE	hInstance
		);


	static
	int
	DialogBoxIndirectParamW_I(
		HINSTANCE			hInstance,
		LPCDLGTEMPLATE		lpdt,
		HWND				hwndOwner,
		CePtr_t<DLGPROC>	DialogProcPtr,
		LPARAM				lParam,
		CREATESTRUCTW*		pcrs,
		MSG*				pmsg
		);


	static
	HWND
	CreateDialogIndirectParamW_I(
		HINSTANCE			hInstance,
		LPCDLGTEMPLATEW		lpDT,
		HWND				hwndOwner,
		CePtr_t<DLGPROC>	DialogProcPtr,
		LPARAM				lParam,
		CREATESTRUCTW*		pcrs
		);


	static
	BOOL
	SetDlgItemInt_I(
		HWND	hwnd,
		int		lid,
		UINT	lValue,
		BOOL	fSigned
		);

	static
	UINT
	GetDlgItemInt_I(
		HWND	hwnd,
		int		lid,
		BOOL*	lpfValOK,
		BOOL	fSigned
		);

	static
	BOOL
	SetDlgItemTextW_I(
		HWND	hDlg,
		int		nIDDlgItem,
		LPCWSTR	lpString
		);

	static
	UINT
	GetDlgItemTextW_I(
		HWND	hDlg,
		int		nIDDlgItem,
		LPWSTR	lpString,
		int		nMaxCount
		);

	static
	LONG
	GetDialogBaseUnits_I(
		void
		);

	static
	BOOL
	MapDialogRect_I(
		HWND	hwnd,
		LPRECT	lprc
		);

	static
	BOOL
	EndDialog_I(
		HWND	hwnd,
		int		lresult
		);

	static
	BOOL
	IsDialogMessageW_I(
		HWND	hwndDlg,
		LPMSG	lpmsg
		);


	static
	int
	GetDlgCtrlID_I(
		HWND	hwnd
		);


	static
	HWND
	GetDlgItem_I(
		HWND	hDlg,
		int		iCtrlID
		);

	static
	BOOL
	CheckRadioButton_I(
		HWND	hwnd,
		int		idFirst,
		int		idLast,
		int		idCheck
		);

	static
	LONG
	SendDlgItemMessageW_I(
		HWND	hDlg,
		int		nIDDlgItem,
		UINT	Msg,
		WPARAM	wParam,
		LPARAM	lParam
		);

	static
	LRESULT
	DefDlgProcW_I(
		HWND	hwnd,
		UINT	message,
		WPARAM	wParam,
		LPARAM	lParam
		);

	static
	HWND
	GetNextDlgTabItem_I(
		HWND	hwndDlg,
		HWND	hwnd,
		BOOL	fPrev
		);


	static
	HWND
	GetNextDlgGroupItem_I(
		HWND	hwndDlg,
		HWND	hwnd,
		BOOL	fPrev
		);




};

