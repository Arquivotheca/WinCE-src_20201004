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
// This enumeration specifies shear types, orthogonal to the standard
// 3-D basis [ < 1 0 0 >, < 0 1 0 >, < 0 0 1 > ].  This test suite
// does not perform shearing orthogonal to arbitrary planes.
//
typedef enum _D3DQA_SHEARTYPE {
    D3DQA_SHEARTYPE_NONE = 0,
    D3DQA_SHEARTYPE_XY,
    D3DQA_SHEARTYPE_XZ,
    D3DQA_SHEARTYPE_YX,
    D3DQA_SHEARTYPE_YZ,
    D3DQA_SHEARTYPE_ZX,
    D3DQA_SHEARTYPE_ZY
} D3DQA_SHEARTYPE;

//
// For convenience in using the above shear enumeration,
// several defines are provided that describe the various
// ranges within the enumeration.
//
#define D3DQA_FIRST_SHEARTYPE 1 // Excludes SHEARTYPE_NONE; which is a NOP.
#define D3DQA_LAST_SHEARTYPE 6
#define D3DQA_NUM_SHEARTYPE 7 // Includes SHEARTYPE_NONE

//
// The array below provides a textual description (e.g., for debug output) of the
// various shear types.
//
__declspec(selectany) TCHAR D3DQA_SHEARTYPE_NAMES[D3DQA_NUM_SHEARTYPE][MAX_TRANSFORM_STRING_LEN] = {
_T("D3DQA_SHEARTYPE_NONE"),
_T("D3DQA_SHEARTYPE_XY"),
_T("D3DQA_SHEARTYPE_XZ"),
_T("D3DQA_SHEARTYPE_YX"),
_T("D3DQA_SHEARTYPE_YZ"),
_T("D3DQA_SHEARTYPE_ZX"),
_T("D3DQA_SHEARTYPE_ZY")
};

//
// The array below provides the "shear factor" row and column for a particular
// shear type.  This could easily be generated on-the-fly, but for the sake of
// clarity it is defined explicitly.
//
// The rows and columns in this table are to be considered as zero-based.
//
// Although this _is_ a declaration, it is intentionally stored in this header 
// file, given the direct relationship with the D3DQA_SHEARTYPE enumeration.
//
// To combat "duplicate identifier" errors (in the case that the header file is
// included by more than one source file of the same binary), __declspec(selectany) is
// used.
//
#define D3DQA_NUM_DIMENSIONS_SHEAR_MATRIX 2
__declspec(selectany) UINT SHEARFACTORPOS[D3DQA_NUM_SHEARTYPE][D3DQA_NUM_DIMENSIONS_SHEAR_MATRIX] =
{
//   |  ROW  | COLUMN |
//   +-------+--------+
	 {   0    ,   0   }, // D3DQA_SHEARTYPE_NONE 
	 {   0    ,   1   }, // D3DQA_SHEARTYPE_XY,
	 {   0    ,   2   }, // D3DQA_SHEARTYPE_XZ,
	 {   1    ,   0   }, // D3DQA_SHEARTYPE_YX,
	 {   1    ,   2   }, // D3DQA_SHEARTYPE_YZ,
	 {   2    ,   0   }, // D3DQA_SHEARTYPE_ZX,
	 {   2    ,   1   }  // D3DQA_SHEARTYPE_ZY
};

D3DMMATRIX ShearMatrix(const D3DQA_SHEARTYPE ShearType, float ShearFactor, D3DMMATRIX *pShearInverse = NULL);
