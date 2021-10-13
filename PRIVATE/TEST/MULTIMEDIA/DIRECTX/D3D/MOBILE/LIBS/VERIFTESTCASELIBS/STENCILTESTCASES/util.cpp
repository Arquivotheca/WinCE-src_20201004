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
#include <tchar.h>
#include <d3dm.h>
#include "DepthStencilTools.h"
#include "DebugOutput.h"
#include "util.h"
#include "BufferTools.h"
#include "DebugOutput.h"


//
// VerifyTestCaseSupport
//
//   Verifies that device is capable of executing a particular test case permutation.
//   Requirements:
//        
//      (1) Verify stenciling support  
//      (2) Verify RT/DepthStencil match  
//      (3) Verify operation support  
//      (4) Verify comparitor support  
//   
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   D3DMFORMAT Format:  Proposed D/S format
//   D3DMSTENCILOP StencilZFail:  Operation to execute upon z fail
//   D3DMSTENCILOP StencilFail:  Operation to execute upon stencil cmp fail
//   D3DMSTENCILOP StencilPass:  Operation to execute upon stencil cmp pass
//   D3DMCMPFUNC StencilFunc:  Stencil comparitor
//   
//   STENCILCMPVALUES *pTestCase:  Test case permutation
//    
// Return Value:
//    
//   BOOL:  TRUE if underlying device supports this test case, FALSE otherwise
//
BOOL VerifyTestCaseSupport(LPDIRECT3DMOBILEDEVICE pDevice,
                           D3DMFORMAT Format,
                           D3DMSTENCILOP StencilZFail,
                           D3DMSTENCILOP StencilFail,
                           D3DMSTENCILOP StencilPass,
                           D3DMCMPFUNC StencilFunc)
{
    HRESULT hr;
    
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Function result
	//
	BOOL Result = TRUE;

	//
	// Direct3D Object
	//
	LPDIRECT3DMOBILE pD3D = NULL;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		Result = FALSE;
		goto cleanup;
	}

	if (FAILED(hr = pDevice->GetDirect3D(&pD3D)))
	{
		DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x)", hr);
		Result = FALSE;
		goto cleanup;
	}

	//
	// (1) Verify stenciling support
	//

	//
	// Verify support for stenciling
	//
	if (!(Caps.RasterCaps & D3DMPRASTERCAPS_STENCIL))
	{
		DebugOut(DO_ERROR, L"No support for D3DMPRASTERCAPS_STENCIL.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// (2) Verify RT/DepthStencil match
	//

	//
	// Verify validity of render target and depth stencil combo
	//
	if (!(IsValidDepthStencilFormat(pD3D, pDevice, Format)))
	{
		DebugOut(DO_ERROR, L"No support for this D/S format: 0x%08x", (DWORD)Format);
		Result = FALSE;
		goto cleanup;
	}

	//
	// (3) Verify operation support
	//

	//
	// If KEEP operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_KEEP)) && 
		((D3DMSTENCILOP_KEEP == StencilZFail) ||
		 (D3DMSTENCILOP_KEEP == StencilFail) ||
		 (D3DMSTENCILOP_KEEP == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_KEEP.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If ZERO operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_ZERO)) && 
		((D3DMSTENCILOP_ZERO == StencilZFail) ||
		 (D3DMSTENCILOP_ZERO == StencilFail) ||
		 (D3DMSTENCILOP_ZERO == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_ZERO.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If REPLACE operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_REPLACE)) && 
		((D3DMSTENCILOP_REPLACE == StencilZFail) ||
		 (D3DMSTENCILOP_REPLACE == StencilFail) ||
		 (D3DMSTENCILOP_REPLACE == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_REPLACE.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If INCRSAT operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_INCRSAT)) && 
		((D3DMSTENCILOP_INCRSAT == StencilZFail) ||
		 (D3DMSTENCILOP_INCRSAT == StencilFail) ||
		 (D3DMSTENCILOP_INCRSAT == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_INCRSAT.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If DECRSAT operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_DECRSAT)) && 
		((D3DMSTENCILOP_DECRSAT == StencilZFail) ||
		 (D3DMSTENCILOP_DECRSAT == StencilFail) ||
		 (D3DMSTENCILOP_DECRSAT == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_DECRSAT.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If INVERT operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_INVERT)) && 
		((D3DMSTENCILOP_INVERT == StencilZFail) ||
		 (D3DMSTENCILOP_INVERT == StencilFail) ||
		 (D3DMSTENCILOP_INVERT == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_INVERT.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If INCR operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_INCR)) && 
		((D3DMSTENCILOP_INCR == StencilZFail) ||
		 (D3DMSTENCILOP_INCR == StencilFail) ||
		 (D3DMSTENCILOP_INCR == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_INCR.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If DECR operation is used for any condition, confirm capability
	// 
	if ((!(Caps.StencilCaps & D3DMSTENCILCAPS_DECR)) && 
		((D3DMSTENCILOP_DECR == StencilZFail) ||
		 (D3DMSTENCILOP_DECR == StencilFail) ||
		 (D3DMSTENCILOP_DECR == StencilPass)))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMSTENCILCAPS_DECR.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// (4) Verify comparitor support
	//

	//
	// If NEVER comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_NEVER)) && 
		(D3DMCMP_NEVER == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_NEVER.");
		Result = FALSE;
		goto cleanup;
	}


	//
	// If LESS comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_LESS)) && 
		(D3DMCMP_LESS == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_LESS.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If EQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_EQUAL)) && 
		(D3DMCMP_EQUAL == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_EQUAL.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If LESSEQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_LESSEQUAL)) && 
		(D3DMCMP_LESSEQUAL == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_LESSEQUAL.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If GREATER comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_GREATER)) && 
		(D3DMCMP_GREATER == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_GREATER.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If NOTEQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_NOTEQUAL)) && 
		(D3DMCMP_NOTEQUAL == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_NOTEQUAL.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If GREATEREQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_GREATEREQUAL)) && 
		(D3DMCMP_GREATEREQUAL == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_GREATEREQUAL.");
		Result = FALSE;
		goto cleanup;
	}

	//
	// If ALWAYS comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_ALWAYS)) && 
		(D3DMCMP_ALWAYS == StencilFunc))
	{
		DebugOut(DO_ERROR, L"Test requires support for D3DMPCMPCAPS_ALWAYS.");
		Result = FALSE;
		goto cleanup;
	}

cleanup:

	if (pD3D)
		pD3D->Release();

	return Result;
}

//
// MakeStencilGeometry
//
//   Creates a vertex buffer that is useful for stencil testing. 
//   
// Arguments:
//
//   LPDIRECT3DMOBILE pD3D:  Direct3D Object
//   LPDIRECT3DMOBILEVERTEXBUFFER *ppVB:  Resultant vertex buffer
//   D3DMCOLOR dwColor:  Color to specify for diffuse vertex component
//   float fZ:  Plane depth
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT MakeStencilGeometry(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB, D3DMCOLOR dwColor, float fZ)
{
	//
	// Pointer to vertex data
	//
	D3DQA_STENCILTEST *pVerts;

	//
	// Function result
	//
	HRESULT hr = S_OK;

	//
	// Viewport extents
	//
	D3DMVIEWPORT d3dViewport;

	//
	// Parameter validation
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid parameter(s).");
		hr = E_POINTER;
		goto cleanup;
	}

	//
	// Retrieve viewport extents
	//
	if( FAILED( hr = pDevice->GetViewport( &d3dViewport ) ) )
	{
		DebugOut(DO_ERROR, L"GetViewport failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Create a Vertex Buffer; set stream source and vertex shader type (FVF)
	//
	(*ppVB) = CreateActiveBuffer(pDevice, D3DQA_STENCILTEST_NUMVERTS, D3DQA_STENCILTEST_FVF, sizeof(D3DQA_STENCILTEST), 0);
	if (NULL == (*ppVB))
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Set up input vertices (lock, copy data into buffer, unlock)
	//
	if( FAILED( hr = (*ppVB)->Lock( 0, D3DQA_STENCILTEST_NUMVERTS * sizeof(D3DQA_STENCILTEST), (VOID**)&pVerts, 0 ) ) )
	{
		DebugOut(DO_ERROR, L"Failure while attempting to lock a vertex buffer. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	pVerts[0].x = (float)d3dViewport.X;
	pVerts[0].y = (float)d3dViewport.Y;
	pVerts[0].z = fZ;
	pVerts[0].rhw = 1.0f;
	pVerts[0].Diffuse = dwColor;

	pVerts[1].x = (float)d3dViewport.X + (float)d3dViewport.Width;
	pVerts[1].y = (float)d3dViewport.Y;
	pVerts[1].z =  fZ;
	pVerts[1].rhw = 1.0f;
	pVerts[1].Diffuse = dwColor;

	pVerts[2].x = (float)d3dViewport.X;
	pVerts[2].y = (float)d3dViewport.Y + (float)d3dViewport.Height;
	pVerts[2].z =   fZ;
	pVerts[2].rhw = 1.0f;
	pVerts[2].Diffuse = dwColor;

	pVerts[3].x = (float)d3dViewport.X + (float)d3dViewport.Width;
	pVerts[3].y = (float)d3dViewport.Y;
	pVerts[3].z =  fZ;
	pVerts[3].rhw = 1.0f;
	pVerts[3].Diffuse = dwColor;

	pVerts[4].x = (float)d3dViewport.X + (float)d3dViewport.Width;
	pVerts[4].y = (float)d3dViewport.Y + (float)d3dViewport.Height;
	pVerts[4].z =   fZ;
	pVerts[4].rhw = 1.0f;
	pVerts[4].Diffuse = dwColor;

	pVerts[5].x = (float)d3dViewport.X;
	pVerts[5].y = (float)d3dViewport.Y + (float)d3dViewport.Height;
	pVerts[5].z =   fZ;
	pVerts[5].rhw = 1.0f;
	pVerts[5].Diffuse = dwColor;

	if( FAILED( hr = (*ppVB)->Unlock() ) )
	{
		DebugOut(DO_ERROR, L"Failure while attempting to unlock a vertex buffer. (hr = 0x%08x)", hr);
		goto cleanup;
	}

cleanup:

	if (FAILED(hr))
		(*ppVB)->Release();

	return hr;
}


//
// GetScreenSpaceClientCenter
//
//   Retrieves screen-space coordinate (X,Y) for center of client
//   area of specified HWND
//   
// Arguments:
//
//   HWND hWnd:  Target window
//   LPPOINT pPoint:  Resulting coordinate data
//    
// Return Value:
//    
//   HRESULT:  Indicates success or failure
//
HRESULT GetScreenSpaceClientCenter(HWND hWnd, LPPOINT pPoint)
{
    HRESULT hr;
    
	//
	// Window client extents
	//
	RECT WndRect;

	//
	// Validate arguments
	//
	if (NULL == pPoint)
	{
		DebugOut(DO_ERROR, L"Bad argument passed to GetScreenSpaceClientCenter");
		return E_INVALIDARG;
	}

	//
	// This function retrieves the coordinates of a window's client area. 
	// The left and top members are zero. 
	//
	// Zero indicates failure.
	//
	if (0 == GetClientRect( hWnd,
							&WndRect))
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
		DebugOut(DO_ERROR, L"GetClientRect failed (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Sampling location for GetPixel, in center of client extents
	//
	pPoint->x = WndRect.right / 2;
	pPoint->y = WndRect.bottom / 2;

	//
	// This function converts the client coordinates of a specified point to screen coordinates.
	// Zero indicates failure. 
	//
	if (0 == ClientToScreen(hWnd,      // HWND hWnd, 
	                        pPoint))   // LPPOINT lpPoint
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
		DebugOut(DO_ERROR, L"ClientToScreen failed (hr = 0x%08x)", hr);
		return hr;
	}

	//
	// Informative debug spew:  sampling location should be at center of client
	// area of window (not center of window's bounding rect)
	// 
	DebugOut(L"Client center in screen-space coordinates: (%li,%li)", pPoint->x, pPoint->y);

	return S_OK;

}
