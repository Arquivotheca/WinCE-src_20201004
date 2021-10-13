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
#include <windows.h>
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "OverDrawPermutations.h"
#include "DebugOutput.h"
#include "BufferTools.h"
#include "DebugOutput.h"


//
// Vertex specification for this test
//
#define OVERDRAWFVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE)
typedef struct _OVERDRAWVERT {
	float x,y,z,rhw;
	DWORD Diffuse;
} OVERDRAWVERT, *LPOVERDRAWVERT;


#define D3DMQA_OVERDRAW_NUM_PRIMS 3


//
// DrawGeometry
//
//
// Arguments:
//   
//   LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//   RECT RectTarget:  Area to render primitives into
//
// Return Value:
// 
//   HRESULT indicates success or failure
//
HRESULT DrawGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                     RECT RectTarget, UINT uiTableIndex)
{
	//
	// Primitive type
	//
	D3DMPRIMITIVETYPE PrimType;

	//
	// Buffers
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

	//
	// Result of function; updated in all failure conditions
	//
	HRESULT hr = S_OK;

	//
	// Vertex structure pointer
	//
	LPOVERDRAWVERT pVertices = NULL;

	//
	// Number of verts in VB
	//
	CONST UINT uiNumVerts = 6;
	UINT uiNumPrims;

	//
	// Render target width, height
	//
	LONG lWidth;
	LONG lHeight;

	//
	// Allocate storage for vertices
	//
	pVertices = (LPOVERDRAWVERT)malloc(sizeof(OVERDRAWVERT) * uiNumVerts);
	if (NULL == pVertices)
	{
		DebugOut(DO_ERROR, L"malloc failed.");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CULLMODE,OverDrawTests[uiTableIndex].CullMode)))
	{
		DebugOut(DO_ERROR, L"SetRenderState failed for D3DMRS_CULLMODE. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	PrimType = OverDrawTests[uiTableIndex].PrimType;

	lWidth  = RectTarget.right  - RectTarget.left;
	lHeight = RectTarget.bottom - RectTarget.top;

	switch(OverDrawTests[uiTableIndex].PrimType)
	{
	case D3DMPT_TRIANGLELIST:
		uiNumPrims = 2;

		switch(OverDrawTests[uiTableIndex].uiDrawOrder)
		{
		case 0:
			// Verts in CCW order
			pVertices[0].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[0].y =   (float)RectTarget.top;
			pVertices[0].z =   0.0f;
			pVertices[0].rhw = 1.0f;
			pVertices[0].Diffuse = D3DMCOLOR_XRGB(192,0,0);

			pVertices[1].x =   (float)RectTarget.left;
			pVertices[1].y =   (float)RectTarget.top;
			pVertices[1].z =   0.0f;
			pVertices[1].rhw = 1.0f;
			pVertices[1].Diffuse = D3DMCOLOR_XRGB(128,0,0);

			pVertices[2].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[2].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[2].z =   0.0f;
			pVertices[2].rhw = 1.0f;
			pVertices[2].Diffuse = D3DMCOLOR_XRGB(64,0,0);

			// Verts in CW order
			pVertices[3].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[3].y =   (float)RectTarget.top;
			pVertices[3].z =   0.0f;
			pVertices[3].rhw = 1.0f;
			pVertices[3].Diffuse = D3DMCOLOR_XRGB(0,192,0);

			pVertices[5].x =   (float)RectTarget.left;
			pVertices[5].y =   (float)RectTarget.top + ((float)lHeight / 2.0f);
			pVertices[5].z =   0.0f;
			pVertices[5].rhw = 1.0f;
			pVertices[5].Diffuse = D3DMCOLOR_XRGB(0,128,0);

			pVertices[4].x =   (float)RectTarget.left + ((float)lWidth / 2.0f);
			pVertices[4].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[4].z =   0.0f;
			pVertices[4].rhw = 1.0f;
			pVertices[4].Diffuse = D3DMCOLOR_XRGB(0,64,0);

			break;

		case 1:

			// Verts in CW order
			pVertices[0].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[0].y =   (float)RectTarget.top;
			pVertices[0].z =   0.0f;
			pVertices[0].rhw = 1.0f;
			pVertices[0].Diffuse = D3DMCOLOR_XRGB(0,192,0);

			pVertices[2].x =   (float)RectTarget.left;
			pVertices[2].y =   (float)RectTarget.top + ((float)lHeight / 2.0f);
			pVertices[2].z =   0.0f;
			pVertices[2].rhw = 1.0f;
			pVertices[2].Diffuse = D3DMCOLOR_XRGB(0,128,0);

			pVertices[1].x =   (float)RectTarget.left + ((float)lWidth / 2.0f);
			pVertices[1].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[1].z =   0.0f;
			pVertices[1].rhw = 1.0f;
			pVertices[1].Diffuse = D3DMCOLOR_XRGB(0,64,0);

			// Verts in CCW order
			pVertices[3].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[3].y =   (float)RectTarget.top;
			pVertices[3].z =   0.0f;
			pVertices[3].rhw = 1.0f;
			pVertices[3].Diffuse = D3DMCOLOR_XRGB(192,0,0);

			pVertices[4].x =   (float)RectTarget.left;
			pVertices[4].y =   (float)RectTarget.top;
			pVertices[4].z =   0.0f;
			pVertices[4].rhw = 1.0f;
			pVertices[4].Diffuse = D3DMCOLOR_XRGB(128,0,0);

			pVertices[5].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[5].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[5].z =   0.0f;
			pVertices[5].rhw = 1.0f;
			pVertices[5].Diffuse = D3DMCOLOR_XRGB(64,0,0);

			break;

		default:
			DebugOut(DO_ERROR, L"Bad Overdraw test permutation.");
			hr = E_UNEXPECTED;
			goto cleanup;
		}
		break;

	case D3DMPT_TRIANGLESTRIP:

		uiNumPrims = 4;

		switch(OverDrawTests[uiTableIndex].uiDrawOrder)
		{
		case 0:
			pVertices[0].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[0].y =   (float)RectTarget.top;
			pVertices[0].z =   0.0f;
			pVertices[0].rhw = 1.0f;
			pVertices[0].Diffuse = D3DMCOLOR_XRGB(192,0,0);

			pVertices[1].x =   (float)RectTarget.left;
			pVertices[1].y =   (float)RectTarget.top;
			pVertices[1].z =   0.0f;
			pVertices[1].rhw = 1.0f;
			pVertices[1].Diffuse = D3DMCOLOR_XRGB(128,0,0);

			pVertices[2].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[2].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[2].z =   0.0f;
			pVertices[2].rhw = 1.0f;
			pVertices[2].Diffuse = D3DMCOLOR_XRGB(64,0,0);

			memcpy(&pVertices[3],&pVertices[0], sizeof(OVERDRAWVERT));
			pVertices[3].Diffuse = D3DMCOLOR_XRGB(0,192,0);

			memcpy(&pVertices[4],&pVertices[2], sizeof(OVERDRAWVERT));
			pVertices[4].Diffuse = D3DMCOLOR_XRGB(0,128,0);

			pVertices[5].x =   (float)RectTarget.left;
			pVertices[5].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[5].z =   0.0f;
			pVertices[5].rhw = 1.0f;
			pVertices[5].Diffuse = D3DMCOLOR_XRGB(0,64,0);

			break;
		case 1:

			pVertices[5].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[5].y =   (float)RectTarget.top;
			pVertices[5].z =   0.0f;
			pVertices[5].rhw = 1.0f;
			pVertices[5].Diffuse = D3DMCOLOR_XRGB(192,0,0);

			pVertices[4].x =   (float)RectTarget.left;
			pVertices[4].y =   (float)RectTarget.top;
			pVertices[4].z =   0.0f;
			pVertices[4].rhw = 1.0f;
			pVertices[4].Diffuse = D3DMCOLOR_XRGB(128,0,0);

			pVertices[3].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[3].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[3].z =   0.0f;
			pVertices[3].rhw = 1.0f;
			pVertices[3].Diffuse = D3DMCOLOR_XRGB(64,0,0);

			pVertices[2].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[2].y =   (float)RectTarget.top;
			pVertices[2].z =   0.0f;
			pVertices[2].rhw = 1.0f;
			pVertices[2].Diffuse = D3DMCOLOR_XRGB(0,192,0);

			pVertices[1].x =   (float)RectTarget.left + (float)lWidth;
			pVertices[1].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[1].z =   0.0f;
			pVertices[1].rhw = 1.0f;
			pVertices[1].Diffuse = D3DMCOLOR_XRGB(0,128,0);

			pVertices[0].x =   (float)RectTarget.left;
			pVertices[0].y =   (float)RectTarget.top + (float)lHeight;
			pVertices[0].z =   0.0f;
			pVertices[0].rhw = 1.0f;
			pVertices[0].Diffuse = D3DMCOLOR_XRGB(0,64,0);

			break;

		default:
			DebugOut(DO_ERROR, L"Bad Overdraw test permutation.");
			hr = E_UNEXPECTED;
			goto cleanup;
		}

		break;

	default:
		DebugOut(DO_ERROR, L"Bad Overdraw test permutation.");
		hr = E_UNEXPECTED;
		goto cleanup;
	}



	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(hr = CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
		                                      &pVB,                   // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
		                                      (BYTE*)pVertices,       // BYTE *pVertices,
		                                      sizeof(OVERDRAWVERT),   // UINT uiVertexSize,
		                                      uiNumVerts,             // UINT uiNumVertices,
		                                      OVERDRAWFVF)))          // DWORD dwFVF
	{
		DebugOut(DO_ERROR, L"CreateFilledVertexBuffer failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Indicate primitive data to be drawn
	//
	if (FAILED(hr = pDevice->DrawPrimitive(PrimType,         // D3DMPRIMITIVETYPE Type,
			                               0,                // UINT StartVertex,
			                               uiNumPrims)))     // UINT PrimitiveCount
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Get rid of vertex buffer reference count held by runtime
	//
	if (FAILED(hr = pDevice->SetStreamSource(0,           // UINT StreamNumber,
		                                     NULL,        // IDirect3DMobileVertexBuffer *pStreamData,
		                                     0)))         // UINT Stride
	{
		DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	if (pVB)
		pVB->Release();

	pVB=NULL;


cleanup:

	pDevice->SetStreamSource(0,           // UINT StreamNumber,
	                         NULL,        // IDirect3DMobileVertexBuffer *pStreamData,
	                         0);          // UINT Stride
		
	if (NULL != pVB)
		pVB->Release();

	if (pVertices)
		free(pVertices);

	return S_OK;
}

