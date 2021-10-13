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

#ifndef _GDI_COMPOSITOR_H_
#define _GDI_COMPOSITOR_H_

#if (_MSC_VER >= 1000) 
    #pragma once
#endif

//================================================================================
//  Include Files
//================================================================================
#include "..\common\compositor.h"

//================================================================================
//  CGdiCompositor Class Declaration
//================================================================================
class CGdiCompositor : public CCompositor
{
public:
    CGdiCompositor();
    ~CGdiCompositor();

    // These Init/Deinit functions are called within the render
    // thread's context.
    BOOL InitRender();
    VOID DeinitRender();

};  // class CGdiCompositor

#endif // _GDI_COMPOSITOR_H_