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

// Intended to be included from qamath.h


//
// This enumeration specifies the various translation types
//
typedef enum _D3DQA_TRANSLATETYPE {
D3DQA_TRANSLATETYPE_NONE = 0,
D3DQA_TRANSLATETYPE_X,
D3DQA_TRANSLATETYPE_Y,
D3DQA_TRANSLATETYPE_Z,

D3DQA_TRANSLATETYPE_XY,
D3DQA_TRANSLATETYPE_XZ,
D3DQA_TRANSLATETYPE_YZ,

D3DQA_TRANSLATETYPE_XYZ
} D3DQA_TRANSLATETYPE;


//
// For convenience in using the above translation enumeration,
// several defines are provided that describe the various
// ranges within the enumeration.
//
#define D3DQA_FIRST_TRANSLATETYPE 1 // Excludes TRANSLATETYPE_NONE; which is a NOP.
#define D3DQA_LAST_TRANSLATETYPE 7
#define D3DQA_NUM_TRANSLATETYPE 8 // Includes TRANSLATETYPE_NONE


//
// The array below provides a textual description (e.g., for debug output) of the
// various translation types.
//
__declspec(selectany) TCHAR D3DQA_TRANSLATETYPE_NAMES[D3DQA_NUM_TRANSLATETYPE][MAX_TRANSFORM_STRING_LEN] = {
_T("D3DQA_TRANSLATETYPE_NONE"),
_T("D3DQA_TRANSLATETYPE_X"),
_T("D3DQA_TRANSLATETYPE_Y"),
_T("D3DQA_TRANSLATETYPE_Z"),
_T("D3DQA_TRANSLATETYPE_XY"),
_T("D3DQA_TRANSLATETYPE_XZ"),
_T("D3DQA_TRANSLATETYPE_YZ"),
_T("D3DQA_TRANSLATETYPE_XYZ")
};

//
// Enumeration of valid axis.
//
typedef enum _D3DQA_TRANSLATEAXIS {
D3DQA_TRANSLATEAXIS_NONE = 0,
D3DQA_TRANSLATEAXIS_X,
D3DQA_TRANSLATEAXIS_Y,
D3DQA_TRANSLATEAXIS_Z
} D3DQA_TRANSLATEAXIS;

//
// Generates a translation matrix, of the specified type
//
bool GetTranslate(D3DQA_TRANSLATETYPE sType,
              const float fXTranslate,
              const float fYTranslate,
              const float fZTranslate,
              D3DMMATRIX* const TranslateMatrix);


//
// Indicates whether a specified translation includes translation
// for a particular axis.
//
bool UsesAxis(D3DQA_TRANSLATEAXIS TranslateAxis, D3DQA_TRANSLATETYPE TranslateType);


//
// Indicates the number of axis that are involved in a particular
// translation type
//
HRESULT GetAxisCount(PUINT puiAxis, D3DQA_TRANSLATETYPE TranslateType);
