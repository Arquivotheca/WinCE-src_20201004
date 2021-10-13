//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "CopyRectsCases.h"
#include "CopyRectsTestFunctions.h"
#include "ImageManagement.h"
#include "utils.h"
#include <tux.h>
#include <SurfaceTools.h>

//
// CopyRectsTest
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
TESTPROCAPI CopyRectsTest(LPTESTCASEARGS pTestCaseArgs)

{
	//
	// Function pointers; all light tests conform to same
	// function signatures
	//
	ACQUIRE_SOURCE_SURFACE pfnAcquireSourceSurface;
	ACQUIRE_DEST_SURFACE   pfnAcquireDestSurface;
	SETUP_SOURCE_SURFACE   pfnSetupSourceSurface;
	GET_COPY_RECTS         pfnGetCopyRects;

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
    RECT RectSource[20];

    //
    // The rectangle to use when capturing the frame.
    //
    RECT RectFrame;

    //
    // The point used to convert the Frame Rect from window space to fullscreen space.
    //
    POINT PtFrame;

    //
    // The points to use when stretching.
    //
    POINT PointDest[20];

    //
    // The sizes of the buffers.
    //
    UINT CountRect  = sizeof(RectSource) / sizeof(*RectSource);
    UINT CountPoint = sizeof(PointDest)  / sizeof(*PointDest);

	//
	// API Result code
	//
	HRESULT hr;

	//
	// Test Result
	//
	INT Result = TPR_PASS;

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
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_COPYRECTSTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_COPYRECTSTEST_MAX))
	{
		//
		// Lighting test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_COPYRECTSTEST_BASE;

		pfnAcquireSourceSurface = AcquireCRSourceSurface;
		pfnAcquireDestSurface   = AcquireCRDestSurface;
		pfnSetupSourceSurface   = SetupCRSourceSurface;
		pfnGetCopyRects         = GetCopyRects;
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
        OutputDebugString(L"Aborting: Could not determine intersection of device and surface extents");
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
    // Iterate through the RECTs to use when calling CopyRects
    //
    if (S_OK == (hr = pfnGetCopyRects(pSurfaceSource, pSurfaceDest, dwTableIndex, RectSource, PointDest, &CountRect, &CountPoint)))
    {
       
        //
        // Call CopyRects
        //
        hr = pDevice->CopyRects(
            pSurfaceSource,
            CountRect ? RectSource : NULL,
            CountRect > CountPoint ? CountRect : CountPoint,
            pSurfaceDest,
            CountPoint ? PointDest : NULL);
        if (FAILED(hr))
        {
            OutputDebugString(L"CopyRects failed");
            Result = TPR_FAIL;
        }
    }
    if (S_FALSE == hr)
    {
        OutputDebugString(L"Could not find appropriate copy rects, skipping test.\n");
        Result = TPR_SKIP;
    }

    //
    // Copy the results of the CopyRects calls to the backbuffer, in anticipation
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
	return Result;
}
