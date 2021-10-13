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
#include "TestCases.h"
#include "TestPermutations.h"
#include <tux.h>
#include <tchar.h>
#include "ImageManagement.h"
#include "DebugOutput.h"

#define D3DQA_NUM_MULTISAMPLE_ENUMS 15
#define D3DQA_MAX_BACKBUFFERS 5

typedef struct _TESTCASEINSTANCE_INFO
{
	WCHAR *wszPath;  // Path for image dumps
	UINT uiInstance; // Test instance
	UINT uiTestID;   // Identifier for image dump
	HWND hWnd;       // HWND to capture
} TESTCASEINSTANCE_INFO;

INT SwapEffectCopyPartialPresent(LPTESTCASEARGS pTestCaseArgs, DWORD dwPartialPresentIndex);

INT SwapChainTest(LPTESTCASEARGS pTestCaseArgs)
{

	//
	// Index into D3DMRS_LASTPIXEL test cases
	//
	DWORD dwTableIndex;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		return TPR_ABORT;
	}

	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_SWAPCHAINTEST_BASE;

	//
	// Choose test case subset
	//
	if (dwTableIndex < PARTIALPRESENTCONSTS.TestCaseCount)
	{
		return SwapEffectCopyPartialPresent(pTestCaseArgs, dwTableIndex);
	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index. Aborting.");
		return TPR_ABORT;
	}

}

// 
// SwapEffectCopyPartialPresent
// 
//    Verifies that, when using the copy swap effect, Present is capable of copying partial
//    back-buffer contents
// 
// Arguments:
// 
// 
// Return Value:
// 
//    TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
// 
INT SwapEffectCopyPartialPresent(LPTESTCASEARGS pTestCaseArgs, DWORD dwPartialPresentIndex)
{
	//
	// All failure conditions set the result code specifically
	//
	INT Result = TPR_PASS;

	HRESULT hr;

	//
	// RECTs for partial Render Target clears
	//
	RECT rTopLeft;   
	RECT rTopRight;  
	RECT rBottomLeft;
	RECT rBottomRight;

	//
	// Viewport extents; render to partial viewport
	//
	D3DMVIEWPORT Viewport;
	RECT rRectSrc, rRectDst;

	//
	// This test will reset the device with required parameters
	//
	D3DMPRESENT_PARAMETERS PresentParms;

	//
	// Intermediate values towards computation of src/dest Present RECTs
	//
	LONG lNewSrcRectWidth, lNewSrcRectHeight;
	LONG lNewDstRectWidth, lNewDstRectHeight;

	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Start with same parameters as device's original creation
	//
	memcpy(&PresentParms, pTestCaseArgs->pParms, sizeof(D3DMPRESENT_PARAMETERS));

	//
	// Reset into swap-effect-copy mode; which mandates only one back-buffer
	//
	PresentParms.BackBufferCount = 1;
	PresentParms.SwapEffect = D3DMSWAPEFFECT_COPY;

	//
	// Obtain viewport extents
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"GetViewport failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Reset is expected to succeed
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->Reset(&PresentParms)))
	{
		DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Compute integer-based RECT region based on relative floating point rect
	//
	lNewSrcRectWidth  = (LONG)((float)Viewport.Width  * (RelativePartialViewports[dwPartialPresentIndex].fRectSrc.right  - RelativePartialViewports[dwPartialPresentIndex].fRectSrc.left));
	lNewSrcRectHeight = (LONG)((float)Viewport.Height * (RelativePartialViewports[dwPartialPresentIndex].fRectSrc.bottom - RelativePartialViewports[dwPartialPresentIndex].fRectSrc.top ));

	rRectSrc.left     = Viewport.X + (LONG)((float)Viewport.Width  * RelativePartialViewports[dwPartialPresentIndex].fRectSrc.left);
	rRectSrc.top      = Viewport.Y + (LONG)((float)Viewport.Height * RelativePartialViewports[dwPartialPresentIndex].fRectSrc.top);
	rRectSrc.right    = rRectSrc.left + lNewSrcRectWidth;
	rRectSrc.bottom   = rRectSrc.top  + lNewSrcRectHeight;

	lNewDstRectWidth  = (LONG)((float)Viewport.Width  * (RelativePartialViewports[dwPartialPresentIndex].fRectDst.right  - RelativePartialViewports[dwPartialPresentIndex].fRectDst.left));
	lNewDstRectHeight = (LONG)((float)Viewport.Height * (RelativePartialViewports[dwPartialPresentIndex].fRectDst.bottom - RelativePartialViewports[dwPartialPresentIndex].fRectDst.top ));

	rRectDst.left     = Viewport.X + (LONG)((float)Viewport.Width  * RelativePartialViewports[dwPartialPresentIndex].fRectDst.left);
	rRectDst.top      = Viewport.Y + (LONG)((float)Viewport.Height * RelativePartialViewports[dwPartialPresentIndex].fRectDst.top);
	rRectDst.right    = rRectDst.left + lNewDstRectWidth;
	rRectDst.bottom   = rRectDst.top  + lNewDstRectHeight;

	//
	// Clear the viewport to some known color
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->Clear(NULL,            // DWORD Count,
		                                          NULL,            // const D3DMRECT *pRects,
		                                          D3DMCLEAR_TARGET,// DWORD Flags,
		                                          0xFFFF0000,      // D3DMCOLOR Color,
		                                          1.0f,            // float Z,
		                                          0x00000000)))    // DWORD Stencil
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pTestCaseArgs->pDevice->Present(NULL,             // CONST RECT *pSourceRect,
									                NULL,             // CONST RECT *pDestRect,
									                NULL,             // HWND hDestWindowOverride,
									                NULL)))           // CONST RGNDATA *pDirtyRegion
	{
		DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Ensure that Present is reflected on screen
	//
	if (FAILED(hr = ForceFlush(pTestCaseArgs->pDevice)))
	{
		DebugOut(DO_ERROR, L"ForceFlush failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// RECTs for partial Render Target clears
	//
	rTopLeft.left       = Viewport.X;
	rTopLeft.top	    = Viewport.Y;
	rTopLeft.right      = Viewport.X+(Viewport.Width/2);
	rTopLeft.bottom     = Viewport.Y+(Viewport.Height/2);

	rTopRight.left      = Viewport.X+(Viewport.Width/2);
	rTopRight.top	    = Viewport.Y;
	rTopRight.right     = Viewport.X+Viewport.Width;
	rTopRight.bottom    = Viewport.Y+(Viewport.Height/2);

	rBottomLeft.left    = Viewport.X;
	rBottomLeft.top     = Viewport.Y+(Viewport.Height/2);
	rBottomLeft.right   = Viewport.X+(Viewport.Width/2);
	rBottomLeft.bottom  = Viewport.Y+ Viewport.Height;

	rBottomRight.left   = Viewport.X+(Viewport.Width/2);
	rBottomRight.top    = Viewport.Y+(Viewport.Height/2);
	rBottomRight.right  = Viewport.X+Viewport.Width;
	rBottomRight.bottom = Viewport.Y+ Viewport.Height;

	//
	// Fill Render Target
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->Clear(1,               // DWORD Count,
		                                          &rTopLeft,       // const D3DMRECT *pRects,
		                                          D3DMCLEAR_TARGET,// DWORD Flags,
		                                          0xFF000000,      // D3DMCOLOR Color,
		                                          1.0f,            // float Z,
		                                          0x00000000)))    // DWORD Stencil
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	if (FAILED(hr = pTestCaseArgs->pDevice->Clear(1,               // DWORD Count,
		                                          &rTopRight,      // const D3DMRECT *pRects,
		                                          D3DMCLEAR_TARGET,// DWORD Flags,
		                                          0xFFFFFFFF,      // D3DMCOLOR Color,
		                                          1.0f,            // float Z,
		                                          0x00000000)))    // DWORD Stencil
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	if (FAILED(hr = pTestCaseArgs->pDevice->Clear(1,               // DWORD Count,
		                                          &rBottomLeft,    // const D3DMRECT *pRects,
		                                          D3DMCLEAR_TARGET,// DWORD Flags,
		                                          0xFF0000FF,      // D3DMCOLOR Color,
		                                          1.0f,            // float Z,
		                                          0x00000000)))    // DWORD Stencil
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	if (FAILED(hr = pTestCaseArgs->pDevice->Clear(1,               // DWORD Count,
		                                          &rBottomRight,   // const D3DMRECT *pRects,
		                                          D3DMCLEAR_TARGET,// DWORD Flags,
		                                          0xFF00FF00,      // D3DMCOLOR Color,
		                                          1.0f,            // float Z,
		                                          0x00000000)))    // DWORD Stencil
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = pTestCaseArgs->pDevice->Present(&rRectSrc, // CONST RECT *pSourceRect,
									                &rRectDst, // CONST RECT *pDestRect,
									                NULL,      // HWND hDestWindowOverride,
									                NULL)))    // CONST RGNDATA *pDirtyRegion
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

	//
	// Reset is expected to succeed; resetting to original parameters
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->Reset(pTestCaseArgs->pParms)))
	{
		DebugOut(DO_ERROR, L"Reset failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

cleanup:

	return Result;
}
/*
// 
// SwapChainDumpContents
// 
//   Creates a swap chain with the specified number of back-buffers.  Fills buffers and dumps contents.
//
//   This test is of limited use with D3DMSWAPEFFECT_DISCARD, because this swap effect allows undefined
//   behavior, and thus a comparison with the reference driver is likely to incorrectly diagnose a bug. 
//
//   The copy swap effect only uses 1 back-buffer.
//
// 
// Arguments:
// 
//    LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//    D3DMPRESENT_PARAMETERS *pPresentParms:  Presentation parameters useful for device reset
//    TESTCASEINSTANCE_INFO *pTestInfo:  All info needed for image dumps
// 
// Return Value:
// 
//    TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
// 
INT SwapChainDumpContents(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiNumBackBuffers, D3DMSWAPEFFECT SwapEffect, D3DMPRESENT_PARAMETERS *pPresentParms, TESTCASEINSTANCE_INFO *pTestInfo)
{
	
	//
	// Device capabilities, for inspecting back-buffer limits
	//
	D3DMCAPS Caps;

	//
	// All failure conditions set the result code specifically
	//
	INT Result = TPR_PASS;

	//
	// For creating / filling swap chain
	//
	UINT uiMaxBackBuffers;
	UINT uiBackBufferIter;
	UINT uiSwapChainRot;
	UINT uiFrameNumber = 0;

	//
	// Interface pointer for back buffers
	//
	LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;

	//
	// Colors for filling swap chain buffers
	//
	CONST DWORD dwColors[] = {0xFFFF0000, 0xFFC00000, 0xFF800000, 0xFF400000, 0xFF000000, 0xFFFF0000, 0xFF00C000, 0xFF008000, 0xFF004000, 0xFF000000};
	UINT uiNextColor = 0;

	//
	// Image capture context
	//
	HDC hDC;

	//
	// Bad-parameter checks
	//
	if ((NULL == pDevice) || (0 == uiNumBackBuffers) || (NULL == pTestInfo))
	{
		DebugOut(DO_ERROR, L"SwapChainDumpContents:  invalid argument(s).");
		Result = TPR_FAIL;
		goto cleanup;
	}
	if (!((D3DMSWAPEFFECT_FLIP == SwapEffect) || (D3DMSWAPEFFECT_COPY == SwapEffect)))
	{
		DebugOut(DO_ERROR, L"SwapChainDumpContents:  invalid argument(s).");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed.");
		Result = TPR_ABORT;
		goto cleanup;
	}
	

	uiMaxBackBuffers = Caps.MaxBackBuffer;

  If uncommenting this code, add special-case handling for the zero-valued caps bit case, which means
  no theoretical limit on backbuffers


	//
	// Bad-parameter checks
	//
	if ((uiNumBackBuffers > uiMaxBackBuffers) || (uiNumBackBuffers > D3DQA_MAX_BACKBUFFERS))
	{
		DebugOut(DO_ERROR, L"Unsupported number of back buffers.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Reset is expected to succeed
	//
	pPresentParms->BackBufferCount = uiNumBackBuffers;
	if (FAILED(pDevice->Reset(pPresentParms)))
	{
		DebugOut(DO_ERROR, L"Reset failed.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Initialize swap chain to a known state
	//
	for (uiBackBufferIter = 0; uiBackBufferIter < uiNumBackBuffers; uiBackBufferIter++)
	{

		//
		// Get the backbuffer, to determine the format thereof
		//
		if (FAILED(pDevice->GetBackBuffer(uiBackBufferIter,        // UINT BackBuffer,
		                                  D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
		                                  &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
		{
			DebugOut(DO_ERROR, L"GetBackBuffer failed.");
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Put known data in the farthest-back-buffer
		//
		if (FAILED(pDevice->ColorFill(pBackBuffer,NULL,dwColors[uiNextColor])))
		{
			DebugOut(DO_ERROR, L"ColorFill failed.");
			Result = TPR_FAIL;
			goto cleanup;
		}
		uiNextColor++;

		pBackBuffer->Release();
		pBackBuffer = NULL;

	}

	//
	// Dump frames as swap chain is rotated
	//
	for (uiSwapChainRot = 0; uiSwapChainRot < uiNumBackBuffers; uiSwapChainRot++)
	{

		//
		// Rotate swap chain
		//
		if (FAILED(pDevice->Present(NULL,             // CONST RECT *pSourceRect,
									NULL,             // CONST RECT *pDestRect,
									NULL,             // HWND hDestWindowOverride,
									NULL)))           // CONST RGNDATA *pDirtyRegion
		{
			DebugOut(DO_ERROR, L"Present failed.");
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve "farthest back" backbuffer
		//
		if (FAILED(pDevice->GetBackBuffer((uiNumBackBuffers-1),    // UINT BackBuffer,
		                                  D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
		                                  &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
		{
			DebugOut(DO_ERROR, L"GetBackBuffer failed.");
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Fill "farthest back" backbuffer
		//
		if (FAILED(pDevice->ColorFill(pBackBuffer,NULL,dwColors[uiNextColor])))
		{
			DebugOut(DO_ERROR, L"ColorFill failed.");
			Result = TPR_FAIL;
			goto cleanup;
		}
		uiNextColor++;

		pBackBuffer->Release();
		pBackBuffer = NULL;

		//
		// Flush the swap chain and capture a frame
		//
		if (FAILED(DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
									0,             // UINT uiFrameNumber,
									NULL)))        // RECT *pRect = NULL
		{
			DebugOut(DO_ERROR, L"DumpFlushedFrame failed.");
			Result = TPR_FAIL;
			goto cleanup;
		}

		uiFrameNumber++;

		//
		// Record the contents of each back buffer
		//
		for (uiBackBufferIter = 0; uiBackBufferIter < uiNumBackBuffers; uiBackBufferIter++)
		{
			//
			// Get the backbuffer, to determine the format thereof
			//
			if (FAILED(pDevice->GetBackBuffer(0,                       // UINT BackBuffer,
											  D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
											  &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
			{
				DebugOut(DO_ERROR, L"GetBackBuffer failed.");
				Result = TPR_ABORT;
				goto cleanup;
			}

			//
			// This will force a flush
			//
			if( FAILED( pBackBuffer->GetDC(&hDC)))
			{
				DebugOut(DO_ERROR, L"GetDC failed.");
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
				DebugOut(DO_ERROR, L"DumpFlushedFrame failed.");
				Result = TPR_FAIL;
				goto cleanup;
			}

			uiFrameNumber++;

			pBackBuffer->ReleaseDC(hDC);
			pBackBuffer->Release();
			pBackBuffer = NULL;
		}

	}

cleanup:

	if (pBackBuffer)
		pBackBuffer->Release();

	return Result;

}


// 
// SwapChainDiscardEffect
// 
//    Places some known data in the back-buffer that is the farthest from becoming the front buffer;
//    then calls Present exactly the number of times that should cause that known back-buffer to
//    become the front buffer.  Captures the image at this point, for verification.
// 
// Arguments:
// 
//    LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D device
//    UINT uiNumBackBuffers:  Number of back-buffers; if beyond limit, function will skip
//    D3DMPRESENT_PARAMETERS *pPresentParms:  Presentation parameters useful for device reset
//    TESTCASEINSTANCE_INFO *pTestInfo:  All info needed for image dumps
// 
// Return Value:
// 
//    TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
// 
INT SwapChainDiscardEffect(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiNumBackBuffers, D3DMPRESENT_PARAMETERS *pPresentParms, TESTCASEINSTANCE_INFO *pTestInfo)
{
	
	//
	// Device capabilities, for inspecting back-buffer limits
	//
	D3DMCAPS Caps;

	//
	// All failure conditions set the result code specifically
	//
	INT Result = TPR_PASS;

	//
	// Interface pointer for back buffers
	//
	LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;

	//
	// For creating / filling / flushing swap chain
	//
	UINT uiMaxBackBuffers;
	UINT uiSwapChainRotate;

	//
	// Colors for filling swap chain buffers
	//
	CONST DWORD dwColors[D3DQA_MAX_BACKBUFFERS] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF, 0xFF000000};

	//
	// Bad-parameter checks
	//
	if ((NULL == pDevice) || (0 == uiNumBackBuffers) || (NULL == pTestInfo))
	{
		DebugOut(DO_ERROR, L"SwapChainDumpContents:  invalid argument(s).");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	uiMaxBackBuffers = Caps.MaxBackBuffer;

  If uncommenting this code, add special-case handling for the zero-valued caps bit case, which means
  no theoretical limit on backbuffers

	//
	// Bad-parameter checks
	//
	if ((uiNumBackBuffers > uiMaxBackBuffers) || (uiNumBackBuffers > D3DQA_MAX_BACKBUFFERS))
	{
		DebugOut(DO_ERROR, L"Unsupported number of back buffers.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Reset is expected to succeed
	//
	pPresentParms->BackBufferCount = uiNumBackBuffers;
	if (FAILED(pDevice->Reset(pPresentParms)))
	{
		DebugOut(DO_ERROR, L"Reset failed.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Get the backbuffer, to determine the format thereof
	//
	if (FAILED(pDevice->GetBackBuffer((uiNumBackBuffers-1),    // UINT BackBuffer,
	                                  D3DMBACKBUFFER_TYPE_MONO,// D3DMBACKBUFFER_TYPE Type,
	                                  &pBackBuffer)))          // LPDIRECT3DMOBILESURFACE *ppBackBuffer
	{
		DebugOut(DO_ERROR, L"GetBackBuffer failed.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Put known data in the farthest-back-buffer
	//
	if (FAILED(pDevice->ColorFill(pBackBuffer,NULL,0xFFFF0000)))
	{
		DebugOut(DO_ERROR, L"ColorFill failed.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	pBackBuffer->Release();
	pBackBuffer = NULL;

	//
	// Present exactly the number of times that should cause the filled back buffer
	// to become the front buffer, regardless of semantics that the driver chooses
	// to implement discard with
	//
	for(uiSwapChainRotate = 0; uiSwapChainRotate < uiNumBackBuffers; uiSwapChainRotate++)
	{
		//
		// Rotate swap chain
		//
		if (FAILED(pDevice->Present(NULL,             // CONST RECT *pSourceRect,
									NULL,             // CONST RECT *pDestRect,
									NULL,             // HWND hDestWindowOverride,
									NULL)))           // CONST RGNDATA *pDirtyRegion
		{
			DebugOut(DO_ERROR, L"Present failed.");
			Result = TPR_FAIL;
			goto cleanup;
		}
	}

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                            0,             // UINT uiFrameNumber,
	                            NULL)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed.");
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	if (pBackBuffer)
		pBackBuffer->Release();

	return Result;

}

*/
