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


D3DMMATRIX* D3DMatrixPerspectiveFovLH( D3DMMATRIX *pOut, float fovy, float aspect, float zn, float zf );
D3DMMATRIX* D3DMatrixOrthoLH( D3DMMATRIX *pOut, float w, float h, float zn, float zf );
D3DMMATRIX* D3DMatrixLookAtLH( D3DMMATRIX *pOut, const D3DMVECTOR *pEye, const D3DMVECTOR *pAt, const D3DMVECTOR *pUp );
D3DMMATRIX* D3DMatrixIdentity( D3DMMATRIX *pOut );
D3DMMATRIX* D3DMatrixMultiply( D3DMMATRIX *pOut, const D3DMMATRIX *pM1, const D3DMMATRIX *pM2 );
D3DMMATRIX* D3DMatrixScaling( D3DMMATRIX *pOut, float sx, float sy, float sz );
D3DMMATRIX* D3DMatrixTranslation( D3DMMATRIX *pOut, float x, float y, float z );
