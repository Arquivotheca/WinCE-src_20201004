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

