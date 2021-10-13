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
#include <windows.h>
#include <d3dm.h>

//
// Vertex type useful for testing view-frustum-clipping
//
#define CUSTOM_VERTEX_FVF (D3DMFVF_XYZ_FLOAT|D3DMFVF_DIFFUSE|D3DMFVF_SPECULAR)
struct CUSTOM_VERTEX
{
    FLOAT x, y, z;        // The untransformed position for the vertex
    D3DMCOLOR dwDiffuse;   // The diffuse vertex color
    D3DMCOLOR dwSpecular;  // The specular vertex color
};

//
// Adjust viewport extents to allow for a blank "border" around rendered area
//
HRESULT ShrinkViewport(LPDIRECT3DMOBILEDEVICE pDevice, float fWidthMult, float fHeightMult);

//
// Writes vertex parameters to FVF structure
//
void FillUntransformedVertex(CUSTOM_VERTEX *pVertex, float x, float y, float z, DWORD dwDiffuse, DWORD dwSpecular);

//
// Provides a linearly-interpolated result color, given two inputs and a
// interpolator
//
DWORD InterpColor(float fLineInterp, DWORD dwColor1, DWORD dwColor2);
