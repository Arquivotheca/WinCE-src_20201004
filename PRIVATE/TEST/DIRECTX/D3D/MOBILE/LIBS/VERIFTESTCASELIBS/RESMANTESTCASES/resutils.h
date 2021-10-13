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
