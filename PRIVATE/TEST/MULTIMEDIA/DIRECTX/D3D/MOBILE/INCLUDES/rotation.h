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
// This enumeration specifies the various rotation types that
// are commonly used in Direct3D testing.
//
typedef enum _D3DQA_ROTTYPE {
D3DQA_ROTTYPE_NONE = 0,
D3DQA_ROTTYPE_X,
D3DQA_ROTTYPE_Y,
D3DQA_ROTTYPE_Z,

D3DQA_ROTTYPE_XY,
D3DQA_ROTTYPE_XZ,
D3DQA_ROTTYPE_YX,
D3DQA_ROTTYPE_YZ,
D3DQA_ROTTYPE_ZX,
D3DQA_ROTTYPE_ZY,

D3DQA_ROTTYPE_XYZ,
D3DQA_ROTTYPE_XZY,
D3DQA_ROTTYPE_YXZ,
D3DQA_ROTTYPE_YZX,
D3DQA_ROTTYPE_ZXY,
D3DQA_ROTTYPE_ZYX
} D3DQA_ROTTYPE;

//
// For convenience in using the above rotation enumeration,
// several defines are provided that describe the various
// ranges within the enumeration.
//
#define D3DQA_FIRST_ROTTYPE 1 // Excludes ROTTYPE_NONE; which is a NOP.
#define D3DQA_LAST_ROTTYPE 15
#define D3DQA_FIRST_ROTTYPE_1ROT 1
#define D3DQA_LAST_ROTTYPE_1ROT 3
#define D3DQA_FIRST_ROTTYPE_2ROT 4
#define D3DQA_LAST_ROTTYPE_2ROT 9
#define D3DQA_FIRST_ROTTYPE_3ROT 10
#define D3DQA_LAST_ROTTYPE_3ROT 15
#define D3DQA_NUM_ROTTYPE 16


//
// The array below provides a textual description (e.g., for debug output) of the
// various rotation types.
//
__declspec(selectany) TCHAR D3DQA_ROTTYPE_NAMES[D3DQA_NUM_ROTTYPE][MAX_TRANSFORM_STRING_LEN] = {
_T("D3DQA_ROTTYPE_NONE"),
_T("D3DQA_ROTTYPE_X"),
_T("D3DQA_ROTTYPE_Y"),
_T("D3DQA_ROTTYPE_Z"),
_T("D3DQA_ROTTYPE_XY"),
_T("D3DQA_ROTTYPE_XZ"),
_T("D3DQA_ROTTYPE_YX"),
_T("D3DQA_ROTTYPE_YZ"),
_T("D3DQA_ROTTYPE_ZX"),
_T("D3DQA_ROTTYPE_ZY"),
_T("D3DQA_ROTTYPE_XYZ"),
_T("D3DQA_ROTTYPE_XZY"),
_T("D3DQA_ROTTYPE_YXZ"),
_T("D3DQA_ROTTYPE_YZX"),
_T("D3DQA_ROTTYPE_ZXY"),
_T("D3DQA_ROTTYPE_ZYX")
};

//
// Enumeration of valid axis.
//
typedef enum _D3DQA_ROTAXIS {
D3DQA_ROTAXIS_NONE = 0,
D3DQA_ROTAXIS_X,
D3DQA_ROTAXIS_Y,
D3DQA_ROTAXIS_Z
} D3DQA_ROTAXIS;


//
// The array below provides a logical breakdown of the order and type that is
// intended for each D3DQA_ROTTYPE.  Although this _is_ a declaration, it is
// intentionally stored in this header file, given the direct relationship with
// the D3DQA_ROTTYPE enumeration.
//
// To combat "duplicate identifier" errors (in the case that the header file is
// included by more than one source file of the same binary), __declspec(selectany) is
// used.
//
#define D3DQA_NUM_AXIS 3
typedef D3DQA_ROTAXIS ROTSPEC[D3DQA_NUM_AXIS];
__declspec(selectany) ROTSPEC ROTORDER[] =
{
	{ D3DQA_ROTAXIS_NONE, D3DQA_ROTAXIS_NONE, D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_NONE
	{ D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_NONE, D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_X
	{ D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_NONE, D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_Y,
	{ D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_NONE, D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_Z,
	{ D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_XY,
	{ D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_XZ,
	{ D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_YX,
	{ D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_YZ,
	{ D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_ZX,
	{ D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_NONE }, // D3DQA_ROTTYPE_ZY,
	{ D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_Z    }, // D3DQA_ROTTYPE_XYZ,
	{ D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_Y    }, // D3DQA_ROTTYPE_XZY,
	{ D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_Z    }, // D3DQA_ROTTYPE_YXZ,
	{ D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_X    }, // D3DQA_ROTTYPE_YZX,
	{ D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_X,    D3DQA_ROTAXIS_Y    }, // D3DQA_ROTTYPE_ZXY,
	{ D3DQA_ROTAXIS_Z,    D3DQA_ROTAXIS_Y,    D3DQA_ROTAXIS_X    }  // D3DQA_ROTTYPE_ZYX
};


//
// The array below provides a specification of "inverse" rotations for each enumerator
// of the D3DQA_ROTTYPE enumeration.  For example, a rotation about the X axis followed
// by a rotation about the Y axis is specified as an inverse to a rotation about the
// Y axis followed by a rotation about the X axis.  Although this _is_ a declaration, it is
// intentionally stored in this header file, given the direct relationship with
// the D3DQA_ROTTYPE enumeration.
//
// To combat "duplicate identifier" errors (in the case that the header file is
// included by more than one source file of the same binary), __declspec(selectany) is
// used.
//
__declspec(selectany) D3DQA_ROTTYPE ROTINVERSE[] =
{
	{ D3DQA_ROTTYPE_NONE }, // D3DQA_ROTTYPE_NONE
	{ D3DQA_ROTTYPE_X    }, // D3DQA_ROTTYPE_X
	{ D3DQA_ROTTYPE_Y    }, // D3DQA_ROTTYPE_Y,
	{ D3DQA_ROTTYPE_Z    }, // D3DQA_ROTTYPE_Z,
	{ D3DQA_ROTTYPE_YX   }, // D3DQA_ROTTYPE_XY,
	{ D3DQA_ROTTYPE_ZX   }, // D3DQA_ROTTYPE_XZ,
	{ D3DQA_ROTTYPE_XY   }, // D3DQA_ROTTYPE_YX,
	{ D3DQA_ROTTYPE_ZY   }, // D3DQA_ROTTYPE_YZ,
	{ D3DQA_ROTTYPE_XZ   }, // D3DQA_ROTTYPE_ZX,
	{ D3DQA_ROTTYPE_YZ   }, // D3DQA_ROTTYPE_ZY,
	{ D3DQA_ROTTYPE_ZYX  }, // D3DQA_ROTTYPE_XYZ,
	{ D3DQA_ROTTYPE_YZX  }, // D3DQA_ROTTYPE_XZY,
	{ D3DQA_ROTTYPE_ZXY  }, // D3DQA_ROTTYPE_YXZ,
	{ D3DQA_ROTTYPE_XZY  }, // D3DQA_ROTTYPE_YZX,
	{ D3DQA_ROTTYPE_YXZ  }, // D3DQA_ROTTYPE_ZXY,
	{ D3DQA_ROTTYPE_XYZ  }  // D3DQA_ROTTYPE_ZYX
};			   
			   

//
// Because it is not feasable to exercise the full range of possible values for 
// various rotation-based tests; a reasonable selection of pre-defined (hard-coded)
// angles is provided by this header file.
//
#define D3DQA_NUM_SAMPLE_ROT_ANGLES 8
typedef enum _D3DQA_SAMPLE_ROT_ANGLES {
	D3DQA_ROT0 = 0,
	D3DQA_ROT45,
	D3DQA_ROT90, 
	D3DQA_ROT135,
	D3DQA_ROT180,
	D3DQA_ROT225,
	D3DQA_ROT270,
	D3DQA_ROT315
} D3DQA_SAMPLE_ROT_ANGLES;

__declspec(selectany) float ROTANGLES[] = {
	(0.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT0
	(1.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT45 
	(2.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT90,
	(3.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT135
	(4.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT180
	(5.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT225
	(6.0f*(D3DQA_PI / 4.0f)), // D3DQA_ROT270
	(7.0f*(D3DQA_PI / 4.0f))  // D3DQA_ROT315
};


//
// Generates a rotation matrix, of the specified type and angles, and also
// generates the inverse matrix for the rotation.
//
bool GetRotationInverses(D3DQA_ROTTYPE rType,
	 		             const float fXRot,
					     const float fYRot,
					     const float fZRot,
						 D3DMMATRIX* const RotMatrix,
						 D3DMMATRIX* const RotMatrixInverse);


//
// Generates a rotation matrix, of the specified type and angles.
//
bool GetRotation(D3DQA_ROTTYPE rType,
                 const float fXRot,
                 const float fYRot,
                 const float fZRot,
                 D3DMMATRIX* const RotMatrix);

//
// Given an enumerator that specifies a rotation type, determine
// if that rotation type has any rotation about the specified
// axis.
//
bool UsesXRot(D3DQA_ROTTYPE Rot);
bool UsesYRot(D3DQA_ROTTYPE Rot);
bool UsesZRot(D3DQA_ROTTYPE Rot);

//
// Indicates whether a specified rotation includes rotation
// about a particular axis.
//
bool UsesAxis(D3DQA_ROTAXIS RotAxis, D3DQA_ROTTYPE RotType);


//
// Indicates the number of axis that are involved in a particular
// rotation type
//
HRESULT GetAxisCount(PUINT puiAxis, D3DQA_ROTTYPE RotType);
