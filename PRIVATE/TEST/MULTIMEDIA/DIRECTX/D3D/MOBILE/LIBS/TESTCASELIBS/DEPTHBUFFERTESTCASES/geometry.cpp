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
#include "Geometry.h"
#include "BufferTools.h"
#include "DebugOutput.h"
#include "ColorTools.h"
#include <tchar.h>
#include "DebugOutput.h"

#define _M(_v) D3DM_MAKE_D3DMVALUE(_v)
#define _F(_f) (*((float*)(&_f)))

#define D3DMQA_NEAR_ONE 0.999f

__forceinline D3DMCOLOR GetVertColor(UINT uiFactor)
{
	D3DMCOLOR dwColor;
	BYTE r,g,b;

	r = (BYTE)(_F(SampleColorValues[uiFactor%uiNumSampleColorValues].r) * 255.0f);
	g = (BYTE)(_F(SampleColorValues[uiFactor%uiNumSampleColorValues].g) * 255.0f);
	b = (BYTE)(_F(SampleColorValues[uiFactor%uiNumSampleColorValues].b) * 255.0f);

	dwColor = D3DMCOLOR_XRGB(r,g,b);
	return dwColor;
}

HRESULT RenderLines(LPDIRECT3DMOBILEDEVICE pDevice, D3DMVIEWPORT Viewport)
{

	UINT uiNumLines = ((Viewport.Width/2) * 2) + ((Viewport.Height/2) * 2);
	UINT uiNumLineVerts = uiNumLines*2;
	UINT uiLineVertIter;
	UINT uiCurVert = 0;
	FLOAT fCenterX, fCenterY;
	LPD3DQA_DEPTHBUFTEST_VERT pLineVerts = NULL;
	LPDIRECT3DMOBILEVERTEXBUFFER pVBLines = NULL;
	D3DMCOLOR dwColor;

	HRESULT hr = S_OK;

	pLineVerts = (LPD3DQA_DEPTHBUFTEST_VERT)malloc(uiNumLineVerts*sizeof(D3DQA_DEPTHBUFTEST_VERT));
	if (NULL == pLineVerts)
	{
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// For bottom-left "quadrant" of viewport, compute center
	//
	fCenterX = (float)Viewport.X + ((float)Viewport.Width /4.0f);
	fCenterY = (float)Viewport.Y + ((float)Viewport.Height/2.0f) + ((float)Viewport.Height/4.0f);

	for (uiLineVertIter=0; uiLineVertIter < (Viewport.Width/2); uiLineVertIter++)
	{
		dwColor = GetVertColor(uiCurVert);
			pLineVerts[uiCurVert].x =       (float)Viewport.X + (float)uiLineVertIter;
			pLineVerts[uiCurVert].y =       (float)Viewport.Y + ((float)Viewport.Height / 2);
			pLineVerts[uiCurVert].z =       0.0f;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;

			pLineVerts[uiCurVert].x =       fCenterX;
			pLineVerts[uiCurVert].y =       fCenterY;
			pLineVerts[uiCurVert].z =       D3DMQA_NEAR_ONE;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;
	}

	for (uiLineVertIter=0; uiLineVertIter < (Viewport.Width/2); uiLineVertIter++)
	{
		dwColor = GetVertColor(uiCurVert);
			pLineVerts[uiCurVert].x =       (float)Viewport.X + (float)uiLineVertIter;
			pLineVerts[uiCurVert].y =       (float)Viewport.Y + (float)Viewport.Height;
			pLineVerts[uiCurVert].z =       0.0f;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;

			pLineVerts[uiCurVert].x =       fCenterX;
			pLineVerts[uiCurVert].y =       fCenterY;
			pLineVerts[uiCurVert].z =       D3DMQA_NEAR_ONE;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;
	}

	for (uiLineVertIter=0; uiLineVertIter < (Viewport.Height/2); uiLineVertIter++)
	{
		dwColor = GetVertColor(uiCurVert);
			pLineVerts[uiCurVert].x =       (float)Viewport.X;
			pLineVerts[uiCurVert].y =       (float)Viewport.Y + ((float)Viewport.Height/2) + uiLineVertIter;
			pLineVerts[uiCurVert].z =       0.0f;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;

			pLineVerts[uiCurVert].x =       fCenterX;
			pLineVerts[uiCurVert].y =       fCenterY;
			pLineVerts[uiCurVert].z =       D3DMQA_NEAR_ONE;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;
	}

	for (uiLineVertIter=0; uiLineVertIter < (Viewport.Height/2); uiLineVertIter++)
	{
		dwColor = GetVertColor(uiCurVert);
			pLineVerts[uiCurVert].x =       (float)Viewport.X + ((float)Viewport.Width/2);
			pLineVerts[uiCurVert].y =       (float)Viewport.Y + ((float)Viewport.Height/2) + uiLineVertIter;
			pLineVerts[uiCurVert].z =       0.0f;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;

			pLineVerts[uiCurVert].x =       fCenterX;
			pLineVerts[uiCurVert].y =       fCenterY;
			pLineVerts[uiCurVert].z =       D3DMQA_NEAR_ONE;
			pLineVerts[uiCurVert].rhw =     1.0f;
			pLineVerts[uiCurVert].Diffuse = dwColor;
			uiCurVert++;
	}


	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(hr = CreateFilledVertexBuffer( pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
				        					  &pVBLines,                       // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
										      (BYTE*)pLineVerts,               // BYTE *pVertices,
										      sizeof(D3DQA_DEPTHBUFTEST_VERT), // UINT uiVertexSize,
										      uiNumLineVerts,                  // UINT uiNumVertices,
										      D3DQA_DEPTHBUFTEST_VERT_FVF)))   // DWORD dwFVF
	{
		DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}


	//
	// Draw geometry:  Lines
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_LINELIST, 0, uiNumLines)))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

cleanup:

	if (pLineVerts)
		free(pLineVerts);

	if (pVBLines)
		pVBLines->Release();

	return hr;

}


HRESULT RenderPoints(LPDIRECT3DMOBILEDEVICE pDevice, D3DMVIEWPORT Viewport)
{

	UINT uiNumPoints = (Viewport.Height/2);
	UINT uiNumPointVerts = uiNumPoints;
	UINT uiPointVertIter;
	UINT uiCurVert = 0;
	LPD3DQA_DEPTHBUFTEST_VERT pPointVerts = NULL;
	LPDIRECT3DMOBILEVERTEXBUFFER pVBPoints = NULL;
	CONST float fSeperator = 2.0f;
	CONST float fConstantOffset = 2.0f;
	UINT uiSubPixelOffset;
	float fSubPixelOffset;

	HRESULT hr = S_OK;

	pPointVerts = (LPD3DQA_DEPTHBUFTEST_VERT)malloc(uiNumPointVerts*sizeof(D3DQA_DEPTHBUFTEST_VERT));
	if (NULL == pPointVerts)
	{
		hr = E_FAIL;
		goto cleanup;
	}


	for (uiSubPixelOffset = 0; uiSubPixelOffset < 16; uiSubPixelOffset++)
	{
		fSubPixelOffset = (1.0f / 16.0f) * (float)uiSubPixelOffset;


		for (uiPointVertIter=0; uiPointVertIter < (Viewport.Height/2); uiPointVertIter++)
		{
			pPointVerts[uiPointVertIter].x =       ((float)Viewport.X + (float)Viewport.Width / 2.0f) + fSubPixelOffset + fSeperator*uiSubPixelOffset + fConstantOffset;
			if (pPointVerts[uiPointVertIter].x > ((float)Viewport.X + (float)Viewport.Width))
			{
				goto cleanup;
			}
			pPointVerts[uiPointVertIter].y =       (float)((Viewport.Height/2) + uiPointVertIter);
			pPointVerts[uiPointVertIter].z =       uiPointVertIter*(1.0f / ((float)uiNumPoints-1));
			pPointVerts[uiPointVertIter].rhw =     1.0f;
			pPointVerts[uiPointVertIter].Diffuse = D3DMCOLOR_XRGB(0,0,0);
			//DebugOut(L"Vert #%u...  X: %f; Y: %f", uiPointVertIter, pPointVerts[uiPointVertIter].x, pPointVerts[uiPointVertIter].y);
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(hr = CreateFilledVertexBuffer( pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
											      &pVBPoints,                       // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
											      (BYTE*)pPointVerts,               // BYTE *pVertices,
											      sizeof(D3DQA_DEPTHBUFTEST_VERT), // UINT uiVertexSize,
											      uiNumPointVerts,                  // UINT uiNumVertices,
											      D3DQA_DEPTHBUFTEST_VERT_FVF)))   // DWORD dwFVF
		{
			DebugOut(DO_ERROR, L"CreateVertexBuffer failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}


		//
		// Draw geometry:  Points
		//
		if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_POINTLIST, 0, uiNumPoints)))
		{
			DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}

		if (pVBPoints)
			pVBPoints->Release();

		pVBPoints=NULL;
	}

cleanup:

	if (pPointVerts)
		free(pPointVerts);

	if (pVBPoints)
		pVBPoints->Release();

	return hr;

}

HRESULT RenderGeometry(LPDIRECT3DMOBILEDEVICE pDevice)
{

	//
	// Vertex buffer for triangles
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVBTriangles = NULL;

	//
	// Vertex data will be scaled to viewport extents
	//
	D3DMVIEWPORT Viewport;

	//
	// Iterator for vertex data
	//
	UINT uiVertIndex;
    
	//
	// Pool for vertex buffer creation
	//
	D3DMPOOL VertexBufPool;

	//
	// Pointer for LPDIRECT3EMOBILEVERTEXBUFFER lock
	//
	LPD3DQA_DEPTHBUFTEST_VERT pVertMem;

	//
	// All failure cases set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Must pass valid device on which to create vertex buffer
	//
	if (NULL == pDevice)
	{
		DebugOut(DO_ERROR, L"Invalid LPDIRECT3DMOBILEDEVICE");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve viewport extents; to be used for vertex data scaling
	//
	if (FAILED(hr = pDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"GetViewport failed (hr = 0x%08x)", hr);
		goto cleanup;
	}


	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSVERTEXBUFFER & Caps.SurfaceCaps)
	{
		VertexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER");
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pDevice->CreateVertexBuffer( sizeof(D3DQA_DEPTHBUFTEST_VERT)*D3DQA_DEPTHBUFTEST_VERTCOUNT, // UINT Length,
	                                              0,                               // DWORD Usage,
	                                              D3DQA_DEPTHBUFTEST_VERT_FVF,     // DWORD FVF,
	                                              VertexBufPool,                   // D3DMPOOL Pool,
	                                              &pVBTriangles)))                 // LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBuffer,
	{
		DebugOut(DO_ERROR, L"Invalid LPDIRECT3DMOBILEDEVICE (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// If NULL-check fails, inspect CreateVertexBuffer for bad failure case
	//
	if (NULL == pVBTriangles)
	{
		DebugOut(DO_ERROR, L"Invalid LPDIRECT3DMOBILEVERTEXBUFFER");
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pVBTriangles->Lock(0,                   // UINT OffsetToLock,
	                                    sizeof(D3DQA_DEPTHBUFTEST_VERT)*D3DQA_DEPTHBUFTEST_VERTCOUNT,  // UINT SizeToLock,
	                                    (VOID**)&pVertMem,   // VOID **ppbData,
	                                    0)))                 // DWORD Flags
	{
		DebugOut(DO_ERROR, L"LPDIRECT3DMOBILEVERTEXBUFFER->Lock() failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// If NULL-check fails, inspect Lock for bad failure case
	//
	if (NULL == pVertMem)
	{
		DebugOut(DO_ERROR, L"Invalid pointer.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Scale to viewport extents
	//
    for (uiVertIndex = 0; uiVertIndex < D3DQA_DEPTHBUFTEST_VERTCOUNT; uiVertIndex++)
	{
		pVertMem[uiVertIndex].x =       DepthBufTestVerts[uiVertIndex].x * (float)Viewport.Width;
		pVertMem[uiVertIndex].y =       DepthBufTestVerts[uiVertIndex].y * (float)Viewport.Height;
		pVertMem[uiVertIndex].z =       DepthBufTestVerts[uiVertIndex].z;
		pVertMem[uiVertIndex].rhw =     DepthBufTestVerts[uiVertIndex].rhw;
		pVertMem[uiVertIndex].Diffuse = DepthBufTestVerts[uiVertIndex].Diffuse;
	}

	//
	// Unlock vertex buffer
	//
	if (FAILED(hr = pVBTriangles->Unlock()))
	{
		DebugOut(DO_ERROR, L"LPDIRECT3DMOBILEVERTEXBUFFER->Unlock() failed (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Indicate active vertex buffer
	//
	if (FAILED(hr = pDevice->SetStreamSource(0,pVBTriangles,0)))
	{
		DebugOut(DO_ERROR, L"LPDIRECT3DMOBILEDEVICE->SetStreamSource() failed (hr = 0x%08x)", hr);
		goto cleanup;
	}


	//
	// Enter a scene; indicates that drawing is about to occur
	//
    if( FAILED( hr = pDevice->BeginScene() ) )
	{
		DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Draw geometry:  Triangles
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DQA_DEPTHBUFTEST_TRIVERT_PT,
	                                       D3DQA_DEPTHBUFTEST_TRIVERTSTART,
	                                       D3DQA_DEPTHBUFTEST_TRIPRIMCOUNT)))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	if (FAILED(hr = RenderLines(pDevice,   // LPDIRECT3DMOBILEDEVICE pDevice,
	                            Viewport)))// D3DMVIEWPORT Viewport
	{
		DebugOut(DO_ERROR, L"RenderLines failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	if (FAILED(hr = RenderPoints(pDevice,   // LPDIRECT3DMOBILEDEVICE pDevice,
	                             Viewport)))// D3DMVIEWPORT Viewport
	{
		DebugOut(DO_ERROR, L"RenderLines failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(hr = pDevice->EndScene()))
	{
		DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}
	
	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
    if (FAILED(hr = pDevice->Present( NULL, NULL, NULL, NULL )))
	{
		DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}
	

cleanup:

	if (pVBTriangles)
		pVBTriangles->Release();

	return hr;
}
