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
#include "DebugOutput.h"
#include "PrimRast.h"
#include "BufferTools.h"
#include "util.h"
#include "DebugOutput.h"

//
// DrawPoints
//
//   D3DMPT_POINT Test Function
//
// Arguments:
//   
//   LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//   RENDER_FUNC RenderFunc:  Specifies rendering function (DIP, DP)
//   DRAW_RANGE DrawRange:  May specify partial rasterization of generated geometry
//   RECT RectTarget:  Area to render primitives into
//
// Return Value:
// 
//   HRESULT indicates success or failure
//
HRESULT DrawPoints(LPDIRECT3DMOBILEDEVICE pDevice,
                   RENDER_FUNC RenderFunc,
                   DRAW_RANGE DrawRange,
                   D3DMFORMAT Format,
                   RECT RectTarget)
{
	//
	// FVF; may be fixed, may be float
	//
	DWORD dwFVF = (D3DMFMT_D3DMVALUE_FIXED == Format) ? PRIMRASTFVF_FXD : PRIMRASTFVF_FLT;

	//
	// Buffers
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;
	LPDIRECT3DMOBILEINDEXBUFFER  pIB = NULL;

	//
	// Result of function; updated in all failure conditions
	//
	HRESULT hr = S_OK;

	//
	// Vertex structure pointer; index pointer
	//
	LPPRIMRASTVERT pVertices = NULL;
	WORD *pIndices = NULL;

	//
	// Like rasterization rules:
	//
	//    * Rightmost column of pixels in specified RECT is not drawn on
	//    * Lowest row of pixels in specified RECT is not drawn on
	//
	CONST UINT uiNumPixelsX = RectTarget.right - RectTarget.left;
	CONST UINT uiNumPixelsY = RectTarget.bottom - RectTarget.top;

	//
	// Iterators for geometry generation
	//
	UINT uiStepX, uiStepY;
	UINT uiX, uiY;

	//
	// Args for drawing calls
	//
	UINT uiPrimitiveCount;

	//
	// Check for invalid range
	//
	if (((DrawRange.fLow < 0.0f) || (DrawRange.fHigh < 0.0f)) ||
	    ((DrawRange.fLow > 1.0f) || (DrawRange.fHigh > 1.0f)) ||
	    (DrawRange.fLow > DrawRange.fHigh))
	{
		DebugOut(DO_ERROR, L"DRAW_RANGE invalid.");
		hr = E_INVALIDARG;
		goto cleanup;
	}

	//
	// Allocate storage for vertices
	//
	pVertices = (LPPRIMRASTVERT)malloc(sizeof(PRIMRASTVERT) * uiNumPixelsX);
	if (NULL == pVertices)
	{
		DebugOut(DO_ERROR, L"malloc failed.");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	//
	// Allocate storage for indices
	//
	pIndices = (WORD*)malloc(sizeof(WORD) * uiNumPixelsX);
	if (NULL == pIndices)
	{
		DebugOut(DO_ERROR, L"malloc failed.");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}


	//
	// Generate points to cover one "row" of target rect
	//
	uiY = RectTarget.top;
	for (uiStepY = 0; uiStepY < uiNumPixelsY; uiStepY++)
	{
		uiX = RectTarget.left;
		for (uiStepX = 0; uiStepX < uiNumPixelsX; uiStepX++)
		{
			pIndices[uiStepX] = (uiNumPixelsX - uiStepX - 1);
			pVertices[uiStepX].ulx =   GetFixedOrFloat((float)uiX, Format);
			pVertices[uiStepX].uly =   GetFixedOrFloat((float)uiY, Format);
			pVertices[uiStepX].ulz =   GetFixedOrFloat(0.0f, Format);
			pVertices[uiStepX].ulrhw = GetFixedOrFloat(1.0f, Format);

			ColorVertex(&pVertices[uiStepX].Diffuse, uiX, uiY);

			uiX++;
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(hr = CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
		                                          &pVB,                   // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
		                                          (BYTE*)pVertices,       // BYTE *pVertices,
		                                          sizeof(PRIMRASTVERT),   // UINT uiVertexSize,
		                                          uiNumPixelsX,           // UINT uiNumVertices,
		                                          dwFVF)))                // DWORD dwFVF
		{
			DebugOut(DO_ERROR, L"CreateFilledVertexBuffer failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		uiPrimitiveCount = (UINT)((float)uiNumPixelsX*(DrawRange.fHigh-DrawRange.fLow));

		switch(RenderFunc)
		{
		case D3DM_DRAWPRIMITIVE:

			//
			// Indicate primitive data to be drawn
			//
			if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_POINTLIST,                           // D3DMPRIMITIVETYPE Type,
			                                        (UINT)((float)uiNumPixelsX*DrawRange.fLow), // UINT StartVertex,
			                                        uiPrimitiveCount)))                         // UINT PrimitiveCount
			{
				DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x)", hr);
				goto cleanup;
			}

			break;
		case D3DM_DRAWINDEXEDPRIMITIVE:

			//
			// Create and fill a index buffer with the generated indices
			//
			if (FAILED(hr = CreateFilledIndexBuffer( pDevice,                    // LPDIRECT3DMOBILEDEVICE pDevice,
			                                         &pIB,                       // LPDIRECT3DMOBILEINDEXBUFFER *ppIB,
			                                         (BYTE*)pIndices,            // BYTE *pIndices,
			                                         sizeof(WORD),               // UINT uiIndexSize,
			                                         uiNumPixelsX)))             // UINT uiNumIndices,
			{
				DebugOut(DO_ERROR, L"CreateFilledIndexBuffer failed. (hr = 0x%08x)", hr);
				goto cleanup;
			}

			//
			// Indicate primitive data to be drawn
			//
			if (FAILED(hr = pDevice->DrawIndexedPrimitive(D3DMPT_POINTLIST,                           // D3DMPRIMITIVETYPE Type,
			                                               0,                                          // INT BaseVertexIndex,
			                                               0,                                          // UINT MinIndex,
			                                               uiNumPixelsX,                               // UINT NumVertices,
			                                               (UINT)((float)uiNumPixelsX*DrawRange.fLow), // UINT StartIndex,
			                                                uiPrimitiveCount)))                         // UINT PrimitiveCount
			{
				DebugOut(DO_ERROR, L"DrawIndexedPrimitive failed. (hr = 0x%08x)", hr);
				goto cleanup;
			}

			break;
		default:
			DebugOut(DO_ERROR, L"Invalid RENDER_FUNC.");
			hr = E_FAIL;
			goto cleanup;
			break;
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
		
		//
		// Get rid of index buffer reference count held by runtime
		//
		if (FAILED(pDevice->SetIndices(NULL)))
		{
			DebugOut(DO_ERROR, L"SetIndices failed.");
			hr = E_FAIL;
			goto cleanup;
		}
	
		if (pVB)
			pVB->Release();

		pVB=NULL;

		if (pIB)
			pIB->Release();

		pIB=NULL;

		uiY++;
	}


cleanup:

	pDevice->SetStreamSource(0,           // UINT StreamNumber,
	                         NULL,        // IDirect3DMobileVertexBuffer *pStreamData,
	                         0);          // UINT Stride
		
	pDevice->SetIndices(NULL);

	if (NULL != pVB)
		pVB->Release();

	if (NULL != pIB)
		pIB->Release();

	if (pVertices)
		free(pVertices);

	if (pIndices)
		free(pIndices);

	return S_OK;

}

