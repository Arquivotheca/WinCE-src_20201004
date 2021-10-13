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
#include "TestCases.h"

namespace DepthBiasNamespace
{
#define _M(_a) D3DM_MAKE_D3DMVALUE(_a)

#define countof(x) (sizeof(x) / sizeof(*x))

#define D3DMQA_TWTEXWIDTH 64
#define D3DMQA_TWTEXHEIGHT 64

    HRESULT CreateAndPrepareVertexBuffers(
        LPDIRECT3DMOBILEDEVICE        pDevice, 
        HWND                          hWnd,
        DWORD                         dwTableIndex, 
        LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBufferBase, 
        LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBufferTest, 
        UINT                         *pVBStrideBase,
        UINT                         *pVBStrideTest);

    HRESULT SetupTextureStages(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex);

    HRESULT SetupBaseRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex);

    HRESULT SetupTestRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex);

    UINT GetBasePrimitiveCount(DWORD dwTableIndex);
    
    UINT GetTestPrimitiveCount(DWORD dwTableIndex);

#define FLOAT_DONTCARE 1.0f

    //////////////////////////////////////////////
    //
    // Geometry definitions
    //
    /////////////////////////////////////////////

#define D3DMQA_PRIMTYPE D3DMPT_TRIANGLELIST

    //    (1)        (2) 
    //     +--------+  +
    //     |       /  /|
    //     |      /  / |
    //     |     /  /  |
    //     |    /  /   |
    //     |   /  /    |
    //     |  /  /     |
    //     | /  /      | 
    //     |/  /       |
    //     +  +--------+
    //    (3)         (4)
    //

#define POSX1  0.0f
#define POSY1  0.0f
#define POSZ1  0.5f

#define POSX2  1.0f
#define POSY2  0.0f
#define POSZ2  0.5f

#define POSX3  0.0f
#define POSY3  1.0f
#define POSZ3  0.5f

#define POSX4  1.0f
#define POSY4  1.0f
#define POSZ4  0.5f

#include "Primitives.h"

    enum Orientation {
        Horizontal,
        Vertical,
        None
    };

    typedef struct _DEPTHBIAS_TESTS {
        DWORD               dwBaseFVF;
        UINT                uiBaseFVFSize;
        PBYTE               pBaseVertexData;
        DWORD               uiBaseNumVerts;
        DWORD               dwTestFVF;
        UINT                uiTestFVFSize;
        PBYTE               pTestVertexData;
        DWORD               uiTestNumVerts;
        Orientation         eOrientation;
        D3DMRENDERSTATETYPE RenderState;
        float               rBiasValue;
    } DEPTHBIAS_TESTS;


    __declspec(selectany) DEPTHBIAS_TESTS DepthBiasCases [D3DMQA_DEPTHBIASTEST_COUNT] = {
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasTestVertices, countof(DepthBiasTestVertices), None, D3DMRS_DEPTHBIAS,  0.0f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasTestVertices, countof(DepthBiasTestVertices), None, D3DMRS_DEPTHBIAS,  0.001f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasTestVertices, countof(DepthBiasTestVertices), None, D3DMRS_DEPTHBIAS, -0.001f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasTestVertices, countof(DepthBiasTestVertices), None, D3DMRS_DEPTHBIAS,  0.5f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasTestVertices, countof(DepthBiasTestVertices), None, D3DMRS_DEPTHBIAS, -0.5f,
                    
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)SSDepthBiasYTestVertices, countof(SSDepthBiasYTestVertices), Vertical,   D3DMRS_SLOPESCALEDEPTHBIAS,  0.57f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)SSDepthBiasYTestVertices, countof(SSDepthBiasYTestVertices), Vertical,   D3DMRS_SLOPESCALEDEPTHBIAS, -0.57f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)SSDepthBiasXTestVertices, countof(SSDepthBiasXTestVertices), Horizontal, D3DMRS_SLOPESCALEDEPTHBIAS,  0.57f,
            D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)DepthBiasBaseVertices, countof(DepthBiasBaseVertices), D3DMDEPTHBIASTEST_COLOR_FVF, sizeof(D3DMDEPTHBIASTEST_COLOR), (PBYTE)SSDepthBiasXTestVertices, countof(SSDepthBiasXTestVertices), Horizontal, D3DMRS_SLOPESCALEDEPTHBIAS, -0.57f,
    };
};

