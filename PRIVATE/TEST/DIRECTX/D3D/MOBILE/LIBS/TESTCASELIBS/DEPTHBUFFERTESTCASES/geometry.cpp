//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "Geometry.h"
#include "BufferTools.h"
#include "DebugOutput.h"
#include "ColorTools.h"
#include <tchar.h>

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
	if (FAILED(CreateFilledVertexBuffer( pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
										 &pVBLines,                       // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
										 (BYTE*)pLineVerts,               // BYTE *pVertices,
										 sizeof(D3DQA_DEPTHBUFTEST_VERT), // UINT uiVertexSize,
										 uiNumLineVerts,                  // UINT uiNumVertices,
										 D3DQA_DEPTHBUFTEST_VERT_FVF)))   // DWORD dwFVF
	{
		OutputDebugString(_T("CreateVertexBuffer failed."));
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// Draw geometry:  Lines
	//
	if (FAILED(pDevice->DrawPrimitive(D3DMPT_LINELIST,
									  0,
									  uiNumLines)))
	{
		OutputDebugString(_T("DrawPrimitive failed."));
		hr = E_FAIL;
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
			//DebugOut(_T("Vert #%u...  X: %f; Y: %f"), uiPointVertIter, pPointVerts[uiPointVertIter].x, pPointVerts[uiPointVertIter].y);
		}

		//
		// Create and fill a vertex buffer with the generated geometry
		//
		if (FAILED(CreateFilledVertexBuffer( pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
											 &pVBPoints,                       // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
											 (BYTE*)pPointVerts,               // BYTE *pVertices,
											 sizeof(D3DQA_DEPTHBUFTEST_VERT), // UINT uiVertexSize,
											 uiNumPointVerts,                  // UINT uiNumVertices,
											 D3DQA_DEPTHBUFTEST_VERT_FVF)))   // DWORD dwFVF
		{
			OutputDebugString(_T("CreateVertexBuffer failed."));
			hr = E_FAIL;
			goto cleanup;
		}


		//
		// Draw geometry:  Points
		//
		if (FAILED(pDevice->DrawPrimitive(D3DMPT_POINTLIST,
										  0,
										  uiNumPoints)))
		{
			OutputDebugString(_T("DrawPrimitive failed."));
			hr = E_FAIL;
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
		OutputDebugString(_T("Invalid LPDIRECT3DMOBILEDEVICE"));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve viewport extents; to be used for vertex data scaling
	//
	if (FAILED(pDevice->GetViewport(&Viewport)))
	{
		OutputDebugString(_T("GetViewport failed"));
		hr = E_FAIL;
		goto cleanup;
	}


	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return E_FAIL;
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
		OutputDebugString(_T("Failure: no support for either D3DMSURFCAPS_SYSVERTEXBUFFER or D3DMSURFCAPS_VIDVERTEXBUFFER\n"));
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(pDevice->CreateVertexBuffer( sizeof(D3DQA_DEPTHBUFTEST_VERT)*D3DQA_DEPTHBUFTEST_VERTCOUNT, // UINT Length,
	                                        0,                               // DWORD Usage,
	                                        D3DQA_DEPTHBUFTEST_VERT_FVF,     // DWORD FVF,
	                                        VertexBufPool,                   // D3DMPOOL Pool,
	                                        &pVBTriangles)))                 // LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBuffer,
	{
		OutputDebugString(_T("Invalid LPDIRECT3DMOBILEDEVICE"));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// If NULL-check fails, inspect CreateVertexBuffer for bad failure case
	//
	if (NULL == pVBTriangles)
	{
		OutputDebugString(_T("Invalid LPDIRECT3DMOBILEVERTEXBUFFER"));
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(pVBTriangles->Lock(0,                   // UINT OffsetToLock,
	                         sizeof(D3DQA_DEPTHBUFTEST_VERT)*D3DQA_DEPTHBUFTEST_VERTCOUNT,  // UINT SizeToLock,
	                         (VOID**)&pVertMem,   // VOID **ppbData,
	                         0)))                 // DWORD Flags
	{
		OutputDebugString(_T("LPDIRECT3DMOBILEVERTEXBUFFER->Lock() failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// If NULL-check fails, inspect Lock for bad failure case
	//
	if (NULL == pVertMem)
	{
		OutputDebugString(_T("Invalid pointer."));
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
	if (FAILED(pVBTriangles->Unlock()))
	{
		OutputDebugString(_T("LPDIRECT3DMOBILEVERTEXBUFFER->Unlock() failed"));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Indicate active vertex buffer
	//
	if (FAILED(pDevice->SetStreamSource(0,pVBTriangles,0)))
	{
		OutputDebugString(_T("LPDIRECT3DMOBILEDEVICE->SetStreamSource() failed"));
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// Enter a scene; indicates that drawing is about to occur
	//
    if( FAILED( pDevice->BeginScene() ) )
	{
		OutputDebugString(_T("BeginScene failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry:  Triangles
	//
	if (FAILED(pDevice->DrawPrimitive(D3DQA_DEPTHBUFTEST_TRIVERT_PT,
	                                  D3DQA_DEPTHBUFTEST_TRIVERTSTART,
	                                  D3DQA_DEPTHBUFTEST_TRIPRIMCOUNT)))
	{
		OutputDebugString(_T("DrawPrimitive failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(RenderLines(pDevice,   // LPDIRECT3DMOBILEDEVICE pDevice,
	                       Viewport)))// D3DMVIEWPORT Viewport
	{
		OutputDebugString(_T("RenderLines failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(RenderPoints(pDevice,   // LPDIRECT3DMOBILEDEVICE pDevice,
	                       Viewport)))// D3DMVIEWPORT Viewport
	{
		OutputDebugString(_T("RenderLines failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(_T("EndScene failed."));
		hr = E_FAIL;
		goto cleanup;
	}
	
	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
    if (FAILED(pDevice->Present( NULL, NULL, NULL, NULL )))
	{
		OutputDebugString(_T("Present failed."));
		hr = E_FAIL;
		goto cleanup;
	}
	

cleanup:

	if (pVBTriangles)
		pVBTriangles->Release();

	return hr;
}
