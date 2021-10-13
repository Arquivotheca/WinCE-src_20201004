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
#include <tchar.h>
#include <d3dm.h>
#include <tux.h>
#include "BufferTools.h"
#include "Geometry.h"
#include "qamath.h"
#include "DebugOutput.h"

//
// FindIteration
//
//   Indicates the data for a single test case, by locating it within the larger data structure.
//   
// Arguments:
//
//   UINT uiIteration: Desired iteration
//   D3DMMATRIX *pMatrix:  \
//   D3DMVECTOR *pVert1:    \
//   D3DMVECTOR *pVert2:     Test case data
//   D3DMVECTOR *pVert3:    /
//   UINT *puiStep:       /
//   
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT FindIteration(UINT uiIteration, D3DMMATRIX *pMatrix, D3DMVECTOR *pVert1, D3DMVECTOR *pVert2, D3DMVECTOR *pVert3, D3DMCULL *pCullMode, UINT *puiStep)
{
	HRESULT Result = S_OK;
	UINT uiTestSet = 0;
	UINT uiTestCountAccum = 0;

	//
	// Find data for this iteration
	//
	while(1)
	{
		if (uiTestSet > CullPrimTestSet.uiCount)
		{
			Result = E_FAIL;
			break;
		}
		
		if ((uiTestCountAccum + CullPrimTestSet.pDesc[uiTestSet].uiNumSteps) > uiIteration)
		{
			*puiStep = uiIteration - uiTestCountAccum;
			memcpy(pMatrix, &CullPrimTestSet.pDesc[uiTestSet].MultPerStep, sizeof(D3DMMATRIX));
			memcpy(pVert1, &CullPrimTestSet.pDesc[uiTestSet].InitialPos1, sizeof(D3DMVECTOR));
			memcpy(pVert2, &CullPrimTestSet.pDesc[uiTestSet].InitialPos2, sizeof(D3DMVECTOR));
			memcpy(pVert3, &CullPrimTestSet.pDesc[uiTestSet].InitialPos3, sizeof(D3DMVECTOR));
			*pCullMode = CullPrimTestSet.pDesc[uiTestSet].CullMode;
			break;
		}

		uiTestCountAccum += CullPrimTestSet.pDesc[uiTestSet].uiNumSteps;
		uiTestSet++;
	}

	return Result;
}

//
// MakeCullPrimGeometry
//
//   Creates a vertex buffer that is useful for cull testing. 
//   
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   LPDIRECT3DMOBILEVERTEXBUFFER *ppVB:  Resultant vertex buffer
//   UINT uiIteration:  Iteration to generate
//    
// Return Value:
//    
//   INT:  TPR_PASS, TPR_FAIL, TPR_ABORT, or TPR_SKIP
//
INT MakeCullPrimGeometry(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB, UINT uiIteration)
{
	//
	// Pointer to vertex data
	//
	D3DQA_CULLPRIMTEST *pVerts;

    HRESULT hr;

	//
	// Function result
	//
	INT Result = TPR_PASS;

	//
	// Viewport extents
	//
	D3DMVIEWPORT d3dViewport;

	//
	// Culling mode for this iteration
	//
	D3DMCULL CullMode;

	//
	// Variables for computing geometry coordinates
	//
	D3DMMATRIX Matrix;
	UINT uiNumMults;
	D3DMVECTOR Vert1;
	D3DMVECTOR Vert2;
	D3DMVECTOR Vert3;
	UINT uiIter;

	//
	// Vertex color is computed to be unique for each test case
	//
	BYTE bRed;
	BYTE bGreen;
	BYTE bBlue;
	DWORD dwVertexColor;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;
	
	//
	// Parameter validation
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		DebugOut(DO_ERROR, _T("Invalid parameter(s). Aborting."));
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Set to NULL for Cleanup purposes
    //
	*ppVB = NULL;

	//
	// Retrieve viewport extents
	//
	if( FAILED( hr = pDevice->GetViewport( &d3dViewport ) ) )
	{
		DebugOut(DO_ERROR, _T("GetViewport failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Create a Vertex Buffer; set stream source and vertex shader type (FVF)
	//
	(*ppVB) = CreateActiveBuffer(pDevice, D3DQA_CULLPRIM_TEST_NUMVERTS, D3DQA_CULLPRIMTEST_FVF, sizeof(D3DQA_CULLPRIMTEST), 0);
	if (NULL == (*ppVB))
	{
		DebugOut(DO_ERROR, _T("CreateActiveBuffer failed. Aborting."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Locate the test case data
	//
	if (FAILED(hr = FindIteration(uiIteration,   // UINT uiIteration,
							      &Matrix,       // D3DMMATRIX *pMatrix,
							      &Vert1,        // D3DMVECTOR *pVert1
							      &Vert2,        // D3DMVECTOR *pVert2
							      &Vert3,        // D3DMVECTOR *pVert3
							      &CullMode,     // D3DMCULL *pCullMode
							      &uiNumMults))) // UINT *puiStep
	{
		DebugOut(DO_ERROR, _T("FindIteration failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// If attempting to set culling to D3DMCULL_NONE,
	// verify that this capability is available
	//
   	if ((!(Caps.PrimitiveMiscCaps & D3DMPMISCCAPS_CULLNONE)) && 
		(D3DMCULL_NONE == CullMode))
	{
		DebugOut(DO_ERROR, _T("D3DMPMISCCAPS_CULLNONE not supported. Skipping."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// If attempting to set culling to D3DMCULL_CW,
	// verify that this capability is available
	//
   	if ((!(Caps.PrimitiveMiscCaps & D3DMPMISCCAPS_CULLCW)) && 
		(D3DMCULL_CW == CullMode))
	{
		DebugOut(DO_ERROR, _T("D3DMPMISCCAPS_CULLCW not supported. Skipping."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// If attempting to set culling to D3DMCULL_CCW,
	// verify that this capability is available
	//
   	if ((!(Caps.PrimitiveMiscCaps & D3DMPMISCCAPS_CULLCCW)) && 
		(D3DMCULL_CCW == CullMode))
	{
		DebugOut(DO_ERROR, _T("D3DMPMISCCAPS_CULLCCW not supported. Skipping."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare culling settings
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CULLMODE, CullMode)))
	{
		DebugOut(DO_ERROR, _T("SetRenderState failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	// 
	// Compute coordinates
	// 
	for (uiIter = 0; uiIter < uiNumMults; uiIter++)
	{
		Vert1 = TransformVector(&Vert1, &Matrix);
		Vert2 = TransformVector(&Vert2, &Matrix);
		Vert3 = TransformVector(&Vert3, &Matrix);
	}

	//
	// Set up input vertices (lock, copy data into buffer, unlock)
	//
	if( FAILED( hr = (*ppVB)->Lock( 0, D3DQA_CULLPRIM_TEST_NUMVERTS * sizeof(D3DQA_CULLPRIMTEST), (VOID**)&pVerts, 0 ) ) )
	{
		DebugOut(DO_ERROR, _T("Failure while attempting to lock a vertex buffer. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Vary primitive color by test case; for flat shading, the first vertex color is the only
	// one that is relavent for rasterization with D3DMSHADE_FLAT
	//
	// Background is red, use *other* colors for the primitive.
	//
	bRed = 0;   
	bGreen = (uiIteration*16) % 256;   // Multiplier helps to accentuate the color stepping
	bBlue = 255 - bGreen;             // between consecutive test cases

	dwVertexColor = D3DMCOLOR_XRGB(bRed, bGreen, bBlue);


	//
	// Fill vertices; scaling and offseting based on viewport width/height/offsets
	//
	pVerts[0].x = (((*(float*)&(Vert1.x)) + 1.0f)/2.0f) * ((float)d3dViewport.Width-1.0f) + ((float)d3dViewport.X);
	pVerts[0].y = (1.0f - (((*(float*)&(Vert1.y)) + 1.0f)/2.0f)) * ((float)d3dViewport.Height-1.0f) + ((float)d3dViewport.Y);
	pVerts[0].z = (((*(float*)&(Vert1.z)) + 1.0f)/2.0f);
	pVerts[0].rhw = 1.0f;
	pVerts[0].Diffuse = dwVertexColor;

	pVerts[1].x = (((*(float*)&(Vert2.x)) + 1.0f)/2.0f) * ((float)d3dViewport.Width-1.0f) + ((float)d3dViewport.X);
	pVerts[1].y = (1.0f - (((*(float*)&(Vert2.y)) + 1.0f)/2.0f)) * ((float)d3dViewport.Height-1.0f) + ((float)d3dViewport.Y);
	pVerts[1].z = (((*(float*)&(Vert2.z)) + 1.0f)/2.0f);
	pVerts[1].rhw = 1.0f;
	pVerts[1].Diffuse = D3DMCOLOR_XRGB(0,0,0);

	pVerts[2].x = (((*(float*)&(Vert3.x)) + 1.0f)/2.0f) * ((float)d3dViewport.Width-1.0f) + ((float)d3dViewport.X);
	pVerts[2].y = (1.0f - (((*(float*)&(Vert3.y)) + 1.0f)/2.0f)) * ((float)d3dViewport.Height-1.0f) + ((float)d3dViewport.Y);
	pVerts[2].z = (((*(float*)&(Vert3.z)) + 1.0f)/2.0f); 
	pVerts[2].rhw = 1.0f;
	pVerts[2].Diffuse = D3DMCOLOR_XRGB(0,0,0);

	if( FAILED( hr = (*ppVB)->Unlock() ) )
	{
		DebugOut(DO_ERROR, _T("Failure while attempting to unlock a vertex buffer. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

cleanup:

	if ((TPR_PASS != Result) && (NULL != ppVB) && (NULL != *ppVB))
		(*ppVB)->Release();

	return Result;
}

