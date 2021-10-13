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

#ifndef _GDI_DRAWCONTEXT_H_
#define _GDI_DRAWCONTEXT_H_

#if (_MSC_VER >= 1000) 
    #pragma once
#endif

//================================================================================
//  Include Files
//================================================================================
#include "..\common\drawcontext.h"

#include <list.hxx>

//================================================================================
// Forward Declarations
//================================================================================
class CGdiWindowVisual;

//================================================================================
//  CDrawContext Class Declaration
//================================================================================
class CGdiDrawContext : public CDrawContext
{
public:
    typedef ce::list<CGdiWindowVisual *>  WindowDrawList;

public:
    CGdiDrawContext();
    ~CGdiDrawContext();

    BOOL Initialize( HWND hwndCompositor );
    VOID Deinitialize();

    VOID Predraw();
    VOID Postdraw();

    HDC GetBackbufferDC() const
    {
        return m_hdcBackbuffer;
    }

    HDC GetCompatibleDC() const
    {
        return m_hdcCompatible;
    }

private:
    VOID StateUpdated( CDrawContext::DRAWCONTEXT_STATE eUpdatedState );

private:
    HWND                    m_hwndCompositor;

    HDC                     m_hdcPrimarySurf;
    HDC                     m_hdcBackbuffer;
    HDC                     m_hdcCompatible;

    HBITMAP                 m_hBackbuffer;
    HBITMAP                 m_hDefault;     // Default bitmap of the backbuffer DC.

    HRGN                    m_hrgnPostDraw;

};  // class CGdiDrawContext

#endif // _GDI_DRAWCONTEXT_H_