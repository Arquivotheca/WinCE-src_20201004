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

LPDIRECT3DMOBILEVERTEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice, UINT uiNumVerts, DWORD dwFVF,
										        DWORD dwFVFSize, DWORD dwUsage);

HRESULT CreateFilledVertexBuffer(LPDIRECT3DMOBILEDEVICE pDevice,
                                 LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
                                 BYTE *pVertices,
                                 UINT uiVertexSize,
                                 UINT uiNumVertices,
                                 DWORD dwFVF);
