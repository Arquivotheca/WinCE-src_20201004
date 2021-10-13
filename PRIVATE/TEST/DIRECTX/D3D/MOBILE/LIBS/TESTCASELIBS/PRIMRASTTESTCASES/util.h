//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <d3dm.h>
#include "PrimRastPermutations.h"

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


HRESULT SetupColorWrite(LPDIRECT3DMOBILEDEVICE pDevice, COLOR_WRITE_MASK WriteMask);
