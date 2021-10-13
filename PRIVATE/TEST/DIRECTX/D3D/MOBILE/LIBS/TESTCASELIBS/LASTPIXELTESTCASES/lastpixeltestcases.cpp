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
#include <tchar.h>
#include <tux.h>
#include "ImageManagement.h"
#include "TestCases.h"
#include "DebugOutput.h"
#include "LastPixelPermutations.h"

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
		OutputDebugString(_T("Invalid argument(s)."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;

	//
	// Valid test case index
	//
	if (dwTableIndex >= D3DMQA_LASTPIXELTEST_COUNT)
	{
		OutputDebugString(_T("Invalid test case ordinal."));
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
			OutputDebugString(_T("GetClientRect failed.\n"));
			Result = TPR_ABORT;
			goto cleanup;
		}
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Check ZFunc capabilities
	//
	if (!bLess && !(Caps.ZCmpCaps & D3DMPCMPCAPS_LESSEQUAL))
	{
	    OutputDebugString(_T("Device does not support D3DMCMP_LESSEQUAL for D3DMRS_ZFUNC"));
	    Result = TPR_SKIP;
	    goto cleanup;
	}
	if (bLess && !(Caps.ZCmpCaps & D3DMPCMPCAPS_LESS))
	{
	    OutputDebugString(_T("Device does not support D3DMCMP_LESS for D3DMRS_ZFUNC"));
	    Result = TPR_SKIP;
	    goto cleanup;
	}

	//
	// Include last pixel in rasterization
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_LASTPIXEL, LastPixelTests[dwTableIndex].dwLastPixel)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_LASTPIXEL, TRUE) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Turn off lighting
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_LIGHTING, FALSE)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_LIGHTING, FALSE) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Set up shading
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}
    if (bLess)
    {
    	//
    	// Make sure to use supported Z compare setting
    	//
    	if (FAILED(pDevice->SetRenderState(D3DMRS_ZFUNC, D3DMCMP_LESS)))
    	{
    	    OutputDebugString(_T("SetRenderState(D3DMRS_ZFUNC, D3DMCMP_LESS) failed."));
    	    Result = TPR_ABORT;
    	    goto cleanup;
    	}
    }


	//
	// Prepare test case permutation
	//
	if (FAILED(pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		OutputDebugString(_T("Clear failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(pDevice->BeginScene()))
	{
		OutputDebugString(_T("BeginScene failed."));
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
		if (FAILED(DrawLineList(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                        ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawLinesList failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;
	case D3DMPT_LINESTRIP:

		//
		// Line primitives
		//
		if (FAILED(DrawLineStrip(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                         ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawLinesStrip failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;

	default:
		OutputDebugString(_T("Invalid D3DMPRIMITIVETYPE."));
		Result = TPR_FAIL;
		goto cleanup;
		break;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(_T("EndScene failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(_T("Present failed."));
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

	return Result;
}
