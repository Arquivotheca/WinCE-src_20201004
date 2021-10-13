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

#ifndef _GDI_FEEDBACKRECT_H_
#define _GDI_FEEDBACKRECT_H_

#if (_MSC_VER >= 1000) 
    #pragma once
#endif

//================================================================================
//  Include Files
//================================================================================
#include "..\common\feedbackrect.h"

//================================================================================
//  CGdiFeedbackRect Class Declaration
//================================================================================
class CGdiFeedbackRect : public CFeedbackRect
{
public:
    CGdiFeedbackRect();
    ~CGdiFeedbackRect();

    // Render methods.
    VOID Draw( CDrawContext * pDrawContext );

private:
    VOID StateUpdated( FEEDBACKRECT_STATE eUpdatedState );

};  // class CGdiFeedbackRect

#endif // _GDI_FEEDBACKRECT_H_