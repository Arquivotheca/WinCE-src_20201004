//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "BlendCases.h"
#include "ImageManagement.h"
#include "BufferTools.h"
#include "BlendTools.h"
#include "DebugOutput.h"
#include "VerifTestCases.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

static void Debug(LPCTSTR szFormat, ...);

//
// Maximum allowable length for OutputDebugString text
//
#define MAX_DEBUG_OUT 256

#define countof(x)  (sizeof(x)/sizeof(*(x)))

#define D3DMQA_BLEND_TOLERANCE 8

//
// Resize viewport to D3DMQA_VIEWPORT_EXTENT x D3DMQA_VIEWPORT_EXTENT, centered
// within original viewport extents
//
#define D3DMQA_VIEWPORT_EXTENT 8

//
// Using the full granularity that this test is capable of is very time consuming; skip some iterations
//
#define D3DMQA_ITER_SKIP 8

//
// GetPixel
//
//   Gets a pixel's color from the device's frontbuffer. Uses GetPixel(hdc, ...) from CaptureBMP tool.
//   *** This is a very bad method to use for getting multiple pixels :VERY SLOW ***
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice: The device from whose frontbuffer we'll be getting the pixel.
//   etc.
//

HRESULT GetPixel(LPDIRECT3DMOBILEDEVICE pDevice, int iXPos, int iYPos, BYTE * pRed, BYTE * pGreen, BYTE * pBlue)
{
    IDirect3DMobileSurface *pFrontBuffer = NULL;
    HRESULT hr = S_OK;
    HDC hDC = NULL;
    D3DMDISPLAYMODE Mode;

    //
    // The current display mode is needed, to determine desired format
    //
    if( FAILED( pDevice->GetDisplayMode( &Mode ) ) )
    {
        Debug(_T("IDirect3DMobile::GetDisplayMode failed."));
        hr = E_FAIL;
        goto cleanup;
    }
    
    //
    // Create an image surface, of the same size to receive the front buffer contents
    //
    if( FAILED( pDevice->CreateImageSurface(Mode.Width,     // UINT Width,
                                                           Mode.Height,    // INT Height,
                                                           Mode.Format,    // D3DMFORMAT Format,
                                                           &pFrontBuffer)))// IDirect3DMobileSurface** ppSurface
    {
        Debug(_T("IDirect3DMobileDevice::CreateImageSurface failed."));
        hr = E_FAIL;
        goto cleanup;
    }

    //
    // Retrieve front buffer copy 
    //
    if( FAILED( pDevice->GetFrontBuffer(pFrontBuffer))) // IDirect3DMobileSurface* pFrontBuffer
    {
        Debug(_T("IDirect3DMobileDevice::GetFrontBuffer failed."));
        hr = E_FAIL;
        goto cleanup;
    }

    //
    // Confirm result
    //
    if (NULL == pFrontBuffer)
    {
        Debug(_T("IDirect3DMobileDevice::GetFrontBuffer resulted in NULL LPDIRECT3DMOBILESURFACE."));
        hr = E_FAIL;
        goto cleanup;
    }

    //
    // Retrieve front-buffer DC
    //
    if (FAILED( pFrontBuffer->GetDC(&hDC))) // HDC* phdc
    {
        Debug(_T("IDirect3DMobileSurface::GetDC failed."));
        hr = E_FAIL;
        goto cleanup;
    }

    hr = GetPixel(hDC, iXPos, iYPos, pRed, pGreen, pBlue);

cleanup:


    //
    // Cleanup front buffer if valid
    //
    if (pFrontBuffer)
    {
        //
        // Release the front-buffer DC
        //
        if (NULL != hDC)
        {
            if (FAILED( pFrontBuffer->ReleaseDC(hDC)))
            {
                Debug(_T("IDirect3DMobileSurface::ReleaseDC failed."));
            }
        }

        if (0 != pFrontBuffer->Release())
        {
            Debug(_T("IDirect3DMobileSurface::Release != 0."));
        }
    }

    return hr;

}

// 
// CreateTestColors
// 
//   Generates color components to be used for each iteration of the test case.
// 
// Arguments:
// 
//   UINT uiIteration:  Iteration to generate color components for
//   DWORD *pdwColorFront:  Front primitive
//   DWORD *pdwColorBack:  Back primitive
//   
// Return Value:
//   
//   HRESULT:  Indicates success or failure
// 
HRESULT CreateTestColors(UINT uiIteration, DWORD *pdwColorFront, DWORD *pdwColorBack)
{
	BYTE RedFront, GreenFront, BlueFront, AlphaFront;
	BYTE RedBack,  GreenBack,  BlueBack,  AlphaBack;
	BYTE FrontColor, BackColor;

	//
	// Enforce upper bound on iterations
	//
	if (uiIteration > D3DQA_ITERATIONS)
		return E_FAIL;

	//
	//                  ITERATION "MAP"
	//
	// |  Range  |        Front Color       |  Back Color   |
	// +---------+--------------------------+---------------+
	// |   0-255 |  Red   w/ Variable Alpha |      Red      |
	// | 256-511 |  Red   w/ Variable Alpha |     Green     |
	// | 512-767 |  Red   w/ Variable Alpha |      Blue     |
	// | 768-1023|  Green w/ Variable Alpha |      Red      |
	// |1024-1279|  Green w/ Variable Alpha |     Green     |
	// |1280-1535|  Green w/ Variable Alpha |      Blue     |
	// |1536-1791|  Blue  w/ Variable Alpha |      Red      |
	// |1792-2047|  Blue  w/ Variable Alpha |     Green     |
	// |2048-2303|  Blue  w/ Variable Alpha |      Blue     |
	//
	AlphaFront = uiIteration % 256;
	AlphaBack = 0xFF;
	FrontColor = uiIteration/768;
	BackColor = (uiIteration/256)%3;

	RedFront = GreenFront = BlueFront = 0x00;
	RedBack  = GreenBack  = BlueBack = 0x00;
	
	switch(FrontColor)
	{
		case 0:
			RedFront = 0xFF;
			break;
		case 1:
			GreenFront = 0xFF;
			break;
		case 2:
			BlueFront = 0xFF;
			break;
	}

	switch(BackColor)
	{
		case 0:
			RedBack = 0xFF;
			break;
		case 1:
			GreenBack = 0xFF;
			break;
		case 2:
			BlueBack = 0xFF;
			break;
	}

	*pdwColorFront = D3DMCOLOR_RGBA(RedFront,GreenFront,BlueFront,AlphaFront);
	*pdwColorBack  = D3DMCOLOR_RGBA(RedBack ,GreenBack ,BlueBack ,AlphaBack );

	return S_OK;
}

// 
// SetupIter
// 
//   Sets up the test case, which will be put to use in the render loop.
// 
// Arguments:
// 
//   UINT uiIteration:  Iteration number (frame) currently under test
//   LPDIRECT3DMOBILEVERTEXBUFFER:  Vertex buffer
//   
// Return Value:
//   
//   HRESULT:  Indicates success or failure
// 
HRESULT SetupIter(
    UINT uiIteration,
	LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	D3DMBLEND SourceBlend,
	D3DMBLEND DestBlend,
	D3DMBLENDOP BlendOp
    )
{
	CUSTOMVERTEX *pVertices;

	DWORD dwColorFront, dwColorBack;

	if (FAILED(CreateTestColors(uiIteration,   // UINT uiIteration,
							   &dwColorFront,  // DWORD *pdwColorFront
							   &dwColorBack))) // DWORD *pdwColorBack
	{
		OutputDebugString(_T("CreateTestColors failed."));
		return E_FAIL;
	}

	//
	// Prepare a vertex buffer with the required color characteristics
	//
	if( FAILED( (*ppVB)->Lock( 0, 0, (VOID**)&pVertices, 0 ) ) )
	    return E_FAIL;

	ColorVertRange((BYTE*)(&pVertices[0]),        // BYTE*,
			       6,                             // UINT uiCount
				   dwColorBack,                   // DWORD dwColor
				   sizeof(CUSTOMVERTEX),          // UINT uiStride
				   offsetof(CUSTOMVERTEX,color)); // UINT uiOffset

	ColorVertRange((BYTE*)(&pVertices[6]),       // BYTE*,
			       6,                            // UINT uiCount
				   dwColorFront,                 // DWORD dwColor
				   sizeof(CUSTOMVERTEX),         // UINT uiStride
				   offsetof(CUSTOMVERTEX,color));// UINT uiOffset

	(*ppVB)->Unlock();

	return S_OK;
}


// 
// DrawIter
// 
//   Draws the primitives for this iteration
// 
// Arguments:
// 
//   UINT uiTestCase:  Test case enumerator
//   UINT uiIteration:  Iteration number (frame) currently under test
//   
// Return Value:
//   
//   HRESULT:  Indicates success or failure
// 
HRESULT DrawIter(
    LPDIRECT3DMOBILEDEVICE pDevice,
    UINT uiTestCase,
    UINT uiIteration,
 	D3DMBLEND SourceBlend,
	D3DMBLEND DestBlend,
	D3DMBLENDOP BlendOp
    )
{
	//
	// Render a sequence of nonindexed, geometric primitives of the specified type
	// from the current data input stream
	//
	pDevice->SetRenderState(D3DMRS_SRCBLEND,  D3DMBLEND_ONE);
	pDevice->SetRenderState(D3DMRS_DESTBLEND, D3DMBLEND_ZERO);
	pDevice->SetRenderState(D3DMRS_BLENDOP,   D3DMBLENDOP_ADD);
	if( FAILED( pDevice->DrawPrimitive( D3DMPT_TRIANGLELIST, 0, 2 ) ) )
	{
		OutputDebugString(_T("DrawPrimitive failed"));
		return E_FAIL;
	}

	pDevice->SetRenderState(D3DMRS_SRCBLEND,  SourceBlend);
	pDevice->SetRenderState(D3DMRS_DESTBLEND, DestBlend);
	pDevice->SetRenderState(D3DMRS_BLENDOP,   BlendOp);
	if( FAILED( pDevice->DrawPrimitive( D3DMPT_TRIANGLELIST, 6, 2 ) ) )
	{
		OutputDebugString(_T("DrawPrimitive failed"));
		return E_FAIL;
	}

	return S_OK;
}


// 
// VerifyIter
// 
//   Compare the expected color components to the actual color components, to verify
//   successful execution of the test case.
// 
// Arguments:
//   
//   UINT uiIteration:  Iteration number (frame) currently under test
//   UINT uiTolerance:  Allowable inaccuracy in color value
//   BYTE Red   \
//   BYTE Green   Actual color components resulting from test
//   BYTE Blue  /
//   D3DMBLEND SourceBlend: 
//   D3DMBLEND DestBlend: 
//   D3DMBLENDOP BlendOp: 
//   
// Return Value:
//   
//   HRESULT:  Success indicates that the color components matched expectations; failure otherwise.
// 
HRESULT VerifyIter(
    UINT uiIteration,
	UINT uiTolerance,
	BYTE Red,
	BYTE Green,
	BYTE Blue,
 	D3DMBLEND SourceBlend,
	D3DMBLEND DestBlend,
	D3DMBLENDOP BlendOp
   )
{
	DWORD dwColorResult = 0;
	DWORD dwColorFront;
	DWORD dwColorBack;
	UINT uiRedDiff, uiGreenDiff, uiBlueDiff;

	//
	// Determine colors for this iteration
	//
	if (FAILED(CreateTestColors(uiIteration,  // UINT uiIteration,
							   &dwColorFront, // DWORD *pdwColorFront
							   &dwColorBack)))// DWORD *pdwColorBack
	{
		OutputDebugString(_T("CreateTestColors failed."));
		return E_FAIL;
	}

	//
	// Blend source and dest
	// 
	if (FAILED(Blend(BlendOp, SourceBlend, DestBlend, 
					 dwColorFront, dwColorBack, &dwColorResult)))
	{
		OutputDebugString(_T("Blend failed."));
		return E_FAIL;
	}

	//
	// Compute absolute diffs
	//
	uiRedDiff   = (UINT)abs((int)(D3D_GETR(dwColorResult))-(int)(Red));
	uiGreenDiff = (UINT)abs((int)(D3D_GETG(dwColorResult))-(int)(Green));
	uiBlueDiff  = (UINT)abs((int)(D3D_GETB(dwColorResult))-(int)(Blue));

	//
	// Verify that result was close to expectations
	//
	if ((uiRedDiff   > uiTolerance) ||
	    (uiGreenDiff > uiTolerance) ||
	    (uiBlueDiff  > uiTolerance))
	{
		Debug(_T("Expected:  [R=%u;G=%u;B=%u]"), (UINT)(D3D_GETR(dwColorResult)), (UINT)(D3D_GETG(dwColorResult)), (UINT)(D3D_GETB(dwColorResult)));
		Debug(_T("  Actual:  [R=%u;G=%u;B=%u]"), (UINT)Red, (UINT)(Green), (UINT)(Blue));
		Debug(_T("   Diffs:  [R=%u;G=%u;B=%u]"), uiRedDiff, uiGreenDiff, uiBlueDiff);
		return E_FAIL;
	}

	return S_OK;
}


//
// ExecuteTestCase
// 
//    Renders and verifies the results of one test case; consisting of many iterations.
//
// Arguments:
//
//   UINT uiTestCase:  Ordinal of test case to run
//   UINT uiSkipCount:  If entire set of permutations is desired, set to 1; otherwise set
//                      to "step size" indicating number of iterations to skip.  For example,
//                      3 indicates that the following iterations will run:  0, 3, 6, 9.
//   UINT uiTolerance:  Absolute difference allowed between expected result and actual result
//
// Return Value:
//
//   INT:  TPR_PASS, TPR_FAIL, TPR_ABORT, or TPR_SKIP
// 


//INT ExecuteTestCase(UINT uiTestCase, UINT uiSkipCount, UINT uiTolerance)

TESTPROCAPI BlendTest(LPVERIFTESTCASEARGS pTestCaseArgs)
{
	OutputDebugString(_T("Beginning ExecuteTestCase."));

	//
	// Vertex Buffer
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

	//
	// Actual color component results, to compare with expected
	//
	BYTE RedResult, GreenResult, BlueResult;

	//
	// Extents of viewport
	//
	D3DMVIEWPORT OldViewport, NewViewport;

	//
	// Window client extents
	//
	RECT WndRect;

	//
	// Frame counter
	//
	UINT uiFrameIndex;

	//
	// Return value
	//
	HRESULT Result = TPR_PASS;

	//
	// Vertex buffer locked bits
	//
	CUSTOMVERTEX *pVertices = NULL;

	//
	// Sampling location for GetPixel
	//
	POINT Point;

	//
	// Index into table of permutations
	//
	DWORD dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_BLENDTEST_BASE;

	//
	// Skip test cases that use an unsupported feature
	//
	if (FALSE == IsBlendSupported(pTestCaseArgs->pDevice, AlphaTests[dwTableIndex].SourceBlend, AlphaTests[dwTableIndex].DestBlend, AlphaTests[dwTableIndex].BlendOp))
	{
		OutputDebugString(_T("Blend not supported."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// A vertex buffer is needed for this test; create and bind to data stream
	// 
	pVB = CreateActiveBuffer(pTestCaseArgs->pDevice,                        // LPDIRECT3DMOBILEDEVICE pd3dDevice,
							 D3DQA_NUMVERTS,                      // UINT uiNumVerts,
							 D3DMFVF_CUSTOMVERTEX,                // DWORD dwFVF,
							 BytesPerVertex(D3DMFVF_CUSTOMVERTEX),// DWORD dwFVFSize,
							 0);                                  // DWORD dwUsage
	if (NULL == pVB)
	{
		OutputDebugString(_T("CreateActiveBuffer failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Clear entire extents of original viewport before moving to smaller viewport
	//
	pTestCaseArgs->pDevice->Clear( 0, NULL, D3DMCLEAR_TARGET|D3DMCLEAR_ZBUFFER,
						D3DMCOLOR_XRGB(128,128,128), 1.0f, 0 );


	//
	// Get old viewport
	//
	if( FAILED( pTestCaseArgs->pDevice->GetViewport(&OldViewport) ) )
	{
		OutputDebugString(_T("GetViewport failed"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Resize to very small viewport, centered within original extents
	//
	NewViewport.Width  = D3DMQA_VIEWPORT_EXTENT;
	NewViewport.Height = D3DMQA_VIEWPORT_EXTENT;
	NewViewport.X = OldViewport.X + (OldViewport.Width / 2) - (NewViewport.Width / 2);
	NewViewport.Y = OldViewport.Y + (OldViewport.Height / 2) - (NewViewport.Height / 2);
	NewViewport.MinZ = OldViewport.MinZ;    
	NewViewport.MaxZ = OldViewport.MaxZ;

	//
	// Set new viewport
	//
	if( FAILED( pTestCaseArgs->pDevice->SetViewport(&NewViewport) ) )
	{
		OutputDebugString(_T("SetViewport failed"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Indicate vertex format
	//
	if( FAILED( pTestCaseArgs->pDevice->SetStreamSource( 0, pVB, 0) ) )
	{
		OutputDebugString(_T("SetFVF failed"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// This function retrieves the coordinates of a window's client area. 
	// The left and top members are zero. 
	//
	// Zero indicates failure.
	//
	if (0 == GetClientRect( pTestCaseArgs->hWnd,
							&WndRect))
	{
		OutputDebugString(_T("GetClientRect failed"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Sampling location for GetPixel
	//
	Point.x = WndRect.right / 2;
	Point.y = WndRect.bottom / 2;

	//
	// This function converts the client coordinates of a specified point to screen coordinates.
	// Zero indicates failure. 
	//
	if (0 == ClientToScreen(pTestCaseArgs->hWnd,  // HWND hWnd, 
	                        &Point))              // LPPOINT lpPoint
	{
		OutputDebugString(_T("ClientToScreen failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Informative debug spew:  sampling location should be at center of client
	// area of window (not center of window's bounding rect)
	// 
	DebugOut(_T("GetPixel will sample at (%li,%li)"), Point.x, Point.y);


	if( FAILED( pVB->Lock( 0, 0, (VOID**)(&pVertices), 0 ) ) )
	{
		OutputDebugString(_T("Lock failed"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	RectangularTLTriList((BYTE*)(&pVertices[0]),    // BYTE*,
			                NewViewport.X,          // UINT uiX
			                NewViewport.Y,          // UINT uiY
			                NewViewport.Width,      // UINT uiWidth
			                NewViewport.Height,     // UINT uiHeight
			                D3DQA_DEPTH_BACK,       // float fDepth
			                sizeof(CUSTOMVERTEX));  // UINT uiStride

	RectangularTLTriList((BYTE*)(&pVertices[6]),    // BYTE*,
			                NewViewport.X,          // UINT uiX
			                NewViewport.Y,          // UINT uiY
			                NewViewport.Width,      // UINT uiWidth
			                NewViewport.Height,     // UINT uiHeight
			                D3DQA_DEPTH_FRONT,      // float fDepth
			                sizeof(CUSTOMVERTEX));  // UINT uiStride

	pVB->Unlock();



	//
	// Prepare for vertex alpha blending test case
	//
	pTestCaseArgs->pDevice->SetRenderState(D3DMRS_DIFFUSEMATERIALSOURCE, D3DMMCS_COLOR1);
	pTestCaseArgs->pDevice->SetRenderState(D3DMRS_ALPHABLENDENABLE, TRUE);
	pTestCaseArgs->pDevice->SetRenderState(D3DMRS_ZENABLE,   D3DMZB_TRUE);

	for (uiFrameIndex = 0; uiFrameIndex < D3DQA_ITERATIONS; uiFrameIndex+=D3DMQA_ITER_SKIP)
	{
		//
		// Clear the backbuffer and the zbuffer
		//
		pTestCaseArgs->pDevice->Clear( 0, NULL, D3DMCLEAR_TARGET|D3DMCLEAR_ZBUFFER,
							D3DMCOLOR_XRGB(0,0,0), 1.0f, 0 );

		//
		// Set up test scenario
		//
		if (FAILED(SetupIter(
			uiFrameIndex,  // UINT uiIteration,
			&pVB,
			AlphaTests[dwTableIndex].SourceBlend, // D3DMBLEND SourceBlend
			AlphaTests[dwTableIndex].DestBlend,   // D3DMBLEND DestBlend
			AlphaTests[dwTableIndex].BlendOp)))   // D3DMBLENDOP BlendOp
		{
			OutputDebugString(_T("Failed to generate vertex buffer for this iteration."));
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Scene rendering is about to begin
		//
		if( FAILED( pTestCaseArgs->pDevice->BeginScene() ) )
		{
			OutputDebugString(_T("BeginScene failed"));
			Result = TPR_ABORT;
			goto cleanup;
		}


		//
		// Verify resulting pixel component colors
		//
		if (FAILED(DrawIter(pTestCaseArgs->pDevice,               // LPDIRECT3DMOBILEDEVICE pDevice
		                    dwTableIndex,                         // UINT dwTableIndex
		                    uiFrameIndex,                         // UINT uiIteration,
		                    AlphaTests[dwTableIndex].SourceBlend, // D3DMBLEND SourceBlend
		                    AlphaTests[dwTableIndex].DestBlend,   // D3DMBLEND DestBlend
		                    AlphaTests[dwTableIndex].BlendOp)))   // D3DMBLENDOP BlendOp
		{
			OutputDebugString(_T("DrawIter failed."));
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// End the scene
		//
		if( FAILED( pTestCaseArgs->pDevice->EndScene() ) )
		{
			OutputDebugString(_T("EndScene failed"));
			Result = TPR_ABORT;
			goto cleanup;
		}

		if( FAILED( pTestCaseArgs->pDevice->Present( NULL, NULL, NULL, NULL ) ) )
		{
			OutputDebugString(_T("Present failed"));
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Retrieve captured pixel components
		//
		if (FAILED(GetPixel(pTestCaseArgs->pDevice,
		                    Point.x,                       // int iXPos,
		                    Point.y,                       // int iYPos,
		                    &RedResult,                    // BYTE *pRed,
		                    &GreenResult,                  // BYTE *pGreen,
		                    &BlueResult)))                 // BYTE *pBlue
		{
			OutputDebugString(_T("GetPixel failed."));
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Verify resulting pixel component colors
		//
		if (FAILED(VerifyIter(
									uiFrameIndex,                    // UINT uiIteration,
									D3DMQA_BLEND_TOLERANCE,          // UINT uiTolerance
									RedResult,                       // BYTE Red,
									GreenResult,                     // BYTE Green,
									BlueResult,                      // BYTE Blue
									AlphaTests[dwTableIndex].SourceBlend, // D3DMBLEND SourceBlend
									AlphaTests[dwTableIndex].DestBlend,   // D3DMBLEND DestBlend
									AlphaTests[dwTableIndex].BlendOp)))   // D3DMBLENDOP BlendOp
		{
			OutputDebugString(_T("VerifyIter failed."));
			Result = TPR_FAIL;

			//
			// Even if test fails because of mismatch, defer the failure; additional iterations may provide
			// more context for debugging
			//
		}

	}

cleanup:

	if( pVB != NULL )
        pVB->Release();

	return Result;
}


//
// Debug
//
//   Printf-like wrapping around OutputDebugString.
//
// Arguments:  
//   
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//   
// Return Value:
//  
//   void
//
void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("D3DQA: ");
    TCHAR   szBuffer[MAX_DEBUG_OUT];

    va_list pArgs;
    va_start(pArgs, szFormat);
    _tcscpy(szBuffer, szHeader);

    _vsntprintf(
        &szBuffer[countof(szHeader)-1],
		MAX_DEBUG_OUT - countof(szHeader),
        szFormat,
        pArgs);

	szBuffer[MAX_DEBUG_OUT-1] = '\0'; // Guarantee NULL termination, in worst case

	OutputDebugString(szBuffer);

    va_end(pArgs);

}
