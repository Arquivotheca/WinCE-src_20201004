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

#ifndef _COMPOSITOR_API_SET_H_
#define _COMPOSITOR_API_SET_H_

#if (_MSC_VER >= 1000) 
    #pragma once
#endif

//================================================================================
//  Include Files
//================================================================================
#include <windows.h>
#include <compositorapi.h>

//================================================================================
//  Preprocessor Definitions
//================================================================================
#define MAX_PAYLOAD_SIZE    32

//================================================================================
//  Function Pointer Definitons
//================================================================================
struct CompositorApiSet_t
{
public:
    typedef VOID (*PFNNOTIFYCALLBACK)(
        __in DWORD,
        __in HPROCESS,
        __in HTHREAD
        );

    typedef BOOL (*PFNREGISTERCOMPOSITOR)(
        __in HWND,
        __in HANDLE
        );

    typedef BOOL (*PFNUNREGISTERCOMPOSITOR)(
        );

    typedef HWND (*PFNGETCOMPOSITORWINDOW)(
        );

    typedef BOOL (*PFNBEGINWINDOWCOMPOSITION)(
        __in HWND
        );

    typedef BOOL (*PFNENDWINDOWCOMPOSITION)(
        __in HWND
        );

    typedef HBITMAP (*PFNGETWINDOWSURFACE)(
        __in HWND
        );

    typedef BOOL (*PFNGETWINDOWSURFACEINVALIDRGN)(
        __in HWND hwnd,
        __in HRGN hrgn
        );

public:
    //                                                              API Set ID
    // Not an external API.
    PFNNOTIFYCALLBACK               pfnNotifyCallback;                  // 0

    // Compositor APIs.
    PFNREGISTERCOMPOSITOR           pfnRegisterCompositor;              // 1
    PFNUNREGISTERCOMPOSITOR         pfnUnregisterCompositor;            // 2
    PFNGETCOMPOSITORWINDOW          pfnGetCompositorWindow;             // 3
    PFNBEGINWINDOWCOMPOSITION       pfnBeginWindowComposition;          // 4
    PFNENDWINDOWCOMPOSITION         pfnEndWindowComposition;            // 5
    PFNGETWINDOWSURFACE             pfnGetWindowSurface;                // 6
    PFNGETWINDOWSURFACEINVALIDRGN   pfnGetWindowSurfaceInvalidRgn;      // 7

};  // struct CompositorApiSet_t


struct CompositorNotify_t
{
    COMPOSITOR_NOTIFY   eNotify;

    BYTE                bPayload[MAX_PAYLOAD_SIZE];
    UINT                cbPayload;

}; // struct CompositorNotify_t

#endif // _COMPOSITOR_API_H_