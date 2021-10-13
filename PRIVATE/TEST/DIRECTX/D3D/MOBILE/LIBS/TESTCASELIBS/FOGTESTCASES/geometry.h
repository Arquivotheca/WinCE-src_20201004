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
// FVF, structure
//
#define D3DMFOGTEST_FVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR)

typedef struct _D3DMFOGTEST_VERTS {
	float x, y, z, rhw;
	D3DMCOLOR Diffuse;
	D3DMCOLOR Specular;
} D3DMFOGTEST_VERTS;


//
// FVF, structure
//
#define D3DMFOGVERTTEST_FVF (D3DMFVF_XYZ_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR)

typedef struct _D3DMFOGVERTTEST_VERTS {
	float x, y, z;
	D3DMCOLOR Diffuse;
	D3DMCOLOR Specular;
} D3DMFOGVERTTEST_VERTS;


#define D3DQA_PRIMTYPE D3DMPT_TRIANGLELIST

//
// Grid characteristics
//
#define D3DQA_RECTS_X 10
#define D3DQA_RECTS_Y 10

//
// Grid of rectangles (2 primitives per square)
//
#define D3DQA_NUMQUADS (D3DQA_RECTS_X*D3DQA_RECTS_Y)

//
// Grid of quads (2 primitives per square)
//
#define D3DQA_NUMPRIM (D3DQA_NUMQUADS*2)

//
// Triangle lists use 3 vertices per primitive
//
#define D3DQA_NUMVERTS (3*D3DQA_NUMPRIM)

#define D3DQA_RHW (1.0f / 0.5f)
#define D3DQA_PI ((FLOAT) 3.141592654f)
