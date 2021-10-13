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
	WCHAR m_pwszImageComparitorDir[D3DQA_FILELENGTH];
	WCHAR m_pwszTestDependenciesDir[D3DQA_FILELENGTH];

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
	INT ExecuteAddtlSwapChainsTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteAlphaTestTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteClippingTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteColorFillTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteCopyRectsTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteCullTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteDepthBiasTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteDepthBufferTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteDXTnMipMapTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteDXTnTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteFogTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteFVFTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteLastPixelTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteLightTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteMipMapTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteMultiInstTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions, LPTEST_CASE_ARGS pTCArgs);
	INT ExecuteOverDrawTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecutePrimRastTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteStretchRectTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteSwapChainTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteTransformTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteTexStageTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);
	INT ExecuteTexWrapTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions);

};

