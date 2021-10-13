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
#include <d3dmtypes.h>

#define MAX_D3DM_STRING_LEN 64

//
// Strings corresponding to enumerators
//
__declspec(selectany) TCHAR D3DQA_D3DMFMT_NAMES[D3DMFMT_NUMFORMAT][MAX_D3DM_STRING_LEN] = {
_T("D3DMFMT_UNKNOWN"),               // D3DMFMT_UNKNOWN                =  0,
_T("D3DMFMT_R8G8B8"),                // D3DMFMT_R8G8B8                 =  1,
_T("D3DMFMT_A8R8G8B8"),              // D3DMFMT_A8R8G8B8               =  2,
_T("D3DMFMT_X8R8G8B8"),              // D3DMFMT_X8R8G8B8               =  3,
_T("D3DMFMT_R5G6B5"),                // D3DMFMT_R5G6B5                 =  4,
_T("D3DMFMT_X1R5G5B5"),              // D3DMFMT_X1R5G5B5               =  5,
_T("D3DMFMT_A1R5G5B5"),              // D3DMFMT_A1R5G5B5               =  6,
_T("D3DMFMT_A4R4G4B4"),              // D3DMFMT_A4R4G4B4               =  7,
_T("D3DMFMT_R3G3B2"),                // D3DMFMT_R3G3B2                 =  8,
_T("D3DMFMT_A8R3G3B2"),              // D3DMFMT_A8R3G3B2               =  9,
_T("D3DMFMT_X4R4G4B4"),              // D3DMFMT_X4R4G4B4               = 10,
_T("D3DMFMT_A8P8"),                  // D3DMFMT_A8P8                   = 11,
_T("D3DMFMT_P8"),                    // D3DMFMT_P8                     = 12,
_T("D3DMFMT_A8"),                    // D3DMFMT_A8                     = 13,
_T("D3DMFMT_UYVY"),                  // D3DMFMT_UYVY                   = 14,
_T("D3DMFMT_YUY2"),                  // D3DMFMT_YUY2                   = 15,
_T("D3DMFMT_DXT1"),                  // D3DMFMT_DXT1                   = 16,
_T("D3DMFMT_DXT2"),                  // D3DMFMT_DXT2                   = 17,
_T("D3DMFMT_DXT3"),                  // D3DMFMT_DXT3                   = 18,
_T("D3DMFMT_DXT4"),                  // D3DMFMT_DXT4                   = 19,
_T("D3DMFMT_DXT5"),                  // D3DMFMT_DXT5                   = 20,
_T("D3DMFMT_D32"),                   // D3DMFMT_D32                    = 21,
_T("D3DMFMT_D15S1"),                 // D3DMFMT_D15S1                  = 22,
_T("D3DMFMT_D24S8"),                 // D3DMFMT_D24S8                  = 23,
_T("D3DMFMT_D16"),                   // D3DMFMT_D16                    = 24,
_T("D3DMFMT_D24X8"),                 // D3DMFMT_D24X8                  = 25,
_T("D3DMFMT_D24X4S4"),               // D3DMFMT_D24X4S4                = 26,
_T("D3DMFMT_INDEX16"),               // D3DMFMT_INDEX16                = 27,
_T("D3DMFMT_INDEX32"),               // D3DMFMT_INDEX32                = 28,
_T("D3DMFMT_VERTEXDATA"),            // D3DMFMT_VERTEXDATA             = 29,
_T("D3DMFMT_D3DMVALUE_FLOAT"),       // D3DMFMT_D3DMVALUE_FLOAT        = 30,
_T("D3DMFMT_D3DMVALUE_FIXED"),       // D3DMFMT_D3DMVALUE_FIXED        = 31,
};                                   // D3DMFMT_NUMFORMAT              = 32,


#define D3DMPOOL_NUMENUMERATORS 3
__declspec(selectany) TCHAR D3DQA_D3DMPOOL_NAMES[D3DMPOOL_NUMENUMERATORS][MAX_D3DM_STRING_LEN] = {
_T("D3DMPOOL_VIDEOMEM"),            // D3DMPOOL_VIDEOMEM                = 0,
_T("D3DMPOOL_SYSTEMMEM"),           // D3DMPOOL_SYSTEMMEM               = 1,
_T("D3DMPOOL_MANAGED"),             // D3DMPOOL_MANAGED                 = 2,
};

