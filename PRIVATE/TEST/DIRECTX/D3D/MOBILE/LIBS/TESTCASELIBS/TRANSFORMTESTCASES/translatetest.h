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
#include <tux.h>

#define D3DQA_TRANSLATEFACTOR_MIN -10.0f
#define D3DQA_TRANSLATEFACTOR_MAX 10.0f

TESTPROCAPI TranslateTest(LPDIRECT3DMOBILEDEVICE pDevice,
                          D3DMTRANSFORMSTATETYPE TransMatrix,
                          D3DQA_TRANSLATETYPE TranslateType,
                          UINT uiFrameNumber,
						  D3DMFORMAT Format,
                          UINT uiTestID, 
                          WCHAR *wszImageDir,
                          UINT uiInstanceHandle,
                          HWND hWnd,
                          LPTESTCASEARGS pTestCaseArgs);
