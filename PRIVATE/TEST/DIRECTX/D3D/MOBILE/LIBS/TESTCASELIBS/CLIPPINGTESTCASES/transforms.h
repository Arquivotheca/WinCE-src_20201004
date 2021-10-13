//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

