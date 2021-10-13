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
