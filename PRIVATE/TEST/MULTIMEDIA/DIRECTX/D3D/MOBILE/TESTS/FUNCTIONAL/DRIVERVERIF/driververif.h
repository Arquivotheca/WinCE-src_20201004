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
#include "Initializer.h"
#include "TestCases.h"

//
// Macros for obtaining components from D3DCOLORs
//
#define D3DQA_GETRED(_c) ((_c & 0x00FF0000) >> 16)
#define D3DQA_GETGREEN(_c) ((_c & 0x0000FF00) >> 8)
#define D3DQA_GETBLUE(_c) ((_c & 0x000000FF))

//
// Test constraints for ProcessVertices testing
//
#define D3DQA_PV_MINEXP -4
#define D3DQA_DEFAULT_MAX_DELTA 0.001f

//
// 1/16th sub-pixel accuracy, with a small additional tolerance
//

// Not to be used (use D3DQA_PV_ADDEND_STEP + m_fTestTolerance instead)
//#define D3DQA_MAX_DELTA_XY ((1.0f/16.0f) + 0.0001f)

#define D3DQA_PV_ADDEND_STEP  (1.0f/16.0f)
#define D3DQA_PV_MAX_ADDEND   1.0f

#define D3DQA_PV_MIN_NEAR 1.0f
#define D3DQA_PV_MAX_NEAR 50.0f
#define D3DQA_PV_NEAR_STEP 5.0f
#define D3DQA_PV_MAX_FAR 50.0f
#define D3DQA_PV_FAR_STEP 5.0f
#define D3DQA_PV_PROJ_XDIVS 10
#define D3DQA_PV_PROJ_YDIVS 10
#define D3DQA_PV_PROJ_ANGLES 10
#define D3DQA_PV_PROJ_VERTS 10

//
// Orthographic projection test
// 
#define D3DQA_PV_ORTH_MIN_WIDTH 50
#define D3DQA_PV_ORTH_MAX_WIDTH 200
#define D3DQA_PV_ORTH_WIDTH_STEP 50
#define D3DQA_PV_ORTH_MIN_HEIGHT 50
#define D3DQA_PV_ORTH_MAX_HEIGHT 200
#define D3DQA_PV_ORTH_HEIGHT_STEP 50

//
// Viewport test
//
#define D3DQA_PV_VPTEST_CLIP_NEAR 1.0f
#define D3DQA_PV_VPTEST_CLIP_FAR  100.0f

//
// There are three possible values for color sources
//
#define D3DQA_COLOR_SOURCES 3


class DriverVerifTest : public D3DMInitializer {
private:
	
	//
	// Indicates whether or not the object is initialized
	//
	BOOL m_bInitSuccess;

	//
	// Indicates the Tolerance level for the test case
	//
	FLOAT m_fTestTolerance;

	//
	// Utilities
	//
	HRESULT SetDegenerateViewAndProj();

public:

	DriverVerifTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, UINT uiTestCase);
	~DriverVerifTest();


	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();

	//
	// ProcessVertices tests
	//

	INT ExecuteTexTransRotate();
	INT ExecuteMultTest();
	INT ExecuteAddTest();
	INT ExecutePerspectiveProjTest();
	INT ExecuteOrthoProjTest();
	INT ExecuteViewportTest();
	INT ExecuteDiffuseMaterialSourceTest();
    INT ExecuteSpecularMaterialSourceTest();
	INT ExecuteSpecularPowerTest();
	INT ExecuteHalfwayVectorTest();
	INT ExecuteNonLocalViewerTest();
	INT ExecuteGlobalAmbientTest();
	INT ExecuteLocalAmbientTest();

	INT ExecuteNoStreamSourceTest();
	INT ExecuteStreamSourceTest();
	INT ExecuteDoNotCopyDataTest();
	INT ExecuteNotEnoughVertsTest();

	//
	// Blend Tests
	//
	INT ExecuteBlendTest(DWORD dwTestIndex);

	//
	// Stencil Tests
	//
	INT ExecuteStencilTest(DWORD dwTestIndex);

	//
	// Resource Manager Tests
	//
	INT ExecuteResManTest(DWORD dwTestIndex);
};

typedef struct _VertXYZFloat {
	float x,y,z;
} VertXYZFloat;
