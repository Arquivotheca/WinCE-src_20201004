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
#include "DXTnCases.h"
#include "ImageManagement.h"
#include <tux.h>
#include "DebugOutput.h"

//
// DXTnTest
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
INT DXTnTest(LPTESTCASEARGS pTestCaseArgs)

{
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
    // The number of primitives to render.
    //
	UINT VertexPrimCount = 0;

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
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_DXTNTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_DXTNTEST_MAX))
	{
		//
		// Lighting test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_DXTNTEST_BASE;
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
	hr = DXTnIsTestSupported(pDevice, &Caps, dwTableIndex, false);
	if (S_OK != hr)
	{
		DebugOut(DO_ERROR, L"Test not supported. Skipping.");
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
    hr = DXTnCreateAndPrepareTexture(
        pDevice, 
        dwTableIndex, 
        &pTexture,
        NULL); // ImageSurface
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not create and prepare texture. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Create test vertex buffer
    //
    hr = DXTnCreateAndPrepareVertexBuffer(
        pDevice, 
        pTestCaseArgs->hWnd, 
        dwTableIndex, 
        &pVertexBuffer, 
        &VertexBufferStride,
        &VertexPrimCount);
    if(FAILED(hr))
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
    if (FAILED(hr = DXTnSetDefaultTextureStates(pDevice)))
    {
        DebugOut(DO_ERROR, L"Could not set the default texture states. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Clear the Texture Transform Flag
    //
    hr = pDevice->SetTextureStageState(
        0, 
        D3DMTSS_TEXTURETRANSFORMFLAGS, 
        D3DMTTFF_DISABLE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not set default TTFF state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    
    //
    // Set the coordinate transformations
    //
    hr = DXTnSetTransforms(
        pDevice, 
        dwTableIndex, 
        true); // Should we set the world transform?
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not set the coordinate transformations. (hr = 0x%08x) Aborting.", hr);
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
    // Set the Render states for the current test case
    //

    if (FAILED(hr = DXTnSetupRenderState(pDevice, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not set the shade render state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
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
        DebugOut(DO_ERROR, L"Could not clear target and zbuffer. (hr = 0x%08x) Aborting.", hr);
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
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMQA_PRIMTYPE, 0, VertexPrimCount)))
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
	if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                                 0,             // UINT uiFrameNumber,
	                                 NULL)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
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

INT DXTnMipMapTest(LPTESTCASEARGS pTestCaseArgs)
    {
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
    // Image surface to hold desired texture
    //
    LPDIRECT3DMOBILESURFACE pImageSurface = NULL;

    //
    // The vertex buffer which contains the test primitives.
    //
    LPDIRECT3DMOBILEVERTEXBUFFER pVertexBuffer = NULL;
    UINT VertexBufferStride = 0;

    //
    // The number of primitives to render.
    //
    UINT VertexPrimCount = 0;

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
    // Choose test case subset
    //
    if ((pTestCaseArgs->dwTestIndex >= D3DMQA_DXTNMIPMAPTEST_BASE) &&
        (pTestCaseArgs->dwTestIndex <= D3DMQA_DXTNMIPMAPTEST_MAX))
    {
        //
        // Lighting test cases
        //

        dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_DXTNMIPMAPTEST_BASE;
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
    hr = DXTnIsTestSupported(pDevice, &Caps, dwTableIndex, true);
    if (S_OK != hr)
    {
        DebugOut(DO_ERROR, L"Test not supported. Skipping.");
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
    hr = DXTnCreateAndPrepareTexture(
        pDevice, 
        dwTableIndex, 
        NULL, // Texture surface pointer
        &pImageSurface);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not create and prepare texture. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Create test vertex buffer
    //
    hr = DXTnCreateAndPrepareVertexBuffer(
        pDevice, 
        pTestCaseArgs->hWnd, 
        dwTableIndex, 
        &pVertexBuffer, 
        &VertexBufferStride,
        &VertexPrimCount);
    if(FAILED(hr))
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
    if (FAILED(hr = DXTnSetDefaultTextureStates(pDevice)))
    {
        DebugOut(DO_ERROR, L"Could not set the default texture states. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    //
    // Clear the Texture Transform Flag
    //
    hr = pDevice->SetTextureStageState(
        0, 
        D3DMTSS_TEXTURETRANSFORMFLAGS, 
        D3DMTTFF_DISABLE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not set default TTFF state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    
    //
    // Set the coordinate transformations
    //
    hr = DXTnSetTransforms(
        pDevice, 
        dwTableIndex, 
        false); // Should we set the world transform?
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not set the coordinate transformations. (hr = 0x%08x) Aborting.", hr);
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
    // Set the Render states for the current test case
    //

    if (FAILED(hr = DXTnSetupRenderState(pDevice, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not set the shade render state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
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
        DebugOut(DO_ERROR, L"Could not clear target and zbuffer. (hr = 0x%08x) Aborting.", hr);
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

    hr = DXTnMipMapDrawPrimitives(
        pDevice,
        pTestCaseArgs->hWnd,
        dwTableIndex,
        pVertexBuffer,
        pImageSurface);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not draw primitives (hr = 0x%08x). Aborting.", hr);
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
    if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
                                     0,             // UINT uiFrameNumber,
                                     NULL)))        // RECT *pRect = NULL
    {
        DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
        Result = TPR_FAIL;
        goto cleanup;
    }

cleanup:
    if (pImageSurface)
    {
        pImageSurface->Release();
    }
    if (pVertexBuffer)
    {
        pDevice->SetStreamSource(0, NULL, 0);
        pVertexBuffer->Release();
    }
    pDevice->SetTexture(0, NULL);
    return Result;
}

