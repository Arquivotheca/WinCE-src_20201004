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
