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
