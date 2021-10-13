//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "ColorFillCases.h"
#include "ColorFillTestFunctions.h"
#include "ImageManagement.h"
#include "utils.h"
#include <tux.h>
#include <SurfaceTools.h>

//
// ColorFillTest
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
TESTPROCAPI ColorFillTest(LPTESTCASEARGS pTestCaseArgs)

{
	//
	// Function pointers; all light tests conform to same
	// function signatures
	//
	ACQUIRE_FILL_SURFACE pfnAcquireFillSurface;
	GET_COLORFILL_RECTS  pfnGetColorFillRects;

	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = pTestCaseArgs->pDevice;

	//
	// The index into the test case table (derived from the test index)
	//
	DWORD dwTableIndex = 0;

	//
	// The number of times GetColorFillRects has been called.
	//
	UINT Iterations = 0;

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
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

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
    // The rect to use for the colorfill.
    //
    RECT Rect;
    LPRECT pRect;

    //
    // The rect to use for the frame capture.
    //
    RECT RectFrame;

    //
    // The point used to convert the Frame Rect from window space to fullscreen space.
    //
    POINT PtFrame;

    //
    // The color to use with colorfill
    //
    D3DMCOLOR Color;

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

	//
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_COLORFILLTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_COLORFILLTEST_MAX))
	{
		//
		// Lighting test cases
		//

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_COLORFILLTEST_BASE;

		pfnAcquireFillSurface = AcquireFillSurface;
		pfnGetColorFillRects  = GetColorFillRects;
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
    if (FAILED(hr = pfnAcquireFillSurface(pDevice, &pParentSource, &pSurface, Width, Height, dwTableIndex)))
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
    // Determine the Rect that we'll use to capture the frame
    //
    if (FAILED(hr = SurfaceIntersection(pSurfaceBackBuffer, pSurface, NULL, &RectFrame)))
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
    // Iterate through the RECTs to use when calling ColorFill
    //
    pRect = &Rect;
    while (S_OK == (hr = pfnGetColorFillRects(pSurface, dwTableIndex, Iterations++, &pRect, &Color)))
    {
       
        //
        // Call ColorFill
        //
        hr = pDevice->ColorFill(
            pSurface,
            pRect,
            Color);
        if (FAILED(hr))
        {
            OutputDebugString(L"ColorFill failed");
            Result = TPR_FAIL;
        }
        pRect = &Rect;
    }

    //
    // Copy the results of the ColorFill calls to the backbuffer, in anticipation
    // of the Present call.
    //
    if (FAILED(hr = CopySurfaceToBackBuffer(pDevice, pSurface, FALSE)))
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
	                            &RectFrame)))  // RECT *pRect = NULL
	{
		OutputDebugString(_T("DumpFlushedFrame failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

    if (pSurface)
        pSurface->Release();
    if (pParentSource)
        pParentSource->Release();
    if (pParentDest)
        pParentDest->Release();
    if (pSurfaceBackBuffer)
        pSurfaceBackBuffer->Release();
	return Result;
}
