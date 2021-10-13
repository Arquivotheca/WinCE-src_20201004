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
/*


*/
/*
    @doc BOTH INTERNAL
    @module GdiObj.h | Declaration of GDI objects.
*/
#ifndef _GDIOBJ_H_
#define _GDIOBJ_H_

#include <winddi.h>
#include <hgtypes.h>
#include "GdiDefs.h"
#include "DispDrvr.h"
#include "GdiObjBase.hpp"
#include <GweMemory.hpp>

class ColorTable_t;
class Driver_t;
class MDC;

extern UINT32           v_iProcUser;
extern HPALETTE         v_hpalDefault;
extern USHORT           g_iUniq;
extern CRITICAL_SECTION g_CreateDCLock;


// This function is used to verify that a buffer doesn't cross a proccess boundry
// and takes the place of MapCallerPtr in GDI because GDI can be called with a
// user pointer or a gwes.exe pointer if called internally by GWE, GDI, DDraw, or
// D3DM.

LPVOID
GdiValidateBuffer(
    __out_bcount(dwLen) LPVOID ptr,
    DWORD dwLen
    );


// Instead of using MapCallerPtr, use GdiValidateBuffer. The function signatures
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
class ColorTranslationExtended_t : public XLATEOBJ
{


public:

    ULONG   m_aulXlate[2];
    ColorTable_t* m_pcolortblDst;
    ColorTable_t* m_pcolortblSrc;


    ColorTranslationExtended_t(
        void
        )
    {
        pulXlate = 0;
    }


    BOOL
    bSetup(
        const   ColorTable_t* pcolortblDst,
                int    formatDst,
        const   ColorTable_t* pcolortblSrc,
                int    formatSrc
        );


    BOOL
    bSetup(
        const DC*   pdcDst,
        const Brush_t* pBrush
        );


    BOOL
    bSetup(
        const   DC* pdcDst,
        const   DC* pdcSrc
        );


    BOOL
    bSetup(
        const DC*    pdcDst,
        const ColorTable_t* pcolortblSrc,
                int    formatSrc
        );

    BOOL
    bSetup(
        const   DC* pdcDst
        );

};



class SurfObjWrapper_t
{
public:

    union
    {
    SURFOBJ m_SurfObj;

    struct  {
            DHSURF    dhsurf;
            SurfObjWrapper_t* m_pSurfObj;  // This is the surfobj handle, hsurf, in SURFOBJ.
            DHPDEV    m_dhpdev;
            Driver_t*   m_pDriver;  // This is the driver handle, hdev, in SURFOBJ.
            SIZEL    m_BitmapSize;
            ULONG    m_cjBits;
            void*    m_pSurfaceBits;
            void*    m_pScanLine0Start;
            LONG    m_Stride;
            ULONG    m_UniqueId;
            ULONG    m_BitmapFormat;
            USHORT    m_Type;
            USHORT    m_BitmapFormatFlags;
            };
    };



    SURFOBJ*
    SURFOBJ_Ptr(
        void
        )
    {
        return &m_SurfObj;
    }


    static
    SurfObjWrapper_t*
    PtrFromHbitmap(
        HBITMAP hbitmap
        )
    {
        return (SurfObjWrapper_t*)hbitmap;
    }


    void
    Dump(
        void
        ) const
    {
        RETAILMSG(1,(TEXT("SurfObjWrapper_t: %08x\n"), this));
        RETAILMSG(1,(TEXT("    m_pSurfaceBits: %08x\n"), m_pSurfaceBits));
        return;
    }


    void
    dib_setup(
        UINT32*    pBase,
        ULONG    iBmf,
        INT32    iWidth,
        INT32    iHeight
        );





};



#define BMTYPE_NULL         0   // no bitmap yet
#define BMTYPE_GWE          1   // gwe space (localfree)
#define BMTYPE_CLIENT       2   // client space (remotefree)
#define BMTYPE_RSRC         3   // resource (don't free)
#define BMTYPE_FRAME        4   // framebuffer (don't free)
#define BMTYPE_DRIVER       5   // driver-created
#define BMTYPE_DDRAW        6   // DDraw surface (don't free)
#define BMTYPE_COMPOSITION  7   // Compositor owned (don't free)

class BitmapBase_t : public GDIOBJ
{
private:


protected:

    BitmapBase_t(
        GdiObjectHandle_t,
        GdiObjectType_t
        );

public:

    DWORD    m_BitmapOwner; // BMTYPE_xxx values, above
    UINT32    m_nBpp;
    SurfObjWrapper_t* m_pSurfObj;
    ColorTable_t*  m_pColorTable;
    BOOL             m_IsDibSection;


    virtual
    ~BitmapBase_t(
        void
        )
    {
        return;
    }


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
        ) const
    {
        return m_pSurfObj -> m_BitmapSize.cx;
    }


    inline
    int
    Height(
        void
        ) const
    {
        return m_pSurfObj -> m_BitmapSize.cy;
    }

    inline
    int
    Stride(
        void
        ) const
    {
        return m_pSurfObj -> m_Stride;
    }

    inline
    int
    Format(
        void
        ) const
    {
        return m_pSurfObj -> m_BitmapFormat;
    }


    inline
    BOOL
    IsDibSection(
        void
        ) const
    {
        return m_IsDibSection;
    }

    SURFOBJ*
    SURFOBJ_Ptr(
        void
        )
    {
        return m_pSurfObj -> SURFOBJ_Ptr();
    }

    virtual
    BOOL
    IsWindowBitmap(
        void
        ) const
    {
        return FALSE;
    }

    virtual
    BOOL
    IsCompositionBitmap(
        void
        ) const
    {
        return FALSE;
    }

    virtual
    LONG
    XStride(
        void
        ) const;

    // DibSize returns number of bytes used by this bitmap's image
    virtual
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
    HBITMAP hbm,
    LONG* pnWidth,
    LONG* pnHeight
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
protected:

    MBITMAP(
        GdiObjectHandle_t GdiObjectHandle
        );

public:
    SurfObjWrapper_t m_so;
    void*   m_pUserBits;
    POINT   m_ptOrigin;

    static
    MBITMAP*
    New(
        void
        );


    virtual
    ~MBITMAP(
        void
        );


    void
    ReleaseResources(
        void
        );

    BOOL
    Create_AllocSetup(
        UINT32 nBitsPP,
        INT32 nWidth,
        INT32 nHeight
        );


    static
    MBITMAP*
    GET_pMBITMAP(
        HBITMAP hbitmap
        );


    BOOL
    Create(
        UINT32   nBitsPP,
        INT32   nWidth,
        INT32   nHeight,
        ColorTable_t* pcolortbl // Get refcounted by MBITMAP
        );

    BOOL
    Create(
        UINT32  nBitsPP,
        INT32  nWidth,
        INT32  nHeight,
        Driver_t* pdrv
        );

    BOOL
    Create(
        int Width,
        int Height
        );

    inline
    int
    BitmapDataSize(
        void
        )
    {
        return abs(Stride() * Height());
    }
};



/*
    @class CBITMAP | Composited/Shared Memory Bitmap object

    @access Public Members

    @cmember void | CBITMAP | New() | Static creator.
    @cmember void | ~CBITMAP | () | Destructor.

    @cmember BOOL | Create | (WBITMAP * pWbitmap, HHGSURF hHgSurf) |
        Required initialization function.

    @comm The purpose of this class is to help manage the overall
    reference count for the underlying memory that belongs to its
    owner handle.

*/
class CBITMAP : public MBITMAP
{
private:
    HWND    m_hwnd;
    HHGSURF m_hHgSurf;

    // These fields are necessary for ddraw compat
    // and mirror what DDBITMAP is doing.  We can't
    // derive from DDBITMAP, though.
    DWORD   m_dwSurfaceSize;
    LONG    m_lXStride;


protected:
    CBITMAP(
        GdiObjectHandle_t GdiObjectHandle
        );

public:
    static
    CBITMAP*
    New(
        void
        );

    static
    CBITMAP*
    GET_pCBITMAP(
        HBITMAP hbitmap
        );

    BOOL
    Create(
        WBITMAP *       pWbitmap,
        HHGSURF         hHgSurf,
        SURFOBJEX *     psoex
        );

    ~CBITMAP(
        void
        );

// BitmapBase_t overrides
    BOOL
    IsCompositionBitmap(
        void
        ) const
    {
        return TRUE;
    }

    LONG    
    XStride(
        void
        ) const;

    DWORD
    DibSize(
        void
        );

// CBITMAP members
    HHGSURF GetSharedSurface() const
    {
        return m_hHgSurf;
    }

    HWND    GetHwnd() const
    {
        return m_hwnd;
    }
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
    static  WBITMAP*        s_pwbmSelectSysClipRgnLatest;
    static  HRGN            s_hrgnSelectSysClipRgnLatest;
    static HRGN             s_hrgnNull;

    RECT                    m_rWindow;
    HRGN                    m_hrgnSysClip;

    void*
    operator new(
        size_t
        );

    WBITMAP(
        GdiObjectHandle_t   GdiObjectHandle,
        Driver_t*           pdriver,
        HWND                hwnd
        );

public:
    DC*                     m_pdcList;     // Used in dcbase.cpp
    MBITMAP*                m_pMBitmap;
    HWND                    m_hwnd;

    void
    ExplicitCleanup(
        void
        );

    struct RGNDAT*
    GetSysClipRgn(
        void
        );

    int
    CombineSysClipRgn(
        HRGN hrgnNew
        );

    void
    put_WindowRect( LONG left, LONG right, LONG top, LONG bottom )
        {
        m_rWindow.left = left;
        m_rWindow.right = right;
        m_rWindow.top = top;
        m_rWindow.bottom = bottom;
        }

    LONG
    WindowLeft( void )
        {
        if ( m_pMBitmap )
            {
            return m_rWindow.left - m_pMBitmap->m_ptOrigin.x ;
            }
        return m_rWindow.left ;
        }

    LONG
    WindowTop( void )
        {
        if ( m_pMBitmap )
            {
            return m_rWindow.top - m_pMBitmap->m_ptOrigin.y ;
            }
        return m_rWindow.top ;
        }

    LONG
    WindowRight( void )
        {
        if ( m_pMBitmap )
            {
            return m_rWindow.right - m_pMBitmap->m_ptOrigin.x ;
            }
        return m_rWindow.right ;
        }

    LONG
    WindowBottom( void )
        {
        if ( m_pMBitmap )
            {
            return m_rWindow.bottom - m_pMBitmap->m_ptOrigin.y ;
            }
        return m_rWindow.bottom ;
        }

    LONG
    WindowWidth( void )
        {
        return m_rWindow.right - m_rWindow.left ;
        }

    LONG
    WindowHeight( void )
        {
        return m_rWindow.bottom - m_rWindow.top ;
        }

    static
    WBITMAP*
    New(
        Driver_t*,
        HWND
        );

    void
    operator delete(
        void*
        );


    virtual
    ~WBITMAP(
        void
        );

    void
    SetViewport(
        const RECT&
        );


    BOOL
    SelectSysClipRgn(
        HRGN
        );

    MBITMAP *
    SelectMbitmap(
        MBITMAP *
        );

    virtual
    BOOL
    IsWindowBitmap(
        void
        ) const
    {
        return TRUE;
    }

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


    virtual
    BOOL
    UpdateClipForResizeScreen();


    static
    void
    CheckSysClipRgn(
        GdiObjectHandle_t GdiObjectHandle
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
        const LOGPEN*
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
        const LOGPEN*
        );

    static
    Pen_t*
    GET_pPEN(
        HPEN hpen
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

    virtual
    ~Pen_t() { }
};


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

    LOGBRUSH m_LogBrush;
    MBITMAP* m_pBitmap;  // bitmap containing pattern

    static
    Brush_t*
    New(
        void
        );

    void
    operator delete(
        void*
        );



    virtual ~Brush_t(
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
        const LOGBRUSH*
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


class FontOverrides_t
{
private:
    static int   s_NativeAngle;          // This is the orientation of the LCD.
                                         // for which the driver supports ClearType.
    static int   s_CurrentAngle;         // This is used when ClearType is turned on or off.
    static bool  s_AllowAntialiased;     // Is the Antialiased override regkey set?
    static bool  s_AllowClearType;       // Is the ClearType override regkey set?
    static bool  s_OffOnRotation;        // Do we turn ClearType off on Rotation?

    static bool  s_UseAntialiasedFonts;  // Should SelectObject use Antialiased fonts?
    static bool  s_UseClearTypeFonts;    // Should SelectObject use ClearType fonts?
    static DWORD s_ClearTypeQuality;     // What quality should SelectObject use for
                                         // for ClearType.

    static
    void
    UpdateOverrides();

public:
    static
    void
    Initialize();

    static
    bool
    Rotate(int Angle);

    static
    BOOL
    SetFontSmoothing(BOOL bSmoothing);

    static
    bool
    UseAntialiasedFonts() { return s_UseAntialiasedFonts; }

    static
    bool
    UseClearTypeFonts() { return s_UseClearTypeFonts; }

    static
    DWORD
    ClearTypeQuality() { return s_ClearTypeQuality; }
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
        const LOGFONTW*
        );

public:
    LOGFONTWITHATOM m_LogFont;

    // TrueType font realizations (RFONTOBJ); unused in the raster font engine.
    void* m_prfont;           // Monochrome realization
    void* m_prfontAA;         // Anti-Aliased version of this realization
    void* m_prfontCT;
    UINT32 m_uEffxDisplay;     // Display-time font effects, eg, Bold, underline, strikeout


    static
    Font_t*
    New(
        const LOGFONTW*
        );

    void
    operator delete(
        void*
        );


    virtual
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

    ColorTable_t* m_pcolortbl;

    void*
    operator new(
        size_t
        );

    Palette_t(
        GdiObjectHandle_t GdiObjectHandle
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



    virtual
    ~Palette_t(
        void
        );

    // If constructor succeeds (ppalette, handle are not NULL),
    // FInitPal() has to be called to init newly created object
    BOOL
    FInitPalette(
        const PALETTEENTRY* ppe,
                ULONG   cLogPalEntries,
                BOOL   fStock
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

    // Getting/Setting palette entries, all params are expected to be validated by the caller.
    void
    GetEntries(
        UINT32   uiStart,
        UINT32   uiNum,
        PALETTEENTRY* pPe
        );

    void
    SetEntries(
                UINT32   uiStart,
                UINT32   uiNum,
        const PALETTEENTRY* pPe
        );


    ULONG
    xxxSetSysPal(
        Driver_t* pdriver
        );


#ifdef DEBUG
    virtual
    int
    Dump(
        void
        )
    {
        int cThis = GweMemory_t::Size(this);
        DEBUGMSG (1, (TEXT("PALETTE size=%d\r\n"), cThis));
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
// GdiMonitor_t maps a Driver_t object to a HMONITOR.
//

class GdiMonitor_t : public GDIOBJ
{
private:
    Driver_t * m_pDriver;

    GdiMonitor_t(
        GdiObjectHandle_t GdiObjectHandle,
        Driver_t*      pDriver
        );

public:
    static
    GdiMonitor_t *
    New(
        Driver_t* pDriver
        );

    virtual
    ~GdiMonitor_t(
        void
        );

    Driver_t *
    GetDriver(
        void
        ) const;

    HMONITOR
    GetHMONITOR(
        void
        ) const;

    static
    GdiMonitor_t*
    GET_pMONITOR(
        HMONITOR
        );

    virtual
    int
    GetObject(
        int    CntBytesBuffer,
        void * pObject
        );

    virtual
    GDIOBJ*
    SelectObject(
        DC*
        );

    virtual
    BOOL
    DeleteObject(
        void
        );

#if DEBUG
    virtual
    int
    Dump(
        void
        );
#endif
};


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
        pdc = DC::GET_pDC(hdc);                 \
        if ( !pdc )                             \
        {                                       \
            ASSERT(0);                          \
            SetLastError(ERROR_INVALID_HANDLE); \
            return(ret);                        \
        }                                       \
    } while(0,0)




#define IS_RGND(P)          ((P)->rdh.nCount < 0x10000)

ULONG   iBmfFromBpp(INT32 iBpp);
INT32   iBppFromBmf(ULONG iBitmapFormat);
BOOL    FValidBMI(CONST BITMAPINFO *pbmpi, UINT iUsage, BOOL AllowRotation);
BOOL    FValidBMI_DIB(CONST BITMAPINFO *pbmpi, UINT iUsage, BOOL AllowRotation);
BOOL    CopyBMI(CONST BITMAPINFO * pbmi, BITMAPINFO * * ppbmiOut, UINT iUsage, BOOL bPackedDIB);

int InitializeGdiObjConstructors(void);

template < typename T >
inline T GetSumNoOverflow( T op1, T op2, T boundary )
{
    // this function is meant to be used with a signed limit boundary
    ASSERT( ( ( static_cast<T>(boundary + 1) ) == ( static_cast<T>(-boundary - 1) ) ) );

    // sign check
    if( ( ( op1 ^ op2 ) >= 0 ) )
        {
        // either two negatives, or 2 positives
        if( op2 < 0 )
            {
            // two negatives
            if( op1 < ( ( boundary + 1 ) - op2 ) )
                {
                // return min, do not underflow
                return -boundary - 1;
                }
            // ok ...
            }
        else
            {
            // two positives
            if( ( boundary - op1 ) < op2 )
                {
                // return max, do not overflow
                return boundary;
                }
            // ok ...
            }
        }
    return op1 + op2;
}

void
dib_setup(
    SurfObjWrapper_t*,
    UINT32*,
    ULONG,
    INT32,
    INT32
    );


#endif  // _GDIOBJ_H_
