//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#define INITGUID 1

#include "DriverComp.h"
#include "KatoUtils.h"
#include "ImageManagement.h"
#include "TestCases.h"
#include "DebugOutput.h"
#include <tux.h>
#include <tchar.h>
#include <stdio.h>

#include "TestIDs.h"


//
//  DriverCompTest
//
//    Constructor for DriverCompTest class; however, most initialization is deferred.
//
//  Arguments:
//
//    none
//
//  Return Value:
//
//    none
//
DriverCompTest::DriverCompTest()
{
	m_bInitSuccess = FALSE;
}

//
// DriverCompTest
//
//   Initialize DriverCompTest object
//
// Arguments:
//
//   UINT uiAdapterOrdinal: D3DM Adapter ordinal
//   WCHAR *pSoftwareDevice: Pointer to wide character string identifying a D3DM software device to instantiate
//   WCHAR* wszImageDir: Directory to dump images to (screen captures)
//   UINT uiInstanceHandle: Handle to use in image dump filenames
//   UINT uiTestCase:  Test case ordinal (to be displayed in window)
//
// Return Value
//
//   HRESULT indicates success or failure
//
HRESULT DriverCompTest::Init(LPTEST_CASE_ARGS pTestCaseArgs, LPWINDOW_ARGS pWindowArgs, UINT uiInstanceHandle, UINT uiTestCase)
{
	m_uiInstanceHandle = uiInstanceHandle;

	wcscpy(m_pwszImageComparitorDir,  pTestCaseArgs->pwchImageComparitorDir );
	wcscpy(m_pwszTestDependenciesDir, pTestCaseArgs->pwchTestDependenciesDir);

	if (FAILED(D3DMInitializer::Init(D3DQA_PURPOSE_DEPTH_STENCIL_TEST,            // D3DQA_PURPOSE Purpose
	                                 pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
	                                 pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
	                                 uiTestCase)))                                // UINT uiTestCase
	{
		DebugOut(_T("Aborting initialization, due to prior initialization failure.\n"));
		return E_FAIL;
	}

	m_bInitSuccess = TRUE;
	return S_OK;
}

//
//  IsReady
//
//    Accessor method for "initialization state" of DriverCompTest object.
//  
//  Return Value:
//  
//    BOOL:  TRUE if the object is initialized; FALSE if it is not.
//
BOOL DriverCompTest::IsReady()
{
	if (FALSE == m_bInitSuccess)
	{
		DebugOut(_T("DriverCompTest is not ready.\n"));
		return FALSE;
	}

	DebugOut(_T("DriverCompTest is ready.\n"));
	return D3DMInitializer::IsReady();
}


// 
// ~DriverCompTest
//
//  Destructor for DriverCompTest.  Currently; there is nothing
//  to do here.
//
DriverCompTest::~DriverCompTest()
{
	return; // Success
}

// 
// ExecuteFVFTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteFVFTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteFVFTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_FVFTEST_FRAMECOUNT;


	return FVFTest(&TestCaseArgs);

}

// 
// ExecutePrimRastTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecutePrimRastTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecutePrimRastTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_PRIMRASTTEST_FRAMECOUNT;

	return PrimRastTest(&TestCaseArgs);

}

// 
// ExecuteClippingTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteClippingTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteClippingTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_CLIPPINGTEST_FRAMECOUNT;

	return ClipTest(&TestCaseArgs);

}

// 
// ExecuteFogTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteFogTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteFogTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_FOGTEST_FRAMECOUNT;

	return FogTest(&TestCaseArgs);

}

// 
// ExecuteCullTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteCullTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteCullTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_CULLTEST_FRAMECOUNT;

	return CullPrimTest(&TestCaseArgs);

}


// 
// ExecuteMipMapTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteMipMapTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteMipMapTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_MIPMAPTEST_FRAMECOUNT;

	return MipMapTest(&TestCaseArgs);

}

// 
// ExecuteLightTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteLightTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteLightTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_LIGHTTEST_FRAMECOUNT;

	return LightTest(&TestCaseArgs);

}


// 
// ExecuteTransformTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteTransformTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteTransformTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_TRANSFORMTEST_FRAMECOUNT;

	return TransformTest(&TestCaseArgs);

}

// 
// ExecuteDepthBufferTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteDepthBufferTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteDepthBufferTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_DEPTHBUFTEST_FRAMECOUNT;

	return DepthBufferTest(&TestCaseArgs);

}

// 
// ExecuteStretchRectTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteStretchRectTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteStretchRectTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_STRETCHRECTTEST_FRAMECOUNT;

	return StretchRectTest(&TestCaseArgs);

}

// 
// ExecuteCopyRectsTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteCopyRectsTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteCopyRectsTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_COPYRECTSTEST_FRAMECOUNT;

	return CopyRectsTest(&TestCaseArgs);

}
// 
// ExecuteColorFillTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteColorFillTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteColorFillTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_COLORFILLTEST_FRAMECOUNT;

	return ColorFillTest(&TestCaseArgs);

}

// 
// ExecuteLastPixelTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteLastPixelTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteLastPixelTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_LASTPIXELTEST_FRAMECOUNT;

	return LastPixelTest(&TestCaseArgs);

}

// 
// ExecuteTexWrapTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteTexWrapTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteTexWrapTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_TEXWRAPTEST_FRAMECOUNT;

	return TexWrapTest(&TestCaseArgs);

}

// 
// ExecuteAlphaTestTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteAlphaTestTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteAlphaTestTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_ALPHATESTTEST_FRAMECOUNT;

	return AlphaTestTest(&TestCaseArgs);

}

// 
// ExecuteDepthBiasTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteDepthBiasTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteDepthBiasTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_DEPTHBIASTEST_FRAMECOUNT;

	return DepthBiasTest(&TestCaseArgs);

}


// 
// ExecuteSwapChainTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteSwapChainTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteSwapChainTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_SWAPCHAINTEST_FRAMECOUNT;

	return SwapChainTest(&TestCaseArgs);

}


// 
// ExecuteOverDrawTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteOverDrawTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteOverDrawTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_OVERDRAWTEST_FRAMECOUNT;

	return OverDrawTest(&TestCaseArgs);

}

// 
// ExecuteTexStageTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteTexStageTest(DWORD dwTestIndex, UINT *puiNumFrames)
{
	DebugOut(_T("Beginning ExecuteTexStageTest.\n"));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_TEXSTAGETEST_FRAMECOUNT;

	return TexStageTest(&TestCaseArgs);

}

