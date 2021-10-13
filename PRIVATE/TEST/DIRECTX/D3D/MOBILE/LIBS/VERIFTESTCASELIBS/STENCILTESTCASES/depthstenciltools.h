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

HRESULT NextDepthStencilFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT RenderTargetFormat, D3DMFORMAT *pFormat);
HRESULT NextStencilFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT RenderTargetFormat, D3DMFORMAT *pFormat);
UINT StencilBits(D3DMFORMAT FmtDepthStencil);
BOOL IsValidDepthStencilFormat(LPDIRECT3DMOBILE pD3D, LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT DepthStencilFormat);

HRESULT SetStencilStates(LPDIRECT3DMOBILEDEVICE pDevice, 
                         DWORD StencilEnable,
                         DWORD StencilMask,
                         DWORD StencilWriteMask,
                         D3DMSTENCILOP StencilZFail,
                         D3DMSTENCILOP StencilFail,
                         D3DMSTENCILOP StencilPass,
                         D3DMCMPFUNC StencilFunc,
                         DWORD StencilRef);
