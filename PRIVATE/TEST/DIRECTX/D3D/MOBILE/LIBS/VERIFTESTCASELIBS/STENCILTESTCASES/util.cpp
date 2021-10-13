//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>
#include <d3dm.h>
#include "DepthStencilTools.h"
#include "DebugOutput.h"
#include "util.h"
#include "BufferTools.h"


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
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		Result = FALSE;
		goto cleanup;
	}

	if (FAILED(pDevice->GetDirect3D(&pD3D)))
	{
		OutputDebugString(_T("GetDirect3D failed."));
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
		OutputDebugString(_T("No support for D3DMPRASTERCAPS_STENCIL."));
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
		OutputDebugString(_T("No support for this D/S format."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_KEEP."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_ZERO."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_REPLACE."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_INCRSAT."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_DECRSAT."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_INVERT."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_INCR."));
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
		OutputDebugString(_T("Test requires support for D3DMSTENCILCAPS_DECR."));
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
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_NEVER."));
		Result = FALSE;
		goto cleanup;
	}


	//
	// If LESS comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_LESS)) && 
		(D3DMCMP_LESS == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_LESS."));
		Result = FALSE;
		goto cleanup;
	}

	//
	// If EQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_EQUAL)) && 
		(D3DMCMP_EQUAL == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_EQUAL."));
		Result = FALSE;
		goto cleanup;
	}

	//
	// If LESSEQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_LESSEQUAL)) && 
		(D3DMCMP_LESSEQUAL == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_LESSEQUAL."));
		Result = FALSE;
		goto cleanup;
	}

	//
	// If GREATER comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_GREATER)) && 
		(D3DMCMP_GREATER == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_GREATER."));
		Result = FALSE;
		goto cleanup;
	}

	//
	// If NOTEQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_NOTEQUAL)) && 
		(D3DMCMP_NOTEQUAL == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_NOTEQUAL."));
		Result = FALSE;
		goto cleanup;
	}

	//
	// If GREATEREQUAL comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_GREATEREQUAL)) && 
		(D3DMCMP_GREATEREQUAL == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_GREATEREQUAL."));
		Result = FALSE;
		goto cleanup;
	}

	//
	// If ALWAYS comparitor is used, confirm capability
	// 
	if ((!(Caps.StencilCmpCaps & D3DMPCMPCAPS_ALWAYS)) && 
		(D3DMCMP_ALWAYS == StencilFunc))
	{
		OutputDebugString(_T("Test requires support for D3DMPCMPCAPS_ALWAYS."));
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
	HRESULT Result = S_OK;

	//
	// Viewport extents
	//
	D3DMVIEWPORT d3dViewport;

	//
	// Parameter validation
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		OutputDebugString(_T("Invalid parameter(s)."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve viewport extents
	//
	if( FAILED( pDevice->GetViewport( &d3dViewport ) ) )
	{
		OutputDebugString(_T("GetViewport failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Create a Vertex Buffer; set stream source and vertex shader type (FVF)
	//
	(*ppVB) = CreateActiveBuffer(pDevice, D3DQA_STENCILTEST_NUMVERTS, D3DQA_STENCILTEST_FVF, sizeof(D3DQA_STENCILTEST), 0);
	if (NULL == (*ppVB))
	{
		OutputDebugString(_T("CreateActiveBuffer failed."));
		Result = E_FAIL;
		goto cleanup;
	}

	//
	// Set up input vertices (lock, copy data into buffer, unlock)
	//
	if( FAILED( (*ppVB)->Lock( 0, D3DQA_STENCILTEST_NUMVERTS * sizeof(D3DQA_STENCILTEST), (VOID**)&pVerts, 0 ) ) )
	{
		OutputDebugString(_T("Failure while attempting to lock a vertex buffer."));
		Result = E_FAIL;
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

	if( FAILED( (*ppVB)->Unlock() ) )
	{
		OutputDebugString(_T("Failure while attempting to unlock a vertex buffer."));
		Result = E_FAIL;
		goto cleanup;
	}

cleanup:

	if (FAILED(Result))
		(*ppVB)->Release();

	return Result;
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
	//
	// Window client extents
	//
	RECT WndRect;

	//
	// Validate arguments
	//
	if (NULL == pPoint)
	{
		DebugOut(_T("Bad argument passed to GetScreenSpaceClientCenter"));
		return ERROR_BAD_ARGUMENTS;
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
		DebugOut(_T("GetClientRect failed"));
		return E_FAIL;
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
		DebugOut(_T("ClientToScreen failed"));
		return E_FAIL;
	}

	//
	// Informative debug spew:  sampling location should be at center of client
	// area of window (not center of window's bounding rect)
	// 
	DebugOut(_T("Client center in screen-space coordinates: (%li,%li)"), pPoint->x, pPoint->y);

	return S_OK;

}