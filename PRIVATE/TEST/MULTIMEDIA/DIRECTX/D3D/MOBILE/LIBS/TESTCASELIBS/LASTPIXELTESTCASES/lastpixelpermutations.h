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
typedef struct _LAST_PIXEL_TESTS {
	D3DMPRIMITIVETYPE PrimType;
	DWORD             dwLastPixel;
} LAST_PIXEL_TESTS, *LPLAST_PIXEL_TESTS;


__declspec(selectany) LAST_PIXEL_TESTS LastPixelTests[D3DMQA_LASTPIXELTEST_COUNT] = {
//
// |  D3DMPRIMITIVETYPE  |  D3DMRS_LASTPIXEL  |
// |                     |                    |
// |                     |                    |
// +---------------------+--------------------+
{         D3DMPT_LINELIST,               FALSE},
{         D3DMPT_LINELIST,                TRUE},
{        D3DMPT_LINESTRIP,               FALSE},
{        D3DMPT_LINESTRIP,                TRUE},
};
