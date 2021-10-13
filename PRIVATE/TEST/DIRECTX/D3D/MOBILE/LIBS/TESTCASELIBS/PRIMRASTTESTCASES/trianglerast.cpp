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
// DrawTriangleFan
//
//   Primitive Drawing Test Function (D3DMPT_TRIANGLEFAN)
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
HRESULT DrawTriangleFan(LPDIRECT3DMOBILEDEVICE pDevice,
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
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_TRIANGLEFAN;

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
	UINT uiFanIter, uiStepY;
	UINT uiY;
	UINT uiNumTrisPerRow;
	UINT uiNumColumns;
	UINT uiNumRows;

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
	// Grid of quads
	//
	uiNumColumns = 1;
	uiNumRows    = (uiNumPixelsY / 20);
	uiNumTrisPerRow    = 8;

	//
	// Triangle fan of 8 tris / fan; includes overhead of center point and point to close fan
	//
	uiNumVertsPerRow = uiNumTrisPerRow+2;

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
	for (uiStepY = 0; uiStepY < uiNumRows; uiStepY++)
	{
		for (uiFanIter = 0; uiFanIter < uiNumVertsPerRow; uiFanIter++)
		{
			float fx=0.0f, fy=0.0f;
			pIndices[uiFanIter] = (uiNumVertsPerRow - uiFanIter - 1);
			pVertices[uiFanIter].ulz =   GetFixedOrFloat(0.0f,Format);
			pVertices[uiFanIter].ulrhw = GetFixedOrFloat(1.0f,Format);

			switch (uiFanIter % 10)
			{
			case 0:
				fx = (float)(RectTarget.left+((RectTarget.right-RectTarget.left)/2));
				fy = (float)(uiY+ 10);
				break;
			case 1:
				fx = (float)(RectTarget.left);
				fy = (float)(uiY+ 20);
				break;
			case 2:
				fx = (float)(RectTarget.left);
				fy = (float)(uiY+ 10);
				break;
			case 3:
				fx = (float)(RectTarget.left);
				fy = (float)(uiY+ 0);
				break;
			case 4:
				fx = (float)(RectTarget.left+((RectTarget.right-RectTarget.left)/2));
				fy = (float)(uiY+ 0);
				break;
			case 5:
				fx = (float)(RectTarget.right);
				fy = (float)(uiY+ 0);
				break;
			case 6:
				fx = (float)(RectTarget.right);
				fy = (float)(uiY+10);
				break;
			case 7:
				fx = (float)(RectTarget.right);
				fy = (float)(uiY+20);
				break;
			case 8:
				fx = (float)(RectTarget.left+((RectTarget.right-RectTarget.left)/2));
				fy = (float)(uiY+20);
				break;
			case 9:
				fx = (float)(RectTarget.left);
				fy = (float)(uiY+20);
				break;
			}

			pVertices[uiFanIter].ulx = GetFixedOrFloat(fx, Format);
			pVertices[uiFanIter].uly = GetFixedOrFloat(fy, Format);

			ColorVertex(&pVertices[uiFanIter].Diffuse, (UINT)fx, (UINT)fy);

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

		uiPrimitiveCount = (UINT)((float)uiNumTrisPerRow*(DrawRange.fHigh-DrawRange.fLow));

		//
		// StartVertex rounded down to nearest triangle fan
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

		uiY+=20;
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
// DrawTriangleList
//
//   Primitive Drawing Test Function (D3DMPT_TRIANGLELIST)
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
HRESULT DrawTriangleList(LPDIRECT3DMOBILEDEVICE pDevice,
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
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_TRIANGLELIST;

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
	UINT uiNumTrisPerRow;
	UINT uiNumColumns;
	UINT uiNumRows;

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
	// Grid of quads; 10px x 10px each
	//
	uiNumColumns = (uiNumPixelsX / 10);
	uiNumRows    = (uiNumPixelsY / 10);
	uiNumTrisPerRow    = (uiNumColumns)*2;

	//
	// Triangle lists take (# trianges * 3) vertices
	//
	uiNumVertsPerRow = (uiNumTrisPerRow*3);

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
	for (uiStepY = 0; uiStepY < uiNumRows; uiStepY++)
	{
		uiX = RectTarget.left;
		for (uiStepX = 0; uiStepX < uiNumVertsPerRow; uiStepX++)
		{
			float fx=0.0f, fy=0.0f;
			pIndices[uiStepX] = (uiNumVertsPerRow - uiStepX - 1);
			pVertices[uiStepX].ulz =   GetFixedOrFloat(0.0f,Format);
			pVertices[uiStepX].ulrhw = GetFixedOrFloat(1.0f,Format);

			switch (uiStepX % 6)
			{
			case 0:
				fx = (float)(uiX+ 0);
				fy = (float)(uiY+10);
				break;
			case 1:
			case 4:
				fx = (float)(uiX+ 0);
				fy = (float)(uiY+ 0);
				break;
			case 2:
			case 3:
				fx = (float)(uiX+10);
				fy = (float)(uiY+10);
				break;
			case 5:
				fx = (float)(uiX+10);
				fy = (float)(uiY+ 0);

				//
				// Next quad position
				//
				uiX+=10;
				break;
			}

			pVertices[uiStepX].ulx = GetFixedOrFloat(fx,Format);
			pVertices[uiStepX].uly = GetFixedOrFloat(fy,Format);

			ColorVertex(&pVertices[uiStepX].Diffuse, (UINT)fx, (UINT)fy);
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

		uiPrimitiveCount = (UINT)((float)uiNumTrisPerRow*(DrawRange.fHigh-DrawRange.fLow));

		//
		// StartVertex rounded down to nearest triangle
		//
		uiStartVertex = (UINT)((float)uiNumVertsPerRow*DrawRange.fLow);
		uiStartVertex = uiStartVertex - (uiStartVertex%3);

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

		uiY+=10;
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
// DrawTriangleStrip
//
//   Primitive Drawing Test Function (D3DMPT_TRIANGLESTRIP)
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
HRESULT DrawTriangleStrip(LPDIRECT3DMOBILEDEVICE pDevice,
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
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_TRIANGLESTRIP;

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
	UINT uiNumTrisPerRow;
	UINT uiNumColumns;
	UINT uiNumRows;

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
	// Grid of quads; 10px x 10px each
	//
	uiNumColumns = (uiNumPixelsX / 10);
	uiNumRows    = (uiNumPixelsY / 10);
	uiNumTrisPerRow    = (uiNumColumns)*2;

	//
	// Triangle strips take (# trianges + 2) vertices
	//
	uiNumVertsPerRow = (uiNumTrisPerRow+2);

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
	for (uiStepY = 0; uiStepY < uiNumRows; uiStepY++)
	{
		uiX = RectTarget.left;
		for (uiStepX = 0; uiStepX < uiNumVertsPerRow; uiStepX++)
		{
			float fx=0.0f, fy=0.0f;
			pIndices[uiStepX] = (uiNumVertsPerRow - uiStepX - 1);
			pVertices[uiStepX].ulz =   GetFixedOrFloat(0.0f,Format);
			pVertices[uiStepX].ulrhw = GetFixedOrFloat(1.0f,Format);

			switch (uiStepX % 2)
			{
			case 0:
				fx = (float)(uiX+ 0);
				fy = (float)(uiY+10);
				break;
			case 1:
				fx = (float)(uiX+ 0);
				fy = (float)(uiY+ 0);

				//
				// Next quad position
				//
				uiX+=10;

				break;
			}

			pVertices[uiStepX].ulx = GetFixedOrFloat(fx,Format);
			pVertices[uiStepX].uly = GetFixedOrFloat(fy,Format);

			ColorVertex(&pVertices[uiStepX].Diffuse, (UINT)fx, (UINT)fy);

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

		uiPrimitiveCount = (UINT)((float)uiNumTrisPerRow*(DrawRange.fHigh-DrawRange.fLow));

		//
		// StartVertex rounded down to nearest triangle
		//
		uiStartVertex = (UINT)((float)uiNumVertsPerRow*DrawRange.fLow);
		uiStartVertex = uiStartVertex - (uiStartVertex%2);

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

		uiY+=10;
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
