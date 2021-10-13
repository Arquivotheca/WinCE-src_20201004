//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "QAMath.h"
#include <math.h>

#define _M(a) D3DM_MAKE_D3DMVALUE(a)

//
// UsesAxis
//
//   This is a utility function for use in conjunction with D3DQA_SCALETYPE.  Given
//   any valid scale type; this function will determine whether or not the
//   specified scale type uses the specified axis.
//
// Return Value:
//
//   bool:  true if the specified scale includes a scale about the specified axis
//
bool UsesAxis(D3DQA_SCALEAXIS ScaleAxis, D3DQA_SCALETYPE ScaleType)
{
	switch(ScaleAxis)
	{
	case D3DQA_SCALEAXIS_X:
		switch(ScaleType)
		{
		case D3DQA_SCALETYPE_X:
		case D3DQA_SCALETYPE_XY:
		case D3DQA_SCALETYPE_XZ:
		case D3DQA_SCALETYPE_XYZ:
			return true;

		case D3DQA_SCALETYPE_Y:
		case D3DQA_SCALETYPE_Z:
		case D3DQA_SCALETYPE_YZ:
		default:
			return false;
		}
	case D3DQA_SCALEAXIS_Y:
		switch(ScaleType)
		{
		case D3DQA_SCALETYPE_Y:
		case D3DQA_SCALETYPE_XY:
		case D3DQA_SCALETYPE_YZ:
		case D3DQA_SCALETYPE_XYZ:
			return true;

		case D3DQA_SCALETYPE_X:
		case D3DQA_SCALETYPE_Z:
		case D3DQA_SCALETYPE_XZ:
		default:
			return false;
		}
	case D3DQA_SCALEAXIS_Z:
		switch(ScaleType)
		{
		case D3DQA_SCALETYPE_Z:
		case D3DQA_SCALETYPE_XZ:
		case D3DQA_SCALETYPE_YZ:
		case D3DQA_SCALETYPE_XYZ:
			return true;

		case D3DQA_SCALETYPE_X:
		case D3DQA_SCALETYPE_Y:
		case D3DQA_SCALETYPE_XY:
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
//   This is a utility function for use in conjunction with D3DQA_SCALETYPE.  Given
//   any valid scale type; this function will determine the number of axis that
//   are involved.
//
// Return Value:
//
//   HRESULT indicates success or failure
//
HRESULT GetAxisCount(PUINT puiAxis, D3DQA_SCALETYPE ScaleType)
{
	if (NULL == puiAxis)
		return E_FAIL;

	*puiAxis = 0;

	if (UsesAxis(D3DQA_SCALEAXIS_X, ScaleType))
		(*puiAxis)++;

	if (UsesAxis(D3DQA_SCALEAXIS_Y, ScaleType))
		(*puiAxis)++;

	if (UsesAxis(D3DQA_SCALEAXIS_Z, ScaleType))
		(*puiAxis)++;

	return S_OK;
}


//
// GetScale
//
//   Creates a matrix suitable for scaling.
//
bool GetScale(D3DQA_SCALETYPE sType,
              const float fXScale,
              const float fYScale,
              const float fZScale,
              D3DMMATRIX* const ScaleMatrix)
{
	*ScaleMatrix = IdentityMatrix();

    ScaleMatrix->m[0][0] = _M(fXScale);
	ScaleMatrix->m[1][1] = _M(fYScale);
	ScaleMatrix->m[2][2] = _M(fZScale);

    return true;
}

