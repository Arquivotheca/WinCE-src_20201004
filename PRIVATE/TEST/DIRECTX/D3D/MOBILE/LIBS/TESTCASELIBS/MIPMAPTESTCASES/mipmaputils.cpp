//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "MipMapUtils.h"
#include "BufferTools.h"
#include "TextureTools.h"
#include "FVFResources.h"
#include <tchar.h>

//
// Allow prefast pragmas
//
#pragma warning(disable:4068)  // warning 4068: unknown pragma

//
// SetupMipMapContents
//
//   Creates a mip-mapped texture, given the defined top-level extents.  Selects an
//   auto-detected valid format for textures.  Selects an auto-detected valid mip-map
//   filtering type.  Verifies, after mip-map creation, that the correct number of
//   subordinate levels were created, and that the extents of each level are correct.
//   This helps to ensure reliable test case execution.
//
//   The caller retains the responsibility of verifying that the specified extents are
//   not precluded by any D3DMCAPS texture extent constraints.  The recommended
//   technique, to verify this, is to call CapsAllowCreateTexture prior to calling
//   this function.  The reasoning for this is that a D3DMCAPS setting that precludes
//   these particular dimensions from being created, is likely to be a case that warrants
//   a test result of TPR_SKIP rather than TPR_FAIL, which should be handled in the test
//   case itself rather than this utility function.
//
//   The caller also retains the responsibility of verifying that mip-mapping and
//   texturing are both supported.
//
// Arguments:
//
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D Device
//   LPDIRECT3DMOBILETEXTURE *ppTexture:  Output for created/filled mip-map interface pointer
//   
// Return Value:  
//   
//   HRESULT indicates success or failure
//
HRESULT SetupMipMapContents(LPDIRECT3DMOBILEDEVICE pDevice, LPDIRECT3DMOBILETEXTURE *ppTexture)
{
	//
	// All failure conditions are set specifically
	//
	HRESULT hr = S_OK;

	//
	// Interface pointer for use with subordinate mip-map surfaces
	//
	LPDIRECT3DMOBILESURFACE pSurface = NULL;

	//
	// Surface description for an individual texture level
	//
	D3DMSURFACE_DESC Desc;

	//
	// Iterator for inspecting texture levels
	//
	UINT uiLevelDescIter;

	//
	// Indicates the expected extents of a automatically-generated level, for this particular
	// call to CreateTexture
	//
	UINT uiExpectedLevelWidth, uiExpectedLevelHeight;

	//
	// D3DMFORMAT iterator; to cycle through formats until a valid format
	// is found.
	//
	D3DMFORMAT FormatIter;

	//
	// The display mode's spatial resolution, color resolution,
	// and refresh frequency.
	//
	D3DMDISPLAYMODE Mode;

	//
	// Number of levels generated automatically
	//
	DWORD dwLevelCount;

	//
	// D3DM Object interface pointer
	//
	LPDIRECT3DMOBILE pD3D = NULL;

	//
	// There must be at least one valid format.
	// The first time a valid format is found, this bool is toggled.
	//
	// If the bool is never toggled, the test will fail after attempting
	// all possible formats.
	//
	bool bFoundValidFormat = false;

	//
	// Device capabilities
	//
	D3DMCAPS Caps;

	//
	// Pool for texture creation
	//
	D3DMPOOL TexturePool;

	//
	// Parameter validation
	//
	if ((NULL == ppTexture) || (NULL == pDevice))
	{
		OutputDebugString(_T("Invalid argument."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Initial setting is NULL; to ensure that cleanup can be handled correctly,
	// in failure cases
	//
	*ppTexture = NULL;
	

	//
	// Get device capabilities to check for mip-mapping support
	//
	if (FAILED(pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		return E_FAIL;
	}

	//
	// Find a valid texture pool
	//
	if (D3DMSURFCAPS_SYSTEXTURE & Caps.SurfaceCaps)
	{
		TexturePool = D3DMPOOL_SYSTEMMEM;
	}
	else if (D3DMSURFCAPS_VIDTEXTURE & Caps.SurfaceCaps)
	{
		TexturePool = D3DMPOOL_VIDEOMEM;
	}
	else
	{
		OutputDebugString(_T("D3DMCAPS.SurfaceCaps:  No valid texture pool."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Retrieve current display mode
	//
	if (FAILED(pDevice->GetDisplayMode(&Mode))) // D3DMDISPLAYMODE* pMode
	{
		OutputDebugString(_T("GetDisplayMode failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Object interface pointer needed for CheckDeviceFormat
	//
	if (FAILED(pDevice->GetDirect3D(&pD3D)))
	{
		OutputDebugString(_T("GetDirect3D failed."));
		hr = E_FAIL;
		goto cleanup;
	}


	//
	// Find a format supported for textures
	//
	for (FormatIter = (D3DMFORMAT)0; FormatIter < D3DMFMT_NUMFORMAT; (*(ULONG*)(&FormatIter))++)
	{
		if (!(FAILED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal, // UINT Adapter
									         Caps.DeviceType,     // D3DMDEVTYPE DeviceType
									         Mode.Format,         // D3DMFORMAT AdapterFormat
									         D3DMUSAGE_LOCKABLE,  // ULONG Usage
									         D3DMRTYPE_TEXTURE,   // D3DMRESOURCETYPE RType
									         FormatIter))))       // D3DMFORMAT CheckFormat
		{
			bFoundValidFormat = true;
			break;
		}
	}

	if (false == bFoundValidFormat)
	{
		OutputDebugString(_T("No valid texture format."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Create a mip-map with automatically-generated levels
	//
	if (FAILED(pDevice->CreateTexture(D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X, // UINT Width,
	                                  D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y, // UINT Height,
	                                  0,                               // UINT Levels,
	                                  D3DMUSAGE_LOCKABLE,              // DWORD Usage,
	                                  FormatIter,                      // D3DMFORMAT Format,
	                                  TexturePool,                     // D3DPOOL Pool,
	                                  ppTexture)))                     // IDirect3DMobileTexture** ppTexture,
	{
		//
		// Texture creation should have succeeded
		//
		OutputDebugString(_T("CreateTexture failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Additional check to satisfy prefast
	//
	if (NULL == *ppTexture)
	{
		OutputDebugString(_T("Failed due to NULL IDirect3DMobileTexture*."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Did CreateTexture generate the correct number of levels?
	//
	dwLevelCount = (*ppTexture)->GetLevelCount();
	if (D3DQA_NUM_LEVELS_EXPECTED != dwLevelCount)
	{
		OutputDebugString(_T("Unexpected level count."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Pick a level to inspect / fill
	//
	for (uiLevelDescIter = 0; uiLevelDescIter < D3DQA_NUM_LEVELS_EXPECTED; uiLevelDescIter++)
	{
		//
		// Retrieve level description to inspect extents
		//
		if (FAILED((*ppTexture)->GetLevelDesc(uiLevelDescIter,  // UINT Level,
							                &Desc)))        // D3DMSURFACE_DESC *pDesc
		{
			OutputDebugString(_T("GetLevelDesc failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Compute the expected surface extents for this mip-map level
		//
		if (FAILED(GetMipLevelExtents(D3DQA_TOP_LEVEL_MIPMAP_EXTENT_X, // UINT uiTopWidth,
									  D3DQA_TOP_LEVEL_MIPMAP_EXTENT_Y, // UINT uiTopHeight,
									  uiLevelDescIter,                 // UINT uiLevel,
									  &uiExpectedLevelWidth,           // UINT *puiLevelWidth,
                                      &uiExpectedLevelHeight       ))) // UINT *puiLevelHeight
		{
			OutputDebugString(_T("GetMipLevelExtents failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// This test is sensative to the level extents; refuse to execute if
		// some unexpected extent value is encountered
		//
		if (uiExpectedLevelWidth != Desc.Width)
		{
			OutputDebugString(_T("Level width mismatch."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// This test is sensative to the level extents; refuse to execute if
		// some unexpected extent value is encountered
		//
		if (uiExpectedLevelHeight != Desc.Height)
		{
			OutputDebugString(_T("Level height mismatch."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Retrieve a surface interface pointer, for this level of the mip-map
		//
		if (FAILED((*ppTexture)->GetSurfaceLevel(uiLevelDescIter, // UINT Level,
										        &pSurface)))      // IDirect3DMobileSurface **ppSurfaceLevel
		{
			OutputDebugString(_T("GetSurfaceLevel failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Put some unique color data into this level, to distinguish it from the other
		// levels' color data.
		//
		if (FAILED(FillMipSurfaceLevel(pSurface,          // LPDIRECT3DMOBILESURFACE pSurface,
		                               uiLevelDescIter))) // UINT uiLevel
		{
			OutputDebugString(_T("FillMipSurfaceLevel failed."));
			hr = E_FAIL;
			goto cleanup;
		}

		//
		// Now that this mip-map level has been filled; there is no need to retain a
		// reference to the surface
		//
		pSurface->Release();
		pSurface = NULL;
	}
cleanup:


	if (pD3D)
		pD3D->Release();

	if (FAILED(hr))
	{
		if (ppTexture)
		{
			if (*ppTexture)
			{
				(*ppTexture)->Release();
				(*ppTexture) = NULL;
			}
		}
	}

	if (pSurface)
		pSurface->Release();
	
	return hr;
}

// 
// FillMipSurfaceLevel
//
//   This function is used to fill individual surfaces, within a mip-map, with unique data.
//   Callers often require unique data to easily distinguish which level is being used in
//   a particular rendering, for testing purposes.
//
// Arguments:
//  
//   LPDIRECT3DMOBILESURFACE pSurface:  Interface pointer to manipulate
//   UINT uiLevel:  Level of mip-map to fill (allows function to select unique data for this level)
//
// Return Value:
//
//  HRESULT indicates success or failure
//
HRESULT FillMipSurfaceLevel(LPDIRECT3DMOBILESURFACE pSurface, UINT uiLevel)
{
	INT iResourceID;

	//
	// Pointer to resource data
	//
	PBYTE pByte = NULL;

	//
	// Grab resource ID for this mip level
	//
	switch(uiLevel)
	{
	case 0: 
		iResourceID = IDR_D3DMQA_MIP0;
		break;
	case 1: 
		iResourceID = IDR_D3DMQA_MIP1;
		break;
	case 2: 
		iResourceID = IDR_D3DMQA_MIP2;
		break;
	case 3: 
		iResourceID = IDR_D3DMQA_MIP3;
		break;
	case 4: 
		iResourceID = IDR_D3DMQA_MIP4;
		break;
	case 5: 
		iResourceID = IDR_D3DMQA_MIP5;
		break;
	case 6: 
		iResourceID = IDR_D3DMQA_MIP6;
		break;
	case 7: 
		iResourceID = IDR_D3DMQA_MIP7;
		break;
	default:
		return E_FAIL;
	}


	if (FAILED(GetBitmapResourcePointer(iResourceID,     // INT iResourceID
	                                    &pByte)))        // PBYTE *ppByte
	{
		OutputDebugString(_T("GetBitmapResourcePointer failed."));
		return E_FAIL;
	}

	if (FAILED(FillSurface(pSurface,     // IDirect3DMobileSurface* pSurface
	                       pByte)))      // BYTE *pBMP
	{
		OutputDebugString(_T("FillSurface failed."));
		return E_FAIL;
	}

	//
	// Success
	//
	return S_OK;

}


// 
// SetupMipMapGeometry
//
//   Creates geometry that will expose a mip-map level of the specified extent.
//   Due to the nature of mip-map levels, the number of pixels that a particular
//   primitive spans is the determining factor in selecting a mip-map level.
//
// Arguments:
//  
//   LPDIRECT3DMOBILEDEVICE pDevice:  Underlying Direct3D Device
//   D3DMPRESENT_PARAMETERS* pPresentParms:  Parameters used with CreateDevice
//   HWND hWnd:  Window to resize according to needs
//   GEOMETRY_DESC *pDesc:  Structure that identifies generated geometry
//   UINT uiSquareExtent:  Extent of mip-map level to be exposed
//
// Return Value:
//
//  HRESULT indicates success or failure
//
#pragma prefast(disable: 262, "temporarily ignore investigated prefast warning") 
HRESULT SetupMipMapGeometry(LPDIRECT3DMOBILEDEVICE pDevice,
                            D3DMPRESENT_PARAMETERS* pPresentParms,
                            HWND hWnd,
                            GEOMETRY_DESC *pDesc,
                            UINT uiSquareExtent)
{
	//
	// Generate geometry that imitates a wrapping for tu and tv.  Multiplier should be 4 or
	// a multiple of 4.
	//
	// Technical dependency, motivating this value choice:
	//
	//   * The image comparison library likes bitmaps with widths divisible by 4
	//   * The mip-map test needs to use quad sizes as small as 1x1
	//   * Given these two issues, a 4x4 grid of quads works well for the
	//     generated geometry in this test.
	//
	UINT uiRenderExtent;
	UINT uiIterX, uiIterY;


	//
	// Compute total prims and verts, in consideration of XY grid of squares
	//
	// (Each "square" is two triangles; each tri-list type primitive is 3 verts)
	//
	CONST UINT uiPrimCount = (D3DQA_NUM_TEXTURE_REPEAT * D3DQA_NUM_TEXTURE_REPEAT) * 2;
	CONST UINT uiVertCount = (uiPrimCount * 3);

	//
	// Temporary storage for generating vertex data
	//
	TTEX_VERTS pVertices[uiVertCount];
	UINT uiCurrentIndex = 0;

	//
	// Offsets used for each iteration of the geometry generation loop
	//
	float fOffsetX, fOffsetY;

	//
	// All failure conditions specifically set this to an error
	//
	HRESULT hr = S_OK;

	//
	// Generated vertices depend on viewport extents
	//
	D3DMVIEWPORT Viewport;

	//
	// Bad-parameter check
	//
	if ((NULL == pDesc) || (NULL == pDevice))
		return E_FAIL;

	pDesc->uiVertCount = uiVertCount;
	pDesc->uiPrimCount = uiPrimCount;

	*(pDesc->ppVB) = NULL;

	uiRenderExtent = uiSquareExtent * D3DQA_NUM_TEXTURE_REPEAT;

	//
	// Don't set up anything that would exceed minimum allowable extents
	//
	if (uiRenderExtent > D3DQA_REQUIRED_SCREEN_EXTENT)
		uiRenderExtent = D3DQA_REQUIRED_SCREEN_EXTENT;

	//
	// Retrieve current viewport; z settings will be retained, others
	// will be overwritten
	//
	if (FAILED(pDevice->GetViewport(&Viewport)))
	{
		OutputDebugString(_T("GetViewport failed."));
		hr = E_FAIL;
		goto cleanup;
	}

	//
	// Create a "grid" of squares (two tri's each).  This produces output similar to
	// using a wrapping texture addressing mode for both u and v.  However, this must
	// be emulated below, because all texture addressing modes are optional (capabilities)
	//
	for (uiIterX = 0; uiIterX < D3DQA_NUM_TEXTURE_REPEAT; uiIterX++)
	{
		for (uiIterY = 0; uiIterY < D3DQA_NUM_TEXTURE_REPEAT; uiIterY++)
		{
			fOffsetX = (float)(uiSquareExtent * uiIterX);
			fOffsetY = (float)(uiSquareExtent * uiIterY);

			pVertices[uiCurrentIndex].x = fOffsetX + (float)Viewport.X;
			pVertices[uiCurrentIndex].y = fOffsetY + (float)Viewport.Y;
			pVertices[uiCurrentIndex].z = 0.0f;
			pVertices[uiCurrentIndex].rhw = 1.0f;
			pVertices[uiCurrentIndex].tu = 0.0f;
			pVertices[uiCurrentIndex].tv = 0.0f;
			uiCurrentIndex++;

			pVertices[uiCurrentIndex].x = fOffsetX + (float)Viewport.X + (float)uiSquareExtent;
			pVertices[uiCurrentIndex].y = fOffsetY + (float)Viewport.Y;
			pVertices[uiCurrentIndex].z =  0.0f;
			pVertices[uiCurrentIndex].rhw = 1.0f;
			pVertices[uiCurrentIndex].tu = 1.0f;
			pVertices[uiCurrentIndex].tv = 0.0f;
			uiCurrentIndex++;

			pVertices[uiCurrentIndex].x = fOffsetX + (float)Viewport.X;
			pVertices[uiCurrentIndex].y = fOffsetY + (float)Viewport.Y + (float)uiSquareExtent;
			pVertices[uiCurrentIndex].z =   0.0f;
			pVertices[uiCurrentIndex].rhw = 1.0f;
			pVertices[uiCurrentIndex].tu = 0.0f;
			pVertices[uiCurrentIndex].tv = 1.0f;
			uiCurrentIndex++;

			pVertices[uiCurrentIndex].x = fOffsetX + (float)Viewport.X + (float)uiSquareExtent;
			pVertices[uiCurrentIndex].y = fOffsetY + (float)Viewport.Y;
			pVertices[uiCurrentIndex].z =  0.0f;
			pVertices[uiCurrentIndex].rhw = 1.0f;
			pVertices[uiCurrentIndex].tu = 1.0f;
			pVertices[uiCurrentIndex].tv = 0.0f;
			uiCurrentIndex++;

			pVertices[uiCurrentIndex].x = fOffsetX + (float)Viewport.X + (float)uiSquareExtent;
			pVertices[uiCurrentIndex].y = fOffsetY + (float)Viewport.Y + (float)uiSquareExtent;
			pVertices[uiCurrentIndex].z =   0.0f;
			pVertices[uiCurrentIndex].rhw = 1.0f;
			pVertices[uiCurrentIndex].tu = 1.0f;
			pVertices[uiCurrentIndex].tv = 1.0f;
			uiCurrentIndex++;

			pVertices[uiCurrentIndex].x = fOffsetX + (float)Viewport.X;
			pVertices[uiCurrentIndex].y = fOffsetY + (float)Viewport.Y + (float)uiSquareExtent;
			pVertices[uiCurrentIndex].z =   0.0f;
			pVertices[uiCurrentIndex].rhw = 1.0f;
			pVertices[uiCurrentIndex].tu = 0.0f;
			pVertices[uiCurrentIndex].tv = 1.0f;
			uiCurrentIndex++;

		}
	}

	//
	// Create and fill a vertex buffer with the generated geometry
	//
	if (FAILED(CreateFilledVertexBuffer( pDevice,                // LPDIRECT3DMOBILEDEVICE pDevice,
	                                     pDesc->ppVB,            // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                     (BYTE*)pVertices,       // BYTE *pVertices,
	                                     sizeof(TTEX_VERTS),     // UINT uiVertexSize,
	                                     pDesc->uiVertCount,     // UINT uiNumVertices,
	                                     D3DQA_FVF_TTEX_VERTS))) // DWORD dwFVF
	{
		OutputDebugString(_T("CreateVertexBuffer failed."));
		hr = E_FAIL;
		goto cleanup;
	}

cleanup:

	if ((FAILED(hr)) && (NULL != *(pDesc->ppVB)))
		(*(pDesc->ppVB))->Release();

	return hr;	
}
#pragma prefast(pop)

