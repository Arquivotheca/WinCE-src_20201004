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
#define D3DM_LIGHTTEST_FVF (D3DMFVF_XYZ_FLOAT | D3DMFVF_NORMAL_FLOAT | D3DMFVF_DIFFUSE | D3DMFVF_SPECULAR)

typedef struct _D3DM_LIGHTTEST_VERTS {
	float x, y, z;
	float nx, ny, nz;
	D3DMCOLOR Diffuse;
	D3DMCOLOR Specular;
} D3DM_LIGHTTEST_VERTS, *LPD3DM_LIGHTTEST_VERTS;


#define D3DQA_PRIMTYPE D3DMPT_TRIANGLELIST

//
// Grid characteristics
//
#define D3DQA_QUADS_X 8
#define D3DQA_QUADS_Y 8

//
// Grid of quads (2 primitives per square)
//
#define D3DQA_NUMQUADS (D3DQA_QUADS_X*D3DQA_QUADS_Y)

//
// Grid of quads (2 primitives per square)
//
#define D3DQA_NUMPRIM (D3DQA_NUMQUADS*2)

//
// Number of vers per quad
//
#define D3DQA_VERTS_PER_QUAD (3*2) // Tri list of two tris

//
// Triangle lists use 3 vertices per primitive
//
#define D3DQA_NUMVERTS (3*D3DQA_NUMPRIM)

#define D3DQA_PI ((FLOAT) 3.141592654f)


//
// World extents within which the geometry will exist.
//
// Geometry will be centered in the Z range listed below.
//
#define WORLD_X_MIN -5.0f
#define WORLD_X_MAX  5.0f
#define WORLD_X_CENTER  ((WORLD_X_MIN + WORLD_X_MAX) / 2.0f)

#define WORLD_Y_MIN -5.0f
#define WORLD_Y_MAX  5.0f
#define WORLD_Y_CENTER  ((WORLD_Y_MIN + WORLD_Y_MAX) / 2.0f)

#define WORLD_Z_MIN -5.0f
#define WORLD_Z_MAX  5.0f
#define WORLD_Z_CENTER  ((WORLD_Z_MIN + WORLD_Z_MAX) / 2.0f)

#define _M(_a) D3DM_MAKE_D3DMVALUE(_a)


HRESULT SetupRotatedQuad(LPD3DM_LIGHTTEST_VERTS pVerts,
                         float fAngleX, float fAngleY, float fAngleZ,
						 D3DMCOLOR TriDiffuse1, D3DMCOLOR TriSpecular1,
						 D3DMCOLOR TriDiffuse2, D3DMCOLOR TriSpecular2
						 );

HRESULT SetupOffsetQuad(LPD3DM_LIGHTTEST_VERTS pVerts,
                        float fX, float fY, float fZ);

HRESULT SetupScaleQuad(LPD3DM_LIGHTTEST_VERTS pVerts,
                       float fX, float fY, float fZ);

