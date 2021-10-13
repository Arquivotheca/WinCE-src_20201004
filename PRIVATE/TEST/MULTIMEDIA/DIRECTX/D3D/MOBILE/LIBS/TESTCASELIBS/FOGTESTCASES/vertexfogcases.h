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

HRESULT SetVertexFogStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex);
HRESULT SupportsVertexFogTableIndex(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex);
HRESULT SetupVertexFogGeometry(LPDIRECT3DMOBILEDEVICE pDevice, HWND hWnd, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB);

typedef enum _D3DQAVERTFOGTYPE {
	D3DQAFOG_DEPTH = 1,
	D3DQAFOG_RANGE
} D3DQAVERTFOGTYPE;

typedef struct _VERTEX_FOG_TESTS {
	D3DMCOLOR FogColor;
	D3DMFOGMODE FogMode;
	FLOAT FogStart;
	FLOAT FogEnd;
	FLOAT FogDensity;
	FLOAT fWNear;
	FLOAT fWFar;
	D3DQAVERTFOGTYPE FogType;
	BOOL FogEnable;
} VERTEX_FOG_TESTS;

#define FLOAT_DONTCARE 1.0f

__declspec(selectany) VERTEX_FOG_TESTS VertexFogCases[D3DMQA_VERTEXFOGTEST_COUNT] = {
// |          FogColor          |    FogMode    | FogStart | FogEnd |    FogDensity     |     fWNear     |     fWFar      |    FogType    |   FogEnable  |
// +----------------------------+---------------+----------+--------+-------------------+----------------+----------------+---------------+--------------+

// Custom Fog
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_NONE,      1.0f,    10.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Custom Fog setup, Fog disabled via D3DMRS_FOGENABLE
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_NONE,      1.0f,    10.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,        FALSE },

// Vertex Depth-Based Fog - Basic Case
{   D3DMCOLOR_XRGB(127,  0,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Basic Case, Fog disabled via D3DMRS_FOGENABLE
{   D3DMCOLOR_XRGB(127,  0,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,        FALSE },

// Vertex Depth-Based Fog - Various Ranges								 
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,     2.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      2.0f,     3.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      3.0f,     4.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      4.0f,     5.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      5.0f,     6.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      6.0f,     7.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      7.0f,     8.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      8.0f,     9.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      9.0f,    10.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,     10.0f,    11.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,     5.5f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      5.5f,    11.0f,    FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Red
{   D3DMCOLOR_XRGB( 63,  0,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(127,  0,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(191,  0,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(255,  0,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Green
{   D3DMCOLOR_XRGB(  0, 63,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,127,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,191,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,255,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Blue
{   D3DMCOLOR_XRGB(  0,  0, 63),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,127),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,191),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Yellow
{   D3DMCOLOR_XRGB(255,255,  0),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Cyan
{   D3DMCOLOR_XRGB(  0,255,255),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - Magenta
{   D3DMCOLOR_XRGB(255,  0,255),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - EXP (various densities)
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.0f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.1f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.2f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.3f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.4f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.5f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.6f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.7f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.8f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),       D3DMFOG_EXP,      1.0f,   11.0f,               0.9f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Depth-Based Fog - EXP2 (various densities)
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.0f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.1f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.2f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.3f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.4f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.5f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.6f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.7f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.8f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),      D3DMFOG_EXP2,      1.0f,   11.0f,               0.9f,  FLOAT_DONTCARE,  FLOAT_DONTCARE, D3DQAFOG_DEPTH,         TRUE },

// Vertex Range-Based Fog (various starts)
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,   11.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      2.0f,   11.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      3.0f,   11.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      4.0f,   11.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      5.0f,   11.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },

// Vertex Range-Based Fog (various ends)
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,    6.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,    7.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,    8.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,    9.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },
{   D3DMCOLOR_XRGB(  0,  0,255),    D3DMFOG_LINEAR,      1.0f,   10.0f,     FLOAT_DONTCARE,            0.0f,            1.0f, D3DQAFOG_RANGE,         TRUE },

};

