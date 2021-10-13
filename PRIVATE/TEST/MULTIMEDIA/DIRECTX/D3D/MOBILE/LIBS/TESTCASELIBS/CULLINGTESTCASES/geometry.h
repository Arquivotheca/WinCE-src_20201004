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
#include <d3dm.h>

#define _M(a) D3DM_MAKE_D3DMVALUE(a)

//
// cos and sin of fractional pi value
//
// PI divisor == 4.0f
//
#define COS_PI_FRAC 0.70710678118654752440084436210485f
#define SIN_PI_FRAC 0.70710678118654752440084436210485f

//
// API for generating geometry
//
INT MakeCullPrimGeometry(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB, UINT uiIteration);

//
// Geometry structure/type/extents
//
#define D3DQA_CULLPRIMTEST_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE
struct D3DQA_CULLPRIMTEST
{
    FLOAT x, y, z, rhw;
	DWORD Diffuse;
};
#define D3DQA_CULLPRIM_TEST_NUMVERTS 3
#define D3DQA_CULLPRIM_TEST_NUMPRIM 1
//
// Number of test sets specified in data structure
//
#define D3DQA_NUM_CULLPRIM_TESTSETS 9

//
// Frame/step counts
//
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET1 8
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET2 8
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET3 8 

#define D3DQA_NUM_CULLPRIM_TESTCASES_SET4 8
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET5 8
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET6 8 

#define D3DQA_NUM_CULLPRIM_TESTCASES_SET7 8
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET8 8
#define D3DQA_NUM_CULLPRIM_TESTCASES_SET9 8 

#define D3DQA_NUM_CULLPRIM_TESTCASES_TOTAL (D3DQA_NUM_CULLPRIM_TESTCASES_SET1+D3DQA_NUM_CULLPRIM_TESTCASES_SET2+D3DQA_NUM_CULLPRIM_TESTCASES_SET3+ \
                                            D3DQA_NUM_CULLPRIM_TESTCASES_SET4+D3DQA_NUM_CULLPRIM_TESTCASES_SET5+D3DQA_NUM_CULLPRIM_TESTCASES_SET6+ \
										    D3DQA_NUM_CULLPRIM_TESTCASES_SET7+D3DQA_NUM_CULLPRIM_TESTCASES_SET8+D3DQA_NUM_CULLPRIM_TESTCASES_SET9)


//
// Data structure for specifying test cases
//
typedef struct _CULLPRIM_DESC {
	D3DMMATRIX MultPerStep;
	D3DMVECTOR InitialPos1;
	D3DMVECTOR InitialPos2;
	D3DMVECTOR InitialPos3;
	D3DMCULL CullMode;
	UINT uiNumSteps;
} CULLPRIM_DESC;


static const CULLPRIM_DESC CullPrimTestCases[D3DQA_NUM_CULLPRIM_TESTSETS] = {

	//
	// D3DMCULL_NONE Test
	//
	// Rotate about an axis parallel with the X axis; covers:
	//
	//      * Verts in CCW orientation
	//      * Verts in CW orientation
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(         1.0f), _M(         0.0f), _M(          0.0f), _M(         0.0f),
		_M(         0.0f), _M(  COS_PI_FRAC), _M(  -SIN_PI_FRAC), _M(         0.0f),
		_M(         0.0f), _M(  SIN_PI_FRAC), _M(   COS_PI_FRAC), _M(         0.0f), 
		_M(         0.0f), _M(         0.0f), _M(          0.0f), _M(         1.0f),
		},										   
		{_M( 1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-1.00f),_M( 0.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_NONE,                              // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET1	        // UINT uiNumSteps
	},										   

	//
	// D3DMCULL_NONE Test
	//
	// Rotate about an axis parallel with the Y axis; covers:
	//
	//      * Verts in CCW orientation
	//      * Verts in CW orientation
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(   COS_PI_FRAC), _M(          0.0f), _M(   SIN_PI_FRAC), _M(0.0f),
		_M(          0.0f), _M(          1.0f), _M(          0.0f), _M(0.0f),
		_M(  -SIN_PI_FRAC), _M(          0.0f), _M(   COS_PI_FRAC), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},
		{_M(-1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_NONE,                              // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET2           // UINT uiNumSteps
	},

	//
	// D3DMCULL_NONE Test
	//
	// Rotate about an axis parallel with the Z axis; covers:
	//
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(   COS_PI_FRAC), _M(  -SIN_PI_FRAC), _M(          0.0f), _M(0.0f),
		_M(   SIN_PI_FRAC), _M(   COS_PI_FRAC), _M(          0.0f), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          1.0f), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},
		{_M(-1.00f),_M(0.00f),_M(-1.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.00f),_M(0.00f),_M( 1.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 1.00f),_M(0.00f),_M(-1.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_NONE,                              // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET3           // UINT uiNumSteps
	},


	//
	// D3DMCULL_CW Test
	//
	// Rotate about an axis parallel with the X axis; covers:
	//
	//      * Verts in CCW orientation
	//      * Verts in CW orientation
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(         1.0f), _M(         0.0f), _M(          0.0f), _M(         0.0f),
		_M(         0.0f), _M(  COS_PI_FRAC), _M(  -SIN_PI_FRAC), _M(         0.0f),
		_M(         0.0f), _M(  SIN_PI_FRAC), _M(   COS_PI_FRAC), _M(         0.0f), 
		_M(         0.0f), _M(         0.0f), _M(          0.0f), _M(         1.0f),
		},										   
		{_M( 1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-1.00f),_M( 0.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_CW,                                // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET1	        // UINT uiNumSteps
	},										   

	//
	// D3DMCULL_CW Test
	//
	// Rotate about an axis parallel with the Y axis; covers:
	//
	//      * Verts in CCW orientation
	//      * Verts in CW orientation
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(   COS_PI_FRAC), _M(          0.0f), _M(   SIN_PI_FRAC), _M(0.0f),
		_M(          0.0f), _M(          1.0f), _M(          0.0f), _M(0.0f),
		_M(  -SIN_PI_FRAC), _M(          0.0f), _M(   COS_PI_FRAC), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},
		{_M(-1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_CW,                                // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET2           // UINT uiNumSteps
	},

	//
	// D3DMCULL_CW Test
	//
	// Rotate about an axis parallel with the Z axis; covers:
	//
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(   COS_PI_FRAC), _M(  -SIN_PI_FRAC), _M(          0.0f), _M(0.0f),
		_M(   SIN_PI_FRAC), _M(   COS_PI_FRAC), _M(          0.0f), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          1.0f), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},
		{_M(-1.00f),_M(0.00f),_M(-1.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.00f),_M(0.00f),_M( 1.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 1.00f),_M(0.00f),_M(-1.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_CW,                                // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET3           // UINT uiNumSteps
	},


	//
	// D3DMCULL_CCW Test
	//
	// Rotate about an axis parallel with the X axis; covers:
	//
	//      * Verts in CCW orientation
	//      * Verts in CW orientation
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(         1.0f), _M(         0.0f), _M(          0.0f), _M(         0.0f),
		_M(         0.0f), _M(  COS_PI_FRAC), _M(  -SIN_PI_FRAC), _M(         0.0f),
		_M(         0.0f), _M(  SIN_PI_FRAC), _M(   COS_PI_FRAC), _M(         0.0f), 
		_M(         0.0f), _M(         0.0f), _M(          0.0f), _M(         1.0f),
		},										   
		{_M( 1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-1.00f),_M( 0.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_CCW,                               // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET1	        // UINT uiNumSteps
	},										   

	//
	// D3DMCULL_CCW Test
	//
	// Rotate about an axis parallel with the Y axis; covers:
	//
	//      * Verts in CCW orientation
	//      * Verts in CW orientation
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(   COS_PI_FRAC), _M(          0.0f), _M(   SIN_PI_FRAC), _M(0.0f),
		_M(          0.0f), _M(          1.0f), _M(          0.0f), _M(0.0f),
		_M(  -SIN_PI_FRAC), _M(          0.0f), _M(   COS_PI_FRAC), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},
		{_M(-1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 1.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_CCW,                               // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET2           // UINT uiNumSteps
	},

	//
	// D3DMCULL_CCW Test
	//
	// Rotate about an axis parallel with the Z axis; covers:
	//
	//      * Verts in colinear orientation (degenerate case)
	//
	{
		{
		_M(   COS_PI_FRAC), _M(  -SIN_PI_FRAC), _M(          0.0f), _M(0.0f),
		_M(   SIN_PI_FRAC), _M(   COS_PI_FRAC), _M(          0.0f), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          1.0f), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},
		{_M(-1.00f),_M(0.00f),_M(-1.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.00f),_M(0.00f),_M( 1.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 1.00f),_M(0.00f),_M(-1.00f)},                      // D3DMVECTOR InitialPos3
		D3DMCULL_CCW,                               // D3DMCULL CullMode;
		D3DQA_NUM_CULLPRIM_TESTCASES_SET3           // UINT uiNumSteps
	},
};

static const struct _CULLPRIMTESTSET
{
	CULLPRIM_DESC *pDesc;
	UINT uiCount;
	UINT uiTotalCases;
} CullPrimTestSet = 
{
	(CULLPRIM_DESC*)CullPrimTestCases,   // CULLPRIM_DESC *pDesc
	D3DQA_NUM_CULLPRIM_TESTSETS,         // UINT uiCount
	D3DQA_NUM_CULLPRIM_TESTCASES_TOTAL   // UINT uiTotalCases
};




