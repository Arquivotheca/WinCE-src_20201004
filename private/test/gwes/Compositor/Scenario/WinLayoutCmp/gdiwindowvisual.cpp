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

//================================================================================
//  Include Files
//================================================================================
#include "gdiwindowvisual.h"

#include <ehm.h>

#include "gdidrawcontext.h"

//================================================================================
//  CGdiWindowVisual Member Function Definitions
//================================================================================
CGdiWindowVisual::
CGdiWindowVisual( HWND hwnd )
    : CWindowVisual( hwnd )
    , m_fUseAlphaBlt( FALSE )
{
}


CGdiWindowVisual::
~CGdiWindowVisual()
{
}


VOID
CGdiWindowVisual::
Draw(
    CDrawContext * pDrawContext
    )
{
    ASSERT( NULL != pDrawContext );

    HRESULT hr = E_FAIL;
    HBITMAP hDefaultBitmap = NULL;
    HRGN hrgnDraw = NULL;

    CGdiDrawContext * pGdiDrawContext = (CGdiDrawContext *)pDrawContext;

    // First check to see if this window is an overlay target.  If it
    // is, draw the window as a cut-out of the primary surface and
    // then return. Otherwise, draw it normally.
    if ( WCF_OVERLAYTARGET & m_dwCompFlags )
    {
        DrawAsOverlayTarget( pGdiDrawContext );
        return;
    }

    // If the window's surface is NULL, then just fill its draw region with black.
    if ( NULL == m_hSurface )
    {
        FillRgn( pGdiDrawContext->GetBackbufferDC(), m_hrgnDraw, (HBRUSH)GetStockObject( BLACK_BRUSH ) );
        return;
    }

    // Select the window's surface into the compatible DC.
    hDefaultBitmap = (HBITMAP)SelectObject( pGdiDrawContext->GetCompatibleDC(), m_hSurface );
    CBRA( NULL != hDefaultBitmap );

    // Set the clip region into the DC prior to blitting.
    SelectClipRgn( pGdiDrawContext->GetBackbufferDC(), m_hrgnDraw );

    // Bit blit the window's surface into the primary surface. If the window is
    // transparent use the AlphaBlend function to blit. Otherwise, use the
    // BitBlt function to blit.
    if ( m_fUseAlphaBlt )
    {
        BLENDFUNCTION sBlendFunc = { 0 };

        // Configure the blend function.
        sBlendFunc.BlendOp = AC_SRC_OVER;
        sBlendFunc.SourceConstantAlpha = m_nOpacity;
        sBlendFunc.AlphaFormat =
            ( m_dwCompFlags & WCF_ALPHATRANSPARENCY ? AC_SRC_ALPHA : 0 );

        // Perform the alpha blended bit blit.
        BOOL bRetval = AlphaBlend(
                            pGdiDrawContext->GetBackbufferDC(),
                            m_rcWindowRect.left, m_rcWindowRect.top,
                            m_rcWindowRect.right - m_rcWindowRect.left,
                            m_rcWindowRect.bottom - m_rcWindowRect.top,
                            pGdiDrawContext->GetCompatibleDC(),
                            0, 0,
                            m_rcWindowRect.right - m_rcWindowRect.left,
                            m_rcWindowRect.bottom - m_rcWindowRect.top,
                            sBlendFunc
                            );
    }
    else
    {
        // Use regular bit blitting.
        TCHAR szTitle[MAX_PATH];
        GetClassName(m_hwnd, szTitle, MAX_PATH);
        int nScrWidth = GetSystemMetrics( SM_CXSCREEN );
        int nScrHeight = GetSystemMetrics( SM_CYSCREEN );

        if (0 == lstrcmp(szTitle, TEXT("Navigation app Class") )
            &&(0 != m_rcWindowRect.left || 0 != m_rcWindowRect.top
               || nScrWidth/2 != m_rcWindowRect.right - m_rcWindowRect.left
               || nScrHeight/2 != m_rcWindowRect.bottom - m_rcWindowRect.top))
        {            
            MoveWindow(m_hwnd, 0, 0, nScrWidth/2, nScrHeight/2, TRUE);
        }
        else
        {            
            BitBlt(
                pGdiDrawContext->GetBackbufferDC(),
                m_rcWindowRect.left, m_rcWindowRect.top,
                m_rcWindowRect.right - m_rcWindowRect.left,
                m_rcWindowRect.bottom - m_rcWindowRect.top,
                pGdiDrawContext->GetCompatibleDC(),
                0, 0, SRCCOPY 
                );
        }
    }

    // Unset the clip region.
    SelectClipRgn( pGdiDrawContext->GetBackbufferDC(), NULL );

Error:
    if ( NULL != hDefaultBitmap )
    {
        SelectObject( pGdiDrawContext->GetCompatibleDC(), hDefaultBitmap );
        hDefaultBitmap = NULL;
    }
}


VOID
CGdiWindowVisual::
DrawAsOverlayTarget(
    CGdiDrawContext * pDrawContext
    )
{
    ASSERT( NULL != pDrawContext );

    HRESULT hr = E_FAIL;
    HBRUSH hbrZeroAlpha = NULL;

    // Create the zero alpha brush.
    hbrZeroAlpha = CreateSolidBrush( 0x00000000 );
    CBR( NULL != hbrZeroAlpha );

    FillRgn(
        pDrawContext->GetBackbufferDC(),
        m_hrgnDraw,
        hbrZeroAlpha
        );

    // Success.
    hr = S_OK;

Error:
    if ( NULL != hbrZeroAlpha )
    {
        DeleteObject( hbrZeroAlpha );
        hbrZeroAlpha = NULL;
    }
}


VOID
CGdiWindowVisual::
StateUpdated(
    CWindowVisual::WINDOW_STATE eUpdatedState
    )
{
    switch ( eUpdatedState )
    {
    case WINDOW_STATE_COMPFLAGS:
    case WINDOW_STATE_OPACITY:
        m_fUseAlphaBlt = ( m_nOpacity < 255 || m_dwCompFlags & WCF_ALPHATRANSPARENCY ? TRUE : FALSE );
        break;

    case WINDOW_STATE_RECT:
    case WINDOW_STATE_ZORDER:
    case WINDOW_STATE_CLIP:
    case WINDOW_STATE_REGION:
    case WINDOW_STATE_DRAWREGION:
    case WINDOW_STATE_CHILDREN:
    case WINDOW_STATE_TOPLEVEL:
    case WINDOW_STATE_VISIBILITY:
    case WINDOW_STATE_SURFACE:
    case WINDOW_STATE_INVALIDATED:
    default:
        // Do Nothing.
        break;
    }
}
