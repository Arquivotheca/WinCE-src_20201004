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

HRESULT BlendColor(D3DMBLEND Blend,
				   DWORD dwBlendFor,
				   BYTE bRedSrc, BYTE bGreenSrc, BYTE bBlueSrc, BYTE bAlphaSrc,
				   BYTE bRedDst, BYTE bGreenDst, BYTE bBlueDst, BYTE bAlphaDst,
				   float *pfRed, float *pfGreen, float *pfBlue, float *pfAlpha);

HRESULT PerformBlendOp(D3DMBLENDOP BlendOp,
				       float fRedSrc, float fGreenSrc, float fBlueSrc, float fAlphaSrc,
				       float fRedDst, float fGreenDst, float fBlueDst, float fAlphaDst,
				       BYTE *pbRed,   BYTE *pbGreen,   BYTE *pbBlue,   BYTE *pbAlpha);

HRESULT Blend(D3DMBLENDOP BlendOp,    D3DMBLEND BlendSrc, D3DMBLEND BlendDst,
			  DWORD dwColorSrc, DWORD dwColorDst, DWORD *pdwColorResult);

bool IsDestBlendSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLEND Blend);

bool IsSrcBlendSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLEND Blend);

bool IsBlendOpSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLENDOP BlendOp);

bool IsBlendSupported(LPDIRECT3DMOBILEDEVICE pDevice, D3DMBLEND SourceBlend, D3DMBLEND DestBlend, D3DMBLENDOP BlendOp);


//
// Operations commonly used in blending
//
BYTE FloatToByteClamp(float fValue);
BYTE FloatToScaleClampByte(float fValue);
