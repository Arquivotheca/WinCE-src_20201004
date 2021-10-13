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
#include "ParseArgs.h"
#include "DebugOutput.h"

bool IsRefDriver(LPDIRECT3DMOBILEDEVICE pDevice)
{
    bool fRet = false;
    LPDIRECT3DMOBILE pD3D = NULL;
    D3DMCAPS Caps;
    D3DMADAPTER_IDENTIFIER AdapterIdentifier;
    HRESULT hr;

    if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"Could not get device caps. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (FAILED(hr = pDevice->GetDirect3D(&pD3D)))
    {
        DebugOut(DO_ERROR, L"Could not get Direct3D object. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (FAILED(hr = pD3D->GetAdapterIdentifier(Caps.AdapterOrdinal, 0, &AdapterIdentifier)))
    {
        DebugOut(DO_ERROR, L"Could not get adapter identifier. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (!_wcsicmp(D3DQA_D3DMREF_FILENAME, AdapterIdentifier.Driver))
    {
        fRet = true;
    }
    
cleanup:
    if (pD3D)
        pD3D->Release();
    return fRet;
}

HRESULT GetBestFormat(
    LPDIRECT3DMOBILEDEVICE pDevice,
    D3DMRESOURCETYPE ResourceType,
    D3DMFORMAT * pFormat)
{
    HRESULT hr;
    //
    // We need the Direct3DMobile object to determine what format to use.
    //
    LPDIRECT3DMOBILE pD3D = NULL;

	//
	// There must be at least one valid format, for creating surfaces.
	// The first time a valid format is found, this BOOL is toggled.
	//
	// If the BOOL is never toggled, the test will fail after attempting
	// all possible formats.
	//
	BOOL bFoundValidFormat = FALSE;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format is found.
	//
	D3DMFORMAT CurrentFormat;

	//
	// The display mode's spatial resolution, color resolution,
	// and refresh frequency.
	//
	D3DMDISPLAYMODE Mode;

    //
    // Retrieve device capabilities
    //
    if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    
    //
    // Retrieve device display mode
    //
    if (FAILED(hr = pDevice->GetDisplayMode(&Mode)))
    {
        DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    //
    // Get the D3DMobile object with which to determine a proper format.
	if (FAILED(hr = pDevice->GetDirect3D(&pD3D)) || NULL == pD3D)
	{
	    DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x)", hr);
	    goto cleanup;
	}

    for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
    {
        if (!(FAILED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal, // UINT Adapter
                                              Caps.DeviceType,          // D3DMDEVTYPE DeviceType
                                              Mode.Format,        // D3DMFORMAT AdapterFormat
                                              0,                  // ULONG Usage
                                              ResourceType,  // D3DMRESOURCETYPE RType
                                              CurrentFormat))))   // D3DMFORMAT CheckFormat
        {
            bFoundValidFormat = TRUE;
            hr = S_OK;
            *pFormat = CurrentFormat;
            break;
        }
    }
    
    if (FALSE == bFoundValidFormat)
    {
        DebugOut(DO_ERROR, L"No valid surface formats found.");
        hr = E_FAIL;
        goto cleanup;
    }
cleanup:
    if (pD3D)
        pD3D->Release();
    return hr;
}

bool IsPowerOfTwo(DWORD dwNum)
{
    if (dwNum < 1)
    {
        return false;
    }

    return ((dwNum - 1) & dwNum) == 0;
}
DWORD NearestPowerOfTwo(DWORD dwNum)
{
    int iPow = 0;
    int iDiff = 0;
    DWORD dwTemp = dwNum;

    // Approximately how many times does 2 go into our number?
    while (dwTemp > 0)
    {
        dwTemp /= 2;
        iPow++;
    }

    // Calculate the power of two based on previous result.
    dwTemp = 1;
    while (iPow--)
    {
        dwTemp *= 2;
    }

    // Figure out if the next nearest power of two is closer to the original
    // (if so, use it instead).
    iDiff = dwNum - dwTemp;
    if (iDiff > 0)
    {
        dwTemp *= 2;
        if (iDiff < (int)(dwTemp - dwNum))
            dwTemp /= 2;
    }
    else
    {
        dwTemp /= 2;
        if (iDiff > (int)(dwTemp - dwNum))
            dwTemp *= 2;
    }
    return dwTemp;
}

