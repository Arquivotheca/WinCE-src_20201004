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
#include "Geometry.h"
#include "DepthBufferPermutations.h"
#include "ImageManagement.h"
#include <tchar.h>
#include <tux.h>
#include "DebugOutput.h"


//
// SupportsTableIndex
//
//   Determines if a test permutation is expected to be supported by the specified
//   Direct3D Mobile device.
//
// Arguments:
//
//   UINT uiTableIndex:  Index into table of test cases
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//
// Return Value:
//
//   S_OK if test case is supported on this device; E_FAIL otherwise.
//
HRESULT SupportsTableIndex(UINT uiTableIndex, LPDIRECT3DMOBILEDEVICE pDevice)
{
	HRESULT hr;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x)", hr);
		return E_FAIL;
	}

	switch(DepthBufTestCases[uiTableIndex].ZCmpFunc)
	{
	case D3DMCMP_NEVER:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_NEVER))
			return E_FAIL;
		break;
	case D3DMCMP_LESS:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_LESS))
			return E_FAIL;
		break;
	case D3DMCMP_EQUAL:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_EQUAL))
			return E_FAIL;
		break;
	case D3DMCMP_LESSEQUAL:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_LESSEQUAL))
			return E_FAIL;
		break;
	case D3DMCMP_GREATER:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_GREATER))
			return E_FAIL;
		break;
	case D3DMCMP_NOTEQUAL:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_NOTEQUAL))
			return E_FAIL;
		break;
	case D3DMCMP_GREATEREQUAL:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_GREATEREQUAL))
			return E_FAIL;
		break;
	case D3DMCMP_ALWAYS:
		if (!(Caps.ZCmpCaps & D3DMPCMPCAPS_ALWAYS))
			return E_FAIL;
		break;
	default:
		DebugOut(DO_ERROR, L"Unknown D3DMCMPFUNC.");
		break;
	}
	
	return S_OK;
}

     

//
// SetupTableIndex
//
//   Prepare render state settings for test case execution.  This function is intended
//   to be called after verifying that the driver is expected to support this test case.
//
//   Thus, any failure here is inconsistent with the driver's reported capability
//   bits.
//
// Arguments:
//
//   UINT uiTableIndex:  Index into table of test cases
//   LPDIRECT3DMOBILEDEVICE pDevice:  Direct3D Mobile device
//
// Return Value:
//
//   S_OK if state settings succeeded; E_FAIL otherwise.
//
HRESULT SetupTableIndex(UINT uiTableIndex, LPDIRECT3DMOBILEDEVICE pDevice)
{
    HRESULT hr;
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_SHADEMODE,*) failed. (hr = 0x%08x)", hr);
		return hr;
	}

	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ZFUNC, DepthBufTestCases[uiTableIndex].ZCmpFunc)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ZFUNC,*) failed. (hr = 0x%08x)", hr);
		return hr;
	}

	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ZENABLE, DepthBufTestCases[uiTableIndex].ZEnable)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ZENABLE,*) failed. (hr = 0x%08x)", hr);
		return hr;
	}

	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ZWRITEENABLE, DepthBufTestCases[uiTableIndex].ZWriteEnable)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ZWRITEENABLE,*) failed. (hr = 0x%08x)", hr);
		return hr;
	}
	
	if (FAILED(hr = pDevice->Clear(0,NULL,D3DMCLEAR_ZBUFFER,0,DepthBufTestCases[uiTableIndex].fClearZ,0)))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x)", hr);
		return hr;
	}
	
	if (FAILED(hr = pDevice->Clear(0,NULL,D3DMCLEAR_TARGET,D3DMCOLOR_XRGB(255,255,255),0.0f,0)))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x)", hr);
		return hr;
	}
	

	return S_OK;

}


//
// DepthBufferTest
//
//   Render and capture a single test frame; corresponding to a test case
//   for depth buffering.
//
// Arguments:
//   
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:
//
//   TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT; depending on test result.
//
TESTPROCAPI DepthBufferTest(LPTESTCASEARGS pTestCaseArgs)
{
    HRESULT hr;

	//
	// Result of test case execution
	//
	INT Result = TPR_PASS;


	//
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// Index into local test case table
	//
	DWORD dwTableIndex;


	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;

	//
	// Choose test case subset
	//
	if ((pTestCaseArgs->dwTestIndex >= D3DMQA_DEPTHBUFTEST_BASE) &&
	    (pTestCaseArgs->dwTestIndex <= D3DMQA_DEPTHBUFTEST_MAX))
	{
		DebugOut(L"Valid test index.");

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_DEPTHBUFTEST_BASE;

	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Verify supported capabilities
	//
	if (FAILED(hr = SupportsTableIndex(dwTableIndex, pDevice)))
	{
		DebugOut(DO_ERROR, L"SupportsTableIndex failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Clear render target
	//
	if (FAILED(hr = pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}
	
	//
	// Prepare render states
	//
	if (FAILED(hr = SetupTableIndex(dwTableIndex, pDevice)))
	{
		DebugOut(DO_ERROR, L"SetupTableIndex failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Create geometry for this test case
	//
	if (FAILED(hr = RenderGeometry(pDevice))) // LPDIRECT3DMOBILEDEVICE pDevice)
	{
		DebugOut(DO_ERROR, L"RenderGeometry failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(hr = DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                            0,             // UINT uiFrameNumber,
	                            NULL)))        // RECT *pRect = NULL
	{
		DebugOut(DO_ERROR, L"DumpFlushedFrame failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	return Result;

}
