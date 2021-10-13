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
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "BufferTools.h"
#include "FVFStyles.h"
#include "ImageManagement.h"
#include "utils.h"
#include "FVFResources.h"
#include "DebugOutput.h"

#define _M(_v) D3DM_MAKE_D3DMVALUE(_v)


//
// FVFTest
//
//   Configures the texture blending unit to effectively test an arbitrary FVF.  Not fully generalized due
//   to complexities of texture blending unit.
//
//   Skips if ValidateDevice fails; it is acceptable for a device to indicate non-support of any
//   set of states provided to the texture blending unit.
//   
// Arguments:
//
//   LPTESTCASEARGS pTestCaseArgs:  Information pertinent to test case execution
//    
// Return Value:
//    
//   INT:  TPR_PASS, TPR_FAIL, TPR_SKIP, or TPR_ABORT
//
TESTPROCAPI FVFTest(LPTESTCASEARGS pTestCaseArgs)
{
	DebugOut(L"FVFTest starting");

	//
	// Texture op that this test case requires
	//
	CONST DWORD RequiredTextureOpCap = D3DMTEXOPCAPS_MODULATE | D3DMTEXOPCAPS_SELECTARG1;

    HRESULT hr;

	//
	// How many textures will the test use (detected based on FVF)
	//
	UINT uiNumTextures;

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
	// Target device (local variable for brevity in code)
	//
	LPDIRECT3DMOBILEDEVICE pDevice = NULL;

	//
	// Variables for guiding the configuration of texture cascade
	//
	UINT uiCurrentStage;
	UINT uiStageCount = 0;

	//
	// For shift+scale processing on RHW vertices; allows for
	// test to be generalized to handle arbitrary window sizes
	//
	UINT uiShift;
	FLOAT *pPosX, *pPosY;

	//
	// Scale factors for positional data
	//
	UINT uiSX;
	UINT uiSY;

	//
	// Table index is based on offset test case index
	//
	DWORD dwTableIndex;

	//
	// Number of rendering passes, reported by ValidateDevice, for a particular
	// combination of texture cascade settings
	//
	DWORD dwNumPasses;

	//
	//                                                             Texture stage state settings used in this test
	//
	//                                |    D3DTEXTUREOP   |   D3DTEXTUREOP   |     DWORD      |     DWORD      |     DWORD      |     DWORD      |
	//                                |      ColorOp      |    AlphaOp       |  ColorTexArg1  |  ColorTexArg2  |  AlphaTexArg1  |  AlphaTexArg2  |
	//                                +-------------------+------------------+----------------+----------------+----------------+----------------+
	TEX_STAGE_SETTINGS FirstTexture  = { D3DMTOP_SELECTARG1, D3DMTOP_SELECTARG1,   D3DMTA_TEXTURE,   D3DMTA_CURRENT,   D3DMTA_TEXTURE,   D3DMTA_CURRENT};
	TEX_STAGE_SETTINGS GenTexture    = {   D3DMTOP_MODULATE,   D3DMTOP_MODULATE,   D3DMTA_TEXTURE,   D3DMTA_CURRENT,   D3DMTA_TEXTURE,   D3DMTA_CURRENT};

	TEX_STAGE_SETTINGS FirstSpecular = { D3DMTOP_SELECTARG1, D3DMTOP_SELECTARG1,  D3DMTA_SPECULAR,   D3DMTA_CURRENT,  D3DMTA_SPECULAR,   D3DMTA_CURRENT};
	TEX_STAGE_SETTINGS GenSpecular   = {   D3DMTOP_MODULATE,   D3DMTOP_MODULATE,  D3DMTA_SPECULAR,   D3DMTA_CURRENT,  D3DMTA_SPECULAR,   D3DMTA_CURRENT};

	TEX_STAGE_SETTINGS FirstDiffuse  = { D3DMTOP_SELECTARG1, D3DMTOP_SELECTARG1,   D3DMTA_DIFFUSE,   D3DMTA_CURRENT,   D3DMTA_DIFFUSE,   D3DMTA_CURRENT};
	TEX_STAGE_SETTINGS GenDiffuse    = {   D3DMTOP_MODULATE,   D3DMTOP_MODULATE,   D3DMTA_DIFFUSE,   D3DMTA_CURRENT,   D3DMTA_DIFFUSE,   D3DMTA_CURRENT};

	// 
	// 
	// Textures to be used for each stage in the cascade
	// 
	LPDIRECT3DMOBILETEXTURE pTexture0 = NULL;
	LPDIRECT3DMOBILETEXTURE pTexture1 = NULL;
	LPDIRECT3DMOBILETEXTURE pTexture2 = NULL;
	LPDIRECT3DMOBILETEXTURE pTexture3 = NULL;

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

	pDevice = pTestCaseArgs->pDevice;
	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_FVFTEST_BASE;

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (0 == GetClientRect( pTestCaseArgs->hWnd, // HWND hWnd, 
	                        &ClientExtents))     // LPRECT lpRect 
	{
		DebugOut(DO_ERROR, L"GetClientRect failed. (hr = 0x%08x) Aborting.", 
		    HRESULT_FROM_WIN32(GetLastError()));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Scale factors
	//
	uiSX = ClientExtents.right;
	uiSY = ClientExtents.bottom;

	//
	// Verify that texture op, required for this test, is supported
	//
	if (!(Caps.TextureOpCaps & RequiredTextureOpCap))
	{
		DebugOut(DO_ERROR, L"Inadequate TextureOpCaps; Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Is specular lighting supported, if required?
	//
	if (!(Caps.DevCaps & D3DMDEVCAPS_SPECULAR))
	{
		if (D3DMFVF_SPECULAR & TestData[dwTableIndex].dwFVF)
		{
			DebugOut(DO_ERROR, L"Test case requires D3DMDEVCAPS_SPECULAR; Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Decode FVF bits specified for this test case, to determine desired
	// texture stage configuration
	//
	switch (D3DMFVF_TEXCOUNT_MASK & TestData[dwTableIndex].dwFVF)
	{
	case D3DMFVF_TEX1: 
		uiStageCount = uiNumTextures = 1;
		break;
	case D3DMFVF_TEX2: 
		uiStageCount = uiNumTextures = 2;
		break;
	case D3DMFVF_TEX3: 
		uiStageCount = uiNumTextures = 3;
		break;
	case D3DMFVF_TEX4: 
		uiStageCount = uiNumTextures = 4;
		break;
	default:
		uiStageCount = uiNumTextures = 0;
		break;
	}

	//
	// This test may use diffuse vertex data in the cascade
	//
	if (D3DMFVF_DIFFUSE & TestData[dwTableIndex].dwFVF)
	{
		uiStageCount++;
	}

	//
	// This test may use specular vertex data in the cascade
	//
	if (D3DMFVF_SPECULAR & TestData[dwTableIndex].dwFVF)
	{
		uiStageCount++;
	}

	//
	// Does the device support enough textures?
	//
	if (Caps.MaxSimultaneousTextures < uiNumTextures)
	{
		DebugOut(DO_ERROR, L"MaxSimultaneousTextures too small for this test case. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Does the device support enough blending stages?
	//
	if (Caps.MaxTextureBlendStages < uiStageCount)
	{
		DebugOut(DO_ERROR, L"MaxTextureBlendStages too small for this test case. Skipping.");
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Verify support for diffuse gouraud shading, if necessary
	//
	if (D3DMFVF_DIFFUSE & TestData[dwTableIndex].dwFVF)
	{
		if ((TestData[dwTableIndex].ShadeMode == D3DMSHADE_GOURAUD)  &&
			(!(D3DMPSHADECAPS_COLORGOURAUDRGB & Caps.ShadeCaps)))
		{
			DebugOut(DO_ERROR, L"Gouraud shading required; D3DMPSHADECAPS_COLORGOURAUDRGB not supported. Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Verify support for specular gouraud shading, if necessary
	//
	if (D3DMFVF_SPECULAR & TestData[dwTableIndex].dwFVF)
	{
		if ((TestData[dwTableIndex].ShadeMode == D3DMSHADE_GOURAUD)  &&
			(!(D3DMPSHADECAPS_SPECULARGOURAUDRGB & Caps.ShadeCaps)))
		{
			DebugOut(DO_ERROR, L"Gouraud shading required; D3DMPSHADECAPS_SPECULARGOURAUDRGB not supported. Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// If test requires projected texture coordinates, does driver support it?
	//
	if (TestData[dwTableIndex].bProjected)
	{
		if (!(Caps.TextureCaps & D3DMPTEXTURECAPS_PROJECTEDSUPPORT))
		{
			DebugOut(DO_ERROR, L"D3DMPTEXTURECAPS_PROJECTEDSUPPORT unavailable.  Skipping.");
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Prepare texture cascade with some defaults
	//
	if (FAILED(hr = SetDefaultTextureStates(pDevice)))
	{
		DebugOut(DO_ERROR, L"SetDefaultTextureStates failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Prepare texture cascade stages that use textures as input
	//
	for (uiCurrentStage = 0; uiCurrentStage < uiNumTextures; uiCurrentStage++)
	{
		switch(uiCurrentStage)
		{
		case 0:
			if (FAILED(hr = SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                             IDR_D3DMQA_TEX0, // INT iResourceID,
			                             &pTexture0,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                             0)))             // UINT uiTextureStage
			{
				DebugOut(DO_ERROR, L"SetupTexture failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		case 1:
			if (FAILED(hr = SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                             IDR_D3DMQA_TEX1, // INT iResourceID,
			                             &pTexture1,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                             1)))             // UINT uiTextureStage
			{
				DebugOut(DO_ERROR, L"SetupTexture failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		case 2:
			if (FAILED(hr = SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                             IDR_D3DMQA_TEX2, // INT iResourceID,
			                             &pTexture2,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                             2)))             // UINT uiTextureStage
			{
				DebugOut(DO_ERROR, L"SetupTexture failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		case 3:
			if (FAILED(hr = SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                             IDR_D3DMQA_TEX3, // INT iResourceID,
			                             &pTexture3,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                             3)))             // UINT uiTextureStage
			{
				DebugOut(DO_ERROR, L"SetupTexture failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		}

		//
		// Texture coords passed directly to rasterizer; unless specifically set to projected later
		//
		pDevice->SetTextureStageState( uiCurrentStage, D3DMTSS_TEXTURETRANSFORMFLAGS, D3DMTTFF_DISABLE );

		if (0 == uiCurrentStage)
		{
			if (FAILED(hr = SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
			                                     &FirstTexture,     // TEX_STAGE_SETTINGS TexStageSettings
			                                     uiCurrentStage)))  // UINT uiStageIndex
			{
				DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		else
		{
			if (FAILED(hr = SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
			                                     &GenTexture,       // TEX_STAGE_SETTINGS TexStageSettings
			                                     uiCurrentStage)))  // UINT uiStageIndex
			{
				DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
		}

	}

	//
	// This test may use diffuse vertex data in the cascade
	//
	if (D3DMFVF_DIFFUSE & TestData[dwTableIndex].dwFVF)
	{
		if (0 == uiCurrentStage)
		{
			if (FAILED(hr = SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											     &FirstDiffuse,     // TEX_STAGE_SETTINGS *pTexStageSettings
											     uiCurrentStage)))  // UINT uiStageIndex
			{
				DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		else
		{
			if (FAILED(hr = SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											     &GenDiffuse,       // TEX_STAGE_SETTINGS *pTexStageSettings
											     uiCurrentStage)))  // UINT uiStageIndex
			{
				DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		uiCurrentStage++;
	}


	//
	// This test may use specular vertex data in the cascade
	//
	if (D3DMFVF_SPECULAR & TestData[dwTableIndex].dwFVF)
	{
		if (0 == uiCurrentStage)
		{
			if (FAILED(hr = SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											     &FirstSpecular,    // TEX_STAGE_SETTINGS *pTexStageSettings
											     uiCurrentStage)))  // UINT uiStageIndex
			{
				DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		else
		{
			if (FAILED(hr = SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											     &GenSpecular,      // TEX_STAGE_SETTINGS *pTexStageSettings
											     uiCurrentStage)))  // UINT uiStageIndex
			{
				DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		uiCurrentStage++;
	}


	//
	// If projected texture coordinates are desired for last stage; set them up...
	//
	// The texture coordinates are all divided by the last element before being passed
	// to the rasterizer. For example, if D3DMTTFF_PROJECTED is specified with the
	// D3DMTTFF_COUNT3 flag, the first and second texture coordinates is divided by
	// the third coordinate before being passed to the rasterizer. 
	//
	if (TestData[dwTableIndex].bProjected)
	{
		if (FAILED(hr = pDevice->SetTextureStageState( TestData[dwTableIndex].uiTTFStage, D3DMTSS_TEXTURETRANSFORMFLAGS, TestData[dwTableIndex].dwTTF)))
		{
			DebugOut(DO_ERROR, L"SetTestTextureStates failed. (hr = 0x%08x) Aborting.", hr);
			Result = TPR_ABORT;
			goto cleanup;
		}
	}

	//
	// Scale to window size
	//
	if (D3DMFVF_XYZRHW_FLOAT & TestData[dwTableIndex].dwFVF)
	{
		for (uiShift = 0; uiShift < TestData[dwTableIndex].uiNumVerts; uiShift++)
		{
			pPosX = (PFLOAT)((PBYTE)(TestData[dwTableIndex].pVertexData) + uiShift * TestData[dwTableIndex].uiFVFSize);
			pPosY = &(pPosX[1]);

			(*pPosX) *= (float)(uiSX - 1);
			(*pPosY) *= (float)(uiSY - 1);

		}
	}

	//
	// Create and fill a vertex buffer
	//
	if (FAILED(hr = CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
	                                         &pVB,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                         TestData[dwTableIndex].pVertexData, // BYTE *pVertices,
	                                         TestData[dwTableIndex].uiFVFSize,   // UINT uiVertexSize,
	                                         TestData[dwTableIndex].uiNumVerts,  // UINT uiNumVertices,
	                                         TestData[dwTableIndex].dwFVF)))     // DWORD dwFVF
	{
		DebugOut(DO_ERROR, L"CreateActiveBuffer failed. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}


	//
	// "Unscale" from window size
	//
	if (D3DMFVF_XYZRHW_FLOAT & TestData[dwTableIndex].dwFVF)
	{
		for (uiShift = 0; uiShift < TestData[dwTableIndex].uiNumVerts; uiShift++)
		{
			pPosX = (PFLOAT)((PBYTE)(TestData[dwTableIndex].pVertexData) + uiShift * TestData[dwTableIndex].uiFVFSize);
			pPosY = &(pPosX[1]);

			(*pPosX) /= (float)(uiSX - 1);
			(*pPosY) /= (float)(uiSY - 1);

		}
	}

	//
	// Set up the shade mode
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, TestData[dwTableIndex].ShadeMode)))
	{
		DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_SHADEMODE) failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// The device *may* expose additional texture stage state constraints via ValidateDevice; for example, lack of support for some D3DMTA_* flag
	//
	if (FAILED(hr = pDevice->ValidateDevice(&dwNumPasses)))
	{
		DebugOut(DO_ERROR, L"ValidateDevice indicates that this device does not support the proposed set of texture stage states. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare test case permutation
	//
	if (FAILED(hr = pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255,0,0) , 1.0f, 0x00000000 )))
	{
		DebugOut(DO_ERROR, L"Clear failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(hr = pDevice->BeginScene()))
	{
		DebugOut(DO_ERROR, L"BeginScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DQA_PRIMTYPE,0,D3DQA_NUMPRIM)))
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(hr = pDevice->EndScene()))
	{
		DebugOut(DO_ERROR, L"EndScene failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(hr = pDevice->Present(NULL, NULL, NULL, NULL)))
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
	pDevice->SetTexture(0,NULL);
	pDevice->SetTexture(1,NULL);
	pDevice->SetTexture(2,NULL);
	pDevice->SetTexture(3,NULL);

	if (pVB)
		pVB->Release();


	return Result;
}
