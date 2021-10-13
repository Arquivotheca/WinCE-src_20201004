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
#include <tux.h>

typedef struct _TESTCASEARGS
{
	LPDIRECT3DMOBILEDEVICE pDevice;
	HWND hWnd;
	PWCHAR pwszImageComparitorDir;
	UINT uiInstance;
	D3DMPRESENT_PARAMETERS *pParms;
	DWORD dwTestIndex;
} TESTCASEARGS, *LPTESTCASEARGS;

#include ".\TestCases\AlphaTestTestCases.h"
#include ".\TestCases\BadTLVertTestCases.h"
#include ".\TestCases\ClippingTestCases.h"
#include ".\TestCases\ColorFillTestCases.h"
#include ".\TestCases\CopyRectsTestCases.h"
#include ".\TestCases\CullingTestCases.h"
#include ".\TestCases\DepthBiasTestCases.h"
#include ".\TestCases\DepthBufferTestCases.h"
#include ".\TestCases\FogTestCases.h"
#include ".\TestCases\FVFTestCases.h"
#include ".\TestCases\LastPixelTestCases.h"
#include ".\TestCases\LightTestCases.h"
#include ".\TestCases\MipMapTestCases.h"
#include ".\TestCases\OverDrawTestCases.h"
#include ".\TestCases\PrimRastTestCases.h"
#include ".\TestCases\ResManTestCases.h"
#include ".\TestCases\StretchRectTestCases.h"
#include ".\TestCases\SwapChainTestCases.h"
#include ".\TestCases\TexStageTestCases.h"
#include ".\TestCases\TexWrapTestCases.h"
#include ".\TestCases\TransformTestCases.h"

