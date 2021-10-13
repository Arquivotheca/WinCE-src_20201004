//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
