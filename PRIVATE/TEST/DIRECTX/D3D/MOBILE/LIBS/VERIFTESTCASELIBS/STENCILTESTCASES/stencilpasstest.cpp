//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <d3dm.h>
#include <tchar.h>
#include "DepthStencilTools.h"
#include "DebugOutput.h"
#include "util.h"
#include "StencilCases.h"
#include "ImageManagement.h"
#include "CaptureBMP.h"
#include <tux.h>

//
// GetPixel (implemented in BlendTestCases)
//
//   Gets a pixel's color from the device's frontbuffer. Uses GetPixel(hdc, ...) from CaptureBMP tool.
//   *** This is a very bad method to use for getting multiple pixels :VERY SLOW ***
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice: The device from whose frontbuffer we'll be getting the pixel.
//   etc.
//
HRESULT GetPixel(LPDIRECT3DMOBILEDEVICE pDevice, int iXPos, int iYPos, BYTE * pRed, BYTE * pGreen, BYTE * pBlue);

//
// StencilPassTest
//
//   Executes a single permutation of D3DMRS_STENCILPASS stencil testing.  Permutations
//   are defined in a table in the header file.
//   
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   HWND hWnd:  Target window
//   UINT uiTestPermutation:  Test case permutation from predefined set of cases
//    
// Return Value:
//    
//   INT:  TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT
//
INT StencilPassTest(LPDIRECT3DMOBILEDEVICE pDevice, HWND hWnd, UINT uiTestPermutation)
{
	//
	// Result of test case execution
	//
	INT Result = TPR_PASS;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// D/S Surface
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Scene geometry variables
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pBackVB = NULL;
	LPDIRECT3DMOBILEVERTEXBUFFER pFrontVB = NULL;

	//
	// Original render target
	//
	LPDIRECT3DMOBILESURFACE pRenderTarget = NULL;
	D3DMSURFACE_DESC SurfaceDesc;

	//
	// Rendered colors/components
	//
	BYTE bRed, bGreen, bBlue;
	D3DMCOLOR ColorResult;

	//
	// Pixel-sampling location
	//
	POINT Point;

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
	// Obtain interface pointer for render target
	//
	if (FAILED(pDevice->GetRenderTarget(&pRenderTarget)))
	{
		OutputDebugString(_T("GetRenderTarget failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Obtain description of render target
	//
	if (FAILED(pRenderTarget->GetDesc(&SurfaceDesc)))
	{
		OutputDebugString(_T("GetDesc failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// If device does not support some prerequisite for this test, skip
	//
	if (!(VerifyTestCaseSupport(pDevice,                                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                        StencilPassTests[uiTestPermutation].Format,       // D3DMFORMAT Format,
		                        StencilPassTests[uiTestPermutation].StencilZFail, // D3DMSTENCILOP StencilZFail,
		                        StencilPassTests[uiTestPermutation].StencilFail,  // D3DMSTENCILOP StencilFail,
		                        StencilPassTests[uiTestPermutation].StencilPass,  // D3DMSTENCILOP StencilPass,
		                        StencilPassTests[uiTestPermutation].StencilFunc)))// D3DMCMPFUNC StencilFunc
	{
		OutputDebugString(_T("Skipping test case."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	// 
	// Create geometry for test scene
	// 
	if (FAILED(MakeStencilGeometry(pDevice, &pBackVB, STENCILPASSCONSTS.ColorBack, 0.0f)))

	{
		OutputDebugString(_T("MakeStencilGeometry failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}
	if (FAILED(MakeStencilGeometry(pDevice, &pFrontVB, STENCILPASSCONSTS.ColorFront, 0.0f)))

	{
		OutputDebugString(_T("MakeStencilGeometry failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Create the D/S surface specified by this permutation
	//
	if (FAILED(pDevice->CreateDepthStencilSurface(SurfaceDesc.Width,                         // UINT Width,
			                                      SurfaceDesc.Height,                        // UINT Height,
    		                                      StencilPassTests[uiTestPermutation].Format,// D3DMFORMAT Format,
    		                                      D3DMMULTISAMPLE_NONE,                      // D3DMMULTISAMPLE_TYPE MultiSample,
    		                                      &pSurface)))                               // LPDIRECT3DMOBILESURFACE *ppSurface,
	{
		OutputDebugString(_T("CreateDepthStencilSurface failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Attach D/S surface for use during rendering
	//
	if (FAILED(pDevice->SetRenderTarget(NULL,pSurface)))
	{
		OutputDebugString(_T("SetRenderTarget failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Prepare test case permutation
	//
	if (FAILED(pDevice->Clear( 0L, NULL, D3DMCLEAR_STENCIL | D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255,0,0) , 1.0f, StencilPassTests[uiTestPermutation].InitialStencilValue )))
	{
		OutputDebugString(_T("Clear failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}
	if (FAILED(SetStencilStates(pDevice,                                              // LPDIRECT3DMOBILEDEVICE pDevice, 
	                            StencilPassTests[uiTestPermutation].StencilEnable,    // DWORD StencilEnable,
	                            StencilPassTests[uiTestPermutation].StencilMask,      // DWORD StencilMask,
	                            StencilPassTests[uiTestPermutation].StencilWriteMask, // DWORD StencilWriteMask,
	                            StencilPassTests[uiTestPermutation].StencilZFail,     // D3DMSTENCILOP StencilZFail,
	                            StencilPassTests[uiTestPermutation].StencilFail,      // D3DMSTENCILOP StencilFail,
	                            StencilPassTests[uiTestPermutation].StencilPass,      // D3DMSTENCILOP StencilPass,
	                            StencilPassTests[uiTestPermutation].StencilFunc,      // D3DMCMPFUNC StencilFunc,
	                            StencilPassTests[uiTestPermutation].StencilRef)))     // DWORD StencilRef
	{
		OutputDebugString(_T("SetStencilStates failed."));
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
	// Indicate active vertex buffer
	//
	if (FAILED(pDevice->SetStreamSource( 0, pBackVB, sizeof(D3DQA_STENCILTEST) )))
	{
		OutputDebugString(_T("SetStreamSource failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,2)))
	{
		OutputDebugString(_T("DrawPrimitive failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Setup to test stencil buffer for equality to expected value
	//
	if (FAILED(SetStencilStates(pDevice,                                                  // LPDIRECT3DMOBILEDEVICE pDevice, 
	                            StencilPassTests[uiTestPermutation].StencilEnable,        // DWORD StencilEnable,
	                            StencilPassTests[uiTestPermutation].StencilMask,          // DWORD StencilMask,
	                            StencilPassTests[uiTestPermutation].StencilWriteMask,     // DWORD StencilWriteMask,
	                            StencilPassTests[uiTestPermutation].StencilZFail,         // D3DMSTENCILOP StencilZFail,
	                            StencilPassTests[uiTestPermutation].StencilFail,          // D3DMSTENCILOP StencilFail,
	                            StencilPassTests[uiTestPermutation].StencilPass,          // D3DMSTENCILOP StencilPass,
	                            D3DMCMP_EQUAL,                                             // D3DMCMPFUNC StencilFunc,
								StencilPassTests[uiTestPermutation].StencilValueExpect))) // DWORD StencilRef

	{
		OutputDebugString(_T("SetStencilStates failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Indicate active vertex buffer
	//
	if (FAILED(pDevice->SetStreamSource( 0, pFrontVB, sizeof(D3DQA_STENCILTEST) )))
	{
		OutputDebugString(_T("SetStreamSource failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,2)))
	{
		OutputDebugString(_T("DrawPrimitive failed."));
		Result = TPR_ABORT;
		goto cleanup;
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
	// Retrieve sampling location
	//
	if (FAILED(GetScreenSpaceClientCenter(hWnd,     // HWND hWnd,
	                                      &Point))) // LPPOINT pPoint
	{
		OutputDebugString(_T("GetScreenSpaceClientCenter failed"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve captured pixel components
	//
	if (FAILED(GetPixel(pDevice,   // IDirect3DMobileDevice * pDevice,
						Point.x,   // int iXPos,
						Point.y,   // int iYPos,
						&bRed,     // BYTE *pRed,
						&bGreen,   // BYTE *pGreen,
						&bBlue)))  // BYTE *pBlue
	{
		OutputDebugString(_T("GetPixel failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Confirm that device's color result matches expectations
	//
	ColorResult = D3DMCOLOR_XRGB(bRed,bGreen,bBlue);
	if (ColorResult != StencilPassTests[uiTestPermutation].ColorExpect)
	{
		DebugOut(_T("Color comparison failed."));

		DebugOut(_T("Expected:  (R: %lu, G: %lu, B: %lu)"),
			D3DQA_GETRED(  StencilPassTests[uiTestPermutation].ColorExpect),
			D3DQA_GETGREEN(StencilPassTests[uiTestPermutation].ColorExpect),
			D3DQA_GETBLUE( StencilPassTests[uiTestPermutation].ColorExpect));

		DebugOut(_T("  Actual:  (R: %lu, G: %lu, B: %lu)"),bRed, bGreen, bBlue);

		Result = TPR_FAIL;
  		goto cleanup;
	}

cleanup:

	if (pSurface)
		pSurface->Release();

	if (pRenderTarget)
		pRenderTarget->Release();

	if (pFrontVB)
		pFrontVB->Release();

	if (pBackVB)
		pBackVB->Release();

	return Result;
}
