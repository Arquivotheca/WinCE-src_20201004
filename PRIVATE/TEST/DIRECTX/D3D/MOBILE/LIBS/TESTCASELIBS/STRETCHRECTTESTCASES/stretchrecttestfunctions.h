//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
(*GET_STRETCH_RECTS)(
    LPDIRECT3DMOBILESURFACE pSurfaceSource,
    LPDIRECT3DMOBILESURFACE pSurfaceDest,
    DWORD dwIteration,
    DWORD dwTableIndex,
    LPRECT pRectSource,
    LPRECT pRectDest);

typedef
HRESULT 
(*GET_FILTER_TYPE)(
    LPDIRECT3DMOBILEDEVICE pDevice,
    D3DMTEXTUREFILTERTYPE * d3dtft,
    DWORD dwTableIndex);

