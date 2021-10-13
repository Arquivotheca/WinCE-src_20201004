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
#include "TestCases.h"

HRESULT SetPixelFogStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex);
HRESULT SupportsPixelFogTableIndex(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex);
HRESULT SetupPixelFogGeometry(LPDIRECT3DMOBILEDEVICE pDevice, HWND hWnd, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB);

typedef enum _D3DQAPIXELFOGTYPE {
	D3DQAFOG_W = 1,
	D3DQAFOG_Z
} D3DQAPIXELFOGTYPE;

typedef struct _PIXEL_FOG_TESTS {
	D3DMCOLOR FogColor;
	D3DMFOGMODE FogMode;
	FLOAT FogStart;
	FLOAT FogEnd;
	FLOAT FogDensity;
	FLOAT fWNear;
	FLOAT fWFar;
	D3DQAPIXELFOGTYPE FogType;
	BOOL FogEnable;
} PIXEL_FOG_TESTS;

#define FLOAT_DONTCARE 1.0f

__declspec(selectany) PIXEL_FOG_TESTS PixelFogCases[D3DMQA_PIXELFOGTEST_COUNT] = {
// |          FogColor          |    FogMode    | FogStart | FogEnd |    FogDensity     |     fWNear     |     fWFar      |  FogType  |   FogEnable  |
// +----------------------------+---------------+----------+--------+-------------------+----------------+----------------+-----------+--------------+

// Pixel Z-Based Fog - Basic Case
{   D3DMCOLOR_XRGB(127,  0,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Basic Case, Fog Disabled via D3DMRS_FOGENABLE
{   D3DMCOLOR_XRGB(127,  0,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,        FALSE },

// Pixel W-Based Fog - Basic Case
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.3f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },

// Pixel W-Based Fog - Basic Case, Fog Disabled via D3DMRS_FOGENABLE
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.3f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,        FALSE },

// Pixel Z-Based Fog - Various start ranges
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.2f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.6f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.8f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Various end ranges
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    0.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    0.2f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    0.4f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    0.6f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    0.8f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Red
{   D3DMCOLOR_XRGB( 63,  0,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(127,  0,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(191,  0,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(255,  0,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Green
{   D3DMCOLOR_XRGB(  0, 63,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,127,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,191,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,255,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Blue
{   D3DMCOLOR_XRGB(  0,  0, 63),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,127),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,191),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Yellow
{   D3DMCOLOR_XRGB(255,255,  0),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Cyan
{   D3DMCOLOR_XRGB(  0,255,255),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - Magenta
{   D3DMCOLOR_XRGB(255,  0,255),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - EXP (various densities)
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.0f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.1f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.2f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.3f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.4f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.5f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.6f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.7f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.8f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      0.0f,    1.0f,               0.9f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel Z-Based Fog - EXP2 (various densities)
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.0f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.1f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.2f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.3f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.4f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.5f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.6f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.7f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.8f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      0.0f,    1.0f,               0.9f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_Z,         TRUE },

// Pixel W-Based Fog - Various starts
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.0f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.1f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.2f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.3f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },

// Pixel W-Based Fog - Various ends
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    0.6f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    0.7f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    0.8f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    0.9f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      0.4f,    1.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_W,         TRUE },

};

