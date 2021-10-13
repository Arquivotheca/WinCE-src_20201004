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
#include <tchar.h>
#include "Transforms.h"
#include "QAD3DMX.h"

#define _M(_v) D3DM_MAKE_D3DMVALUE(_v)

//
// SetTransformDefaults
//   
//   Sets trivial, but useful, defaults for Direct3D transformation pipeline
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying D3D device
//   
// Return Value
// 
//   HRESULT indicates success or failure
//   
HRESULT SetTransformDefaults(LPDIRECT3DMOBILEDEVICE pDevice)
{
	D3DMMATRIX World = { _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	D3DMMATRIX View  = { _M(1.0f),  _M(0.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(1.0f),  _M(0.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(1.0f),  _M(0.0f)  ,
		                 _M(0.0f),  _M(0.0f),  _M(0.0f),  _M(1.0f)  };

	
	D3DMMATRIX Proj;

	D3DMatrixPerspectiveFovLH( &Proj, ProjSpecs.fFOV, 1.0f, ProjSpecs.fNearZ, ProjSpecs.fFarZ);

	if (FAILED(pDevice->SetTransform(D3DMTS_WORLD,              // D3DMTRANSFORMSTATETYPE State,
								     &World,                    // CONST D3DMMATRIX *pMatrix
	                                 D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		OutputDebugString(_T("SetTransform failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetTransform(D3DMTS_VIEW,               // D3DMTRANSFORMSTATETYPE State,
								     &View,                     // CONST D3DMMATRIX *pMatrix
	                                 D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		OutputDebugString(_T("SetTransform failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetTransform(D3DMTS_PROJECTION,         // D3DMTRANSFORMSTATETYPE State,
								     &Proj,                     // CONST D3DMMATRIX *pMatrix
	                                 D3DMFMT_D3DMVALUE_FLOAT))) // D3DMFORMAT Format
	{
		OutputDebugString(_T("SetTransform failed."));
		return E_FAIL;
	}

	return S_OK;
}