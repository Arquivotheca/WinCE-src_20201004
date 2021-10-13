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
// Structure to conveniently pass around information about
// generated geometry
//
typedef struct _GEOMETRY_DESC {
	UINT uiPrimCount;
	UINT uiVertCount;
	LPDIRECT3DMOBILEVERTEXBUFFER *ppVB;
} GEOMETRY_DESC;

//
// These utilities require extents of at least 128x128; but to ensure usefulness on
// all reasonable devices, the utilities won't use extents beyond that
//
#define D3DQA_REQUIRED_SCREEN_EXTENT 128
#define D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X D3DQA_REQUIRED_SCREEN_EXTENT
#define D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y D3DQA_REQUIRED_SCREEN_EXTENT
#define D3DQA_NUM_LEVELS_EXPECTED 8
#define D3DQA_NUM_TEXTURE_REPEAT 16

#define D3DQA_REQUIRED_VIEWPORT_EXTENT (D3DQA_REQUIRED_SCREEN_EXTENT+5)

#define D3DQA_BIAS_MIN  -3.0f
#define D3DQA_BIAS_MAX   3.0f
#define D3DQA_BIAS_STEP 0.25f

#define D3DQA_FVF_TTEX_VERTS (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_TEX1)
typedef struct _TTEX_VERTS {
	float x,y,z,rhw;
	float tu, tv;
} TTEX_VERTS;


HRESULT SetupMipMapGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                            D3DMPRESENT_PARAMETERS* pPresentParms,
                            HWND hWnd,
                            GEOMETRY_DESC *pDesc,
                            UINT uiSquareExtent);

HRESULT SetupMipMapContents(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILETEXTURE *ppTexture);

HRESULT FillMipSurfaceLevel(LPDIRECT3DMOBILESURFACE pSurface, UINT uiLevel);


