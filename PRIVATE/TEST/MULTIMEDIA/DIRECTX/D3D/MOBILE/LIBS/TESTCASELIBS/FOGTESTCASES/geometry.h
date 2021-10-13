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
