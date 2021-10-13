//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "VerifTestCases.h"
#include "StencilCases.h"
#include <tux.h>
#include <tchar.h>


TESTPROCAPI StencilTest(LPVERIFTESTCASEARGS pTestCaseArgs)
{
	OutputDebugString(_T("Beginning StencilTest."));

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
		OutputDebugString(_T("Unknown test index."));
		return TPR_ABORT;
	}
	
}

