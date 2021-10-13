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

#ifndef _GDI_WINDOWVISUAL_H_
#define _GDI_WINDOWVISUAL_H_

#if (_MSC_VER >= 1000) 
    #pragma once
#endif

//================================================================================
//  Include Files
//================================================================================
#include "..\common\windowvisual.h"

//================================================================================
//  Forward Declarations
//================================================================================
class CGdiDrawContext;

//================================================================================
//  CGdiWindowVisual Class Declaration
//================================================================================
class CGdiWindowVisual : public CWindowVisual
{
public:
    CGdiWindowVisual( HWND hwnd );
    ~CGdiWindowVisual();

    VOID Draw( CDrawContext * pDrawContext );

private:
    VOID DrawAsOverlayTarget( CGdiDrawContext * pDrawContext );
    VOID StateUpdated( CWindowVisual::WINDOW_STATE eUpdatedState );

private:
    BOOL            m_fUseAlphaBlt;

};  // class CGdiWindowVisual

#endif // _GDI_WINDOWVISUAL_H_