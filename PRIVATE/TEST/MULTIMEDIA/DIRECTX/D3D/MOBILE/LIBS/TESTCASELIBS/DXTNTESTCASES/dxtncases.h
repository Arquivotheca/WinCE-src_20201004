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
// To be included from ..\TestCases.h only

#pragma once

#include <windows.h>
#include <d3dm.h>
#include "TestCases.h"

#define D3DMQA_PRIMTYPE D3DMPT_TRIANGLESTRIP
#define D3DMQA_NUMPRIM 0

#define SAFERELEASE(x) if (x) {(x)->Release(); x = NULL;}

#define D3DQA_TEXTURE_WIDTH 32
#define D3DQA_TEXTURE_HEIGHT 32

#define _M(_v) D3DM_MAKE_D3DMVALUE(_v)

#define countof(x) (sizeof(x) / sizeof(*(x)))

/**
 * DXTnIsTestSupported
 * 
 *     This function determines if the driver under test has the right 
 *     capabilities to support the current DXTn test.
 * 
 * Parameters
 * 
 * 	  pDevice
 * 	      The Direct3DM device under test
 * 
 * 	  pCaps
 * 	      Capabilities of the driver
 * 
 * 	  dwTableIndex
 * 	      Index into the test case table
 * 
 * Return Value : HRESULT
 *     S_OK for success, S_FALSE if the device doesn't support the test case, 
 *     and Error otherwise.
 * 
 */
HRESULT DXTnIsTestSupported(
    IDirect3DMobileDevice * pDevice, 
    const D3DMCAPS * pCaps, 
    DWORD dwTableIndex,
    bool bMipMap);

/**
 * DXTnCreateAndPrepareTexture
 * 
 *     This function will create a DXTn texture (DXT format based on the test 
 *     index) and then fill the texture with appropriate DXT color and alpha 
 *     pixels.
 * 
 * Parameters
 * 
 * 	  pDevice
 * 	      The Direct3DM device under test
 * 
 * 	  dwTableIndex
 * 	      Index into the test case table
 * 
 * 	  ppTexture
 * 	      Pointer to return the texture pointer that has been created
 * 
 * Return Value : HRESULT
 *     S_OK for success, Error otherwise
 * 
 */
HRESULT DXTnCreateAndPrepareTexture (
    IDirect3DMobileDevice * pDevice, 
    DWORD dwTableIndex, 
    IDirect3DMobileTexture ** ppTexture,
    IDirect3DMobileSurface ** ppImageSurface);

/**
 * DXTnCreateAndPrepareVertexBuffer
 * 
 *     This function creates and fills a vertex buffer with the appropriate 
 *     test-case-specific vertex data.
 * 
 * Parameters
 * 
 * 	  pDevice
 * 	      The Direct3DM device under test
 * 
 * 	  hWnd
 * 	      Window that should be used to determine viewport size in pixels 
 * 	      (used to modify RHW vertices to fit to the window)
 * 
 * 	  dwTableIndex
 * 	      Index into the test case table
 * 
 * 	  ppVertexBuffer
 * 	      Pointer to return the vertex buffer pointer that has been created
 * 
 * 	  pVertexBufferStride
 * 	      Pointer to UINT that takes the stride of the vertex buffer
 * 
 * 	  pVertexPrimCount
 * 	      Pointer to UINT that takes the number of primitives that are put in 
 * 	      the vertex buffer
 * 
 * Return Value : HRESULT
 *     S_OK for success, Error otherwise
 * 
 */
HRESULT DXTnCreateAndPrepareVertexBuffer(
    IDirect3DMobileDevice * pDevice,
    HWND hWnd,
    DWORD dwTableIndex,
    IDirect3DMobileVertexBuffer ** ppVertexBuffer,
    UINT * pVertexBufferStride,
    UINT * pVertexPrimCount);

/**
 * DXTnSetDefaultTextureStates
 * 
 *     This function sets the texture stage states appropriately.
 * 
 * Parameters
 * 
 * 	  pDevice
 * 	      The Direct3DM device under test
 * 
 * Return Value : HRESULT
 *     S_OK for success, Error otherwise
 * 
 */
HRESULT DXTnSetDefaultTextureStates(IDirect3DMobileDevice * pDevice);

/**
 * DXTnSetTransforms
 * 
 *     This function sets the WORLD, VIEW, and PROJECTION matrices for the 
 *     test case. The WORLD matrix will vary depending on the test case.
 * 
 * Parameters
 * 
 * 	  pDevice
 * 	      The Direct3DM device under test
 * 
 * 	  dwTableIndex
 * 	      Index into the test case table
 *
 *    bSetWorld
 *        If true, set the world transform based on the entry in DXTnParams.
 * 
 * Return Value : HRESULT
 *     S_OK for success, Error otherwise
 * 
 */
HRESULT DXTnSetTransforms(IDirect3DMobileDevice * pDevice, DWORD dwTableIndex, bool bSetWorld);

/**
 * DXTnSetupRenderState
 * 
 *     This function sets the render states for the test case.
 * 
 * Parameters
 * 
 * 	  pDevice
 * 	      The Direct3DM device under test
 * 
 * 	  dwTableIndex
 * 	      Index into the test case table
 * 
 * Return Value : HRESULT
 *     S_OK for success, Error otherwise
 * 
 */
HRESULT DXTnSetupRenderState(IDirect3DMobileDevice * pDevice, DWORD dwTableIndex);

HRESULT DXTnMipMapDrawPrimitives(
    IDirect3DMobileDevice * pDevice,
    HWND hwnd,
    DWORD dwTableIndex,
    IDirect3DMobileVertexBuffer * pVertices,
    IDirect3DMobileSurface * pImageSurface);

#define D3DMDXTN_FVFDEF (D3DMFVF_XYZ_FLOAT | D3DMFVF_TEX1 | D3DMFVF_TEXCOORDSIZE2(0))

typedef struct _D3DMDXTNTEST_FVF {
	float x, y, z;
	float u,v;
} D3DMDXTN_FVF;


struct DXTnTestParameters {
    D3DMFORMAT Format;
    D3DMMATRIX * pTransform;
};

struct DXTnMipMapTestParameters {
    D3DMFORMAT Format;
    UINT TopLevelWidth;
    UINT TopLevelHeight;
    UINT Levels;
    UINT LevelToTest;
};

//
// These positions are untransformed. They work appropriately with the 
// transformation matrices set to the identity matrix (for simplicity).
//
#define POSX1  -1.0f
#define POSY1  1.0f
#define POSZ1  0.0f

#define POSX2  1.0f
#define POSY2  1.0f
#define POSZ2  0.0f

#define POSX3  -1.0f
#define POSY3  -1.0f
#define POSZ3  0.0f

#define POSX4  1.0f
#define POSY4  -1.0f
#define POSZ4  0.0f

