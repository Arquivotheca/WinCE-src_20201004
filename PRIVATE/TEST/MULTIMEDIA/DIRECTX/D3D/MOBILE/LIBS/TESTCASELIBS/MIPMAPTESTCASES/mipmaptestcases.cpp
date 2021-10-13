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
#include "TestCases.h"
#include "ImageManagement.h"
#include "TextureTools.h"
#include "BufferTools.h"
#include "MipMapUtils.h"
#include "MipMapPerms.h"
#include "DebugOutput.h"
#include <tux.h>
#include <tchar.h>
#include "DebugOutput.h"

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

// 
// ExecuteMipMapRenderLoop
// 
//   This test iterates through a variety of primitive rendering loops that are designed
//   to expose particular mip-map levels.  Testing of LOD bias is optional.
//
//   Filter settings are specified by the caller.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying device
//   D3DMPRESENT_PARAMETERS* pPresentParms:  Parameters used with CreateDevice
//   HWND hWnd:  Target window
//   UINT uiInstanceHandle:  Identifier for image dumps
//   WCHAR *wszImageCmpDir:  Known good, test, and diff images belong in this path
//   UINT uiTestID:  Identifier for image dumps
//   D3DMTEXTUREFILTERTYPE MinFilter \
//   D3DMTEXTUREFILTERTYPE MagFilter  Tex filter settings
//   D3DMTEXTUREFILTERTYPE MipFilter /
//   FLOAT fBias:  Bias value for D3DTSS_MIPMAPLODBIAS
// 
// Return Value:  
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
INT ExecuteMipMapRenderLoop(LPDIRECT3DMOBILEDEVICE pDevice,
                            D3DMPRESENT_PARAMETERS* pPresentParms,
                            HWND hWnd,
                            UINT uiInstanceHandle,
                            WCHAR *wszImageCmpDir,
                            UINT uiTestID,
                            D3DMTEXTUREFILTERTYPE MinFilter,
                            D3DMTEXTUREFILTERTYPE MagFilter,
                            D3DMTEXTUREFILTERTYPE MipFilter, 
                            FLOAT fBias,
                            UINT uiNaturalMipLevel,
                            LPTESTCASEARGS pTestCaseArgs)
{

	LPDIRECT3DMOBILE pD3D = NULL;
	D3DMDISPLAYMODE DisplayMode;
	LPDIRECT3DMOBILESURFACE pRenderTargetSurface = NULL;
	LPDIRECT3DMOBILESURFACE pSurfaceBackBuffer = NULL;
	LPDIRECT3DMOBILESURFACE pCachedRenderTarget = NULL;
	D3DMVIEWPORT Viewport;

	//
	// Storage in which generated geometry algorithm indicates the specifications of the
	// resultant vertex buffer.
	//
	GEOMETRY_DESC Geometry;

	// 
	// Vertex buffer interface to pass into generated geometry function
	//
	LPDIRECT3DMOBILEVERTEXBUFFER pVB = NULL;

	//
	// Expected extents of a particular level in the mip-map
	//
	UINT uiExpectedLevelWidth, uiExpectedLevelHeight;

	//
	// Device Capabilities
	// 
	D3DMCAPS Caps;

	//
	// All failure conditions specifically set this
	//
	INT Result = TPR_PASS;

    HRESULT hr;

	//
	// StretchRect source and destination
	//
	RECT SourceRect, DestRect;

	//
	// Interface pointer for use with mip-map
	//
	LPDIRECT3DMOBILETEXTURE pTexture = NULL;

	//
	// Under certain rare circumstances, the capabilities of the device might prevent
	// this test case from running, despite the face that the device supports mip-
	// mapping.  In this unusual case, be sure to cause a SKIP rather than a FAIL.
	//
	if (FALSE == CapsAllowCreateTexture(pDevice,                          // LPDIRECT3DMOBILEDEVICE pDevice,
		                                D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X,  // UINT uiWidth,
		                                D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y)) // UINT uiHeight
	{
		//
		// Texture of the specified dimensions cannot be created on this device,
		// due to capabilities constraints
		//
		DebugOut(DO_ERROR, 
		    L"Device cannot create texture of required mipmap extents (%d x %d). Skipping.",
		    D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X,
		    D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y);
		Result = TPR_SKIP;
		goto cleanup;
	}

	Geometry.ppVB = &pVB;

	//
	// Compute the expected surface extents for this mip-map level
	//
	if (FAILED(hr = GetMipLevelExtents(D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X, // UINT uiTopWidth,
								       D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y, // UINT uiTopHeight,
								       uiNaturalMipLevel,               // UINT uiLevel,
								       &uiExpectedLevelWidth,           // UINT *puiLevelWidth,
								       &uiExpectedLevelHeight       ))) // UINT *puiLevelHeight
	{
		DebugOut(DO_ERROR, L"GetMipLevelExtents failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Arbitrarily use width as square extent, since width/height should be identical
	// 
	// This function generates a vertex buffer that is useful for testing a particular
	// mip-map level
	//
	if (FAILED(hr = SetupMipMapGeometry(pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
								        pPresentParms,          // D3DMPRESENT_PARAMETERS* pPresentParms,
								        hWnd,                   // HWND hWnd
								        &Geometry,              // GEOMETRY_DESC *pDesc,
								        uiExpectedLevelWidth))) // UINT uiSquareExtent
	{
		DebugOut(DO_ERROR, L"SetupMipMapGeometry failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Create a mip-map texture, and fill each of the subordinate surfaces
	//
	if (FAILED(hr = SetupMipMapContents(pDevice,          // LPDIRECT3DMOBILEDEVICE pDevice,
			                            &pTexture)))      // LPDIRECT3DMOBILETEXTURE *ppTexture)

	{
		DebugOut(DO_ERROR, L"SetupMipMapContents failed. (hr = 0x%08x) Failing.", hr);
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
	// Bind the mip-map to a texture stage
	//
	if (FAILED(hr = pDevice->SetTexture(0,       // DWORD Stage
								        pTexture))) // IDirect3DMobileBaseTexture *pTexture
	{
		DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	pDevice->SetTextureStageState(0, D3DMTSS_MIPMAPLODBIAS, *((LPDWORD) (&fBias)));

	pDevice->SetTextureStageState(0, D3DMTSS_MINFILTER, MinFilter);
	pDevice->SetTextureStageState(0, D3DMTSS_MAGFILTER, MagFilter);
	pDevice->SetTextureStageState(0, D3DMTSS_MIPFILTER, MipFilter);

	//
	// Retrieve Direct3D Mobile Object
	//
	if (FAILED(hr = pDevice->GetDirect3D(&pD3D))) // IDirect3DMobile** ppD3DM
	{
		DebugOut(DO_ERROR, L"GetDirect3D failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve display mode
	//
	if (FAILED(hr = pDevice->GetDisplayMode(&DisplayMode))) // D3DMDISPLAYMODE* pMode
	{
		DebugOut(DO_ERROR, L"GetDisplayMode failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// This should almost certainly succeed
	//
	if (FAILED(hr = pD3D->CheckDeviceFormat(Caps.AdapterOrdinal,    // UINT Adapter,
									        Caps.DeviceType,        // D3DMDEVTYPE DeviceType,
									        DisplayMode.Format,     // D3DMFORMAT AdapterFormat
									        D3DMUSAGE_RENDERTARGET, // ULONG Usage
									        D3DMRTYPE_SURFACE,      // D3DMRESOURCETYPE RType
									        DisplayMode.Format)))   // D3DMFORMAT CheckFormat
	{
		DebugOut(DO_ERROR, L"CheckDeviceFormat failed. (hr = 0x%08x) Skipping.", hr);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Done with Direct3D Mobile interface pointer
	//
	pD3D->Release();
	pD3D=NULL;

	//
	// Attempt the surface creation
	//
	if (FAILED(hr = pDevice->CreateRenderTarget(D3DQA_REQUIRED_SCREEN_EXTENT,  // UINT Width
										        D3DQA_REQUIRED_SCREEN_EXTENT,  // UINT Height
										        DisplayMode.Format,            // D3DMFORMAT Format
										        D3DMMULTISAMPLE_NONE,          // D3DMMULTISAMPLE_TYPE MultiSample
										        FALSE,                         // BOOL Lockable
										        &pRenderTargetSurface)))       // IDirect3DMobileSurface** ppSurface
	{
		DebugOut(DO_ERROR, L"CreateRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve current viewport
	//
	if (FAILED(hr = pDevice->GetViewport(&Viewport)))
	{
		DebugOut(DO_ERROR, L"GetViewport failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Currently this test generates TLVerts that cover a required extent of
	// D3DQA_REQUIRED_VIEWPORT_EXTENT x D3DQA_REQUIRED_VIEWPORT_EXTENT
	//
	if ((Viewport.Width < D3DQA_REQUIRED_VIEWPORT_EXTENT) || (Viewport.Height < D3DQA_REQUIRED_VIEWPORT_EXTENT))
	{
		DebugOut(DO_ERROR, 
		    L"Test needs a viewport of at least size (%lu,%lu) to run. Skipping.", 
		    D3DQA_REQUIRED_VIEWPORT_EXTENT, 
		    D3DQA_REQUIRED_VIEWPORT_EXTENT);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Retrieve the render target interface pointer, to re-use later
	//
	if (FAILED(hr = pDevice->GetRenderTarget(&pCachedRenderTarget))) // IDirect3DMobileSurface** ppRenderTarget
	{
		DebugOut(DO_ERROR, L"GetRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Render to alternative target
	//
	if (FAILED(hr = pDevice->SetRenderTarget(pRenderTargetSurface,    // IDirect3DMobileSurface* pRenderTarget
										     NULL     )))          // IDirect3DMobileSurface* pNewZStencil
	{
		DebugOut(DO_ERROR, L"SetRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

    //
    // Make sure the device can support the given settings
    //
    DWORD dwNumPasses = 0;
    if (FAILED(hr = pDevice->ValidateDevice(&dwNumPasses)))
    {
        DebugOut(DO_ERROR, L"ValidateDevice failed. (hr = 0x%08x) Skipping.", hr);
        Result = TPR_SKIP;
        goto cleanup;
    }

	//
	//
	//
	if (FAILED(hr = pDevice->ColorFill(pRenderTargetSurface,  // IDirect3DMobileSurface* pSurface
								       NULL,                  // CONST RECT* pRect
								       0xFFFF0000)))          // D3DMCOLOR Color
	{
		DebugOut(DO_ERROR, L"SetRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
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
	// Indicate primitive data to be drawn
	//
	if (FAILED(hr = pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,     // D3DMPRIMITIVETYPE Type,
									       0,                      // UINT StartVertex,
									       Geometry.uiPrimCount))) // UINT PrimitiveCount
	{
		DebugOut(DO_ERROR, L"DrawPrimitive failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
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
	// Return to original
	//
	if (FAILED(hr = pDevice->SetRenderTarget(pCachedRenderTarget,        // IDirect3DMobileSurface* pRenderTarget
										     NULL     )))             // IDirect3DMobileSurface* pNewZStencil
	{
		DebugOut(DO_ERROR, L"SetRenderTarget failed. (hr = 0x%08x) Aborting.", hr);
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Get backbuffer for StretchRect target
	//
	if (FAILED(hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pSurfaceBackBuffer)))
	{
		DebugOut(DO_ERROR, L"GetBackBuffer failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	SourceRect.left = 0;
	SourceRect.top  = 0;
	SourceRect.right  = D3DQA_REQUIRED_SCREEN_EXTENT;
	SourceRect.bottom = D3DQA_REQUIRED_SCREEN_EXTENT;

	//
	// Copy render target contents into "spaced grid" pattern on backbuffer, for presentation.  Spacing
	// is arbitrary.
	//
	for (DestRect.left = 5;
		 ((ULONG)DestRect.left + D3DQA_REQUIRED_SCREEN_EXTENT+ 5) < Viewport.Width;
		 DestRect.left += D3DQA_REQUIRED_SCREEN_EXTENT+10)
	{
		for (DestRect.top = 5;
			 ((ULONG)DestRect.top + D3DQA_REQUIRED_SCREEN_EXTENT + 5) < Viewport.Height;
			 DestRect.top += D3DQA_REQUIRED_SCREEN_EXTENT+10)
		{
			DestRect.right  = DestRect.left + D3DQA_REQUIRED_SCREEN_EXTENT;
			DestRect.bottom = DestRect.top + D3DQA_REQUIRED_SCREEN_EXTENT;

			if (FAILED(hr = pDevice->StretchRect(pRenderTargetSurface,  // IDirect3DMobileSurface* pSourceSurface
											     &SourceRect,           // CONST RECT* pSourceRect
											     pSurfaceBackBuffer,    // IDirect3DMobileSurface* pDestSurface
											     &DestRect,             // CONST RECT* pDestRect
											     D3DMTEXF_NONE)))       // D3DMTEXTUREFILTERTYPE Filter
			{
				DebugOut(DO_ERROR, L"StretchRect failed. (hr = 0x%08x) Failing.", hr);
				Result = TPR_FAIL;
				goto cleanup;
			}

		}
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

	//
	// Remove texture ref-count due to prior SetTexture call
	//
	if (FAILED(hr = pDevice->SetTexture(0,      // DWORD Stage
								        NULL))) // IDirect3DMobileBaseTexture *pTexture
	{
		DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Remove vertex buffer reference count due to prior SetStreamSource call
	//
	if (FAILED(hr = pDevice->SetStreamSource(0,NULL,0)))
	{
		DebugOut(DO_ERROR, L"SetStreamSource failed. (hr = 0x%08x) Failing.", hr);
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Release interfaces that are re-generated for each iteration
	//
	if (NULL != pVB)
	{
		if (0 != pVB->Release())
		{
			DebugOut(DO_ERROR, L"Unexpected refcount at Release.");
		}
		pVB = NULL;
	}
	if (NULL != pTexture)
	{
		if (0 != pTexture->Release())
		{
			DebugOut(DO_ERROR, L"Unexpected refcount at Release.");
		}
		pTexture = NULL;
	}

cleanup:

	if (pTexture)
		pTexture->Release();

	if (pVB)
		pVB->Release();
	
	if (pD3D)
		pD3D->Release();

	if (pRenderTargetSurface)
		pRenderTargetSurface->Release();

	if (pSurfaceBackBuffer)
		pSurfaceBackBuffer->Release();

	return Result;
}


TESTPROCAPI MipMapTest(LPTESTCASEARGS pTestCaseArgs)
{

	//
	// Device capabilities; for inspecting filtering support
	//
	D3DMCAPS Caps;

    HRESULT hr;
    
	//
	// Adjust to index of mip map test case permutation table
	//
	#pragma prefast(suppress: 12018, "Possible integer overflow: Correct values are verified in the following if statement")
	DWORD dwTableIndex = pTestCaseArgs->dwTestIndex-D3DMQA_MIPMAPTEST_BASE;

	//
	// Range check; ensure valid test case ID
	//
	if(((DWORD)D3DMQA_MIPMAPTEST_COUNT) <= dwTableIndex)
	{
		DebugOut(DO_ERROR, L"Invalid test case ordinal. Aborting.");
		return TPR_ABORT;
	}

	//
	// Bad-parameter checks
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		DebugOut(DO_ERROR, L"Invalid argument(s). Aborting.");
		return TPR_ABORT;
	}

	//
	// Get device capabilities
	//
	if (FAILED(hr = pTestCaseArgs->pDevice->GetDeviceCaps(&Caps)))
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. (hr = 0x%08x) Aborting.", hr);
		return TPR_ABORT;
	}

	//
	// Verify that device supports mip-mapping
	//
	if (!(Caps.TextureCaps & D3DMPTEXTURECAPS_MIPMAP))
	{
		DebugOut(DO_ERROR, L"No support for D3DMPTEXTURECAPS_MIPMAP. Skipping.");
		return TPR_SKIP;
	}

	//
	// Verify two prerequisites:
	//
	//   (1) Filter type must be valid for minification
	//   (2) Filter type must be supported by the driver
	//
	switch(TestCasePerms[dwTableIndex].MinFilter)
	{
	case D3DMTEXF_POINT:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MINFPOINT))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MINFPOINT not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_LINEAR:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MINFLINEAR not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_ANISOTROPIC:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MINFANISOTROPIC))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MINFANISOTROPIC not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_NONE: // Always valid
		DebugOut(DO_ERROR, L"Turning min filtering off.");
		break;
	default:
		DebugOut(DO_ERROR, L"Invalid texture filter type. Skipping.");
		return TPR_SKIP;
	}

	//
	// Verify two prerequisites:
	//
	//   (1) Filter type must be valid for magnification
	//   (2) Filter type must be supported by the driver
	//
	switch(TestCasePerms[dwTableIndex].MagFilter)
	{
	case D3DMTEXF_POINT:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MAGFPOINT))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MAGFPOINT not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_LINEAR:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MAGFLINEAR not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_ANISOTROPIC:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MAGFANISOTROPIC))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MAGFANISOTROPIC not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_NONE: // Always valid
		DebugOut(DO_ERROR, L"Turning mag filtering off.");
		break;
	default:
		DebugOut(DO_ERROR, L"Invalid texture filter type. Skipping.");
		return TPR_SKIP;
	}


	//
	// Verify two prerequisites:
	//
	//   (1) Filter type must be valid for mip-maps
	//   (2) Filter type must be supported by the driver
	//
	switch(TestCasePerms[dwTableIndex].MipFilter)
	{
	case D3DMTEXF_POINT:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MIPFPOINT))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MIPFPOINT not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_LINEAR:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MIPFLINEAR))
		{
			DebugOut(DO_ERROR, L"D3DMPTFILTERCAPS_MIPFLINEAR not supported. Skipping.");
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_NONE: // Always valid
		DebugOut(DO_ERROR, L"Turning mip filtering off.");
		break;
	default:
		DebugOut(DO_ERROR, L"Invalid texture filter type. Skipping.");
		return TPR_SKIP;
	}

	if (0 != TestCasePerms[dwTableIndex].fBias)
	{
		//
		// Verify that device supports LOD bias
		//
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_MIPMAPLODBIAS))
		{
			DebugOut(DO_ERROR, L"No D3DMPRASTERCAPS_MIPMAPLODBIAS support. Skipping.");
			return TPR_SKIP;
		}
	}

	return ExecuteMipMapRenderLoop(pTestCaseArgs->pDevice,                   // LPDIRECT3DMOBILEDEVICE pDevice
	                               pTestCaseArgs->pParms,                    // D3DMPRESENT_PARAMETERS* pPresentParms,
	                               pTestCaseArgs->hWnd,                      // HWND hWnd
	                               pTestCaseArgs->uiInstance,                // UINT uiInstanceHandle,
	                               pTestCaseArgs->pwszImageComparitorDir,    // WCHAR *wszImageCmpDir,
	                               pTestCaseArgs->dwTestIndex,               // UINT uiTestID
	                               TestCasePerms[dwTableIndex].MinFilter,    // D3DMTEXTUREFILTERTYPE MinFilter
	                               TestCasePerms[dwTableIndex].MagFilter,    // D3DMTEXTUREFILTERTYPE MagFilter
	                               TestCasePerms[dwTableIndex].MipFilter,    // D3DMTEXTUREFILTERTYPE MipFilter
	                               TestCasePerms[dwTableIndex].fBias,        // FLOAT fBias
	                               TestCasePerms[dwTableIndex].dwLevel,      // UINT uiNaturalMipLevel
	                               pTestCaseArgs);                           // LPTESTCASEARGS pTestCaseArgs

}

