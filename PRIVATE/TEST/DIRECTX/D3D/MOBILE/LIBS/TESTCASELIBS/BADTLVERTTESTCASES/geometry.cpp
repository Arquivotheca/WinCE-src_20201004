//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>
#include <d3dm.h>
#include "BufferTools.h"
#include "Geometry.h"
#include "qamath.h"


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
		OutputDebugString(_T("Invalid parameter(s)."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve viewport extents
	//
	if( FAILED( pDevice->GetViewport( &d3dViewport ) ) )
	{
		OutputDebugString(_T("GetViewport failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Create a Vertex Buffer; set stream source and vertex shader type (FVF)
	//
	(*ppVB) = CreateActiveBuffer(pDevice, D3DQA_BADTLVERTTEST_NUMVERTS, D3DQA_BADTLVERTTEST_FVF, sizeof(D3DQA_BADTLVERTTEST), 0);
	if (NULL == (*ppVB))
	{
		OutputDebugString(_T("CreateActiveBuffer failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Locate the test case data
	//
	if (FAILED(FindIteration(uiIteration,   // UINT uiIteration,
							&Matrix,       // D3DMMATRIX *pMatrix,
							&Vert1,        // D3DMVECTOR *pVert1
							&Vert2,        // D3DMVECTOR *pVert2
							&Vert3,        // D3DMVECTOR *pVert3
							&uiNumMults))) // UINT *puiStep
	{
		OutputDebugString(_T("FindIteration failed."));
		Result = E_FAIL;
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
	if( FAILED( (*ppVB)->Lock( 0, D3DQA_BADTLVERTTEST_NUMVERTS * sizeof(D3DQA_BADTLVERTTEST), (VOID**)&pVerts, 0 ) ) )
	{
		OutputDebugString(_T("Failure while attempting to lock a vertex buffer."));
		Result = E_FAIL;
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

	if( FAILED( (*ppVB)->Unlock() ) )
	{
		OutputDebugString(_T("Failure while attempting to unlock a vertex buffer."));
		Result = E_FAIL;
		goto cleanup;
	}

cleanup:

	if ((FAILED(Result)) && (NULL != *ppVB))
		(*ppVB)->Release();

	return Result;
}

