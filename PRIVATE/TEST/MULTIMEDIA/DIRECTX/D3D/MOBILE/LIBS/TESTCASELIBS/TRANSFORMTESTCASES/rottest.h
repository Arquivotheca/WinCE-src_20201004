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
#include <tux.h>

#define D3DQA_ROTFACTOR_MIN 0.0f
#define D3DQA_ROTFACTOR_MAX (D3DQA_PI * 2.0f)

TESTPROCAPI RotationTest(LPDIRECT3DMOBILEDEVICE pDevice,
                         D3DMTRANSFORMSTATETYPE TransMatrix,
                         D3DQA_ROTTYPE RotType,
                         UINT uiFrameNumber,
                         D3DMFORMAT Format,
                         UINT uiTestID, 
                         WCHAR *wszImageDir,
                         UINT uiInstanceHandle,
                         HWND hWnd,
                         LPTESTCASEARGS pTestCaseArgs);
