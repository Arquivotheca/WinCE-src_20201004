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
#include <windows.h>
#include "QAMath.h"
#include <math.h>

#define _M(a) D3DM_MAKE_D3DMVALUE(a)

//
// UsesAxis
//
//   This is a utility function for use in conjunction with D3DQA_TRANSLATETYPE.  Given
//   any valid translation type; this function will determine whether or not the
//   specified translation type uses the specified axis.
//
// Return Value:
//
//   bool:  true if the specified translation includes a translation about the specified axis
//
bool UsesAxis(D3DQA_TRANSLATEAXIS TranslateAxis, D3DQA_TRANSLATETYPE TranslateType)
{
	switch(TranslateAxis)
	{
	case D3DQA_TRANSLATEAXIS_X:
		switch(TranslateType)
		{
		case D3DQA_TRANSLATETYPE_X:
		case D3DQA_TRANSLATETYPE_XY:
		case D3DQA_TRANSLATETYPE_XZ:
		case D3DQA_TRANSLATETYPE_XYZ:
			return true;

		case D3DQA_TRANSLATETYPE_Y:
		case D3DQA_TRANSLATETYPE_Z:
		case D3DQA_TRANSLATETYPE_YZ:
		default:
			return false;
		}
	case D3DQA_TRANSLATEAXIS_Y:
		switch(TranslateType)
		{
		case D3DQA_TRANSLATETYPE_Y:
		case D3DQA_TRANSLATETYPE_XY:
		case D3DQA_TRANSLATETYPE_YZ:
		case D3DQA_TRANSLATETYPE_XYZ:
			return true;

		case D3DQA_TRANSLATETYPE_X:
		case D3DQA_TRANSLATETYPE_Z:
		case D3DQA_TRANSLATETYPE_XZ:
		default:
			return false;
		}
	case D3DQA_TRANSLATEAXIS_Z:
		switch(TranslateType)
		{
		case D3DQA_TRANSLATETYPE_Z:
		case D3DQA_TRANSLATETYPE_XZ:
		case D3DQA_TRANSLATETYPE_YZ:
		case D3DQA_TRANSLATETYPE_XYZ:
			return true;

		case D3DQA_TRANSLATETYPE_X:
		case D3DQA_TRANSLATETYPE_Y:
		case D3DQA_TRANSLATETYPE_XY:
		default:
			return false;
		}
	default:
		return false;
	}
}

//
// GetAxisCount
//
//   This is a utility function for use in conjunction with D3DQA_TRANSLATETYPE.  Given
//   any valid translation type; this function will determine the number of axis that
//   are involved.
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetAxisCount(PUINT puiAxis, D3DQA_TRANSLATETYPE TranslateType)
{
	if (NULL == puiAxis)
		return E_FAIL;

	*puiAxis = 0;

	if (UsesAxis(D3DQA_TRANSLATEAXIS_X, TranslateType))
		(*puiAxis)++;

	if (UsesAxis(D3DQA_TRANSLATEAXIS_Y, TranslateType))
		(*puiAxis)++;

	if (UsesAxis(D3DQA_TRANSLATEAXIS_Z, TranslateType))
		(*puiAxis)++;

	return S_OK;
}


//
// GetTranslate
//
//   Creates a matrix suitable for scaling.
//
bool GetTranslate(D3DQA_TRANSLATETYPE sType,
              const float fXTranslate,
              const float fYTranslate,
              const float fZTranslate,
              D3DMMATRIX* const TranslateMatrix)
{
	*TranslateMatrix = IdentityMatrix();

    TranslateMatrix->m[3][0] = _M(fXTranslate);
	TranslateMatrix->m[3][1] = _M(fYTranslate);
	TranslateMatrix->m[3][2] = _M(fZTranslate);

    return true;
}



