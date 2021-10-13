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
#include <Engine.h>

class FONTRESOURCE;
class XPATHOBJ;
class Driver_t;



class BrushObjExtended_t : public BRUSHOBJ
{

public:

    DC*    m_pdc;

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



struct ForwardedDc_t
{
    HDC     m_Handle;
    DC*     m_pDc;

    HDC     m_OriginalHandle;
    DC*     m_pOriginalDc;

    BOOL    m_fHasBitmap;

    ForwardedDc_t(
        void
        );
    
};


ForwardedDc_t
ValidateAndForwardHdc(
    HDC     hdc,
    DCOP    dcop
    );


class DC : public GDIOBJ
{
private:

    void*
    operator new(
        size_t
        );

    DC(
        GdiObjectHandle_t   GdiObjectHandle,
        HDC                 hdcRef,
        GdiObjectType_t     DcType,
        WBITMAP*            pwbitmap,
        BOOL                bDcEx,
        UINT32              grfExStyle
        );

    DC(
        GdiObjectHandle_t   GdiObjectHandle,
        Driver_t*           pdriver,
        GdiObjectType_t     DcType,
        WBITMAP*            pwbitmap
        );

    DC(
        GdiObjectHandle_t   GdiObjectHandle,
        MBITMAP*            pMemoryBitmap
        );



    void
    FinishConstruction(
        Driver_t*       pdriver,
        GdiObjectType_t DcType,
        WBITMAP*        pwbitmap,
        MBITMAP*        pMemoryBitmap,
        BOOL            bRTLDc,
        BOOL            bRTLReading
        );

    static
    BOOL
    InternalCopyDCNoClip(
        DC*    pdcDst,
        DC*    pdcSrc
        );



    Pen_t*          m_pPen;

public:

    Brush_t*        m_pBrush;
    BitmapBase_t*   m_pBitmap;
    Font_t*         m_pFont;
    RGNDAT*         m_prgndAppClip;
    COLORREF        m_crBackground;
    COLORREF        m_crForeground;
    POINTL          m_ptAppBrushOrg;
    Palette_t*      m_pPalette;
    POINTL          m_ptViewportOrg;
    RECT            m_ptWindowOffset1;

    // realized attributes
    BrushObjExtended_t    m_xboBrush;
    POINTL                m_ptBrushOrg;
    BrushObjExtended_t    m_xboPen;


    FONTRESOURCE*   m_pFontResource;
    UINT32          m_uiBackground;
    UINT32          m_uiForeground;
    POINTL          m_ptCurrentPos;
    UINT32          m_ulTextAlign;
    int             m_nCharExtra;
    
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
    XCLIPOBJ        m_co;             // contains RGNDAT with intersection of app's clip w/ bitmap or screen
    Driver_t*       m_pdriver;      // Pointer to all info about this DC's driver

    //    Sleazy data packing tricks start here.
    BYTE            m_iBkMode;
    BYTE            m_ROP2;
    /*BOOL*/ BYTE   m_bPrint:7;   // TRUE if printer DC.
    /*BOOL*/ BYTE   m_bRTL:1;     // TRUE if Right To Left DC.
    

    /*BOOL*/ BYTE   m_fFree_prgndClip : 1;  // Free the above clip rgn when done?
    /*BOOL*/ BYTE   m_fDcEx : 1;            // Dc was created by a GetDCEx call.

    /*PRTSTAT*/ BYTE  m_prtstat : 3;  // Print job status
    BYTE              m_iStretchMode : 3;   // The mode used by StretchBlt.
    // Sleazy data packing tricks end here.

    BOOL              m_bDisabledState; // Used in cooperative level processing.
                                        // A DC could be "disabled" if it points
                                        // to the primary of a device, and another
                                        // process takes fullscreen exclusive
                                        // for the device.

                                        // When this is set to TRUE,
                                        // any drawing calls to/from this will HDC
                                        // will silently fail. (No drawing, no
                                        // error code.)

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

    inline
    int
    Width(
        void
        )
    {
        if ( m_dwDcType == OBJ_DC )
            {
            return (((WBITMAP *)m_pBitmap)->WindowWidth());
            }
        return m_pBitmap->Width();
    }

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
        HDC              hdcRef,
        GdiObjectType_t  DcType,
        WBITMAP*         pwbitmap,
        BOOL             bDcEx,
        UINT32           grfExStyle
        );

    static
    DC*
    New(
        HDC                hdcRef,
        GdiObjectType_t    DcType
        );



    static
    DC*
    New(
        Driver_t*        pdriver,
        GdiObjectType_t  DcType,
        WBITMAP*         pwbitmap
        );


    static
    DC*
    New(
        MBITMAP*    pMemoryBitmap
        );


    static
    DC*
    GET_pDC(
        HDC    hdc
        );

    HDC
    GetHDC(
        void
        ) const;



public:

    virtual ~DC();

    //
    // REVIEW Vladimir/Martin: Which of these methods should be const?
    //
    BOOL            UpdateClipping(BOOL);
    int             ModifyClipRect (int, int, int, int, BOOL);
    int             UpdateClipForResizeScreen();
    int             SetAppClipRgn (RGNDAT *);

    const DRVENABLEDATA*
    pDrvGet(
        void
        ) const;

    virtual int     GetObject(int,void*);


    static
    int
    SaveDC(
        HDC    hdc
        );

    static
    BOOL
    RestoreDC(
        HDC    hdc,
        int    nSavedDC
        );



    HGDIOBJ
    GetCurrentObject(
        UINT    uObjectType
        );


    virtual GDIOBJ*    SelectObject(DC*);


    BitmapBase_t*
    SelectBitmap(
        BitmapBase_t*    pNewBitmap
        );


    Pen_t*
    SelectPen(
        Pen_t*    pNewPen
        );


    Palette_t*
    SelectPalette(
        Palette_t*    pNewPalette
        );


    Brush_t*
    SelectBrush(
        Brush_t*    pNewBrush
        );



    static
    BOOL
    IsCurrentPenWide(
        HDC    hdc
        );


    static
    BOOL
    Rectangle(
        HDC     hdc,
        int     nLeftRect,
        int     nTopRect,
        int     nRightRect,
        int     nBottomRect
        );

    static
    BOOL
    Ellipse(
        HDC    hdc,
        int    nLeftRect,
        int    nTopRect,
        int    nRightRect,
        int    nBottomRect
        );


    static
    BOOL
    RoundRect(
        HDC    hdc,
        int    iLeftRect,
        int    iTopRect,
        int    iRightRect,
        int    iBottomRect,
        int    iCornerWidth,
        int    iCornerHeight
        );



    BOOL
    bDrawEllipseOrRR(
        EPOINTFIX aeptfx[],
        UINT32    cPts,
        BOOL      bRTL
        );


    static
    BOOL
    Polygon(
                HDC        hdc,
        const    POINT*    lpPointsUnmapped,
                int        nCount
        );


    static
    BOOL
    PolyLine(
                HDC        hdc,
        const   POINT*     pptVertsUnmapped,
                int        iNumVerts
        );




    UINT32          GetMix(void) { return (((UINT32)m_ROP2 << 8) | (UINT32)m_ROP2); }
    DWORD
    dwRealizeColor(
        COLORREF    clrf,
        COLORREF*   pclrfReal = 0,
        BOOL        fIgnoreError = 1
        ) const;


    COLORREF        clrfUnrealizeColor(DWORD);

    POINTL          LogicalToDevice(const POINTL &pt);
    RECTL           LogicalToDevice(const RECTL& rc);
    RECT            LogicalToDevice(const RECT& rc);
    void            LogicalToDevice(XPATHOBJ *ppo);
    BOOL            GetDCOrg(POINTL *);
    void            SetDCOrg(long x, long y);
    // Return TRUE iff the DC's bitmap can be written to
    BOOL            FWritableBitmap();
    void            BackBufferToLogical(RECT *lprc);
    void            MirrorRect(RECT* lprc);
#if DEBUG
    virtual int     Dump (void);
#endif


    PFN_DrvAlphaBlend
    DrvAlphaBlendFuncPtr(
        void
        ) const;


    SurfObjWrapper_t*
    DrvCreateDeviceBitmap(
        const    SIZEL&    sizel
        );


    BOOL
    StrokePath(
        XPATHOBJ*    pXpathObj
        );




};



#endif

