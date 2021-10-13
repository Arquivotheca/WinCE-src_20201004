//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once


D3DMMATRIX* D3DMatrixPerspectiveFovLH( D3DMMATRIX *pOut, float fovy, float aspect, float zn, float zf );
D3DMMATRIX* D3DMatrixOrthoLH( D3DMMATRIX *pOut, float w, float h, float zn, float zf );
D3DMMATRIX* D3DMatrixLookAtLH( D3DMMATRIX *pOut, const D3DMVECTOR *pEye, const D3DMVECTOR *pAt, const D3DMVECTOR *pUp );
D3DMMATRIX* D3DMatrixIdentity( D3DMMATRIX *pOut );
D3DMMATRIX* D3DMatrixMultiply( D3DMMATRIX *pOut, const D3DMMATRIX *pM1, const D3DMMATRIX *pM2 );
D3DMMATRIX* D3DMatrixScaling( D3DMMATRIX *pOut, float sx, float sy, float sz );
D3DMMATRIX* D3DMatrixTranslation( D3DMMATRIX *pOut, float x, float y, float z );
