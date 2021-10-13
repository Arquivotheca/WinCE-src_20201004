//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

// Intended to be included from qamath.h


//
// This enumeration specifies the various scale types that
// are commonly used in Direct3D testing.
//
typedef enum _D3DQA_SCALETYPE {
D3DQA_SCALETYPE_NONE = 0,
D3DQA_SCALETYPE_X,
D3DQA_SCALETYPE_Y,
D3DQA_SCALETYPE_Z,

D3DQA_SCALETYPE_XY,
D3DQA_SCALETYPE_XZ,
D3DQA_SCALETYPE_YZ,

D3DQA_SCALETYPE_XYZ
} D3DQA_SCALETYPE;


//
// For convenience in using the above scale enumeration,
// several defines are provided that describe the various
// ranges within the enumeration.
//
#define D3DQA_FIRST_SCALETYPE 1 // Excludes SCALETYPE_NONE; which is a NOP.
#define D3DQA_LAST_SCALETYPE 7
#define D3DQA_NUM_SCALETYPE 8 // Includes SCALETYPE_NONE


//
// The array below provides a textual description (e.g., for debug output) of the
// various scale types.
//
__declspec(selectany) TCHAR D3DQA_SCALETYPE_NAMES[D3DQA_NUM_SCALETYPE][MAX_TRANSFORM_STRING_LEN] = {
_T("D3DQA_SCALETYPE_NONE"),
_T("D3DQA_SCALETYPE_X"),
_T("D3DQA_SCALETYPE_Y"),
_T("D3DQA_SCALETYPE_Z"),
_T("D3DQA_SCALETYPE_XY"),
_T("D3DQA_SCALETYPE_XZ"),
_T("D3DQA_SCALETYPE_YZ"),
_T("D3DQA_SCALETYPE_XYZ")
};

//
// Enumeration of valid axis.
//
typedef enum _D3DQA_SCALEAXIS {
D3DQA_SCALEAXIS_NONE = 0,
D3DQA_SCALEAXIS_X,
D3DQA_SCALEAXIS_Y,
D3DQA_SCALEAXIS_Z
} D3DQA_SCALEAXIS;

//
// Generates a scale matrix, of the specified type
//
bool GetScale(D3DQA_SCALETYPE sType,
              const float fXScale,
              const float fYScale,
              const float fZScale,
              D3DMMATRIX* const ScaleMatrix);


//
// Indicates whether a specified scale includes scaling
// for a particular axis.
//
bool UsesAxis(D3DQA_SCALEAXIS ScaleAxis, D3DQA_SCALETYPE ScaleType);


//
// Indicates the number of axis that are involved in a particular
// scale type
//
HRESULT GetAxisCount(PUINT puiAxis, D3DQA_SCALETYPE ScaleType);
