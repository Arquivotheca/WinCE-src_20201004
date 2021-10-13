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
#include "ImageManagement.h"
#include "TestCases.h"

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
		OutputDebugString(_T("Invalid argument(s)."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_CULLTEST_BASE;
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
	Result = MakeCullPrimGeometry(pDevice, &pVB, dwTableIndex);
	if (TPR_PASS != Result)
	{
		OutputDebugString(_T("MakeCullPrimGeometry did not succeed."));
		goto cleanup;
	}

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
	// Turn off lighting
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_LIGHTING, FALSE) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Use flat shading
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT) failed."));
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
	if (FAILED(pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,D3DQA_CULLPRIM_TEST_NUMPRIM)))
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

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                            0,             // UINT uiFrameNumber,
	                            NULL)))        // RECT *pRect = NULL
	{
		OutputDebugString(_T("DumpFlushedFrame failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pVB)
		pVB->Release();

	return Result;
}
