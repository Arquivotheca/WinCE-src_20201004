//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
		OutputDebugString(_T("Skipping mip-mapping test due to D3DMCAPs constraints."));
		Result = TPR_SKIP;
		goto cleanup;
	}

	Geometry.ppVB = &pVB;

	//
	// Compute the expected surface extents for this mip-map level
	//
	if (FAILED(GetMipLevelExtents(D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X, // UINT uiTopWidth,
								D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y, // UINT uiTopHeight,
								uiNaturalMipLevel,               // UINT uiLevel,
								&uiExpectedLevelWidth,           // UINT *puiLevelWidth,
								&uiExpectedLevelHeight       ))) // UINT *puiLevelHeight
	{
		OutputDebugString(_T("GetMipLevelExtents failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Arbitrarily use width as square extent, since width/height should be identical
	// 
	// This function generates a vertex buffer that is useful for testing a particular
	// mip-map level
	//
	if (FAILED(SetupMipMapGeometry(pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
								   pPresentParms,          // D3DMPRESENT_PARAMETERS* pPresentParms,
								   hWnd,                   // HWND hWnd
								   &Geometry,              // GEOMETRY_DESC *pDesc,
								   uiExpectedLevelWidth))) // UINT uiSquareExtent
	{
		OutputDebugString(_T("SetupMipMapGeometry failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Create a mip-map texture, and fill each of the subordinate surfaces
	//
	if (FAILED(SetupMipMapContents(pDevice,          // LPDIRECT3DMOBILEDEVICE pDevice,
			                       &pTexture)))      // LPDIRECT3DMOBILETEXTURE *ppTexture)

	{
		OutputDebugString(_T("SetupMipMapContents failed."));
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
	if (FAILED(pDevice->SetTexture(0,       // DWORD Stage
								pTexture))) // IDirect3DMobileBaseTexture *pTexture
	{
		OutputDebugString(_T("SetTexture failed."));
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
	if (FAILED(pDevice->GetDirect3D(&pD3D))) // IDirect3DMobile** ppD3DM
	{
		OutputDebugString(_T("GetDirect3D failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve display mode
	//
	if (FAILED(pDevice->GetDisplayMode(&DisplayMode))) // D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("GetDisplayMode failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve device capabilities
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		OutputDebugString(L"GetDeviceCaps failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// This should almost certainly succeed
	//
	if (FAILED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal,    // UINT Adapter,
									   Caps.DeviceType,        // D3DMDEVTYPE DeviceType,
									   DisplayMode.Format,     // D3DMFORMAT AdapterFormat
									   D3DMUSAGE_RENDERTARGET, // ULONG Usage
									   D3DMRTYPE_SURFACE,      // D3DMRESOURCETYPE RType
									   DisplayMode.Format)))   // D3DMFORMAT CheckFormat
	{
		OutputDebugString(L"CheckDeviceFormat failed.\n");
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
	if (FAILED(pDevice->CreateRenderTarget(D3DQA_REQUIRED_SCREEN_EXTENT,  // UINT Width
										   D3DQA_REQUIRED_SCREEN_EXTENT,  // UINT Height
										   DisplayMode.Format,            // D3DMFORMAT Format
										   D3DMMULTISAMPLE_NONE,          // D3DMMULTISAMPLE_TYPE MultiSample
										   FALSE,                         // BOOL Lockable
										   &pRenderTargetSurface)))       // IDirect3DMobileSurface** ppSurface
	{
		OutputDebugString(L"CreateRenderTarget failed.\n");
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Retrieve current viewport
	//
	if (FAILED(pDevice->GetViewport(&Viewport)))
	{
		OutputDebugString(_T("GetViewport failed."));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Currently this test generates TLVerts that cover a required extent of
	// D3DQA_REQUIRED_VIEWPORT_EXTENT x D3DQA_REQUIRED_VIEWPORT_EXTENT
	//
	if ((Viewport.Width < D3DQA_REQUIRED_VIEWPORT_EXTENT) || (Viewport.Height < D3DQA_REQUIRED_VIEWPORT_EXTENT))
	{
		DebugOut(_T("Test needs a viewport of at least size (%lu,%lu) to run; skipping.\n"), D3DQA_REQUIRED_VIEWPORT_EXTENT, D3DQA_REQUIRED_VIEWPORT_EXTENT);
		Result = TPR_SKIP;
		goto cleanup;
	}

	//
	// Retrieve the render target interface pointer, to re-use later
	//
	if (FAILED(pDevice->GetRenderTarget(&pCachedRenderTarget))) // IDirect3DMobileSurface** ppRenderTarget
	{
		OutputDebugString(_T("GetRenderTarget failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Render to alternative target
	//
	if (FAILED(pDevice->SetRenderTarget(pRenderTargetSurface,    // IDirect3DMobileSurface* pRenderTarget
										   NULL     )))          // IDirect3DMobileSurface* pNewZStencil
	{
		OutputDebugString(_T("SetRenderTarget failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	//
	//
	if (FAILED(pDevice->ColorFill(pRenderTargetSurface,  // IDirect3DMobileSurface* pSurface
								  NULL,                  // CONST RECT* pRect
								  0xFFFF0000)))          // D3DMCOLOR Color
	{
		OutputDebugString(_T("SetRenderTarget failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Enter a scene; indicates that drawing is about to occur
	//
	if (FAILED(pDevice->BeginScene()))
	{
		OutputDebugString(_T("BeginScene failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}


	//
	// Indicate primitive data to be drawn
	//
	if (FAILED(pDevice->DrawPrimitive(D3DMPT_TRIANGLELIST,     // D3DMPRIMITIVETYPE Type,
									0,                      // UINT StartVertex,
									Geometry.uiPrimCount))) // UINT PrimitiveCount
	{
		OutputDebugString(_T("DrawPrimitive failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Exit this scene; drawing is complete for this frame
	//
	if (FAILED(pDevice->EndScene()))
	{
		OutputDebugString(_T("EndScene failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Return to original
	//
	if (FAILED(pDevice->SetRenderTarget(pCachedRenderTarget,        // IDirect3DMobileSurface* pRenderTarget
										   NULL     )))             // IDirect3DMobileSurface* pNewZStencil
	{
		OutputDebugString(_T("SetRenderTarget failed.\n"));
		Result = TPR_ABORT;
		goto cleanup;
	}

	//
	// Get backbuffer for StretchRect target
	//
	if (FAILED(pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pSurfaceBackBuffer)))
	{
		OutputDebugString(_T("GetBackBuffer failed."));
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

			if (FAILED(pDevice->StretchRect(pRenderTargetSurface,  // IDirect3DMobileSurface* pSourceSurface
											&SourceRect,           // CONST RECT* pSourceRect
											pSurfaceBackBuffer,    // IDirect3DMobileSurface* pDestSurface
											&DestRect,             // CONST RECT* pDestRect
											D3DMTEXF_NONE)))       // D3DMTEXTUREFILTERTYPE Filter
			{
				OutputDebugString(_T("StretchRect failed."));
				Result = TPR_FAIL;
				goto cleanup;
			}

		}
	}


	//
	// Presents the contents of the next buffer in the sequence of
	// back buffers owned by the device.
	//
	if (FAILED(pDevice->Present(NULL, NULL, NULL, NULL)))
	{
		OutputDebugString(_T("Present failed."));
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

	//
	// Remove texture ref-count due to prior SetTexture call
	//
	if (FAILED(pDevice->SetTexture(0,      // DWORD Stage
								NULL))) // IDirect3DMobileBaseTexture *pTexture
	{
		OutputDebugString(_T("SetTexture failed."));
		Result = TPR_FAIL;
		goto cleanup;
	}

	//
	// Remove vertex buffer reference count due to prior SetStreamSource call
	//
	if (FAILED(pDevice->SetStreamSource(0,NULL,0)))
	{
		OutputDebugString(_T("SetStreamSource failed."));
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
			OutputDebugString(_T("Unexpected refcount at Release."));
		}
		pVB = NULL;
	}
	if (NULL != pTexture)
	{
		if (0 != pTexture->Release())
		{
			OutputDebugString(_T("Unexpected refcount at Release."));
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

	//
	// Adjust to index of mip map test case permutation table
	//
	DWORD dwTableIndex = pTestCaseArgs->dwTestIndex-D3DMQA_MIPMAPTEST_BASE;

	//
	// Range check; ensure valid test case ID
	//
	if(D3DMQA_MIPMAPTEST_COUNT <= dwTableIndex)
	{
		OutputDebugString(_T("Invalid test case ordinal."));
		return TPR_ABORT;
	}

	//
	// Bad-parameter checks
	//
	if ((NULL == pTestCaseArgs) || (NULL == pTestCaseArgs->pParms) || (NULL == pTestCaseArgs->pDevice))
	{
		OutputDebugString(_T("Invalid argument(s)."));
		return TPR_ABORT;
	}

	//
	// Get device capabilities
	//
	if (FAILED(pTestCaseArgs->pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return TPR_ABORT;
	}

	//
	// Verify that device supports mip-mapping
	//
	if (!(Caps.TextureCaps & D3DMPTEXTURECAPS_MIPMAP))
	{
		OutputDebugString(_T("No support for D3DMPTEXTURECAPS_MIPMAP."));
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
			OutputDebugString(_T("D3DMPTFILTERCAPS_MINFPOINT not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_LINEAR:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR))
		{
			OutputDebugString(_T("D3DMPTFILTERCAPS_MINFLINEAR not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_ANISOTROPIC:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MINFANISOTROPIC))
		{
			OutputDebugString(_T("D3DMPTFILTERCAPS_MINFANISOTROPIC not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_NONE: // Always valid
		OutputDebugString(_T("Turning min filtering off."));
		break;
	default:
		OutputDebugString(_T("Invalid texture filter type."));
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
			OutputDebugString(_T("D3DMPTFILTERCAPS_MAGFPOINT not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_LINEAR:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR))
		{
			OutputDebugString(_T("D3DMPTFILTERCAPS_MAGFLINEAR not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_ANISOTROPIC:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MAGFANISOTROPIC))
		{
			OutputDebugString(_T("D3DMPTFILTERCAPS_MAGFANISOTROPIC not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_NONE: // Always valid
		OutputDebugString(_T("Turning mag filtering off."));
		break;
	default:
		OutputDebugString(_T("Invalid texture filter type."));
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
			OutputDebugString(_T("D3DMPTFILTERCAPS_MIPFPOINT not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_LINEAR:
		if (!(Caps.TextureFilterCaps & D3DMPTFILTERCAPS_MIPFLINEAR))
		{
			OutputDebugString(_T("D3DMPTFILTERCAPS_MIPFLINEAR not supported."));
			return TPR_SKIP;
		}
		break;
	case D3DMTEXF_NONE: // Always valid
		OutputDebugString(_T("Turning mip filtering off."));
		break;
	default:
		OutputDebugString(_T("Invalid texture filter type."));
		return TPR_SKIP;
	}

	if (0 != TestCasePerms[dwTableIndex].fBias)
	{
		//
		// Verify that device supports LOD bias
		//
		if (!(Caps.RasterCaps & D3DMPRASTERCAPS_MIPMAPLODBIAS))
		{
			OutputDebugString(_T("No D3DMPRASTERCAPS_MIPMAPLODBIAS support."));
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

