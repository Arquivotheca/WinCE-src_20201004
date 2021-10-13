//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "DebugOutput.h"
#include "PrimRast.h"
#include "BufferTools.h"
#include "util.h"

//
// DrawLineList
//
//   Primitive Drawing Test Function (D3DMPT_LINELIST)
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

HRESULT DrawLineList(LPDIRECT3DMOBILEDEVICE pDevice,
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
	// Primitive type
	//
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_LINELIST;

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
	UINT uiNumLinesPerRow;

	//
	// Args for drawing calls
	//
	UINT uiPrimitiveCount, uiNumVertsPerRow, uiStartVertex;

	//
	// Check for invalid range
	//
	if (((DrawRange.fLow < 0.0f) || (DrawRange.fHigh < 0.0f)) ||
	    ((DrawRange.fLow > 1.0f) || (DrawRange.fHigh > 1.0f)) ||
	    (DrawRange.fLow > DrawRange.fHigh))
	{
		OutputDebugString(_T("DRAW_RANGE invalid."));
		hr = ERROR_INVALID_PARAMETER;
		goto cleanup;
	}

	//
	// Row of lines, 10 pixels long each, ignoring empty rightmost space if it does not fit another whole line.
	//
	uiNumLinesPerRow = (uiNumPixelsX / 10);

	//
	// Line lists take two verts per line
	//
	uiNumVertsPerRow = (uiNumLinesPerRow*2);

	//
	// Allocate storage for vertices
	//
	pVertices = (LPPRIMRASTVERT)malloc(sizeof(PRIMRASTVERT) * uiNumVertsPerRow);
	if (NULL == pVertices)
	{
		OutputDebugString(_T("malloc failed."));
		hr = ERROR_NOT_ENOUGH_MEMORY;
		goto cleanup;
	}

	//
	// Allocate storage for indices
	//
	pIndices = (WORD*)malloc(sizeof(WORD) * uiNumVertsPerRow);
	if (NULL == pIndices)
	{
		OutputDebugString(_T("malloc failed."));
		hr = ERROR_NOT_ENOUGH_MEMORY;
		goto cleanup;
	}

	//
	// Generate points to cover one "row" of target rect
	//
	uiY = RectTarget.top;
	for (uiStepY = 0; uiStepY < uiNumPixelsY; uiStepY++)
	{
		uiX = RectTarget.left;
		for (uiStepX = 0; uiStepX < uiNumVertsPerRow; uiStepX++)
		{
			pIndices[uiStepX] = (uiNumVertsPerRow - uiStepX - 1);

			pVertices[uiStepX].ulx =   GetFixedOrFloat((float)uiX, Format);
			pVertices[uiStepX].uly =   GetFixedOrFloat((float)uiY, Format);
			pVertices[uiStepX].ulz =   GetFixedOrFloat(0.0f, Format);
			pVertices[uiStepX].ulrhw = GetFixedOrFloat(1.0f, Format);

			ColorVertex(&pVertices[uiStepX].Diffuse, uiX, uiY);

			//
			// End of one line is beginning of next; but requires two verts.
			// Thus, X position is incremented only on even (or zero) iterations
			//
			if (0 == (uiStepX % 2))
				uiX+=10;
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
		                                     &pVB,                   // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
		                                     (BYTE*)pVertices,       // BYTE *pVertices,
		                                     sizeof(PRIMRASTVERT),   // UINT uiVertexSize,
		                                     uiNumVertsPerRow,       // UINT uiNumVertices,
		                                     dwFVF)))                // DWORD dwFVF
		{
			OutputDebugString(_T("CreateFilledVertexBuffer failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		uiPrimitiveCount = (UINT)((float)uiNumLinesPerRow*(DrawRange.fHigh-DrawRange.fLow));

		//
		// StartVertex rounded down to nearest line start
		//
		uiStartVertex = (UINT)((float)uiNumVertsPerRow*DrawRange.fLow);
		uiStartVertex = (uiStartVertex%2) ? uiStartVertex-1 : uiStartVertex;

		switch(RenderFunc)
		{
		case D3DM_DRAWPRIMITIVE:

			//
			// Indicate primitive data to be drawn
			//
			if (FAILED(pDevice->DrawPrimitive(PrimType,               // D3DMPRIMITIVETYPE Type,
			                                  uiStartVertex,          // UINT StartVertex,
			                                  uiPrimitiveCount)))     // UINT PrimitiveCount
			{
				OutputDebugString(_T("DrawPrimitive failed."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		case D3DM_DRAWINDEXEDPRIMITIVE:

			//
			// Create and fill a index buffer with the generated indices
			//
			if (FAILED(CreateFilledIndexBuffer( pDevice,                    // LPDIRECT3DMOBILEDEVICE pDevice,
			                                    &pIB,                       // LPDIRECT3DMOBILEINDEXBUFFER *ppIB,
			                                    (BYTE*)pIndices,            // BYTE *pIndices,
			                                    sizeof(WORD),               // UINT uiIndexSize,
			                                    uiNumVertsPerRow)))         // UINT uiNumIndices,
			{
				OutputDebugString(_T("CreateFilledIndexBuffer failed."));
				hr = E_FAIL;
				goto cleanup;
			}


			//
			// Indicate primitive data to be drawn
			//
			if (FAILED(SimpleDIP(pDevice,            // LPDIRECT3DMOBILEDEVICE pDevice,
			                     pVB,                // LPDIRECT3DMOBILEVERTEXBUFFER pVB,
			                     PrimType,           // D3DMPRIMITIVETYPE Type,
			                     uiStartVertex,      // UINT StartIndex,
			                     uiPrimitiveCount))) // UINT PrimitiveCount);
			{
				OutputDebugString(_T("DrawIndexedPrimitive failed."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		default:
			OutputDebugString(_T("Invalid RENDER_FUNC."));
			hr = E_FAIL;
			goto cleanup;
			break;
		}
			
		//
		// Get rid of vertex buffer reference count held by runtime
		//
		if (FAILED(pDevice->SetStreamSource(0,           // UINT StreamNumber,
		                                    NULL,        // IDirect3DMobileVertexBuffer *pStreamData,
		                                    0)))         // UINT Stride
		{
			OutputDebugString(_T("SetStreamSource failed."));
			hr = E_FAIL;
			goto cleanup;
		}
		
		//
		// Get rid of index buffer reference count held by runtime
		//
		if (FAILED(pDevice->SetIndices(NULL)))
		{
			OutputDebugString(_T("SetIndices failed."));
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


//
// DrawLineStrip
//
//   Primitive Drawing Test Function (D3DMPT_LINESTRIP)
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

HRESULT DrawLineStrip(LPDIRECT3DMOBILEDEVICE pDevice,
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
	// Primitive type
	//
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_LINESTRIP;

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
	UINT uiNumLinesPerRow;

	//
	// Args for drawing calls
	//
	UINT uiPrimitiveCount, uiNumVertsPerRow, uiStartVertex;

	//
	// Check for invalid range
	//
	if (((DrawRange.fLow < 0.0f) || (DrawRange.fHigh < 0.0f)) ||
	    ((DrawRange.fLow > 1.0f) || (DrawRange.fHigh > 1.0f)) ||
	    (DrawRange.fLow > DrawRange.fHigh))
	{
		OutputDebugString(_T("DRAW_RANGE invalid."));
		hr = ERROR_INVALID_PARAMETER;
		goto cleanup;
	}

	//
	// Row of lines, 10 pixels long each, ignoring empty rightmost space if it does not fit another whole line.
	//
	uiNumLinesPerRow = (uiNumPixelsX / 10);

	//
	// Line strips take (# lines + 1) vertices
	//
	uiNumVertsPerRow = (uiNumLinesPerRow+1);

	//
	// Allocate storage for vertices
	//
	pVertices = (LPPRIMRASTVERT)malloc(sizeof(PRIMRASTVERT) * uiNumVertsPerRow);
	if (NULL == pVertices)
	{
		OutputDebugString(_T("malloc failed."));
		hr = ERROR_NOT_ENOUGH_MEMORY;
		goto cleanup;
	}

	//
	// Allocate storage for indices
	//
	pIndices = (WORD*)malloc(sizeof(WORD) * uiNumVertsPerRow);
	if (NULL == pIndices)
	{
		OutputDebugString(_T("malloc failed."));
		hr = ERROR_NOT_ENOUGH_MEMORY;
		goto cleanup;
	}

	//
	// Generate points to cover one "row" of target rect
	//
	uiY = RectTarget.top;
	for (uiStepY = 0; uiStepY < uiNumPixelsY; uiStepY++)
	{
		uiX = RectTarget.left;
		for (uiStepX = 0; uiStepX < uiNumVertsPerRow; uiStepX++)
		{
			pIndices[uiStepX] = (uiNumVertsPerRow - uiStepX - 1);

			pVertices[uiStepX].ulx =   GetFixedOrFloat((float)uiX,Format);
			pVertices[uiStepX].uly =   GetFixedOrFloat((float)uiY,Format);
			pVertices[uiStepX].ulz =   GetFixedOrFloat(0.0f,Format);
			pVertices[uiStepX].ulrhw = GetFixedOrFloat(1.0f,Format);

			ColorVertex(&pVertices[uiStepX].Diffuse, uiX, uiY);

			//
			// Line strips share endpoints/startpoints
			//
			uiX+=10;
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
		                                     &pVB,                   // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
		                                     (BYTE*)pVertices,       // BYTE *pVertices,
		                                     sizeof(PRIMRASTVERT),   // UINT uiVertexSize,
		                                     uiNumVertsPerRow,       // UINT uiNumVertices,
		                                     dwFVF)))                // DWORD dwFVF
		{
			OutputDebugString(_T("CreateFilledVertexBuffer failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		uiPrimitiveCount = (UINT)((float)uiNumLinesPerRow*(DrawRange.fHigh-DrawRange.fLow));

		//
		// StartVertex rounded down to nearest line start
		//
		uiStartVertex = (UINT)((float)uiNumVertsPerRow*DrawRange.fLow);

		switch(RenderFunc)
		{
		case D3DM_DRAWPRIMITIVE:

			//
			// Indicate primitive data to be drawn
			//
			if (FAILED(pDevice->DrawPrimitive(PrimType,               // D3DMPRIMITIVETYPE Type,
			                                  uiStartVertex,          // UINT StartVertex,
			                                  uiPrimitiveCount)))     // UINT PrimitiveCount
			{
				OutputDebugString(_T("DrawPrimitive failed."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		case D3DM_DRAWINDEXEDPRIMITIVE:

			//
			// Create and fill a index buffer with the generated indices
			//
			if (FAILED(CreateFilledIndexBuffer( pDevice,                    // LPDIRECT3DMOBILEDEVICE pDevice,
			                                    &pIB,                       // LPDIRECT3DMOBILEINDEXBUFFER *ppIB,
			                                    (BYTE*)pIndices,            // BYTE *pIndices,
			                                    sizeof(WORD),               // UINT uiIndexSize,
			                                    uiNumVertsPerRow)))         // UINT uiNumIndices,
			{
				OutputDebugString(_T("CreateFilledIndexBuffer failed."));
				hr = E_FAIL;
				goto cleanup;
			}


			//
			// Indicate primitive data to be drawn
			//
			if (FAILED(SimpleDIP(pDevice,            // LPDIRECT3DMOBILEDEVICE pDevice,
			                     pVB,                // LPDIRECT3DMOBILEVERTEXBUFFER pVB,
			                     PrimType,           // D3DMPRIMITIVETYPE Type,
			                     uiStartVertex,      // UINT StartIndex,
			                     uiPrimitiveCount))) // UINT PrimitiveCount);
			{
				OutputDebugString(_T("DrawIndexedPrimitive failed."));
				hr = E_FAIL;
				goto cleanup;
			}

			break;
		default:
			OutputDebugString(_T("Invalid RENDER_FUNC."));
			hr = E_FAIL;
			goto cleanup;
			break;
		}
			
		//
		// Get rid of vertex buffer reference count held by runtime
		//
		if (FAILED(pDevice->SetStreamSource(0,           // UINT StreamNumber,
		                                    NULL,        // IDirect3DMobileVertexBuffer *pStreamData,
		                                    0)))         // UINT Stride
		{
			OutputDebugString(_T("SetStreamSource failed."));
			hr = E_FAIL;
			goto cleanup;
		}
		
		//
		// Get rid of index buffer reference count held by runtime
		//
		if (FAILED(pDevice->SetIndices(NULL)))
		{
			OutputDebugString(_T("SetIndices failed."));
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

