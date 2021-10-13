//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "d3dmtypes.h"

//
// Matrix / vector operations for D3DMMATRIX, D3DMVECTOR of type D3DMFMT_D3DMVALUE_FLOAT
//

#define _M(a) D3DM_MAKE_D3DMVALUE(a)

//
// ZeroMatrix
//
//   Creates a matrix with all elements initialized to 0.0f.
//
D3DMMATRIX ZeroMatrix(void)
{
    D3DMMATRIX ret;
    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++) {
			ret.m[i][j] = _M(0.0f);
		}
	}
    return ret;
}

//
// IdentityMatrix
//
//   Creates a matrix with all elements initialized to 0.0f, except for the
//   members along the diagonal, which are set to 1.0f.
//
D3DMMATRIX IdentityMatrix(void)
{
    D3DMMATRIX ret;

    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++)	{
            ret.m[i][j] = _M(0.0f);
		}
		ret.m[i][i] = _M(1.0f);
	}
    return ret;
}


//
// MatrixMult
//
//   Returns the result of [a] * [b]
//
D3DMMATRIX MatrixMult(const D3DMMATRIX *a, const D3DMMATRIX *b)
{
	D3DMMATRIX ret = ZeroMatrix();

	for (int i=0; i<4; i++) {
		for (int j=0; j<4; j++) {
			for (int k=0; k<4; k++) {
				*(float*)&(ret.m[i][j]) += (*(float*)&(a->m[k][j])) * (*(float*)&(b->m[i][k]));
			}
		}
	}
	return ret;
}

D3DMVECTOR TransformVector(const D3DMVECTOR *v, const D3DMMATRIX *m)
{
	D3DMVECTOR Result;
	const float Vector[3] = {*(float*)&(v->x), *(float*)&(v->y), *(float*)&(v->z)};
	float	hvec[4];

	for (int i=0; i<4; i++) {
		hvec[i] = 0.0f;
		for (int j=0; j<4; j++) {
			if (j==3) {
				hvec[i] += *(float*)&(m->m[j][i]);
			} else {
				hvec[i] += Vector[j] * *(float*)&(m->m[j][i]);
			}
		}
	}

	Result.x = _M((float)hvec[0]/(float)hvec[3]);
	Result.y = _M((float)hvec[1]/(float)hvec[3]);
	Result.z = _M((float)hvec[2]/(float)hvec[3]);

	return Result;
}


//
// TransposeMatrix
//
//   Returns [M]' corresponding to matrix [M]
//
D3DMMATRIX MatrixTranspose(const D3DMMATRIX *m)
{
	D3DMMATRIX	ret;
	int			i, j;

    if (NULL == m)
        return IdentityMatrix();

	for (i=0; i<4; i++) {
		for (j=0; j<4; j++) {
			ret.m[i][j] = m->m[j][i];
		}
	}

	return ret;
}

//
// NormalizeVector
//
//   Forces a vector length of one, while retaining original direction
//
D3DMVECTOR NormalizeVector(const D3DMVECTOR *v)
{
    D3DMVECTOR ResultVect = { _M(0.0f), _M(0.0f), _M(0.0f) };
    double dDivisor;

    //
    // Handle the NULL pointer case gracefully.
    // 
    if (NULL == v)
        return ResultVect;

    //
    // dDivisor is the original vector length, which is the divisor
    // for the normalization operation.
    //
    dDivisor = sqrt(*(float*)&(v->x) * (*(float*)&(v->x)) +
                    *(float*)&(v->y) * (*(float*)&(v->y)) +
                    *(float*)&(v->z) * (*(float*)&(v->z)));

    //
    // Despite a float-based vector type; attempt to get a little
    // extra precision by using a double divisor, and doing the
    // division operation in double.
    //
    ResultVect = *v;
    ResultVect.x = _M((float)((double)ResultVect.x/dDivisor));
    ResultVect.y = _M((float)((double)ResultVect.y/dDivisor));
    ResultVect.z = _M((float)((double)ResultVect.z/dDivisor));

    return ResultVect;
}
