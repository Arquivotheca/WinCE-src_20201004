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
(*ACQUIRE_FILL_SURFACE) (
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex
    );

typedef
HRESULT 
(*GET_COLORFILL_RECTS)(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex,
    UINT uiIterations,
    LPRECT * ppRect,
    D3DMCOLOR * pColor);

