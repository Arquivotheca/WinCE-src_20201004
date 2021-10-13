//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

//
// Forward declarations
//
BOOL VerifyTestCaseSupport(LPDIRECT3DMOBILEDEVICE pDevice,
                           D3DMFORMAT Format,
                           D3DMSTENCILOP StencilZFail,
                           D3DMSTENCILOP StencilFail,
                           D3DMSTENCILOP StencilPass,
                           D3DMCMPFUNC StencilFunc);

HRESULT MakeStencilGeometry(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB, D3DMCOLOR dwColor, float fZ);

HRESULT GetScreenSpaceClientCenter(HWND hWnd, LPPOINT pPoint);

//
// Geometry structure/type/extents
//
#define D3DQA_STENCILTEST_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE
struct D3DQA_STENCILTEST
{
    FLOAT x, y, z, rhw;
	DWORD Diffuse;
};
#define D3DQA_STENCILTEST_NUMVERTS 6
#define D3DQA_STENCILTEST_NUMPRIM 2


//
// Macros for obtaining components from D3DCOLORs
//
#define D3DQA_GETRED(_c) ((_c & 0x00FF0000) >> 16)
#define D3DQA_GETGREEN(_c) ((_c & 0x0000FF00) >> 8)
#define D3DQA_GETBLUE(_c) ((_c & 0x000000FF))
