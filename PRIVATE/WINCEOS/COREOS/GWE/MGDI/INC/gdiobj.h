//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*


*/
/*
    @doc BOTH INTERNAL
    @module GdiObj.h | Declaration of GDI objects.
*/
#ifndef _GDIOBJ_H_
#define _GDIOBJ_H_

#include <winddi.h>
#include "GdiDefs.h"
#include "DispDrvr.h"
#include "GdiObjBase.hpp"


class ColorTable_t;
class Driver_t;
class MDC;

extern UINT32          v_iProcUser;
extern HPALETTE        v_hpalDefault;
extern USHORT          g_iUniq;


// This function is used to verify that a buffer doesn't cross a proccess boundry
// and takes the place of MapCallerPtr in GDI because GDI can be called with a
// user pointer or a gwes.exe pointer if called internally by GWE, GDI, DDraw, or
// D3DM.

LPVOID
GdiValidateBuffer(
	LPVOID ptr,
	DWORD dwLen
	);

// Instead of using MapCallerPtr, use << GdiValidateBuffer>> . The function signatures
// are the same. See the comments above for more information.

/***   From winddi.h:

typedef struct _XLATEOBJ
{
    ULONG   iUniq;
    FLONG   flXlate;
    USHORT  iSrcType;
    USHORT  iDstType;
    ULONG   cEntries;
    ULONG  *pulXlate;
} XLATEOBJ;

***/

//
//  "Extended" color transformation object
//
//  The XLATEOBJ is used to tell the device driver how to do color conversions when Bltting.
//  Information about the source and destination COLORTABLEs are cached after the XLATEOBJ
//  members, for our use during callbacks.
//
class EXLATEOBJ : public XLATEOBJ
	{
public:
    ULONG         aulXlate[2];
	ColorTable_t  *m_pcolortblDst;
	ColorTable_t  *m_pcolortblSrc;

    EXLATEOBJ()  { pulXlate = NULL; }
    ~EXLATEOBJ() {}


	BOOL
	bSetup(
		const	ColorTable_t*	pcolortblDst,
				int				formatDst,
		const	ColorTable_t*	pcolortblSrc,
				int				formatSrc
		);


	BOOL
	bSetup(
		const	DC*			pdcDst,
		const	Brush_t*	pBrush
		);

	BOOL
	bSetup(
		const	DC*	pdcDst,
		const	DC*	pdcSrc
		);

	BOOL
	bSetup(
		const  DC*         pdcDst,
		const  ColorTable_t* pcolortblSrc,
		int    formatSrc
	);

	};


/*
	@class VBITMAP | Bitmap object (base class)

	@access Public Members

	@cmember PVOID | m_pvBits || Pointer to bitmap bits.
	@cmember DWORD | m_BitmapType || Describes what address space bits live in

	@cmember virtual int | GetObject | (int,PVOID) |
		Returns information about the bitmap.
	@cmember virtual GDIOBJ * | SelectObject | (DC *) | Selects bitmap into DC.

	@devnote We do not create objects of this class, only of its derived
	classes, MBITMAP and WBITMAP.

*/

// Values for VBITMAP::m_bitsrc
#define BMTYPE_NULL    0   // no bitmap yet
#define BMTYPE_GWE     1   // gwe space (localfree)
#define BMTYPE_CLIENT  2   // client space (remotefree)
#define BMTYPE_RSRC    3   // resource (don't free)
#define BMTYPE_FRAME   4   // framebuffer (don't free)
#define BMTYPE_DRIVER  5   // driver-created
#define BMTYPE_DDRAW   6   // DDraw surface (don't free)




class BitmapBase_t : public GDIOBJ
{
private:


protected:

	BitmapBase_t(
		GdiObjectHandle_t,
		GdiObjectType_t
		);


public:

	DWORD			m_BitmapType;	// BMTYPE_xxx values, above
	UINT32			m_nBpp;
	SURFOBJ*		m_pso;
	ColorTable_t*	m_pcolortbl;	// Color table to use with this bitmap
	BOOL            m_bDIB;    // Is this bitmap a dibsect


	static
	BitmapBase_t*
	GET_pVBITMAP(
		HGDIOBJ
		);


	HBITMAP
	GetHBITMAP(
		void
		) const;


	inline
	int
	Width(
		void
		)
	{
		return m_pso->sizlBitmap.cx;
	}


	inline
	int
	Height(
		void
		)
	{
		return m_pso->sizlBitmap.cy;
	}


	inline
	int
	Format(
		void
		)
	{
		return m_pso->iBitmapFormat;
	}


	inline
	BOOL
	bDIBSection(
		void
		)
	{
		return m_bDIB;
	}

	//	DibSize returns number of bytes used by this bitmap's image
	DWORD
	DibSize(
		void
		);


	virtual
	int
	GetObject(
		int,
		PVOID
		);



	virtual
	GDIOBJ*
	SelectObject(
		DC*
		);



#if DEBUG
	virtual
	int
	Dump(
		void
		);
#endif

	};


HBITMAP
GdiCopyBitmap(
	HBITMAP	hbm,
	LONG*	pnWidth,
	LONG*	pnHeight
	);



/*
    @class MBITMAP | Memory Bitmap object

    @access Public Members

    @cmember void | MBITMAP | () | Constructor.
    @cmember void | ~MBITMAP | () | Destructor.

    @cmember BOOL | Create | (UINT32 nBitsPP,INT32 nWidth,INT32 nHeight) |
        Required initialization function.

    @devnote <f CreateDIBSectionStub> initializes this structure manually
        instead of calling <mf MBITMAP.Create>.
*/
class  MBITMAP : public BitmapBase_t
{
private:

	void*
	operator new(
		size_t
		);

	MBITMAP(
		GdiObjectHandle_t	GdiObjectHandle
		);

public:
    SURFOBJ         m_so;

	void
	operator delete(
		void*
		);

	static
	MBITMAP*
	New(
		void
		);


    ~MBITMAP(
		void
		);


	static
	MBITMAP*
	GET_pMBITMAP(
		HBITMAP	hbitmap
		);


	BOOL
	Create(
		UINT32			nBitsPP,
		INT32			nWidth,
		INT32			nHeight,
		ColorTable_t*	pcolortbl // Get refcounted by MBITMAP
		);

	BOOL
	Create(
		UINT32		nBitsPP,
		INT32		nWidth,
		INT32		nHeight,
		Driver_t*	pdrv
		);

	BOOL
	Create_AllocSetup(
		UINT32	nBitsPP,
		INT32	nWidth,
		INT32	nHeight
		);



};



/*
    @class WBITMAP | GDI representation of window

    @access Public Members

    @cmember RECT | m_rWindow || Position and extent of window.
    @cmember HRGN | m_hrgnSysClip || System clipping (visible) region.
    @cmember DC * | m_pdcList || List of device contexts.

        When the system clipping region is changed,
        each of the device contexts that refers to this window
        must have its clipping region updated.

    @cmember void | WBITMAP | () | Constructor.
    @cmember void | ~WBITMAP | () | Destructor.

    @cmember void | SetViewport | (CONST RECT *) | Set position and extent.

        This should always be followed by <mf WBITMAP.SelectSysClipRgn>.

    @cmember BOOL | SelectSysClipRgn | (RGNDAT *) | Set system clipping region.

        Updates clipping regions for all device contexts
        which refer to this window.

    @cmember virtual BOOL | DeleteObject | () | Deletes object.
    @cmember virtual GDIOBJ * | SelectObject | (DC *) |
        Sets DC pointing to window.


    @comm This class does not correspond to any GDI object that is visible
        to an application.  It is used to represent windows in GDI.
        All windows point to the same bits (the frame buffer).
*/
class WBITMAP : public BitmapBase_t
{
private:

	void*
	operator new(
		size_t
		);


	WBITMAP(
		GdiObjectHandle_t	GdiObjectHandle,
		Driver_t*			pdriver
		);



public:
	static	WBITMAP*		s_pwbmSelectSysClipRgnLatest;
	static	HRGN			s_hrgnSelectSysClipRgnLatest;
	static	HRGN			s_hrgnNull;


	RECT	m_rWindow;
	HRGN	m_hrgnSysClip;
	DC*		m_pdcList;

	static
	WBITMAP*
	New(
		Driver_t*
		);


	void
	operator delete(
		void*
		);



	~WBITMAP(
		void
		);


	void
	SetViewport(
		const	RECT*
		);


	BOOL
	SelectSysClipRgn(
		HRGN
		);


	virtual
	BOOL
	DeleteObject(
		void
		);


	virtual
	GDIOBJ*
	SelectObject(
		DC*
		);


	static
	void
	CheckSysClipRgn(
		GdiObjectHandle_t	GdiObjectHandle
		);

};




class Pen_t : public GDIOBJ
{
private:

	void*
	operator new(
		size_t
		);

	Pen_t(
				GdiObjectHandle_t,
		const	LOGPEN*
		);

public:
    LOGPEN          m_LogPen;


	void
	operator delete(
		void*
		);



	static
	Pen_t*
	New(
		const	LOGPEN*
		);

	static
	Pen_t*
	GET_pPEN(
		HPEN	hpen
		);

	HPEN
	GetHPEN(
		void
		) const;


    virtual
	int
	GetObject(
		int,
		PVOID
		);



    virtual
	GDIOBJ*
	SelectObject(
		DC*
		);

#if DEBUG
	virtual
	int
	Dump(
		void
		);
#endif
};

BOOL IsCurrentPenWide(HDC);

/*
    @class BRUSH | Brush object.

    @access Public Members

    @cmember LOGBRUSH | m_LogBrush || <t LOGBRUSH> structure.
    @cmember PVOID | m_pvAppDib ||
        Pointer to application DIB if this is a pattern brush.

    @cmember void | BRUSH | () | Constructor.
    @cmember void | ~BRUSH | () | Destructor.

    @cmember BOOL | Create | (CONST LOGBRUSH *) | Required initialization.

    @cmember virtual int | GetObject | (int,PVOID) |
        Returns information about the brush.
    @cmember virtual GDIOBJ * | SelectObject | (DC *) | Selects brush into DC.
*/
class Brush_t : public GDIOBJ
{
private:

	void*
	operator new(
		size_t
		);


	Brush_t(
		GdiObjectHandle_t
		);


public:

	LOGBRUSH	m_LogBrush;
	MBITMAP*	m_pBitmap;  // bitmap containing pattern

	static
	Brush_t*
	New(
		void
		);

	void
	operator delete(
		void*
		);



	~Brush_t(
		void
		);

	HBRUSH
	GetHBRUSH(
		void
		);



	static
	Brush_t*
	GET_pBRUSH(
		HGDIOBJ
		);



    BOOL
	Create(
		const	LOGBRUSH*
		);


    virtual
	int
	GetObject(
		int,
		PVOID
		);



    virtual
	GDIOBJ*
	SelectObject(
		DC*
		);


    #if DEBUG
    virtual int     Dump (void);
    #endif

};


struct LOGFONTWITHATOM
{
    LONG      lfHeight;
    LONG      lfWidth;
    LONG      lfEscapement;
    LONG      lfOrientation;
    LONG      lfWeight;
    BYTE      lfItalic;
    BYTE      lfUnderline;
    BYTE      lfStrikeOut;
    BYTE      lfCharSet;
    BYTE      lfOutPrecision;
    BYTE      lfClipPrecision;
    BYTE      lfQuality;
    BYTE      lfPitchAndFamily;
    ATOM      lfAtomFaceName;
};



class Font_t : public GDIOBJ
{
private:

	void*
	operator new(
		size_t
		);

	Font_t(
				GdiObjectHandle_t,
		const	LOGFONTW*
		);

public:
	LOGFONTWITHATOM	m_LogFont;

	//	TrueType font realizations (RFONTOBJ); unused in the raster font engine.
	void*	m_prfont;           // Monochrome realization
	void*	m_prfontAA;         // Anti-Aliased version of this realization
	void*	m_prfontCT;
	UINT32	m_uEffxDisplay;     // Display-time font effects, eg, Bold, underline, strikeout


	static
	Font_t*
	New(
		const	LOGFONTW*
		);

	void
	operator delete(
		void*
		);


	~Font_t(
		void
		);


	static
	Font_t*
	GET_pFONT(
		HFONT
		);




	HFONT
	GetHFONT(
		void
		);



	virtual
	int
	GetObject(
		int,
		void*
		);



	virtual
	GDIOBJ*
	SelectObject(
		DC*
		);

#if DEBUG
	virtual
	int
	Dump(
		void
		);
#endif
};



class Palette_t : public GDIOBJ
{
private:

    ColorTable_t  *m_pcolortbl;

	void*
	operator new(
		size_t
		);

	Palette_t(
		GdiObjectHandle_t	GdiObjectHandle
		);

public:

	static
	Palette_t*
	New(
		void
		);


	void
	operator delete(
		void*
		);



	~Palette_t(
		void
		);

	//	If constructor succeeds (ppalette, handle are not NULL),
	//	FInitPal() has to be called to init newly created object
    BOOL
	FInitPalette(
		const	PALETTEENTRY*	ppe,
				ULONG			cLogPalEntries,
				BOOL			fStock
				);


	static
	Palette_t*
	GET_pPALETTE(
		HPALETTE
		);



	HPALETTE
	GetHPALETTE(
		void
		) const;




	virtual
	int
	GetObject(
		int,
		PVOID
		);



    virtual
	GDIOBJ*
	SelectObject(
		DC*
		);


	Palette_t*
	SelectPalette(
		DC*
		);

	//	Getting/Setting palette entries, all params are expected to be validated by the caller.
    void
	GetEntries(
		UINT32			uiStart,
		UINT32			uiNum,
		PALETTEENTRY*	pPe
		);

	void
	SetEntries(
				UINT32			uiStart,
				UINT32			uiNum,
		const	PALETTEENTRY*	pPe
		);


    ULONG
	xxxSetSysPal(
		Driver_t*	pdriver
		);


#ifdef DEBUG
    virtual
	int
	Dump(
		void
		)
	{
		int cThis;
		DEBUGMSG (1, (TEXT("PALETTE size=%d\r\n"), cThis = LocalSize(this)));
		return cThis;
	}
#endif

	ColorTable_t*
	Pcolortbl(
		void
		) const;

	PALETTEENTRY*
	PPalentries(
		void
		) const;

	USHORT
	CEntries(
		void
		) const;


};


//
// At app activation is called by DefWinProc if app is not processing
// WM_QUERYNEWPALETTE message
//
void LoadDefaultPalette(HWND hwnd);

//
// Creates default(stock) palette object on !PAL_INDEXED hw platforms
//
HPALETTE HCreateDefaultPal(void);


//
// PRTSTAT is used to track the status of a print job.  It is only used
// in printer DCs.
//
// The normal usage of a printer DC looks something like this (the comment
// on each line says what PRTSTAT the DC has after each statement):
//
//      HDC hdc = CreateDC(..."PRINTER"...);    // prtstatPreDoc
//
//      StartDoc(hdc, pdocinfo);                // prtstatPrePage
//
//      while (more pages to print)
//      {
//          StartPage(hdc);                     // prtstatRecord
//
//          /* Compose page via GDI calls */
//
//          EndPage(hdc);                       // prtstatPlayback as GDI
//                                              // commands are banded to the
//                                              // driver, then
//                                              // prtstatPostPage when done
//      }
//
//      EndDoc(hdc);                            // prtstatPreDoc
//
// If the apps calls AbortDoc, the print job is aborted and the print DC's
// state is returned to prtstatPreDoc (which means the print job can be
// tried again by calling StartDoc).
//
typedef BYTE PRTSTAT;

//
// Normal printer DC usage will progress through these states in order
//
#define prtstatPreDoc   ((PRTSTAT)0)    // Waiting for call to StartDoc
#define prtstatPrePage  ((PRTSTAT)1)    // Waiting for call to StartPage
#define prtstatRecord   ((PRTSTAT)2)    // Recording GDI calls into metafile
#define prtstatPlayback ((PRTSTAT)3)    // Playing metafile back to driver
#define prtstatPostPage ((PRTSTAT)4)    // Page has been played back to driver




//
// Flags for dc.m_ulDirty.
//
#define DC_PLAYMETAFILE         0x00000001


// macros to validate the handles passed in and setup some local variables
// for accessing the handle information.

#define DC_PDC(hdc, pdc, ret)                   \
    do {                                        \
        pdc = DC::GET_pDC(hdc);                     \
		if ( !pdc )	\
        {                                       \
            ASSERT(0);                        \
            SetLastError(ERROR_INVALID_HANDLE); \
            return(ret);                        \
        }                                       \
    } while (0)




#define IS_RGND(P)          ((P)->rdh.nCount < 0x10000)

ULONG   iBmfFromBpp(INT32 iBpp);
INT32   iBppFromBmf(ULONG iBitmapFormat);
void    dib_setup(SURFOBJ *, UINT32 *, ULONG, INT32, INT32);
BOOL    FValidBMI(CONST BITMAPINFO *pbmpi, UINT iUsage);

int InitializeGdiObjConstructors(void);


#endif  // _GDIOBJ_H_

