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
#include "DepthBiasCases.h"
#include "utils.h"
#include "DebugOutput.h"

namespace DepthBiasNamespace
{
    bool IsRefDriver(LPDIRECT3DMOBILEDEVICE pDevice)
    {
        HRESULT hr;
        bool fRet = false;
        LPDIRECT3DMOBILE pD3D = NULL;
        D3DMCAPS Caps;
        D3DMADAPTER_IDENTIFIER AdapterIdentifier;

        if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
        {
            DebugOut(DO_ERROR, L"Could not get device caps. (hr = 0x%08x) ", hr);
            goto cleanup;
        }

        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)))
        {
            DebugOut(DO_ERROR, L"Could not get Direct3D object. (hr = 0x%08x) ", hr);
            goto cleanup;
        }

        if (FAILED(hr = pD3D->GetAdapterIdentifier(Caps.AdapterOrdinal, 0, &AdapterIdentifier)))
        {
            DebugOut(DO_ERROR, L"Could not get adapter identifier. (hr = 0x%08x) ", hr);
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
            DebugOut(DO_ERROR, L"GetDeviceCaps failed.. (hr = 0x%08x) ", hr);
            goto cleanup;
        }
        
        //
        // Retrieve device display mode
        //
        if (FAILED(hr = pDevice->GetDisplayMode(&Mode)))
        {
            DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x) ", hr);
            goto cleanup;
        }

        //
        // Get the D3DMobile object with which to determine a proper format.
        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)) || NULL == pD3D)
        {
            DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x) ", hr);
            goto cleanup;
        }

        for (CurrentFormat = (D3DMFORMAT)0; CurrentFormat < D3DMFMT_NUMFORMAT; (*(ULONG*)(&CurrentFormat))++)
        {
            if (!(FAILED(hr = pD3D->CheckDeviceFormat(Caps.AdapterOrdinal, // UINT Adapter
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

    HRESULT GetBestAlphaFormat(
        LPDIRECT3DMOBILEDEVICE pDevice,
        D3DMRESOURCETYPE ResourceType,
        D3DMFORMAT * pFormat)
    {
        HRESULT hr;
        D3DMFORMAT rgAlphaFormats[] = {
            D3DMFMT_A8R8G8B8,
            D3DMFMT_A8R3G3B2,
    //        D3DMFMT_A4R4G4B4,
    //        D3DMFMT_A1R5G5B5
        };
        
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
        int CurrentFormat;

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
            DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) ", hr);
            goto cleanup;
        }
        
        //
        // Retrieve device display mode
        //
        if (FAILED(hr = pDevice->GetDisplayMode(&Mode)))
        {
            DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x) ", hr);
            goto cleanup;
        }

        //
        // Get the D3DMobile object with which to determine a proper format.
        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)) || NULL == pD3D)
        {
            DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x) ", hr);
            goto cleanup;
        }

        for (CurrentFormat = 0; CurrentFormat < countof(rgAlphaFormats); CurrentFormat++)
        {
            if (!(FAILED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal, // UINT Adapter
                                                  Caps.DeviceType,          // D3DMDEVTYPE DeviceType
                                                  Mode.Format,        // D3DMFORMAT AdapterFormat
                                                  0,                  // ULONG Usage
                                                  ResourceType,  // D3DMRESOURCETYPE RType
                                                  rgAlphaFormats[CurrentFormat]))))   // D3DMFORMAT CheckFormat
            {
                bFoundValidFormat = TRUE;
                hr = S_OK;
                *pFormat = rgAlphaFormats[CurrentFormat];
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

    int GetFormatSize(D3DMFORMAT Format)
    {
        switch(Format)
        {
        // 32 bpp w/ alpha colors (including the indexed ones)
        case D3DMFMT_A8R8G8B8:
        case D3DMFMT_X8R8G8B8:
            return 32;
        case D3DMFMT_R8G8B8:
            return 24;
        case D3DMFMT_A1R5G5B5:
        case D3DMFMT_X1R5G5B5:
        case D3DMFMT_R5G6B5:
            return 16;
        }
        return 0;
    }

    HRESULT SetPixelLockedRect(D3DMFORMAT Format, D3DMLOCKED_RECT &LockedRect, int iX, int iY, UINT uiPixel)
    {
        // How many bits per pixel?
        int iBPP = GetFormatSize(Format);

        // uiTemp stores the masked and shifted version of the original then combined with the new.
        UINT uiTemp;

        // How many bytes do we need to copy?
        int iBytesPerPixel = (iBPP + 7) / 8;
        // How many pixels are in the uiTemp?
        int iPixelsPerI = 32 / iBPP;
        // What bit location in the uiTemp is the current pixel at?
        int iShift = iBPP * ((iPixelsPerI - 1) - iX % iPixelsPerI) % 8;
        // What mask do we need to get at the pixel?
        UINT uiValueMask = (((UINT64)1) << iBPP) - 1;
        uiTemp = 0;
        // Get the bytes that define the pixel.
        memcpy((BYTE*)&uiTemp, ((BYTE*)LockedRect.pBits) + (iY * LockedRect.Pitch) + iX * iBPP / 8, iBytesPerPixel);

        // Insert the pixel
        uiTemp &= ~(uiValueMask << iShift);
        uiTemp |= (uiPixel & uiValueMask) << iShift;
        // Save the bytes.
        memcpy(((BYTE*)LockedRect.pBits) + (iY * LockedRect.Pitch) + iX * iBPP / 8, (BYTE*)&uiTemp, iBytesPerPixel);

        return S_OK;
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
};
