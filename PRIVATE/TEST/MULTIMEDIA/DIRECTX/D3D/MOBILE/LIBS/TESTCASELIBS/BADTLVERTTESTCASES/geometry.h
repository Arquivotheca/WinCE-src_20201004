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

#define _M(_v) D3DM_MAKE_D3DMVALUE(_v)

//
// PI/10 for trig
//
#define COS_PI_OV_10 0.95105651629515357211643933337938f
#define SIN_PI_OV_10 0.30901699437494742410229341718282f


//
// API for generating geometry
//
HRESULT MakeBadVertGeometry(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILEVERTEXBUFFER *ppVB, UINT uiIteration);

//
// Geometry structure/type/extents
//
#define D3DQA_BADTLVERTTEST_FVF D3DMFVF_XYZRHW_FLOAT | D3DMFVF_DIFFUSE
struct D3DQA_BADTLVERTTEST
{
    FLOAT x, y, z, rhw;
	DWORD Diffuse;
};
#define D3DQA_BADTLVERTTEST_NUMVERTS 3
#define D3DQA_BADTLVERTTEST_NUMPRIM 1

//
// Number of test sets specified in data structure
//
#define D3DQA_NUM_BADVERT_TESTSETS 8

//
// Frame counts
//
#define D3DQA_NUM_BADVERT_TESTCASES_SET1 10
#define D3DQA_NUM_BADVERT_TESTCASES_SET2 20
#define D3DQA_NUM_BADVERT_TESTCASES_SET3 5 
#define D3DQA_NUM_BADVERT_TESTCASES_SET4 5 
#define D3DQA_NUM_BADVERT_TESTCASES_SET5 15
#define D3DQA_NUM_BADVERT_TESTCASES_SET6 15
#define D3DQA_NUM_BADVERT_TESTCASES_SET7 15
#define D3DQA_NUM_BADVERT_TESTCASES_SET8 15
#define D3DQA_NUM_BADVERT_TESTCASES_TOTAL (D3DQA_NUM_BADVERT_TESTCASES_SET1+D3DQA_NUM_BADVERT_TESTCASES_SET2+D3DQA_NUM_BADVERT_TESTCASES_SET3+ \
                                           D3DQA_NUM_BADVERT_TESTCASES_SET4+D3DQA_NUM_BADVERT_TESTCASES_SET5+D3DQA_NUM_BADVERT_TESTCASES_SET6+ \
										   D3DQA_NUM_BADVERT_TESTCASES_SET7+D3DQA_NUM_BADVERT_TESTCASES_SET8)


//
// Data structure for specifying test cases
//
typedef struct _BADVERT_DESC {
	D3DMMATRIX MultPerStep;
	D3DMVECTOR InitialPos1;
	D3DMVECTOR InitialPos2;
	D3DMVECTOR InitialPos3;
	UINT uiNumSteps;
} BADVERT_DESC;


static const BADVERT_DESC BadVertexTestCases[D3DQA_NUM_BADVERT_TESTSETS] = {

	//
	// Violate XY viewport extents by scaling
	//
	{
		{
		_M( 1.1f), _M( 0.0f), _M( 0.0f), _M( 0.0f), // D3DMMATRIX MultPerStep
		_M( 0.0f), _M( 1.1f), _M( 0.0f), _M( 0.0f),
		_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
		_M( 0.0f), _M( 0.0f), _M( 0.0f), _M( 1.0f)
		},
		{_M(-1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 0.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET1            // UINT uiNumSteps
	},
	//
	// Violate XY viewport extents by rotation about Z axis (pi/10)
	//
	{
		{
		_M( COS_PI_OV_10), _M( -SIN_PI_OV_10), _M(0.0f), _M(0.0f),
		_M( SIN_PI_OV_10), _M(  COS_PI_OV_10), _M(0.0f), _M(0.0f),
		_M(         0.0f), _M(          0.0f), _M(1.0f), _M(0.0f),
		_M(		    0.0f), _M(          0.0f), _M(0.0f), _M(1.0f)
		},										   
		{_M(-1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 1.00f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 0.00f),_M(-1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET2	        // UINT uiNumSteps
	},										   

	//
	// Violate Z viewport extents by rotation about Y axis (pi/10)
	//
	{
		{
		_M(  COS_PI_OV_10), _M(          0.0f), _M( SIN_PI_OV_10), _M(0.0f),
		_M(          0.0f), _M(          1.0f), _M(          0.0f), _M(0.0f),
		_M( -SIN_PI_OV_10), _M(          0.0f), _M( COS_PI_OV_10), _M(0.0f),
		_M(          0.0f), _M(          0.0f), _M(          0.0f), _M(1.0f)
		},										   
		{_M(-2.00f),_M( 0.50f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 2.00f),_M( 0.50f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 0.00f),_M(-0.50f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET3	        // UINT uiNumSteps
	},										   

	//
	// Violate Y viewport extents by rotation about X axis (pi/10)
	//
	{
		{
		_M(         1.0f), _M(         0.0f), _M(          0.0f), _M(         0.0f),
		_M(         0.0f), _M( COS_PI_OV_10), _M( -SIN_PI_OV_10), _M(         0.0f),
		_M(         0.0f), _M( SIN_PI_OV_10), _M(  COS_PI_OV_10), _M(         0.0f), 
		_M(         0.0f), _M(         0.0f), _M(          0.0f), _M(         1.0f),
		},										   
		{_M( 0.50f),_M( 2.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 0.50f),_M(-2.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-0.50f),_M( 0.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET4	        // UINT uiNumSteps
	},										   


	//
	// Violate right viewport edge
	//
	{
		{
		_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f), // D3DMMATRIX MultPerStep
		_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 0.0f),
		_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
		_M( 0.0f), _M(-0.2f), _M( 0.0f), _M( 1.0f)   
		},										   
		{_M( 0.25f), _M(2.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M( 1.25f), _M(2.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M( 0.75f), _M(1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET5	        // UINT uiNumSteps
	},											   

	//
	// Violate left viewport edge
	//
	{
		{
		_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f), // D3DMMATRIX MultPerStep
		_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 0.0f),
		_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
		_M( 0.0f), _M(-0.2f), _M( 0.0f), _M( 1.0f)   
		},										   
		{_M(-1.25f),_M( 2.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M(-0.25f),_M( 2.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-0.75f),_M( 1.00f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET6	        // UINT uiNumSteps
	},											   

	//
	// Violate top viewport edge
	//
	{
		{
		_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f), // D3DMMATRIX MultPerStep
		_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 0.0f),
		_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
		_M( 0.2f), _M( 0.0f), _M( 0.0f), _M( 1.0f)   
		},										   
		{_M(-2.00f),_M( 1.25f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M(-1.00f),_M( 0.75f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-2.00f),_M( 0.25f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET7	        // UINT uiNumSteps
	},											   

	//
	// Violate bottom viewport edge
	//
	{
		{
		_M( 1.0f), _M( 0.0f), _M( 0.0f), _M( 0.0f), // D3DMMATRIX MultPerStep
		_M( 0.0f), _M( 1.0f), _M( 0.0f), _M( 0.0f),
		_M( 0.0f), _M( 0.0f), _M( 1.0f), _M( 0.0f),
		_M( 0.2f), _M( 0.0f), _M( 0.0f), _M( 1.0f)   
		},										   
		{_M(-2.00f),_M(-0.25f),_M(0.00f)},                      // D3DMVECTOR InitialPos1
		{_M(-1.00f),_M(-0.75f),_M(0.00f)},                      // D3DMVECTOR InitialPos2
		{_M(-2.00f),_M(-1.25f),_M(0.00f)},                      // D3DMVECTOR InitialPos3
		D3DQA_NUM_BADVERT_TESTCASES_SET8	        // UINT uiNumSteps
	}											   

};

static const struct _BADVERTTESTSET
{
	BADVERT_DESC *pDesc;
	UINT uiCount;
	UINT uiTotalCases;
} BadVertTestSet = 
{
	(BADVERT_DESC*)BadVertexTestCases,   // BADVERT_DESC *pDesc
	D3DQA_NUM_BADVERT_TESTSETS,          // UINT uiCount
	D3DQA_NUM_BADVERT_TESTCASES_TOTAL    // UINT uiTotalCases
};




