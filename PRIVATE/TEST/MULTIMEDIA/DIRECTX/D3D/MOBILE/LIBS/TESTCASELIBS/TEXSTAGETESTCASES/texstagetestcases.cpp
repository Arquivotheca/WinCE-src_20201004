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
#include "ImageManagement.h"
#include "TestCases.h"
#include "ThreeArgOps.h"
#include "TwoArgOps.h"
#include "OneArgOps.h"
#include "VertexData.h"
#include "BufferTools.h"
#include "TextureTools.h"
#include <windows.h>
#include <d3dm.h>
#include <tux.h>
#include "DebugOutput.h"

//
// TexStageTest
//
//   Test texture blend operations.
//
// Arguments
//
// Return Value
//
//   TPR_PASS, TPR_FAIL, TPR_ABORT, or TPR_SKIP
//
TESTPROCAPI TexStageTest(LPTESTCASEARGS pTestCaseArgs)
{

	//
	// Result of test case execution
	//
	INT Result = TPR_PASS;

    HRESULT hr;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Vertex buffer to be rendered
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;
	//
	// Scale factors for positional data
	//
	UINT uiSX;
	UINT uiSY;

	//
	// Number of rendering passes, reported by ValidateDevice, for a particular
	// combination of texture cascade settings
	//
	DWORD dwNumPasses;

	//
	// Zero-based table index for test configuration
	//
	DWORD dwTableIndex;

	//
	// Flag that determines if we are using ARGB or RGB and XRGB
	//
	TextureAlphaFlag AlphaFlag = QATAF_DONTCARE;

	//
	// Color values for blender inputs
	//
	PDWORD pdwDiffuse  = NULL;
	PDWORD pdwSpecular = NULL;
	PDWORD pdwTFactor  = NULL;
	PDWORD pdwTexture  = NULL;

	//
	// Capture region
	//
	CONST RECT rRegion = {0,0,8,8};

	// 
	// 
	// Textures to be used for each stage in the cascade
	// 
	LPDIRECT3DMOBILETEXTURE pTexture0 = NULL;

	//
	// Iterater and data pointers for updating the X and Y components
	// of each vertex (scaled to window size)
	//
	UINT uiShift;
	FLOAT *pPosX;
	FLOAT *pPosY;

	//
	// Window client extents
	//
	RECT ClientExtents;

	//
	// Parameter validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Texturing support is required for this test
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_TEXTURE))
	{
		DebugOut(DO_ERROR, L"D3DMDEVCAPS_TEXTURE required, but not available. Aborting.");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (0 == GetClientRect( pTestCaseArgs->hWnd,           // HWND hWnd, 
	                       &ClientExtents)) // LPRECT lpRect 
	{
		DebugOut(DO_ERROR, L"GetClientRect failed. (hr = 0x%08x) Aborting.", HRESULT_FROM_WIN32(GetLastError()));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Scale factors
	//
	uiSX = ClientExtents.right;
	uiSY = ClientExtents.bottom;


	//
	// Testing a three arg texture op?
	//
	if (((D3DMQA_TEXSTAGETHREEARGNOALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
		 (D3DMQA_TEXSTAGETHREEARGNOALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex)) ||
		((D3DMQA_TEXSTAGETHREEARGWITHALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
		 (D3DMQA_TEXSTAGETHREEARGWITHALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex)))
	{
	    if ((D3DMQA_TEXSTAGETHREEARGNOALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
		    (D3DMQA_TEXSTAGETHREEARGNOALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex))
		{
		    dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGETHREEARGNOALPHATEST_BASE;
		    AlphaFlag = QATAF_NOALPHAONLY;
		}
	    if ((D3DMQA_TEXSTAGETHREEARGWITHALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
		    (D3DMQA_TEXSTAGETHREEARGWITHALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex))
		{
		    dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGETHREEARGWITHALPHATEST_BASE;
		    AlphaFlag = QATAF_ALPHAONLY;
		}

		//
		// Does driver support D3DMTEXTUREOP used in this test case?
		//
		if (FALSE == IsThreeArgTestOpSupported( pTestCaseArgs->pDevice, dwTableIndex))
		{
			DebugOut(DO_ERROR, L"Device capabilities do not expose the D3DMTEXTUREOP(s) used in this test case. Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Set arg states for this test case
		//
		if (FAILED(hr = SetThreeArgStates( pTestCaseArgs->pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
									       dwTableIndex))) // DWORD dwTableIndex
		{
			DebugOut(DO_ERROR, L"SetThreeArgStates failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve colors that need to be set for blender input
		//
		if (FAILED(hr = GetThreeArgColors(pTestCaseArgs->pDevice,       // LPDIRECT3DMOBILEDEVICE pDevice,
									      dwTableIndex,  // DWORD dwTableIndex
									      &pdwDiffuse,   // PPDWORD ppdwDiffuse 
									      &pdwSpecular,  // PPDWORD ppdwSpecular
									      &pdwTFactor,   // PPDWORD ppdwTFactor 
									      &pdwTexture))) // PPDWORD ppdwTexture 
		{
			DebugOut(DO_ERROR, L"GetThreeArgColors failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

	}
	else if (((D3DMQA_TEXSTAGEONEARGNOALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	          (D3DMQA_TEXSTAGEONEARGNOALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex)) ||
	         ((D3DMQA_TEXSTAGEONEARGWITHALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	          (D3DMQA_TEXSTAGEONEARGWITHALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex)))
	{
        if ((D3DMQA_TEXSTAGEONEARGNOALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	        (D3DMQA_TEXSTAGEONEARGNOALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex))
	    {
		    dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGEONEARGNOALPHATEST_BASE;
		    AlphaFlag = QATAF_NOALPHAONLY;
		}
		
        if ((D3DMQA_TEXSTAGEONEARGWITHALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	        (D3DMQA_TEXSTAGEONEARGWITHALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex))
	    {
		    dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGEONEARGWITHALPHATEST_BASE;
		    AlphaFlag = QATAF_ALPHAONLY;
		}


		//
		// Does driver support D3DMTEXTUREOP used in this test case?
		//
		if (FALSE == IsOneArgTestOpSupported( pTestCaseArgs->pDevice, dwTableIndex))
		{
			DebugOut(DO_ERROR, L"Device capabilities do not expose the D3DMTEXTUREOP(s) used in this test case. Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Set arg states for this test case
		//
		if (FAILED(hr = SetOneArgStates( pTestCaseArgs->pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
									     dwTableIndex))) // DWORD dwTableIndex
		{
			DebugOut(DO_ERROR, L"SetOneArgStates failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve colors that need to be set for blender input
		//
		if (FAILED(hr = GetOneArgColors(pTestCaseArgs->pDevice,       // LPDIRECT3DMOBILEDEVICE pDevice,
		                                dwTableIndex,  // DWORD dwTableIndex
		                                &pdwDiffuse,   // PPDWORD ppdwDiffuse 
		                                &pdwSpecular,  // PPDWORD ppdwSpecular
		                                &pdwTFactor,   // PPDWORD ppdwTFactor 
		                                &pdwTexture))) // PPDWORD ppdwTexture 
		{
			DebugOut(DO_ERROR, L"GetOneArgColors failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

	}
 	else if (((D3DMQA_TEXSTAGETWOARGNOALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
 	          (D3DMQA_TEXSTAGETWOARGNOALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex)) ||
 	         ((D3DMQA_TEXSTAGETWOARGWITHALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	          (D3DMQA_TEXSTAGETWOARGWITHALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex)))
	{
		if ((D3DMQA_TEXSTAGETWOARGNOALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
 	          (D3DMQA_TEXSTAGETWOARGNOALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex))
 	    {
	    	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGETWOARGNOALPHATEST_BASE;
	    	AlphaFlag = QATAF_NOALPHAONLY;
	    }
		if ((D3DMQA_TEXSTAGETWOARGWITHALPHATEST_BASE <= pTestCaseArgs->dwTestIndex) &&
 	          (D3DMQA_TEXSTAGETWOARGWITHALPHATEST_MAX >=  pTestCaseArgs->dwTestIndex))
 	    {
	    	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGETWOARGWITHALPHATEST_BASE;
	    	AlphaFlag = QATAF_ALPHAONLY;
	    }

		//
		// Does driver support D3DMTEXTUREOP used in this test case?
		//
		if (FALSE == IsTwoArgTestOpSupported( pTestCaseArgs->pDevice, dwTableIndex))
		{
			DebugOut(DO_ERROR, L"Device capabilities do not expose the D3DMTEXTUREOP(s) used in this test case. Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Set arg states for this test case
		//
		if (FAILED(hr = SetTwoArgStates( pTestCaseArgs->pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
									     dwTableIndex))) // DWORD dwTableIndex
		{
			DebugOut(DO_ERROR, L"SetTwoArgStates failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve colors that need to be set for blender input
		//
		if (FAILED(hr = GetTwoArgColors(pTestCaseArgs->pDevice,       // LPDIRECT3DMOBILEDEVICE pDevice,
		                                dwTableIndex,  // DWORD dwTableIndex
		                                &pdwDiffuse,   // PPDWORD ppdwDiffuse 
		                                &pdwSpecular,  // PPDWORD ppdwSpecular
		                                &pdwTFactor,   // PPDWORD ppdwTFactor 
		                                &pdwTexture))) // PPDWORD ppdwTexture 
		{
			DebugOut(DO_ERROR, L"GetTwoArgColors failed. (hr = 0x%08x) Failing.", hr);
			Result = TPR_FAIL;
			goto cleanup;
		}

	}
	else
	{
		DebugOut(DO_ERROR, L"Invalid test index. Failing.");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Set up diffuse blend input
	//
	if (NULL != pdwDiffuse)
	{
		TexStageTestVerts[0].Diffuse = 
		TexStageTestVerts[1].Diffuse = 
		TexStageTestVerts[2].Diffuse = 
		TexStageTestVerts[3].Diffuse = *pdwDiffuse;
	}


	//
	// Set up specular blend input
	//
	if (NULL != pdwSpecular)
	{
		TexStageTestVerts[0].Specular = 
		TexStageTestVerts[1].Specular = 
		TexStageTestVerts[2].Specular = 
		TexStageTestVerts[3].Specular = *pdwSpecular;
	}

	//
	// Set up texture factor blend input
	//
	if (NULL != pdwTFactor)
	{
		if (FAILED(hr = pTestCaseArgs->pDevice->SetRenderState(D3DMRS_TEXTUREFACTOR, *pdwTFactor)))
		{
			DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_TEXTUREFACTOR,) failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}
	}

	//
	// Set up texture blend input
	//
	if (NULL != pdwTexture)
	{
	    HRESULT hr = GetSolidTexture( pTestCaseArgs->pDevice,     // LPDIRECT3DMOBILEDEVICE pDevice,
									  *pdwTexture, // D3DCOLOR Color,
									  &pTexture0,  // LPDIRECT3DMOBILETEXTURE *ppTexture
									  AlphaFlag);  // TextureAlphaFlag AlphaFlag = QATAF_DONTCARE
		if (D3DMERR_NOTFOUND == hr)
		{
		    DebugOut(DO_ERROR, L"GetSolidTexture found no suitable texture format. Skipping.");
		    Result = TPR_SKIP;
		    goto cleanup;
		}
		else if (FAILED(hr))
		{
			DebugOut(DO_ERROR, L"GetSolidTexture failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		if (FAILED(hr = pTestCaseArgs->pDevice->SetTexture(0, pTexture0)))
		{
			DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}

		pTexture0->Release();  // SetTexture will prevent object deletion
	}


	//
	// Scale to window size
	//
	for (uiShift = 0; uiShift < D3DQA_NUMVERTS; uiShift++)
	{
		pPosX = (PFLOAT)((PBYTE)(TexStageTestVerts) + uiShift * sizeof(D3DMTEXSTAGETEST_VERTS));
		pPosY = &(pPosX[1]);

		(*pPosX) *= (float)(uiSX);
		(*pPosY) *= (float)(uiSY);
	}

	//
	// Create and fill a vertex buffer
	//
	if (FAILED(hr = CreateFilledVertexBuffer(pTestCaseArgs->pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
	                                         &pVB,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                         (PBYTE)TexStageTestVerts,          // BYTE *pVertices,
	                                         sizeof(D3DMTEXSTAGETEST_VERTS),    // UINT uiVertexSize,
	                                         D3DQA_NUMVERTS,                    // UINT uiNumVertices,
	                                         D3DMTEXSTAGETEST_FVF)))            // DWORD dwFVF
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// "Unscale" from window size
	//
	for (uiShift = 0; uiShift < D3DQA_NUMVERTS; uiShift++)
	{
		pPosX = (PFLOAT)((PBYTE)(TexStageTestVerts) + uiShift * sizeof(D3DMTEXSTAGETEST_VERTS));
		pPosY = &(pPosX[1]);

		(*pPosX) /= (float)(uiSX);
		(*pPosY) /= (float)(uiSY);

	}

	//
	// The device *may* expose additional texture stage state constraints via ValidateDevice 
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->ValidateDevice(&dwNumPasses)))
	{
		DebugOut(DO_ERROR, L"ValidateDevice indicates that this device does not support the proposed set of texture stage states. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare test case permutation
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->BeginScene()))
	{
		DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->DrawPrimitive(D3DQA_PRIMTYPE,0,D3DQA_NUMPRIM)))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->EndScene()))
	{
		DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		DebugOut(DO_ERROR, L"Present failed. (hr = 0x%08x) Failing.", hr);
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

	//
	// Detaching textures will remove remaining reference count
	//
	if (pTestCaseArgs)
		if (pTestCaseArgs->pDevice)
				pTestCaseArgs->pDevice->SetTexture(0,NULL);

	if (pVB)
		pVB->Release();


	return Result;
}
