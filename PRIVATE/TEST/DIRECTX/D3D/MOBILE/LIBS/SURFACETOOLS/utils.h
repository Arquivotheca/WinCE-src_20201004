//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

bool GetPixelFormatInformation(
    const D3DMFORMAT  Format,
    unsigned int    & uiAlphaMask,
    unsigned int    & uiRedMask,
    unsigned int    & uiGreenMask,
    unsigned int    & uiBlueMask,
    int             & iAlphaOffset,
    int             & iRedOffset,
    int             & iGreenOffset,
    int             & iBlueOffset);

HRESULT SetPixelLockedRect(
    D3DMFORMAT        Format, 
    D3DMLOCKED_RECT & LockedRect, 
    int               iX, 
    int               iY, 
    UINT              uiPixel);

