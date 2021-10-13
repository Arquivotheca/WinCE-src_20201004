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
#include "VerifTestCases.h"
#include "StencilCases.h"
#include <tux.h>
#include <tchar.h>
#include "DebugOutput.h"


TESTPROCAPI StencilTest(LPVERIFTESTCASEARGS pTestCaseArgs)
{
	DebugOut(_T("Beginning StencilTest."));

	//
	// Index into table of permutations
	//
	DWORD dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_STENCILTEST_BASE;
	DWORD dwStencilFuncIndex, dwStencilPassIndex, dwStencilMaskIndex;

	if (dwTableIndex < STENCILFUNCCONSTS.TestCaseCount)
	{
		// Use dwTableIndex as index into D3DMRS_STENCILFUNC table

		dwStencilFuncIndex = dwTableIndex;

		return StencilFuncTest(pTestCaseArgs->pDevice, // LPDIRECT3DMOBILEDEVICE pDevice,
		                       pTestCaseArgs->hWnd,    // HWND hWnd,
				               dwStencilFuncIndex);    // UINT uiTestPermutation

	}
	else if (dwTableIndex < (STENCILFUNCCONSTS.TestCaseCount + STENCILPASSCONSTS.TestCaseCount))
	{
		// Use (dwTableIndex - STENCILFUNCCONSTS.TestCaseCount) as index into D3DMRS_STENCILPASS/D3DMRS_STENCILFAIL table

		dwStencilPassIndex = dwTableIndex - STENCILFUNCCONSTS.TestCaseCount;

		return StencilPassTest(pTestCaseArgs->pDevice, // LPDIRECT3DMOBILEDEVICE pDevice,
		                       pTestCaseArgs->hWnd,    // HWND hWnd,
				               dwStencilPassIndex);    // UINT uiTestPermutation

	}
	else if (dwTableIndex < (STENCILFUNCCONSTS.TestCaseCount + STENCILPASSCONSTS.TestCaseCount + STENCILMASKCONSTS.TestCaseCount))
	{
		// Use (dwTableIndex - STENCILFUNCCONSTS.TestCaseCount - STENCILPASSCONSTS.TestCaseCount) as index into D3DMRS_STENCILMASK table

		dwStencilMaskIndex = dwTableIndex - STENCILFUNCCONSTS.TestCaseCount - STENCILPASSCONSTS.TestCaseCount;

		return StencilMaskTest(pTestCaseArgs->pDevice, // LPDIRECT3DMOBILEDEVICE pDevice,
		                       pTestCaseArgs->hWnd,    // HWND hWnd,
				               dwStencilMaskIndex);    // UINT uiTestPermutation

	}
	else
	{
		DebugOut(DO_ERROR, _T("Unknown test index. Aborting."));
		return TPR_ABORT;
	}
	
}

