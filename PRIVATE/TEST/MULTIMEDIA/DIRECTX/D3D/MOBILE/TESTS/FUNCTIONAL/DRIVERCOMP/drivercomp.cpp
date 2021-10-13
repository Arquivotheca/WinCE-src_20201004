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

	StringCchCopy(m_pwszImageComparitorDir, D3DQA_FILELENGTH,   pTestCaseArgs->pwchImageComparitorDir );
	StringCchCopy(m_pwszTestDependenciesDir, D3DQA_FILELENGTH,  pTestCaseArgs->pwchTestDependenciesDir);

	if (FAILED(D3DMInitializer::Init(D3DQA_PURPOSE_DEPTH_STENCIL_TEST,            // D3DQA_PURPOSE Purpose
	                                 pWindowArgs,                                 // LPWINDOW_ARGS pWindowArgs
	                                 pTestCaseArgs->pwchSoftwareDeviceFilename,   // TCHAR *ptchDriver
	                                 uiTestCase)))                                // UINT uiTestCase
	{
		DebugOut(DO_ERROR, _T("Aborting initialization, due to prior initialization failure."));
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
		DebugOut(DO_ERROR, _T("DriverCompTest is not ready."));
		return FALSE;
	}

	DebugOut(_T("DriverCompTest is ready."));
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
INT DriverCompTest::ExecuteFVFTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteFVFTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_FVFTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;


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
INT DriverCompTest::ExecutePrimRastTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecutePrimRastTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_PRIMRASTTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteClippingTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteClippingTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_CLIPPINGTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteFogTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteFogTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_FOGTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteCullTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteCullTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_CULLTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteMipMapTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteMipMapTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_MIPMAPTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteLightTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteLightTest."));

	INT Result = TPR_SKIP;

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};
	bool CheckPreviousStatus = false;

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_LIGHTTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

	if (NULL == ppFrameDescriptions)
	{
	    DebugOut(DO_ERROR, _T("ExecuteLightTest called with bad FrameDescriptions pointer. Aborting."));
	    return TPR_ABORT;
	}

	if (NULL == (*ppFrameDescriptions))
	{
        (*ppFrameDescriptions) = new FRAMEDESCRIPTION[*puiNumFrames];
        if (NULL == (*ppFrameDescriptions))
        {
            DebugOut(DO_ERROR, _T("Unable to allocate frame description array. Aborting."));
            return TPR_ABORT;
        }
    }
    else
    {
        CheckPreviousStatus = true;
    }

    for (UINT i = 0; i < *puiNumFrames; ++i)
    {
        if (CheckPreviousStatus && TPR_PASS != (*ppFrameDescriptions)[i].FrameResult)
        {
            continue;
        }
        (*ppFrameDescriptions)[i].FrameIndex = i;

        // The test function will assign this as appropriate. Should not be assigned to point to
        // a dynamically allocated value (it will not be freed).
        (*ppFrameDescriptions)[i].FrameDescription = NULL;

        (*ppFrameDescriptions)[i].FrameResult = LightTest(&TestCaseArgs, (*ppFrameDescriptions) + i);
        if ((*ppFrameDescriptions)[i].FrameResult > Result)
        {
            Result = (*ppFrameDescriptions)[i].FrameResult;
        }
    }

	return Result;

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
INT DriverCompTest::ExecuteTransformTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteTransformTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_TRANSFORMTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteDepthBufferTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteDepthBufferTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_DEPTHBUFTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteStretchRectTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteStretchRectTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_STRETCHRECTTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteCopyRectsTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteCopyRectsTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_COPYRECTSTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteColorFillTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteColorFillTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_COLORFILLTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteLastPixelTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteLastPixelTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_LASTPIXELTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteTexWrapTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteTexWrapTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_TEXWRAPTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteAlphaTestTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteAlphaTestTest."));

	INT Result = TPR_SKIP;

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};
	bool CheckPreviousStatus = false;
    
	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_ALPHATESTTEST_FRAMECOUNT;

	if (NULL == ppFrameDescriptions)
	{
	    DebugOut(DO_ERROR, _T("ExecuteAlphaTestTest called with bad FrameDescriptions pointer. Aborting."));
	    return TPR_ABORT;
	}

	if (NULL == (*ppFrameDescriptions))
	{
        (*ppFrameDescriptions) = new FRAMEDESCRIPTION[*puiNumFrames];
        if (NULL == (*ppFrameDescriptions))
        {
            DebugOut(DO_ERROR, _T("Unable to allocate frame description array. Aborting."));
            return TPR_ABORT;
        }
    }
    else
    {
        CheckPreviousStatus = true;
    }

    for (UINT i = 0; i < *puiNumFrames; ++i)
    {
        if (CheckPreviousStatus && TPR_PASS != (*ppFrameDescriptions)[i].FrameResult)
        {
            continue;
        }
        (*ppFrameDescriptions)[i].FrameIndex = i;

        // The test function will assign this as appropriate. Should not be assigned to point to
        // a dynamically allocated value (it will not be freed).
        (*ppFrameDescriptions)[i].FrameDescription = NULL;

        (*ppFrameDescriptions)[i].FrameResult = AlphaTestTest(&TestCaseArgs, (*ppFrameDescriptions) + i);
        if ((*ppFrameDescriptions)[i].FrameResult > Result)
        {
            Result = (*ppFrameDescriptions)[i].FrameResult;
        }
    }

	return Result;

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
INT DriverCompTest::ExecuteDepthBiasTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteDepthBiasTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_DEPTHBIASTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteSwapChainTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteSwapChainTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_SWAPCHAINTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteOverDrawTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteOverDrawTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_OVERDRAWTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

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
INT DriverCompTest::ExecuteTexStageTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteTexStageTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_TEXSTAGETEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

	return TexStageTest(&TestCaseArgs);

}

// 
// ExecuteDXTnTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteDXTnTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteDXTnTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_DXTNTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

	return DXTnTest(&TestCaseArgs);

}

// 
// ExecuteDXTnMipMapTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteDXTnMipMapTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteDXTnTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_DXTNMIPMAPTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

	return DXTnMipMapTest(&TestCaseArgs);

}

// 
// ExecuteAddtlSwapChainsTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteAddtlSwapChainsTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions)
{
	DebugOut(_T("Beginning ExecuteAddtlSwapChainsTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_ADDTLSWAPCHAINSTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

	return AddtlSwapChainsTest(&TestCaseArgs);

}

// 
// ExecuteMultiInstTest
//
// Arguments:
//  
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT DriverCompTest::ExecuteMultiInstTest(DWORD dwTestIndex, UINT *puiNumFrames, FRAMEDESCRIPTION ** ppFrameDescriptions, LPTEST_CASE_ARGS pTCArgs)
{
	DebugOut(_T("Beginning ExecuteMultiInstTest."));

	TESTCASEARGS TestCaseArgs = {m_pd3dDevice,m_hWnd,m_pwszImageComparitorDir,m_uiInstanceHandle,&m_d3dpp,dwTestIndex};

	//
	// On a fully featured driver, running this test case to completion,
	// this many frames will be generated:
	//
	*puiNumFrames = D3DMQA_MULTIINSTTEST_FRAMECOUNT;
	(*ppFrameDescriptions) = NULL;

	return MultiInstTest(&TestCaseArgs, pTCArgs);

}

