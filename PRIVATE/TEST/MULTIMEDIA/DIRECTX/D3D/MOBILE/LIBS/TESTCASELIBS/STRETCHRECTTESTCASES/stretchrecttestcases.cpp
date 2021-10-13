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
#include <d3dm.h>
#include <SurfaceTools.h>
#include "StretchRectCases.h"
#include "StretchRectTestFunctions.h"
#include "ImageManagement.h"
#include "utils.h"
#include <tux.h>
#include "DebugOutput.h"

//
// StretchRectTest
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
TESTPROCAPI StretchRectTest(LPTESTCASEARGS pTestCaseArgs)

{
	//
	// Function pointers; all light tests conform to same
	// function signatures
	//
	ACQUIRE_SOURCE_SURFACE pfnAcquireSourceSurface;
	ACQUIRE_DEST_SURFACE   pfnAcquireDestSurface;
	SETUP_SOURCE_SURFACE   pfnSetupSourceSurface;
	GET_STRETCH_RECTS      pfnGetStretchRects;
	GET_FILTER_TYPE        pfnGetFilterType;

	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// The index into the test case table (derived from the test index)
	//
	DWORD dwTableIndex = 0;

    //
    // We need the Direct3DMobile object to determine what format to use.
    //
    LPDIRECT3DMOBILE pD3D = NULL;

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// Interfaces for test operation
	//
	LPDIRECT3DMOBILESURFACE pSurfaceSource = NULL, pSurfaceDest = NULL;

	//
	// Use backbuffer to determine proper size of surfaces
	//
	LPDIRECT3DMOBILESURFACE pSurfaceBackBuffer = NULL;
    D3DMSURFACE_DESC BackBufferDesc;
	UINT Width = 0, Height = 0;
	
    //
    // Interfaces for the parents of the surfaces. If the surface is a texture,
    // the texture needs to be released along with the surface.
    //
    LPUNKNOWN pParentSource = NULL, pParentDest = NULL;

    //
    // The rectangles to use when stretching.
    //
    RECT RectSource, RectDest;

    //
    // The rectangle to use for capturing the frame.
    //
    RECT RectFrame;

    //
    // The point used to convert the Frame Rect from window space to fullscreen space.
    //
    POINT PtFrame;

    //
    // The filter type to use when calling StretchRect
    //
    D3DMTEXTUREFILTERTYPE d3dtft;

	// 
	// Device capabilities
	//
	D3DMCAPS Caps;
    
	//
	// API Result code
	//
	HRESULT hr;

	//
	// Test Result
	//
	INT Result = TPR_PASS;

	//
	// Surface descriptions for source and destination of StretchRect call
	//
	D3DMSURFACE_DESC SourceDesc;
	D3DMSURFACE_DESC DestDesc;

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
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Get Direct3D Mobile object
	//
	if (FAILED(hr = pDevice->GetDirect3D(&pD3D))) // IDirect3DMobile** ppD3DM
	{
		DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_STRETCHRECTTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_STRETCHRECTTEST_MAX))
	{
		//
		// Lighting test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_STRETCHRECTTEST_BASE;

		pfnAcquireSourceSurface = AcquireSourceSurface;
		pfnAcquireDestSurface   = AcquireDestSurface;
		pfnSetupSourceSurface   = SetupSourceSurface;
		pfnGetStretchRects      = GetStretchRects;
		pfnGetFilterType        = GetFilterType;
	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}
	
    //
    // Determine backbuffer size (we will determine all surface sizes based on this
    //
    if (FAILED(hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pSurfaceBackBuffer)))
    {
        DebugOut(DO_ERROR, L"Could not get backbuffer to determine proper size. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (FAILED(hr = pSurfaceBackBuffer->GetDesc(&BackBufferDesc)))
    {
        DebugOut(DO_ERROR, L"Could not get backbuffer description. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    Width = BackBufferDesc.Width;
    Height = BackBufferDesc.Height;

    //
    // Obtain a source surface to StretchBlt from
    //
    if (FAILED(hr = pfnAcquireSourceSurface(pDevice, &pParentSource, &pSurfaceSource, Width, Height, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not acquire source surface. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        DebugOut(DO_ERROR, L"Source surface type not supported. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Obtain a destination surface to StretchBlt to
    //
    if (FAILED(hr = pfnAcquireDestSurface(pDevice, &pParentDest, &pSurfaceDest, Width, Height, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not acquire destination surface. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        DebugOut(DO_ERROR, L"Destination surface type not supported. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

    pfnSetupSourceSurface(pSurfaceSource, dwTableIndex);

    if (FAILED(hr = SurfaceIntersection(pSurfaceBackBuffer, pSurfaceDest, pSurfaceSource, &RectFrame)))
    {
        DebugOut(DO_ERROR, L"Could not determine intersection of device and surface extents. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    pSurfaceBackBuffer->Release();
    pSurfaceBackBuffer = NULL;

    if (pTestCaseArgs->pParms->Windowed)
    {
        memset(&PtFrame, 0x00, sizeof(PtFrame));
        ClientToScreen(pTestCaseArgs->hWnd, &PtFrame);
        RectFrame.left   += PtFrame.x;
        RectFrame.right  += PtFrame.x;
        RectFrame.top    += PtFrame.y;
        RectFrame.bottom += PtFrame.y;
    }

    //
    // Get the correct filter type
    //
    if (FAILED(hr = pfnGetFilterType(pDevice, &d3dtft, dwTableIndex)))
    {
        DebugOut(DO_ERROR, L"Could not determine filter type. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_OK != hr)
    {
        DebugOut(DO_ERROR, L"Filter type not supported. Skipping.");
        Result = TPR_SKIP;
        goto cleanup;
    }

	//
	// Get description of source surface (for the purposes of format inspection)
	//
	if (FAILED(hr = pSurfaceSource->GetDesc(&SourceDesc))) // D3DMSURFACE_DESC *pDesc
	{
        DebugOut(DO_ERROR, L"LPDIRECT3DMOBILESURFACE->GetDesc failed. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
	}

	//
	// Get description of destination surface (for the purposes of format inspection)
	//
	if (FAILED(hr = pSurfaceDest->GetDesc(&DestDesc))) // D3DMSURFACE_DESC *pDesc
	{
        DebugOut(DO_ERROR, L"LPDIRECT3DMOBILESURFACE->GetDesc failed. (hr = 0x%08x) Aborting.", hr);
        Result = TPR_ABORT;
        goto cleanup;
	}

	//
	// Does the device support this format conversion?
	//
	if (FAILED(hr = pD3D->CheckDeviceFormatConversion(Caps.AdapterOrdinal, // UINT Adapter,
	                                                  Caps.DeviceType,     // D3DMDEVTYPE DeviceType,
	                                                  SourceDesc.Format,   // D3DMFORMAT SourceFormat,
	                                                  DestDesc.Format)))   // D3DMFORMAT DestFormat
	{
        DebugOut(DO_ERROR, L"LPDIRECT3DMOBILESURFACE->CheckDeviceFormatConversion failed. (hr = 0x%08x) Skipping.", hr);
        Result = TPR_SKIP;
        goto cleanup;
	}

    //
    // Iterate through the RECTs to use when calling StretchRect
    //
    for (DWORD Iter = 0; 
         S_OK == (hr = pfnGetStretchRects(pSurfaceSource, pSurfaceDest, Iter, dwTableIndex, &RectSource, &RectDest)); 
         Iter++)
    {
        //
        // Call StretchRect
        //
        hr = pDevice->StretchRect(
            pSurfaceSource,
            &RectSource,
            pSurfaceDest,
            &RectDest,
            d3dtft);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"StretchRect failed. (hr = 0x%08x) Failing.", hr);
            Result = TPR_FAIL;
        }
    }

    //
    // Copy the results of the StretchRect calls to the backbuffer, in anticipation
    // of the Present call.
    //
    if (FAILED(hr = CopySurfaceToBackBuffer(pDevice, pSurfaceDest, FALSE)))
    {
        DebugOut(DO_ERROR, L"Results could not be copied to backbuffer. (hr = 0x%08x) Failing.", hr);
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
	                                 &RectFrame)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}


cleanup:

    if (pSurfaceSource)
        pSurfaceSource->Release();
    if (pSurfaceDest)
        pSurfaceDest->Release();
    if (pParentSource)
        pParentSource->Release();
    if (pParentDest)
        pParentDest->Release();
    if (pSurfaceBackBuffer)
        pSurfaceBackBuffer->Release();
	if (pD3D)
		pD3D->Release();
	return Result;
}
