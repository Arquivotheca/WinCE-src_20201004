//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
		OutputDebugString(_T("Invalid argument(s)."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(pTestCaseArgs->pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(L"GetDeviceCaps failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Texturing support is required for this test
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_TEXTURE))
	{
		OutputDebugString(L"D3DMDEVCAPS_TEXTURE required, but not available.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (0 == GetClientRect( pTestCaseArgs->hWnd,           // HWND hWnd, 
	                       &ClientExtents)) // LPRECT lpRect 
	{
		OutputDebugString(L"GetClientRect failed.\n");
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
	if ((D3DMQA_TEXSTAGETHREEARGTEST_BASE <= pTestCaseArgs->dwTestIndex) &&
		(D3DMQA_TEXSTAGETHREEARGTEST_MAX >=  pTestCaseArgs->dwTestIndex))
	{
		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGETHREEARGTEST_BASE;

		//
		// Does driver support D3DMTEXTUREOP used in this test case?
		//
		if (FALSE == IsThreeArgTestOpSupported( pTestCaseArgs->pDevice, dwTableIndex))
		{
			OutputDebugString(L"Device capabilities do not expose the D3DMTEXTUREOP(s) used in this test case.\n");
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Set arg states for this test case
		//
		if (FAILED(SetThreeArgStates( pTestCaseArgs->pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
									dwTableIndex))) // DWORD dwTableIndex
		{
			OutputDebugString(L"SetThreeArgStates failed.\n");
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve colors that need to be set for blender input
		//
		if (FAILED(GetThreeArgColors(pTestCaseArgs->pDevice,       // LPDIRECT3DMOBILEDEVICE pDevice,
									dwTableIndex,  // DWORD dwTableIndex
									&pdwDiffuse,   // PPDWORD ppdwDiffuse 
									&pdwSpecular,  // PPDWORD ppdwSpecular
									&pdwTFactor,   // PPDWORD ppdwTFactor 
									&pdwTexture))) // PPDWORD ppdwTexture 
		{
			OutputDebugString(L"GetThreeArgColors failed.\n");
			Result = TPR_FAIL;
			goto cleanup;
		}

	}
	else if ((D3DMQA_TEXSTAGEONEARGTEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	         (D3DMQA_TEXSTAGEONEARGTEST_MAX >=  pTestCaseArgs->dwTestIndex))
	{
		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGEONEARGTEST_BASE;

		//
		// Does driver support D3DMTEXTUREOP used in this test case?
		//
		if (FALSE == IsOneArgTestOpSupported( pTestCaseArgs->pDevice, dwTableIndex))
		{
			OutputDebugString(L"Device capabilities do not expose the D3DMTEXTUREOP(s) used in this test case.\n");
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Set arg states for this test case
		//
		if (FAILED(SetOneArgStates( pTestCaseArgs->pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
									dwTableIndex))) // DWORD dwTableIndex
		{
			OutputDebugString(L"SetOneArgStates failed.\n");
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve colors that need to be set for blender input
		//
		if (FAILED(GetOneArgColors(pTestCaseArgs->pDevice,       // LPDIRECT3DMOBILEDEVICE pDevice,
		                           dwTableIndex,  // DWORD dwTableIndex
		                           &pdwDiffuse,   // PPDWORD ppdwDiffuse 
		                           &pdwSpecular,  // PPDWORD ppdwSpecular
		                           &pdwTFactor,   // PPDWORD ppdwTFactor 
		                           &pdwTexture))) // PPDWORD ppdwTexture 
		{
			OutputDebugString(L"GetOneArgColors failed.\n");
			Result = TPR_FAIL;
			goto cleanup;
		}

	}
	else if ((D3DMQA_TEXSTAGETWOARGTEST_BASE <= pTestCaseArgs->dwTestIndex) &&
	         (D3DMQA_TEXSTAGETWOARGTEST_MAX >=  pTestCaseArgs->dwTestIndex))
	{
		dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_TEXSTAGETWOARGTEST_BASE;

		//
		// Does driver support D3DMTEXTUREOP used in this test case?
		//
		if (FALSE == IsTwoArgTestOpSupported( pTestCaseArgs->pDevice, dwTableIndex))
		{
			OutputDebugString(L"Device capabilities do not expose the D3DMTEXTUREOP(s) used in this test case.\n");
			Result = TPR_SKIP;
			goto cleanup;
		}

		//
		// Set arg states for this test case
		//
		if (FAILED(SetTwoArgStates( pTestCaseArgs->pDevice,        // LPDIRECT3DMOBILEDEVICE pDevice,
									dwTableIndex))) // DWORD dwTableIndex
		{
			OutputDebugString(L"SetTwoArgStates failed.\n");
			Result = TPR_FAIL;
			goto cleanup;
		}

		//
		// Retrieve colors that need to be set for blender input
		//
		if (FAILED(GetTwoArgColors(pTestCaseArgs->pDevice,       // LPDIRECT3DMOBILEDEVICE pDevice,
		                           dwTableIndex,  // DWORD dwTableIndex
		                           &pdwDiffuse,   // PPDWORD ppdwDiffuse 
		                           &pdwSpecular,  // PPDWORD ppdwSpecular
		                           &pdwTFactor,   // PPDWORD ppdwTFactor 
		                           &pdwTexture))) // PPDWORD ppdwTexture 
		{
			OutputDebugString(L"GetTwoArgColors failed.\n");
			Result = TPR_FAIL;
			goto cleanup;
		}

	}
	else
	{
		OutputDebugString(L"Invalid test index.\n");
		Result = TPR_SKIP;
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
		if (FAILED(pTestCaseArgs->pDevice->SetRenderState(D3DMRS_TEXTUREFACTOR, *pdwTFactor)))
		{
			OutputDebugString(L"SetRenderState(D3DMRS_TEXTUREFACTOR,) failed.  Aborting.\n");
			Result = TPR_ABORT;
			goto cleanup;
		}
	}

	//
	// Set up texture blend input
	//
	if (NULL != pdwTexture)
	{
		if (FAILED(GetSolidTexture( pTestCaseArgs->pDevice,     // LPDIRECT3DMOBILEDEVICE pDevice,
									*pdwTexture, // D3DCOLOR Color,
									&pTexture0)))// LPDIRECT3DMOBILETEXTURE *ppTexture
		{
			OutputDebugString(L"GetSolidTexture failed.\n");
			Result = TPR_ABORT;
			goto cleanup;
		}

		if (FAILED(pTestCaseArgs->pDevice->SetTexture(0, pTexture0)))
		{
			OutputDebugString(L"SetTexture failed.\n");
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
	if (FAILED(CreateFilledVertexBuffer(pTestCaseArgs->pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
	                                    &pVB,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                    (PBYTE)TexStageTestVerts,          // BYTE *pVertices,
	                                    sizeof(D3DMTEXSTAGETEST_VERTS),    // UINT uiVertexSize,
	                                    D3DQA_NUMVERTS,                    // UINT uiNumVertices,
	                                    D3DMTEXSTAGETEST_FVF)))            // DWORD dwFVF
	{
		OutputDebugString(L"CreateActiveBuffer failed.\n");
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
	if (FAILED(pTestCaseArgs->pDevice->ValidateDevice(&dwNumPasses)))
	{
		OutputDebugString(L"ValidateDevice indicates that this device does not support the proposed set of texture stage states.\n");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare test case permutation
	//
	if (FAILED(pTestCaseArgs->pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(0,0,0) , 1.0f, 0x00000000 )))
	{
		OutputDebugString(L"Clear failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(pTestCaseArgs->pDevice->BeginScene()))
	{
		OutputDebugString(L"BeginScene failed.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(pTestCaseArgs->pDevice->DrawPrimitive(D3DQA_PRIMTYPE,0,D3DQA_NUMPRIM)))
	{
		OutputDebugString(L"DrawPrimitive failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pTestCaseArgs->pDevice->EndScene()))
	{
		OutputDebugString(L"EndScene failed.\n");
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(pTestCaseArgs->pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(L"Present failed.\n");
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
