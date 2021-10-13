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
#define WINCOMMCTRLAPI
#include <windows.h>
#include <commctrl.h>
#include <CePtr.hpp>
#include <GweApiSet1.hpp>

extern  GweApiSet1_t*    pGweApiSet1Entrypoints;

extern "C"
HIMAGELIST
ImageList_Create(
    int        cx,
    int        cy,
    UINT    flags,
    int        cInitial,
    int        cGrow
    )
{
    return (HIMAGELIST)pGweApiSet1Entrypoints->m_pCreate(cx, cy, flags, cInitial, cGrow);
}

extern "C"
BOOL
WINAPI
ImageList_Destroy(
    HIMAGELIST    himl
    )
{
    return pGweApiSet1Entrypoints->m_pDestroy(himl);
}

extern "C"
int
WINAPI
ImageList_GetImageCount(
    HIMAGELIST    himl
    )
{
    return pGweApiSet1Entrypoints->m_pGetImageCount(himl);
}

extern "C"
int
WINAPI
ImageList_Add(
    HIMAGELIST    himl,
    HBITMAP        hbmImage,
    HBITMAP        hbmMask
    )
{
    return pGweApiSet1Entrypoints->m_pAdd(himl,hbmImage,hbmMask);
}


extern "C"
int
WINAPI
ImageList_ReplaceIcon(
    HIMAGELIST    himl,
    int            i,
    HICON        hicon
    )
{
    return pGweApiSet1Entrypoints->m_pReplaceIcon(himl,i,hicon);
}

extern "C"
COLORREF
WINAPI
ImageList_SetBkColor(
    HIMAGELIST    himl,
    COLORREF    clrBk
    )
{
    return pGweApiSet1Entrypoints->m_pSetBkColor(himl,clrBk);
}

extern "C"
COLORREF
WINAPI
ImageList_GetBkColor(
    HIMAGELIST    himl
    )
{
    return pGweApiSet1Entrypoints->m_pGetBkColor(himl);
}

extern "C"
BOOL
WINAPI
ImageList_SetOverlayImage(
    HIMAGELIST    himl,
    int            iImage,
    int            iOverlay
    )
{
    return pGweApiSet1Entrypoints->m_pSetOverlayImage(himl,iImage,iOverlay);
}

extern "C"
BOOL
WINAPI
ImageList_Draw(
    HIMAGELIST    himl,
    int            i,
    HDC            hdcDst,
    int            x,
    int            y,
    UINT        fStyle
    )
{
    return pGweApiSet1Entrypoints->m_pDraw(himl,i,hdcDst,x,y,fStyle);
}

extern "C"
BOOL
WINAPI
ImageList_Replace(
    HIMAGELIST    himl,
    int            i,
    HBITMAP        hbmImage,
    HBITMAP        hbmMask
    )
{
    return pGweApiSet1Entrypoints->m_pReplace(himl,i,hbmImage,hbmMask);
}

extern "C"
int
WINAPI
ImageList_AddMasked(
    HIMAGELIST    himl,
    HBITMAP        hbmImage,
    COLORREF    crMask
    )
{
    return pGweApiSet1Entrypoints->m_pAddMasked(himl,hbmImage,crMask);
}

extern "C"
BOOL
WINAPI
ImageList_DrawEx(
    HIMAGELIST    himl,
    int            i,
    HDC            hdcDst,
    int            x,
    int            y,
    int            dx,
    int            dy,
    COLORREF    rgbBk,
    COLORREF    rgbFg,
    UINT        fStyle
    )
{
    return pGweApiSet1Entrypoints->m_pDrawEx(himl,i,hdcDst,x,y,dx,dy,rgbBk,rgbFg,fStyle);
}

extern "C"
BOOL
WINAPI
ImageList_DrawIndirect(
    __in IMAGELISTDRAWPARAMS*    pimldp
    )
{
    return pGweApiSet1Entrypoints->m_pDrawIndirect(pimldp, sizeof(IMAGELISTDRAWPARAMS));
}


extern "C"
BOOL
WINAPI
ImageList_Remove(
    HIMAGELIST    himl,
    int            i
    )
{
    return pGweApiSet1Entrypoints->m_pRemove(himl,i);
}

extern "C"
HICON
WINAPI
ImageList_GetIcon(
    HIMAGELIST    himl,
    int            i,
    UINT        flags
    )
{
    return pGweApiSet1Entrypoints->m_pGetIcon(himl,i,flags);
}

extern "C"
HIMAGELIST
WINAPI
ImageList_LoadImage(
    HINSTANCE    hInstance,
    __in const WCHAR*   pImageName,
    int            cx,
    int            cGrow,
    COLORREF    crMask,
    unsigned int    uType,
    unsigned int    uFlags
    )
{
    DWORD ImageNameResourceID =  0;

    if( IS_RESOURCE_ID(pImageName) )
        {
        ImageNameResourceID  = reinterpret_cast<DWORD>(pImageName);
        pImageName = NULL;
        }
        
    return (HIMAGELIST)pGweApiSet1Entrypoints->m_pLoadImage( hInstance, ImageNameResourceID, pImageName,cx,cGrow,crMask,uType,uFlags);
}

extern "C"
BOOL
WINAPI
ImageList_BeginDrag(
    HIMAGELIST    himlTrack,
    int            iTrack,
    int            dxHotspot,
    int            dyHotspot
    )
{
    return pGweApiSet1Entrypoints->m_pBeginDrag(himlTrack,iTrack,dxHotspot,dyHotspot);
}

extern "C"
void
WINAPI
ImageList_EndDrag(
    void
    )
{
    pGweApiSet1Entrypoints->m_pEndDrag();
}

extern "C"
BOOL
WINAPI
ImageList_DragEnter(
    HWND    hwndLock,
    int        x,
    int        y
    )
{
    return pGweApiSet1Entrypoints->m_pDragEnter(hwndLock,x,y);
}

extern "C"
BOOL
WINAPI
ImageList_DragLeave(
    HWND    hwndLock
    )
{
    return pGweApiSet1Entrypoints->m_pDragLeave(hwndLock);
}

extern "C"
BOOL
WINAPI
ImageList_DragMove(
    int    x,
    int    y
    )
{
    return pGweApiSet1Entrypoints->m_pDragMove(x,y);
}

extern "C"
BOOL
WINAPI
ImageList_SetDragCursorImage(
    HIMAGELIST    himlDrag,
    int            iDrag,
    int            dxHotspot,
    int            dyHotspot
    )
{
    return pGweApiSet1Entrypoints->m_pSetDragCursorImage(himlDrag,iDrag,dxHotspot,dyHotspot);
}

extern "C"
BOOL
WINAPI
ImageList_DragShowNolock(
    BOOL    fShow
    )
{
    return pGweApiSet1Entrypoints->m_pDragShowNolock(fShow);
}

extern "C"
HIMAGELIST
WINAPI
ImageList_GetDragImage(
    __out_opt POINT*    ppt,
    __out_opt POINT*    pptHotspot
    )
{
    return (HIMAGELIST)pGweApiSet1Entrypoints->m_pGetDragImage(ppt,sizeof(POINT),pptHotspot,sizeof(POINT));
}

extern "C"
BOOL
WINAPI
ImageList_GetIconSize(
    HIMAGELIST    himl,
    __out int*        cx,
    __out int*        cy
    )
{
    return pGweApiSet1Entrypoints->m_pGetIconSize(himl,cx,cy);
}

extern "C"
BOOL
WINAPI
ImageList_SetIconSize(
    HIMAGELIST    himl,
    int            cx,
    int            cy
    )
{
    return pGweApiSet1Entrypoints->m_pSetIconSize(himl,cx,cy);
}

extern "C"
BOOL
WINAPI
ImageList_GetImageInfo(
    HIMAGELIST        himl,
    int                i,
    __out IMAGEINFO*    pImageInfo
    )
{
    return pGweApiSet1Entrypoints->m_pGetImageInfo(himl,i,pImageInfo,sizeof(IMAGEINFO));
}

extern "C"
HIMAGELIST
WINAPI
ImageList_Merge(
    HIMAGELIST    himl1,
    int            i1,
    HIMAGELIST    himl2,
    int            i2,
    int            dx,
    int            dy
    )
{
    return (HIMAGELIST)pGweApiSet1Entrypoints->m_pMerge(himl1, i1, himl2,i2,dx,dy);
}

extern "C"
void
ImageList_CopyDitherImage(
    HIMAGELIST    himlDest,
    WORD        iDst,
    int            xDst,
    int            yDst,
    HIMAGELIST    himlSrc,
    int            iSrc,
    UINT        fStyle
    )
{
    pGweApiSet1Entrypoints->m_pCopyDitherImage(himlDest,iDst,xDst,yDst,himlSrc,iSrc,fStyle);
}


extern "C"
BOOL
WINAPI
ImageList_Copy(
    HIMAGELIST    himlDst,
    int            iDst,
    HIMAGELIST    himlSrc,
    int            iSrc,
    UINT        uFlags
    )
{
    return pGweApiSet1Entrypoints->m_pCopy(himlDst, iDst, himlSrc, iSrc, uFlags);
}



extern "C"
HIMAGELIST
WINAPI
ImageList_Duplicate(
    HIMAGELIST    himl
    )
{
    return (HIMAGELIST)pGweApiSet1Entrypoints->m_pDuplicate(himl);
}



extern "C"
BOOL
WINAPI
ImageList_SetImageCount(
    HIMAGELIST    himl,
    UINT uNewCount
    )
{
    return pGweApiSet1Entrypoints->m_pSetImageCount(himl, uNewCount);
}


