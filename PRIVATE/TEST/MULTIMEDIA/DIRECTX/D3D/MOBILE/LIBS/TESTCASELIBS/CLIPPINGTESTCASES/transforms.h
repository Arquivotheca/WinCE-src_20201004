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

//
// For the purposes of this, the aspect ratio is assumed at 1.0f, and
// a left-handed perspective projection is assumed.
//
typedef struct _PERSPECTIVE_PROJECTION_SPECS {
	float fFOV;
	float fNearZ;
	float fFarZ;
} PERSPECTIVE_PROJECTION_SPECS;

//
// Defaults used for perspective projection; useful at SetTransform-time, and 
// during geometry generation
//
#define D3DQA_PI ((FLOAT)  3.141592654f)
static PERSPECTIVE_PROJECTION_SPECS ProjSpecs = {D3DQA_PI / 4.0f, 1.0f, 25.0f};

//
// Invoke SetTransform with the specified defaults
//
HRESULT SetTransformDefaults(LPDIRECT3DMOBILEDEVICE pDevice);

