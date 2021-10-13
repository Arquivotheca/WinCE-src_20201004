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
#include "gdifeedbackrect.h"

#include <ehm.h>
#include "gdidrawcontext.h"

//================================================================================
//  CGdiFeedbackRect Member Function Definitions
//================================================================================
CGdiFeedbackRect::
CGdiFeedbackRect()
    : CFeedbackRect()
{    
}


CGdiFeedbackRect::
~CGdiFeedbackRect()
{
}


// Render methods.
VOID
CGdiFeedbackRect::
Draw(
    CDrawContext * pDrawContext
    )
{
    ASSERT( NULL != pDrawContext );

    // If not visible, don't draw anything.
    if ( !m_fVisible )
    {
        return;
    }

    CGdiDrawContext * pGdiDrawContext = (CGdiDrawContext *)pDrawContext;

    // Draw a focus rectangle corresponding to the feedback rectangle.
    DrawFocusRect( pGdiDrawContext->GetBackbufferDC(), &m_rcFeedbackRect );
}


VOID
CGdiFeedbackRect::
StateUpdated(
    CFeedbackRect::FEEDBACKRECT_STATE eUpdatedState
    )
{
    switch ( eUpdatedState )
    {
    case CFeedbackRect::FEEDBACKRECT_STATE_VISIBLE:
    case CFeedbackRect::FEEDBACKRECT_STATE_RECT:
    default:
        // Don't do anything.
        break;
    }
}
