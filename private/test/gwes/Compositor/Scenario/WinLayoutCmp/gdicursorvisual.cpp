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
#include "gdicursorvisual.h"

#include <ehm.h>
#include "gdidrawcontext.h"

//================================================================================
//  CGdiCursorVisual Member Function Definitions
//================================================================================
CGdiCursorVisual::
CGdiCursorVisual()
    : CCursorVisual()
{
}


CGdiCursorVisual::
~CGdiCursorVisual()
{
}


// Render methods.
VOID
CGdiCursorVisual::
Draw(
    CDrawContext * pDrawContext
    )
{
    ASSERT( NULL != pDrawContext );

    // If not enabled, don't draw anything.
    if ( !m_fEnabled )
        return;

    HRESULT hr = E_FAIL;
    HBITMAP hDefaultBitmap = NULL;
    BLENDFUNCTION sBlendFunc = { 0 };
    CGdiDrawContext * pGdiDrawContext = (CGdiDrawContext *)pDrawContext;

    // Select the cursor's surface into the compatible DC.
    hDefaultBitmap = (HBITMAP)SelectObject( pGdiDrawContext->GetCompatibleDC(), m_hSurface );
    CBRA( NULL != hDefaultBitmap );

    // Configure the blend function.
    sBlendFunc.BlendOp = AC_SRC_OVER;
    sBlendFunc.SourceConstantAlpha = 255;
    sBlendFunc.AlphaFormat = AC_SRC_ALPHA;

    // Perform the alpha blended bit blit.
    AlphaBlend(
            pGdiDrawContext->GetBackbufferDC(),
            m_ptPosition.x - m_ptHotspot.x,
            m_ptPosition.y - m_ptHotspot.y,
            m_szSurface.cx, m_szSurface.cy,
            pGdiDrawContext->GetCompatibleDC(),
            0, 0,
            m_szSurface.cx, m_szSurface.cy,
            sBlendFunc
            );

Error:
    if ( NULL != hDefaultBitmap )
    {
        SelectObject( pGdiDrawContext->GetCompatibleDC(), hDefaultBitmap );
        hDefaultBitmap = NULL;
    }
}


VOID
CGdiCursorVisual::
StateUpdated(
    CCursorVisual::CURSOR_STATE eUpdatedState
    )
{
    switch( eUpdatedState )
    {
    case CCursorVisual::CURSOR_STATE_ENABLE:
    case CCursorVisual::CURSOR_STATE_POSITION:
    case CCursorVisual::CURSOR_STATE_SURFACE:
    default:
        // Don't do anything.
        break;
    }
}
