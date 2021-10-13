//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
