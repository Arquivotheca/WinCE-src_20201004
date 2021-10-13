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
#include "QAMath.h"
#include <tchar.h>

#define _M(a) D3DM_MAKE_D3DMVALUE(a)

//
// ShearMatrix
//
//   Creates a shear suitable for shearing to planes that are orthogonally to
//   the standard-basis
//
// Arguments
//
//   ShearType:  Indicates which shear plane to use (all supported shear planes
//               are orthogonal to the main axis)
//
//   ShearFactor: floating point value that controls the extent of the shear
//
//   D3DMMATRIX*: This is an optional parameter that receives the inverse of the
//               computed shear matrix.
//
// Return Value
//
//   D3DMMATRIX:  Resulting shear matrix
//
D3DMMATRIX ShearMatrix(const D3DQA_SHEARTYPE ShearType, float ShearFactor, D3DMMATRIX *pShearInverse)
{
    //
    // Begin with the identity matrix
    //
	D3DMMATRIX ShearMat = {_M(1.0f), _M(0.0f), _M(0.0f), _M(0.0f),
		                   _M(0.0f), _M(1.0f), _M(0.0f), _M(0.0f),
		    	 		   _M(0.0f), _M(0.0f), _M(1.0f), _M(0.0f),
	 				       _M(0.0f), _M(0.0f), _M(0.0f), _M(1.0f)};

    if (( 0 ==(SHEARFACTORPOS[ShearType][0])) &&
        ( 0 ==(SHEARFACTORPOS[ShearType][1])))
    {
        //
        // If both shear-factor positions are listed as zero, the
        // shear type that has been passed must be an invalid one.
        //
        // The resultant matrices are identities.
        //
        if (NULL != pShearInverse)
        {
           *pShearInverse = ShearMat;
        }
        return ShearMat;
    }

    //
    // Perform the lookup in advance, purely for code readability
    //
    UINT uiShearFactorRow = SHEARFACTORPOS[ShearType][0];
    UINT uiShearFactorColumn = SHEARFACTORPOS[ShearType][1];

    //
    // Then, insert the shear factor at the position specified
    // by the table for shear-factor types
    //
    ShearMat.m[uiShearFactorRow][uiShearFactorColumn] = _M(ShearFactor);

    //
    // The inverse is differs from the shear matrix in only one
    // element:  the position that holds the shear factor.  For
    // this type of shear matrix, all that is needed to find the
    // inverse is to invert the shear factor.
    //
    if (NULL != pShearInverse)
    {
           *pShearInverse = ShearMat;
           pShearInverse->m[uiShearFactorRow][uiShearFactorColumn] *= -1;
    }

    return ShearMat;
}

