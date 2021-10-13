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

typedef struct _CREATEINDEXBUFFER_ARGS {
	UINT Length;
	DWORD Usage;
	D3DMFORMAT Format;
	D3DMPOOL Pool;
} CREATEINDEXBUFFER_ARGS;

typedef struct _CREATEVERTEXBUFFER_ARGS {
	UINT Length;
	DWORD Usage;
	DWORD FVF;
	D3DMPOOL Pool;
} CREATEVERTEXBUFFER_ARGS;

typedef struct _CREATETEXTURE_ARGS {
	UINT Width;
	UINT Height;
	UINT Levels;
	DWORD Usage;
	D3DMFORMAT Format;
	D3DMPOOL Pool;
} CREATETEXTURE_ARGS;
