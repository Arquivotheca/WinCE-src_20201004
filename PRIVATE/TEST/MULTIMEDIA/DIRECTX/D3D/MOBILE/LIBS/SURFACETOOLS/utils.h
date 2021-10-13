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

