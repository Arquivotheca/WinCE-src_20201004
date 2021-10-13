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
#include <tchar.h>
#include <d3dmtypes.h>

//
// Maximum string length for textual descriptions of transformations
//
#define MAX_TRANSFORM_STRING_LEN 25

#define D3DQA_PI ((FLOAT) 3.141592654f)
#define D3DQA_TWOPI (2 * D3DQA_PI)

//
// Specific transformation types
//
#include "rotation.h"
#include "shear.h"
#include "scale.h"
#include "translate.h"

//
// Numeric utilities
//
#include "fixedfloat.h"


//
// General matrix and vector utilities
//
D3DMVECTOR TransformVector(const D3DMVECTOR *v, const D3DMMATRIX *m);
D3DMVECTOR NormalizeVector(const D3DMVECTOR *v);
D3DMMATRIX ZeroMatrix(void);
D3DMMATRIX IdentityMatrix(void);
D3DMMATRIX MatrixMult(const D3DMMATRIX *a, const D3DMMATRIX *b);
D3DMMATRIX MatrixTranspose(const D3DMMATRIX *m);
D3DMMATRIX* D3DMatrixPerspectiveFovLH( D3DMMATRIX *pOut, float fovy, float aspect, float zn, float zf );
D3DMMATRIX* WINAPI D3DMatrixOrthoLH( D3DMMATRIX *pOut, float w, float h, float zn, float zf );

