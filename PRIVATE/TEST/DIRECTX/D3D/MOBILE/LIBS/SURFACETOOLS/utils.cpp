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

bool GetPixelFormatInformation(
    const D3DMFORMAT  Format,
    unsigned int    & uiAlphaMask,
    unsigned int    & uiRedMask,
    unsigned int    & uiGreenMask,
    unsigned int    & uiBlueMask,
    int             & iAlphaOffset,
    int             & iRedOffset,
    int             & iGreenOffset,
    int             & iBlueOffset)
{
    uiAlphaMask = 0;
    uiRedMask = 0;
    uiGreenMask = 0;
    uiBlueMask = 0;
    iAlphaOffset = 0;
    iRedOffset = 0;
    iGreenOffset = 0;
    iBlueOffset = 0;
    switch(Format)
    {
    // 32 bpp w/ alpha colors (including the indexed ones)
    case D3DMFMT_A8R8G8B8:
        uiAlphaMask = 0xff000000;
        iAlphaOffset = 24;
    case D3DMFMT_X8R8G8B8:
    case D3DMFMT_R8G8B8:
        uiRedMask = 0xff0000;
        uiGreenMask = 0xff00;
        uiBlueMask = 0xff;
        iRedOffset = 16;
        iGreenOffset = 8;
        iBlueOffset = 0;
        break;
    case D3DMFMT_A1R5G5B5:
        uiAlphaMask = 0x8000;
        iAlphaOffset = 15;
        // Fall through to the masks and offsets for 16bpp rgb555
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
        return false;
    }
    return true;
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


//
// SetPixelLockedRect is a comprehensive method for setting pixels in a locked surface,
// regardless of the format's bpp. This version works with anything from 1 bpp to 32 bpp,
// in powers of 2.
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

