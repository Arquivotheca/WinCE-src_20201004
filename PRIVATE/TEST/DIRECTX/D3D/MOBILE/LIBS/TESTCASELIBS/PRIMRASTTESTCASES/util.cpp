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
#include "util.h"

//
// SetupColorWrite
//
//   Indicates specific color channels that should (and should not) be rendered.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device to manipulate
//   COLOR_WRITE_MASK WriteMask:  Indicates color mask to set
//   
// Return Value:
//
//   Failure if attempting to disable any channel without COLORWRITEENABLE support
//
HRESULT SetupColorWrite(LPDIRECT3DMOBILEDEVICE pDevice, COLOR_WRITE_MASK WriteMask)
{
	//
	// Result:  only fails if color writes are disabled, but driver doesn't support it
	//
	HRESULT hr = S_OK;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// If testing COLORWRITEENABLE, build bitmask to indicate writable channels
	//
	DWORD dwColorWrite;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return E_FAIL;
	}

	//
	// Tweak COLORWRITEENABLE according to caller's instructions.
	//
	// Note to caller: render states are not retained across Resets
	//
	if (WriteMask != COLOR_WRITE_RGB)
	{
		if (D3DMPMISCCAPS_COLORWRITEENABLE & Caps.PrimitiveMiscCaps)
		{
			//
			// Update color write settings
			//
			dwColorWrite = D3DMCOLORWRITEENABLE_ALL;

			//
			// Red mask?
			//
			switch (WriteMask)
			{
			//case COLOR_WRITE_RGB:
			case COLOR_WRITE_XGB:
			//case COLOR_WRITE_RXB:
			//case COLOR_WRITE_RGX:
			case COLOR_WRITE_XXB:
			//case COLOR_WRITE_RXX:
			case COLOR_WRITE_XGX:
			case COLOR_WRITE_XXX:
				dwColorWrite &= (~D3DMCOLORWRITEENABLE_RED);
				break;
			}


			//
			// Green mask?
			//
			switch (WriteMask)
			{
			//case COLOR_WRITE_RGB:
			//case COLOR_WRITE_XGB:
			case COLOR_WRITE_RXB:
			//case COLOR_WRITE_RGX:
			case COLOR_WRITE_XXB:
			case COLOR_WRITE_RXX:
			//case COLOR_WRITE_XGX:
			case COLOR_WRITE_XXX:
				dwColorWrite &= (~D3DMCOLORWRITEENABLE_GREEN);
				break;
			}


			//
			// Blue mask?
			//
			switch (WriteMask)
			{
			//case COLOR_WRITE_RGB:
			//case COLOR_WRITE_XGB:
			//case COLOR_WRITE_RXB:
			case COLOR_WRITE_RGX:
			//case COLOR_WRITE_XXB:
			case COLOR_WRITE_RXX:
			case COLOR_WRITE_XGX:
			case COLOR_WRITE_XXX:
				dwColorWrite &= (~D3DMCOLORWRITEENABLE_BLUE);
				break;
			}
			
			pDevice->SetRenderState(D3DMRS_COLORWRITEENABLE, dwColorWrite);
		}
		else
		{
			OutputDebugString(_T("D3DMMPMISCCAPS_COLORWRITEENABLE not supported."));
			return E_FAIL;
		}
	}
	else
	{
		//
		// In case a previous test iteration has overwritten the COLORWRITEENABLE setting; reset it.
		//
		pDevice->SetRenderState(D3DMRS_COLORWRITEENABLE, D3DMCOLORWRITEENABLE_ALL);
	}

	return S_OK;
}


