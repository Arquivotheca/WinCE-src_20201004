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
#include <windows.h>
#include <d3dm.h>
#include <tux.h>
#include "parseargs.h"

typedef struct _TESTCASEARGS
{
	LPDIRECT3DMOBILEDEVICE pDevice;
	HWND hWnd;
	PWCHAR pwszImageComparitorDir;
	UINT uiInstance;
	D3DMPRESENT_PARAMETERS *pParms;
	DWORD dwTestIndex;
} TESTCASEARGS, *LPTESTCASEARGS;

typedef struct _FRAMEDESCRIPTION
{
    INT FrameIndex;
    INT FrameResult;
    const TCHAR * FrameDescription;
} FRAMEDESCRIPTION, *LPFRAMEDESCRIPTION;

#include ".\TestCases\AddtlSwapChainsTestCases.h"
#include ".\TestCases\AlphaTestTestCases.h"
#include ".\TestCases\BadTLVertTestCases.h"
#include ".\TestCases\ClippingTestCases.h"
#include ".\TestCases\ColorFillTestCases.h"
#include ".\TestCases\CopyRectsTestCases.h"
#include ".\TestCases\CullingTestCases.h"
#include ".\TestCases\DepthBiasTestCases.h"
#include ".\TestCases\DepthBufferTestCases.h"
#include ".\TestCases\DXTnTestCases.h"
#include ".\TestCases\FogTestCases.h"
#include ".\TestCases\FVFTestCases.h"
#include ".\TestCases\LastPixelTestCases.h"
#include ".\TestCases\LightTestCases.h"
#include ".\TestCases\MultiInstTestCases.h"
#include ".\TestCases\MipMapTestCases.h"
#include ".\TestCases\OverDrawTestCases.h"
#include ".\TestCases\PrimRastTestCases.h"
#include ".\TestCases\ResManTestCases.h"
#include ".\TestCases\StretchRectTestCases.h"
#include ".\TestCases\SwapChainTestCases.h"
#include ".\TestCases\TexStageTestCases.h"
#include ".\TestCases\TexWrapTestCases.h"
#include ".\TestCases\TransformTestCases.h"

