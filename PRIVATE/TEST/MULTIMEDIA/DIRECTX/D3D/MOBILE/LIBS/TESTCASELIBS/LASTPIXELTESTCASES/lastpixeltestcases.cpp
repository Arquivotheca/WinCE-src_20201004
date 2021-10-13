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
#include <tchar.h>
#include <tux.h>
#include "ImageManagement.h"
#include "TestCases.h"
#include "DebugOutput.h"
#include "LastPixelPermutations.h"
#include "DebugOutput.h"

//
// Rendering functions for D3DMRS_LASTPIXEL test case geometry
//
HRESULT DrawLineList(LPDIRECT3DMOBILEDEVICE pDevice,
                     RECT RectTarget);

HRESULT DrawLineStrip(LPDIRECT3DMOBILEDEVICE pDevice,
                      RECT RectTarget);


//
// LastPixelTest
//
//   D3DMRS_LASTPIXEL test.
//
// Arguments:
//
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:  
//   
//   INT:  Indicates test result
//   
TESTPROCAPI LastPixelTest(LPTESTCASEARGS pTestCaseArgs)
{
	//
	// Result of attempt to draw test case
	//
	INT Result = TPR_PASS;

    HRESULT hr;

	//
	// Index into D3DMRS_LASTPIXEL test cases
	//
	DWORD dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_LASTPIXELTEST_BASE;

	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Window client extents
	//
	RECT ClientExtents = {0,0,0,0};

    //
    // Is this the test case for LESS or LESSEQUAL?
    //
	BOOL bLess = FALSE;

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
	// Valid test case index
	//
	if (dwTableIndex >= D3DMQA_LASTPIXELTEST_COUNT)
	{
		DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (dwTableIndex >= D3DMQA_LASTPIXELTEST_COUNT/2)
	{
	    bLess = TRUE;
	    dwTableIndex -= D3DMQA_LASTPIXELTEST_COUNT/2;
	}
 
	if (FALSE == pTestCaseArgs->pParms->Windowed)
	{
		ClientExtents.right  = GetSystemMetrics(SM_CXSCREEN);
		ClientExtents.bottom = GetSystemMetrics(SM_CYSCREEN);
	}
	else
	{
		//
		// Retrieve device capabilities
		//
		if (0 == GetClientRect( pTestCaseArgs->hWnd,  // HWND hWnd, 
							   &ClientExtents))       // LPRECT lpRect 
		{
			DebugOut(DO_ERROR, L"GetClientRect failed. (hr = 0x%08x) Aborting.", HRESULT_FROM_WIN32(GetLastError()));
			Result = TPR_ABORT;
			goto cleanup;
		}
	}

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
	// Check ZFunc capabilities
	//
	if (!bLess && !(Caps.ZCmpCaps & D3DMPCMPCAPS_LESSEQUAL))
	{
	    DebugOut(DO_ERROR, L"Device does not support D3DMCMP_LESSEQUAL for D3DMRS_ZFUNC. Skipping.");
	    Result = TPR_SKIP;
	    goto cleanup;
	}
	if (bLess && !(Caps.ZCmpCaps & D3DMPCMPCAPS_LESS))
	{
	    DebugOut(DO_ERROR, L"Device does not support D3DMCMP_LESS for D3DMRS_ZFUNC. Skipping.");
	    Result = TPR_SKIP;
	    goto cleanup;
	}

	//
	// Include last pixel in rasterization
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_LASTPIXEL, LastPixelTests[dwTableIndex].dwLastPixel)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_LASTPIXEL, TRUE) failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Turn off lighting
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_LIGHTING, FALSE) failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Set up shading
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT) failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    if (bLess)
    {
    	//
    	// Make sure to use supported Z compare setting
    	//
    	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ZFUNC, D3DMCMP_LESS)))
    	{
    	    DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ZFUNC, D3DMCMP_LESS) failed. (hr = 0x%08x) Aborting.", hr);
    	    Result = TPR_ABORT;
    	    goto cleanup;
    	}
    }

	//
	// Prepare test case permutation
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
	// Which primitive type?
	//
	switch(LastPixelTests[dwTableIndex].PrimType)
	{
	case D3DMPT_LINELIST:

		//
		// Line primitives
		//
		if (FAILED(hr = DrawLineList(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                             ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawLinesList failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;
	case D3DMPT_LINESTRIP:

		//
		// Line primitives
		//
		if (FAILED(hr = DrawLineStrip(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                              ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawLinesStrip failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;

	default:
		DebugOut(DO_ERROR, L"Invalid D3DMPRIMITIVETYPE. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
		break;
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
	if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                                 0,             // UINT uiFrameNumber,
	                                 NULL)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
			
cleanup:

	return Result;
}
