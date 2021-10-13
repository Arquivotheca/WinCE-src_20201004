//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
    @class DC | Device contexts.

    @access Public Members

    @cmember PEN * | m_pPen || Selected pen.
    @cmember BRUSH * | m_pBrush || Selected brush.
    @cmember MBITMAP * | m_pBitmap || Selected bitmap.
    @cmember FONT * | m_pFont || Selected font.
    @cmember RGNDAT * | m_prgndAppClip || Selected application clip region.
        The application clipping region is given in application coordinates.
        The clipping region is adjusted when the actual clipping region
        is computed, if necessary.
    @cmember COLORREF | m_crBackground || Current background color.
    @cmember COLORREF | m_crForeground || Current foreground (text) color.
    @cmember int | m_iBkMode || Current background mode.
    @cmember POINT | m_ptBrushOrg || Current brush origin.

    @cmember FONTRESOURCE * | m_pFontResource || Realization of font.
    @cmember UINT32 | m_uiBackground || Realization of background color.
    @cmember UINT32 | m_uiForeground || Realization of foreground color.

    @cmember XCLIPOBJ | co || Clipping object, containing the actual clipping region.
        The actual clipping region is given in device coordinates.
    @cmember DWORD | m_dwDcType || Type of device context.
    @cmember DC * | m_pdcStack || Stack of saved device contexts.
    @cmember DC * | m_pdcList || Device contexts pointing to same window.
    @cmember INT32 | m_iStackDepth || Stack depth.

    @cmember void | DC | () | Constructor.
    @cmember void | ~DC | () | Destructor.

    @cmember BOOL | UpdateClipping | (BOOL) | Update clipping region for DC.
    @cmember int | ModifyClipRect | (int nLeftRect, int nTopRect, int nRightRect, int nBottomRect, RGNFUNC fnRgnFunc) |
         Implements ExcludeClipRect and IntersectClipRect.

    @cmember int | SetAppClipRgn | (RGNDAT * prgn) |
        Set the application clipping region


    @cmember virtual int | GetObject | (int,PVOID) | This function fails.
    @cmember virtual DWORD | GetObjectType | () | Returns type of DC.
    @cmember virtual GDIOBJ * | SelectObject | (DC *) | This function fails.
*/

#ifndef __DC_HPP_INCLUDED__
#define __DC_HPP_INCLUDED__

#include  <windows.h>
#include <winddi.h>
#include <GdiObjBase.hpp>
#include <Rgn.hpp>

class FONTRESOURCE;
class XPATHOBJ;
class Driver_t;


/*
    Realized brush object.
    Contains the brush color (for solid brushes)
    or a pointer to the pattern bitmap (for pattern brushes).

From winddi.h:
typedef struct _BRUSHOBJ
{
    ULONG   iSolidColor;
    PVOID   pvRbrush;
} BRUSHOBJ;

*/
struct XBRUSHOBJ : public BRUSHOBJ
{
    DC *m_pdc;
};


//
// All public APIs should validate their HDC parameters by using
// ValidateAndForwardHdc.  Cases are:
//
//      1) Bogus HDC  -- Return NULL pdc and hdc
//      2) Printer DC -- Return pdc/hdc of associated metafile DC
//      3) Other DC   -- Return pdc/hdc of input HDC
//
// Note that the "Forward" part of this APIs name refers to the
// forwarding of API calls from printer DCs to their associated
// metafile DCs.  This is done iff the printer DC is currently
// recording GDI calls into the metafile DC.
//
// The DCOP parameter defines what kind of operation is going to be
// performed on the DC.  Output operations on print DCs can succeed
// only if the DC has been "primed" by calls to StartDoc and StartPage.
// Without those calls, printer DCs are considered invalid for
// output operations, but valid for property get/set calls.
//
enum DCOP   // DC Operation
{
    dcopProp,       // Get/set property
    dcopOutput      // Graphical output (and input, like GetPixel)
};



//class DcHandle_t : public GdiObjectHandle_t
//{
//public:
//
//
//	DcHandle_t(
//		void
//		);
//
//	DcHandle_t(
//		HDC
//		);
//
//	HDC
//	AsHDC(
//		void
//		) const;
//
//	DC*
//	GET_pDC(
//		void
//		) const;
//
//
//};

struct ForwardedDc_t
{
	HDC			m_Handle;
	DC*			m_pDc;

	HDC			m_OriginalHandle;
	DC*			m_pOriginalDc;

	ForwardedDc_t(
		void
		);
	
};


ForwardedDc_t
ValidateAndForwardHdc(
	HDC		hdc,
	DCOP	dcop
	);


class DC : public GDIOBJ
{
private:

	void*
	operator new(
		size_t
		);

	DC(
		GdiObjectHandle_t	GdiObjectHandle,
		HDC					hdcRef,
		GdiObjectType_t		DcType,
		WBITMAP*			pwbitmap,
		BOOL				bDcEx
		);

    DC(
		GdiObjectHandle_t	GdiObjectHandle,
		Driver_t*			pdriver,
		GdiObjectType_t		DcType,
        WBITMAP*			pwbitmap
		);


    void
	FinishConstruction(
		Driver_t*		pdriver,
		GdiObjectType_t	DcType,
		WBITMAP*		pwbitmap
		);


public:

	Pen_t*			m_pPen;
	Brush_t*		m_pBrush;
	BitmapBase_t*	m_pBitmap;
	Font_t*			m_pFont;
    RGNDAT*			m_prgndAppClip;
    COLORREF        m_crBackground;
    COLORREF        m_crForeground;
    POINTL          m_ptAppBrushOrg;
	Palette_t*		m_pPalette;
    POINTL          m_ptViewportOrg;

    // realized attributes
    XBRUSHOBJ       m_xboBrush;
    POINTL          m_ptBrushOrg;
    XBRUSHOBJ       m_xboPen;
    FONTRESOURCE*	m_pFontResource;
    UINT32          m_uiBackground;
    UINT32          m_uiForeground;
    POINTL          m_ptCurrentPos;
    UINT32          m_ulTextAlign;
	
    // implementation details
    //
    // REVIEW MartSh: Make m_dwDcType a bitfield and combine with the
    // other bitfields below?
    //
    // We need to balance the data-size savings of this against the
    // increased code size, especially since m_dwDcType is used so
    // often (usually through inline methods).
    //
    DWORD           m_dwDcType;     // OBJ_DC, OBJ_MEMDC, or OBJ_ENHMETADC.
    DC*             m_pdcStack;     // stack of DCs for SaveDC() and RestoreDC()
    DC*             m_pdcList;      // list of DCs that point to same WBITMAP
    INT32           m_iStackDepth;  // stack depth
    XCLIPOBJ		co;             // contains RGNDAT with intersection of app's clip w/ bitmap or screen
    Driver_t*		m_pdriver;      // Pointer to all info about this DC's driver

    //	Sleazy data packing tricks start here.
    BYTE            m_iBkMode;
    BYTE            m_ROP2;
    /*BOOL*/ BYTE   m_bPrint;   // TRUE iff printer DC.  This could be a
                                // bitfield, but we use it in almost
                                // every API, and don't want to pay the
                                // code size/performance penalty

    /*BOOL*/ BYTE   m_fFree_prgndClip : 1;  // Free the above clip rgn when done?
    /*BOOL*/ BYTE   m_fDcEx : 1;            // Dc was created by a GetDCEx call.

    /*PRTSTAT*/ BYTE  m_prtstat : 3;  // Print job status
    // Sleazy data packing tricks end here.


    ABORTPROC         m_pfnAbortProc; // Set by SetAbortProc API
    HDC               m_hdcMetafile;  // Metafile used during banding

    MDC              *m_pmdc;

    //
    // These inline methods tell us if the DC is a printer DC,
    // a metafile DC, or an "alt" DC.  NT uses the word "alt" to
    // mean "either printer or metafile," so we'll use this
    // terminology, too.
    //
    inline BOOL     IsPrint() const { return m_bPrint; }
    inline BOOL     IsMeta()  const { return m_dwDcType == OBJ_ENHMETADC; }
    inline BOOL     IsAlt()   const { return IsMeta() || IsPrint(); }

    // IsPrintJobAborted executes the AbortProc associated with
    // the given printer DC.  Returns FALSE if the print job has been
    // aborted, TRUE otherwise.
    friend BOOL IsPrintJobAborted(HDC hdc);

	void
	operator delete(
		void*
		);

	static
	DC*
	New(
		HDC				hdcRef,
		GdiObjectType_t	DcType,
		WBITMAP*		pwbitmap,
		BOOL			bDcEx
		);

	static
	DC*
	New(
		HDC				hdcRef,
		GdiObjectType_t	DcType
		);



	static
	DC*
	New(
		Driver_t*		pdriver,
		GdiObjectType_t	DcType,
		WBITMAP*		pwbitmap
		);


	static
	DC*
	GET_pDC(
		HDC	hdc
		);

	HDC
	GetHDC(
		void
		) const;



public:

	~DC();

    //
    // REVIEW Vladimir/Martin: Which of these methods should be const?
    //
    BOOL            UpdateClipping(BOOL);
    int             ModifyClipRect (int, int, int, int, BOOL);
    int 			UpdateClipForResizeScreen();
    int             SetAppClipRgn (RGNDAT *);
    DRVENABLEDATA  *pDrvGet(void);
    virtual int     GetObject(int,void*);
    virtual GDIOBJ *SelectObject(DC *);
    UINT32          GetMix(void) { return (((UINT32)m_ROP2 << 8) | (UINT32)m_ROP2); }
    DWORD           dwRealizeColor(COLORREF clrf, COLORREF *pclrfReal = 0, BOOL fIgnoreError = 1);
    COLORREF            clrfUnrealizeColor(DWORD);

    POINTL          LogicalToDevice(const POINTL &pt);
    RECTL           LogicalToDevice(const RECTL& rc);
    RECT            LogicalToDevice(const RECT& rc);
    void            LogicalToDevice(XPATHOBJ *ppo);
	BOOL            GetDCOrg(POINTL *);
	void            SetDCOrg(long x, long y);
    // Return TRUE iff the DC's bitmap can be written to
    BOOL            FWritableBitmap();

#if DEBUG
    virtual int     Dump (void);
#endif
};



#endif

