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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "d3dm.h"

typedef
HRESULT
(*ACQUIRE_DEST_SURFACE) (
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex
    );

typedef
HRESULT
(*ACQUIRE_SOURCE_SURFACE) (
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex
    );

typedef
HRESULT 
(*SETUP_SOURCE_SURFACE)(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex);

typedef
HRESULT 
(*GET_COPY_RECTS)(
    LPDIRECT3DMOBILESURFACE pSurfaceSource,
    LPDIRECT3DMOBILESURFACE pSurfaceDest,
    DWORD dwTableIndex,
    LPRECT rgRectSource,
    LPPOINT rgPointDest,
    UINT * pRectCount,
    UINT * pPointCount);

