//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <windows.h>
#include "commctrl.h"
#include "gwe_s.h"
#include "ImageList.hpp"
#include <gdi.hpp>
#include <StockObjectHandles.hpp>
#include <window.hpp>
#include <manager.hpp>
#include <table.h>
#include <memory.h>
#include <LoadImg.hpp>
#include <icon.h>

#include <gwe.hpp>
#include <GweMemory.hpp>


// a global table of imagelists
GENTBL* v_ptblImageLists;
extern "C" DWORD const HalftoneColorPalette[];

static const unsigned int IMAGELIST_SIG = 0x53545857;

#ifndef ILC_COLORMASK
static const unsigned int ILC_COLORMASK = 0x00FE;
static const unsigned int ILD_BLENDMASK = 0x000E;
#endif

static const unsigned int CLR_WHITE = 0x00FFFFFF;
static const unsigned int CLR_BLACK = 0x00000000;

static const unsigned int NOTSRCAND = 0x00220326;
static const unsigned int ROP_PSo = 0x00FC008A;
static const unsigned int ROP_DPo = 0x00FA0089;
static const unsigned int ROP_DPna = 0x000A0329;
static const unsigned int ROP_DPSona = 0x00020c89;
static const unsigned int ROP_SDPSanax = 0x00E61ce8;
static const unsigned int ROP_DSna = 0x00220326;
static const unsigned int ROP_PSDPxax = 0x00b8074a;

static const unsigned int ROP_PatNotMask = 0x00b8074a;      // D <- S==0 ? P : D
static const unsigned int ROP_PatMask = 0x00E20746;      // D <- S==1 ? P : D
static const unsigned int ROP_MaskPat = 0x00AC0744;      // D <- P==1 ? D : S

static const unsigned int ROP_DSo = 0x00EE0086;
static const unsigned int ROP_DSno = 0x00BB0226;
static const unsigned int ROP_DSa = 0x008800C6;

static const unsigned int ILC_WIN95 =  (ILC_MASK | ILC_COLORMASK | ILC_SHARED | ILC_PALETTE);

#define IS_WHITE_PIXEL(pj,x,y,cScan) \
    ((pj)[((y) * (cScan)) + ((x) >> 3)] & (1 << (7 - ((x) & 7))))

#define BITS_ALL_WHITE(b) (b == 0xff)

struct DIBMONO_t {
    BITMAPINFOHEADER bmiHeader;
    DWORD bmiColors[2];
    DWORD bmBits[8];
};

struct DIBINFO_t {
    BITMAPINFOHEADER bmiHeader;
    DWORD bmiColors[2];
} DIBINFO;

// statics
HDC ImageList::s_hdcSrc;
HBITMAP ImageList::s_hbmSrc;
HBITMAP ImageList::s_hbmDcDeselect;
HDC ImageList::s_hdcDst;
HBITMAP ImageList::s_hbmDst;
int ImageList::s_iILRefCount;
HBITMAP ImageList::s_hbmWork;                   // work buffer.
BITMAP ImageList::s_bmWork;                           // work buffer size
HBRUSH ImageList::s_hbrMonoDither;              // gray dither brush for dragging
HBRUSH ImageList::s_hbrStripe;
int ImageList::s_iDither = 0;

DRAGCONTEXT_t ImageList::s_DragContext = {
    (ImageList*)NULL,
    (ImageList*)NULL,
    (ImageList*)NULL,
    -1,
    {0, 0},
    {0, 0},
    {0, 0},
    false,
    false,
    (HWND)NULL
};

DRAGRESTOREBMP_t ImageList::s_DragRestoreBmp = {
    0,
    NULL,
    NULL,
    {-1,-1}
};

ImageList*
ImageList::
GetDragImage(
    POINT *ppt,
    POINT *pptHotspot
    )
{
    if (ppt)
    {
        ppt->x = ImageList::s_DragContext.ptDrag.x;
        ppt->y = ImageList::s_DragContext.ptDrag.y;
    }
    if (pptHotspot)
    {
        pptHotspot->x = ImageList::s_DragContext.ptDragHotspot.x;
        pptHotspot->y = ImageList::s_DragContext.ptDragHotspot.y;
    }
    return s_DragContext.pimlDrag;
}

BOOL
ImageList::
GetIconSize(
    ImageList *piml,
    int  *cx,
    int  *cy
    )
{
    if (!piml->IsImageList())
    {
        ASSERT(0);
        return FALSE;
    }

    if (!cx || !cy)
    {
        return FALSE;
    }

    *cx = piml->m_cx;
    *cy = piml->m_cy;
    return TRUE;
}

BOOL
ImageList::
SetDragImage(
    ImageList* piml,
    int i,
    int dxHotspot,
    int dyHotspot
    )
{
    bool bVisible = ImageList::s_DragContext.bDragShow;
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    EnterUserMaybe();
    if (bVisible)
    {
        piml->DragShowNolock(FALSE);
    }
    // only do this last step if everything is there.
    bRet = piml->MergeDragImages(dxHotspot, dyHotspot);

    if (bVisible)
    {
        piml->DragShowNolock(TRUE);
    }
    LeaveUserMaybe();
Exit:
    return bRet;
}


//
//  change the size of a existing image list
//  also removes all items
//
BOOL
ImageList::
SetIconSize(
    ImageList *piml,
    int cx,
    int cy
    )
{
    BOOL bRet = FALSE;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    if (piml->m_cx == cx && piml->m_cy == cy)
    {
        goto Exit;       // no change
    }

    piml->m_cx = cx;
    piml->m_cy = cy;

    ImageList::Remove(piml, -1);
    bRet = TRUE;
Exit:
    return bRet;
}

BOOL
ImageList::
GetImageInfo(
    ImageList* piml,
    int i,
    IMAGEINFO *pImageInfo
    )
{
    BOOL bRet = FALSE;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    if (!pImageInfo || !(i >= 0 && i < piml->m_cImage))
    {
        goto Exit;
    }
    pImageInfo->hbmImage      = piml->m_hbmImage;
    pImageInfo->hbmMask       = piml->m_hbmMask;

    bRet = piml->GetImageRect(i, &pImageInfo->rcImage);

Exit:
    return bRet;
}


ImageList*
ImageList::
Merge(
    ImageList* piml1,
    int i1,
    ImageList* piml2,
    int i2,
    int dx,
    int dy
    )
{
    ImageList* pimlNew = NULL;
    RECT rcNew;
    RECT rc1;
    RECT rc2;
    int cx, cy;
    int c1, c2;
    UINT wFlags;
    UserTakenHereFlag;

    if (!piml1->IsImageList() || !piml2->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    EnterUserMaybe();

    SetRect(&rc1, 0, 0, piml1->m_cx, piml1->m_cy);
    SetRect(&rc2, dx, dy, piml2->m_cx + dx, piml2->m_cy + dy);
    UnionRect(&rcNew, &rc1, &rc2);

    cx = rcNew.right - rcNew.left;
    cy = rcNew.bottom - rcNew.top;

    //
    // If one of images are shared, create a shared image.
    //
    wFlags = (piml1->m_flags | piml2->m_flags) & ~ILC_COLORMASK;

    c1 = (piml1->m_flags & ILC_COLORMASK);
    c2 = (piml2->m_flags & ILC_COLORMASK);

    if ((c1 == 16 || c1 == 32) && c2 == ILC_COLORDDB)
    {
        c2 = c1;
    }

    wFlags |= max(c1, c2);

    pimlNew = ImageList::Create(cx, cy, ILC_MASK|wFlags, 1, 0);
    if (pimlNew)
    {
        pimlNew->m_cImage++;

        if (pimlNew->m_hdcMask)
        {
            Gdi::PatBlt_I(pimlNew->m_hdcMask,  0, 0, cx, cy, WHITENESS);
        }
        Gdi::PatBlt_I(pimlNew->m_hdcImage, 0, 0, cx, cy, BLACKNESS);

        pimlNew->Merge2(piml1, i1, rc1.left - rcNew.left, rc1.top - rcNew.top);
        pimlNew->Merge2(piml2, i2, rc2.left - rcNew.left, rc2.top - rcNew.top);
    }

    LeaveUserMaybe();

Exit:
    return pimlNew;
}


/*--------------------------------------------------------------------
** make a dithered copy of the source image in the destination image.
** allows placing of the final image in the destination.
**--------------------------------------------------------------------*/
void
ImageList::
CopyDitherImage(
    ImageList *pimlDst,
    WORD iDst,
    int xDst,
    int yDst,
    ImageList *pimlSrc,
    int iSrc,
    UINT fStyle
    )
{
    RECT rc;
    int x;
    int y;

    if (!pimlDst->IsImageList() || !pimlSrc->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    pimlDst->GetImageRect(iDst, &rc);

    // coordinates in destination image list
    x = xDst + rc.left;
    y = yDst + rc.top;

    fStyle &= ILD_OVERLAYMASK;
    ImageList::DrawEx(pimlSrc, iSrc, pimlDst->m_hdcImage, x, y, 0, 0, CLR_DEFAULT, CLR_NONE, ILD_IMAGE | fStyle);
    if (pimlDst->m_hdcMask)
    {
        ImageList::DrawEx(pimlSrc, iSrc, pimlDst->m_hdcMask,  x, y, 0, 0, CLR_NONE, CLR_NONE, ILD_BLEND50|ILD_MASK | fStyle);
    }
    pimlDst->ResetBkColor(iDst, iDst+1, pimlDst->m_clrBk);
Exit:
    return;
}

/*
** Draw the image, either selected, transparent, or just a blt
**
** For the selected case, a new highlighted image is generated
** and used for the final output.
**
**      piml    ImageList to get image from.
**      i       the image to get.
**      hdc     DC to draw image to
**      x,y     where to draw image (upper left corner)
**      cx,cy   size of image to draw (0,0 means normal size)
**
**      rgbBk   background color
**              CLR_NONE            - draw tansparent
**              CLR_DEFAULT         - use bk color of the image list
**
**      rgbFg   foreground (blend) color (only used if ILD_BLENDMASK set)
**              CLR_NONE            - blend with destination (transparent)
**              CLR_DEFAULT         - use windows hilight color
**
**  if blend
**      if blend with color
**          copy image, and blend it with color.
**      else if blend with dst
**          copy image, copy mask, blend mask 50%
##
**  if ILD_TRANSPARENT
**      draw transparent (two blts) special case black or white background
**      unless we copied the mask or image
**  else if (rgbBk == piml->rgbBk && fSolidBk)
**      just blt it
**  else if mask
**      copy image
**      replace bk color
**      blt it.
**  else
**      just blt it
*/

BOOL
ImageList::
DrawIndirect(
    IMAGELISTDRAWPARAMS* pimldp
    )
{
    RECT rcImage;
    RECT rc;
    HBRUSH  hbrT;
    BOOL bRet = FALSE;
    BOOL fImage;
    HDC hdcMask;
    HDC hdcImage;
    int xMask, yMask;
    int xImage, yImage;
    ImageList& ImageListRef = *((ImageList*)pimldp->himl);
    UserTakenHereFlag;
    int     cxDest, cyDest, xDest, yDest;

    if (!ImageListRef.IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    if (pimldp->cbSize != sizeof(IMAGELISTDRAWPARAMS))
    {
        goto Exit;
    }

    if (pimldp->i < 0 || pimldp->i >= ImageListRef.m_cImage)
    {
        goto Exit;
    }

    EnterUserMaybe();

    ImageListRef.GetImageRect(pimldp->i, &rcImage);
    rcImage.left += pimldp->xBitmap;
    rcImage.top += pimldp->yBitmap;

    if (pimldp->rgbBk == CLR_DEFAULT)
    {
        pimldp->rgbBk = ImageListRef.m_clrBk;
    }

    if (pimldp->rgbBk == CLR_NONE)
    {
        pimldp->fStyle |= ILD_TRANSPARENT;
    }

    if (pimldp->cx == 0)
    {
        pimldp->cx = rcImage.right  - rcImage.left;
    }

    if (pimldp->cy == 0)
    {
        pimldp->cy = rcImage.bottom - rcImage.top;
    }

Again:
    if ((pimldp->fStyle & ILD_DPISCALE))
        {
        // Adjust the output dimensions based on the screen DPI.
        // Assumes: image is designed for 96dpi.
        cxDest = Gwe::ComputeHorizontalDpiAwareValue(pimldp->cx); //((pimldp->cx * g_PixelsPerHorizontalInch) + 48) / 96;
        cyDest = Gwe::ComputeVerticalDpiAwareValue(pimldp->cy);   //((pimldp->cy * g_PixelsPerVerticalInch) + 48) / 96;
        }
    else
        {
        cxDest = pimldp->cx;
        cyDest = pimldp->cy;
        }
    xDest = pimldp->x;
    yDest = pimldp->y;

    hdcMask = ImageListRef.m_hdcMask;
    xMask = rcImage.left;
    yMask = rcImage.top;

    hdcImage = ImageListRef.m_hdcImage;
    xImage = rcImage.left;
    yImage = rcImage.top;

    if (pimldp->fStyle & ILD_BLENDMASK)
    {
        // make a copy of the image, because we will have to modify it
        hdcImage = ImageList::GetWorkDC(pimldp->hdcDst, pimldp->cx, pimldp->cy);
        xImage = 0;
        yImage = 0;

        //
        //  blend with the destination
        //  by "oring" the mask with a 50% dither mask
        //
        if (pimldp->rgbFg == CLR_NONE && hdcMask)
        {
            if ((ImageListRef.m_flags & ILC_COLORMASK) == ILC_COLOR16 && !(pimldp->fStyle & ILD_MASK))
            {
                // copy dest to our work buffer
                Gdi::BitBlt_I(hdcImage, 0, 0, pimldp->cx, pimldp->cy, pimldp->hdcDst, pimldp->x, pimldp->y, SRCCOPY);

                // blend source into our work buffer
                ImageListRef.Blend16(hdcImage, 0, 0, pimldp->i, pimldp->cx, pimldp->cy, pimldp->rgbFg, pimldp->fStyle);
                pimldp->fStyle |= ILD_TRANSPARENT;
            }
            else
            {
                ImageListRef.GetSpareImageRect( &rc);
                xMask = rc.left;
                yMask = rc.top;

                // copy the source image
                Gdi::BitBlt_I(hdcImage, 0, 0, pimldp->cx, pimldp->cy,
                       ImageListRef.m_hdcImage, rcImage.left, rcImage.top, SRCCOPY);

                // make a dithered copy of the mask
                hbrT = (HBRUSH)Gdi::SelectObject_I(hdcMask, ImageList::s_hbrMonoDither);
                Gdi::BitBlt_I(hdcMask, rc.left, rc.top, pimldp->cx, pimldp->cy,
                       hdcMask, rcImage.left, rcImage.top, ROP_PSo);
                Gdi::SelectObject_I(hdcMask, hbrT);

                pimldp->fStyle |= ILD_TRANSPARENT;
            }
        }
        else
        {
            // blend source into our work buffer
            ImageListRef.Blend(
                    hdcImage,
                    0,
                    0,
                    pimldp->i,
                    pimldp->cx,
                    pimldp->cy,
                    pimldp->rgbFg,
                    pimldp->fStyle
                    );
        }
    }

    // is the source image from the image list (not hdcWork)
    fImage = hdcImage == ImageListRef.m_hdcImage;

    //
    // ILD_MASK means draw the mask only
    //
    if ((pimldp->fStyle & ILD_MASK) && hdcMask)
    {
        DWORD dwRop;

        if (pimldp->fStyle & ILD_ROP)
        {
            dwRop = pimldp->dwRop;
        }
        else if (pimldp->fStyle & ILD_TRANSPARENT)
        {
            dwRop = SRCAND;
        }
        else
        {
            dwRop = SRCCOPY;
        }

        Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcMask, xMask, yMask, pimldp->cx, pimldp->cy, dwRop);
    }
    else if (pimldp->fStyle & ILD_IMAGE)
    {
        COLORREF clrBk = Gdi::GetBkColor_I(hdcImage);
        DWORD dwRop;

        if (pimldp->rgbBk != CLR_DEFAULT)
        {
            Gdi::SetBkColor_I(hdcImage, pimldp->rgbBk);
        }

        if (pimldp->fStyle & ILD_ROP)
        {
            dwRop = pimldp->dwRop;
        }
        else
        {
            dwRop = SRCCOPY;
        }
        Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, dwRop);
        Gdi::SetBkColor_I(hdcImage, clrBk);
    }
    //
    // if there is a mask and the drawing is to be transparent,
    // use the mask for the drawing.
    //
    else if ((pimldp->fStyle & ILD_TRANSPARENT) && hdcMask)
    {
//
// on NT just call MaskBlt
//
#ifdef USE_MASKBLT
        if (!(pimldp->fStyle & ILD_DPISCALE))
        {
            HBITMAP hbmMask;
            hbmMask = (HBITMAP)Gdi::GetCurrentObject_I(hdcMask, OBJ_BITMAP);
            Gdi::MaskBlt_I(pimldp->hdcDst, pimldp->x, pimldp->y, pimldp->cx,
                pimldp->cy, hdcImage, xImage, yImage, hbmMask, xMask, yMask, 0xCCAA0000);
        }
        else
#else  // USE_MASKBLT
        {
        COLORREF clrTextSave;
        COLORREF clrBkSave;

        //
        //  we have some special cases:
        //
        //  if the background color is black, we just do a AND then OR
        //  if the background color is white, we just do a OR then AND
        //  otherwise change source, then AND then OR
        //

        clrTextSave = Gdi::SetTextColor_I(pimldp->hdcDst, CLR_BLACK);
        clrBkSave = Gdi::SetBkColor_I(pimldp->hdcDst, CLR_WHITE);

        // we cant do white/black special cases if we munged the mask or image

        if (fImage && ImageListRef.m_clrBk == CLR_WHITE)
        {
            Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcMask, xMask, yMask, pimldp->cx, pimldp->cy, ROP_DSno);
            Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, ROP_DSa);
        }
        else if (fImage && (ImageListRef.m_clrBk == CLR_BLACK || ImageListRef.m_clrBk == CLR_NONE))
        {
            Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcMask,  xMask, yMask, pimldp->cx, pimldp->cy, ROP_DSa);
            Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, ROP_DSo);
        }
        else
        {
            ASSERT(Gdi::GetTextColor_I(hdcImage) == CLR_BLACK);
            ASSERT(Gdi::GetBkColor_I(hdcImage) == CLR_WHITE);

            // black out the source image.
            Gdi::BitBlt_I(hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, hdcMask, xMask, yMask, ROP_DSna);

            Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcMask,  xMask, yMask, pimldp->cx, pimldp->cy, ROP_DSa);
            Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, ROP_DSo);

            // restore the bkcolor, if it came from the image list
            if (fImage)
            {
                ImageListRef.ResetBkColor(pimldp->i, pimldp->i, ImageListRef.m_clrBk);
            }
        }

        Gdi::SetTextColor_I(pimldp->hdcDst, clrTextSave);
        Gdi::SetBkColor_I(pimldp->hdcDst, clrBkSave);
        }
#endif // !USE_MASKBLT
    }
    else if (fImage && pimldp->rgbBk == ImageListRef.m_clrBk && ImageListRef.m_bSolidBk)
    {
        Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, SRCCOPY);
    }
    else if (hdcMask)
    {
        if (fImage && 
            ((pimldp->rgbBk == ImageListRef.m_clrBk && !ImageListRef.m_bSolidBk) || 
            Gdi::GetNearestColor_I(hdcImage, pimldp->rgbBk) != pimldp->rgbBk))
        {
            // make a copy of the image, because we will have to modify it
            hdcImage = ImageList::GetWorkDC(pimldp->hdcDst, pimldp->cx, pimldp->cy);
            xImage = 0;
            yImage = 0;
            fImage = FALSE;

            Gdi::BitBlt_I(hdcImage, 0, 0, pimldp->cx, pimldp->cy, ImageListRef.m_hdcImage, rcImage.left, rcImage.top, SRCCOPY);
        }

        Gdi::SetBrushOrgEx_I(hdcImage, xImage - pimldp->x, yImage - pimldp->y, NULL);
        hbrT = (HBRUSH)Gdi::SelectObject_I(hdcImage, Gdi::CreateSolidBrush_I(pimldp->rgbBk));
        Gdi::BitBlt_I(hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, hdcMask, xMask, yMask, ROP_PatMask);
        Gdi::DeleteObject_I(Gdi::SelectObject_I(hdcImage, hbrT));
        Gdi::SetBrushOrgEx_I(hdcImage, 0, 0, NULL);

        Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, SRCCOPY);

        if (fImage)
        {
            ImageListRef.ResetBkColor(pimldp->i, pimldp->i, ImageListRef.m_clrBk);
        }
    }
    else
    {
        Gdi::StretchBlt_I(pimldp->hdcDst, xDest, yDest, cxDest, cyDest, hdcImage, xImage, yImage, pimldp->cx, pimldp->cy, SRCCOPY);
    }

    //
    // now deal with a overlay image, use the minimal bounding rect (and flags)
    // we computed in ImageList_SetOverlayImage()
    //
    if (pimldp->fStyle & ILD_OVERLAYMASK)
    {
        int n = OVERLAYMASKTOINDEX(pimldp->fStyle);

        if (n < NUM_OVERLAY_IMAGES)
        {
            pimldp->i = ImageListRef.m_aOverlayIndexes[n];
            ImageListRef.GetImageRect(pimldp->i, &rcImage);

            pimldp->cx = ImageListRef.m_aOverlayDX[n];
            pimldp->cy = ImageListRef.m_aOverlayDY[n];
            pimldp->x += ImageListRef.m_aOverlayX[n];
            pimldp->y += ImageListRef.m_aOverlayY[n];
            rcImage.left += ImageListRef.m_aOverlayX[n]+pimldp->xBitmap;
            rcImage.top  += ImageListRef.m_aOverlayY[n]+pimldp->yBitmap;

            pimldp->fStyle &= ILD_MASK;
            pimldp->fStyle |= ILD_TRANSPARENT;
            pimldp->fStyle |= ImageListRef.m_aOverlayF[n];

            if (pimldp->cx > 0 && pimldp->cy > 0) // WINCE_REVIEW v1 port not carried over.
            {
                goto Again;  // ImageList_DrawEx(piml, i, hdcDst, x, y, 0, 0, CLR_DEFAULT, CLR_NONE, fStyle);
            }
        }
    }

    if (!fImage)
    {
        ImageList::ReleaseWorkDC(hdcImage);
    }

    LeaveUserMaybe();
    bRet = TRUE;
Exit:
    return bRet;
}


BOOL
ImageList::
DragShowNolock(
    BOOL fShow
    )
{
    HDC hdcScreen;
    int x;
    int y;
    UserTakenHereFlag;

    x = s_DragContext.ptDrag.x - s_DragContext.ptDragHotspot.x;
    y = s_DragContext.ptDrag.y - s_DragContext.ptDragHotspot.y;

    if (!s_DragContext.pimlDrag)
    {
        return FALSE;
    }

    //
    // REVIEW: Why this block is in the critical section? We are supposed
    //  to have only one dragging at a time, aren't we?
    //
    EnterUserMaybe();
    if (fShow && !s_DragContext.bDragShow)
    {
        hdcScreen = CWindow::GetDC_I(s_DragContext.hwndDC);

        ImageList::SelectSrcBitmap(s_DragRestoreBmp.hbmRestore);

        Gdi::BitBlt_I(
                s_hdcSrc,
                0,
                0,
                s_DragRestoreBmp.sizeRestore.cx,
                s_DragRestoreBmp.sizeRestore.cy,
                hdcScreen,
                x,
                y,
                SRCCOPY
                );

        UINT fStyle = PixelDoublePoint(NULL,NULL) ? ILD_DPISCALE : ILD_NORMAL;
        if (s_DragContext.bHiColor)
        {
            ImageList::DrawEx(s_DragContext.pimlDrag, 0, hdcScreen, x, y, 0, 0, CLR_NONE, CLR_NONE, fStyle | ILD_BLEND50);

            if (s_DragContext.pimlCursor)
            {
                ImageList::Draw(s_DragContext.pimlCursor,
                                s_DragContext.iCursor,
                                hdcScreen,
                                x + s_DragContext.ptCursor.x,
                                y + s_DragContext.ptCursor.y,
                                fStyle);
            }
        }
        else
        {
            ImageList::Draw(s_DragContext.pimlDrag, 0, hdcScreen, x, y, fStyle);
        }

        CWindow::ReleaseDC_I(s_DragContext.hwndDC, hdcScreen);
    }
    else if (!fShow && s_DragContext.bDragShow)
    {
        hdcScreen = CWindow::GetDC_I(s_DragContext.hwndDC);

        ImageList::SelectSrcBitmap(s_DragRestoreBmp.hbmRestore);

        Gdi::BitBlt_I(
                hdcScreen,
                x,
                y,
                s_DragRestoreBmp.sizeRestore.cx,
                s_DragRestoreBmp.sizeRestore.cy,
                s_hdcSrc,
                0,
                0,
                SRCCOPY
                );

        CWindow::ReleaseDC_I(s_DragContext.hwndDC, hdcScreen);
    }

    s_DragContext.bDragShow = !!fShow;
    LeaveUserMaybe();

    return TRUE;
}


ImageList*
ImageList::
Create(
    int cx,
    int cy,
    UINT flags,
    int cInitial,
    int cGrow
    )
{
    ImageList* piml = NULL;
    UserTakenHereFlag;

    if (cx <= 0 || cy <= 0)
    {
        goto Exit;
    }

    EnterUserMaybe();
    if (!s_iILRefCount)
    {
        if (!ImageList::Init())
        {
            goto Error;
        }
    }

    s_iILRefCount++;

    if (flags == (DWORD)-1)
    {
        // possibly
        // passed -1 for this....
        // map this to all win95 flags
        flags = ILC_WIN95;
    }

    piml = ImageList::Create2(cx, cy, flags, cGrow);

    // allocate the bitmap PLUS one re-usable entry
    if (piml)
    {
        if (flags & ILC_VIRTUAL)
        {
            piml->m_hdcImage = (HDC)-1;
            piml->m_hbmImage = (HBITMAP)-1;

            if (piml->m_flags & ILC_MASK)
            {
                piml->m_hdcMask = (HDC)-1;
                piml->m_hbmMask = (HBITMAP)-1;
            }
        }
        else
        {
            // make the hdc's
            piml->m_hdcImage = Gdi::CreateCompatibleDC_I(NULL);

            if (piml->m_hdcImage == NULL)
            {
                goto Error;
            }

            if (flags & ILC_MIRROR)
            {
                Gdi::SetLayout_I(piml->m_hdcImage, LAYOUT_RTL);
            }

            if (piml->m_flags & ILC_MASK)
            {
                piml->m_hdcMask = Gdi::CreateCompatibleDC_I(NULL);

                // were they both made ok?
                if (!piml->m_hdcMask)
                {
                    goto Error;
                }
            }

            if (flags & ILC_MIRROR)
            {
                Gdi::SetLayout_I(piml->m_hdcMask, LAYOUT_RTL);
            }
 
            if (!piml->ReAllocBitmaps(cInitial + 1) &&
                !piml->ReAllocBitmaps(1))
            {
                goto Error;
            }

            // Add this image list to our list
            piml->m_hprc = GetCallerProcess();
            TblAdd(v_ptblImageLists, piml);
        }
    }

LeaveCritical:
    LeaveUserMaybe();
Exit:
    return piml;

Error:
    if (piml)
    {
        ImageList::Destroy(piml);
        piml = NULL;
    }
    goto LeaveCritical;
}

BOOL
ImageList::
Destroy(
    ImageList* piml
    )
{
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    EnterUserMaybe();
    bRet = (BOOL)TblLookup(v_ptblImageLists, 0, 0, piml, TBL_BYPOINTER|TBL_DELETE, NULL);
    LeaveUserMaybe();

Exit:
    return bRet;
}

int
ImageList::
GetImageCount(
    ImageList* piml
    )
{
    if (!piml->IsImageList())
    {
        ASSERT(0);
        return 0;
    }
    return piml->m_cImage;
}

int
ImageList::
Add(
    ImageList* piml,
    HBITMAP hbmImage,
    HBITMAP hbmMask
    )
{
    BITMAP bm;
    int cImage;

    ASSERT(piml && piml->m_cx);
    ASSERT(hbmImage);

    if (!piml || Gdi::GetObjectW_I(hbmImage, sizeof(bm), &bm) != sizeof(bm) || bm.bmWidth < piml->m_cx)
    {
        return -1;
    }

    cImage = bm.bmWidth / piml->m_cx;     // # of images in source

    // serialization handled within Add2.
    return(piml->Add2(hbmImage, hbmMask, cImage, 0, 0));
}


//
// Parameter:
//  i -- -1 to add
//
int
ImageList::
ReplaceIcon(
    ImageList* piml,
    int i,
    HICON hIcon
    )
{
    RECT rc;
    int RetVal = -1;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    // be win95 compatible
    if (i < -1)
    {
        goto Exit;
    }


    if (hIcon == NULL)
    {
        goto Exit;
    }

    //
    //  alocate a slot for the icon
    //
    if (i == -1)
    {
        i = piml->Add2(NULL,NULL,1,0,0);
    }

    if (i == -1)
    {
        goto Exit;
    }
    //
    //  now draw it into the image bitmaps
    //
    if (!piml->GetImageRect(i, &rc))
    {
        goto Exit;
    }

    Gdi::FillRect_I(piml->m_hdcImage, &rc, piml->m_hbrBk);
    DrawIconEx_I(piml->m_hdcImage, rc.left, rc.top, hIcon, 0, 0, 0, NULL, DI_NORMAL);

    if (piml->m_hdcMask)
    {
        DrawIconEx_I(piml->m_hdcMask, rc.left, rc.top, hIcon, 0, 0, 0, NULL, DI_MASK);
    }

    RetVal = i;
Exit:
    return RetVal;

}

COLORREF
ImageList::
GetBkColor(
    ImageList* piml
    )
{

    if (!piml->IsImageList())
    {
        ASSERT(0);
        return CLR_NONE;
    }

    return piml->m_clrBk;
}

COLORREF
ImageList::
SetBkColor(
    ImageList* piml,
    COLORREF clrBk
    )
{
    COLORREF clrBkOld;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        clrBkOld = CLR_NONE;
        goto Exit;
    }

    // Quick out if there is no change in color
    if (piml->m_clrBk == clrBk)
    {
        clrBkOld = clrBk;
        goto Exit;
    }

    // The following code deletes the brush, resets the background color etc.,
    // so, protect it with a critical section.
    EnterUserMaybe();

    if (piml->m_hbrBk)
    {
        Gdi::DeleteObject_I(piml->m_hbrBk);
    }

    clrBkOld = piml->m_clrBk;
    piml->m_clrBk = clrBk;

    if ((clrBk == CLR_NONE) || (piml->m_flags & ILC_VIRTUAL))
    {
        piml->m_hbrBk = StockObjectHandles::BlackBrush();
        piml->m_bSolidBk = true;
    }
    else
    {
        piml->m_hbrBk = Gdi::CreateSolidBrush_I(clrBk);
        piml->m_bSolidBk = (Gdi::GetNearestColor_I(piml->m_hdcImage, clrBk) == clrBk);
    }

    ASSERT(piml->m_hbrBk);

    if (piml->m_cImage > 0)
    {
        piml->ResetBkColor(0, piml->m_cImage - 1, clrBk);
    }

    LeaveUserMaybe();

Exit:
    return clrBkOld;
}

// Set the image iImage as one of the special images for us in combine
// drawing.  to draw with these specify the index of this
// in:
//      piml    imagelist
//      iImage  image index to use in speical drawing
//      iOverlay        index of special image, values 1-4

BOOL
ImageList::
SetOverlayImage(
    ImageList* piml,
    int iImage,
    int iOverlay
    )
{
    RECT rcImage;
    RECT rc;
    int x,y;
    int cx,cy;
    ULONG cScan;
    ULONG cBits;
    HBITMAP hbmMem;
    BOOL bRet = FALSE;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    iOverlay--;         // make zero based
    if (piml->m_hdcMask == NULL ||
        iImage < 0 || iImage >= piml->m_cImage ||
        iOverlay < 0 || iOverlay >= NUM_OVERLAY_IMAGES)
    {
        goto Exit;
    }

    if (piml->m_aOverlayIndexes[iOverlay] == (SHORT)iImage)
    {
        bRet = TRUE;
        goto Exit;
    }

    piml->m_aOverlayIndexes[iOverlay] = (SHORT)iImage;

    //
    // find minimal rect that bounds the image
    //
    piml->GetImageRect(iImage, &rcImage);
    SetRect(&rc, 0x7FFF, 0x7FFF, 0, 0);

    //
    // now compute the black box.  This is much faster than GetPixel but
    // could still be improved by doing more operations looking at entire
    // bytes.  We basicaly get the bits in monochrome form and then use
    // a private GetPixel.  This decreased time on NT from 50 milliseconds to
    // 1 millisecond for a 32X32 image.
    //
    cx = rcImage.right  - rcImage.left;
    cy = rcImage.bottom - rcImage.top;

    // compute the number of bytes in a scan.  Note that they are dword aligned
    cScan  = (cx + 31) / 32;
    cBits  = cScan * cy;

    DIBINFO_t dibinfo;
    PBYTE pBits;

    dibinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dibinfo.bmiHeader.biWidth = cx;
    dibinfo.bmiHeader.biHeight = cy;
    dibinfo.bmiHeader.biPlanes = 1;
    dibinfo.bmiHeader.biBitCount = 1;
    dibinfo.bmiHeader.biCompression = BI_RGB;
    dibinfo.bmiHeader.biSizeImage = 0;
    dibinfo.bmiHeader.biXPelsPerMeter = 0;
    dibinfo.bmiHeader.biYPelsPerMeter = 0;
    dibinfo.bmiHeader.biClrUsed = 2;
    dibinfo.bmiHeader.biClrImportant = 2;
    dibinfo.bmiColors[0] = RGB(0, 0, 0);
    dibinfo.bmiColors[1] = RGB(0xFF, 0xFF, 0xFF);

    hbmMem = CreateDIBSection(piml->m_hdcMask,
                              (LPBITMAPINFO)&dibinfo,
                              DIB_RGB_COLORS,
                              (void **)&pBits,
                              NULL,
                              0);

    if (hbmMem)
    {
        HDC hdcMem = Gdi::CreateCompatibleDC_I(piml->m_hdcMask);

        if (hdcMem)
        {
            PBYTE pScan;

            if (pBits)
            {
                Gdi::SelectObject_I(hdcMem, hbmMem);

                //
                // map black pixels to 0, white to 1
                //
                Gdi::BitBlt_I(hdcMem, 0, 0, cx, cy, piml->m_hdcMask, rcImage.left, rcImage.top, SRCCOPY);
                //
                // for each scan, find the bounds
                //
                for (y = 0, pScan = pBits; y < cy; ++y,pScan += cScan)
                {
                    int i;

                    //
                    // first go byte by byte through white space
                    //
                    for (x = 0, i = 0; (i < (cx >> 3)) && BITS_ALL_WHITE(pScan[i]); ++i)
                    {
                        x += 8;
                    }

                    //
                    // now finish the scan bit by bit
                    //
                    for (; x < cx; ++x)
                    {
                        if (!IS_WHITE_PIXEL(pBits, x,y,cScan))
                        {
                            rc.left   = min(rc.left, x);
                            rc.right  = max(rc.right, x+1);
                            rc.top    = min(rc.top, y);
                            rc.bottom = max(rc.bottom, y+1);

                            // now that we found one, quickly jump to the known right edge

                            if ((x >= rc.left) && (x < rc.right))
                            {
                                x = rc.right-1;
                            }
                        }
                    }
                }

                if (rc.left == 0x7FFF)
                {
                    rc.left = 0;
                    ASSERT(0);
                }

                if (rc.top == 0x7FFF)
                {
                    rc.top = 0;
                    ASSERT(0);
                }

                piml->m_aOverlayDX[iOverlay] = (SHORT)(rc.right - rc.left);
                piml->m_aOverlayDY[iOverlay] = (SHORT)(rc.bottom- rc.top);
                piml->m_aOverlayX[iOverlay]  = (SHORT)(rc.left);
                piml->m_aOverlayY[iOverlay]  = (SHORT)(rc.top);
                piml->m_aOverlayF[iOverlay]  = 0;

                //
                // see if the image is non-rectanglar
                //
                // if the overlay does not require a mask to be drawn set the
                // ILD_IMAGE flag, this causes ImageList_DrawEx to just
                // draw the image, ignoring the mask.
                //
                for (y=rc.top; y<rc.bottom; y++)
                {
                    for (x=rc.left; x<rc.right; x++)
                    {
                        if (IS_WHITE_PIXEL(pBits, x, y,cScan))
                        {
                            break;
                        }
                    }

                    if (x != rc.right)
                    {
                        break;
                    }
                }

                if (y == rc.bottom)
                {
                    piml->m_aOverlayF[iOverlay] = ILD_IMAGE;
                }

                bRet = TRUE;
            }

            Gdi::DeleteDC_I(hdcMem);
        }

        Gdi::DeleteObject_I(hbmMem);
    }
Exit:
    return bRet;
}


BOOL
ImageList::
Draw(
    ImageList* piml,
    int i,
    HDC hdcDst,
    int x,
    int y,
    UINT fStyle
    )
{
    IMAGELISTDRAWPARAMS imldp;

    imldp.cbSize = sizeof(imldp);
    imldp.himl   = reinterpret_cast<HIMAGELIST>(piml);
    imldp.i      = i;
    imldp.hdcDst = hdcDst;
    imldp.x      = x;
    imldp.y      = y;
    imldp.cx     = 0;
    imldp.cy     = 0;
    imldp.xBitmap= 0;
    imldp.yBitmap= 0;
    imldp.rgbBk  = CLR_DEFAULT;
    imldp.rgbFg  = CLR_DEFAULT;
    imldp.fStyle = fStyle;

    return ImageList::DrawIndirect(&imldp);
}

BOOL
ImageList::
Replace(
    ImageList* piml,
    int i,
    HBITMAP hbmImage,
    HBITMAP hbmMask
    )
{
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    if (i < 0 || i >= piml->m_cImage)
    {
        goto Exit;
    }

    EnterUserMaybe();
    bRet = piml->Replace2(i, 1, hbmImage, hbmMask, 0, 0);
    LeaveUserMaybe();
Exit:
    return bRet;
}


int
ImageList::
AddMasked(
    ImageList* piml,
    HBITMAP hbmImage,
    COLORREF crMask
    )
{
    COLORREF crbO, crtO;
    HBITMAP hbmMask;
    int cImage;
    int RetVal = -1;
    int n=0, i;
    BITMAP bm;
    BOOL fUseColorTable = FALSE;
    DWORD* pColorTableSave = NULL;
    DWORD* pColorTable = NULL;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    if (Gdi::GetObjectW_I(hbmImage, sizeof(bm), &bm) != sizeof(bm))
    {
        goto Exit;
    }

    if (bm.bmBits != NULL && bm.bmBitsPixel <= 8)
    {
        pColorTable = GweMemory_t::NewArray<DWORD>(256);
        if (!pColorTable)
        {
            goto Exit;
        }

        pColorTableSave = GweMemory_t::NewArray<DWORD>(256);
        if(!pColorTableSave)
        {
            goto Exit;
        }
        fUseColorTable = TRUE;
    }

    hbmMask = ImageList::CreateMonoBitmap(bm.bmWidth, bm.bmHeight);
    if (!hbmMask)
    {
        goto Exit;
    }

    EnterUserMaybe();

    // copy color to mono, with crMask turning 1 and all others 0, then
    // punch all crMask pixels in color to 0
    ImageList::SelectSrcBitmap(hbmImage);
    ImageList::SelectDstBitmap(hbmMask);

    // crMask == CLR_DEFAULT, means use the pixel in the upper left
    //
    if (crMask == CLR_DEFAULT)
    {
        crMask = Gdi::GetPixel_I(s_hdcSrc, 0, 0);
    }

    // DIBSections dont do color->mono like DDBs do, so we have to do it.
    // this only works for <=8bpp DIBSections, this method does not work
    // for HiColor DIBSections.
    //
    // This code is a workaround for a problem in Win32 when a DIB is converted to 
    // monochrome. The conversion is done according to closeness to white or black
    // and without regard to the background color. This workaround is is not required 
    // under MainWin. 
    //
    // Please note, this code has an endianship problems the comparision in the if statement
    // below is sensitive to endianship
    // ----> if (ColorTableSave[i] == RGB(GetBValue(crMask),GetGValue(crMask),GetRValue(crMask))
    //
    if (fUseColorTable)
    {
        n = Gdi::GetDIBColorTable_I(s_hdcSrc, 0, 256, (RGBQUAD*)pColorTableSave);

        for (i=0; i<n; i++)
        {
            if (pColorTableSave[i] == RGB(GetBValue(crMask),GetGValue(crMask),GetRValue(crMask)))
            {
                pColorTable[i] = 0x00FFFFFF;
            }
            else
            {
                pColorTable[i] = 0x00000000;
            }
        }

        Gdi::SetDIBColorTable_I(s_hdcSrc, 0, n, (RGBQUAD*)pColorTable);
    }

    crbO = Gdi::SetBkColor_I(s_hdcSrc, crMask);
    Gdi::BitBlt_I(s_hdcDst, 0, 0, bm.bmWidth, bm.bmHeight, s_hdcSrc, 0, 0, SRCCOPY);
    Gdi::SetBkColor_I(s_hdcSrc, 0x00FFFFFFL);
    crtO = Gdi::SetTextColor_I(s_hdcSrc, 0x00L);
    Gdi::BitBlt_I(s_hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight, s_hdcDst, 0, 0, ROP_DSna);
    Gdi::SetBkColor_I(s_hdcSrc, crbO);
    Gdi::SetTextColor_I(s_hdcSrc, crtO);

    if (fUseColorTable)
    {
        Gdi::SetDIBColorTable_I(s_hdcSrc, 0, n, (RGBQUAD*)pColorTableSave);
    }

    ImageList::SelectSrcBitmap(NULL);
    ImageList::SelectDstBitmap(NULL);

    ASSERT(piml->m_cx);
    cImage = bm.bmWidth / piml->m_cx;   // # of images in source

    RetVal = piml->Add2(hbmImage, hbmMask, cImage, 0, 0);

    Gdi::DeleteObject_I(hbmMask);

    LeaveUserMaybe();

Exit:
    if (pColorTable)
    {
        GweMemory_t::Delete(pColorTable);
    }

    if (pColorTableSave)
    {
        GweMemory_t::Delete(pColorTableSave);
    }
    return RetVal;
}

BOOL
ImageList::
DrawEx(
    ImageList* piml,
    int i,
    HDC hdcDst,
    int x,
    int y,
    int cx,
    int cy,
    COLORREF rgbBk,
    COLORREF rgbFg,
    UINT fStyle
    )
{
    IMAGELISTDRAWPARAMS imldp;

    imldp.cbSize = sizeof(imldp);
    imldp.himl   = reinterpret_cast<HIMAGELIST>(piml);
    imldp.i      = i;
    imldp.hdcDst = hdcDst;
    imldp.x      = x;
    imldp.y      = y;
    imldp.cx     = cx;
    imldp.cy     = cy;
    imldp.xBitmap= 0;
    imldp.yBitmap= 0;
    imldp.rgbBk  = rgbBk;
    imldp.rgbFg  = rgbFg;
    imldp.fStyle = fStyle;

    return ImageList::DrawIndirect(&imldp);

}

//
//  ImageList_Remove - remove a image from the image list
//
//  i - image to remove, or -1 to remove all images.
//
//  NOTE all images are "shifted" down, ie all image index's
//  above the one deleted are changed by 1
//
BOOL
ImageList::
Remove(
    ImageList* piml,
    int i
    )
{
    BOOL bRet = TRUE;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        bRet = FALSE;
        goto Exit;
    }

    EnterUserMaybe();

    if (i == -1)
    {
        piml->m_cImage = 0;
        piml->m_cAlloc = 0;

        for (i=0; i<NUM_OVERLAY_IMAGES; i++)
        {
            piml->m_aOverlayIndexes[i] = -1;
        }

        piml->ReAllocBitmaps(-piml->m_cGrow);
    }
    else
    {
        if (i < 0 || i >= piml->m_cImage)
        {
            bRet = FALSE;
        }
        else
        {
            piml->RemoveItemBitmap(i);

            --piml->m_cImage;

            if (piml->m_cAlloc - (piml->m_cImage + 1) > piml->m_cGrow)
            {
                piml->ReAllocBitmaps(piml->m_cAlloc - piml->m_cGrow);
            }
        }
    }
    LeaveUserMaybe();

Exit:
    return bRet;
}

HICON
ImageList::
GetIcon(
    ImageList* piml,
    int i,
    UINT flags
    )
{
    UINT cx, cy;
    HICON hIcon = NULL;
    HBITMAP hbmMask, hbmColor;
    ICONINFO ii;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Error1;
    }

    if (i < 0 || i >= piml->m_cImage)
    {
        goto Error1;
    }

    cx = piml->m_cx;
    cy = piml->m_cy;

    hbmColor = ImageList::CreateColorBitmap(cx,cy);
    if (!hbmColor)
    {
        goto Error1;
    }
    hbmMask  = ImageList::CreateMonoBitmap(cx,cy);
    if (!hbmMask)
    {
        goto Error2;
    }

    EnterUserMaybe();
    ImageList::SelectDstBitmap(hbmMask);
    Gdi::PatBlt_I(s_hdcDst, 0, 0, cx, cy, WHITENESS);
    ImageList::Draw(piml, i, s_hdcDst, 0, 0, ILD_MASK | flags);

    ImageList::SelectDstBitmap(hbmColor);
    Gdi::PatBlt_I(s_hdcDst, 0, 0, cx, cy, BLACKNESS);
    ImageList::Draw(piml, i, s_hdcDst, 0, 0, ILD_TRANSPARENT | flags);

    ImageList::SelectDstBitmap(NULL);
    LeaveUserMaybe();

    ii.fIcon    = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmColor = hbmColor;
    ii.hbmMask  = hbmMask;
    hIcon = CreateIconIndirect_I(&ii);
    Gdi::DeleteObject_I(hbmMask);
Error2:
    Gdi::DeleteObject_I(hbmColor);
Error1:
    return hIcon;
}


ImageList*
ImageList::
LoadImage(
    HINSTANCE hi,
    LPCTSTR lpbmp,
    int cx,
    int cGrow,
    COLORREF crMask,
    UINT uType,
    UINT uFlags
    )
{
    HBITMAP hbmImage;
    ImageList* piml = NULL;
    BITMAP bm;
    int cy, cInitial;
    UINT flags;
    UserTakenHereFlag;

    hbmImage = (HBITMAP)LoadImageW_I(hi, lpbmp, uType, 0, 0, uFlags);
    if (!hbmImage || (sizeof(bm) != Gdi::GetObjectW_I(hbmImage, sizeof(bm), &bm)))
    {
        goto Exit;
    }

    // If cx is not stated assume it is the same as cy.
    // Assert(cx);
    cy = bm.bmHeight;

    if (cx == 0)
    {
        cx = cy;
    }

    cInitial = bm.bmWidth / cx;

    EnterUserMaybe();

    flags = 0;
    if (crMask != CLR_NONE)
    {
        flags |= ILC_MASK;
    }

    if (bm.bmBits)
    {
        flags |= (bm.bmBitsPixel & ILC_COLORMASK);
    }

    piml = ImageList::Create(cx, cy, flags, cInitial, cGrow);
    if (piml)
    {
        int added;

        if (crMask == CLR_NONE)
        {
            added = ImageList::Add(piml, hbmImage, NULL);
        }
        else
        {
            added = ImageList::AddMasked(piml, hbmImage, crMask);
        }

        if (added < 0)
        {
            ImageList::Destroy(piml);
            piml = NULL;
        }
    }
    LeaveUserMaybe();

Exit:
    if (hbmImage)
    {
        Gdi::DeleteObject_I(hbmImage);
    }

    return piml;
}



BOOL
ImageList::
BeginDrag(
    ImageList* pimlTrack,
    int iTrack,
    int dxHotspot,
    int dyHotspot
    )
{
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    if(!pimlTrack->IsImageList())
    {
        goto Exit;
    }

    ImageList::PixelDoublePoint(&dxHotspot, &dyHotspot);

    EnterUserMaybe();

    if (!s_DragContext.pimlDrag)
    {
        UINT newflags = pimlTrack->m_flags | ILC_SHARED;
        int iSreenDepth =ImageList::GetScreenDepth();
        
        s_DragContext.bDragShow = false;
        s_DragContext.hwndDC = NULL;
        s_DragContext.bHiColor = (iSreenDepth > 8);

        // for backwards compatibility
        if ((pimlTrack->m_flags & ILC_COLORMASK) == 0 ||
            (pimlTrack->m_flags & ILC_COLORMASK) == ILC_COLORDDB)
        {
            s_DragContext.bHiColor = false;
        }

        if (s_DragContext.bHiColor)
        {
            UINT uColorFlag = ILC_COLOR16;
            if (iSreenDepth == 32 || iSreenDepth == 24)
            {
                uColorFlag = ILC_COLOR32;
            }

            newflags = (newflags & ~ILC_COLORMASK) | uColorFlag;
        }

        /*
        ** make a copy of the drag image
        */
        s_DragContext.pimlDither = ImageList::Create(
                    pimlTrack->m_cx,
                    pimlTrack->m_cy,
                    newflags,
                    1,
                    0);

        if (s_DragContext.pimlDither)
        {
            s_DragContext.pimlDither->m_cImage++;
            s_DragContext.ptDragHotspot.x = dxHotspot;
            s_DragContext.ptDragHotspot.y = dyHotspot;

            s_DragContext.pimlDither->CopyOneImage(0, 0, 0, pimlTrack, iTrack);
            bRet = ImageList::SetDragImage(s_DragContext.pimlDither, 0, dxHotspot, dyHotspot);
        }
    }
    LeaveUserMaybe();

Exit:
    return bRet;
}

void
ImageList::
EndDrag()
{
    ImageList* piml = s_DragContext.pimlDrag;
    UserTakenHereFlag;

    EnterUserMaybe();
    if (piml->IsImageList())
    {
        ImageList::DragShowNolock(FALSE);

    // WARNING: Don't destroy pimlDrag if it is pimlDither.
        if (s_DragContext.pimlDrag && (s_DragContext.pimlDrag != s_DragContext.pimlDither))
        {
            ImageList::Destroy(s_DragContext.pimlDrag);
        }

        if (s_DragContext.pimlDither)
        {
            ImageList::Destroy(s_DragContext.pimlDither);
            s_DragContext.pimlDither = NULL;
        }

        s_DragContext.pimlCursor = NULL;
        s_DragContext.iCursor = -1;
        s_DragContext.pimlDrag = NULL;
        s_DragContext.hwndDC = NULL;
    }
    LeaveUserMaybe();
}

BOOL
ImageList::
DragEnter(
    HWND hwndLock,
    int x,
    int y
    )
{
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    // Let a NULL hwndLock represent the entire screen instead of the desktop
    // hwndLock = hwndLock ? hwndLock : GetDesktopWindow();

    ImageList::PixelDoublePoint(&x, &y);

    EnterUserMaybe();
    if (!s_DragContext.hwndDC)
    {
        s_DragContext.hwndDC = hwndLock;

        s_DragContext.ptDrag.x = x;
        s_DragContext.ptDrag.y = y;

        ImageList::DragShowNolock(TRUE);
        bRet = TRUE;
    }
    LeaveUserMaybe();

    return bRet;
}

BOOL
ImageList::
DragLeave(
    HWND hwndLock
    )
{
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    // Let a NULL hwndLock represent the entire screen instead of the desktop
    // hwndLock = hwndLock ? hwndLock : GetDesktopWindow();

    EnterUserMaybe();
    if (s_DragContext.hwndDC == hwndLock)
    {
        ImageList::DragShowNolock(FALSE);
        s_DragContext.hwndDC = NULL;
        bRet = TRUE;
    }
    LeaveUserMaybe();

    return bRet;
}

//
//  x, y     -- Specifies the initial cursor position in the coords of hwndLock,
//      which is specified by the previous ImageList_StartDrag call.
//
BOOL
ImageList::
DragMove(
    int x,
    int y
    )
{
    UserTakenHereFlag;

    EnterUserMaybe();

    ImageList::PixelDoublePoint(&x, &y);

    if (s_DragContext.bDragShow)
    {
        RECT rcOld, rcNew, rcBounds;
        int dx, dy;

        dx = x - s_DragContext.ptDrag.x;
        dy = y - s_DragContext.ptDrag.y;
        rcOld.left = s_DragContext.ptDrag.x - s_DragContext.ptDragHotspot.x;
        rcOld.top = s_DragContext.ptDrag.y - s_DragContext.ptDragHotspot.y;
        rcOld.right = rcOld.left + s_DragRestoreBmp.sizeRestore.cx;
        rcOld.bottom = rcOld.top + s_DragRestoreBmp.sizeRestore.cy;
        rcNew = rcOld;
        OffsetRect(&rcNew, dx, dy);

        if (!IntersectRect(&rcBounds, &rcOld, &rcNew))
        {
            //
            // No intersection. Simply hide the old one and show the new one.
            //
            ImageList::DragShowNolock(FALSE);
            s_DragContext.ptDrag.x = x;
            s_DragContext.ptDrag.y = y;
            ImageList::DragShowNolock(TRUE);
        }
        else
        {
            //
            // Some intersection.
            //
            HDC hdcScreen;
            int cx, cy;
            DWORD dwLayout;

            UnionRect(&rcBounds, &rcOld, &rcNew);

            hdcScreen = CWindow::GetDC_I(s_DragContext.hwndDC);
            if(hdcScreen)
            {
                //
                // Keep our DC's layout in sync.
                //
                dwLayout = Gdi::GetLayout_I(hdcScreen);
                if (dwLayout)
                {
                    Gdi::SetLayout_I(s_hdcSrc, dwLayout);
                    Gdi::SetLayout_I(s_hdcDst, dwLayout);
                }
                
                cx = rcBounds.right - rcBounds.left;
                cy = rcBounds.bottom - rcBounds.top;

                //
                // Copy the union rect from the screen to hbmOffScreen.
                //
                ImageList::SelectDstBitmap(s_DragRestoreBmp.hbmOffScreen);
                Gdi::BitBlt_I(s_hdcDst,
                                0,
                                0,
                                cx,
                                cy,
                                hdcScreen,
                                rcBounds.left,
                                rcBounds.top,
                                SRCCOPY);

                //
                // Hide the cursor on the hbmOffScreen by copying hbmRestore.
                //
                ImageList::SelectSrcBitmap(s_DragRestoreBmp.hbmRestore);
                Gdi::BitBlt_I(s_hdcDst,
                              rcOld.left - rcBounds.left,
                              rcOld.top - rcBounds.top,
                              s_DragRestoreBmp.sizeRestore.cx,
                              s_DragRestoreBmp.sizeRestore.cy,
                              s_hdcSrc,
                              0,
                              0,
                              SRCCOPY
                              );

                //
                // Copy the original screen bits to hbmRestore
                //
                Gdi::BitBlt_I(s_hdcSrc,
                              0,
                              0,
                              s_DragRestoreBmp.sizeRestore.cx,
                              s_DragRestoreBmp.sizeRestore.cy,
                              s_hdcDst,
                              rcNew.left - rcBounds.left,
                              rcNew.top - rcBounds.top,
                              SRCCOPY
                              );

                //
                // Draw the image on hbmOffScreen
                //
                UINT fStyle = PixelDoublePoint(NULL,NULL) ? ILD_DPISCALE : ILD_NORMAL;
                if (s_DragContext.bHiColor)
                {
                    ImageList::DrawEx(s_DragContext.pimlDrag,
                                    0,
                                    s_hdcDst,
                                    rcNew.left - rcBounds.left,
                                    rcNew.top - rcBounds.top,
                                    0,
                                    0,
                                    CLR_NONE,
                                    CLR_NONE,
                                    fStyle | ILD_BLEND50);
                    if (s_DragContext.pimlCursor)
                    {
                        ImageList::Draw(s_DragContext.pimlCursor,
                                        s_DragContext.iCursor,
                                        s_hdcDst,
                                        rcNew.left - rcBounds.left + s_DragContext.ptCursor.x,
                                        rcNew.top - rcBounds.top + s_DragContext.ptCursor.y,
                                        fStyle);
                    }
                }
                else
                {
                    ImageList::Draw(s_DragContext.pimlDrag,
                                    0,
                                    s_hdcDst,
                                    rcNew.left - rcBounds.left,
                                    rcNew.top - rcBounds.top,
                                    fStyle);
                }

                //
                // Copy the hbmOffScreen back to the screen.
                //
                Gdi::BitBlt_I(hdcScreen,
                                rcBounds.left,
                                rcBounds.top,
                                cx,
                                cy,
                                s_hdcDst,
                                0,
                                0,
                                SRCCOPY);

                //
                // Restore the DC's Layout, and it is for sure zero since
                // these DCs are compatible to the desktop DC which is  
                // always has LTR layout i.e. zero.
                //
                if (dwLayout)
                {
                    Gdi::SetLayout_I(s_hdcSrc, 0);
                    Gdi::SetLayout_I(s_hdcDst, 0);
                }
                CWindow::ReleaseDC_I(s_DragContext.hwndDC, hdcScreen);
            }
            s_DragContext.ptDrag.x = x;
            s_DragContext.ptDrag.y = y;
        }
    }
    LeaveUserMaybe();
    return TRUE;
}


BOOL
ImageList::
SetDragCursorImage(
    ImageList * piml,
    int i,
    int dxHotspot,
    int dyHotspot
    )
{
    bool bVisible = s_DragContext.bDragShow;
    BOOL bRet = FALSE;
    UserTakenHereFlag;


    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    bRet = TRUE;

    EnterUserMaybe();
    // do work only if something has changed
    if ((s_DragContext.pimlCursor != piml) || (s_DragContext.iCursor != i))
    {

        if (bVisible)
        {
            ImageList::DragShowNolock(FALSE);
        }

        s_DragContext.pimlCursor = piml;
        s_DragContext.iCursor = i;
        s_DragContext.ptCursor.x = dxHotspot;
        s_DragContext.ptCursor.y = dyHotspot;

        bRet = ImageList::MergeDragImages(dxHotspot, dyHotspot);

        if (bVisible)
        {
            ImageList::DragShowNolock(TRUE);
        }
    }
    LeaveUserMaybe();

Exit:
    return bRet;
}


//
//  ImageList_Copy - move an image in the image list
//
BOOL
ImageList::
Copy(
    ImageList* pimlDst,
    int iDst,
    ImageList* pimlSrc,
    int iSrc,
    UINT uFlags
    )
{
    RECT rcDst, rcSrc, rcTmp;
    ImageList *pimlTmp;
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    if (uFlags & ~ILCF_VALID)
    {
        // Flags must be valid
        ASSERT(FALSE);
        goto Exit;
    }

    if(!pimlSrc->IsImageList())
    {
        goto Exit;
    }

    // Not supported
    if (pimlDst != pimlSrc)
    {
        goto Exit;
    }

    EnterUserMaybe();
    pimlTmp = (uFlags & ILCF_SWAP)? pimlSrc : NULL;

    if (pimlDst->GetImageRect(iDst, &rcDst) &&
        pimlSrc->GetImageRect(iSrc, &rcSrc) &&
        (!pimlTmp || pimlTmp->GetSpareImageRect(&rcTmp)))
    {
        int cx = pimlSrc->m_cx;
        int cy = pimlSrc->m_cy;

        //
        // iff we are swapping we need to save the destination image
        //
        if (pimlTmp)
        {
            Gdi::BitBlt_I(pimlTmp->m_hdcImage, rcTmp.left, rcTmp.top, cx, cy,
                   pimlDst->m_hdcImage, rcDst.left, rcDst.top, SRCCOPY);

            if (pimlTmp->m_hdcMask /*&& pimlDst->hdcMask*/)
            {
                Gdi::BitBlt_I(pimlTmp->m_hdcMask, rcTmp.left, rcTmp.top, cx, cy,
                       pimlDst->m_hdcMask, rcDst.left, rcDst.top, SRCCOPY);
            }
        }

        //
        // copy the image
        //
        Gdi::BitBlt_I(pimlDst->m_hdcImage, rcDst.left, rcDst.top, cx, cy,
           pimlSrc->m_hdcImage, rcSrc.left, rcSrc.top, SRCCOPY);

        if (pimlSrc->m_hdcMask )
        {
            Gdi::BitBlt_I(pimlDst->m_hdcMask, rcDst.left, rcDst.top, cx, cy,
                   pimlSrc->m_hdcMask, rcSrc.left, rcSrc.top, SRCCOPY);
        }

        //
        // iff we are swapping we need to copy the saved image too
        //
        if (pimlTmp)
        {
            Gdi::BitBlt_I(pimlSrc->m_hdcImage, rcSrc.left, rcSrc.top, cx, cy,
                   pimlTmp->m_hdcImage, rcTmp.left, rcTmp.top, SRCCOPY);

            if (pimlSrc->m_hdcMask )
            {
                Gdi::BitBlt_I(pimlSrc->m_hdcMask, rcSrc.left, rcSrc.top, cx, cy,
                       pimlTmp->m_hdcMask, rcTmp.left, rcTmp.top, SRCCOPY);
            }
        }

        bRet = TRUE;
}

    LeaveUserMaybe();
Exit:
    return bRet;
}



//
// ImageList_Duplicate
//
// Makes a copy of the passed in imagelist.
//
ImageList*
ImageList::
Duplicate(
    ImageList* piml
    )
{
    ImageList* pimlCopy = NULL;
    HBITMAP hbmImage;
    HBITMAP hbmMask = NULL;
    RGBQUAD* pargbImage;
    UserTakenHereFlag;

    ASSERT(piml);
    if(!piml || !piml->IsImageList())
    {
        goto Exit;
    }

    EnterUserMaybe();

    hbmImage = piml->CopyDIBBitmap(piml->m_hbmImage, piml->m_hdcImage, &pargbImage);
    if (!hbmImage)
    {
        goto Error;
    }

    if (piml->m_hdcMask)
    {
        hbmMask = ImageList::CopyBitmap(piml->m_hbmMask, piml->m_hdcMask);
        if (!hbmMask)
        {
            goto Error;
        }
    }

    pimlCopy = ImageList::Create(piml->m_cx, piml->m_cy, piml->m_flags, 0, piml->m_cGrow);

    if (pimlCopy)
    {

        // Slam in our bitmap copies and delete the old ones
        Gdi::SelectObject_I(pimlCopy->m_hdcImage, hbmImage);
        ImageList::DeleteBitmap(pimlCopy->m_hbmImage);
        if (pimlCopy->m_hdcMask)
        {
            Gdi::SelectObject_I(pimlCopy->m_hdcMask, hbmMask);
            ImageList::DeleteBitmap(pimlCopy->m_hbmMask);
        }
        pimlCopy->m_hbmImage = hbmImage;
        pimlCopy->m_hbmMask = hbmMask;
        pimlCopy->m_pargbImage = pargbImage;

        // Make sure other info is correct
        pimlCopy->m_cImage = piml->m_cImage;

        pimlCopy->m_cAlloc = piml->m_cAlloc;
        pimlCopy->m_cStrip = piml->m_cStrip;
        pimlCopy->m_clrBlend = piml->m_clrBlend;
        pimlCopy->m_clrBk = piml->m_clrBk;
        pimlCopy->m_bColorsSet = piml->m_bColorsSet;

#define CopyAggregate(field) \
        memcpy(&pimlCopy->field, &(piml->field), sizeof(piml->field))

        CopyAggregate(m_aOverlayIndexes);
        CopyAggregate(m_aOverlayX);
        CopyAggregate(m_aOverlayY);
        CopyAggregate(m_aOverlayDX);
        CopyAggregate(m_aOverlayDY);

#undef CopyAggregate

        if(piml->m_pdib)
        {
            pimlCopy->m_pdib = GweMemory_t::New<DIB_t>();
            if(pimlCopy->m_pdib)
            {
                memcpy(pimlCopy->m_pdib, piml->m_pdib, sizeof(DIB_t));
            }
        }

        // Delete the old brush and create the correct one
        if (pimlCopy->m_hbrBk)
        {
            Gdi::DeleteObject_I(pimlCopy->m_hbrBk);
        }

        if ((pimlCopy->m_clrBk == CLR_NONE) || (pimlCopy->m_flags & ILC_VIRTUAL))
        {
            pimlCopy->m_hbrBk = StockObjectHandles::BlackBrush();
            pimlCopy->m_bSolidBk = true;
        }
        else
        {
            pimlCopy->m_hbrBk = Gdi::CreateSolidBrush_I(pimlCopy->m_clrBk);
            pimlCopy->m_bSolidBk = (Gdi::GetNearestColor_I(pimlCopy->m_hdcImage, pimlCopy->m_clrBk) == pimlCopy->m_clrBk);
        }
    }
    else
    {
Error:
        if (hbmImage)
        {
            ImageList::DeleteBitmap(hbmImage);
        }
        if (hbmMask)
        {
            ImageList::DeleteBitmap(hbmMask);
        }
    }

    LeaveUserMaybe();

Exit:
    return pimlCopy;
}


BOOL
ImageList::
SetImageCount(
    ImageList* piml,
    UINT uAlloc
    )
{
    BOOL bRet = FALSE;
    UserTakenHereFlag;

    if (!piml->IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    EnterUserMaybe();
    if (piml->ReAllocBitmaps(-((int)uAlloc + 1)))
    {
        piml->m_cImage = (int)uAlloc;
        bRet = TRUE;
    }
    LeaveUserMaybe();
Exit:
    return bRet;
}

BOOL
ImageList::
GetImageRect(
    int i,
    RECT *prcImage
    )
{
    int x, y;
    BOOL bRet = FALSE;

    ASSERT(prcImage);
    if (!this || !prcImage || (i < 0 || i >= m_cImage))
    {
        goto Exit;
    }

    x = m_cx * (i % m_cStrip);
    y = m_cy * (i / m_cStrip);

    SetRect(prcImage, x, y, x + m_cx, y + m_cy);
    bRet = TRUE;

Exit:
    return bRet;
}

BOOL
ImageList::
GetImageRectInverted(
    int i,
    RECT * prcImage
    )
{
    int x, y;
    BOOL bRet = FALSE;

    ASSERT(prcImage);
    if (!this || !prcImage || (i < 0 || i >= m_cImage))
    {
        goto Exit;
    }

    x = 0;
    y = (m_cy * m_cAlloc) - (m_cy * i) - m_cy;

    SetRect(prcImage, x, y, x + m_cx, y + m_cy);
    bRet = TRUE;

Exit:
    return bRet;
}

void
ImageList::
Merge2(
    ImageList* pimlMerge,
    int i,
    int dx,
    int dy
    )
{
    if (m_hdcMask && pimlMerge->m_hdcMask)
    {
        RECT rcMerge;
        rcMerge.left = 0;
        rcMerge.top = 0;

        pimlMerge->GetImageRect(i, &rcMerge);

        Gdi::BitBlt_I(m_hdcMask, dx, dy, pimlMerge->m_cx, pimlMerge->m_cy,
               pimlMerge->m_hdcMask, rcMerge.left, rcMerge.top, SRCAND);
    }

    ImageList::Draw(pimlMerge, i, m_hdcImage, dx, dy, ILD_TRANSPARENT);
}


// reset the background color of images iFirst through iLast
void
ImageList::
ResetBkColor(
    int iFirst,
    int iLast,
    COLORREF clr
    )
{
    HBRUSH hbrT=NULL;
    DWORD  rop;

    if (!IsImageList() || m_hdcMask == NULL)
    {
        return;
    }

    if (clr == CLR_BLACK || clr == CLR_NONE)
    {
        rop = ROP_DSna;
    }
    else if (clr == CLR_WHITE)
    {
        rop = ROP_DSo;
    }
    else
    {
        ASSERT(m_hbrBk);
        ASSERT(m_clrBk == clr);

        rop = ROP_PatMask;
        hbrT = (HBRUSH)Gdi::SelectObject_I(m_hdcImage, m_hbrBk);
    }

    for ( ; iFirst <= iLast; iFirst++)
    {
        RECT rc;

        GetImageRect(iFirst, &rc);

        Gdi::BitBlt_I(m_hdcImage, rc.left, rc.top, m_cx, m_cy,
           m_hdcMask, rc.left, rc.top, rop);
    }

    if (hbrT)
    {
        Gdi::SelectObject_I(m_hdcImage, hbrT);
    }
    return;

}

BOOL
ImageList::
GetSpareImageRect(
    RECT *prcImage
    )
{
    BOOL bRet = FALSE;

    if(m_cImage < m_cAlloc)
    {
        // special case to use the one scratch image at tail of list :)
        m_cImage++;
        bRet = GetImageRect(m_cImage-1, prcImage);
        m_cImage--;
    }

    return bRet;
}

BOOL
ImageList::
GetSpareImageRectInverted(
    RECT * prcImage
    )
{
    BOOL bRet = FALSE;

    if (m_cImage < m_cAlloc)
    {
        // special hacking to use the one scratch image at tail of list :)
        m_cImage++;
        bRet = GetImageRectInverted(m_cImage-1, prcImage);
        m_cImage--;
    }

    return bRet;
}

/*
** ImageList_Blend
**
**  copy the source to the dest blended with the given color.
**
*/
void
ImageList::
Blend(
    HDC hdcDst,
    int xDst,
    int yDst,
    int iImage,
    int cx,
    int cy,
    COLORREF rgb,
    UINT fStyle
    )
{
    BITMAP bm;
    RECT rc;
    int bpp = Gdi::GetDeviceCaps_I(hdcDst, BITSPIXEL);

    Gdi::GetObjectW_I(m_hbmImage, sizeof(bm), &bm);
    GetImageRect(iImage, &rc);

    //
    // if _hbmImage is a DIBSection and we are on a HiColor device
    // the do a "real" blend
    //
    if (bm.bmBits && bm.bmBitsPixel <= 8 && (bpp > 8 || bm.bmBitsPixel==8))
    {
        // blend from a 4bit or 8bit DIB
        BlendCT(hdcDst, xDst, yDst, rc.left, rc.top, cx, cy, rgb, fStyle);
    }
    else if (bm.bmBits && bm.bmBitsPixel == 16 && bpp > 8)
    {
        // blend from a 16bit 555 DIB
        Blend16(hdcDst, xDst, yDst, iImage, cx, cy, rgb, fStyle);
    }
    else
    {
        // simulate a blend with a dither pattern.
        BlendDither(hdcDst, xDst, yDst, rc.left, rc.top, cx, cy, rgb, fStyle);
    }
}

/*
** Blend16
**
**  copy the source to the dest blended with the given color.
**
**  source is assumed to be a 16 bit (RGB 555) bottom-up DIBSection
**  (this is the only kind of DIBSection we create)
*/
void
ImageList::
Blend16(
    HDC hdcDst,
    int xDst,
    int yDst,
    int iImage,
    int cx,
    int cy,
    COLORREF rgb,
    UINT fStyle
    )
{
    BITMAP bm;
    RECT rc;
    RECT rcSpare;
    RECT rcSpareInverted;
    int  a, x, y;

    // get bitmap info for source bitmap
    Gdi::GetObjectW_I(m_hbmImage, sizeof(bm), &bm);
    ASSERT(bm.bmBitsPixel==16);

    // get blend RGB
    if (rgb == CLR_DEFAULT)
    {
        rgb = GetSysColor(COLOR_HIGHLIGHT);
    }

    // get blend factor as a fraction of 256
    // only 50% or 25% is currently used.
    if ((fStyle & ILD_BLENDMASK) == ILD_BLEND50)
    {
        a = 128;
    }
    else
    {
        a = 64;
    }

    GetImageRectInverted(iImage, &rc);
    x = rc.left;
    y = rc.top;

    // blend the image with the specified color and place at end of image list
    if(GetSpareImageRectInverted(&rcSpareInverted) &&
        GetSpareImageRect(&rcSpare))
    {
        // if blending with the destination, copy the dest to our work buffer
        if (rgb == CLR_NONE)
        {
            Gdi::BitBlt_I(m_hdcImage, rcSpare.left, rcSpare.top, m_cx, m_cy, hdcDst, xDst, yDst, SRCCOPY);
        }

        // sometimes the user can change the icon size (via plustab) between 32x32 and 48x48,
        // thus the values we have might be bigger than the actual bitmap. To prevent us from
        // crashing in Blend16 when this happens we do some bounds checks here
        if (rc.left + m_cx <= bm.bmWidth  &&
            rc.top + m_cy <= bm.bmHeight &&
            x + m_cx <= bm.bmWidth  &&
            y + m_cy <= bm.bmHeight)
        {
            // Needs inverted coordinates
            Blend16Helper(x, y, rcSpareInverted.left, rcSpareInverted.top, m_cx, m_cy, rgb, a);
        }

        // blt blended image to the dest DC
        Gdi::BitBlt_I(hdcDst, xDst, yDst, cx, cy, m_hdcImage, rc.left, rc.top, SRCCOPY);
    }
}

//  RGB555 macros
// On 16 bit BI_RGB format, the relative intensities of red, green, and blue are represented with five bits
// for each color component. Because of the 5-bits grouping, this format is also known as RGB555
// The most significant bit is not used
// The value for blue is in the least significant five bits, followed by five bits each for green and red.
#define RGB555(r,g,b)       (((((r)>>3)&0x1F)<<10) | ((((g)>>3)&0x1F)<<5) | (((b)>>3)&0x1F))
#define R_555(w)            (int)(((w) >> 7) & 0xF8)
#define G_555(w)            (int)(((w) >> 2) & 0xF8)
#define B_555(w)            (int)(((w) << 3) & 0xF8)

void
ImageList::
Blend16Helper(
    int xSrc,
    int ySrc,
    int xDst,
    int yDst,
    int cx,
    int cy,
    COLORREF rgb,
    int a)          // alpha value
{
    // If it's odd, Adjust. 
    if ((cx & 1) == 1)
    {
        cx++;
    }

    if (rgb == CLR_NONE)
    {
        // blending with the destination, we ignore the alpha and always
        // do 50% (this is what the old dither mask code did)

        int ys = ySrc;
        int yd = yDst;

        for (; ys < ySrc + cy; ys++, yd++)
        {
            const WORD *pSrc = &((WORD*)m_pargbImage)[xSrc + ys * cx];  // Cast because we've gotten to this case because we are a 555 imagelist
            WORD* pDst = &((WORD*)m_pargbImage)[xDst + yd * cx];
            for (int x = 0; x < cx; x++)
            {
                *pDst++ = ((*pDst & 0x7BDE) >> 1) + ((*pSrc++ & 0x7BDE) >> 1);
            }
        }
    }
    else
    {
        // blending with a solid color

        // pre multiply source (constant) rgb by alpha
        int sr = GetRValue(rgb) * a;
        int sg = GetGValue(rgb) * a;
        int sb = GetBValue(rgb) * a;

        // compute inverse alpha for inner loop
        a = 256 - a;

        // special case a 50% blend, to avoid a multiply
        if (a == 128)
        {
            sr = RGB555(sr>>8,sg>>8,sb>>8);

            int ys = ySrc;
            int yd = yDst;
            for (; ys < ySrc + cy; ys++, yd++)
            {
                const WORD *pSrc = &((WORD*)m_pargbImage)[xSrc + ys * cx];
                WORD* pDst = &((WORD*)m_pargbImage)[xDst + yd * cx];
                for (int x = 0; x < cx; x++)
                {
                    int i = *pSrc++;
                    i = sr + ((i & 0x7BDE) >> 1);
                    *pDst++ = (WORD) i;
                }
            }
        }
        else
        {
            int ys = ySrc;
            int yd = yDst;
            for (; ys < ySrc + cy; ys++, yd++)
            {
                const WORD *pSrc = &((WORD*)m_pargbImage)[xSrc + ys * cx];
                WORD* pDst = &((WORD*)m_pargbImage)[xDst + yd * cx];
                for (int x = 0; x < cx; x++)
                {
                    int i = *pSrc++;
                    int r = (R_555(i) * a + sr) >> 8;
                    int g = (G_555(i) * a + sg) >> 8;
                    int b = (B_555(i) * a + sb) >> 8;
                    *pDst++ = RGB555(r,g,b);
                }
            }
        }
    }
}

void
ImageList::
BlendCT(
    HDC hdcDst,
    int xDst,
    int yDst,
    int x,
    int y,
    int cx,
    int cy,
    COLORREF rgb,
    UINT fStyle
    )
{
    BITMAP bm;

    Gdi::GetObjectW_I(m_hbmImage, sizeof(bm), &bm);

    if (rgb == CLR_DEFAULT)
    {
        rgb = GetSysColor(COLOR_HIGHLIGHT);
    }

    ASSERT(rgb != CLR_NONE);

    //
    // get the DIB color table and blend it, only do this when the
    // blend color changes
    //
    if (m_clrBlend != rgb)
    {
        int n,cnt;

        if(!m_pdib)
        {
            m_pdib = GweMemory_t::New<DIB_t>();
            if(!m_pdib)
            {
                DEBUGMSG(1, (L"ImageList: Out of near memory"));
                goto Default;
            }
        }

        m_clrBlend = rgb;

        Gdi::GetObjectW_I(m_hbmImage, sizeof(DIB_t), &m_pdib->bm);
        cnt = Gdi::GetDIBColorTable_I(m_hdcImage, 0, 256, (RGBQUAD*)&m_pdib->bmiColors);

        if ((fStyle & ILD_BLENDMASK) == ILD_BLEND50)
        {
            n = 50;
        }
        else
        {
            n = 25;
        }

        BlendCTHelper(m_pdib->bmiColors, rgb, n, cnt);
    }

    if(m_pdib)
    {
        //
        // draw the image with a different color table
        //
        Gdi::StretchDIBits_I(hdcDst,
                        xDst,
                        yDst,
                        cx,
                        cy,
                        x,
                        m_pdib->bmiHeader.biHeight-(y+cy),
                        cx,
                        cy,
                        bm.bmBits,
                        (LPBITMAPINFO)&m_pdib->bmiHeader,
                        DIB_RGB_COLORS,
                        SRCCOPY);
    }
    else
    {
Default:
        // simulate a blend with a dither pattern.
        BlendDither(hdcDst, xDst, yDst, x, y, cx, cy, rgb, fStyle);
    }
}

void
ImageList::
BlendCTHelper(
    DWORD *pdw,
    DWORD rgb,
    UINT n,
    UINT count
    )
{
    UINT i;

    for (i=0; i<count; i++)
    {
        pdw[i] = RGB(
            ((UINT)GetRValue(pdw[i]) * (100-n) + (UINT)GetBValue(rgb) * (n)) / 100,
            ((UINT)GetGValue(pdw[i]) * (100-n) + (UINT)GetGValue(rgb) * (n)) / 100,
            ((UINT)GetBValue(pdw[i]) * (100-n) + (UINT)GetRValue(rgb) * (n)) / 100);
    }
}

/*
** BlendDither
**
**  copy the source to the dest blended with the given color.
**
**  simulate a blend with a dither pattern.
**
*/
void
ImageList::
BlendDither(
    HDC hdcDst,
    int xDst,
    int yDst,
    int x,
    int y,
    int cx,
    int cy,
    COLORREF rgb,
    UINT fStyle
    )
{
    HBRUSH hbr;
    HBRUSH hbrT;
    HBRUSH hbrMask;
    HBRUSH hbrFree = NULL;         // free if non-null

    ASSERT(Gdi::GetTextColor_I(hdcDst) == CLR_BLACK);
    ASSERT(Gdi::GetBkColor_I(hdcDst) == CLR_WHITE);

    // choose a dither/blend brush

    switch (fStyle & ILD_BLENDMASK)
    {
        default:
        case ILD_BLEND50:
            hbrMask = s_hbrMonoDither;
            break;

        case ILD_BLEND25:
            hbrMask = s_hbrStripe;
            break;
    }

    // create (or use a existing) brush for the blend color

    switch (rgb)
    {
        case CLR_DEFAULT:
            hbr = Gdi::GetSysColorBrush_I(COLOR_HIGHLIGHT);
            break;

        case CLR_NONE:
            hbr = m_hbrBk;
            break;

        default:
            if (rgb == m_clrBk)
            {
                hbr = m_hbrBk;
            }
            else
            {
                hbr = hbrFree = Gdi::CreateSolidBrush_I(rgb);
            }
            break;
    }

    hbrT = (HBRUSH)Gdi::SelectObject_I(hdcDst, hbr);
    Gdi::PatBlt_I(hdcDst, xDst, yDst, cx, cy, PATCOPY);
    Gdi::SelectObject_I(hdcDst, hbrT);

    hbrT = (HBRUSH)Gdi::SelectObject_I(hdcDst, hbrMask);
    Gdi::BitBlt_I(hdcDst, xDst, yDst, cx, cy, m_hdcImage, x, y, ROP_MaskPat);
    Gdi::SelectObject_I(hdcDst, hbrT);


    if (hbrFree)
    {
        Gdi::DeleteObject_I(hbrFree);
    }
}

ImageList*
ImageList::
Create2(
    int cx,
    int cy,
    UINT flags,
    int cGrow
    )
{
    ImageList* piml = NULL;
    int i;

    if (flags & ~ILC_VALID)
    {
        ASSERT(FALSE);
        goto Exit;
    }

    // WinCE: we keep track of all image lists in the system so we can
    // free them when a process exits without freeing its imagelist(s)
    piml = reinterpret_cast<ImageList*>(TblNewElement(sizeof(ImageList)));
    if (!piml)
    {
        DEBUGMSG(1, (L"ImageList: Out of near memory"));
        goto Exit;
    }
    
    memset(piml, 0, sizeof(ImageList));
    if (cGrow < 4)
    {
        cGrow = 4;
    }
    else
    {
        // round up by 4's
        cGrow = (cGrow + 3) & ~3;
    }

    //piml->m_cImage = 0;
    //piml->m_cAlloc = 0;
    piml->m_cStrip = 4;
    piml->m_cGrow = cGrow;
    piml->m_cx = cx;
    piml->m_cy = cy;
    piml->m_clrBlend = CLR_NONE;
    piml->m_clrBk = CLR_NONE;
    piml->m_hbrBk = StockObjectHandles::BlackBrush();
    piml->m_bSolidBk = true;
    piml->m_flags = flags;
    //piml->m_hbmImage = NULL;
    //piml->m_hbmMask = NULL;
    //piml->m_hInstOwner = NULL;
    //piml->m_hdcImage = NULL;
    //piml->m_hdcMask = NULL;
    //piml->m_pdib = NULL;

    piml->m_Sig = IMAGELIST_SIG;

    //
    // Initialize the overlay indexes to -1 since 0 is a valid index.
    //

    for (i = 0; i < NUM_OVERLAY_IMAGES; i++)
    {
        piml->m_aOverlayIndexes[i] = -1;
    }

Exit:
    return piml;
}



BOOL
ImageList::
ReAllocBitmaps(
    int cAlloc
    )
{
    BOOL bRet = FALSE;
    HBITMAP hbmImageNew;
    HBITMAP hbmMaskNew;
    RGBQUAD* pargbImageNew;
    int cx, cy;

    if (!IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    // NOTE NOTE: don't shrink unless the caller passes a negative count
    if (cAlloc > 0)
    {
        if (m_cAlloc >= cAlloc)
        {
            // nothing to do
            bRet = TRUE;
            goto Exit;
        }
    }
    else
    {
        cAlloc *= -1;
    }
    hbmMaskNew = NULL;
    hbmImageNew = NULL;
    pargbImageNew = NULL;

    cx = m_cx * m_cStrip;
    cy = m_cy * ((cAlloc + m_cStrip - 1) / m_cStrip);
    if (cAlloc > 0)
    {
        if (m_flags & ILC_MASK)
        {

            hbmMaskNew = ImageList::CreateMonoBitmap(cx, cy);
            if (!hbmMaskNew)
            {
                DEBUGMSG(1, (L"ImageList: Can't create bitmap"));
                goto Exit;
            }
        }

        hbmImageNew = CreateBitmap(cx, cy, &pargbImageNew);
        if (!hbmImageNew)
        {
            if (hbmMaskNew)
            {
                ImageList::DeleteBitmap(hbmMaskNew);
            }

            DEBUGMSG(1, (L"ImageList: Can't create bitmap"));
            goto Exit;
        }
    }

    if (m_cImage > 0)
    {
        int cyCopy = m_cy * ((min(cAlloc, m_cImage) + m_cStrip - 1) / m_cStrip);

        if (m_flags & ILC_MASK)
        {
            ImageList::SelectDstBitmap(hbmMaskNew);
            Gdi::BitBlt_I(s_hdcDst, 0, 0, cx, cyCopy, m_hdcMask, 0, 0, SRCCOPY);
        }

        ImageList::SelectDstBitmap(hbmImageNew);
        Gdi::BitBlt_I(s_hdcDst, 0, 0, cx, cyCopy, m_hdcImage, 0, 0, SRCCOPY);
    }

    // select into DC's, delete then assign
    ImageList::SelectDstBitmap(NULL);
    ImageList::SelectSrcBitmap(NULL);
    Gdi::SelectObject_I(m_hdcImage, hbmImageNew);

    if (m_hdcMask)
    {
        Gdi::SelectObject_I(m_hdcMask, hbmMaskNew);
    }

    if (m_hbmMask)
    {
        ImageList::DeleteBitmap(m_hbmMask);
    }

    if (m_hbmImage)
    {
        ImageList::DeleteBitmap(m_hbmImage);
    }

    m_hbmMask = hbmMaskNew;
    m_hbmImage = hbmImageNew;
    m_pargbImage = pargbImageNew;
    m_clrBlend = CLR_NONE;

    m_cAlloc = cAlloc;
    bRet = TRUE;

Exit:
    return bRet;
}

// in:
//  piml            image list to add to
//  hbmImage & hbmMask  the new image(s) to add, if multiple pass in horizontal strip
//  cImage          number of images to add in hbmImage and hbmMask
//
// returns:
//  index of new item, if more than one starting index of new items
int
ImageList::
Add2(
    HBITMAP hbmImage,
    HBITMAP hbmMask,
    int cImage,
    int xStart,
    int yStart
    )
{
    int i = -1;
    RGBQUAD* pargb = NULL;
    UserTakenHereFlag;

    if (!IsImageList())
    {
        ASSERT(0);
        goto Exit;
    }

    EnterUserMaybe();

    //
    // if the ImageList is empty clone the color table of the first
    // bitmap you add to the imagelist.
    //
    // the ImageList needs to be a 8bpp image list
    // the bitmap being added needs to be a 8bpp DIBSection
    //
    if (hbmImage && m_cImage == 0 &&
        (m_flags & ILC_COLORMASK) != ILC_COLORDDB)
    {
        if (!m_bColorsSet)
        {
            int n;

            pargb = GweMemory_t::NewArray<RGBQUAD>(256);
            if (!pargb)
            {
                goto Cleanup;
            }

            ImageList::SelectDstBitmap(hbmImage);

            if (n = Gdi::GetDIBColorTable_I(s_hdcDst, 0, 256, pargb))
            {
                m_bColorsSet = true;
                if(m_hdcImage)
                {
                    Gdi::SetDIBColorTable_I(m_hdcImage, 0, n, pargb);
                }
            }

            ImageList::SelectDstBitmap(NULL);
        }
        
        m_clrBlend = CLR_NONE;
    }


    if (m_cImage + cImage + 1 > m_cAlloc)
    {
        if (!ReAllocBitmaps(m_cAlloc + max(cImage, m_cGrow) + 1))
        {
            goto Cleanup;
        }
    }

    i = m_cImage;
    m_cImage += cImage;

    if (hbmImage && !Replace2(i, cImage, hbmImage, hbmMask, xStart, yStart))
    {
        m_cImage -= cImage;
        i = -1;
        goto Cleanup;
    }

Cleanup:
    LeaveUserMaybe();

    if (pargb)
    {
        GweMemory_t::Delete(pargb);
    }
    
Exit:
    return i;
}

// NOTE: ***IMPORTANT***  This routine used to contain critical sections.
// It is now a callback used when deleting items from the system image list
// table. All calls that invoke this callback ARE and MUST be protected by
// critical sections.
void
ImageList::
Cleanup(
    ImageList* piml
    )
{
    if (!piml->IsImageList())
    {
        return;
    }
    // delete dc's
    if (piml->m_hdcImage && (piml->m_hdcImage != (HDC)-1))
    {
        Gdi::SelectObject_I(piml->m_hdcImage, s_hbmDcDeselect);
        Gdi::DeleteDC_I(piml->m_hdcImage);
    }
    if (piml->m_hdcMask && (piml->m_hdcMask != (HDC)-1))
    {
        Gdi::SelectObject_I(piml->m_hdcMask, s_hbmDcDeselect);
        Gdi::DeleteDC_I(piml->m_hdcMask);
    }

    // delete bitmaps
    if (piml->m_hbmImage && (piml->m_hbmImage != (HBITMAP)-1))
    {
        ImageList::DeleteBitmap(piml->m_hbmImage);
    }
    if (piml->m_hbmMask && (piml->m_hbmMask != (HBITMAP)-1))
    {
        ImageList::DeleteBitmap(piml->m_hbmMask);
    }
    if (piml->m_hbrBk)
    {
        Gdi::DeleteObject_I(piml->m_hbrBk);
    }

    //delete dib
    if(piml->m_pdib)
    {
        GweMemory_t::Delete(piml->m_pdib);
    }
    
    // one less use of imagelists.  if it's the last, terminate the imagelist
    s_iILRefCount--;
    if (!s_iILRefCount)
    {
        ImageList::Terminate();
    }
    piml->m_Sig = 0;

}


//
// ImageList_CopyBitmap
//
// Worker function for ImageList_Duplicate.
//
// Given a bitmap and an hdc, creates and returns a copy of the passed in bitmap.
//
HBITMAP
ImageList::
CopyBitmap(
    HBITMAP hbm,
    HDC hdc
    )
{
    HBITMAP hbmCopy;
    BITMAP bm;
    UserTakenHereFlag;

    ASSERT(hbm);

    hbmCopy = NULL;
    if (Gdi::GetObjectW_I(hbm, sizeof(bm), &bm) == sizeof(bm))
    {
        EnterUserMaybe();
        if (hbmCopy = Gdi::CreateCompatibleBitmap_I(hdc, bm.bmWidth, bm.bmHeight))
        {
            ImageList::SelectDstBitmap(hbmCopy);

            Gdi::BitBlt_I(s_hdcDst,
                            0,
                            0,
                            bm.bmWidth,
                            bm.bmHeight,
                            hdc,
                            0,
                            0,
                            SRCCOPY);

            ImageList::SelectDstBitmap(NULL);
        }
        LeaveUserMaybe();
    }
    return hbmCopy;
}

HBITMAP
ImageList::
CopyDIBBitmap(
    HBITMAP hbm,
    HDC hdc,
    RGBQUAD** ppargb
    )
{
    HBITMAP hbmCopy = NULL;
    BITMAP bm;
    UserTakenHereFlag;
    
    ASSERT(hbm);

    if (Gdi::GetObjectW_I(hbm, sizeof(bm), &bm) == sizeof(bm))
    {
        EnterUserMaybe();
        hbmCopy = CreateBitmap(bm.bmWidth, bm.bmHeight, ppargb);

        if (hbmCopy)
        {
            ImageList::SelectDstBitmap(hbmCopy);

            Gdi::BitBlt_I(s_hdcDst,
                            0,
                            0,
                            bm.bmWidth,
                            bm.bmHeight,
                            hdc,
                            0,
                            0,
                            SRCCOPY);

            ImageList::SelectDstBitmap(NULL);
        }
        LeaveUserMaybe();
    }
    return hbmCopy;
}


// copy an image from one imagelist to another at x,y within iDst in pimlDst.
// pimlDst's image size should be larger than pimlSrc
void
ImageList::
CopyOneImage(
    int iDst,
    int x,
    int y,
    ImageList* pimlSrc,
    int iSrc
    )
{
    RECT rcSrc, rcDst;

    pimlSrc->GetImageRect(iSrc, &rcSrc);
    GetImageRect(iDst, &rcDst);

    if (pimlSrc->m_hdcMask && m_hdcMask)
    {
        Gdi::BitBlt_I(m_hdcMask, rcDst.left + x, rcDst.top + y, pimlSrc->m_cx, pimlSrc->m_cy,
               pimlSrc->m_hdcMask, rcSrc.left, rcSrc.top, SRCCOPY);

    }

    Gdi::BitBlt_I(m_hdcImage, rcDst.left + x, rcDst.top + y, pimlSrc->m_cx, pimlSrc->m_cy,
           pimlSrc->m_hdcImage, rcSrc.left, rcSrc.top, SRCCOPY);
}


//
// create a bitmap compatible with the given ImageList
//
HBITMAP
ImageList::
CreateBitmap(
    int cx,
    int cy,
    RGBQUAD** ppargb
    )
{
    HDC hdc;
    HBITMAP hbm = NULL;
    UINT flags;

    if (this)
    {
        flags = m_flags;
    }
    else
    {
        flags = 0;
    }

    hdc = CWindow::GetDC_I(NULL);
    if (!hdc)
    {
        goto Exit;
    }

    // no color depth was specifed
    //
    // Desktop defaults to 4bit DIBSections to save memory,
    // if it is a DIBENG based DISPLAY
    // On WinCE use ILC_COLORDDB for backwards compatibility
    if ((flags & ILC_COLORMASK) == 0)
    {
        flags |= ILC_COLORDDB;
    }

    if ((flags & ILC_COLORMASK) != ILC_COLORDDB)
    {
        DIB_t* pdib = GweMemory_t::New<DIB_t>();
        if (!pdib)
        {
            goto CleanUp;
        }

        pdib->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        pdib->bmiHeader.biWidth = cx;
        pdib->bmiHeader.biHeight = cy;
        pdib->bmiHeader.biPlanes = 1;
        pdib->bmiHeader.biBitCount = (flags & ILC_COLORMASK);
        pdib->bmiHeader.biCompression = BI_RGB;
        pdib->bmiHeader.biSizeImage = 0;
        pdib->bmiHeader.biXPelsPerMeter = 0;
        pdib->bmiHeader.biYPelsPerMeter = 0;
        pdib->bmiHeader.biClrUsed = 16;
        pdib->bmiHeader.biClrImportant = 0;
        pdib->bmiColors[0] = 0x00000000;    // 0000  black
        pdib->bmiColors[1] = 0x00800000;    // 0001  dark red
        pdib->bmiColors[2] = 0x00008000;    // 0010  dark green
        pdib->bmiColors[3] = 0x00808000;    // 0011  mustard
        pdib->bmiColors[4] = 0x00000080;    // 0100  dark blue
        pdib->bmiColors[5] = 0x00800080;    // 0101  purple
        pdib->bmiColors[6] = 0x00008080;    // 0110  dark turquoise
        pdib->bmiColors[7] = 0x00C0C0C0;    // 1000  gray
        pdib->bmiColors[8] = 0x00808080;    // 0111  dark gray
        pdib->bmiColors[9] = 0x00FF0000;    // 1001  red
        pdib->bmiColors[10] = 0x0000FF00;    // 1010  green
        pdib->bmiColors[11] = 0x00FFFF00;    // 1011  yellow
        pdib->bmiColors[12] = 0x000000FF;    // 1100  blue
        pdib->bmiColors[13] = 0x00FF00FF;    // 1101  pink (magenta)
        pdib->bmiColors[14] = 0x0000FFFF;    // 1110  cyan
        pdib->bmiColors[15] = 0x00FFFFFF;    // 1111  white

        if (pdib->bmiHeader.biBitCount == 8)
        {
#ifndef _WIN32_CE // WinCE doesn't support CreateHalftonePalette
            HPALETTE hpal;
            int i;
            if (hpal = CreateHalftonePalette(NULL))
            {
                i = GetPaletteEntries(hpal, 0, 256, (LPPALETTEENTRY)&pdib->bmiColors[0]);
                DeleteObject(hpal);

                if (i > 64)
                {
                    pdib->bmiHeader.biClrUsed = i;
                    for (i=0; i<(int)pdib->bmiHeader.biClrUsed; i++)
                    {
                        pdib->bmiColors[i] = RGB(GetBValue(pdib->bmiColors[i]),
                                        GetGValue(pdib->bmiColors[i]),
                                        GetRValue(pdib->bmiColors[i]));
                    }
                }
            }
            else
            {
                pdib->bmiHeader.biBitCount = (flags & ILC_COLORMASK);
                pdib->bmiHeader.biClrUsed = 256;
            }

            if (pdib->bmiHeader.biClrUsed <= 16)
            {
                pdib->bmiHeader.biBitCount = 4;
            }
#else
            pdib->bmiHeader.biClrUsed = 256;
            memcpy(&pdib->bmiColors, HalftoneColorPalette, 256*sizeof(DWORD));
#endif
        }
        hbm = CreateDIBSection(hdc,
                        (LPBITMAPINFO)&pdib->bmiHeader,
                        DIB_RGB_COLORS,
                        (void**)ppargb,
                        NULL,
                        0);

        GweMemory_t::Delete(pdib);
    }
    else
    {
        hbm = Gdi::CreateCompatibleBitmap_I(hdc, cx, cy);
    }

CleanUp:
    CWindow::ReleaseDC_I(NULL, hdc);

Exit:
    return hbm;
}


BOOL
ImageList::
CreateDragBitmaps(
    void
    )
{
    HDC hdc;

    if (!IsImageList())
    {
        ASSERT(0);
        return FALSE;
    }

    hdc = CWindow::GetDC_I(NULL);

    if (m_cx != s_DragRestoreBmp.sizeRestore.cx ||
        m_cy != s_DragRestoreBmp.sizeRestore.cy ||
        Gdi::GetDeviceCaps_I(hdc, BITSPIXEL) != s_DragRestoreBmp.BitsPixel)
    {
        ImageList::DeleteDragBitmaps();

        s_DragRestoreBmp.BitsPixel      = Gdi::GetDeviceCaps_I(hdc, BITSPIXEL);
        s_DragRestoreBmp.sizeRestore.cx = m_cx;
        s_DragRestoreBmp.sizeRestore.cy = m_cy;
        
        ImageList::PixelDoublePoint((int*)&s_DragRestoreBmp.sizeRestore.cx, (int*)&s_DragRestoreBmp.sizeRestore.cy);

        s_DragRestoreBmp.hbmRestore   = CreateColorBitmap(s_DragRestoreBmp.sizeRestore.cx, s_DragRestoreBmp.sizeRestore.cy);
        s_DragRestoreBmp.hbmOffScreen = CreateColorBitmap(s_DragRestoreBmp.sizeRestore.cx * 2 - 1, s_DragRestoreBmp.sizeRestore.cy * 2 - 1);

        if (!s_DragRestoreBmp.hbmRestore || !s_DragRestoreBmp.hbmOffScreen)
        {
            ImageList::DeleteDragBitmaps();
            CWindow::ReleaseDC_I(NULL, hdc);
            return FALSE;
        }

        Gdi::GdiSetObjectOwner_I (s_DragRestoreBmp.hbmRestore, v_hprcUser);
        Gdi::GdiSetObjectOwner_I (s_DragRestoreBmp.hbmOffScreen, v_hprcUser);

    }
    CWindow::ReleaseDC_I(NULL, hdc);
    return TRUE;
}

void
ImageList::
DeleteBitmap(
    HBITMAP hbm
    )
{
    if (hbm)
    {
        if (s_hbmDst == hbm)
        {
            ImageList::SelectDstBitmap(NULL);
        }
        if (s_hbmSrc == hbm)
        {
            ImageList::SelectSrcBitmap(NULL);
        }
        Gdi::DeleteObject_I(hbm);
    }
}

void
ImageList::
DeleteDragBitmaps(
    void
    )
{
    if (s_DragRestoreBmp.hbmRestore)
    {
        ImageList::DeleteBitmap(s_DragRestoreBmp.hbmRestore);
        s_DragRestoreBmp.hbmRestore = NULL;
    }
    if (s_DragRestoreBmp.hbmOffScreen)
    {
        ImageList::DeleteBitmap(s_DragRestoreBmp.hbmOffScreen);
        s_DragRestoreBmp.hbmOffScreen = NULL;
    }

    s_DragRestoreBmp.sizeRestore.cx = -1;
    s_DragRestoreBmp.sizeRestore.cy = -1;
}

int
ImageList::
GetScreenDepth(
    void
    )
{
    int i=8; // best guess

    HDC hdc = GetDC(NULL);
    if(hdc)
    {
        i = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
        ReleaseDC(NULL, hdc);
    }
    return i;
}

HDC
ImageList::
GetWorkDC(
    HDC hdc,
    int dx,
    int dy
    )
{
    if (s_hbmWork == NULL ||
        Gdi::GetDeviceCaps_I(hdc, BITSPIXEL) != s_bmWork.bmBitsPixel ||
        s_bmWork.bmWidth  < dx ||
        s_bmWork.bmHeight < dy)
    {
        ImageList::DeleteBitmap(s_hbmWork);
        s_hbmWork = NULL;

        if (dx == 0 || dy == 0)
        {
            return NULL;
        }

        s_hbmWork = Gdi::CreateCompatibleBitmap_I(hdc, dx, dy);
        if (s_hbmWork)
        {
            Gdi::GdiSetObjectOwner_I (s_hbmWork, v_hprcUser);
            Gdi::GetObjectW_I(s_hbmWork, sizeof(s_bmWork), &s_bmWork);
        }
    }

    ImageList::SelectSrcBitmap(s_hbmWork);

    if (Gdi::GetDeviceCaps_I(hdc, RASTERCAPS) & RC_PALETTE)
    {
        HPALETTE hpal;
        hpal = Gdi::SelectPalette_I(hdc, StockObjectHandles::DefaultPalette(), TRUE);
        Gdi::SelectPalette_I(s_hdcSrc, hpal, TRUE);
    }

    return s_hdcSrc;
}

BOOL
ImageList::
Init(
    void
)
{
    HDC hdcScreen = NULL;
    WORD stripebits[] = {0x7777, 0xdddd, 0x7777, 0xdddd,
                         0x7777, 0xdddd, 0x7777, 0xdddd};
    HBITMAP hbmTemp;
    BOOL bRet = FALSE;

    // if already initialized, there is nothing to do
    if (s_hdcDst)
    {
        bRet = TRUE;
        goto Exit;
    }

    hdcScreen = CWindow::GetDC_I(HWND_DESKTOP);
    if(!hdcScreen)
    {
        goto Exit;
    }

    s_hdcSrc = Gdi::CreateCompatibleDC_I(hdcScreen);
    if(!s_hdcSrc)
    {
        goto Exit;
    }

    s_hdcDst = Gdi::CreateCompatibleDC_I(hdcScreen);
    if(!s_hdcDst)
    {
        goto Exit;
    }

    Gdi::GdiSetObjectOwner_I (s_hdcSrc, v_hprcUser);
    Gdi::GdiSetObjectOwner_I (s_hdcDst, v_hprcUser);

    InitDitherBrush();

    hbmTemp = Gdi::CreateBitmap_I(8, 8, 1, 1, stripebits);
    if (hbmTemp)
    {
        // initialize the deselect 1x1 bitmap
        s_hbmDcDeselect = (HBITMAP)Gdi::SelectObject_I(s_hdcDst, hbmTemp);
        Gdi::SelectObject_I(s_hdcDst, s_hbmDcDeselect);

        s_hbrStripe = Gdi::CreatePatternBrush_I(hbmTemp);

        Gdi::GdiSetObjectOwner_I (s_hbrStripe, v_hprcUser);
        Gdi::DeleteObject_I(hbmTemp);
    }

    bRet = TRUE;

Exit:

    if(hdcScreen)
    {
        CWindow::ReleaseDC_I(HWND_DESKTOP, hdcScreen);
    }

    if (!s_hdcSrc || !s_hdcDst || !s_hbrMonoDither)
    {
        ImageList::Terminate();
        DEBUGMSG(1, (L"ImageList: Unable to initialize"));
        bRet = FALSE;
    }
    
    return bRet;
}


// BUGBUG: this hotspot stuff is broken in design
BOOL
ImageList::
MergeDragImages(
    int dxHotspot,
    int dyHotspot
    )
{
    ImageList* pimlNew;
    BOOL bRet = FALSE;

    if (s_DragContext.pimlDither)
    {
        if (s_DragContext.pimlCursor)
        {
            pimlNew = ImageList::Merge(s_DragContext.pimlDither, 0, s_DragContext.pimlCursor, s_DragContext.iCursor, dxHotspot, dyHotspot);

            if (pimlNew && pimlNew->CreateDragBitmaps())
            {
                // WARNING: Don't destroy pimlDrag if it is pimlDither.
                if (s_DragContext.pimlDrag && (s_DragContext.pimlDrag != s_DragContext.pimlDither))
                {
                    ImageList::Destroy(s_DragContext.pimlDrag);
                }

                s_DragContext.pimlDrag = pimlNew;
                bRet = TRUE;
            }
        }
        else
        {
            if (s_DragContext.pimlDither->CreateDragBitmaps())
            {
                s_DragContext.pimlDrag = s_DragContext.pimlDither;
                bRet = TRUE;
            }
        }
    }
    else
    {
        // not an error case if both aren't set yet
        // only an error if we actually tried the merge and failed
        bRet = TRUE;
    }

    return bRet;
}


void
ImageList::
ReleaseWorkDC(
    HDC hdc
    )
{

    ASSERT(hdc == s_hdcSrc);

    if (Gdi::GetDeviceCaps_I(hdc, RASTERCAPS) & RC_PALETTE)
    {
        Gdi::SelectPalette_I(hdc, StockObjectHandles::DefaultPalette(), TRUE);
    }

}


// this removes an image from the bitmap but doing all the
// proper shuffling.
//
//   this does the following:
//  if the bitmap being removed is not the last in the row
//      it blts the images to the right of the one being deleted
//      to the location of the one being deleted (covering it up)
//
//  for all rows until the last row (where the last image is)
//      move the image from the next row up to the last position
//      in the current row.  then slide over all images in that
//      row to the left.

void
ImageList::
RemoveItemBitmap(
    int i
    )
{
    RECT rc1;
    RECT rc2;
    int dx, y;
    int x;

    GetImageRect(i, &rc1);
    GetImageRect(m_cImage - 1, &rc2);

    // the row with the image being deleted, do we need to shuffle?
    // amount of stuff to shuffle
    dx = m_cStrip * m_cx - rc1.right;

    if (dx)
    {
        // yes, shuffle things left
        Gdi::BitBlt_I(
                m_hdcImage,
                rc1.left,
                rc1.top,
                dx,
                m_cy,
                m_hdcImage,
                rc1.right,
                rc1.top,
                SRCCOPY
                );
        if (m_hdcMask)
        {
            Gdi::BitBlt_I(
                    m_hdcMask,
                    rc1.left,
                    rc1.top,
                    dx,
                    m_cy,
                    m_hdcMask,
                    rc1.right,
                    rc1.top,
                    SRCCOPY
                    );
        }
    }

    y = rc1.top;    // top of row we are working on
    x = m_cx * (m_cStrip - 1); // x coord of last bitmaps in each row
    while (y < rc2.top)
    {

    // copy first from row below to last image position on this row
        Gdi::BitBlt_I(
                    m_hdcImage,
                    x,
                    y,
                    m_cx,
                    m_cy,
                    m_hdcImage,
                    0,
                    y + m_cy,
                    SRCCOPY
                    );

        if (m_hdcMask)
        {
            Gdi::BitBlt_I(
                        m_hdcMask,
                        x,
                        y,
                        m_cx,
                        m_cy,
                        m_hdcMask,
                        0,
                        y + m_cy,
                        SRCCOPY
                        );
        }

        y += m_cy;  // jump to row to slide left
        if (y <= rc2.top)
        {

            // slide the rest over to the left
            Gdi::BitBlt_I(
                        m_hdcImage,
                        0,
                        y,
                        x,
                        m_cy,
                        m_hdcImage,
                        m_cx,
                        y,
                        SRCCOPY
                        );

            // slide the rest over to the left
            if (m_hdcMask)
            {
                Gdi::BitBlt_I(
                            m_hdcMask,
                            0,
                            y,
                            x,
                            m_cy,
                            m_hdcMask,
                            m_cx,
                            y,
                            SRCCOPY
                            );
            }
        }
    }
}


BOOL
ImageList::
Replace2(
    int i,
    int cImage,
    HBITMAP hbmImage,
    HBITMAP hbmMask,
    int xStart,
    int yStart
    )
{
    RECT rcImage;
    int x, iImage;

    if (!IsImageList())
    {
        return FALSE;
    }
    ASSERT(hbmImage);

    ImageList::SelectSrcBitmap(hbmImage);
    if (m_hdcMask)
    {
        ImageList::SelectDstBitmap(hbmMask); // using as just a second source hdc
    }

    for (x = xStart, iImage = 0; iImage < cImage; iImage++, x += m_cx)
    {

        GetImageRect(i + iImage, &rcImage);

        if (m_hdcMask)
        {
            Gdi::BitBlt_I(
                    m_hdcMask,
                    rcImage.left,
                    rcImage.top,
                    m_cx,
                    m_cy,
                    s_hdcDst,
                    x,
                    yStart,
                    SRCCOPY
                    );
        }

        Gdi::BitBlt_I(
                m_hdcImage,
                rcImage.left,
                rcImage.top,
                m_cx,
                m_cy,
                s_hdcSrc,
                x,
                yStart,
                SRCCOPY
                );
    }

    ResetBkColor(i, i + cImage - 1, m_clrBk);

    //
    // Bug fix : We should unselect hbmImage, so that the client can play with
    //           it. (SatoNa)
    //
    ImageList::SelectSrcBitmap(NULL);
    if (m_hdcMask)
    {
        ImageList::SelectDstBitmap(NULL);
    }

    return TRUE;
}


void
ImageList::
SelectDstBitmap(
    HBITMAP hbmDst
    )
{

    if (hbmDst != s_hbmDst)
    {
        // If it's selected in the source DC, then deselect it first
        //
        if (hbmDst && hbmDst == s_hbmSrc)
        {
            ImageList::SelectSrcBitmap(NULL);
        }

        if (!Gdi::SelectObject_I(s_hdcDst, hbmDst ? hbmDst : s_hbmDcDeselect))
        {
            // ImageList_SelectFailed(hbmDst);
        }
        s_hbmDst = hbmDst;
    }
}

void
ImageList::SelectSrcBitmap(
    HBITMAP hbmSrc
    )
{

    if (hbmSrc != s_hbmSrc)
    {
        // If it's selected in the dest DC, then deselect it first
        //
        if (hbmSrc && hbmSrc == s_hbmDst)
        {
            ImageList::SelectDstBitmap(NULL);
        }

        if (!Gdi::SelectObject_I(s_hdcSrc, hbmSrc ? hbmSrc : s_hbmDcDeselect))
        {
            // ImageList_SelectFailed(hbmSrc);
        }
        s_hbmSrc = hbmSrc;
    }
}

void
ImageList::
Terminate(
    void
    )
{
    ImageList::TerminateDitherBrush();

    if (s_hbrStripe)
    {
        Gdi::DeleteObject_I(s_hbrStripe);
        s_hbrStripe = NULL;
    }

    ImageList::DeleteDragBitmaps();


    if (s_hdcDst)
    {
        ImageList::SelectDstBitmap(NULL);
        Gdi::DeleteDC_I(s_hdcDst);
        s_hdcDst = NULL;
    }

    if (s_hdcSrc)
    {
        ImageList::SelectSrcBitmap(NULL);
        Gdi::DeleteDC_I(s_hdcSrc);
        s_hdcSrc = NULL;
    }

    if (s_hbmWork)
    {
        Gdi::DeleteObject_I(s_hbmWork);
        s_hbmWork = NULL;
    }
}


BOOL
ImageList::
IsImageList(
    void
    )
{
    return (this && !IsBadWritePtr(this, sizeof(ImageList)) &&
        (m_Sig == IMAGELIST_SIG));
}

void
ImageList::
TerminateDitherBrush()
{
    s_iDither--;
    if (s_iDither == 0)
    {
        Gdi::DeleteObject_I(s_hbrMonoDither);
        s_hbrMonoDither = NULL;
    }
}

HBITMAP
ImageList::
CreateColorBitmap(
    int cx,
    int cy
    )
{
    HDC hdc;
    HBITMAP hbm = NULL;

    hdc = CWindow::GetDC_I(NULL);
    if(hdc)
    {
        hbm = Gdi::CreateCompatibleBitmap_I(hdc, cx, cy);
        CWindow::ReleaseDC_I(NULL, hdc);
    }

    return hbm;
}

HBITMAP
ImageList::
CreateMonoBitmap(
    int cx,
    int cy
    )
{
    return Gdi::CreateBitmap_I(cx, cy, 1, 1, NULL);
}

void
ImageList::
InitDitherBrush()
{
    if (!s_iDither)
    {
        DIBMONO_t dib;

        dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        dib.bmiHeader.biHeight = 8;
        dib.bmiHeader.biWidth = 8;
        dib.bmiHeader.biPlanes = 1;
        dib.bmiHeader.biBitCount = 1;
        dib.bmiHeader.biCompression = BI_RGB;
        dib.bmiHeader.biSizeImage = 0;
        dib.bmiHeader.biXPelsPerMeter = 0;
        dib.bmiHeader.biYPelsPerMeter = 0;
        dib.bmiHeader.biClrUsed = 0;
        dib.bmiHeader.biClrImportant = 0;
        dib.bmiColors[0] = 0x00000000;
        dib.bmiColors[1] = 0x00FFFFFF;
        dib.bmBits[0] = 0xAAAAAAAA;
        dib.bmBits[1] = 0x55555555;
        dib.bmBits[2] = 0xAAAAAAAA;
        dib.bmBits[3] = 0x55555555;
        dib.bmBits[4] = 0xAAAAAAAA;
        dib.bmBits[5] = 0x55555555;
        dib.bmBits[6] = 0xAAAAAAAA;
        dib.bmBits[7] = 0x55555555;

        s_hbrMonoDither = Gdi::CreateDIBPatternBrushPt_I(&dib, DIB_RGB_COLORS);
        Gdi::GdiSetObjectOwner_I(s_hbrMonoDither, v_hprcUser);
    }

    s_iDither++;
}

BOOL
ImageList::
PixelDoublePoint(
        int* px, 
        int* py)
{
    if (MsgQueue::NeedsPixelDoubling())
    {
        if (px)
        {
            *px = Gwe::ComputeHorizontalDpiAwareValue(*px);
        }
        if (py)
        {
            *py = Gwe::ComputeVerticalDpiAwareValue(*py);
        }
        return TRUE;
    }
    return FALSE;
}

void
ImageListProcessCleanup(
    HPROCESS hprc
    )
{
    UserTakenHereFlag;

    EnterUserMaybe();
    while (TblLookup (v_ptblImageLists, offsetof(ImageList, m_hprc), sizeof(hprc),
            &hprc, TBL_DELETE, NULL))
    {
        RETAILMSG(1, (__TEXT("APPLICATION ERROR: Exited with Image List not destroyed\r\n")));
    }
    LeaveUserMaybe();
}

BOOL InitImageLists(void)
{
    return (v_ptblImageLists = TblCreate((PFNCLEANUP)ImageList::Cleanup)) != NULL;
}

