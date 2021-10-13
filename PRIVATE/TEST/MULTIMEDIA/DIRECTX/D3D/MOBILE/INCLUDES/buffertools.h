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

//
//    Creates a vertex buffer of the specified FVF and FVF size
//
LPDIRECT3DMOBILEVERTEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice, UINT uiNumVerts, DWORD dwFVF,
										        DWORD dwFVFSize, DWORD dwUsage, D3DMPOOL d3dPool);

LPDIRECT3DMOBILEVERTEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice, UINT uiNumVerts, DWORD dwFVF,
										        DWORD dwFVFSize, DWORD dwUsage);



//
// Given data, and specs for a vertex buffer, creates the vertex buffer
// and fills it with the data
//
HRESULT CreateFilledVertexBuffer(LPDIRECT3DMOBILEDEVICE pDevice,
                                 LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
                                 BYTE *pVertices,
                                 UINT uiVertexSize,
                                 UINT uiNumVertices,
                                 DWORD dwFVF);


//
// Inspect a FVF mask and determine the number of required bytes
//
ULONG BytesPerVertex(ULONG FVF);


//
// Creates an index buffer based on args
//
LPDIRECT3DMOBILEINDEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice,
                                               UINT uiNumIndices,
                                               UINT uiIndexSize,
                                               DWORD dwUsage,
                                               D3DMPOOL D3DMPOOL);

LPDIRECT3DMOBILEINDEXBUFFER CreateActiveBuffer(LPDIRECT3DMOBILEDEVICE pd3dDevice,
                                               UINT uiNumIndices,
                                               UINT uiIndexSize,
                                               DWORD dwUsage);

//
// Given data, and specs for a index buffer, creates the index buffer
// and fills it with the data
//
HRESULT CreateFilledIndexBuffer(LPDIRECT3DMOBILEDEVICE pDevice,
                                LPDIRECT3DMOBILEINDEXBUFFER *ppIB,
                                BYTE *pIndices,
                                UINT uiIndexSize,
                                UINT uiNumIndices);


//
// Vertex data fillers
//
VOID ColorVertRange(BYTE *pVertices, UINT uiCount, DWORD dwColor, UINT uiStride, UINT uiOffset);
VOID RegularTLTriListTex(BYTE *pVertices, UINT uiOffset, UINT uiStride);
VOID RectangularTLTriList(BYTE *pVertices, UINT uiX, UINT uiY, UINT uiWidth, UINT uiHeight, float fDepth, UINT uiStride);
VOID FillTLPos(BYTE *pVertices, float fX, float fY, float fZ, float fRHW);

//
// Color component extraction
//
#define D3D_GETA(rgba)  ((BYTE)((rgba & 0xFF000000) >> 24))
#define D3D_GETR(rgba)  ((BYTE)((rgba & 0x00FF0000) >> 16))
#define D3D_GETG(rgba)  ((BYTE)((rgba & 0x0000FF00) >>  8))
#define D3D_GETB(rgba)  ((BYTE)((rgba & 0x000000FF) >>  0))

