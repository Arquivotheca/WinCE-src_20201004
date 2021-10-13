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
#include "AddtlSwapChainsCases.h"
#include "ImageManagement.h"
#include <tux.h>
#include "DebugOutput.h"

//
// DepthBiasTest
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
INT AddtlSwapChainsTest(LPTESTCASEARGS pTestCaseArgs)

{
    using namespace AddtlSwapChainsNamespace;
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
	// Target swap chains
	//
	LPDIRECT3DMOBILESWAPCHAIN * rgpSwapChains = NULL;

	//
	// Swap Chain Windows
	//
	HWND *pSwapWindows = NULL;

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
	// The number of passes the device will need (not used)
	//
	DWORD dwNumPasses;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// The client rect of the currently active window
	//
	RECT Rect;
	RECT * pRect = NULL;

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
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_ADDTLSWAPCHAINSTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_ADDTLSWAPCHAINSTEST_MAX))
	{
		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_ADDTLSWAPCHAINSTEST_BASE;
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
    // Prepare the main window by stripping it of its borders and titles
    //
    hr = PrepareMainHWnd(
        pDevice,
        pTestCaseArgs->hWnd);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not prepare main window for test. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }

    
    //
    // Create the swap chains that will be used in the test
    //
    hr = CreateSwapChains(
        pDevice,
        pTestCaseArgs->hWnd,
        dwTableIndex,
        &rgpSwapChains,
        &pSwapWindows);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not create swap chains. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        DebugOut(DO_ERROR, L"Cound not create swap chains due to acceptable limitation. (hr = 0x%08x) Skipping.", hr);
        Result = TPR_SKIP;
        goto cleanup;
    }
	
    //
    // Create test vertex buffer
    //
    hr = CreateAndPrepareVertexBuffers(
        pDevice, 
        pTestCaseArgs->hWnd, 
        dwTableIndex, 
        &pVertexBuffer, 
        &VertexBufferStride);
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

    //
    // Set the proper texture stage states for the test
    //
    hr = SetupTextureStages(pDevice, dwTableIndex);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not setup the texture stages. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }


    //
    // Set the Render state SHADEMODE
    //
    hr = SetupRenderStates(pDevice, dwTableIndex);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not set the shade render state. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        DebugOut(DO_ERROR, L"Current DepthBias compare function not supported. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Verify that the current pipeline state is supported
    //
    hr = pDevice->ValidateDevice(&dwNumPasses);
    if (FAILED(hr))
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
    // Verify that the current pipeline state is supported
    //
    
    if (FAILED(hr = pDevice->ValidateDevice(&dwNumPasses)))
    {
        DebugOut(DO_ERROR, L"ValidateDevice failed, current test case unsupported. (hr = 0x%08x) Skipping.", hr);
        Result = TPR_SKIP;
        goto cleanup;
    }

    for (UINT iChain = 0; iChain < AddtlSwapChainsCases[dwTableIndex].AddtlSwapChainsCount + 1; ++iChain)
    {
        int iCurrentChain = iChain + 1;
        if (AddtlSwapChainsCases[dwTableIndex].DevPresent + 1 == iCurrentChain)
        {
            iCurrentChain = 0;
        }
        else if (AddtlSwapChainsCases[dwTableIndex].DevPresent + 1 < iCurrentChain)
        {
            --iCurrentChain;
        }
        hr = PrepareDeviceWithSwapChain(
            pDevice,
            rgpSwapChains,
            pVertexBuffer,
            iCurrentChain);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not prepare device with current swap chain. (hr = 0x%08x) Aborting.", hr);
            Result = TPR_ABORT;
            goto cleanup;
        }
            
        //
        // Clear the device
        //
        hr = pDevice->Clear(
            0, 
            NULL, 
            D3DMCLEAR_TARGET, 
            D3DMCOLOR_XRGB(255, 0, 0), 
            1.0, 
            0);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not clear target and zbuffer. (hr = 0x%08x) Aborting.", hr);
            Result = TPR_ABORT;
            goto cleanup;
        }

        //
        // Begin the scene
        //
        hr = pDevice->BeginScene();
    	if (FAILED(hr))
    	{
    		DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing.", hr);
    		Result = TPR_FAIL;
    		goto cleanup;
    	}

        //
        // DrawPrimitive
        //
    	hr = pDevice->DrawPrimitive(
    	    D3DMQA_PRIMTYPE, 
    	    0, 
    	    VERTCOUNT - 2);
    	if (FAILED(hr))
    	{
    		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Aborting.", hr);
    		Result = TPR_ABORT;
    		goto cleanup;
    	}

        //
        // End the scene
        //
        hr = pDevice->EndScene();
    	if (FAILED(hr))
    	{
    		DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Failing.", hr);
    		Result = TPR_FAIL;
    		goto cleanup;
    	}

        
        //
    	// Presents the contents of the next buffer in the sequence of
    	// back buffers owned by the device.
    	//
    	if (0 == iCurrentChain)
    	{
    	    hr = pDevice->Present(NULL, NULL, NULL, NULL);
        	if (FAILED(hr))
        	{
        		DebugOut(DO_ERROR, L"Present on main device failed. (hr = 0x%08x) Failing.", hr);
        		Result = TPR_FAIL;
        		goto cleanup;
        	}
    	}
    	else
    	{
    	    hr = rgpSwapChains[iCurrentChain - 1]->Present(NULL, NULL, pSwapWindows[iCurrentChain - 1], NULL);
        	if (FAILED(hr))
        	{
        		DebugOut(DO_ERROR, L"Present on Additional Swap Chain %d failed. (hr = 0x%08x) Failing.", iCurrentChain - 1, hr);
        		Result = TPR_FAIL;
        		goto cleanup;
        	}
    	    
    	}
    }

    if (AddtlSwapChainsCases[dwTableIndex].TargetSwapChain != 0)
    {
        int Target = AddtlSwapChainsCases[dwTableIndex].TargetSwapChain - 1;
        if (GetClientRect(pSwapWindows[Target], &Rect))
        {
            RECT WndRect;
            GetWindowRect(pSwapWindows[Target], &WndRect);
            OffsetRect(&Rect, WndRect.left, WndRect.top);
            pRect = &Rect;
        }
        else
        {
            DebugOut(DO_ERROR, L"Could not get client rect for swap chain %d window. (GLE = %d) Aborting.", Target, GetLastError());
            Result = TPR_ABORT;
            goto cleanup;
        }
    }

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                                 0,             // UINT uiFrameNumber,
	                                 pRect)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, _T("DumpFlushedFrame failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:
    
    hr = DestroySwapChains(
        dwTableIndex,
        rgpSwapChains,
        pSwapWindows);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not destroy swap chains. (hr = 0x%08x) Failing.", hr);
        Result = TPR_FAIL;
    }
    
    if (pVertexBuffer)
    {
        pDevice->SetStreamSource(0, NULL, 0);
        pVertexBuffer->Release();
    }
	return Result;
}
