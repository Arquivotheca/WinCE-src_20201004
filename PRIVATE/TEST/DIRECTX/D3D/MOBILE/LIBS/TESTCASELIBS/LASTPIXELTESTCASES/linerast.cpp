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
#include "BufferTools.h"

//
// Short lines help to accentuate the affect of D3DMRS_LASTPIXEL
//
#define D3DMQA_LINE_LENGTH 3

//
// Vertex specification for this test
//
#define LASTPIXELFVF (D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE)
typedef struct _LASTPIXELVERT {
	float x,y,z,rhw;
	DWORD Diffuse;
} LASTPIXELVERT, *LPLASTPIXELVERT;



//
// DrawLineList
//
//   D3DMRS_LASTPIXEL test of primitive type D3DMPT_LINELIST
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
HRESULT DrawLineList(LPDIRECT3DMOBILEDEVICE pDevice,
                     RECT RectTarget)
{
	//
	// Primitive type
	//
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_LINELIST;

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
	LPLASTPIXELVERT pVertices = NULL;

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
	UINT uiNumVertsPerRow;

	//
	// Row of lines, D3DMQA_LINE_LENGTH pixels long each, ignoring empty
	// rightmost space if it does not fit another whole line.
	//
	uiNumLinesPerRow = (uiNumPixelsX / D3DMQA_LINE_LENGTH);

	//
	// Line lists take two verts per line
	//
	uiNumVertsPerRow = (uiNumLinesPerRow*2);

	//
	// Allocate storage for vertices
	//
	pVertices = (LPLASTPIXELVERT)malloc(sizeof(LASTPIXELVERT) * uiNumVertsPerRow);
	if (NULL == pVertices)
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
			pVertices[uiStepX].x =   (float)uiX;
			pVertices[uiStepX].y =   (float)uiY;
			pVertices[uiStepX].z =   0.0f;
			pVertices[uiStepX].rhw = 1.0f;

			//
			// Use distinct colors to help delineate between seperate lines
			//
			switch(uiStepX % 3)
			{
			case 0:
				pVertices[uiStepX].Diffuse = D3DMCOLOR_XRGB(255,0,0);
				break;
			case 1:
				pVertices[uiStepX].Diffuse = D3DMCOLOR_XRGB(0,255,0);
				break;
			case 2:
				pVertices[uiStepX].Diffuse = D3DMCOLOR_XRGB(0,0,255);
				break;
			}

			//
			// End of one line is beginning of next; but requires two verts.
			// Thus, X position is incremented only on even (or zero) iterations
			//
			if (0 == (uiStepX % 2))
				uiX+=D3DMQA_LINE_LENGTH;
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
		                                     &pVB,                   // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
		                                     (BYTE*)pVertices,       // BYTE *pVertices,
		                                     sizeof(LASTPIXELVERT),  // UINT uiVertexSize,
		                                     uiNumVertsPerRow,       // UINT uiNumVertices,
		                                     LASTPIXELFVF)))         // DWORD dwFVF
		{
			OutputDebugString(_T("CreateFilledVertexBuffer failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Indicate primitive data to be drawn
		//
		if (FAILED(pDevice->DrawPrimitive(PrimType,               // D3DMPRIMITIVETYPE Type,
			                              0,                      // UINT StartVertex,
			                              uiNumLinesPerRow)))     // UINT PrimitiveCount
		{
			OutputDebugString(_T("DrawPrimitive failed."));
			hr = E_FAIL;
			goto cleanup;
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

		if (pVB)
			pVB->Release();

		pVB=NULL;

		uiY++;
	}


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


//
// DrawLineStrip
//
//   D3DMRS_LASTPIXEL test of primitive type D3DMPT_LINESTRIP
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

HRESULT DrawLineStrip(LPDIRECT3DMOBILEDEVICE pDevice,
                      RECT RectTarget)
{
	//
	// Primitive type
	//
	CONST D3DMPRIMITIVETYPE PrimType = D3DMPT_LINESTRIP;

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
	LPLASTPIXELVERT pVertices = NULL;

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
	UINT uiNumVertsPerRow;

	//
	// Row of lines, D3DMQA_LINE_LENGTH pixels long each, ignoring empty rightmost space if it does not fit another whole line.
	//
	uiNumLinesPerRow = (uiNumPixelsX / D3DMQA_LINE_LENGTH);

	//
	// Line strips take (# lines + 1) vertices
	//
	uiNumVertsPerRow = (uiNumLinesPerRow+1);

	//
	// Allocate storage for vertices
	//
	pVertices = (LPLASTPIXELVERT)malloc(sizeof(LASTPIXELVERT) * uiNumVertsPerRow);
	if (NULL == pVertices)
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
			pVertices[uiStepX].x =   (float)uiX;
			pVertices[uiStepX].y =   (float)uiY;
			pVertices[uiStepX].z =   0.0f;
			pVertices[uiStepX].rhw = 1.0f;

			//
			// Use distinct colors to help delineate between seperate lines
			//
			switch(uiStepX % 3)
			{
			case 0:
				pVertices[uiStepX].Diffuse = D3DMCOLOR_XRGB(255,0,0);
				break;
			case 1:
				pVertices[uiStepX].Diffuse = D3DMCOLOR_XRGB(0,255,0);
				break;
			case 2:
				pVertices[uiStepX].Diffuse = D3DMCOLOR_XRGB(0,0,255);
				break;
			}

			//
			// Line strips share endpoints/startpoints
			//
			uiX+=D3DMQA_LINE_LENGTH;
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
		                                     &pVB,                   // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
		                                     (BYTE*)pVertices,       // BYTE *pVertices,
		                                     sizeof(LASTPIXELVERT),  // UINT uiVertexSize,
		                                     uiNumVertsPerRow,       // UINT uiNumVertices,
		                                     LASTPIXELFVF)))         // DWORD dwFVF
		{
			OutputDebugString(_T("CreateFilledVertexBuffer failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Indicate primitive data to be drawn
		//
		if (FAILED(pDevice->DrawPrimitive(PrimType,               // D3DMPRIMITIVETYPE Type,
			                              0,                      // UINT StartVertex,
			                              uiNumLinesPerRow)))     // UINT PrimitiveCount
		{
			OutputDebugString(_T("DrawPrimitive failed."));
			hr = E_FAIL;
			goto cleanup;
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
	
		if (pVB)
			pVB->Release();

		pVB=NULL;

		uiY++;
	}


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

