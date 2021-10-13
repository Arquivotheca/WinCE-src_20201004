//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <d3dm.h>
#include <tchar.h>
#include <tux.h>
#include "BufferTools.h"
#include "FVFStyles.h"
#include "ImageManagement.h"
#include "utils.h"
#include "FVFResources.h"

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
	OutputDebugString(_T("FVFTest starting\n"));

	//
	// Texture op that this test case requires
	//
	CONST DWORD RequiredTextureOpCap = D3DMTEXOPCAPS_MODULATE | D3DMTEXOPCAPS_SELECTARG1;

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
		OutputDebugString(_T("Invalid argument(s)."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	pDevice = pTestCaseArgs->pDevice;
	dwTableIndex = pTestCaseArgs->dwTestIndex - D3DMQA_FVFTEST_BASE;

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (0 == GetClientRect( pTestCaseArgs->hWnd, // HWND hWnd, 
	                        &ClientExtents))     // LPRECT lpRect 
	{
		OutputDebugString(_T("GetClientRect failed.\n"));
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
		OutputDebugString(_T("Inadequate TextureOpCaps; skipping.\n"));
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
			OutputDebugString(L"Test case requires D3DMDEVCAPS_SPECULAR; skipping.\n");
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
		OutputDebugString(_T("MaxSimultaneousTextures too small for this test case.\n"));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Does the device support enough blending stages?
	//
	if (Caps.MaxTextureBlendStages < uiStageCount)
	{
		OutputDebugString(_T("MaxTextureBlendStages too small for this test case.\n"));
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
			OutputDebugString(_T("Gouraud shading required; D3DMPSHADECAPS_COLORGOURAUDRGB not supported.\n"));
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
			OutputDebugString(_T("Gouraud shading required; D3DMPSHADECAPS_SPECULARGOURAUDRGB not supported.\n"));
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
			OutputDebugString(_T("D3DMPTEXTURECAPS_PROJECTEDSUPPORT unavailable.  Skipping test.\n"));
			Result = TPR_SKIP;
			goto cleanup;
		}
	}

	//
	// Prepare texture cascade with some defaults
	//
	if (FAILED(SetDefaultTextureStates(pDevice)))
	{
		OutputDebugString(_T("SetDefaultTextureStates failed.\n"));
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
			if (FAILED(SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                        IDR_D3DMQA_TEX0, // INT iResourceID,
			                        &pTexture0,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                        0)))             // UINT uiTextureStage
			{
				OutputDebugString(_T("SetupTexture failed.\n"));
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		case 1:
			if (FAILED(SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                        IDR_D3DMQA_TEX1, // INT iResourceID,
			                        &pTexture1,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                        1)))             // UINT uiTextureStage
			{
				OutputDebugString(_T("SetupTexture failed.\n"));
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		case 2:
			if (FAILED(SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                        IDR_D3DMQA_TEX2, // INT iResourceID,
			                        &pTexture2,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                        2)))             // UINT uiTextureStage
			{
				OutputDebugString(_T("SetupTexture failed.\n"));
				Result = TPR_ABORT;
				goto cleanup;
			}
			break;
		case 3:
			if (FAILED(SetupTexture(pDevice,         // LPDIRECT3DMOBILEDEVICE pDevice
			                        IDR_D3DMQA_TEX3, // INT iResourceID,
			                        &pTexture3,      // LPDIRECT3DMOBILETEXTURE *ppTexture
			                        3)))             // UINT uiTextureStage
			{
				OutputDebugString(_T("SetupTexture failed.\n"));
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
			if (FAILED(SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
			                                &FirstTexture,     // TEX_STAGE_SETTINGS TexStageSettings
			                                uiCurrentStage)))  // UINT uiStageIndex
			{
				OutputDebugString(_T("SetTestTextureStates failed.\n"));
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		else
		{
			if (FAILED(SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
			                                &GenTexture,       // TEX_STAGE_SETTINGS TexStageSettings
			                                uiCurrentStage)))  // UINT uiStageIndex
			{
				OutputDebugString(_T("SetTestTextureStates failed.\n"));
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
			if (FAILED(SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											&FirstDiffuse,     // TEX_STAGE_SETTINGS *pTexStageSettings
											uiCurrentStage)))  // UINT uiStageIndex
			{
				OutputDebugString(_T("SetTestTextureStates failed.\n"));
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		else
		{
			if (FAILED(SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											&GenDiffuse,       // TEX_STAGE_SETTINGS *pTexStageSettings
											uiCurrentStage)))  // UINT uiStageIndex
			{
				OutputDebugString(_T("SetTestTextureStates failed.\n"));
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
			if (FAILED(SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											&FirstSpecular,    // TEX_STAGE_SETTINGS *pTexStageSettings
											uiCurrentStage)))  // UINT uiStageIndex
			{
				OutputDebugString(_T("SetTestTextureStates failed.\n"));
				Result = TPR_ABORT;
				goto cleanup;
			}
		}
		else
		{
			if (FAILED(SetTestTextureStates(pDevice,           // LPDIRECT3DMOBILEDEVICE pDevice,
											&GenSpecular,      // TEX_STAGE_SETTINGS *pTexStageSettings
											uiCurrentStage)))  // UINT uiStageIndex
			{
				OutputDebugString(_T("SetTestTextureStates failed.\n"));
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
		if (FAILED(pDevice->SetTextureStageState( TestData[dwTableIndex].uiTTFStage, D3DMTSS_TEXTURETRANSFORMFLAGS, TestData[dwTableIndex].dwTTF)))
		{
			OutputDebugString(_T("SetTestTextureStates failed.\n"));
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
	if (FAILED(CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
	                                    &pVB,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                    TestData[dwTableIndex].pVertexData, // BYTE *pVertices,
	                                    TestData[dwTableIndex].uiFVFSize,   // UINT uiVertexSize,
	                                    TestData[dwTableIndex].uiNumVerts,  // UINT uiNumVertices,
	                                    TestData[dwTableIndex].dwFVF)))     // DWORD dwFVF
	{
		OutputDebugString(_T("CreateActiveBuffer failed.\n"));
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
	if (FAILED(pDevice->SetRenderState(D3DMRS_SHADEMODE, TestData[dwTableIndex].ShadeMode)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE) failed.\n"));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// The device *may* expose additional texture stage state constraints via ValidateDevice; for example, lack of support for some D3DMTA_* flag
	//
	if (FAILED(pDevice->ValidateDevice(&dwNumPasses)))
	{
		OutputDebugString(_T("ValidateDevice indicates that this device does not support the proposed set of texture stage states.\n"));
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Prepare test case permutation
	//
	if (FAILED(pDevice->Clear( 0L, NULL, D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, D3DMCOLOR_XRGB(255,0,0) , 1.0f, 0x00000000 )))
	{
		OutputDebugString(_T("Clear failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(pDevice->BeginScene()))
	{
		OutputDebugString(_T("BeginScene failed.\n"));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Draw geometry
	//
	if (FAILED(pDevice->DrawPrimitive(D3DQA_PRIMTYPE,0,D3DQA_NUMPRIM)))
	{
		OutputDebugString(_T("DrawPrimitive failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(_T("EndScene failed.\n"));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(_T("Present failed.\n"));
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
	pDevice->SetTexture(0,NULL);
	pDevice->SetTexture(1,NULL);
	pDevice->SetTexture(2,NULL);
	pDevice->SetTexture(3,NULL);

	if (pVB)
		pVB->Release();


	return Result;
}
