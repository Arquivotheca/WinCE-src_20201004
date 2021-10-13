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
#include "ColorFillCases.h"
#include "ColorFillTestFunctions.h"
#include "ImageManagement.h"
#include "utils.h"
#include <tux.h>
#include <SurfaceTools.h>
#include "DebugOutput.h"

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
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
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
		DebugOut(DO_ERROR, L"Invalid test index. Aborting.");
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
    if (FAILED(hr = pfnAcquireFillSurface(pDevice, &pParentSource, &pSurface, Width, Height, dwTableIndex)))
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
    // Determine the Rect that we'll use to capture the frame
    //
    if (FAILED(hr = SurfaceIntersection(pSurfaceBackBuffer, pSurface, NULL, &RectFrame)))
    {
        DebugOut(DO_ERROR, L"Aborting: Could not determine intersection of device and surface extents. (hr = 0x%08x) Aborting.", hr);
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
            DebugOut(DO_ERROR, L"ColorFill failed. (hr = 0x%08x) Failing.", hr);
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
	                                 &RectFrame)))  // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
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
