//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "Geometry.h"
#include "FogTestFunctions.h"
#include "ImageManagement.h"
#include <tux.h>

//
// FogTest
//
//   Render and capture a single test frame; corresponding to a test case
//   for vertex or pixel fog.
//
// Arguments:
//   
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:
//
//   TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
//
TESTPROCAPI FogTest(LPTESTCASEARGS pTestCaseArgs)

{
	//
	// Function pointers; vertex fog and pixel fog tests
	// conform to same function signatures
	//
	SET_STATES           pfnSetStates;
	SUPPORTS_TABLE_INDEX pfnSupportsTableIndex;
	SETUP_GEOMETRY       pfnSetupGeometry;

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
		OutputDebugString(L"GetDeviceCaps failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}


	//
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_PIXELFOGTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_PIXELFOGTEST_MAX))
	{
		//
		// Pixel fog test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_PIXELFOGTEST_BASE;

		pfnSupportsTableIndex = SupportsPixelFogTableIndex;
		pfnSetupGeometry      = SetupPixelFogGeometry;
		pfnSetStates          = SetPixelFogStates;
	}
	else if ((pTestCaseArgs->dwTestIndex >= D3DMQA_VERTEXFOGTEST_BASE) &&
	         (pTestCaseArgs->dwTestIndex <= D3DMQA_VERTEXFOGTEST_MAX))
	{
		//
		// Vertex fog test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_VERTEXFOGTEST_BASE;

		pfnSupportsTableIndex = SupportsVertexFogTableIndex;
		pfnSetupGeometry      = SetupVertexFogGeometry;
		pfnSetStates          = SetVertexFogStates;
	}
	else
	{
		OutputDebugString(L"Invalid test index.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Verify supported capabilities
	//
	if (FAILED(pfnSupportsTableIndex(pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
	                                 dwTableIndex))) // DWORD dwTableIndex
	{
		OutputDebugString(L"pfnSupportsTableIndex failed.  Device does not support this test case; skipping.\n");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Create geometry for this test case
	//
	if (FAILED(pfnSetupGeometry(pDevice,              // LPDIRECT3DMOBILEDEVICE pDevice,
	                            pTestCaseArgs->hWnd,  // HWND hWnd,
	                            &pVB)))               // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB);
	{
		OutputDebugString(L"pfnSetupGeometry failed.\n");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare render states
	//
	if (FAILED(pfnSetStates(pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
	                        dwTableIndex))) // DWORD dwTableIndex
	{
		OutputDebugString(L"pfnSetStates failed.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Clear render target
	//
	if (FAILED(pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		OutputDebugString(L"Clear failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(pDevice->BeginScene()))
	{
		OutputDebugString(L"BeginScene failed.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(pDevice->DrawPrimitive(D3DQA_PRIMTYPE,0,D3DQA_NUMPRIM)))
	{
		OutputDebugString(L"DrawPrimitive failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(L"EndScene failed.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(L"Present failed.\n");
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

	//
	// Detaching textures will remove remaining reference count
	//
	if (pDevice)
		pDevice->SetTexture(0,NULL);

	if (pVB)
		pVB->Release();


	return Result;
}



//
// Future expansion:
//
//   * Verify interaction between D3DMRS_SPECULARMATERIALSOURCE and specular alpha
//
