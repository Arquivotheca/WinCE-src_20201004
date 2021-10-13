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
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "Geometry.h"
#include "TestCases.h"
#include "DebugOutput.h"

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
    HRESULT hr;
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
		DebugOut(DO_ERROR, _T("Invalid argument(s). Aborting."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	// 
	// Create geometry for test scene
	// 
	if (FAILED(hr = MakeBadVertGeometry(pDevice, &pVB, pTestCaseArgs->dwTestIndex)))
	{
		DebugOut(DO_ERROR, _T("MakeBadVertGeometry failed. (hr = 0x%08x) Aborting."), hr);
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
	if (FAILED(hr = pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255,0,0) , 1.0f, 0x00000000 )))
	{
		DebugOut(DO_ERROR, _T("Clear failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(hr = pDevice->BeginScene()))
	{
		DebugOut(DO_ERROR, _T("BeginScene failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,D3DQA_BADTLVERTTEST_NUMPRIM)))
	{
		DebugOut(DO_ERROR, _T("DrawPrimitive failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(hr = pDevice->EndScene()))
	{
		DebugOut(DO_ERROR, _T("EndScene failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(hr = pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		DebugOut(DO_ERROR, _T("Present failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


cleanup:

	if (pVB)
		pVB->Release();

	return Result;
}
