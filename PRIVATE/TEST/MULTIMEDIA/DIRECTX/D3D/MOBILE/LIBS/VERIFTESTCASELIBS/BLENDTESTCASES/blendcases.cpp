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
#include "BlendCases.h"
#include "ImageManagement.h"
#include "BufferTools.h"
#include "BlendTools.h"
#include "DebugOutput.h"
#include "VerifTestCases.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>
#include "DebugOutput.h"


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
    if( FAILED( hr = pDevice->GetDisplayMode( &Mode ) ) )
    {
        DebugOut(DO_ERROR, L"IDirect3DMobile::GetDisplayMode failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    
    //
    // Create an image surface, of the same size to receive the front buffer contents
    //
    if( FAILED( hr = pDevice->CreateImageSurface(Mode.Width,     // UINT Width,
                                                 Mode.Height,    // INT Height,
                                                 Mode.Format,    // D3DMFORMAT Format,
                                                 &pFrontBuffer)))// IDirect3DMobileSurface** ppSurface
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileDevice::CreateImageSurface failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    //
    // Retrieve front buffer copy 
    //
    if( FAILED( hr = pDevice->GetFrontBuffer(pFrontBuffer))) // IDirect3DMobileSurface* pFrontBuffer
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetFrontBuffer failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    //
    // Confirm result
    //
    if (NULL == pFrontBuffer)
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileDevice::GetFrontBuffer resulted in NULL LPDIRECT3DMOBILESURFACE.");
        hr = E_POINTER;
        goto cleanup;
    }

    //
    // Retrieve front-buffer DC
    //
    if (FAILED( hr = pFrontBuffer->GetDC(&hDC))) // HDC* phdc
    {
        DebugOut(DO_ERROR, L"IDirect3DMobileSurface::GetDC failed. (hr = 0x%08x)", hr);
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
                DebugOut(DO_ERROR, L"IDirect3DMobileSurface::ReleaseDC failed.");
            }
        }

        if (0 != pFrontBuffer->Release())
        {
            DebugOut(DO_ERROR, L"IDirect3DMobileSurface::Release != 0.");
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
    HRESULT hr;
	CUSTOMVERTEX *pVertices;

	DWORD dwColorFront, dwColorBack;

	if (FAILED(hr = CreateTestColors(uiIteration,   // UINT uiIteration,
							         &dwColorFront,  // DWORD *pdwColorFront
							         &dwColorBack))) // DWORD *pdwColorBack
	{
		DebugOut(DO_ERROR, L"CreateTestColors failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Prepare a vertex buffer with the required color characteristics
	//
	if( FAILED( hr = (*ppVB)->Lock( 0, 0, (VOID**)&pVertices, 0 ) ) )
	{
	    DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x)", hr);
	    return hr;
	}

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
    HRESULT hr;
	//
	// Render a sequence of nonindexed, geometric primitives of the specified type
	// from the current data input stream
	//
	pDevice->SetRenderState(D3DMRS_SRCBLEND,  D3DMBLEND_ONE);
	pDevice->SetRenderState(D3DMRS_DESTBLEND, D3DMBLEND_ZERO);
	pDevice->SetRenderState(D3DMRS_BLENDOP,   D3DMBLENDOP_ADD);
	if( FAILED( hr = pDevice->DrawPrimitive( D3DMPT_TRIANGLELIST, 0, 2 ) ) )
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed (hr = 0x%08x)", hr);
		return hr;
	}

	pDevice->SetRenderState(D3DMRS_SRCBLEND,  SourceBlend);
	pDevice->SetRenderState(D3DMRS_DESTBLEND, DestBlend);
	pDevice->SetRenderState(D3DMRS_BLENDOP,   BlendOp);
	if( FAILED( hr = pDevice->DrawPrimitive( D3DMPT_TRIANGLELIST, 6, 2 ) ) )
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed (hr = 0x%08x)", hr);
		return hr;
	}

	return S_OK;
}

/**
 * AdjustBlendedColors
 * 
 *     Takes colors returned by GDIs GetPixel and runs them through a reverse 
 *     process of removing the bit replication that GDI performs (which 
 *     provides a more accurate conversion from 5 or 6b to 8b) to conform with 
 *     the D3DM 16bpp and smaller formats.
 * 
 * Parameters
 * 
 * 	  Format
 * 	      The format of the frontbuffer (generally the same as the back buffer 
 * 	      format)
 * 
 * 	  pRed, pGreen, pBlue
 * 	      [in/out]  In: The GDI colors that are to be adjusted. Out: The 
 * 	      colors adjusted to more accurately represent the expected colors.
 *
 *    bRound
 *        Should the values be rounded to the nearest valid color (based on
 *        format) before being truncated? This is only necessary for the
 *        values that have been calculated. The D3DM driver should already
 *        have rounded the color value.
 * 
 * Return Value : HRESULT
 *     Assorted errors on failure, S_OK on success.
 * 
 */
HRESULT AdjustBlendedColors(D3DMFORMAT Format, BYTE * pRed, BYTE * pGreen, BYTE * pBlue, bool bRound)
{
    DWORD dwRedMask;
    DWORD dwGreenMask;
    DWORD dwBlueMask;
    UINT uiRedBitCount;
    UINT uiGreenBitCount;
    UINT uiBlueBitCount;
    BYTE RedRounder;
    BYTE GreenRounder;
    BYTE BlueRounder;

    if (NULL == pRed || NULL == pBlue || NULL == pGreen)
    {
        DebugOut(DO_ERROR, L"AdjustBlendedColors called with NULL colors pointer.");
        return E_POINTER;
    }
    
    BYTE Red = *pRed;
    BYTE Green = *pGreen;
    BYTE Blue = *pBlue;

    switch(Format)
    {
    // Handle the 8bit per channel formats by just returning without changing anything.
    case D3DMFMT_R8G8B8: // Fall through
    case D3DMFMT_A8R8G8B8: // Fall through
    case D3DMFMT_X8R8G8B8:
        return S_OK;

    // Handle the 565 case formats
    case D3DMFMT_R5G6B5:
        uiRedBitCount = 5;
        uiGreenBitCount = 6;
        uiBlueBitCount = 5;
        break;

    // Handle the 555 case formats
    case D3DMFMT_X1R5G5B5: // Fall through
    case D3DMFMT_A1R5G5B5:
        uiRedBitCount = 5;
        uiGreenBitCount = 5;
        uiBlueBitCount = 5;
        break;

    // Handle the 444 case formats
    case D3DMFMT_A4R4G4B4: // Fall through
    case D3DMFMT_X4R4G4B4:
        uiRedBitCount = 4;
        uiGreenBitCount = 4;
        uiBlueBitCount = 4;
        break;

    // Handle the 332 case formats
    case D3DMFMT_R3G3B2: // Fall through
    case D3DMFMT_A8R3G3B2:
        uiRedBitCount = 3;
        uiGreenBitCount = 3;
        uiBlueBitCount = 2;
        break;

    // Shouldn't get here
    default:
        DebugOut(DO_ERROR, L"Cannot handle format %08x", Format);
        return E_INVALIDARG;
    }
    dwRedMask = ((1 << uiRedBitCount) - 1) << (8 - uiRedBitCount);
    dwGreenMask = ((1 << uiGreenBitCount) - 1) << (8 - uiGreenBitCount);
    dwBlueMask = ((1 << uiBlueBitCount) - 1) << (8 - uiBlueBitCount);
    if (bRound)
    {
        RedRounder = 1 << (7 - uiRedBitCount);
        GreenRounder = 1 << (7 - uiGreenBitCount);
        BlueRounder = 1 << (7 - uiBlueBitCount);

        // We need to guard against overflow
        if ((BYTE)(Red + RedRounder) > Red)
            Red += RedRounder;
        if ((BYTE)(Green + GreenRounder) > Green)
            Green += GreenRounder;
        if ((BYTE)(Blue + BlueRounder) > Blue)
            Blue += BlueRounder;
    }
    
    Red &= dwRedMask;
    Green &= dwGreenMask;
    Blue &= dwBlueMask;
    
    *pRed = Red;
    *pGreen = Green;
    *pBlue = Blue;
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
    D3DMFORMAT Format,
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
    HRESULT hr;
	DWORD dwColorResult = 0;
	DWORD dwColorFront;
	DWORD dwColorBack;
	BYTE ExpectedRed;
	BYTE ExpectedGreen;
	BYTE ExpectedBlue;
	UINT uiRedDiff, uiGreenDiff, uiBlueDiff;

	//
	// Determine colors for this iteration
	//
	if (FAILED(hr = CreateTestColors(uiIteration,  // UINT uiIteration,
							         &dwColorFront, // DWORD *pdwColorFront
							         &dwColorBack)))// DWORD *pdwColorBack
	{
		DebugOut(DO_ERROR, L"CreateTestColors failed. (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Blend source and dest
	// 
	if (FAILED(hr = Blend(BlendOp, SourceBlend, DestBlend, 
					      dwColorFront, dwColorBack, &dwColorResult)))
	{
		DebugOut(DO_ERROR, L"Blend failed. (hr = 0x%08x)", hr);
		return hr;
	}

	ExpectedRed = D3D_GETR(dwColorResult);
	ExpectedGreen = D3D_GETG(dwColorResult);
	ExpectedBlue = D3D_GETB(dwColorResult);

	if (FAILED(hr = AdjustBlendedColors(Format, &Red, &Green, &Blue, false)))
	{
	    DebugOut(DO_ERROR, L"Could not adjust colors based on format of frontbuffer. (hr = 0x%08x)", hr);
	    return hr;
	}

	if (FAILED(hr = AdjustBlendedColors(Format, &ExpectedRed, &ExpectedGreen, &ExpectedBlue, true)))
	{
	    DebugOut(DO_ERROR, L"Could not adjust colors based on format of frontbuffer. (hr = 0x%08x)", hr);
	    return hr;
	}

	//
	// Compute absolute diffs
	//
	uiRedDiff   = (UINT)abs((int)(ExpectedRed)-(int)(Red));
	uiGreenDiff = (UINT)abs((int)(ExpectedGreen)-(int)(Green));
	uiBlueDiff  = (UINT)abs((int)(ExpectedBlue)-(int)(Blue));

	//
	// Verify that result was close to expectations
	//
	if ((uiRedDiff   > uiTolerance) ||
	    (uiGreenDiff > uiTolerance) ||
	    (uiBlueDiff  > uiTolerance))
	{
	    DebugOut(DO_ERROR, L"Blending iteration failed verification: Iteration %u exceeded per-channel tolerance of %u", uiIteration, uiTolerance);
		DebugOut(L"Expected:  [R=%u;G=%u;B=%u]", (UINT)(ExpectedRed), (UINT)(ExpectedGreen), (UINT)(ExpectedBlue));
		DebugOut(L"  Actual:  [R=%u;G=%u;B=%u]", (UINT)Red, (UINT)(Green), (UINT)(Blue));
		DebugOut(L"   Diffs:  [R=%u;G=%u;B=%u]", uiRedDiff, uiGreenDiff, uiBlueDiff);
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
	DebugOut(L"Beginning ExecuteTestCase.");

    HRESULT hr;

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
		DebugOut(DO_ERROR, L"Blend not supported. Skipping.");
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
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. Aborting.");
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
	if( FAILED( hr = pTestCaseArgs->pDevice->GetViewport(&OldViewport) ) )
	{
		DebugOut(DO_ERROR, L"GetViewport failed. (hr = 0x%08x) Aborting.", hr);
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
	if( FAILED( hr = pTestCaseArgs->pDevice->SetViewport(&NewViewport) ) )
	{
		DebugOut(DO_ERROR, L"SetViewport failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Indicate vertex format
	//
	if( FAILED( hr = pTestCaseArgs->pDevice->SetStreamSource( 0, pVB, 0) ) )
	{
		DebugOut(DO_ERROR, L"SetFVF failed. (hr = 0x%08x) Aborting.", hr);
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
		DebugOut(DO_ERROR, L"GetClientRect failed. (hr = 0x%08x) Aborting.", HRESULT_FROM_WIN32(GetLastError()));
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
		DebugOut(DO_ERROR, L"ClientToScreen failed. (hr = 0x%08x) Aborting.", HRESULT_FROM_WIN32(GetLastError()));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Informative debug spew:  sampling location should be at center of client
	// area of window (not center of window's bounding rect)
	// 
	DebugOut(L"GetPixel will sample at (%li,%li)", Point.x, Point.y);


	if( FAILED( hr = pVB->Lock( 0, 0, (VOID**)(&pVertices), 0 ) ) )
	{
		DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x) Aborting.", hr);
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
		if (FAILED(hr = SetupIter(
			uiFrameIndex,  // UINT uiIteration,
			&pVB,
			AlphaTests[dwTableIndex].SourceBlend, // D3DMBLEND SourceBlend
			AlphaTests[dwTableIndex].DestBlend,   // D3DMBLEND DestBlend
			AlphaTests[dwTableIndex].BlendOp)))   // D3DMBLENDOP BlendOp
		{
			DebugOut(DO_ERROR, L"Failed to generate vertex buffer for this iteration. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Scene rendering is about to begin
		//
		if( FAILED( hr = pTestCaseArgs->pDevice->BeginScene() ) )
		{
			DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}


		//
		// Verify resulting pixel component colors
		//
		if (FAILED(hr = DrawIter(pTestCaseArgs->pDevice,               // LPDIRECT3DMOBILEDEVICE pDevice
		                         dwTableIndex,                         // UINT dwTableIndex
		                         uiFrameIndex,                         // UINT uiIteration,
		                         AlphaTests[dwTableIndex].SourceBlend, // D3DMBLEND SourceBlend
		                         AlphaTests[dwTableIndex].DestBlend,   // D3DMBLEND DestBlend
		                         AlphaTests[dwTableIndex].BlendOp)))   // D3DMBLENDOP BlendOp
		{
			DebugOut(DO_ERROR, L"DrawIter failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// End the scene
		//
		if( FAILED( hr = pTestCaseArgs->pDevice->EndScene() ) )
		{
			DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		if( FAILED( hr = pTestCaseArgs->pDevice->Present( NULL, NULL, NULL, NULL ) ) )
		{
			DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Retrieve captured pixel components
		//
		if (FAILED(hr = GetPixel(pTestCaseArgs->pDevice,
		                         Point.x,                       // int iXPos,
		                         Point.y,                       // int iYPos,
		                         &RedResult,                    // BYTE *pRed,
		                         &GreenResult,                  // BYTE *pGreen,
		                         &BlueResult)))                 // BYTE *pBlue
		{
			DebugOut(DO_ERROR, L"GetPixel failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		//
		// Verify resulting pixel component colors
		//
		if (FAILED(hr = VerifyIter(
		                            pTestCaseArgs->pParms->BackBufferFormat,
									uiFrameIndex,                    // UINT uiIteration,
									D3DMQA_BLEND_TOLERANCE,          // UINT uiTolerance
									RedResult,                       // BYTE Red,
									GreenResult,                     // BYTE Green,
									BlueResult,                      // BYTE Blue
									AlphaTests[dwTableIndex].SourceBlend, // D3DMBLEND SourceBlend
									AlphaTests[dwTableIndex].DestBlend,   // D3DMBLEND DestBlend
									AlphaTests[dwTableIndex].BlendOp)))   // D3DMBLENDOP BlendOp
		{
			DebugOut(DO_ERROR, L"VerifyIter failed. (hr = 0x%08x) Failing.", hr);
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

