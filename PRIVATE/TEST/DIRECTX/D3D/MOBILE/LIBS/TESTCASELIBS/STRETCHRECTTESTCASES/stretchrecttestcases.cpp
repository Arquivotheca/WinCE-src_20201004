//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <d3dm.h>
#include <SurfaceTools.h>
#include "StretchRectCases.h"
#include "StretchRectTestFunctions.h"
#include "ImageManagement.h"
#include "utils.h"
#include <tux.h>

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
		OutputDebugString(_T("Invalid argument(s)."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		OutputDebugString(L"GetDeviceCaps failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Get Direct3D Mobile object
	//
	if (FAILED(pDevice->GetDirect3D(&pD3D))) // IDirect3DMobile** ppD3DM
	{
		OutputDebugString(L"GetDirect3D failed.\n");
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
		OutputDebugString(L"Invalid test index.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}
	
    //
    // Determine backbuffer size (we will determine all surface sizes based on this
    //
    if (FAILED(pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pSurfaceBackBuffer)))
    {
        OutputDebugString(L"Could not get backbuffer to determine proper size.\n");
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (FAILED(pSurfaceBackBuffer->GetDesc(&BackBufferDesc)))
    {
        OutputDebugString(L"Could not get backbuffer description.\n");
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
        OutputDebugString(L"Could not acquire source surface.\n");
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        OutputDebugString(L"Source surface type not supported, skipping.\n");
        Result = TPR_SKIP;
        goto cleanup;
    }

    //
    // Obtain a destination surface to StretchBlt to
    //
    if (FAILED(hr = pfnAcquireDestSurface(pDevice, &pParentDest, &pSurfaceDest, Width, Height, dwTableIndex)))
    {
        OutputDebugString(L"Could not acquire destination surface.\n");
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_FALSE == hr)
    {
        OutputDebugString(L"Destination surface type not supported, skipping.\n");
        Result = TPR_SKIP;
        goto cleanup;
    }

    pfnSetupSourceSurface(pSurfaceSource, dwTableIndex);

    if (FAILED(hr = SurfaceIntersection(pSurfaceBackBuffer, pSurfaceDest, pSurfaceSource, &RectFrame)))
    {
        OutputDebugString(L"Abort: Could not determine intersection of device and surface extents.\n");
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
        OutputDebugString(L"Could not determine filter type.\n");
        Result = TPR_ABORT;
        goto cleanup;
    }
    if (S_OK != hr)
    {
        OutputDebugString(L"Filter type not supported, skipping.\n");
        Result = TPR_SKIP;
        goto cleanup;
    }

	//
	// Get description of source surface (for the purposes of format inspection)
	//
	if (FAILED(pSurfaceSource->GetDesc(&SourceDesc))) // D3DMSURFACE_DESC *pDesc
	{
        OutputDebugString(L"LPDIRECT3DMOBILESURFACE->GetDesc failed.\n");
        Result = TPR_ABORT;
        goto cleanup;
	}

	//
	// Get description of destination surface (for the purposes of format inspection)
	//
	if (FAILED(pSurfaceDest->GetDesc(&DestDesc))) // D3DMSURFACE_DESC *pDesc
	{
        OutputDebugString(L"LPDIRECT3DMOBILESURFACE->GetDesc failed.\n");
        Result = TPR_ABORT;
        goto cleanup;
	}

	//
	// Does the device support this format conversion?
	//
	if (FAILED(pD3D->CheckDeviceFormatConversion(Caps.AdapterOrdinal, // UINT Adapter,
	                                             Caps.DeviceType,     // D3DMDEVTYPE DeviceType,
	                                             SourceDesc.Format,   // D3DMFORMAT SourceFormat,
	                                             DestDesc.Format)))   // D3DMFORMAT DestFormat
	{
        OutputDebugString(L"LPDIRECT3DMOBILESURFACE->CheckDeviceFormatConversion failed.\n");
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
            OutputDebugString(L"StretchRect failed");
            Result = TPR_FAIL;
        }
    }

    //
    // Copy the results of the StretchRect calls to the backbuffer, in anticipation
    // of the Present call.
    //
    if (FAILED(hr = CopySurfaceToBackBuffer(pDevice, pSurfaceDest, FALSE)))
    {
        OutputDebugString(L"Results could not be copied to backbuffer.\n");
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
	                            &RectFrame)))        // RECT *pRect = NULL
	{
		OutputDebugString(_T("DumpFlushedFrame failed."));
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
