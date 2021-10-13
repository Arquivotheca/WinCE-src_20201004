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

#pragma once
#include <hgtypes.h>

// Forward class declarations
enum PixelFormat;

class CompositorApiSet_t
{
public:

    typedef
    void
    (*Compositor_NotifyCallback_t)(
        DWORD       cause,
        HPROCESS    hprc,
        HTHREAD     hthd
        );

    typedef
    HRESULT
    (*Compositor_CreateDesktop_t)(
        __in_bcount(cbDesktopParams) const HGCREATEDESKTOPPARAMS * pDesktopParams,
        size_t cbDesktopParams,
        __in_opt IHgMonitor * pMonitor,
        __out HHGCOMPOSITOR * phCompositor,
        __out HHGWINDOW * phCursor,
        __in HANDLE hMsgqWrite,
        __in HANDLE hMsgqRead,
        __in HANDLE hMtxPacketSend
        );

    typedef
    HRESULT
    (*Compositor_DestroyDesktop_t)(
        __in HHGCOMPOSITOR hCompositor
        );

    typedef
    HRESULT
    (*Compositor_CreateDisplaySurface_t)(
        __in HWND hwndCompositor,
        __out HHGDISPSURF * phDispSurf
        );

    typedef
    HRESULT
    (*Compositor_DestroyDisplaySurface_t)(
        __in HHGDISPSURF hDispSurf
        );

    typedef
    HRESULT
    (*Compositor_CreateDisplay_t)(
        __in_bcount(cbDisplayParams) const HGREGISTERDISPLAYPARAMS * pDisplayParams,
        size_t cbDisplayParams,
        __out HHGDISPLAY * phDisplay,
        __in_opt IHgMonitor * pMonitor
        );

    typedef
    HRESULT
    (*Compositor_DestroyDisplay_t)(
        __in HHGDISPLAY hDisplay
        );

    typedef
    HRESULT
    (*Compositor_ResumeDisplay_t)(
        __in HHGDISPLAY hDisplay
        );

    typedef
    HRESULT
    (*Compositor_SuspendDisplay_t)(
        __in HHGDISPLAY hDisplay
        );

    typedef
    HRESULT
    (*Compositor_AttachDisplayToDesktop_t)(
        __in HHGDISPLAY hDisplay,
        __in_opt HHGCOMPOSITOR hCompositor
        );

    typedef
    PixelFormat
    (*Compositor_GetSupportedPixelFormat_t)(
        __in PixelFormat fmtDesired
        );

    typedef
    HRESULT
    (*Compositor_CreateWindowSurface_t)(
        __in HWND hWnd,
        __out HHGCOMPSURF * phSurf
        );

    typedef
    HRESULT
    (*Compositor_CreateSurface_t)(
        __in void * pvBits,
        __in UINT cbBits,
        __in UINT uWidth,
        __in UINT uHeight,
        __in int nStride,
        __in PixelFormat fmt,
        __out HHGCOMPSURF * phSurf
        );

    Compositor_NotifyCallback_t                 m_pCompositor_NotifyCallback;                   //  0
    Compositor_CreateDesktop_t                  m_pCompositor_CreateDesktop;                    //  1
    Compositor_DestroyDesktop_t                 m_pCompositor_DestroyDesktop;                   //  2
    Compositor_CreateDisplaySurface_t           m_pCompositor_CreateDisplaySurface;             //  3
    Compositor_DestroyDisplaySurface_t          m_pCompositor_DestroyDisplaySurface;            //  4
    Compositor_CreateDisplay_t                  m_pCompositor_CreateDisplay;                    //  5
    Compositor_DestroyDisplay_t                 m_pCompositor_DestroyDisplay;                   //  6
    Compositor_ResumeDisplay_t                  m_pCompositor_ResumeDisplay;                    //  7
    Compositor_SuspendDisplay_t                 m_pCompositor_SuspendDisplay;                   //  8
    Compositor_AttachDisplayToDesktop_t         m_pCompositor_AttachDisplayToDesktop;           //  9
    Compositor_GetSupportedPixelFormat_t        m_pCompositor_GetSupportedPixelFormat;          //  10
    Compositor_CreateWindowSurface_t            m_pCompositor_CreateWindowSurface;              //  11
    Compositor_CreateSurface_t                  m_pCompositor_CreateSurface;                    //  12
};

