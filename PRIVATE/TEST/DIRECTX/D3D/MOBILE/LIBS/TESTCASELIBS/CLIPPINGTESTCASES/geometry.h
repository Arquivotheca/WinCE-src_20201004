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
#include <clippingtest.h>
#include "util.h"
#include "transforms.h"

//
// Generate geometry, and fill a vertex buffer with it, for
// the specified scenario
//
HRESULT SetupClipGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                          PERSPECTIVE_PROJECTION_SPECS *pProjection,
                          UINT uiMaxIter,
                          UINT uiCurIter,
                          PRIM_DIRECTION PrimDir,
                          TRANSLATE_DIR TranslateDir,
						  GEOMETRY_DESC *pDesc,
						  D3DMPRIMITIVETYPE PrimType);

