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
#include <windows.h>

#include "..\common\compositor.h"
#include "..\common\compositordbg.h"

#include "gdicompositor.h"
#include "gdicursorvisual.h"
#include "gdidrawcontext.h"
#include "gdifeedbackrect.h"
#include "gdiwindowvisual.h"

//================================================================================
//  Global Variables
//================================================================================
static CCompositor * g_pCompositor = NULL;

// Debug Zone Configuration.
DBGPARAM dpCurSettings = {
    // Module name.
    TEXT("COMPOSITOR"),

    // Debug zone names.
    {
        TEXT("ERROR"),
        TEXT("WARNING"),
        TEXT("INFO"),
        TEXT("TRACE"),
        TEXT("FUNCTION"),
        TEXT("NOTIFY"),
        TEXT("PERF"),
        TEXT("VERBOSE")
    },

    // Initial debug zone mask.
    ZONEMASK_ERROR | ZONEMASK_WARNING
};

//================================================================================
//  Plugin Entry Point
//================================================================================
BOOL WINAPI
DllMain(
    HANDLE hInstDll,
    DWORD dwReason,
    LPVOID lpReserved
    )
{
    switch ( dwReason )
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( (HMODULE)hInstDll );

        // Register debug zones.
        RegisterDbgZones( (HMODULE)hInstDll, &dpCurSettings );
        break;

    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

//================================================================================
//  Plugin Function Definitions
//================================================================================
EXTERN_C
BOOL
Compositor_Initialize(
    __in HWND hwndCompositor
    )
{
    g_pCompositor = CCompositor::Create();

    if ( NULL == g_pCompositor )
    {
        DBGMSG_ERROR( TEXT("Failed to create compositor object.") );
        ASSERT( 0 );
        return FALSE;
    }

    return g_pCompositor->Initialize( hwndCompositor );
}


EXTERN_C
VOID
Compositor_Deinitialize()
{
    if ( NULL == g_pCompositor )
    {
        DBGMSG_ERROR( TEXT("Invalid compositor object.") );
        ASSERT( 0 );
        return;
    }

    g_pCompositor->Deinitialize();
}


EXTERN_C
VOID
Compositor_HandleNotify(
    __in COMPOSITOR_NOTIFY eNotify,
    __deref_in_bcount(cbData) LPVOID lpData,
    __in UINT cbData
    )
{
    if ( NULL == g_pCompositor )
    {
        DBGMSG_ERROR( TEXT("Invalid compositor object.") );
        ASSERT( 0 );
        return;
    }

    g_pCompositor->HandleCompositorNotify( eNotify, lpData, cbData );
}


EXTERN_C
VOID
Compositor_ComposeScene()
{
    if ( NULL == g_pCompositor )
    {
        DBGMSG_ERROR( TEXT("Invalid compositor object.") );
        ASSERT( 0 );
        return;
    }

    g_pCompositor->ComposeScene();
}


EXTERN_C
BOOL
Compositor_InitRender()
{
    if ( NULL == g_pCompositor )
    {
        DBGMSG_ERROR( TEXT("Invalid compositor object.") );
        ASSERT( 0 );
        return FALSE;
    }

    return ( g_pCompositor->InitRender() ? TRUE : FALSE );
}


EXTERN_C
VOID
Compositor_DeinitRender()
{
    if ( NULL == g_pCompositor )
    {
        DBGMSG_ERROR( TEXT("Invalid compositor object.") );
        ASSERT( 0 );
        return;
    }

    g_pCompositor->DeinitRender();
}


//================================================================================
//  Factory Method Definitions
//================================================================================
CCompositor *
CCompositor::Create()
{
    return new CGdiCompositor();
}


CCursorVisual *
CCursorVisual::
Create()
{
    return new CGdiCursorVisual();
}


CDrawContext *
CDrawContext::Create()
{
    return new CGdiDrawContext();
}


CFeedbackRect *
CFeedbackRect::Create()
{
    return new CGdiFeedbackRect();
}


CWindowVisual *
CWindowVisual::
Create( HWND hwnd )
{
    return new CGdiWindowVisual( hwnd );
}
