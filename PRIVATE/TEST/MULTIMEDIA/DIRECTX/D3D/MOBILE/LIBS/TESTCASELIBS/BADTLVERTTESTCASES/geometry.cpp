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
HRESULT FindIteration(UINT uiIteration, D3DMMATRIX *pMatrix, D3DMVECTOR *pVert1, D3DMVECTOR *pVert2, D3DMVECTOR *pVert3, UINT *puiStep)
{
	HRESULT Result = S_OK;
	UINT uiTestSet = 0;
	UINT uiTestCountAccum = 0;

	//
	// Find data for this iteration
	//
	while(1)
	{
		if (uiTestSet > BadVertTestSet.uiCount)
		{
			Result = E_FAIL;
			break;
		}
		
		if ((uiTestCountAccum + BadVertTestSet.pDesc[uiTestSet].uiNumSteps) > uiIteration)
		{
			*puiStep = uiIteration - uiTestCountAccum;
			memcpy(pMatrix, &BadVertTestSet.pDesc[uiTestSet].MultPerStep, sizeof(D3DMMATRIX));
			memcpy(pVert1, &BadVertTestSet.pDesc[uiTestSet].InitialPos1, sizeof(D3DMVECTOR));
			memcpy(pVert2, &BadVertTestSet.pDesc[uiTestSet].InitialPos2, sizeof(D3DMVECTOR));
			memcpy(pVert3, &BadVertTestSet.pDesc[uiTestSet].InitialPos3, sizeof(D3DMVECTOR));
			break;
		}

		uiTestCountAccum += BadVertTestSet.pDesc[uiTestSet].uiNumSteps;
		uiTestSet++;
	}

	return Result;
}

//
// MakeBadVertGeometry
//
//   Creates a vertex buffer that is useful for testing vertices beyond viewport extents. 
//   
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D device
//   LPDIRECT3DMOBILEVERTEXBUFFER *ppVB:  Resultant vertex buffer
//   UINT uiIteration:  Iteration to generate
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT MakeBadVertGeometry(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB, UINT uiIteration)
{
	//
	// Pointer to vertex data
	//
	D3DQA_BADTLVERTTEST *pVerts;

	//
	// Function result
	//
	HRESULT Result = S_OK;

	//
	// Viewport extents
	//
	D3DMVIEWPORT d3dViewport;

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
	// Parameter validation
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		DebugOut(DO_ERROR, _T("Invalid parameter(s)."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Set to NULL for Cleanup purposes
	//
	*ppVB = NULL;

	//
	// Retrieve viewport extents
	//
	if( FAILED( Result = pDevice->GetViewport( &d3dViewport ) ) )
	{
		DebugOut(DO_ERROR, _T("GetViewport failed. hr = 0x%08x"), Result);
		goto cleanup;
	}

	//
	// Create a Vertex Buffer; set stream source and vertex shader type (FVF)
	//
	(*ppVB) = CreateActiveBuffer(pDevice, D3DQA_BADTLVERTTEST_NUMVERTS, D3DQA_BADTLVERTTEST_FVF, sizeof(D3DQA_BADTLVERTTEST), 0);
	if (NULL == (*ppVB))
	{
		DebugOut(DO_ERROR, _T("CreateActiveBuffer failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Locate the test case data
	//
	if (FAILED(Result = FindIteration(uiIteration,   // UINT uiIteration,
							          &Matrix,       // D3DMMATRIX *pMatrix,
							          &Vert1,        // D3DMVECTOR *pVert1
							          &Vert2,        // D3DMVECTOR *pVert2
							          &Vert3,        // D3DMVECTOR *pVert3
							          &uiNumMults))) // UINT *puiStep
	{
		DebugOut(DO_ERROR, _T("FindIteration failed. hr = 0x%08x"), Result);
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
	if( FAILED( Result = (*ppVB)->Lock( 0, D3DQA_BADTLVERTTEST_NUMVERTS * sizeof(D3DQA_BADTLVERTTEST), (VOID**)&pVerts, 0 ) ) )
	{
		DebugOut(DO_ERROR, _T("Failure while attempting to lock a vertex buffer. hr = 0x%08x"), Result);
		goto cleanup;
	}

	//
	// Fill vertices; scaling and offseting based on viewport width/height/offsets
	//
	// Note that with D3DMSHADE_FLAT, only the first vertex color has an effect on rasterization.
	//

	pVerts[0].x = (((*(float*)&(Vert1.x)) + 1.0f)/2.0f) * (float)d3dViewport.Width + (float)d3dViewport.X;
	pVerts[0].y = (1.0f - (((*(float*)&(Vert1.y)) + 1.0f)/2.0f)) * (float)d3dViewport.Height + (float)d3dViewport.Y;
	pVerts[0].z = *(float*)&(Vert1.z);
	pVerts[0].rhw = 1.0f;
	pVerts[0].Diffuse = D3DMCOLOR_XRGB(0,255,0);

	pVerts[1].x = (((*(float*)&(Vert2.x)) + 1.0f)/2.0f) * (float)d3dViewport.Width + (float)d3dViewport.X;
	pVerts[1].y = (1.0f - (((*(float*)&(Vert2.y)) + 1.0f)/2.0f)) * (float)d3dViewport.Height + (float)d3dViewport.Y;
	pVerts[1].z = *(float*)&(Vert2.z);
	pVerts[1].rhw = 1.0f;
	pVerts[1].Diffuse = D3DMCOLOR_XRGB(255,0,0);

	pVerts[2].x = (((*(float*)&(Vert3.x)) + 1.0f)/2.0f) * (float)d3dViewport.Width + (float)d3dViewport.X;
	pVerts[2].y = (1.0f - (((*(float*)&(Vert3.y)) + 1.0f)/2.0f)) * (float)d3dViewport.Height + (float)d3dViewport.Y;
	pVerts[2].z = *(float*)&(Vert3.z); 
	pVerts[2].rhw = 1.0f;
	pVerts[2].Diffuse = D3DMCOLOR_XRGB(0,0,255);

	if( FAILED( Result = (*ppVB)->Unlock() ) )
	{
		DebugOut(DO_ERROR, _T("Failure while attempting to unlock a vertex buffer. hr = 0x%08x"), Result);
		goto cleanup;
	}

cleanup:

	if ((FAILED(Result)) && (NULL != ppVB) && (NULL != *ppVB))
		(*ppVB)->Release();

	return Result;
}

