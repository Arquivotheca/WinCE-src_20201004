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
#include "Geometry.h"
#include "LightCases.h"
#include "LightTestFunctions.h"
#include "ImageManagement.h"
#include <tux.h>
#include "DebugOutput.h"

//
// LightTest
//
//   Render and capture a single test frame; corresponding to a test case
//   for lighting.
//
// Arguments:
//   
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:
//
//   TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
//
TESTPROCAPI LightTest(LPTESTCASEARGS pTestCaseArgs, LPFRAMEDESCRIPTION pFrameDescription)

{
	//
	// Function pointers; all light tests conform to same
	// function signatures
	//
	SET_STATES           pfnSetStates;
	SUPPORTS_TABLE_INDEX pfnSupportsTableIndex;
	SETUP_GEOMETRY       pfnSetupGeometry;

    HRESULT hr;
    
	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// Result of test case execution
	//
	INT Result = TPR_PASS;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Vertex buffer to be rendered
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

	//
	// Index into local test case table
	//
	DWORD dwTableIndex;

	//
	// Should we perform alpha blending?
	//
	BOOL bAlphaBlend = FALSE;

	//
	// What is the diffuse alpha we will use?
	//
	BYTE DiffuseAlpha = 0xff;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Prepare for the current frame.
    //
	pFrameDescription->FrameDescription = LightTestFrames[pFrameDescription->FrameIndex].FrameDescription;
	if (D3DMALPHA_NOALPHA == LightTestFrames[pFrameDescription->FrameIndex].FrameAlpha)
	{
	    bAlphaBlend = FALSE;
	    DiffuseAlpha = 0xff;
	}
	else
	{
	    bAlphaBlend = TRUE;
	    DiffuseAlpha = (BYTE)(LightTestFrames[pFrameDescription->FrameIndex].FrameAlpha & 0xff);
	}
	DebugOut(L"Running Light test for frame %d (%s)", pFrameDescription->FrameIndex, pFrameDescription->FrameDescription);

	//
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_LIGHTTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_LIGHTTEST_MAX))
	{
		//
		// Lighting test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_LIGHTTEST_BASE;

		pfnSupportsTableIndex = SupportsLightingTableIndex;
		pfnSetupGeometry      = SetupLightingGeometry;
		pfnSetStates          = SetLightingStates;
	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index (%d). Failing.", pTestCaseArgs->dwTestIndex);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Verify supported capabilities
	//
	if (FAILED(pfnSupportsTableIndex(pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
	                                 dwTableIndex,   // DWORD dwTableIndex
	                                 bAlphaBlend)))  // BOOL bAlphaBlend
	{
		DebugOut(DO_ERROR, L"pfnSupportsTableIndex failed.  Device does not support this test case. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create geometry for this test case
	//
	if (FAILED(hr = pfnSetupGeometry(pDevice,              // LPDIRECT3DMOBILEDEVICE pDevice,
	                                 pTestCaseArgs->hWnd,  // HWND hWnd,
	                                 DiffuseAlpha,         // BYTE DiffuseAlpha
	                                 &pVB)))               // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB);
	{
		DebugOut(DO_ERROR, L"pfnSetupGeometry failed. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare render states
	//
	if (FAILED(hr = pfnSetStates(pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
	                             dwTableIndex,   // DWORD dwTableIndex
	                             bAlphaBlend,   
	                             DiffuseAlpha)))
	{
		DebugOut(DO_ERROR, L"pfnSetStates failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Clear render target
	//
	if (FAILED(hr = pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(hr = pDevice->BeginScene()))
	{
		DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DQA_PRIMTYPE,0,D3DQA_NUMPRIM)))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(hr = pDevice->EndScene()))
	{
		DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(hr = pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	
	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs,                 // LPTESTCASEARGS pTestCaseArgs
                                     pFrameDescription->FrameIndex, // UINT uiFrameNumber,
	                                 NULL)))                        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pVB)
		pVB->Release();

	return Result;
}
