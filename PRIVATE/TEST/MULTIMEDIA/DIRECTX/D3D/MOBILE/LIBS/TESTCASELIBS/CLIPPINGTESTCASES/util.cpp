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
#include <windows.h>
#include <d3dm.h>
#include <tchar.h>
#include "util.h"
#include "BufferTools.h"
#include "DebugOutput.h"

//
// Utility macros for color manipulation
//
#define GET_ALPHA(_c) ((_c & 0xFF000000) >> 24)
#define GET_RED(_c)   ((_c & 0x00FF0000) >> 16)
#define GET_GREEN(_c) ((_c & 0x0000FF00) >>  8)
#define GET_BLUE(_c)  ((_c & 0x000000FF) >>  0)

//
// ShrinkViewport
//   
//   Each test case, prior to execution, shrinks the viewport to ensure that clipping
//   against updated viewport extents is possible.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying D3D device
//   float fWidthMult:  Multiplier for viewport width, in the range of [0.0f, 1.0f] 
//   float fHeightMult:  Multiplier for viewport height, in the range of [0.0f, 1.0f] 
//   
// Return Value
// 
//   HRESULT indicates success or failure
//   
HRESULT ShrinkViewport(LPDIRECT3DMOBILEDEVICE pDevice, float fWidthMult, float fHeightMult)
{
	D3DMVIEWPORT Viewport;
	HRESULT hr;

	//
	// Parameter validation
	//
	if ((fHeightMult > 1.0f) ||
		(fWidthMult > 1.0f) ||
		(NULL == pDevice))
	{
		DebugOut(DO_ERROR, _T("Invalid parameter(s)."));
		return E_FAIL;
	}

	//
	// Retrieve viewable dimensions
	//
	if (FAILED(hr = pDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, _T("GetViewport failed. hr = 0x%08x"), hr);
		return hr;
	}

	//
	// Offset X and Y by 50% of width shrinkage
	//
	Viewport.X = Viewport.X + (DWORD)((Viewport.Width *(1.0f-fWidthMult )) / 2.0f);
	Viewport.Y = Viewport.Y + (DWORD)((Viewport.Height*(1.0f-fHeightMult)) / 2.0f);

	//
	// Shrink widths/heights
	//
	Viewport.Width  = (DWORD)(fWidthMult  * (float)Viewport.Width);
	Viewport.Height = (DWORD)(fHeightMult * (float)Viewport.Height);

	//
	// Indicate new viewable extents to Direct3D
	//
	if (FAILED(hr = pDevice->SetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, _T("SetViewport failed. hr = 0x%08x"), hr);
		return hr;
	}

	return S_OK;
}

//
// FillUntransformedVertex
//   
//   Convenient method for storing vertex data in buffer; accepts arguments for
//   all vertex components for this vertex type.
//
// Arguments:
//
//   CUSTOM_VERTEX *pVertex:  Storage for vertex data
//   
//   float x  \
//   float y   Vertex position
//   float z  /
//   
//   DWORD dwDiffuse:  -  Vertex color
//   DWORD dwSpecular: /
//   
// Return Value
// 
//   void
//   
void FillUntransformedVertex(CUSTOM_VERTEX *pVertex, float x, float y, float z, DWORD dwDiffuse, DWORD dwSpecular)
{
	pVertex->dwDiffuse = dwDiffuse;
	pVertex->dwSpecular = dwSpecular;
	pVertex->x = x;
	pVertex->y = y;
	pVertex->z = z;
}

//
// InterpColor
//   
//   Creates an intermediate color, via interpolation
//
// Arguments:
//
//   float fLineInterp:  float in range of [0.0f, 1.0f] indicating interpolation
//   DWORD dwColor1:  Starting color
//   DWORD dwColor2:  Ending color
//   
// Return Value
// 
//   DWORD:  interpolated color
//   
DWORD InterpColor(float fLineInterp, DWORD dwColor1, DWORD dwColor2)
{

	BYTE bRed   =   (BYTE)((float)GET_RED(dwColor1) * fLineInterp +   (float)GET_RED(dwColor2) * (1.0f - fLineInterp));
	BYTE bGreen = (BYTE)((float)GET_GREEN(dwColor1) * fLineInterp + (float)GET_GREEN(dwColor2) * (1.0f - fLineInterp));
	BYTE bBlue  =  (BYTE)((float)GET_BLUE(dwColor1) * fLineInterp +  (float)GET_BLUE(dwColor2) * (1.0f - fLineInterp));
	BYTE bAlpha = (BYTE)((float)GET_ALPHA(dwColor1) * fLineInterp + (float)GET_ALPHA(dwColor2) * (1.0f - fLineInterp));

	return D3DMCOLOR_RGBA(bRed, bGreen, bBlue, bAlpha);
}