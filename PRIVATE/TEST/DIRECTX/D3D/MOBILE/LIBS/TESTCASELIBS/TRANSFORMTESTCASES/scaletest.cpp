//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "Geometry.h"
#include "Permutations.h"
#include "ImageManagement.h"
#include "qamath.h"
#include "Transforms.h"
#include "TransformWrapper.h"
#include "ScaleTest.h"
#include <d3dm.h>
#include <tux.h>

TESTPROCAPI ScaleTest(LPDIRECT3DMOBILEDEVICE pDevice,
                      D3DMTRANSFORMSTATETYPE TransMatrix,
                      D3DQA_SCALETYPE ScaleType,
                      UINT uiFrameNumber,
					  D3DMFORMAT Format,
                      UINT uiTestID, 
                      WCHAR *wszImageDir,
                      UINT uiInstanceHandle,
                      HWND hWnd,
                      LPTESTCASEARGS pTestCaseArgs)
{
	//
	// Level of detail in sphere model
	//
	CONST UINT uiNumDivisions = 12;

	//
	// Scale factors, per axis
	//
	float fX, fY, fZ;

	//
	// Temporary values used while computing per-axis scale factors
	//
	float fAxis1, fAxis2;

	//
	// Number of axis used for a particular scale
	//
	UINT uiNumAxis;

	//
	// Each test case consists of multiple rendered frames
	//
	UINT uiFrame;

	//
	// Scale matrix to be applied to Direct3D TnL
	//
	D3DMMATRIX ScaleMatrix;

	//
	// Interfaces for scene data
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVBSphere = NULL;
	LPDIRECT3DMOBILEINDEXBUFFER pIBSphere = NULL;

	//
	// All failure conditions set this to an error
	//
	INT iTestResult = TPR_PASS;

	//
	// Number of frames expected to be generated by this test case
	//
	UINT uiNumDesiredFrames;

	//
	// The geometry-generating algorithm reports it's "specs"; useful at
	// DrawPrimitive-time
	//
	D3DMPRIMITIVETYPE PrimType;
	UINT uiPrimCount;
	UINT uiNumVertices;

	UINT uiTotalFrames;

	//
	// Flat shading is in base profile
	//
	pDevice->SetRenderState( D3DMRS_SHADEMODE, D3DMSHADE_FLAT );

	//
	// Lighting is "baked" in verts
	//
	pDevice->SetRenderState( D3DMRS_LIGHTING, 0 );

	pDevice->SetRenderState(D3DMRS_CULLMODE, D3DMCULL_CW);

	//
	// Enable clipping
	//
	if( FAILED( pDevice->SetRenderState(D3DMRS_CLIPPING, TRUE)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_CLIPPING,TRUE) failed.\n"));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(GetAxisCount(&uiNumAxis, ScaleType)))
	{
		OutputDebugString(_T("GetNumberofAxis failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	//
	// Valid range is [1,3]
	//
	if (!((1 <= uiNumAxis) &&
	      (3 >= uiNumAxis)))
	{
		OutputDebugString(_T("GetAxisCount resulted in invalid number of axis.\n"));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve expected number of frames for this kind of test case
	//
	uiNumDesiredFrames = NumTransformFramesByIters[uiNumAxis];

	if (FAILED(GetPermutationSpecs(uiNumAxis,                // UINT uiNumIters,
	                               uiNumDesiredFrames,       // UINT uiMaxIter,
	                               &uiTotalFrames,           // UINT *puiNumStepsTotal,
	                               NULL)))
	{
		OutputDebugString(_T("GetPermutationSpecs failed.\n"));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	//
	// Confirm that test case can generate exactly the desired number of frames
	//
	if (uiNumDesiredFrames != uiTotalFrames)
	{
		OutputDebugString(_T("Unexpected number of frames.\n"));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(GenerateSphere( pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
	                           &pVBSphere,        // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                           Format,            // D3DMFORMAT Format,
	                           &pIBSphere,        // LPDIRECT3DMOBILEINDEXBUFFER *ppIB, 
	                           uiNumDivisions,    // const int nDivisions
	                           &PrimType,         // D3DMPRIMITIVETYPE *pPrimType
	                           &uiPrimCount,      // PUINT puiPrimCount
	                           &uiNumVertices)))  // PUINT puiNumVertices
	{
		OutputDebugString(_T("GenerateSphere failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	uiFrame = uiFrameNumber;

	if (FAILED(SetTransformDefaults(pDevice))) // LPDIRECT3DMOBILEDEVICE pDevice
	{
		OutputDebugString(_T("SetTransformDefaults failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	switch(uiNumAxis)
	{
	case 1:
		if (FAILED(OneAxisPermutations(uiFrame,                 // UINT uiFrame,
			                           D3DQA_SCALEFACTOR_MIN,   // float fMin,
			                           D3DQA_SCALEFACTOR_MAX,   // float fMax,
			                           uiNumDesiredFrames,      // UINT uiMaxIters,
			                           &fAxis1)))               // float *pfAxis,
		{
			OutputDebugString(_T("OneAxisPermutations failed."));
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		// Default all axis to no scaling
		fX = fY = fZ = 1.0f;

		if (UsesAxis(D3DQA_SCALEAXIS_X, ScaleType))
			fX = fAxis1;

		if (UsesAxis(D3DQA_SCALEAXIS_Y, ScaleType))
			fY = fAxis1;

		if (UsesAxis(D3DQA_SCALEAXIS_Z, ScaleType))
			fZ = fAxis1;

		if (false == GetScale(ScaleType,    // D3DQA_ScaleType rType,
			                  fX,           // const float fXScale,
			                  fY,           // const float fYScale,
			                  fZ,           // const float fZScale,
			                  &ScaleMatrix))// D3DMMATRIX* const ScaleMatrix,
		{
			OutputDebugString(_T("GetScale failed."));
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		break;
	case 2:
		if (FAILED(TwoAxisPermutations(uiFrame,                 // UINT uiFrame,
			                           D3DQA_SCALEFACTOR_MIN,   // float fMin,
			                           D3DQA_SCALEFACTOR_MAX,   // float fMax,
			                           uiNumDesiredFrames,      // UINT uiMaxIters,
			                           &fAxis1,                 // float *pfAxis1,
			                           &fAxis2)))               // float *pfAxis2,
		{
			OutputDebugString(_T("TwoAxisPermutations failed."));
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		// Default all axis to no scaling
		fX = fY = fZ = 1.0f;

		if (false == UsesAxis(D3DQA_SCALEAXIS_X, ScaleType))
		{
			fY = fAxis1;
			fZ = fAxis2;
		}
		else if (false == UsesAxis(D3DQA_SCALEAXIS_Y, ScaleType))
		{
			fX = fAxis1;
			fZ = fAxis2;
		}
		else if (false == UsesAxis(D3DQA_SCALEAXIS_Z, ScaleType))
		{
			fX = fAxis1;
			fY = fAxis2;
		}
		else
		{
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		if (false == GetScale(ScaleType,    // D3DQA_ScaleType rType,
			                  fX,           // const float fXScale,
			                  fY,           // const float fYScale,
			                  fZ,           // const float fZScale,
			                  &ScaleMatrix))// D3DMMATRIX* const ScaleMatrix,
		{
			OutputDebugString(_T("GetScale failed."));
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		break;
	case 3:
		if (FAILED(ThreeAxisPermutations(uiFrame,                 // UINT uiFrame,
			                             D3DQA_SCALEFACTOR_MIN,   // float fMin,
			                             D3DQA_SCALEFACTOR_MAX,   // float fMax,
			                             uiNumDesiredFrames,      // UINT uiMaxIters,
			                             &fX,                     // float *pfX,
			                             &fY,                     // float *pfY,
			                             &fZ )))                  // float *pfZ
		{
			OutputDebugString(_T("ThreeAxisPermutations failed."));
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		if (false == GetScale(ScaleType,    // D3DQA_ScaleType rType,
			                  fX,           // const float fXScale,
			                  fY,           // const float fYScale,
			                  fZ,           // const float fZScale,
			                  &ScaleMatrix))// D3DMMATRIX* const ScaleMatrix,
		{
			OutputDebugString(_T("GetScale failed."));
			iTestResult = TPR_FAIL;
			goto cleanup;
		}

		break;
	default:
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(pDevice->SetTransform(TransMatrix,  // D3DMTRANSFORMSTATETYPE State,
		                             &ScaleMatrix, D3DMFMT_D3DMVALUE_FLOAT))) // CONST D3DMMATRIX *pMatrix
	{
		OutputDebugString(_T("SetTransform failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	//
	// Clear the backbuffer and the zbuffer
	//
	pDevice->Clear( 0, NULL, D3DMCLEAR_TARGET|D3DMCLEAR_ZBUFFER,
						D3DMCOLOR_XRGB(0,0,0), 1.0f, 0 );

	if (FAILED(pDevice->BeginScene()))
	{
		OutputDebugString(_T("BeginScene failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(pDevice->DrawIndexedPrimitive(
						 PrimType,       // D3DMPRIMITIVETYPE Type,
						 0,              // INT BaseVertexIndex,
						 0,              // UINT MinIndex,
						 uiNumVertices,  // UINT NumVertices,
						 0,              // UINT StartIndex,
						 uiPrimCount)))  // UINT PrimitiveCount
	{
		OutputDebugString(_T("DrawIndexedPrimitive failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(_T("EndScene failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(_T("Present failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                            0,             // UINT uiFrameNumber,
	                            NULL)))        // RECT *pRect = NULL
	{
		OutputDebugString(_T("DumpFlushedFrame failed."));
		iTestResult = TPR_FAIL;
		goto cleanup;
	}



cleanup:

	if (FAILED(pDevice->SetIndices(NULL)))
	{
		OutputDebugString(_T("SetIndices failed."));
		iTestResult = TPR_FAIL;
	}

	if (pVBSphere)
		pVBSphere->Release();

	if (pIBSphere)
		pIBSphere->Release();

	return iTestResult;
}


