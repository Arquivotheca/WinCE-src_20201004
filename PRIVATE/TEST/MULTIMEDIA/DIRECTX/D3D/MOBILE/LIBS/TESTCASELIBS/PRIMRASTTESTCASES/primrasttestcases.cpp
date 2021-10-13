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
#include "util.h"
#include "ImageManagement.h"
#include "TestCases.h"
#include "DebugOutput.h"
#include "PrimRast.h"
#include "PrimRastPermutations.h"
#include "DebugOutput.h"


typedef struct _TESTCASEINSTANCE_INFO
{
	WCHAR *wszPath;  // Path for image dumps
	UINT uiInstance; // Test instance
	UINT uiTestID;   // Identifier for image dump
	HWND hWnd;       // HWND to capture
} TESTCASEINSTANCE_INFO;


//
// PrimRastTestEx
//
//   Renders a scene to test rasterization of a specific kind, specified by arguments.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   TESTCASEINSTANCE_INFO *pTestCaseInfo:  Specifications for image dumps
//   D3DMPRESENT_PARAMETERS* pPresentParms:  Specification of swap chain
//   LPPRIM_RAST_TESTS pPrimRastTest:  Specific test case permutation to exercise
//   
// Return Value:  
//   
//   INT:  Indicates test result
//   

INT PrimRastTestEx(LPDIRECT3DMOBILEDEVICE pDevice,
                   HWND hWnd,
                   TESTCASEINSTANCE_INFO *pTestCaseInfo,
                   D3DMPRESENT_PARAMETERS* pPresentParms,
                   LPPRIM_RAST_TESTS pPrimRastTest,
                   LPTESTCASEARGS pTestCaseArgs)
{
	//
	// Result of attempt to draw test case
	//
	INT Result = TPR_PASS;

    HRESULT hr;

	//
	// Vertex/index buffer interfaces for generated geometry
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;
	LPDIRECT3DMOBILEINDEXBUFFER pIB = NULL;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Window client extents
	//
	RECT ClientExtents = {0,0,0,0};
 

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}


	if (FALSE == pPresentParms->Windowed)
	{
		ClientExtents.right  = GetSystemMetrics(SM_CXSCREEN);
		ClientExtents.bottom = GetSystemMetrics(SM_CYSCREEN);
	}
	else
	{
		//
		// Retrieve device capabilities
		//
		if (0 == GetClientRect( hWnd,            // HWND hWnd, 
							   &ClientExtents)) // LPRECT lpRect 
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
	// Verify support shading arguments; skip if not available
	// 
	if (D3DMSHADE_GOURAUD == pPrimRastTest->ShadeMode)
	{
		if (!(D3DMPSHADECAPS_COLORGOURAUDRGB & Caps.ShadeCaps))
		{
			DebugOut(DO_ERROR, L"No support for D3DMSHADE_GOURAUD. Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Include last pixel in rasterization
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_LASTPIXEL, TRUE)))
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
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, pPrimRastTest->ShadeMode)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_SHADEMODE, ) failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Set up fill mode
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_FILLMODE, pPrimRastTest->FillMode)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_FILLMODE, ) failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Set up cull mode
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE) failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Setup COLORWRITEENABLE settings
	//
	if (FAILED(hr = SetupColorWrite(pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
	                                pPrimRastTest->ColorWriteMask))) // COLOR_WRITE_MASK WriteMask
	{
		DebugOut(DO_ERROR, L"Skipping test; D3DMPMISCCAPS_COLORWRITEENABLE not supported. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
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
	switch(pPrimRastTest->PrimType)
	{
	case D3DMPT_POINTLIST:

		//
		// Point primitives
		//
		if (FAILED(hr = DrawPoints(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
							       pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
						    	   pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							       pPrimRastTest->Format,            // D3DMFORMAT Format
							       ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawPoints failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

		break;
	case D3DMPT_LINELIST:

		//
		// Line primitives
		//
		if (FAILED(hr = DrawLineList(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                             pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                             pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							         pPrimRastTest->Format,            // D3DMFORMAT Format
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
		                              pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                              pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							          pPrimRastTest->Format,            // D3DMFORMAT Format
		                              ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawLinesStrip failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;
	case D3DMPT_TRIANGLELIST:
	
		//
		// Triangle primitives
		//
		if (FAILED(hr = DrawTriangleList(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                                 pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                                 pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							             pPrimRastTest->Format,            // D3DMFORMAT Format
		                                 ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawTriangleList failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;

	case D3DMPT_TRIANGLESTRIP:

		//
		// Triangle primitives
		//
		if (FAILED(hr = DrawTriangleStrip(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                                  pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                                  pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							              pPrimRastTest->Format,            // D3DMFORMAT Format
		                                  ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawTriangleStrip failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;

	case D3DMPT_TRIANGLEFAN:

		//
		// Triangle primitives
		//
		if (FAILED(hr = DrawTriangleFan(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                                pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                                pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							            pPrimRastTest->Format,            // D3DMFORMAT Format
		                                ClientExtents)))                  // RECT RectTarget)
		{
			DebugOut(DO_ERROR, L"DrawTriangleFan failed. (hr = 0x%08x) Failing.", hr);
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

	if (NULL != pVB)
		pVB->Release();

	if (NULL != pIB)
		pIB->Release();

	return Result;
}

//
// PrimRastTest
//
//   Wrapper for PrimRastTestEx; to expose easily as a set of tux tests.
//
// Arguments:
//
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:  
//   
//   INT:  Indicates test result
//   
TESTPROCAPI PrimRastTest(LPTESTCASEARGS pTestCaseArgs)
{
	INT Result;
	TESTCASEINSTANCE_INFO TestCaseInfo;
	DWORD dwTableIndex;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	TestCaseInfo.wszPath      = pTestCaseArgs->pwszImageComparitorDir; // Path for image dumps
	TestCaseInfo.uiInstance   = pTestCaseArgs->uiInstance;             // Test instance 
	TestCaseInfo.uiTestID     = pTestCaseArgs->dwTestIndex;            // Identifier for image dump
	TestCaseInfo.hWnd         = pTestCaseArgs->hWnd;                   // HWND to capture     

	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_PRIMRASTTEST_BASE;

	if (dwTableIndex >= D3DMQA_PRIMRASTTEST_COUNT)
	{
		DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}
	
	Result = PrimRastTestEx(pTestCaseArgs->pDevice,                      // LPDIRECT3DMOBILEDEVICE pDevice,
	                        pTestCaseArgs->hWnd,                         // HWND hWnd,
	                        &TestCaseInfo,                               // TESTCASEINSTANCE_INFO *pTestCaseInfo,
	                        pTestCaseArgs->pParms,                       // D3DMPRESENT_PARAMETERS* pPresentParms,
	                        &PrimRastCases[dwTableIndex],                // LPPRIM_RAST_TESTS pPrimRastTest
	                        pTestCaseArgs);                              // LPTESTCASEARGS pTestCaseArgs

cleanup:

	return Result;
}

