//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TexWrapCases.h"
#include "QAD3DMX.h"
#include "utils.h"
#include <SurfaceTools.h>
#include <BufferTools.h>

HRESULT CreateAndPrepareTexture(
    LPDIRECT3DMOBILEDEVICE   pDevice, 
    DWORD                    dwTableIndex, 
    LPDIRECT3DMOBILETEXTURE *ppTexture)
{
    HRESULT hr;

    //
    // The device caps, to determine in which pool to put the texture.
    //
    D3DMCAPS Caps;
    
    //
    // What is the proper format for this texture?
    //
    D3DMFORMAT Format;

    //
    // The proper resource pool for the texture.
    //
    D3DMPOOL TexturePool;

    //
    // The surface will be used to fill the texture.
    //
    LPDIRECT3DMOBILESURFACE pTextureSurface = NULL;
    
	//
	// Get device capabilities to check for mip-mapping support
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
	{
		OutputDebugString(_T("GetDeviceCaps failed."));
		goto cleanup;
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


    if (FAILED(hr = GetBestFormat(pDevice, D3DMRTYPE_TEXTURE, &Format)))
    {
        OutputDebugString(L"Could not find best format with which to create texture.\n");
        goto cleanup;
    }

    if (FAILED(hr = pDevice->CreateTexture(D3DMQA_TWTEXWIDTH, D3DMQA_TWTEXHEIGHT, 1, D3DMUSAGE_RENDERTARGET, Format, TexturePool, ppTexture)))
    {
        OutputDebugString(L"Could not create texture.\n");
        goto cleanup;
    }

    if (FAILED(hr = (*ppTexture)->GetSurfaceLevel(0, &pTextureSurface)))
    {
        OutputDebugString(L"Could not get surface from texture for filling.\n");
        goto cleanup;
    }

    if (FAILED(hr = SurfaceGradient(pTextureSurface)))
    {
        OutputDebugString(L"Could not fill texture surface with gradient.\n");
        goto cleanup;
    }

    if (FAILED(hr = pDevice->SetTexture(0, (*ppTexture))))
    {
        OutputDebugString(L"Could not add texture to stage 0 of device");
        goto cleanup;
    }
    
cleanup:
    if (pTextureSurface)
        pTextureSurface->Release();
    return hr;
}

HRESULT CreateAndPrepareVertexBuffer(
    LPDIRECT3DMOBILEDEVICE        pDevice, 
    HWND                          hWnd,
    DWORD                         dwTableIndex, 
    LPDIRECT3DMOBILEVERTEXBUFFER *ppVertexBuffer, 
    UINT                         *pVertexBufferStride)
{
    HRESULT hr = S_OK;
    UINT uiShift;
	FLOAT *pPosX, *pPosY;
	UINT uiSX;
	UINT uiSY;
	//
	// Window client extents
	//
	RECT ClientExtents;
	//
	// Retrieve device capabilities
	//
	if (0 == GetClientRect( hWnd,            // HWND hWnd, 
	                       &ClientExtents)) // LPRECT lpRect 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		OutputDebugString(_T("GetClientRect failed.\n"));
		goto cleanup;
	}
	
	//
	// Scale factors
	//
	uiSX = ClientExtents.right;
	uiSY = ClientExtents.bottom;

	//
	// Scale to window size
	//
	if (D3DMFVF_XYZRHW_FLOAT & TexWrapCases[dwTableIndex].dwFVF)
	{
		for (uiShift = 0; uiShift < TexWrapCases[dwTableIndex].uiNumVerts; uiShift++)
		{
			pPosX = (PFLOAT)((PBYTE)(TexWrapCases[dwTableIndex].pVertexData) + uiShift * TexWrapCases[dwTableIndex].uiFVFSize);
			pPosY = &(pPosX[1]);

			(*pPosX) *= (float)(uiSX - 1);
			(*pPosY) *= (float)(uiSY - 1);

		}
	}

	if (FAILED(CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
	                                    ppVertexBuffer,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
	                                    TexWrapCases[dwTableIndex].pVertexData, // BYTE *pVertices,
	                                    TexWrapCases[dwTableIndex].uiFVFSize,   // UINT uiVertexSize,
	                                    TexWrapCases[dwTableIndex].uiNumVerts,  // UINT uiNumVertices,
	                                    TexWrapCases[dwTableIndex].dwFVF)))     // DWORD dwFVF
	{
		OutputDebugString(_T("CreateActiveBuffer failed.\n"));
        hr = S_FALSE;
	}
	
	//
	// "Unscale" from window size
	//
	if (D3DMFVF_XYZRHW_FLOAT & TexWrapCases[dwTableIndex].dwFVF)
	{
		for (uiShift = 0; uiShift < TexWrapCases[dwTableIndex].uiNumVerts; uiShift++)
		{
			pPosX = (PFLOAT)((PBYTE)(TexWrapCases[dwTableIndex].pVertexData) + uiShift * TexWrapCases[dwTableIndex].uiFVFSize);
			pPosY = &(pPosX[1]);

			(*pPosX) /= (float)(uiSX - 1);
			(*pPosY) /= (float)(uiSY - 1);

		}
	}
cleanup:
    return hr;
}

HRESULT SetupTextureStages(
    LPDIRECT3DMOBILEDEVICE pDevice, 
    DWORD                  dwTableIndex)
{
	TEX_STAGE_SETTINGS FirstTexture  = { D3DMTOP_SELECTARG1, D3DMTOP_SELECTARG1,   D3DMTA_TEXTURE,   D3DMTA_CURRENT,   D3DMTA_TEXTURE,   D3DMTA_CURRENT};

	if (
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP,   D3DMTOP_SELECTARG1 ))) ||
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_TEXTURE     ))) ||
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT     ))) ||
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP,   D3DMTOP_SELECTARG1 ))) ||
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, D3DMTA_TEXTURE     ))) ||
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, D3DMTA_CURRENT     ))) /*||
		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ADDRESSU,  D3DMTADDRESS_CLAMP )))*/
		)
	{
		OutputDebugString(_T("SetTextureStageState failed."));
		return E_FAIL;
	}

	return S_OK;

}

HRESULT SetupTextureTransformFlag(
    LPDIRECT3DMOBILEDEVICE pDevice, 
    DWORD                  dwTableIndex)
{
    HRESULT hr = S_OK;
	//
	// If projected texture coordinates are desired for last stage; set them up...
	//
	// The texture coordinates are all divided by the last element before being passed
	// to the rasterizer. For example, if D3DMTTFF_PROJECTED is specified with the
	// D3DMTTFF_COUNT3 flag, the first and second texture coordinates is divided by
	// the third coordinate before being passed to the rasterizer. 
	//
	if (TexWrapCases[dwTableIndex].bProjected)
	{
		if (FAILED(hr = pDevice->SetTextureStageState( TexWrapCases[dwTableIndex].uiTTFStage, D3DMTSS_TEXTURETRANSFORMFLAGS, TexWrapCases[dwTableIndex].dwTTF)))
		{
			OutputDebugString(_T("SetTestTextureStates failed.\n"));
		}
	}
    return hr;
}
HRESULT SetupRenderState(
    LPDIRECT3DMOBILEDEVICE pDevice, 
    DWORD                  dwTableIndex)
{
    HRESULT hr;
    HRESULT hrResult = S_OK;

    ////////////////////////////////////////
    //
    // Set up general states
    //

    //
    // Set up the culling mode
    //
    if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CULLMODE, D3DMCULL_NONE)))
    {
        OutputDebugString(_T("SetRenderState(D3DMRS_CULLMODE, D3DMCULL_CCW) failed.\n"));
        hrResult = hr;
    }

    //
    // Set up clipping
    //
    if (FAILED(hr = pDevice->SetRenderState(D3DMRS_CLIPPING, TRUE)))
    {
        OutputDebugString(_T("SetRenderState(D3DMRS_CLIPPING, TRUE) failed.\n"));
        hrResult = hr;
    }

	//
	// Set up the shade mode
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE) failed.\n"));
		hrResult = hr;
    }
    
    ////////////////////////////////////////
    //
    // Set up test case specific states
    //
    
	//
	// Set up the WRAP mode
	//
	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_WRAP0, TexWrapCases[dwTableIndex].dwWrapCoord)))
	{
		OutputDebugString(_T("SetRenderState(D3DMRS_WRAP0) failed.\n"));
		hrResult = hr;
    }
   
    return hrResult;
}
