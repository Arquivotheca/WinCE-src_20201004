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
#include "AlphaTestCases.h"
#include "utils.h"
#include "DebugOutput.h"

namespace AlphaTestNamespace
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
            DebugOut(DO_ERROR, L"Could not get device caps. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)))
        {
            DebugOut(DO_ERROR, L"Could not get Direct3D object. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = pD3D->GetAdapterIdentifier(Caps.AdapterOrdinal, 0, &AdapterIdentifier)))
        {
            DebugOut(DO_ERROR, L"Could not get adapter identifier. hr = 0x%08x", hr);
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
            DebugOut(DO_ERROR, L"GetDeviceCaps failed. hr = 0x%08x", hr);
            goto cleanup;
        }
        
        //
        // Retrieve device display mode
        //
        if (FAILED(hr = pDevice->GetDisplayMode(&Mode)))
        {
            DebugOut(DO_ERROR, L"GetDisplayMode failed. hr = 0x%08x", hr);
            goto cleanup;
        }

        //
        // Get the D3DMobile object with which to determine a proper format.
        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)) || NULL == pD3D)
        {
            DebugOut(DO_ERROR, L"GetDirect3D failed. hr = 0x%08x", hr);
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
            DebugOut(DO_ERROR, _T("No valid surface formats found."));
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
            DebugOut(DO_ERROR, L"GetDeviceCaps failed. hr = 0x%08x", hr);
            goto cleanup;
        }
        
        //
        // Retrieve device display mode
        //
        if (FAILED(hr = pDevice->GetDisplayMode(&Mode)))
        {
            DebugOut(DO_ERROR, L"GetDisplayMode failed. hr = 0x%08x", hr);
            goto cleanup;
        }

        //
        // Get the D3DMobile object with which to determine a proper format.
        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)) || NULL == pD3D)
        {
            DebugOut(DO_ERROR, L"GetDirect3D failed. hr = 0x%08x", hr);
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
            DebugOut(DO_ERROR, _T("No valid surface formats found."));
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
        case D3DMFMT_A8R3G3B2:
        case D3DMFMT_A4R4G4B4:
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

    HRESULT FillAlphaSurface(LPDIRECT3DMOBILESURFACE pSurface, TextureFill TexFill, D3DMCOLOR Color)
    {
        UINT Width = 0, Height = 0;
        D3DMFORMAT Format;
        D3DMSURFACE_DESC SurfaceDesc;
        D3DMLOCKED_RECT LockedRect = {0};
        HRESULT hr;
        LPDIRECT3DMOBILEDEVICE pDevice = NULL;
        LPDIRECT3DMOBILESURFACE pSurfaceTemp = NULL;

        //
        // These values will be needed to determine the colors in the gradient.
        //
        int iAlphaOffset = 0;
        int iRedOffset = 0;
        int iGreenOffset = 0;
        int iBlueOffset = 0;
        int iAlphaMax = 0;
        int iRedMax = 0;
        int iGreenMax = 0;
        int iBlueMax = 0;
        int iRed;
        int iGreen;
        int iBlue;
        int iAlpha;
        unsigned int uiAlphaMask = 0;
        unsigned int uiRedMask = 0;
        unsigned int uiGreenMask = 0;
        unsigned int uiBlueMask = 0;
        int iY, iX;

        if (FAILED(hr = pSurface->GetDesc(&SurfaceDesc)))
        {
            DebugOut(DO_ERROR, L"Could not get surface description of source surface to setup. hr = 0x%08x", hr);
            goto cleanup;
        }

        Width = SurfaceDesc.Width;
        Height = SurfaceDesc.Height;
        Format = SurfaceDesc.Format;

        if (FAILED(hr = pSurface->LockRect(&LockedRect, NULL, 0)))
        {
            //
            // The surface cannot be locked, so create a temporary image surface instead.
            // We will fill that surface with a gradient, and then copy over to the
            // test source surface.
            //
            if (FAILED(hr = pSurface->GetDevice(&pDevice)))
            {
                DebugOut(DO_ERROR, L"Could not get device with which to create temp image surface. hr = 0x%08x", hr);
                goto cleanup;
            }

            if (FAILED(hr = pDevice->CreateImageSurface(Width, Height, Format, &pSurfaceTemp)))
            {
                DebugOut(DO_ERROR, L"Could not create temporary image surface. hr = 0x%08x", hr);
                goto cleanup;
            }

            if (FAILED(hr = pSurfaceTemp->LockRect(&LockedRect, NULL, 0)))
            {
                DebugOut(DO_ERROR, L"Could not lock image surface. hr = 0x%08x", hr);
                goto cleanup;
            }
        }

        switch(Format)
        {
        // 32 bpp w/ alpha colors (including the indexed ones)
        case D3DMFMT_A8R8G8B8:
            uiAlphaMask = 0xff000000;
            iAlphaOffset = 24;
            __fallthrough;
        case D3DMFMT_X8R8G8B8:
        case D3DMFMT_R8G8B8:
            uiRedMask = 0xff0000;
            uiGreenMask = 0xff00;
            uiBlueMask = 0xff;
            iRedOffset = 16;
            iGreenOffset = 8;
            iBlueOffset = 0;
            break;
        case D3DMFMT_A4R4G4B4:
            uiAlphaMask = 0xf000;
            uiRedMask   = 0x0f00;
            uiGreenMask = 0x00f0;
            uiBlueMask  = 0x000f;
            iAlphaOffset = 12;
            iRedOffset   = 8;
            iGreenOffset = 4;
            iBlueOffset  = 0;            
            break;
        case D3DMFMT_A8R3G3B2:
            uiAlphaMask = 0xff00;
            uiRedMask   = 0x00e0;
            uiGreenMask = 0x001c;
            uiBlueMask  = 0x0003;
            iAlphaOffset = 8;
            iRedOffset   = 5;
            iGreenOffset = 2;
            iBlueOffset  = 0;            
            break;
        case D3DMFMT_A1R5G5B5:
            uiAlphaMask = 0x8000;
            iAlphaOffset = 15;
            // Fall through to the masks and offsets for 16bpp rgb555
            __fallthrough;
        case D3DMFMT_X1R5G5B5:
            uiRedMask = 0x7c00;
            uiGreenMask = 0x3e0;
            uiBlueMask = 0x1f;
            iRedOffset = 10;
            iGreenOffset = 5;
            iBlueOffset = 0;
            break;
        case D3DMFMT_R5G6B5:
            uiRedMask = 0xf800;
            uiGreenMask = 0x7e0;
            uiBlueMask = 0x1f;
            iRedOffset = 11;
            iGreenOffset = 5;
            iBlueOffset = 0;
            break;
        default:
            DebugOut(DO_ERROR, L"Unsupported format in FillAlphaSurface: 0x%08x", Format);
            return E_FAIL;
        }

        iAlphaMax = uiAlphaMask >> iAlphaOffset;
        iRedMax   = uiRedMask   >> iRedOffset;
        iGreenMax = uiGreenMask >> iGreenOffset;
        iBlueMax  = uiBlueMask  >> iBlueOffset;
        
        iRed   = R(Color) * iRedMax / 255;
        iGreen = G(Color) * iGreenMax / 255;
        iBlue  = B(Color) * iBlueMax / 255;

        //
        // Fill in the surface with a gradient.
        //
        for (iY = 0; iY < (int) Height; iY++)
        {
            for (iX = 0; iX < (int) Width; iX++)
            {
                UINT uiPixel;

                if (tfGradient == TexFill) 
                    iAlpha = iAlphaMax * iX / Width;
                else if (tfBlocks == TexFill)
                {
                    int rgAlpha[] = {0, 1, 126, 127, 127, 128, 254, 255};
                    int iBlockX = (8 * iX / Width) & 7;
                    int iBlockY = 4 * iY / Height;
                    iAlpha = rgAlpha[iBlockX];
                    if (0 == iBlockY && 0 != iAlpha)
                    {
                        --iAlpha;
                    }
                    if (3 == iBlockY && 255 != iAlpha)
                    {
                        ++iAlpha;
                    }
                    if (iAlpha > iAlphaMax)
                        iAlpha = iAlphaMax;
                }
                else if (tfSolid == TexFill)
                {
                    iAlpha = iAlphaMax;
                }
                else
                {
                    DebugOut(DO_ERROR, L"FillAlphaSurface was called with an invalid TextureFill.");
                    hr = E_INVALIDARG;
                    goto cleanup;
                }
                uiPixel = 
                    (iAlpha << iAlphaOffset) | 
                    (iRed << iRedOffset) | 
                    (iGreen << iGreenOffset) | 
                    (iBlue << iBlueOffset);
                SetPixelLockedRect(Format, LockedRect, iX, iY, uiPixel);
            }
        }

        //
        // If we needed to create a temporary surface to construct the image,
        // copy the image back to the test surface.
        //
        if (pSurfaceTemp)
        {
            RECT rcSource = {0, 0, Width, Height}, rcDest = {0, 0, Width, Height};
            pSurfaceTemp->UnlockRect();
            pDevice->CopyRects(pSurfaceTemp, NULL, 0, pSurface, NULL);
        }
        else
        {
            pSurface->UnlockRect();
        }
        
    cleanup:
        if (pSurfaceTemp)
            pSurfaceTemp->Release();
        if (pDevice)
            pDevice->Release();
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
};
