//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <d3dm.h>
#include "qamath.h"
#include "QAD3DMX.h"

#define _M(a) D3DM_MAKE_D3DMVALUE(a)

HRESULT SetTransformDefaults(LPDIRECT3DMOBILEDEVICE pDevice)
{
	D3DMMATRIX World = {  _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
	                      _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
	                      _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
	                      _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	D3DMMATRIX View  = {  _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
	                      _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
	                      _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
	                      _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	
	D3DMMATRIX ProjGen;
	D3DMMATRIX Proj;

	D3DMatrixPerspectiveFovLH( &ProjGen, D3DQA_PI / 4.0f, 1.0f, 1.0f, 500.0f);

	Proj = ProjGen;

	if (FAILED(pDevice->SetTransform(D3DMTS_WORLD,  // D3DMTRANSFORMSTATETYPE State,
	                                 &World,        // CONST D3DMMATRIX *pMatrix
	                                 D3DMFMT_D3DMVALUE_FLOAT)))
	{
		OutputDebugString(_T("SetTransform failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetTransform(D3DMTS_VIEW,  // D3DMTRANSFORMSTATETYPE State,
	                                 &View,        // CONST D3DMMATRIX *pMatrix
	                                 D3DMFMT_D3DMVALUE_FLOAT)))
	{
		OutputDebugString(_T("SetTransform failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetTransform(D3DMTS_PROJECTION, // D3DMTRANSFORMSTATETYPE State,
	                                 &Proj,             // CONST D3DMMATRIX *pMatrix
	                                 D3DMFMT_D3DMVALUE_FLOAT)))
	{
		OutputDebugString(_T("SetTransform failed."));
		return E_FAIL;
	}

	return S_OK;
}
