//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

LPDIRECT3DMOBILEVERTEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice, UINT uiNumVerts, DWORD dwFVF,
										        DWORD dwFVFSize, DWORD dwUsage);

HRESULT CreateFilledVertexBuffer(LPDIRECT3DMOBILEDEVICE pDevice,
                                 LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
                                 BYTE *pVertices,
                                 UINT uiVertexSize,
                                 UINT uiNumVertices,
                                 DWORD dwFVF);
