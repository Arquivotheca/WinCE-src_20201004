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
    HRESULT hr;
    bool fRet = false;
    LPDIRECT3DMOBILE pD3D = NULL;
    D3DMCAPS Caps;
    D3DMADAPTER_IDENTIFIER AdapterIdentifier;

    if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"Could not get device caps (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (FAILED(hr = pDevice->GetDirect3D(&pD3D)))
    {
        DebugOut(DO_ERROR, L"Could not get Direct3D object (hr = 0x%08x) ",hr);
        goto cleanup;
    }

    if (FAILED(hr = pD3D->GetAdapterIdentifier(Caps.AdapterOrdinal, 0, &AdapterIdentifier)))
    {
        DebugOut(DO_ERROR, L"Could not get adapter identifier (hr = 0x%08x) ", hr);
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
    LPDIRECT3DMOBILESURFACE pBackBuffer = NULL;

    //
    // The surface description of the backbuffer.
    //
    D3DMSURFACE_DESC d3dmsd;
    
    if (FAILED(hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        DebugOut(DO_ERROR, L"GetBackBuffer failed in GetBestFormat (hr = 0x%08x) ", hr);
        goto cleanup;
    }

    if (FAILED(hr = pBackBuffer->GetDesc(&d3dmsd)))
    {
        DebugOut(DO_ERROR, L"GetDesc failed in GetBestFormat (hr = 0x%08x) ", hr);
        goto cleanup;
    }

    *pFormat = d3dmsd.Format;
    
cleanup:
    if (pBackBuffer)
        pBackBuffer->Release();
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
    int iShifts = 0;
    DWORD dwTemp = dwNum;

    if (dwNum >= 0x80000000)
    {
        return 0x80000000;
    }
    while (dwTemp)
    {
        dwTemp >>= 1;
        ++iShifts;
    }

    if ((dwNum >> (iShifts - 2)) == 3)
        return 1 << iShifts;
    return 1 << (iShifts - 1);
}

