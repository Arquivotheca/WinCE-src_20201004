//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
// Included for the SetDefaultTextureStages method.
#include "..\FVFTestCases\utils.h"

#define countof(x) (sizeof(x) / sizeof(*x))
namespace AlphaTestNamespace
{
    bool IsRefDriver(LPDIRECT3DMOBILEDEVICE pDevice);

    HRESULT GetBestFormat(
        LPDIRECT3DMOBILEDEVICE pDevice,
        D3DMRESOURCETYPE       ResourceType,
        D3DMFORMAT           * pFormat);
        
    HRESULT GetBestAlphaFormat(
        LPDIRECT3DMOBILEDEVICE pDevice,
        D3DMRESOURCETYPE       ResourceType,
        D3DMFORMAT           * pFormat);

    HRESULT FillAlphaSurface(LPDIRECT3DMOBILESURFACE pSurface, TextureFill TexFill);

    bool IsPowerOfTwo(DWORD dwNum);
    DWORD NearestPowerOfTwo(DWORD dwNum);
};
