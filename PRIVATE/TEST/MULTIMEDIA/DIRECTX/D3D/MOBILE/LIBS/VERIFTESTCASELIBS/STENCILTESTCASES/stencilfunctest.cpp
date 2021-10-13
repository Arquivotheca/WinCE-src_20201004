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
#include <d3dm.h>
#include <tchar.h>
#include "DepthStencilTools.h"
#include "DebugOutput.h"
#include "util.h"
#include "StencilCases.h"
#include "CaptureBMP.h"
#include "ImageManagement.h"
#include <tux.h>
#include "DebugOutput.h"

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
// StencilFuncTest
//
//   Executes a single permutation of D3DMRS_STENCILFUNC stencil testing.  Permutations
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
INT StencilFuncTest(LPDIRECT3DMOBILEDEVICE pDevice, HWND hWnd, UINT uiTestPermutation)
{
    HRESULT hr;
    
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
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

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
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Obtain interface pointer for render target
	//
	if (FAILED(hr = pDevice->GetRenderTarget(&pRenderTarget)))
	{
		DebugOut(DO_ERROR, L"GetRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Obtain description of render target
	//
	if (FAILED(hr = pRenderTarget->GetDesc(&SurfaceDesc)))
	{
		DebugOut(DO_ERROR, L"GetDesc failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// If device does not support some prerequisite for this test, skip
	//
	if (!(VerifyTestCaseSupport(pDevice,                                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                        StencilFuncTests[uiTestPermutation].Format,       // D3DMFORMAT Format,
		                        StencilFuncTests[uiTestPermutation].StencilZFail, // D3DMSTENCILOP StencilZFail,
		                        StencilFuncTests[uiTestPermutation].StencilFail,  // D3DMSTENCILOP StencilFail,
		                        StencilFuncTests[uiTestPermutation].StencilPass,  // D3DMSTENCILOP StencilPass,
		                        StencilFuncTests[uiTestPermutation].StencilFunc)))// D3DMCMPFUNC StencilFunc
	{
		DebugOut(DO_ERROR, L"VerifyTestCaseSupport returned false. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	// 
	// Create geometry for test scene
	// 
	if (FAILED(hr = MakeStencilGeometry(pDevice, &pVB, D3DMCOLOR_XRGB(0,255,0), 0.0f)))

	{
		DebugOut(DO_ERROR, L"MakeStencilGeometry failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Create the D/S surface specified by this permutation
	//
	if (FAILED(hr = pDevice->CreateDepthStencilSurface(SurfaceDesc.Width,                         // UINT Width,
			                                           SurfaceDesc.Height,                        // UINT Height,
    		                                           StencilFuncTests[uiTestPermutation].Format,// D3DMFORMAT Format,
    		                                           D3DMMULTISAMPLE_NONE,                      // D3DMMULTISAMPLE_TYPE MultiSample,
    		                                           &pSurface)))                               // LPDIRECT3DMOBILESURFACE *ppSurface,
	{
		DebugOut(DO_ERROR, L"CreateDepthStencilSurface failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Attach D/S surface for use during rendering
	//
	if (FAILED(hr = pDevice->SetRenderTarget(NULL,pSurface)))
	{
		DebugOut(DO_ERROR, L"SetRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Prepare test case permutation
	//
	if (FAILED(hr = pDevice->Clear( 0L, NULL, D3DMCLEAR_STENCIL | D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255,0,0) , 1.0f, StencilFuncTests[uiTestPermutation].InitialStencilValue )))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	if (FAILED(hr = SetStencilStates(pDevice,                                              // LPDIRECT3DMOBILEDEVICE pDevice, 
	                                 StencilFuncTests[uiTestPermutation].StencilEnable,    // DWORD StencilEnable,
	                                 StencilFuncTests[uiTestPermutation].StencilMask,      // DWORD StencilMask,
	                                 StencilFuncTests[uiTestPermutation].StencilWriteMask, // DWORD StencilWriteMask,
	                                 StencilFuncTests[uiTestPermutation].StencilZFail,     // D3DMSTENCILOP StencilZFail,
	                                 StencilFuncTests[uiTestPermutation].StencilFail,      // D3DMSTENCILOP StencilFail,
	                                 StencilFuncTests[uiTestPermutation].StencilPass,      // D3DMSTENCILOP StencilPass,
	                                 StencilFuncTests[uiTestPermutation].StencilFunc,      // D3DMCMPFUNC StencilFunc,
	                                 StencilFuncTests[uiTestPermutation].StencilRef)))     // DWORD StencilRef
	{
		DebugOut(DO_ERROR, L"SetStencilStates failed. (hr = 0x%08x) Aborting.", hr);
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
	// Indicate active vertex buffer
	//
	if (FAILED(hr = pDevice->SetStreamSource( 0, pVB, sizeof(D3DQA_STENCILTEST) )))
	{
		DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,0,2)))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
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
	// Retrieve sampling location
	//
	if (FAILED(hr = GetScreenSpaceClientCenter(hWnd,     // HWND hWnd,
	                                           &Point))) // LPPOINT pPoint
	{
		DebugOut(DO_ERROR, L"GetScreenSpaceClientCenter failed (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve captured pixel components
	//
	if (FAILED(hr = GetPixel(pDevice,   // IDirect3DMobileDevice * pDevice,
						     Point.x,   // int iXPos,
						     Point.y,   // int iYPos,
						     &bRed,     // BYTE *pRed,
						     &bGreen,   // BYTE *pGreen,
						     &bBlue)))  // BYTE *pBlue
	{
		DebugOut(DO_ERROR, L"GetPixel failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Confirm that device's color result matches expectations
	//
	ColorResult = D3DMCOLOR_XRGB(bRed,bGreen,bBlue);
	if (ColorResult != StencilFuncTests[uiTestPermutation].ColorExpect)
	{
		DebugOut(DO_ERROR, L"Color comparison failed. Failing.");

		DebugOut(L"Expected:  (R: %lu, G: %lu, B: %lu)",
			D3DQA_GETRED(  StencilPassTests[uiTestPermutation].ColorExpect),
			D3DQA_GETGREEN(StencilPassTests[uiTestPermutation].ColorExpect),
			D3DQA_GETBLUE( StencilPassTests[uiTestPermutation].ColorExpect));

		DebugOut(L"  Actual:  (R: %lu, G: %lu, B: %lu)",bRed, bGreen, bBlue);

		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pSurface)
		pSurface->Release();

	if (pRenderTarget)
		pRenderTarget->Release();

	if (pVB)
		pVB->Release();

	return Result;
}
