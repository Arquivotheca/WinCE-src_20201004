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
#include "TwoArgOps.h"
#include "TexOpUtil.h"
#include "DebugOutput.h"

//
// IsTwoArgTestOpSupported
//
//   Verifies that the driver's capabilities indicate support for the operation(s)
//   specified for this test.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile Device
//   DWORD dwTableIndex:  Index of test case
//   
// Return Value:
//
//   BOOL:  TRUE if supported; FALSE otherwise
//
BOOL IsTwoArgTestOpSupported(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex)
{
	//
	// Future note: may expand test to use distinct ops on alpha and color.
	//

	if (FALSE == IsTexOpSupported(pDevice, TwoArgOpCases[dwTableIndex].TextureOp))
		return FALSE;

	return TRUE;
}


//
// SetTwoArgStates
//  
//   Set the texture stage states, for texture arguments and operations,
//   per the specification for a particular test case.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile Device
//   DWORD dwTableIndex:  Index of test case
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT SetTwoArgStates(LPDIRECT3DMOBILEDEVICE pDevice, DWORD dwTableIndex)
{
	HRESULT hr = S_OK;

	//
	// Indicate sources of texture blending arguments
	//
	if ((FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_COLORARG1, TwoArgOpCases[dwTableIndex].TextureArg1))) ||
		(FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAARG1, TwoArgOpCases[dwTableIndex].TextureArg1))) ||
		(FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_COLORARG2, TwoArgOpCases[dwTableIndex].TextureArg2))) ||
		(FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAARG2, TwoArgOpCases[dwTableIndex].TextureArg2))))
	{
		DebugOut(DO_ERROR, L"SetTextureStageState failed. (hr = 0x%08x)", hr);
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// Verify that the texture op is supported
	//
	if (FALSE == IsTexOpSupported(pDevice, TwoArgOpCases[dwTableIndex].TextureOp))
	{
		DebugOut(DO_ERROR, L"A texture op required for this test case is not supported. (hr = 0x%08x)", hr);
		hr = E_FAIL;
		goto cleanup;
	}



	//
	// Set texture ops
	//
	if ((FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_COLOROP, TwoArgOpCases[dwTableIndex].TextureOp))) ||
		(FAILED(hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAOP, TwoArgOpCases[dwTableIndex].TextureOp))))

	{
		DebugOut(DO_ERROR, L"SetTextureStageState failed. (hr = 0x%08x)", hr);
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	return hr;
}



//
// GetTwoArgColors
//  
//   Gets the colors to be associated with each texture blend input.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile Device
//   DWORD dwTableIndex:  Index of test case
//   PDWORD *ppdwDiffuse:  Value to associate with diffuse FVF component
//   PDWORD *ppdwSpecular: Value to associate with specular FVF component
//   PDWORD *ppdwTFactor:  Value to associate with D3DRS_TEXTUREFACTOR
//   PDWORD *ppdwTexture:  Value to associate with texture bound to stage zero
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetTwoArgColors(LPDIRECT3DMOBILEDEVICE pDevice,
                          DWORD dwTableIndex,
                          PDWORD *ppdwDiffuse, 
                          PDWORD *ppdwSpecular,
                          PDWORD *ppdwTFactor, 
                          PDWORD *ppdwTexture)
{
	HRESULT hr = S_OK;

	//
	// Iterator for setting up D3DMTA_*
	//
	UINT uiArg;

	//
	// Bad parameter checks
	//
	if ((NULL == ppdwDiffuse) || 
	    (NULL == ppdwSpecular) || 
	    (NULL == ppdwTFactor) || 
	    (NULL == ppdwTexture))
	{
		DebugOut(DO_ERROR, L"GetTwoArgColors:  NULL argument(s). Aborting.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Default to NULL; if still NULL at completion of function, the caller will
	// know that the blender input is not needed for this test case.
	//
	*ppdwDiffuse = NULL;
	*ppdwSpecular= NULL;
	*ppdwTFactor = NULL;
	*ppdwTexture = NULL;
	
	//
	// Set up sources of texture blending arguments
	//
	for (uiArg = 1; uiArg < 3; uiArg++)
	{
		PDWORD pdwColor;
 		PDWORD pdwTexArg;

		switch(uiArg)
		{
		case 1:
			pdwColor =  &TwoArgOpCases[dwTableIndex].ColorValue1;
			pdwTexArg = &TwoArgOpCases[dwTableIndex].TextureArg1;
			break;
		case 2:
			pdwColor =  &TwoArgOpCases[dwTableIndex].ColorValue2;
			pdwTexArg = &TwoArgOpCases[dwTableIndex].TextureArg2;
			break;
		default:
			DebugOut(DO_ERROR, L"Invalid texture argument index. Aborting.");
			hr = E_FAIL;
			goto cleanup;
		};

		//
		// Take action based on input selection; ignore D3DMTA_* options
		//
		switch((*pdwTexArg) & D3DMTA_SELECTMASK)
		{
		case D3DMTA_DIFFUSE:
			*ppdwDiffuse = pdwColor;
			break;
		case D3DMTA_TEXTURE:
			*ppdwTexture = pdwColor;
			break;
		case D3DMTA_TFACTOR:
			*ppdwTFactor = pdwColor;
			break;
		case D3DMTA_SPECULAR:
			*ppdwSpecular = pdwColor;
			break;
		default:
			DebugOut(DO_ERROR, L"Invalid D3DMTA_*. Aborting.");
			hr = E_FAIL;
			goto cleanup;
		};
	}

cleanup:

	return hr;
}

