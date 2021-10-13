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

/// <file_topic_scope tref="WindowCompositionInternal"/>

#pragma once
#include <winddi.h>

// Forward class declarations:
__interface IHgMonitor;

// Surface allocation flags
#define HGALLOC_SHARED  (0x00000001)


DECLARE_HANDLE(HHGWND);         // Wraps an HgWindowHost object (HWND, HHGWINDOW & HHGDESKTOP)
DECLARE_HANDLE(HHGSURF);        // Wraps an IWindowSurface object
DECLARE_HANDLE(HHGDESKTOP);     // Wraps an IHgDesktop object
DECLARE_HANDLE(HHGDISPLAY);     // Wraps an IHgRasterizer object

DECLARE_HANDLE(HHGWINDOW);      // Wraps an IDependencyObject object (in/out of proc)
DECLARE_HANDLE(HHGCOMPSURF);    // Wraps in IPALSurface object (in/out of proc)
DECLARE_HANDLE(HHGCOMPOSITOR);  // Wraps an HgDesktop object (in/out of proc)
DECLARE_HANDLE(HHGDISPSURF);    // Wraps an IHgDisplaySurface object (in/out of proc)

typedef struct tagHGCREATESTRUCT 
{
    int         cy;
    int         cx;
    int         y;
    int         x;
} HGCREATESTRUCT;

typedef struct tagHGREGISTERDISPLAYPARAMS
{
    HHGDISPSURF hHgDisplaySurface;
    DWORD dwFrameInterval;
    int iThreadPriority;
    HANDLE hFrameCompositedEvent;
} HGREGISTERDISPLAYPARAMS;

typedef struct tagHGCREATEDESKTOPPARAMS
{
    SIZE  sizeDesktop;
} HGCREATEDESKTOPPARAMS;

typedef struct tagSURFOBJEX
{
    SURFOBJ so;
    LONG    lXDelta;
    DWORD   dwSurfaceSize;
} SURFOBJEX;
