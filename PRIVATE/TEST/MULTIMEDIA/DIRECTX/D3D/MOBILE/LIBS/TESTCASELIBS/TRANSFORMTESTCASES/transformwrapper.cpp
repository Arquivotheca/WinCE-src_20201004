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
#include "TransformWrapper.h"
#include <tux.h>
#include "ShearTest.h"
#include "ScaleTest.h"
#include "TranslateTest.h"
#include "RotTest.h"
#include "DebugOutput.h"

//
// TransformTest
//
//   Render and capture a single test case for transformations.
//
// Arguments:
//   
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:
//
//   TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
//
TESTPROCAPI TransformTest(LPTESTCASEARGS pTestCaseArgs)
{

	D3DMTRANSFORMSTATETYPE TransformStateType;
	DWORD dwTableIndex;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		return TPR_ABORT;
	}

	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TRANSFORMTEST_BASE;

	//
	// Range checking
	//
	if ((pTestCaseArgs->dwTestIndex < D3DMQA_TRANSFORMTEST_BASE) || (pTestCaseArgs->dwTestIndex > D3DMQA_TRANSFORMTEST_MAX))
	{
		DebugOut(DO_ERROR, L"Invalid test index. Failing.");
		return TPR_FAIL;
	}

	TransformStateType = TransformTestCases[dwTableIndex].TransType;

	//
	// Choose test case subset
	//
	if ((dwTableIndex >= TRANSLATE_TABLE_MIN) &&
	    (dwTableIndex <= TRANSLATE_TABLE_MAX))
	{
		

		return TranslateTest(pTestCaseArgs->pDevice,                         // LPDIRECT3DMOBILEDEVICE pDevice,
		                     TransformStateType,                             // D3DMTRANSFORMSTATETYPE TransMatrix,
		                     TransformTestCases[dwTableIndex].TranslateType, // D3DQA_TRANSLATETYPE TranslateType
		                     TransformTestCases[dwTableIndex].uiFrameNum,    // UINT uiFrameNumber
		                     TransformTestCases[dwTableIndex].Format,        // D3DMFORMAT Format
		                     pTestCaseArgs->dwTestIndex,                     // UINT uiTestID
		                     pTestCaseArgs->pwszImageComparitorDir,          // WCHAR *wszImageDir
		                     pTestCaseArgs->uiInstance,                      // UINT uiInstanceHandle
		                     pTestCaseArgs->hWnd,                            // HWND hWnd
		                     pTestCaseArgs);                                 // LPTESTCASEARGS pTestCaseArgs
	}
	else if ((dwTableIndex >= SHEAR_TABLE_MIN) &&
	         (dwTableIndex <= SHEAR_TABLE_MAX))
	{
		

		return ShearTest(pTestCaseArgs->pDevice,                     // LPDIRECT3DMOBILEDEVICE pDevice,
		                 TransformStateType,                         // D3DMTRANSFORMSTATETYPE TransMatrix,
		                 TransformTestCases[dwTableIndex].ShearType, // D3DQA_SHEARTYPE ShearType
		                 TransformTestCases[dwTableIndex].uiFrameNum,// UINT uiFrameNumber
		                 TransformTestCases[dwTableIndex].Format,    // D3DMFORMAT Format
		                 pTestCaseArgs->dwTestIndex,                 // UINT uiTestID
		                 pTestCaseArgs->pwszImageComparitorDir,      // WCHAR *wszImageDir
		                 pTestCaseArgs->uiInstance,                  // UINT uiInstanceHandle
		                 pTestCaseArgs->hWnd,                        // HWND hWnd
		                 pTestCaseArgs);                             // LPTESTCASEARGS pTestCaseArgs
	}
	else if ((dwTableIndex >= ROTATE_TABLE_MIN) &&
	         (dwTableIndex <= ROTATE_TABLE_MAX))
	{
		

		return RotationTest(pTestCaseArgs->pDevice,                      // LPDIRECT3DMOBILEDEVICE pDevice,
		                    TransformStateType,                          // D3DMTRANSFORMSTATETYPE TransMatrix,
		                    TransformTestCases[dwTableIndex].RotType,    // D3DQA_ROTTYPE RotType
		                    TransformTestCases[dwTableIndex].uiFrameNum, // UINT uiFrameNumber
		                    TransformTestCases[dwTableIndex].Format,     // D3DMFORMAT Format
		                    pTestCaseArgs->dwTestIndex,                  // UINT uiTestID
		                    pTestCaseArgs->pwszImageComparitorDir,       // WCHAR *wszImageDir
		                    pTestCaseArgs->uiInstance,                   // UINT uiInstanceHandle
		                    pTestCaseArgs->hWnd,                         // HWND hWnd
		                    pTestCaseArgs);                              // LPTESTCASEARGS pTestCaseArgs
	}
	else if ((dwTableIndex >= SCALE_TABLE_MIN) &&
	         (dwTableIndex <= SCALE_TABLE_MAX))
	{
		

		return ScaleTest(pTestCaseArgs->pDevice,                      // LPDIRECT3DMOBILEDEVICE pDevice,
		                 TransformStateType,                          // D3DMTRANSFORMSTATETYPE TransMatrix,
		                 TransformTestCases[dwTableIndex].ScaleType,  // D3DQA_SCALETYPE ScaleType
		                 TransformTestCases[dwTableIndex].uiFrameNum, // UINT uiFrameNumber
		                 TransformTestCases[dwTableIndex].Format,     // D3DMFORMAT Format
		                 pTestCaseArgs->dwTestIndex,                  // UINT uiTestID
		                 pTestCaseArgs->pwszImageComparitorDir,       // WCHAR *wszImageDir
		                 pTestCaseArgs->uiInstance,                   // UINT uiInstanceHandle
		                 pTestCaseArgs->hWnd,                         // HWND hWnd
		                 pTestCaseArgs);                              // LPTESTCASEARGS pTestCaseArgs
	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index. Failing");
		return TPR_FAIL;
	}

}
