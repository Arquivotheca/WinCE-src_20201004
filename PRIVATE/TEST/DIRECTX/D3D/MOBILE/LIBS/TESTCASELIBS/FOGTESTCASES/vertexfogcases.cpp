//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "VertexFogCases.h"
#include "Geometry.h"
#include "BufferTools.h"
#include "QAD3DMX.h"

#define _M(_a) D3DM_MAKE_D3DMVALUE(_a)

//
// SupportsVertexFogTableIndex
//
//   Vertex fog, depending type and underlying driver, may not be supported.
//   D3DMPRASTERCAPS_FOGVERTEX is required for any kind of vertex fog, and
//   range-based vertex fog has the additional requirement of D3DMPRASTERCAPS_FOGRANGE.
//
//   Depth-based vertex fog is supported on any driver that exposes
//   D3DMPRASTERCAPS_FOGVERTEX; there is no additional D3DMCAPS requirement.
//
//   Function prototype must conform to SUPPORTS_TABLE_INDEX.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   DWORD dwTableIndex:  Index into table of vertex fog test cases
//
// Return Value:
//
//   S_OK if test case is supported on this device; E_FAIL otherwise.
//
HRESULT SupportsVertexFogTableIndex(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex)
{
	//
	// Function result
	//
	HRESULT hr = S_OK;
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(L"GetDeviceCaps failed.\n");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Is vertex fog supported?
	//
	if (!(Caps.RasterCaps & D3DMPRASTERCAPS_FOGVERTEX))
	{
		OutputDebugString(L"D3DMPRASTERCAPS_FOGVERTEX required for this test case; skipping.\n");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Support for range-based fog required?
	//
	if (D3DQAFOG_RANGE == VertexFogCases[dwTableIndex].FogType)
	{
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_FOGRANGE))
		{
			OutputDebugString(L"D3DMPRASTERCAPS_FOGRANGE required for this test case; skipping.\n");
			hr = E_FAIL;
			goto cleanup;
		}
	}

	//
	// If EXP fog is required, does the driver support it?
	//
	if (D3DMFOG_EXP == VertexFogCases[dwTableIndex].FogMode)
	{
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_EXPFOG))
		{
			OutputDebugString(L"D3DMPRASTERCAPS_EXPFOG required for this test case; skipping.\n");
			hr = E_FAIL;
			goto cleanup;
		}
	}

	//
	// If EXP2 fog is required, does the driver support it?
	//
	if (D3DMFOG_EXP2 == VertexFogCases[dwTableIndex].FogMode)
	{
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_EXP2FOG))
		{
			OutputDebugString(L"D3DMPRASTERCAPS_EXP2FOG required for this test case; skipping.\n");
			hr = E_FAIL;
			goto cleanup;
		}
	}

cleanup:
	return hr;

}

//
// SetVertexFogStates
//
//   Prepare render state settings for test case execution.  This function is intended
//   to be called after verifying that the driver is expected to support this test case.
//
//   Thus, any failure here is inconsistent with the driver's reported capability
//   bits.
//
//   Function prototype must conform to SET_STATES.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   DWORD dwTableIndex:  Index into table of vertex fog test cases
//
// Return Value:
//
//   S_OK if state settings succeeded; E_FAIL otherwise.
//
HRESULT SetVertexFogStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex)
{
	//
	// Function result
	//
	HRESULT hr = S_OK;
	
	//
	// Bad parameter check
	//
	if (NULL == pDevice)
	{
		OutputDebugString(L"Aborting due to NULL LPDIRECT3DMOBILEDEVICE.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Bad parameter check
	//
	if (dwTableIndex >= D3DMQA_VERTEXFOGTEST_COUNT)
	{
		OutputDebugString(L"Aborting due to invalid table index.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Set up fogging render states
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_LIGHTING,      FALSE                                               )) || 
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGENABLE,     VertexFogCases[dwTableIndex].FogEnable              )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGCOLOR,      VertexFogCases[dwTableIndex].FogColor               )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGVERTEXMODE, VertexFogCases[dwTableIndex].FogMode                )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGTABLEMODE,  D3DMFOG_NONE                                        )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGSTART,      *(DWORD *)(&VertexFogCases[dwTableIndex].FogStart  ))) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGEND,        *(DWORD *)(&VertexFogCases[dwTableIndex].FogEnd    ))) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGDENSITY,    *(DWORD *)(&VertexFogCases[dwTableIndex].FogDensity))))
	{
		OutputDebugString(L"SetRenderState failed for fog settings.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Support for range-based fog required?
	//
	if (D3DQAFOG_RANGE == VertexFogCases[dwTableIndex].FogType)
	{
		if (FAILED(pDevice->SetRenderState(D3DMRS_RANGEFOGENABLE,  TRUE)))
		{
			OutputDebugString(L"SetRenderState failed for D3DMRS_RANGEFOGENABLE(TRUE) setting.");
			hr = E_FAIL;
			goto cleanup;
		}
	}
	else
	{
		if (FAILED(pDevice->SetRenderState(D3DMRS_RANGEFOGENABLE,  FALSE)))
		{
			OutputDebugString(L"SetRenderState failed for D3DMRS_RANGEFOGENABLE(FALSE) setting.");
			hr = E_FAIL;
			goto cleanup;
		}
	}


cleanup:

	return hr;
}



//
// SetupVertexFogGeometry
//  
//   Prepare geometry, in the form of a vertex buffer, for test execution.
//   
//   Function prototype must conform to SETUP_GEOMETRY.
//   
// Arguments: 
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   HWND hWnd:  Target window
//   LPDIRECT3DMOBILEVERTEXBUFFER *ppVB:  Resultant vertex buffer
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT SetupVertexFogGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                               HWND hWnd,
                               LPDIRECT3DMOBILEVERTEXBUFFER *ppVB)

{

	//
	// Pointer for generated vertex data
	// 
	D3DMFOGVERTTEST_VERTS * pVertices = NULL;

	//
	// Geometry generation characteristics
	//
	CONST UINT uiStepCountX = D3DQA_RECTS_X;
	CONST UINT uiStepCountY = D3DQA_RECTS_Y;
	CONST UINT uiStepCountZ = D3DQA_NUMQUADS;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Iterated geometry generation variables
	//
	UINT uiCurrentVert;
	D3DMCOLOR dwColor;
	D3DMMATRIX ProjMatrix;

	//
	// Bad-parameter check
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		OutputDebugString(L"malloc failed");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Projection matrix specifications; if updating, also update fog test cases (e.g., near/far settings)
	//
	CONST float fFov = (D3DQA_PI / 4.0f);
	CONST float fAspect = 1.0f;
	CONST float fNear = 1.0f;
	CONST float fFar = 11.0f;

	//
	// Steppers/boundaries for geometry generation
	//
	float  fStartX, fStartY, fStartZ;
	float   fStepX,  fStepY,  fStepZ;
	float       fX,      fY,      fZ;
	float fFrustumHeight, fFrustumWidth;
	UINT  uiStepX, uiStepY, uiStepZ;

	//
	// Custom fog computed here; then stored in vertex
	//
	D3DMCOLOR dwSpecColor;

	//
	// Generate projection matrix
	//
	// Retval is just a pointer to ProjMatrix; no need to examine it
	//
	(void)D3DMatrixPerspectiveFovLH( &ProjMatrix, // D3DMATRIX* pOut,
	                                  fFov,       // FLOAT fovy,
	                                  fAspect,    // FLOAT Aspect,
	                                  fNear,      // FLOAT zn,
	                                  fFar );     // FLOAT zf
	                        

	//
	// Set the projection matrix for transformation
	//
	if( FAILED( pDevice->SetTransform( D3DMTS_PROJECTION, &ProjMatrix, D3DMFMT_D3DMVALUE_FLOAT)))
	{
		OutputDebugString(L"SetTransform failed");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Allocate memory for vertices
	//
	pVertices = (D3DMFOGVERTTEST_VERTS *)malloc(sizeof(D3DMFOGVERTTEST_VERTS) * D3DQA_NUMVERTS);
	if (NULL == pVertices)
	{
		OutputDebugString(L"malloc failed");
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// Initial Z; and step size
	//
	fStepZ = (fFar - fNear) / (float)uiStepCountZ;
	fStartZ = fNear;

	//
	// Initialize vertex buffer index
	//
	uiCurrentVert = 0;

	//
	// Iterate through Z steps
	//
	for (uiStepZ = 0; uiStepZ < uiStepCountZ; uiStepZ++)
	{
		//
		// Compute all primitive data for this z step
		//

		fZ = fStartZ + uiStepZ * fStepZ;

		fFrustumHeight = 
		fFrustumWidth = 2 * fZ * (float)tan(fFov/2.0f);

		uiStepX = uiStepZ % uiStepCountX;
		uiStepY = uiStepZ / uiStepCountY;

		fStartX = (-fFrustumWidth / 2.0f);
		fStartY = (-fFrustumHeight / 2.0f);

		fStepX = (fFrustumWidth / uiStepCountX);
		fStepY = (fFrustumHeight / uiStepCountY);

		fX = fStartX + uiStepX * fStepX;
		fY = fStartY + uiStepY * fStepY;

		//
		// Checkered color pattern
		//
		if ((uiStepY % 2))
		{
			if ((uiStepX % 2))
				dwColor = D3DMCOLOR_XRGB(0,255,0);
			else
				dwColor = D3DMCOLOR_XRGB(255,0,0);
		}
		else
		{
			if ((uiStepX % 2))
				dwColor = D3DMCOLOR_XRGB(255,0,0);
			else
				dwColor = D3DMCOLOR_XRGB(0,255,0);
		}

		//
		// Vertical strips of custom fog (only a factor when
		// fog is enabled, but both vertex and table mode are D3DMFOG_NONE)
		// 
		switch(uiStepX % 5)
		{
		case 0:
			dwSpecColor = D3DMCOLOR_ARGB(  0,0,0,0);
			break;
		case 1:
			dwSpecColor = D3DMCOLOR_ARGB( 64,0,0,0);
			break;
		case 2:
			dwSpecColor = D3DMCOLOR_ARGB(128,0,0,0);
			break;
		case 3:
			dwSpecColor = D3DMCOLOR_ARGB(192,0,0,0);
			break;
		case 4:
		default:
			dwSpecColor = D3DMCOLOR_ARGB(255,0,0,0);
			break;
		}

		pVertices[uiCurrentVert].Diffuse = dwColor;
		pVertices[uiCurrentVert].Specular = dwSpecColor;
		pVertices[uiCurrentVert].x = fX;
		pVertices[uiCurrentVert].y = fY + fStepY;
		pVertices[uiCurrentVert].z = fZ;
		uiCurrentVert++;

		pVertices[uiCurrentVert].Diffuse = dwColor;
		pVertices[uiCurrentVert].Specular = dwSpecColor;
		pVertices[uiCurrentVert].x = fX + fStepX;
		pVertices[uiCurrentVert].y = fY + fStepY;
		pVertices[uiCurrentVert].z = fZ;
		uiCurrentVert++;

		pVertices[uiCurrentVert].Diffuse = dwColor;
		pVertices[uiCurrentVert].Specular = dwSpecColor;
		pVertices[uiCurrentVert].x = fX + fStepX;
		pVertices[uiCurrentVert].y = fY;
		pVertices[uiCurrentVert].z = fZ;
		uiCurrentVert++;

		pVertices[uiCurrentVert].Diffuse = dwColor;
		pVertices[uiCurrentVert].Specular = dwSpecColor;
		pVertices[uiCurrentVert].x = fX;
		pVertices[uiCurrentVert].y = fY + fStepY;
		pVertices[uiCurrentVert].z = fZ;
		uiCurrentVert++;

		pVertices[uiCurrentVert].Diffuse = dwColor;
		pVertices[uiCurrentVert].Specular = dwSpecColor;
		pVertices[uiCurrentVert].x = fX + fStepX;
		pVertices[uiCurrentVert].y = fY;
		pVertices[uiCurrentVert].z = fZ;
		uiCurrentVert++;

		pVertices[uiCurrentVert].Diffuse = dwColor;
		pVertices[uiCurrentVert].Specular = dwSpecColor;
		pVertices[uiCurrentVert].x = fX;
		pVertices[uiCurrentVert].y = fY;
		pVertices[uiCurrentVert].z = fZ;
		uiCurrentVert++;

	}

	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(CreateFilledVertexBuffer( pDevice,                       // LPDIRECT3DMOBILEDEVICE pDevice,
	                                     ppVB,                          // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                     (BYTE*)pVertices,              // BYTE *pVertices,
	                                     sizeof(D3DMFOGVERTTEST_VERTS), // UINT uiVertexSize,
	                                     D3DQA_NUMVERTS,                // UINT uiNumVertices,
	                                     D3DMFOGVERTTEST_FVF)))         // DWORD dwFVF
	{
		OutputDebugString(L"CreateFilledVertexBuffer failed.");
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	if (pVertices)
		free(pVertices);

	if ((FAILED(hr)) && (NULL != ppVB) && (NULL != (*ppVB)))
		(*ppVB)->Release();

	return hr;	
}

