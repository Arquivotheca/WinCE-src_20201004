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
