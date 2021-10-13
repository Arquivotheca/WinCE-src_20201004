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
#include "AlphaTestCases.h"
#include "QAD3DMX.h"
#include <SurfaceTools.h>
#include <BufferTools.h>
#include "utils.h"
#include "DebugOutput.h"

namespace AlphaTestNamespace
{
    HRESULT CreateAndPrepareTexture(
        LPDIRECT3DMOBILEDEVICE   pDevice, 
        D3DMFORMAT               Format,
        D3DMCOLOR                Color,
        DWORD                    dwTableIndex, 
        LPDIRECT3DMOBILETEXTURE *ppTexture)
    {
        HRESULT hr;

        //
        // The device caps, to determine in which pool to put the texture.
        //
        D3DMCAPS Caps;

        //
        // The D3DM object, used when determining if the desired format is supported.
        //
        LPDIRECT3DMOBILE pD3D = NULL;
        
        //
        // The current mode, used when determining if the desired format is supported.
        //
        D3DMDISPLAYMODE Mode;

        //
        // The proper resource pool for the texture.
        //
        D3DMPOOL TexturePool;

        //
        // The surface will be used to fill the texture.
        //
        LPDIRECT3DMOBILESURFACE pTextureSurface = NULL;

        //
        // How should the texture be used? This will depend on caps.
        //
        DWORD Usage = 0;

//        //
//        // Does this test need a texture with alpha?
//        //
//        bool NeedsAlpha = 
//            (AlphaTestCases[dwTableIndex].dwAlphaArg1 & D3DMTA_SELECTMASK) == D3DMTA_TEXTURE
//            || (AlphaTestCases[dwTableIndex].dwAlphaArg2 & D3DMTA_SELECTMASK) == D3DMTA_TEXTURE;

        //
        // Get the current D3DM object.
        //
        if (FAILED(hr = pDevice->GetDirect3D(&pD3D)))
        {
            DebugOut(DO_ERROR, _T("GetDirect3D failed. hr = 0x%08x"), hr);
            goto cleanup;
        }
        
    	//
    	// Get device capabilities to check for mip-mapping support
    	//
    	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    	{
    		DebugOut(DO_ERROR, _T("GetDeviceCaps failed. hr = 0x%08x"), hr);
    		goto cleanup;
    	}

    	if (FAILED(hr = pDevice->GetDisplayMode(&Mode)))
    	{
    	    DebugOut(DO_ERROR, _T("GetDisplayMode failed. hr = 0x%08x"), hr);
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
    		DebugOut(DO_ERROR, _T("D3DMCAPS.SurfaceCaps:  No valid texture pool."));
    		hr = E_FAIL;
    		goto cleanup;
    	}

    	if (D3DMSURFCAPS_LOCKTEXTURE & Caps.SurfaceCaps)
    	{
    	    Usage = D3DMUSAGE_LOCKABLE;
    	}


//        if (NeedsAlpha)
//        {
//            if (FAILED(hr = GetBestAlphaFormat(pDevice, D3DMRTYPE_TEXTURE, &Format)))
//            {
//                DebugOut(DO_ERROR, L"Could not find 8 bit Alpha format with which to create texture.");
//                hr = S_FALSE;
//                goto cleanup;
//            }
//        }
//        else
//        {
//            if (FAILED(hr = GetBestFormat(pDevice, D3DMRTYPE_TEXTURE, &Format)))
//            {
//                DebugOut(DO_ERROR, L"Could not find best format with which to create texture.");
//                goto cleanup;
//            }
//        }
//
        if (FAILED(pD3D->CheckDeviceFormat(Caps.AdapterOrdinal, // UINT Adapter
                                           Caps.DeviceType,          // D3DMDEVTYPE DeviceType
                                           Mode.Format,        // D3DMFORMAT AdapterFormat
                                           Usage,                  // ULONG Usage
                                           D3DMRTYPE_TEXTURE,  // D3DMRESOURCETYPE RType
                                           Format)))   // D3DMFORMAT CheckFormat
        {
            DebugOut(L"Format 0x%08x unsupported.", Format);
            hr = S_FALSE;
            goto cleanup;
        }

        if (FAILED(hr = pDevice->CreateTexture(D3DMQA_TWTEXWIDTH, D3DMQA_TWTEXHEIGHT, 1, Usage, Format, TexturePool, ppTexture)))
        {
            DebugOut(DO_ERROR, L"Could not create texture. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = (*ppTexture)->GetSurfaceLevel(0, &pTextureSurface)))
        {
            DebugOut(DO_ERROR, L"Could not get surface from texture for filling. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = FillAlphaSurface(pTextureSurface, AlphaTestCases[dwTableIndex].TexFill, Color)))
        {
            DebugOut(DO_ERROR, L"Could not fill texture surface with gradient. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = pDevice->SetTexture(0, (*ppTexture))))
        {
            DebugOut(DO_ERROR, L"Could not add texture to stage 0 of device. hr = 0x%08x", hr);
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
    		DebugOut(DO_ERROR, _T("GetClientRect failed. Error: 0x%08x"), hr);
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

    	if (FAILED(hr = CreateFilledVertexBuffer(pDevice,                           // LPDIRECT3DMOBILEDEVICE pDevice,
    	                                         ppVertexBuffer,                              // LPDIRECT3DMOBILEVERTEXBUFFER *ppVB,
    	                                         AlphaTestCases[dwTableIndex].pVertexData, // BYTE *pVertices,
    	                                         AlphaTestCases[dwTableIndex].uiFVFSize,   // UINT uiVertexSize,
    	                                         AlphaTestCases[dwTableIndex].uiNumVerts,  // UINT uiNumVertices,
    	                                         AlphaTestCases[dwTableIndex].dwFVF)))     // DWORD dwFVF
    	{
    		DebugOut(DO_ERROR, _T("CreateFilledVertexBuffer failed. hr = 0x%08x"), hr);
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
    		DebugOut(DO_ERROR, _T("SetTextureStageState failed."));
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
            DebugOut(DO_ERROR, L"Current AlphaTest compare function not supported, skipping");
            return S_FALSE;
        }

    	//
    	// Set up the shade mode
    	//
    	if (FAILED(hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, D3DMSHADE_FLAT)))
    	{
    		DebugOut(DO_ERROR, _T("SetRenderState(D3DMRS_SHADEMODE) failed. hr = 0x%08x"), hr);
        }

        //
        // Set up the alpha testing
        //
        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHATESTENABLE, TRUE)))
        {
            DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ALPHATESTENABLE) failed. hr = 0x%08x", hr);
        }

        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHAFUNC, AlphaTestCases[dwTableIndex].dwCmpFunc)))
        {
            DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ALPHAFUNC) failed. hr = 0x%08x", hr);
        }

        if (FAILED(hr = pDevice->SetRenderState(D3DMRS_ALPHAREF, AlphaTestCases[dwTableIndex].dwRefAlpha)))
        {
            DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_ALPHAREF) failed. hr = 0x%08x", hr);
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
        
        DebugOut(DO_ERROR, L"Unhandled primitive type (0x%08x)", dwCurrentPrimType);
        return 0;
    }
};
