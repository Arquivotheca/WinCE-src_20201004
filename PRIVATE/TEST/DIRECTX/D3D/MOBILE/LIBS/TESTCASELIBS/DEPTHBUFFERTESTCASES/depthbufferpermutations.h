//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include <d3dm.h>
#include "TestCases.h"

HRESULT SupportsTableIndex(UINT uiTableIndex, LPDIRECT3DMOBILEDEVICE pDevice);
HRESULT SetupTableIndex(UINT uiTableIndex, LPDIRECT3DMOBILEDEVICE pDevice);
HRESULT RenderTableIndex(UINT uiTableIndex, LPDIRECT3DMOBILEDEVICE pDevice);

typedef struct _DEPTHBUFTESTPERMS {
	float fClearZ;
	D3DMCMPFUNC ZCmpFunc;
	BOOL ZEnable;
	BOOL ZWriteEnable;
} DEPTHBUFTESTPERMS, *LPDEPTHBUFTESTPERMS;

__declspec(selectany) DEPTHBUFTESTPERMS DepthBufTestCases[D3DMQA_DEPTHBUFTEST_COUNT] = {

//
// Various clear depths
//
{	1.0f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.9f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.8f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.7f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.6f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.5f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.4f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.3f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.2f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.1f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },
{	0.0f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE  },

//
// Various Z-Cmp Functions
//
{	0.5f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE },
{	1.0f, D3DMCMP_NEVER       , D3DMZB_TRUE  , TRUE },
{	0.5f, D3DMCMP_LESS        , D3DMZB_TRUE  , TRUE },
{	0.8f, D3DMCMP_EQUAL       , D3DMZB_TRUE  , TRUE },
{	1.0f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , TRUE },
{	0.2f, D3DMCMP_GREATER     , D3DMZB_TRUE  , TRUE },
{	0.8f, D3DMCMP_NOTEQUAL    , D3DMZB_TRUE  , TRUE },
{	0.2f, D3DMCMP_GREATEREQUAL, D3DMZB_TRUE  , TRUE }, 
{	0.5f, D3DMCMP_ALWAYS      , D3DMZB_TRUE  , TRUE },
									 
//									 
// Z Disabled
//
{	1.0f, D3DMCMP_LESSEQUAL   , D3DMZB_FALSE , TRUE },

									 
//									 
// Z Write Disabled
//
{	1.0f, D3DMCMP_LESSEQUAL   , D3DMZB_TRUE  , FALSE},

};
