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

HRESULT GetValidTextureArgs(LPDIRECT3DMOBILEDEVICE pDevice, CREATETEXTURE_ARGS *pCreateTexArgs, SIZE_T *pEstimatedSize);
HRESULT GetValidVertexBufferArgs(LPDIRECT3DMOBILEDEVICE pDevice, CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs, SIZE_T *pEstimatedSize);
HRESULT GetValidIndexBufferArgs(LPDIRECT3DMOBILEDEVICE pDevice, CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs, SIZE_T *pEstimatedSize);
HRESULT CreateVertexBufferResources(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiResourceCount, CONST CREATEVERTEXBUFFER_ARGS *pCreateVertexBufferArgs, LPDIRECT3DMOBILERESOURCE **pppInterfaces);
HRESULT CreateIndexBufferResources(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiResourceCount, CONST CREATEINDEXBUFFER_ARGS *pCreateIndexBufferArgs, LPDIRECT3DMOBILERESOURCE **pppInterfaces);
HRESULT CreateTextureResources(LPDIRECT3DMOBILEDEVICE pDevice, UINT uiResourceCount, CONST CREATETEXTURE_ARGS *pCreateTextureArgs, LPDIRECT3DMOBILERESOURCE **pppInterfaces);

HRESULT GetValidTextureFormat(LPDIRECT3DMOBILEDEVICE pDevice, D3DMFORMAT *pFormat);
HRESULT GetBytesPerPixel(D3DMFORMAT Format, UINT *puiBytesPerPixel);

HRESULT SupportsManagedPool(LPDIRECT3DMOBILEDEVICE pDevice);
HRESULT SupportsTextures(LPDIRECT3DMOBILEDEVICE pDevice);
