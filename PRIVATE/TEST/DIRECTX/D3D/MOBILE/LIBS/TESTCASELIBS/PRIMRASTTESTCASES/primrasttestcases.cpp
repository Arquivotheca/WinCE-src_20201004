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
#include "util.h"
#include "ImageManagement.h"
#include "TestCases.h"
#include "DebugOutput.h"
#include "PrimRast.h"
#include "PrimRastPermutations.h"


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
		OutputDebugString(_T("Invalid argument(s)."));
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
	// Verify support shading arguments; skip if not available
	// 
	if (D3DMSHADE_GOURAUD == pPrimRastTest->ShadeMode)
	{
		if (!(D3DMPSHADECAPS_COLORGOURAUDRGB & Caps.ShadeCaps))
		{
			OutputDebugString(_T("Skipping due to no support for D3DMSHADE_GOURAUD."));
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Include last pixel in rasterization
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_LASTPIXEL, TRUE)))
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
	if (FAILED(pDevice->SetRenderState(D3DMRS_SHADEMODE, pPrimRastTest->ShadeMode)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE, ) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Set up fill mode
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_FILLMODE, pPrimRastTest->FillMode)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_FILLMODE, ) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Set up cull mode
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE) failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Setup COLORWRITEENABLE settings
	//
	if (FAILED(SetupColorWrite(pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
	                           pPrimRastTest->ColorWriteMask))) // COLOR_WRITE_MASK WriteMask
	{
		OutputDebugString(_T("Skipping test; D3DMPMISCCAPS_COLORWRITEENABLE not supported."));
		Result = TPR_SKIP;
		goto cleanup;
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
	switch(pPrimRastTest->PrimType)
	{
	case D3DMPT_POINTLIST:

		//
		// Point primitives
		//
		if (FAILED(DrawPoints(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
							  pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
							  pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							  pPrimRastTest->Format,            // D3DMFORMAT Format
							  ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawPoints failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}

		break;
	case D3DMPT_LINELIST:

		//
		// Line primitives
		//
		if (FAILED(DrawLineList(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                        pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                        pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							    pPrimRastTest->Format,            // D3DMFORMAT Format
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
		                         pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                         pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							     pPrimRastTest->Format,            // D3DMFORMAT Format
		                         ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawLinesStrip failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;
	case D3DMPT_TRIANGLELIST:
	
		//
		// Triangle primitives
		//
		if (FAILED(DrawTriangleList(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                            pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                            pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							        pPrimRastTest->Format,            // D3DMFORMAT Format
		                            ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawTriangleList failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;

	case D3DMPT_TRIANGLESTRIP:

		//
		// Triangle primitives
		//
		if (FAILED(DrawTriangleStrip(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                             pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                             pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							         pPrimRastTest->Format,            // D3DMFORMAT Format
		                             ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawTriangleStrip failed."));
			Result = TPR_FAIL;
			goto cleanup;
		}
		
		break;

	case D3DMPT_TRIANGLEFAN:

		//
		// Triangle primitives
		//
		if (FAILED(DrawTriangleFan(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                           pPrimRastTest->RenderFunc,        // RENDER_FUNC RenderFunc,
		                           pPrimRastTest->DrawRange,         // DRAW_RANGE DrawRange,
							       pPrimRastTest->Format,            // D3DMFORMAT Format
		                           ClientExtents)))                  // RECT RectTarget)
		{
			OutputDebugString(_T("DrawTriangleFan failed."));
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
		OutputDebugString(_T("Invalid argument(s)."));
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
		OutputDebugString(_T("Invalid test case ordinal."));
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

