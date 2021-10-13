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
#include "utils.h"
#include "DebugOutput.h"

#define _M(_v) D3DM_MAKE_D3DMVALUE(_v)

HRESULT SetTransforms(LPDIRECT3DMOBILEDEVICE pDevice)
{
    HRESULT hr;
	D3DMMATRIX World = { _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	D3DMMATRIX View  = { _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	
	D3DMMATRIX Proj  = { _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	if (FAILED(hr = pDevice->SetTransform(D3DMTS_WORLD,              // D3DMTRANSFORMSTATETYPE State,
								          &World,                    // CONST D3DMMATRIX *pMatrix
	                                      D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		DebugOut(DO_ERROR, L"SetTransform WORLD failed. (hr = 0x%08x)", hr);
		return hr;
	}

	if (FAILED(hr = pDevice->SetTransform(D3DMTS_VIEW,               // D3DMTRANSFORMSTATETYPE State,
								          &View,                     // CONST D3DMMATRIX *pMatrix
	                                      D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		DebugOut(DO_ERROR, L"SetTransform VIEW failed. (hr = 0x%08x)", hr);
		return hr;
	}

	if (FAILED(hr = pDevice->SetTransform(D3DMTS_PROJECTION,         // D3DMTRANSFORMSTATETYPE State,
								          &Proj,                     // CONST D3DMMATRIX *pMatrix
	                                      D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		DebugOut(DO_ERROR, L"SetTransform PROJECTION failed. (hr = 0x%08x)", hr);
		return hr;
	}

	return S_OK;
}

