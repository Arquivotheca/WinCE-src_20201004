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
#include "Geometry.h"
#include "BufferTools.h"
#include <tchar.h>
#include <windows.h>
#include "DebugOutput.h"

void FillCustomVertex(CUSTOM_VERTEX *pVertex, float x, float y, float z, DWORD dwDiffuse, D3DMFORMAT Format)
{
	
	pVertex->color = dwDiffuse;
	pVertex->ulX = GetFixedOrFloat(x, Format);
	pVertex->ulY = GetFixedOrFloat(y, Format);
	pVertex->ulZ = GetFixedOrFloat(z, Format);
}

//
// Use, for debugging purposes to position sphere
//
#define D3DQA_ZOFFSET 0.0f


HRESULT GenerateSphere(LPDIRECT3DMOBILEDEVICE pDevice,
                       LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
					   D3DMFORMAT Format,
                       LPDIRECT3DMOBILEINDEXBUFFER *ppIB, 
                       const int nDivisions,
                       D3DMPRIMITIVETYPE *pPrimType,
                       PUINT puiPrimCount,
                       PUINT puiNumVertices)
{
	//
	// non-configurable radius
	//
	const float fRadius = 25.0f;

	bool bInvertFaces = FALSE;
	const float	pi = 3.1415926535f;
	const int   nVertices = (2 * nDivisions * nDivisions) + 2;
	const int   nIndices = 12 * nDivisions * nDivisions;
	CUSTOM_VERTEX * pVertices = NULL;
	WORD *      pIndices = NULL;

	//
	// May be a fixed-point FVF, may be a floating-point FVF
	//
	DWORD dwFVF;

	//
	// Intermediate variables used during color computations
	//
	BYTE Red = 0xFF, Green = 0, Blue = 0;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Iterators for vertex position generation
	//
	int     i, j;

	//
	// Variables used for sphere computation
	//
	float   dj = (float)(pi/(nDivisions+1.0f));
	float	di = (float)(pi/nDivisions);
	int	    voff;	// vertex offset
	int     ind;	// indices offset

	//
	// Pointer for buffer locks
	// 
	BYTE *pByte;

	//
	// Pool for index buffers
	//
	D3DMPOOL IndexBufPool;

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Some colored stripes are useful to help in visual orientation during testing
	//
	CONST UINT uiStripe1 = 1*(nDivisions / 4);
	CONST UINT uiStripe2 = 2*(nDivisions / 4);
	CONST UINT uiStripe3 = 3*(nDivisions / 4);

	if ((NULL == ppIB) || (NULL == ppVB))
		return E_FAIL;

	*ppIB = NULL;
	*ppVB = NULL;


	switch(Format)
	{
	case D3DMFMT_D3DMVALUE_FIXED:
		dwFVF = CUSTOM_VERTEX_FXD;
		break;
	case D3DMFMT_D3DMVALUE_FLOAT:
		dwFVF = CUSTOM_VERTEX_FLT;
		break;
	default:
		DebugOut(DO_ERROR, L"Bad D3DMFORMAT passed into GenerateSphere.");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve driver capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Pick the pool that the driver supports, as reported in D3DMCAPS
	//
	if (D3DMSURFCAPS_VIDINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_VIDEOMEM;
	}
	else if (D3DMSURFCAPS_SYSINDEXBUFFER & Caps.SurfaceCaps)
	{
		IndexBufPool = D3DMPOOL_SYSTEMMEM;
	}
	else
	{
		DebugOut(DO_ERROR, L"Failure: no support for either D3DMSURFCAPS_SYSINDEXBUFFER or D3DMSURFCAPS_VIDINDEXBUFFER");
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Allocate memory for vertices and indices
	//
	pVertices = (CUSTOM_VERTEX *)malloc(sizeof(CUSTOM_VERTEX) * nVertices);
	pIndices = (WORD *)malloc(sizeof(WORD) * nIndices);
	if ((NULL == pVertices) || (NULL == pIndices))
	{
		DebugOut(DO_ERROR, L"malloc failed");
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	//
	// Generate all sphere vertices; beginning with north and south poles (0 and 1)
	//
	FillCustomVertex(&pVertices[0], 0.0f,  fRadius, D3DQA_ZOFFSET, D3DMCOLOR_XRGB(Red,Green,Blue), Format);
	FillCustomVertex(&pVertices[1], 0.0f, -fRadius, D3DQA_ZOFFSET, D3DMCOLOR_XRGB(Red,Green,Blue), Format);

	for (i=0; i<nDivisions*2; i++) 
	{
		for (j=0; j<nDivisions; j++) 
		{
			float x, y, z;

			//
			// Calculate vertex positions
			//
			y = (float) cos((j+1) * dj);
			x = (float) sin(i * di) * (float) sin((j+1) * dj);
			z = (float) cos(i * di) * (float) sin((j+1) * dj);

			//
			// apply radius, and optional Z offset
			//
			y *= fRadius;
			x *= fRadius;
			z *= fRadius;
			z += D3DQA_ZOFFSET;

			//
			// Generate vertex color
			//
			if ((j == uiStripe1) || (j == uiStripe2) || (j == uiStripe3))
			{
				Red   = 0x00;
				Green = 0x00;
				Blue  = 0xFF;
			}
			else if (i%2) // Vert Stripes
			{
				Red = 0xFF;
				Green = 0x00;
				Blue = 0x00;
			}
			else // Vert Gradient
			{
				Red = 0x00; 
				Green = (BYTE)(((float)i / (float)(nDivisions*2)) * 255.0f);
				Blue = 0x00;
			}

			//
			// Store vertex
			//
			FillCustomVertex(&pVertices[2+i+j*nDivisions*2], x, y, z, D3DMCOLOR_XRGB(Red,Green,Blue), Format);
		}
	}

	// now generate the triangle indices
	// strip around north pole first
	for (i=0; i<nDivisions*2; i++) 
	{
		if (bInvertFaces)
		{
			pIndices[3*i] = (WORD)(i+3);
			pIndices[3*i+1] = (WORD)(i+2);
			pIndices[3*i+2] = (WORD)0;
		}
		else
		{
			pIndices[3*i] = (WORD)0;
			pIndices[3*i+1] = (WORD)(i+2);
			pIndices[3*i+2] = (WORD)(i+3);
		}

		if (i==nDivisions*2-1)
			pIndices[3*i+2] = 2;
	}

	// now all the middle strips
	for (j=0; j<nDivisions-1; j++) 
	{
		voff = 2+j*nDivisions*2;
		ind = 3*nDivisions*2 + j*6*nDivisions*2;

		for (i=0; i<nDivisions*2; i++) 
		{
			if (bInvertFaces)
			{
				pIndices[6*i+ind]   = (WORD)(voff + i + nDivisions * 2);
				pIndices[6*i+2+ind] = (WORD)(voff + i + 1);
				pIndices[6*i+1+ind] = (WORD)(voff + i);

				pIndices[6*i+ind+3]   = (WORD)(voff + i + 1);
				pIndices[6*i+2+ind+3] = (WORD)(voff + i + nDivisions * 2);
				pIndices[6*i+1+ind+3] = (WORD)(voff + i + nDivisions * 2 + 1);

				if (i==nDivisions*2-1) 
				{
					pIndices[6*i+2+ind] = (WORD)(voff + i + 1 - 2 * nDivisions);
					pIndices[6*i+2+ind+3] = (WORD)(voff + i + 1 - 2 * nDivisions);
					pIndices[6*i+1+ind+3] = (WORD)(voff + i + nDivisions * 2 + 1 - 2 * nDivisions);
				}
			}
			else
			{
				pIndices[6*i+ind] = (WORD)(voff + i);
				pIndices[6*i+2+ind] = (WORD)(voff + i + 1);
				pIndices[6*i+1+ind] = (WORD)(voff + i + nDivisions * 2);

				pIndices[6*i+ind+3] = (WORD)(voff + i + nDivisions * 2);
				pIndices[6*i+2+ind+3] = (WORD)(voff + i + 1);
				pIndices[6*i+1+ind+3] = (WORD)(voff + i + nDivisions * 2 + 1);

				if (i==nDivisions*2-1) 
				{
					pIndices[6*i+2+ind] = (WORD)(voff + i + 1 - 2 * nDivisions);
					pIndices[6*i+2+ind+3] = (WORD)(voff + i + 1 - 2 * nDivisions);
					pIndices[6*i+1+ind+3] = (WORD)(voff + i + nDivisions * 2 + 1 - 2 * nDivisions);
				}
			}
		}
	}

	// finally strip around south pole
	voff = nVertices - nDivisions * 2;
	ind = nIndices - 3 * nDivisions * 2;

	for (i=0; i<nDivisions*2; i++) 
	{
		if (bInvertFaces)
		{
			pIndices[3*i+ind] = (WORD)(voff + i);
			pIndices[3*i+1+ind] = (WORD)(voff + i + 1);
			pIndices[3*i+2+ind] = (WORD)1;
		}
		else
		{
			pIndices[3*i+ind] = (WORD)1;
			pIndices[3*i+1+ind] = (WORD)(voff + i + 1);
			pIndices[3*i+2+ind] = (WORD)(voff + i);
		}

		if (i==nDivisions*2-1)
			pIndices[3*i+1+ind] = (WORD)voff;
	}

	//
	// A vertex buffer is needed for this test; create and bind to data stream
	// 
	*ppVB = CreateActiveBuffer(pDevice,              // LPDIRECT3DMOBILEDEVICE pd3dDevice,
	                           nVertices,            // UINT uiNumVerts,
	                           dwFVF,                // DWORD dwFVF,
	                           sizeof(CUSTOM_VERTEX),// DWORD dwFVFSize,
	                           0);                   // DWORD dwUsage
	if (NULL == *ppVB)
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed.");
		hr = E_FAIL;
		goto cleanup;
	}

	if (FAILED(hr = (*ppVB)->Lock(0, nVertices*sizeof(CUSTOM_VERTEX), (VOID**)&pByte, 0)))
	{
		DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	memcpy(pByte, pVertices, nVertices*sizeof(CUSTOM_VERTEX));

	if (FAILED(hr = (*ppVB)->Unlock()))
	{
		DebugOut(DO_ERROR, L"Unlock failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	if (FAILED(hr = pDevice->CreateIndexBuffer((sizeof(WORD)*nIndices), // UINT Length,
	                                            0,                       // DWORD Usage,
	                                            D3DMFMT_INDEX16,         // D3DMFORMAT Format,
	                                            IndexBufPool,            // D3DMPOOL Pool,
	                                            ppIB)))                  // IDirect3DMobileIndexBuffer** ppIndexBuffer,
	{
		DebugOut(DO_ERROR, L"CreateIndexBuffer failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	//
	// Additional check to satisfy prefast
	//
	if (NULL == *ppIB)
	{
		DebugOut(DO_ERROR, L"Failing due to NULL IDirect3DMobileIndexBuffer**.");
		hr = E_POINTER;
		goto cleanup;
	}

	if (FAILED(hr = (*ppIB)->Lock(0, (sizeof(WORD)*nIndices), (VOID**)&pByte, 0)))
	{
		DebugOut(DO_ERROR, L"Lock failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	memcpy(pByte, pIndices, nIndices*sizeof(WORD));

	if (FAILED(hr = (*ppIB)->Unlock()))
	{
		DebugOut(DO_ERROR, L"Unlock failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	if (FAILED(hr = pDevice->SetIndices(*ppIB))) // IDirect3DMobileIndexBuffer *pIndexData
	{
		DebugOut(DO_ERROR, L"SetIndices failed. (hr = 0x%08x)", hr);
		goto cleanup;
	}

	*pPrimType = D3DMPT_TRIANGLELIST;

	//
	// Triangle strips use, in total, two more vertices than the number of resultant triangles
	//
	*puiPrimCount = nIndices / 3; 
	*puiNumVertices = nVertices;

cleanup:

	if (pVertices)
		free(pVertices);

	if (pIndices)
		free(pIndices);

	if ((FAILED(hr)) && (NULL != *ppIB))
		(*ppIB)->Release();

	if ((FAILED(hr)) && (NULL != *ppVB))
		(*ppVB)->Release();

	return hr;
}
