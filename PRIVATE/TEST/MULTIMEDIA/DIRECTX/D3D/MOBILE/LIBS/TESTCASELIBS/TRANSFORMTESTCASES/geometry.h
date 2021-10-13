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
#include <windows.h>
#include <d3dm.h>


// Custom FVF / vertex type
#define CUSTOM_VERTEX_FLT (D3DMFVF_XYZ_FLOAT|D3DMFVF_DIFFUSE)
#define CUSTOM_VERTEX_FXD (D3DMFVF_XYZ_FIXED|D3DMFVF_DIFFUSE)
struct CUSTOM_VERTEX
{
	// The untransformed position for the vertex

    union {
        float fX;
        ULONG ulX;
    };

    union {
        float fY;
        ULONG ulY;
    };

    union {
        float fZ;
        ULONG ulZ;
    };
    
    D3DMCOLOR color;     // The diffuse vertex color
};



HRESULT GenerateSphere(LPDIRECT3DMOBILEDEVICE pDevice,
                       LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
					   D3DMFORMAT Format,
                       LPDIRECT3DMOBILEINDEXBUFFER *ppIB, 
                       const int nDivisions,
                       D3DMPRIMITIVETYPE *pPrimType,
                       PUINT puiPrimCount,
                       PUINT puiNumVertices);


typedef int SFIX32;
#define DEFAULT_SFIX32 16 //default mantissa bits for 32-bits signed
#define FloatToSFIX32(a,n) ( (SFIX32)((a)*((SFIX32)1<<(n))) )

#define FLOAT_TO_FIXED(_f) FloatToSFIX32(_f, DEFAULT_SFIX32)


DWORD __forceinline GetFixedOrFloat(FLOAT fValue, D3DMFORMAT Format)
{
	DWORD dwResult;

	switch(Format)
	{
	case D3DMFMT_D3DMVALUE_FIXED:
		dwResult = FLOAT_TO_FIXED(fValue);
		break;
	case D3DMFMT_D3DMVALUE_FLOAT:
		dwResult = *(DWORD*)(&fValue);
		break;
	default:
		OutputDebugString(_T("Bad D3DMFORMAT passed into GetFixedOrFloat."));
		dwResult = 0;
		break;
	}
	return dwResult;
}
