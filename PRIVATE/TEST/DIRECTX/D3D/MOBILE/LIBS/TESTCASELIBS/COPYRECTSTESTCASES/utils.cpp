//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <d3dm.h>
#include "ParseArgs.h"

bool IsRefDriver(LPDIRECT3DMOBILEDEVICE pDevice)
{
    bool fRet = false;
    LPDIRECT3DMOBILE pD3D = NULL;
    D3DMCAPS Caps;
    D3DMADAPTER_IDENTIFIER AdapterIdentifier;

    if (FAILED(pDevice->GetDeviceCaps(&Caps)))
    {
        OutputDebugString(L"Could not get device caps");
        goto cleanup;
    }

    if (FAILED(pDevice->GetDirect3D(&pD3D)))
    {
        OutputDebugString(L"Could not get Direct3D object");
        goto cleanup;
    }

    if (FAILED(pD3D->GetAdapterIdentifier(Caps.AdapterOrdinal, 0, &AdapterIdentifier)))
    {
        OutputDebugString(L"Could not get adapter identifier");
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
        OutputDebugString(L"GetBackBuffer failed in GetBestFormat");
        goto cleanup;
    }

    if (FAILED(hr = pBackBuffer->GetDesc(&d3dmsd)))
    {
        OutputDebugString(L"GetDesc failed in GetBestFormat");
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

