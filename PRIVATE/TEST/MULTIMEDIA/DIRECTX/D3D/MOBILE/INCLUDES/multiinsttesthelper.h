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

#define D3DMMULTIINSTTEST_FVF_TEX (D3DMFVF_XYZ_FLOAT | D3DMFVF_NORMAL_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0) | D3DMFVF_DIFFUSE)

typedef struct _D3DMMULTIINSTTEST_VERT_TEX
{
    float x, y, z;
    float nx, ny, nz;
    D3DMCOLOR diffuse;
    float u, v;
} D3DMMULTIINSTTEST_VERT_TEX;

#define D3DMMULTIINSTTEST_FVF_NOTEX (D3DMFVF_XYZ_FLOAT | D3DMFVF_NORMAL_FLOAT | D3DMFVF_DIFFUSE)

typedef struct _D3DMMULTIINSTTEST_VERT_NOTEX
{
    float x, y, z;
    float nx, ny, nz;
    D3DMCOLOR diffuse;
} D3DMMULTIINSTTEST_VERT_NOTEX;

struct MultiInstOptions
{
    DWORD FVF;
    D3DMSHADEMODE Shading;
    BOOL VertexAlpha;
    BOOL TableFog;
    BOOL VertexFog;
    int InstanceCount;
    BOOL UseLocalInstances;
};

HRESULT MultiInstRenderScene(IDirect3DMobileDevice * pDevice, MultiInstOptions * pOptions);
