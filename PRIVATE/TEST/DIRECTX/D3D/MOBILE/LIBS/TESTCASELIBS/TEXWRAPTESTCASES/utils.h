//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include "..\FVFTestCases\utils.h"

HRESULT SetTransforms(LPDIRECT3DMOBILEDEVICE pDevice);

/////////////////////////////////////////////////////////
//
// The below are defined in D3DMColorFillTests.lib
//
bool IsRefDriver(LPDIRECT3DMOBILEDEVICE pDevice);

HRESULT GetBestFormat(
    LPDIRECT3DMOBILEDEVICE pDevice,
    D3DMRESOURCETYPE       ResourceType,
    D3DMFORMAT           * pFormat);

bool IsPowerOfTwo(DWORD dwNum);
DWORD NearestPowerOfTwo(DWORD dwNum);
