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
#include "ImageManagement.h"
#include "TestCases.h"
#include "DebugOutput.h"

//
// CullPrimTest
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
TESTPROCAPI CullPrimTest(LPTESTCASEARGS pTestCaseArgs)
{
	//
	// Result of test case execution
	//
	INT Result = TPR_PASS;

    HRESULT hr;

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
	// The index into the test case table (derived from the test index)
	//
	DWORD dwTableIndex;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, _T("Invalid argument(s). Aborting."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_CULLTEST_BASE;
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
	Result = MakeCullPrimGeometry(pDevice, &pVB, dwTableIndex);
	if (TPR_PASS != Result)
	{
		DebugOut(DO_ERROR, _T("MakeCullPrimGeometry did not succeed. (hr = 0x%08x)."), hr);
		goto cleanup;
	}

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
	// Turn off lighting
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_LIGHTING, FALSE) failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Use flat shading
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT) failed. (hr = 0x%08x) Aborting."), hr);
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
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,D3DQA_CULLPRIM_TEST_NUMPRIM)))
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

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                                 0,             // UINT uiFrameNumber,
	                                 NULL)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, _T("DumpFlushedFrame failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pVB)
		pVB->Release();

	return Result;
}
