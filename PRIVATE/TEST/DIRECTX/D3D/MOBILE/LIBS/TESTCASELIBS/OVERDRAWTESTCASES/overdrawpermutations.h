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

//
// Test Case Parameters
//
typedef struct _OVERDRAW_TESTS {
	D3DMPRIMITIVETYPE PrimType;
	D3DMCULL  CullMode;
	UINT      uiDrawOrder;
} OVERDRAW_TESTS, *LPOVERDRAW_TESTS;


__declspec(selectany) OVERDRAW_TESTS OverDrawTests[D3DMQA_OVERDRAWTEST_COUNT] = {

{ D3DMPT_TRIANGLELIST,     D3DMCULL_NONE, 0},
{ D3DMPT_TRIANGLELIST,     D3DMCULL_CCW,  0},
{ D3DMPT_TRIANGLELIST,     D3DMCULL_CW,   0},
{ D3DMPT_TRIANGLELIST,     D3DMCULL_NONE, 1},
{ D3DMPT_TRIANGLELIST,     D3DMCULL_CCW,  1},
{ D3DMPT_TRIANGLELIST,     D3DMCULL_CW,   1},

{ D3DMPT_TRIANGLESTRIP,    D3DMCULL_NONE, 0},
{ D3DMPT_TRIANGLESTRIP,    D3DMCULL_CCW,  0},
{ D3DMPT_TRIANGLESTRIP,    D3DMCULL_CW,   0},
{ D3DMPT_TRIANGLESTRIP,    D3DMCULL_NONE, 1},
{ D3DMPT_TRIANGLESTRIP,    D3DMCULL_CCW,  1},
{ D3DMPT_TRIANGLESTRIP,    D3DMCULL_CW,   1}
};
