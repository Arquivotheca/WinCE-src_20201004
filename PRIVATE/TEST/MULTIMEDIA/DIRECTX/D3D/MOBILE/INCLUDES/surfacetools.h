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
#pragma once

#define D3DMQA_MAXTEXTURESIZE 256

//
// Generates a set of RECTs that each cover the entire specified extents
//
HRESULT FullCoverageRECTs(RECT **ppRects, UINT uiNumRects, UINT uiWidth, UINT uiHeight);

//
// Copies the given surface to the first backbuffer of the given device
//
HRESULT CopySurfaceToBackBuffer(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPDIRECT3DMOBILESURFACE pSurface,
    BOOL fStretch);


typedef HRESULT (*PFNGETCOLORS)(
    D3DMFORMAT Format, 
    int iX, 
    int iY, 
    int Width, 
    int Height, 
    float * prRed,
    float * prGreen,
    float * prBlue,
    float * prAlpha);

//
// Generates a gradient across the given surface.
//
HRESULT SurfaceGradient(
    LPDIRECT3DMOBILESURFACE pSurface,
    PFNGETCOLORS pfnGetColors = NULL);

//
// Determines the intersection of the device extents and one or two surfaces.
//
HRESULT SurfaceIntersection(
    LPDIRECT3DMOBILESURFACE pBackbuffer, 
    LPDIRECT3DMOBILESURFACE pSurface1, 
    LPDIRECT3DMOBILESURFACE pSurface2, 
    RECT * pIntersection);

