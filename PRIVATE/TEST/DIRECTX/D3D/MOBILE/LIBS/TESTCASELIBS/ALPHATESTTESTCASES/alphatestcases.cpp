//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "AlphaTestCases.h"
#include "QAD3DMX.h"
#include <SurfaceTools.h>
#include <BufferTools.h>
#include "utils.h"

namespace AlphaTestNamespace
{
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
        // Does this test need a texture with alpha?
        //
        bool NeedsAlpha = 
            (AlphaTestCases[dwTableIndex].dwAlphaArg1 & D3DMTA_SELECTMASK) == D3DMTA_TEXTURE
            || (AlphaTestCases[dwTableIndex].dwAlphaArg2 & D3DMTA_SELECTMASK) == D3DMTA_TEXTURE;
        
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


        if (NeedsAlpha)
        {
            if (FAILED(hr = GetBestAlphaFormat(pDevice, D3DMRTYPE_TEXTURE, &Format)))
            {
                OutputDebugString(L"Could not find 8 bit Alpha format with which to create texture.\n");
                hr = S_FALSE;
                goto cleanup;
            }
        }
        else
        {
            if (FAILED(hr = GetBestFormat(pDevice, D3DMRTYPE_TEXTURE, &Format)))
            {
                OutputDebugString(L"Could not find best format with which to create texture.\n");
                goto cleanup;
            }
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

        if (FAILED(hr = FillAlphaSurface(pTextureSurface, AlphaTestCases[dwTableIndex].TexFill)))
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
    	if (D3DMFVF_XYZRHW_FLOAT & AlphaTestCases[dwTableIndex].dwFVF)
    	{
    		for (uiShift = 0; uiShift < AlphaTestCases[dwTableIndex].uiNumVerts; uiShift++)
    		{
    			pPosX = (PFLOAT)((PBYTE)(AlphaTestCases[dwTableIndex].pVertexData) + uiShift * AlphaTestCases[dwTableIndex].uiFVFSize);
    			pPosY = &(pPosX[1]);

    			(*pPosX) *= (float)(uiSX - 1);
    			(*pPosY) *= (float)(uiSY - 1);

    		}
    	}

    	if (FAILED(CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
    	                                    ppVertexBuffer,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
    	                                    AlphaTestCases[dwTableIndex].pVertexData, // BYTE *pVertices,
    	                                    AlphaTestCases[dwTableIndex].uiFVFSize,   // UINT uiVertexSize,
    	                                    AlphaTestCases[dwTableIndex].uiNumVerts,  // UINT uiNumVertices,
    	                                    AlphaTestCases[dwTableIndex].dwFVF)))     // DWORD dwFVF
    	{
    		OutputDebugString(_T("CreateActiveBuffer failed.\n"));
            hr = S_FALSE;
    	}
    	
    	//
    	// "Unscale" from window size
    	//
    	if (D3DMFVF_XYZRHW_FLOAT & AlphaTestCases[dwTableIndex].dwFVF)
    	{
    		for (uiShift = 0; uiShift < AlphaTestCases[dwTableIndex].uiNumVerts; uiShift++)
    		{
    			pPosX = (PFLOAT)((PBYTE)(AlphaTestCases[dwTableIndex].pVertexData) + uiShift * AlphaTestCases[dwTableIndex].uiFVFSize);
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
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLOROP,   D3DMTOP_SELECTARG1                       ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG1, D3DMTA_TEXTURE                           ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_COLORARG2, D3DMTA_CURRENT                           ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAOP,   AlphaTestCases[dwTableIndex].dwAlphaOp   ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG1, AlphaTestCases[dwTableIndex].dwAlphaArg1 ))) ||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ALPHAARG2, AlphaTestCases[dwTableIndex].dwAlphaArg2 ))) /*||
    		(FAILED(pDevice->SetTextureStageState( 0, D3DMTSS_ADDRESSU,  D3DMTADDRESS_CLAMP )))*/
    		)
    	{
    		OutputDebugString(_T("SetTextureStageState failed."));
    		return E_FAIL;
    	}

    	return S_OK;

    }

    HRESULT SetupRenderStates(
        LPDIRECT3DMOBILEDEVICE pDevice, 
        DWORD                  dwTableIndex)
    {
        HRESULT hr;
        D3DMCAPS Caps;
        DWORD dwCmpCapNeeded = 1 << (AlphaTestCases[dwTableIndex].dwCmpFunc - 1);

        pDevice->GetDeviceCaps(&Caps);

        if (!(Caps.AlphaCmpCaps & dwCmpCapNeeded))
        {
            OutputDebugString(L"Current AlphaTest compare function not supported, skipping");
            return S_FALSE;
        }

    	//
    	// Set up the shade mode
    	//
    	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
    	{
    		OutputDebugString(_T("SetRenderState(D3DMRS_SHADEMODE) failed.\n"));
        }

        //
        // Set up the alpha testing
        //
        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHATESTENABLE, TRUE)))
        {
            OutputDebugString(L"SetRenderState(D3DMRS_ALPHATESTENABLE) failed.\n");
        }

        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHAFUNC, AlphaTestCases[dwTableIndex].dwCmpFunc)))
        {
            OutputDebugString(L"SetRenderState(D3DMRS_ALPHAFUNC) failed.\n");
        }

        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHAREF, AlphaTestCases[dwTableIndex].dwRefAlpha)))
        {
            OutputDebugString(L"SetRenderState(D3DMRS_ALPHAREF) failed.\n");
        }
        return hr;
    }

    UINT GetPrimitiveCount(DWORD dwTableIndex)
    {
        DWORD dwCurrentPrimType = D3DMQA_PRIMTYPE;
        if (dwCurrentPrimType == D3DMPT_TRIANGLESTRIP || dwCurrentPrimType == D3DMPT_TRIANGLEFAN)
        {
            return AlphaTestCases[dwTableIndex].uiNumVerts - 2;
        }
        
        if (dwCurrentPrimType == D3DMPT_TRIANGLELIST)
        {
            return AlphaTestCases[dwTableIndex].uiNumVerts / 3;
        }
        
        OutputDebugString(L"Unhandled primitive type.\n");
        return 0;
    }
};
