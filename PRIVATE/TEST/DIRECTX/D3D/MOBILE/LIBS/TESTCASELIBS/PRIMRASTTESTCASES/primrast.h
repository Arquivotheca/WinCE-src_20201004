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
// Rendering function enumeration
//
typedef enum _RENDER_FUNC {
	D3DM_DRAWPRIMITIVE = 0,
	D3DM_DRAWINDEXEDPRIMITIVE,
} RENDER_FUNC;

//
// Test can render partial primitive set
//
typedef struct _DRAW_RANGE {
	float fLow;
	float fHigh;
} DRAW_RANGE;


//
// Vertex specification for this test
//
#define PRIMRASTFVF_FLT (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE)
#define PRIMRASTFVF_FXD (D3DMFVF_XYZRHW_FIXED | D3DMFVF_DIFFUSE)
typedef struct _PRIMRASTVERT {

    union {
        float fx;
        ULONG ulx;
    };

    union {
        float fy;
        ULONG uly;
    };

    union {
        float fz;
        ULONG ulz;
    };

    union {
        float frhw;
        ULONG ulrhw;
    };

	DWORD Diffuse;
} PRIMRASTVERT, *LPPRIMRASTVERT;



//
// Color Write Mask ('X' denotes channel that will not be written)
//
typedef enum _COLOR_WRITE_MASK {
	COLOR_WRITE_RGB = 0, 
	COLOR_WRITE_XGB, 
	COLOR_WRITE_RXB, 
	COLOR_WRITE_RGX, 
	COLOR_WRITE_XXB, 
	COLOR_WRITE_RXX, 
	COLOR_WRITE_XGX, 
	COLOR_WRITE_XXX,
} COLOR_WRITE_MASK, *LPCOLOR_WRITE_MASK;

//
// Geometry generation considerations:
//
//   * Keep vertex buffer sizes reasonable
//   * Allow partial buffer rendering (DP, DIP args)
//   * Pick vertex colors that are good for both flat and gouraud shade testing
//   * GIQ Conformance
//

//
// Primitive Drawing Test Function (D3DMPT_POINT)
//
HRESULT DrawPoints(LPDIRECT3DMOBILEDEVICE pDevice,
                   RENDER_FUNC RenderFunc,
                   DRAW_RANGE DrawRange,
				   D3DMFORMAT Format,
                   RECT RectTarget);

//
// Primitive Drawing Test Functions (D3DMPT_LINELIST, D3DMPT_LINESTRIP)
//
HRESULT DrawLineList(LPDIRECT3DMOBILEDEVICE pDevice,
                     RENDER_FUNC RenderFunc,
                     DRAW_RANGE DrawRange,
 				     D3DMFORMAT Format,
                     RECT RectTarget);

HRESULT DrawLineStrip(LPDIRECT3DMOBILEDEVICE pDevice,
                      RENDER_FUNC RenderFunc,
                      DRAW_RANGE DrawRange,
				      D3DMFORMAT Format,
                      RECT RectTarget);

//
// Primitive Drawing Test Function (D3DMPT_TRIANGLELIST, D3DMPT_TRIANGLESTRIP, D3DMPT_TRIANGLEFAN)
//
HRESULT DrawTriangleList(LPDIRECT3DMOBILEDEVICE pDevice,
                         RENDER_FUNC RenderFunc,
                         DRAW_RANGE DrawRange,
				         D3DMFORMAT Format,
                         RECT RectTarget);

HRESULT DrawTriangleStrip(LPDIRECT3DMOBILEDEVICE pDevice,
                          RENDER_FUNC RenderFunc,
                          DRAW_RANGE DrawRange,
				          D3DMFORMAT Format,
                          RECT RectTarget);

HRESULT DrawTriangleFan(LPDIRECT3DMOBILEDEVICE pDevice,
                        RENDER_FUNC RenderFunc,
                        DRAW_RANGE DrawRange,
				        D3DMFORMAT Format,
                        RECT RectTarget);

//
// Pick a vertex color based on position
//
VOID ColorVertex(D3DMCOLOR *pColor, UINT uiX, UINT uiY);


//
// Wrapper for DrawIndexedPrimitive, that eliminates some complexity
// by passing trivially computed values for arguments that might allow
// driver optimizations.
//
HRESULT SimpleDIP(LPDIRECT3DMOBILEDEVICE pDevice,
                  LPDIRECT3DMOBILEVERTEXBUFFER pVB,
                  D3DMPRIMITIVETYPE Type,
                  UINT StartIndex,
                  UINT PrimitiveCount);
