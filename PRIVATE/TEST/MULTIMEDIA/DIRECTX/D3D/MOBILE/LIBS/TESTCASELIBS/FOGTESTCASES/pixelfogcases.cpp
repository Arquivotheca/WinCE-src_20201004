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
#include "PixelFogCases.h"
#include "Geometry.h"
#include "BufferTools.h"
#include "DebugOutput.h"

#define _M(_a) D3DM_MAKE_D3DMVALUE(_a)

// 
// GenProjWFog
//
//   Create a projection matrix with the elements necessary for the driver
//   to decompose into WNear and WFar for fog computations.
//
// Arguments:
//
//   D3DMMATRIX *pMatrix:  Resultant matrix
//   float fNear: Near frustum plane 
//   float fFar: Far frustum plane  
//   
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GenProjWFog(D3DMMATRIX *pMatrix, float fNear, float fFar)
{

	//
	// Bad-parameter test
	//
	if (NULL == pMatrix)
		return E_FAIL;

	//
	// Prevent divide-by-zero
	//
	if (0 == (fFar-fNear))
		return E_FAIL;

	pMatrix->_11 = _M(1.0f);
	pMatrix->_12 = _M(0.0f);
	pMatrix->_13 = _M(0.0f);
	pMatrix->_14 = _M(0.0f);

	pMatrix->_21 = _M(0.0f);
	pMatrix->_22 = _M(1.0f);
	pMatrix->_23 = _M(0.0f);
	pMatrix->_24 = _M(0.0f);

	pMatrix->_31 = _M(0.0f);
	pMatrix->_32 = _M(0.0f);
	pMatrix->_33 = _M(fFar/(fFar-fNear));
	pMatrix->_34 = _M(1.0f);

	pMatrix->_41 = _M(0.0f);
	pMatrix->_42 = _M(0.0f);
	pMatrix->_43 = _M(0.0f);
	pMatrix->_44 = _M(fNear);

	return S_OK;
}


//
// SupportsPixelFogTableIndex
//
//   Pixel fog, depending type and underlying driver, may not be supported.
//   D3DMPRASTERCAPS_FOGTABLE is required for any kind of pixel fog.
//
//   Additionally, each fog type requires a specific capability:
//
//      (1) Z-Based-Fog: D3DMPRASTERCAPS_ZFOG
//      (2) W-Based-Fog: D3DMPRASTERCAPS_WFOG
//   
//   Function prototype must conform to SUPPORTS_TABLE_INDEX.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//   DWORD dwTableIndex:  Index into table of pixel fog test cases
//
// Return Value:
//
//   S_OK if test case is supported on this device; E_FAIL otherwise.
//
HRESULT SupportsPixelFogTableIndex(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex)
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
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Is pixel fog supported?
	//
	if (!(Caps.RasterCaps & D3DMPRASTERCAPS_FOGTABLE))
	{
		DebugOut(DO_ERROR, L"D3DMPRASTERCAPS_FOGTABLE required for this test case; skipping.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Is this W or Z fog?
	//
	switch(PixelFogCases[dwTableIndex].FogType)
	{
	case (D3DQAFOG_W):
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_WFOG))
		{
			DebugOut(DO_ERROR, L"D3DMPRASTERCAPS_WFOG required for this test case; skipping.");
			hr = E_FAIL;
			goto cleanup;
		}

		break;

	case (D3DQAFOG_Z):
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_ZFOG))
		{
			DebugOut(DO_ERROR, L"D3DMPRASTERCAPS_ZFOG required for this test case; skipping.");
			hr = E_FAIL;
			goto cleanup;
		}
		break;

	default:
		DebugOut(DO_ERROR, L"Unknown fog type specified for this test case.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// If EXP fog is required, does the driver support it?
	//
	if (D3DMFOG_EXP == PixelFogCases[dwTableIndex].FogMode)
	{
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_EXPFOG))
		{
			DebugOut(DO_ERROR, L"D3DMPRASTERCAPS_EXPFOG required for this test case; skipping.");
			hr = E_FAIL;
			goto cleanup;
		}
	}

	//
	// If EXP2 fog is required, does the driver support it?
	//
	if (D3DMFOG_EXP2 == PixelFogCases[dwTableIndex].FogMode)
	{
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_EXP2FOG))
		{
			DebugOut(DO_ERROR, L"D3DMPRASTERCAPS_EXP2FOG required for this test case; skipping.");
			hr = E_FAIL;
			goto cleanup;
		}
	}


cleanup:
	return hr;

}

//
// SetPixelFogStates
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
HRESULT SetPixelFogStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex)
{
	//
	// Function result
	//
	HRESULT hr = S_OK;

	D3DMMATRIX Proj = {  _M(1.0f), _M(0.0f), _M(0.0f), _M(0.0f), 
	                     _M(0.0f), _M(1.0f), _M(0.0f), _M(0.0f), 
	                     _M(0.0f), _M(0.0f), _M(1.0f), _M(0.0f), 
	                     _M(0.0f), _M(0.0f), _M(0.0f), _M(1.0f)};

	
	//
	// Bad parameter check
	//
	if (NULL == pDevice)
	{
		DebugOut(DO_ERROR, L"Aborting due to NULL LPDIRECT3DMOBILEDEVICE.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Bad parameter check
	//
	if (dwTableIndex >= D3DMQA_PIXELFOGTEST_COUNT)
	{
		DebugOut(DO_ERROR, L"Aborting due to invalid table index.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Is this W or Z fog?
	//
	switch(PixelFogCases[dwTableIndex].FogType)
	{
	case (D3DQAFOG_W):
		//
		// Prepare a projection matrix that corresponds to a particular
		// dvWNear, dvWFar
		//
		if (FAILED(hr = GenProjWFog(&Proj, PixelFogCases[dwTableIndex].fWNear, PixelFogCases[dwTableIndex].fWFar)))
		{
			DebugOut(DO_ERROR, L"GenProjWFog failed. (hr = 0x%08x)", hr);
			goto cleanup;
		}
		break;

	case (D3DQAFOG_Z):

		//
		// Identity matrix is fine
		//

		break;

	default:
		DebugOut(DO_ERROR, L"Unknown fog type specified for this test case.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Set projection matrix
	//
	if (FAILED(hr = pDevice->SetTransform(D3DMTS_PROJECTION, &Proj, D3DMFMT_D3DMVALUE_FLOAT)))
	{
		DebugOut(DO_ERROR, L"SetTransform(D3DMTS_PROJECTION,) failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Set up fogging render states
	//
	if (FAILED(pDevice->SetRenderState(D3DMRS_FOGENABLE,    PixelFogCases[dwTableIndex].FogEnable               )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGCOLOR,     PixelFogCases[dwTableIndex].FogColor                )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGTABLEMODE, PixelFogCases[dwTableIndex].FogMode                 )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGVERTEXMODE,D3DMFOG_NONE                                        )) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGSTART,     *(DWORD *)(&PixelFogCases[dwTableIndex].FogStart   ))) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGEND,       *(DWORD *)(&PixelFogCases[dwTableIndex].FogEnd     ))) ||
	    FAILED(pDevice->SetRenderState(D3DMRS_FOGDENSITY,   *(DWORD *)(&PixelFogCases[dwTableIndex].FogDensity ))))
	{
		DebugOut(DO_ERROR, L"SetRenderState failed for fog settings.");
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	return hr;
}

//
// SetupPixelFogGeometry
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
HRESULT SetupPixelFogGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                              HWND hWnd,
                              LPDIRECT3DMOBILEVERTEXBUFFER *ppVB)

{

	//
	// Pointer for generated vertex data
	// 
	D3DMFOGTEST_VERTS * pVertices = NULL;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Iterated geometry generation variables
	//
	UINT uiStepX, uiStepY;
	UINT uiCurrentVert;
	FLOAT fRectWidth, fRectHeight;
	FLOAT fStepSizeZ;
	FLOAT fRectZ;
	D3DMCOLOR dwColor;
	D3DMCOLOR dwSpecColor;
	float fBaseX, fBaseY;

	//
	// Window client extents
	//
	RECT ClientExtents;

	//
	// Bad-parameter check
	//
	if ((NULL == ppVB) || (NULL == pDevice))
	{
		DebugOut(DO_ERROR, L"malloc failed");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (0 == GetClientRect( hWnd,           // HWND hWnd, 
	                       &ClientExtents)) // LPRECT lpRect 
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
		DebugOut(DO_ERROR, L"GetClientRect failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Scale factors
	//
	fRectWidth = (float)ClientExtents.right / (float)D3DQA_RECTS_X;
	fRectHeight = (float)ClientExtents.bottom / (float)D3DQA_RECTS_Y;

	//
	// Allocate memory for vertices
	//
	pVertices = (D3DMFOGTEST_VERTS *)malloc(sizeof(D3DMFOGTEST_VERTS) * D3DQA_NUMVERTS);
	if (NULL == pVertices)
	{
		DebugOut(DO_ERROR, L"malloc failed");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	uiCurrentVert = 0;
	fStepSizeZ = (1.0f / (D3DQA_RECTS_X * D3DQA_RECTS_Y - 1));
	for (uiStepY = 0; uiStepY < D3DQA_RECTS_Y; uiStepY++)
	{
		for (uiStepX = 0; uiStepX < D3DQA_RECTS_X; uiStepX++)
		{
			fRectZ = fStepSizeZ * (uiStepY * D3DQA_RECTS_X + uiStepX);

			//
			// Clamp
			//
			if (fRectZ > 0.99f)
				fRectZ = 0.99f;

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

			fBaseX = (float)uiStepX * fRectWidth;
			fBaseY = (float)uiStepY * fRectHeight;

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
			pVertices[uiCurrentVert].rhw = D3DQA_RHW;
			pVertices[uiCurrentVert].x = fBaseX;
			pVertices[uiCurrentVert].y = fBaseY;
			pVertices[uiCurrentVert].z = fRectZ;
			uiCurrentVert++;

			pVertices[uiCurrentVert].Diffuse = dwColor;
			pVertices[uiCurrentVert].Specular = dwSpecColor;
			pVertices[uiCurrentVert].rhw = D3DQA_RHW;
			pVertices[uiCurrentVert].x = fBaseX + fRectWidth;
			pVertices[uiCurrentVert].y = fBaseY;
			pVertices[uiCurrentVert].z = fRectZ;
			uiCurrentVert++;

			pVertices[uiCurrentVert].Diffuse = dwColor;
			pVertices[uiCurrentVert].Specular = dwSpecColor;
			pVertices[uiCurrentVert].rhw = D3DQA_RHW;
			pVertices[uiCurrentVert].x = fBaseX;
			pVertices[uiCurrentVert].y = fBaseY + fRectHeight;
			pVertices[uiCurrentVert].z = fRectZ;
			uiCurrentVert++;

			pVertices[uiCurrentVert].Diffuse = dwColor;
			pVertices[uiCurrentVert].Specular = dwSpecColor;
			pVertices[uiCurrentVert].rhw = D3DQA_RHW;
			pVertices[uiCurrentVert].x = fBaseX + fRectWidth;
			pVertices[uiCurrentVert].y = fBaseY;
			pVertices[uiCurrentVert].z = fRectZ;
			uiCurrentVert++;

			pVertices[uiCurrentVert].Diffuse = dwColor;
			pVertices[uiCurrentVert].Specular = dwSpecColor;
			pVertices[uiCurrentVert].rhw = D3DQA_RHW;
			pVertices[uiCurrentVert].x = fBaseX + fRectWidth;
			pVertices[uiCurrentVert].y = fBaseY + fRectHeight;
			pVertices[uiCurrentVert].z = fRectZ;
			uiCurrentVert++;

			pVertices[uiCurrentVert].Diffuse = dwColor;
			pVertices[uiCurrentVert].Specular = dwSpecColor;
			pVertices[uiCurrentVert].rhw = D3DQA_RHW;
			pVertices[uiCurrentVert].x = fBaseX;
			pVertices[uiCurrentVert].y = fBaseY + fRectHeight;
			pVertices[uiCurrentVert].z = fRectZ;
			uiCurrentVert++;
		}
	}

	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(hr = CreateFilledVertexBuffer( pDevice,                   // LPDIRECT3DMOBILEDEVICE pDevice,
	                                           ppVB,                      // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                           (BYTE*)pVertices,          // BYTE *pVertices,
	                                           sizeof(D3DMFOGTEST_VERTS), // UINT uiVertexSize,
	                                           D3DQA_NUMVERTS,            // UINT uiNumVertices,
	                                           D3DMFOGTEST_FVF)))         // DWORD dwFVF
	{
		DebugOut(DO_ERROR, L"CreateFilledVertexBuffer failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

cleanup:

	if (pVertices)
		free(pVertices);

	if ((FAILED(hr)) && (NULL != ppVB) && (NULL != (*ppVB)))
		(*ppVB)->Release();

	return hr;	
}


