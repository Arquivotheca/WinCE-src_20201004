//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

