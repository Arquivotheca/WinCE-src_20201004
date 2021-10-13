//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "Geometry.h"
#include "DepthBufferPermutations.h"
#include "ImageManagement.h"
#include <tchar.h>
#include <tux.h>


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
	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
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
		OutputDebugString(_T("Unknown D3DMCMPFUNC.\n"));
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

	if (FAILED(pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE,*) failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetRenderState(D3DMRS_ZFUNC, DepthBufTestCases[uiTableIndex].ZCmpFunc)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_ZFUNC,*) failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetRenderState(D3DMRS_ZENABLE, DepthBufTestCases[uiTableIndex].ZEnable)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_ZENABLE,*) failed."));
		return E_FAIL;
	}

	if (FAILED(pDevice->SetRenderState(D3DMRS_ZWRITEENABLE, DepthBufTestCases[uiTableIndex].ZWriteEnable)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_ZWRITEENABLE,*) failed."));
		return E_FAIL;
	}
	
	if (FAILED(pDevice->Clear(0,NULL,D3DMCLEAR_ZBUFFER,0,DepthBufTestCases[uiTableIndex].fClearZ,0)))
	{
		OutputDebugString(_T("Clear failed."));
		return E_FAIL;
	}
	
	if (FAILED(pDevice->Clear(0,NULL,D3DMCLEAR_TARGET,D3DMCOLOR_XRGB(255,255,255),0.0f,0)))
	{
		OutputDebugString(_T("Clear failed."));
		return E_FAIL;
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
		OutputDebugString(_T("Invalid argument(s)."));
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
		OutputDebugString(_T("Valid test index."));

		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_DEPTHBUFTEST_BASE;

	}
	else
	{
		OutputDebugString(L"Invalid test index.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Verify supported capabilities
	//
	if (FAILED(SupportsTableIndex(dwTableIndex, pDevice)))
	{
		OutputDebugString(L"SupportsTableIndex failed..\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Clear render target
	//
	if (FAILED(pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		OutputDebugString(L"Clear failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}
	
	//
	// Prepare render states
	//
	if (FAILED(SetupTableIndex(dwTableIndex, pDevice)))
	{
		OutputDebugString(L"SetupTableIndex failed..\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Create geometry for this test case
	//
	if (FAILED(RenderGeometry(pDevice))) // LPDIRECT3DMOBILEDEVICE pDevice)
	{
		OutputDebugString(L"RenderGeometry failed..\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Flush the swap chain and capture a frame
	//
	if (FAILED(DumpFlushedFrame(pTestCaseArgs, // LPTESTCASEARGS pTestCaseArgs
	                            0,             // UINT uiFrameNumber,
	                            NULL)))        // RECT *pRect = NULL
	{
		OutputDebugString(_T("DumpFlushedFrame failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

cleanup:

	return Result;

}
