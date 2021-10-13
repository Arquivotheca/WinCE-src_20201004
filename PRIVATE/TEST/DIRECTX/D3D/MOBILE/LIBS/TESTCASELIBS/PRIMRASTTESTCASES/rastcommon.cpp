//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <d3dm.h>
#include "PrimRast.h"

//
// ColorVertex
//
//   Color the vertex based on it's X and Y position (not relative to viewport extents).
//   Specific formula, and colors generated as a result, are arbitrary.  The important
//   factor is to cause some significant variance in the colors picked for the checkerboard's
//   squares.
//
//
// Arguments:
//
//   D3DMCOLOR *pColor:  Location to which function should write color
//   UINT uiX:  X Position of vertex
//   UINT uiY:  Y Position of vertex
//
#define D3DMCOLOR_FORMULA_1(_x,_y) D3DMCOLOR_XRGB((_x+_y)%255,255-(_x+_y)%255,128)
#define D3DMCOLOR_FORMULA_2(_x,_y) D3DMCOLOR_XRGB((_x%2)*255,(_x+_y)%255,(_y%2)*255)
VOID ColorVertex(D3DMCOLOR *pColor, UINT uiX, UINT uiY)
{
	UINT uiComputeX, uiComputeY;

	//
	// Color computations are on granularity of 10x10 areas of target
	//
	uiComputeX = uiX / 10;
	uiComputeY = uiY / 10;

	//
	// nested if/else causes checkerboard pattern (color choices are arbitrary)
	//
	if (uiComputeX % 2)
	{
		if (uiComputeY % 2)
		{
			*pColor = D3DMCOLOR_FORMULA_1(uiComputeX*5, uiComputeY*5);
		}
		else
		{
			*pColor = D3DMCOLOR_FORMULA_2(uiComputeX*5, uiComputeY*5);
		}
	}
	else
	{
		if (uiComputeY % 2)
		{
			*pColor = D3DMCOLOR_FORMULA_2(uiComputeX*5, uiComputeY*5);
		}
		else
		{
			*pColor = D3DMCOLOR_FORMULA_1(uiComputeX*5, uiComputeY*5);
		}
	}
}

//
// SimpleDIP
//
//   Wrapper for DrawIndexedPrimitive, that eliminates some complexity
//   by passing trivially computed values for arguments that might allow
//   driver optimizations.
//
// Arguments:
// 
//    LPDIRECT3DMOBILEDEVICE pDevice:  Target device
//    LPDIRECT3DMOBILEVERTEXBUFFER pVB:  Vertex buffer to render from
//    D3DMPRIMITIVETYPE Type:  Passed straight through to DrawIndexedPrimitive
//    UINT StartIndex:  Passed straight through to DrawIndexedPrimitive
//    UINT PrimitiveCount:  Passed straight through to DrawIndexedPrimitive
//    
// Return Value:
//
//   HRESULT: S_OK if successful; failure otherwise
//
HRESULT SimpleDIP(LPDIRECT3DMOBILEDEVICE pDevice,
                  LPDIRECT3DMOBILEVERTEXBUFFER pVB,
                  D3DMPRIMITIVETYPE Type,
                  UINT StartIndex,
                  UINT PrimitiveCount)
{
	D3DMVERTEXBUFFER_DESC Desc;
	UINT uiNumVertices;

	//
	// Get VB Description
	//
	if (FAILED(pVB->GetDesc(&Desc)))
	{
		OutputDebugString(_T("SimpleDIP: LPDIRECT3DMOBILEVERTEXBUFFER->GetDesc Failed."));
		return E_FAIL;
	}

	//
	// Validate that VB is as-expected
	//
	if ((PRIMRASTFVF_FXD != Desc.FVF) && (PRIMRASTFVF_FLT != Desc.FVF))
	{
		OutputDebugString(_T("SimpleDIP: No support for this FVF."));
		return E_FAIL;
	}

	//
	// VB Size must be exactly divisible by vert size
	//
	if (Desc.Size % sizeof(PRIMRASTVERT))
	{
		OutputDebugString(_T("SimpleDIP: Unsupported vertex size."));
		return E_FAIL;
	}

	//
	// Number of vertices
	//
	uiNumVertices = Desc.Size / sizeof(PRIMRASTVERT);

	//
	// Indicate primitive data to be drawn
	//
	if (FAILED(pDevice->DrawIndexedPrimitive(Type,             // D3DMPRIMITIVETYPE Type,
	                                         0,                // INT BaseVertexIndex, *
	                                         0,                // UINT MinIndex,       *** Assume simplest case
	                                         uiNumVertices,    // UINT NumVertices,    *
	                                         StartIndex,       // UINT StartIndex,
	                                         PrimitiveCount))) // UINT PrimitiveCount
	{
		OutputDebugString(_T("DrawIndexedPrimitive failed."));
		return E_FAIL;
	}

	return S_OK;
}
                                   

