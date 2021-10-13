//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once
#include "Initializer.h"

class DriverCompTest : public D3DMInitializer {
private:
	
	//
	// Indicates whether or not the object is initialized
	//
	BOOL m_bInitSuccess;

	//
	// "Instance handle" to identify dumped images
	//
	UINT m_uiInstanceHandle;

	//
	// Paths for image dumps
	//
	WCHAR m_pwszImageComparitorDir[MAX_PATH];
	WCHAR m_pwszTestDependenciesDir[MAX_PATH];

public:

	DriverCompTest();
	HRESULT Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, UINT uiInstanceHandle, UINT uiTestCase);
	~DriverCompTest();

	//
	// A convenience function, that indicates whether the initialization
	// has completed successfully.
	//
	BOOL IsReady();

	//
	// Tests
	//
	INT ExecuteAlphaTestTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteClippingTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteColorFillTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteCopyRectsTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteCullTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteDepthBiasTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteDepthBufferTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteFogTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteFVFTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteLastPixelTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteLightTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteMipMapTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteOverDrawTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecutePrimRastTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteStretchRectTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteSwapChainTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteTransformTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteTexStageTest(DWORD dwTestIndex, UINT *puiNumFrames);
	INT ExecuteTexWrapTest(DWORD dwTestIndex, UINT *puiNumFrames);

};

