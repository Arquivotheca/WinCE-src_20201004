//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "Geometry.h"
#include "TestCases.h"

//
// BadVertTest
//
//   Renders a single test case from a predefined set.
//   
// Arguments:
//
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//    
// Return Value:
//    
//   INT:  TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT
//
INT BadVertTest(LPTESTCASEARGS pTestCaseArgs)
{
	//
	// Result of test case execution
	//
	INT Result = TPR_PASS;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// Scene geometry variables
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		OutputDebugString(_T("Invalid argument(s)."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	// 
	// Create geometry for test scene
	// 
	if (FAILED(MakeBadVertGeometry(pDevice, &pVB, pTestCaseArgs->dwTestIndex)))
	{
		OutputDebugString(_T("MakeBadVertGeometry failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Possibly compel the driver to try to draw even "bad" back-facing primitives
	//
	pDevice->SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE);

	//
	// Prepare test case permutation
	//
	if (FAILED(pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255,0,0) , 1.0f, 0x00000000 )))
	{
		OutputDebugString(_T("Clear failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(pDevice->BeginScene()))
	{
		OutputDebugString(_T("BeginScene failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,D3DQA_BADTLVERTTEST_NUMPRIM)))
	{
		OutputDebugString(_T("DrawPrimitive failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(_T("EndScene failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(_T("Present failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}


cleanup:

	if (pVB)
		pVB->Release();

	return Result;
}
