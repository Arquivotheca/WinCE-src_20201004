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
#include "AlphaTestCases.h"
#include "AlphaTestTestFunctions.h"
#include "ImageManagement.h"
#include "utils.h"
#include <tux.h>
#include "DebugOutput.h"

//
// AlphaTestTest
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
INT AlphaTestTest(LPTESTCASEARGS pTestCaseArgs, LPFRAMEDESCRIPTION const pFrameDescription)

{
    using namespace AlphaTestNamespace;
	//
	// API Result code
	//
	HRESULT hr;

	//
	// Texture capabilities required by this test.
	//
	CONST DWORD RequiredTextureOpCap = D3DMTEXOPCAPS_SELECTARG1;

	//
	// Test Result
	//
	INT Result = TPR_PASS;

	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// Texture to wrap with
	//
	LPDIRECT3DMOBILETEXTURE pTexture = NULL;

	//
	// The vertex buffer which contains the test primitives.
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVertexBuffer = NULL;
	UINT VertexBufferStride = 0;

	//
	// The index into the test case table, based off of the test case ID
	//
	DWORD dwTableIndex;

	//
	// The number of passes required for the texture wrapping. (Hopefully 1).
	//
	DWORD dwNumPasses;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Format of the texture to use
	//
	D3DMFORMAT Format;

	//
	// Color to fill the texture with
	//
	D3DMCOLOR Color;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice) || (NULL == pFrameDescription))
	{
		DebugOut(DO_ERROR, _T("Invalid test case argument(s). Aborting."));
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Prepare for the current frame.
    //
	pFrameDescription->FrameDescription = AlphaTestFrames[pFrameDescription->FrameIndex].tszFrameDescription;
	Format = AlphaTestFrames[pFrameDescription->FrameIndex].TextureFormat;
	Color  = AlphaTestFrames[pFrameDescription->FrameIndex].FillColor;
	DebugOut(L"Running AlphaTest test for frame %d (%s)", pFrameDescription->FrameIndex, pFrameDescription->FrameDescription);

	pDevice = pTestCaseArgs->pDevice;

	//
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_ALPHATESTTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_ALPHATESTTEST_MAX))
	{
		//
		// Lighting test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_ALPHATESTTEST_BASE;
	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

    if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"Could not retrieve device capabilities. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

	//
	// Verify that texture op, required for this test, is supported
	//
	if (!(Caps.TextureOpCaps & RequiredTextureOpCap))
	{
		DebugOut(DO_ERROR, _T("Inadequate TextureOpCaps; Skipping."));
		Result = TPR_SKIP;
		goto cleanup;
	}


    /////////////////////////////////////////////////////
    //
    // Prepare our test objects
    //
    /////////////////////////////////////////////////////
	
    //
    // Create test texture(s)
    //

    if (FAILED(hr = CreateAndPrepareTexture(pDevice, Format, Color, dwTableIndex, &pTexture)))
    {
        DebugOut(DO_ERROR, L"Could not create and prepare texture. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        DebugOut(DO_ERROR, L"Given format not supported, Skipping");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Create test vertex buffer
    //

    if(FAILED(hr = CreateAndPrepareVertexBuffer(pDevice, pTestCaseArgs->hWnd, dwTableIndex, &pVertexBuffer, &VertexBufferStride)))
    {
        DebugOut(DO_ERROR, L"Could not create and prepare vertex buffer. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }


    /////////////////////////////////////////////////////
    //
    // Prepare the pipeline
    //
    /////////////////////////////////////////////////////
    

    //
    // Set the default texture stage states
    //

    // SetDefaultTextureStates is from FVFTestCases\utils.*
    if (FAILED(hr = SetDefaultTextureStates(pDevice)))
    {
        DebugOut(DO_ERROR, L"Could not set the default texture states. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Clear the Texture Transform Flag
    //

    if (FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_TEXTURETRANSFORMFLAGS, D3DMTTFF_DISABLE)))
    {
        DebugOut(DO_ERROR, L"Could not set default TTFF state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    
    //
    // Set the proper texture stage states for the test
    //

    if (FAILED(hr = SetupTextureStages(pDevice, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not setup the texture stages. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Set the pipeline's Stream Source
    //

    if (FAILED(hr = pDevice->SetStreamSource(0, pVertexBuffer, VertexBufferStride)))
    {
        DebugOut(DO_ERROR, L"Could not set the stream source to the test vertex buffer. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Set the Render state SHADEMODE
    //

    if (FAILED(hr = SetupRenderStates(pDevice, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not set the shade render state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        DebugOut(DO_ERROR, L"Current AlphaTest compare function not supported, Skipping");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Verify that the current pipeline state is supported
    //

    if (FAILED(hr = pDevice->ValidateDevice(&dwNumPasses)))
    {
        DebugOut(DO_ERROR, L"ValidateDevice failed, current test case unsupported. (hr = 0x%08x) Skipping.", hr);
        Result = TPR_SKIP;
        goto cleanup;
    }

    /////////////////////////////////////////////////////
    //
    // Use the pipeline!
    //
    /////////////////////////////////////////////////////

    //
    // Clear the device
    //

    if (FAILED(hr = pDevice->Clear(0, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255, 0, 0), 1.0, 0)))
    {
        DebugOut(DO_ERROR, L"Could not clear target and zbuffer. (hr = 0x%08x) Aborting", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Begin the scene
    //
    
	if (FAILED(hr = pDevice->BeginScene()))
	{
		DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

    //
    // DrawPrimitive
    //
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMQA_PRIMTYPE, 0, GetPrimitiveCount(dwTableIndex))))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // End the scene
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
		DebugOut(DO_ERROR, _T("DumpFlushedFrame failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


cleanup:
    if (pVertexBuffer)
    {
        pDevice->SetStreamSource(0, NULL, 0);
        pVertexBuffer->Release();
    }
    if (pTexture)
    {
        pDevice->SetTexture(0, NULL);
        pTexture->Release();
    }
	return Result;
}
