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
#include "ImageManagement.h"
#include "Transforms.h"
#include "Util.h"
#include "ClippingTest.h"
#include "TestCases.h"
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "DebugOutput.h"

#ifndef _PREFAST_
// warning 4068: unknown pragma
// This is disabled because the prefast pragma gives a warning.
#pragma warning(disable:4068)
#endif

//
// Viewport shrinkage multipliers
//
#define D3DQA_VIEWPORT_HEIGHT_MULT 0.75f
#define D3DQA_VIEWPORT_WIDTH_MULT 0.75f



//
// ClipTestEx
//   
//   Tests primitive rendering, when primitives are partially or wholly outside of the
//   view frustum.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device to exercise
//   UINT uiTestID:  Identifier for use in image dump filenames
//   WCHAR *wszImageDir:  Directory to dump images to
//   UINT uiInstanceHandle:  Identifier for use in image dump filenames
//   HWND hWnd:  HWND to render to
//   PRIM_DIRECTION PrimDir:  Toggle for primitive orientation setting; for test variety
//   TRANSLATE_DIR TranslateDir:  Indicates clip plane to be violated by primitive
//   D3DMPRIMITIVETYPE PrimType:  Indicates whether to generate lines or triangles
//   
// Return Value
// 
//   TPR_PASS, TPR_ABORT, TPR_SKIP, or TPR_FAIL
//   
#pragma prefast(disable: 11, "ignore investigated prefast warning") 
INT ClipTestEx(LPDIRECT3DMOBILEDEVICE pDevice,
               UINT uiTestID,
               BOOL bSpecularEnable,
               D3DMSHADEMODE ShadeMode,
               D3DMFILLMODE FillMode,
               WCHAR *wszImageDir,
               UINT uiInstanceHandle,
               HWND hWnd,
               PRIM_DIRECTION PrimDir,
               TRANSLATE_DIR TranslateDir,
               D3DMPRIMITIVETYPE PrimType,
               LPTESTCASEARGS pTestCaseArgs)
{
	//
	// All failure conditions set this to an error
	//
	INT Result = TPR_PASS;

    HRESULT hr;

	//
	// Interfaces for scene data
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

	//
	// Storage for geometry specs
	//
	GEOMETRY_DESC Geometry = { 0, 0, &pVB };

	//
	// Device Capabilities
	//
	D3DMCAPS Caps;

	//
	// Storage for original viewport extents, to be restored at end of test case execution
	//
	D3DMVIEWPORT Viewport;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, _T("Invalid argument(s). Aborting."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Lighting is "baked" in verts
	//
	pDevice->SetRenderState( D3DMRS_LIGHTING, 0 );

	//
	// Counterclockwise culling; guaranteed to be supported by base profile
	//
	pDevice->SetRenderState( D3DMRS_CULLMODE, D3DMCULL_CCW );

	//
	// Store original viewport extents; to restore at end of test case execution
	//
	if (FAILED(hr = pDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, _T("GetViewport failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Prepare TnL with reasonable defaults for transformations
	//
	if (FAILED(hr = SetTransformDefaults(pDevice)))
	{
		DebugOut(DO_ERROR, _T("SetTransformDefaults failed. (hr = 0x%08x) Aborting."), hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Clear the backbuffer and the zbuffer; particularly important
	// due to frame capture
	//
	pDevice->Clear( 0, NULL, D3DMCLEAR_TARGET|D3DMCLEAR_ZBUFFER,
						D3DMCOLOR_XRGB(0,0,0), 1.0f, 0 );

	//
	// Verify that the driver supports this test case
	//
	if ((!(Caps.ShadeCaps & D3DMPSHADECAPS_SPECULARGOURAUDRGB)) &&
		(TRUE == bSpecularEnable) &&
		(D3DMSHADE_GOURAUD == ShadeMode))
	{
		DebugOut(DO_ERROR, _T("No support for D3DMPSHADECAPS_SPECULARGOURAUDRGB. Skipping."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	if ((!(Caps.ShadeCaps & D3DMPSHADECAPS_COLORGOURAUDRGB)) &&
		(D3DMSHADE_GOURAUD == ShadeMode))
	{
		DebugOut(DO_ERROR, _T("No support for D3DMPSHADECAPS_COLORGOURAUDRGB. Skipping."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Is specular lighting supported, if required?
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
	{
		if (TRUE == bSpecularEnable)
		{
			DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR; Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Set render states, according to arguments
	//
	if (bSpecularEnable)
	{
		pDevice->SetRenderState(D3DMRS_SPECULARENABLE, TRUE);
	}
	else
	{
		pDevice->SetRenderState(D3DMRS_SPECULARENABLE, FALSE);
	}

	pDevice->SetRenderState(D3DMRS_SHADEMODE, ShadeMode);
	pDevice->SetRenderState(D3DMRS_FILLMODE, FillMode);


	//
	// Enable clipping
	//
	if( FAILED( hr = pDevice->SetRenderState(D3DMRS_CLIPPING, TRUE)))
	{
		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_CLIPPING,TRUE) failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	
	//
	// Pick a frame approximately in the middle of the range
	//
	// Create geometry; stored in vertex buffer for convenient drawing
	//
	if (FAILED(hr = SetupClipGeometry(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
		                              &ProjSpecs,        // PERSPECTIVE_PROJECTION_SPECS *pProjection,
		                              10,                // UINT uiMaxIter,
		                              5,                 // UINT uiCurIter
		                              PrimDir,           // PRIM_DIRECTION PrimDir
		                              TranslateDir,      // TRANSLATE_DIR TranslateDir
		                              &Geometry,         // GEOMETRY_DESC *pDesc
							          PrimType)))        // D3DMPRIMITIVETYPE PrimType
	{
		DebugOut(DO_ERROR, _T("SetupClipGeometry failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Clear the backbuffer and the zbuffer; particularly important
	// due to frame capture
	//
	pDevice->Clear( 0, NULL, D3DMCLEAR_TARGET|D3DMCLEAR_ZBUFFER,
						D3DMCOLOR_XRGB(0,0,0), 1.0f, 0 );

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(hr = pDevice->BeginScene()))
	{
		DebugOut(DO_ERROR, _T("BeginScene failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Indicate primitive data to be drawn
	//
	if (FAILED(hr = pDevice->DrawPrimitive(PrimType,               // D3DMPRIMITIVETYPE Type,
							               0,                      // UINT StartVertex,
							               Geometry.uiPrimCount))) // UINT PrimitiveCount
	{
		DebugOut(DO_ERROR, _T("DrawIndexedPrimitive failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(hr = pDevice->EndScene()))
	{
		DebugOut(DO_ERROR, _T("EndScene failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(hr = pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		DebugOut(DO_ERROR, _T("Present failed. (hr = 0x%08x) Failing."), hr);
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
		DebugOut(DO_ERROR, _T("DumpFlushedFrame failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Get rid of vertex buffer reference count held by runtime
	//
	if (FAILED(hr = pDevice->SetStreamSource(0,           // UINT StreamNumber,
								             NULL,        // IDirect3DMobileVertexBuffer *pStreamData,
								             0)))         // UINT Stride
	{
		DebugOut(DO_ERROR, _T("SetStreamSource failed. (hr = 0x%08x) Failing."), hr);
		Result = TPR_FAIL;
		goto cleanup;
	}
	pVB->Release();
	pVB = NULL;

cleanup:

	if (pVB)
		pVB->Release();

	return Result;
}


//
// ClipTest
//
//   Wrapper for ClipTestEx; to expose easily as a set of tux tests.
//
// Arguments:
//
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//
// Return Value:  
//   
//   INT:  Indicates test result
//   
TESTPROCAPI ClipTest(LPTESTCASEARGS pTestCaseArgs)
{

	//
	// Decode test permutation
	//
	DWORD dwCode;


	//
	// Test parameters
	//
	D3DMFILLMODE FillMode;
	D3DMSHADEMODE ShadeMode;
	BOOL bSpecular;
	TRANSLATE_DIR TranslateDir;
	D3DMPRIMITIVETYPE PrimType;


	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, _T("Invalid argument(s). Aborting."));
		return TPR_ABORT;
	}

	dwCode = dwClipTestPermutations[pTestCaseArgs->dwTestIndex - D3DMQA_CLIPPINGTEST_BASE];
	
	if (FILL_SOLID & dwCode)
	{
		FillMode = D3DMFILL_SOLID;
	}
	else if (FILL_LINE  & dwCode)
	{
		FillMode = D3DMFILL_WIREFRAME;
	}
	else if (FILL_POINT  & dwCode)
	{
		FillMode = D3DMFILL_POINT;
	}
	else
	{
		DebugOut(DO_ERROR, _T("Invalid test case. Failing."));
		return TPR_FAIL;
	}

	if (SHADE_GOURAUD & dwCode)
	{
		ShadeMode = D3DMSHADE_GOURAUD;
	}
	else if (SHADE_FLAT  & dwCode)
	{
		ShadeMode = D3DMSHADE_FLAT;
	}
	else
	{
		DebugOut(DO_ERROR, _T("Invalid test case. Failing."));
		return TPR_FAIL;
	}

	if (FVF_SPECULAR  & dwCode)
	{
		bSpecular = TRUE;
	}
	else if (FVF_DIFFUSE & dwCode)
	{
		bSpecular = FALSE;
	}
	else
	{
		DebugOut(DO_ERROR, _T("Invalid test case. Failing."));
		return TPR_FAIL;
	}

	if (PLANE_LEFT & dwCode)
	{
		TranslateDir = TRANSLATE_TO_NEGX;
	}
	else if (PLANE_RIGHT  & dwCode)
	{
		TranslateDir = TRANSLATE_TO_POSX;
	}
	else if (PLANE_TOP  & dwCode)
	{
		TranslateDir = TRANSLATE_TO_POSY;
	}
	else if (PLANE_BOTTOM  & dwCode)
	{
		TranslateDir = TRANSLATE_TO_NEGY;
	}
	else if (PLANE_NEAR  & dwCode)
	{
		TranslateDir = TRANSLATE_TO_NEGZ;
	}
	else if (PLANE_FAR  & dwCode)
	{
		TranslateDir = TRANSLATE_TO_POSZ;
	}
	else
	{
		DebugOut(DO_ERROR, _T("Invalid test case. Failing"));
		return TPR_FAIL;
	}

	if (PRIM_TRIANGLE & dwCode)
	{
		PrimType = D3DMPT_TRIANGLELIST;
	}
	else if (PRIM_LINE  & dwCode)
	{
		PrimType = D3DMPT_LINELIST;
	}
	else
	{
		DebugOut(DO_ERROR, _T("Invalid test case. Failing."));
		return TPR_FAIL;
	}

	return ClipTestEx(pTestCaseArgs->pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
	                  pTestCaseArgs->dwTestIndex,            // UINT uiTestID,
	                  bSpecular,                             // BOOL bSpecularEnable,
	                  ShadeMode,                             // D3DMSHADEMODE ShadeMode,
	                  FillMode,                              // D3DMFILLMODE FillMode,
	                  pTestCaseArgs->pwszImageComparitorDir, // WCHAR *wszImageDir,
	                  pTestCaseArgs->uiInstance,             // UINT uiInstanceHandle,
	                  pTestCaseArgs->hWnd,                   // HWND hWnd,
	                  PRIM_DIR1,                             // PRIM_DIRECTION PrimDir,
	                  TranslateDir,                          // TRANSLATE_DIR TranslateDir,
	                  PrimType,                              // D3DMPRIMITIVETYPE PrimType
	                  pTestCaseArgs);                        // LPTESTCASEARGS pTestCaseArgs
}

#pragma prefast(pop)
