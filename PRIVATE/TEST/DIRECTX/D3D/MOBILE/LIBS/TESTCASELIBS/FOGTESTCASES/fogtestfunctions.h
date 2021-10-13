//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "VertexFogCases.h"
#include "PixelFogCases.h"

typedef
HRESULT
(*SET_STATES) (
	LPDIRECT3DMOBILEDEVICE pDevice,
	DWORD dwTableIndex
	);

typedef
HRESULT
(*SUPPORTS_TABLE_INDEX) (
	LPDIRECT3DMOBILEDEVICE pDevice,
	DWORD dwTableIndex
	);

typedef
HRESULT
(*SETUP_GEOMETRY) (
	LPDIRECT3DMOBILEDEVICE pDevice,
	HWND hWnd,
	LPDIRECT3DMOBILEVERTEXBUFFER *ppVB
	);
