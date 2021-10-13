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
#include "ColorFillCases.h"
#include "QAD3DMX.h"
#include "utils.h"
#include <SurfaceTools.h>
#include "DebugOutput.h"

HRESULT AcquireFillSurface(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPUNKNOWN *ppParentObject,
    LPDIRECT3DMOBILESURFACE *ppSurface,
    DWORD dwWidth,
    DWORD dwHeight,
    DWORD dwTableIndex)
{
    HRESULT hr;

    //
    // The capabilities of the device. Use this to determine where
    // textures can be created
    //
    D3DMCAPS Caps;
    
    //
    // The resource type of the surface we are creating
    // (either SURFACE or TEXTURE), and where it is being created.
    //
	D3DMRESOURCETYPE ResourceType;
	D3DMPOOL ResourcePool;

	//
	// The format that will be used to create the resource
	//
	D3DMFORMAT Format;

	//
	// The texture will be used when creating a texture surface.
	//
    LPDIRECT3DMOBILETEXTURE pTexture = NULL;

    if (FAILED(hr = pDevice->GetDeviceCaps(&Caps)))
    {
        DebugOut(DO_ERROR, L"Cannot get device caps. hr = 0x%08x", hr);
        goto cleanup;
    }

    if (D3DQA_BACKBUFFER == ColorFillCases[dwTableIndex].Creator)
    {
        // If the primary is the requested surface, ignore the width and height.

        // The primary has no parent object that will need to be released.
        *ppParentObject = NULL;
        hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, ppSurface);
    }
    else if (D3DQA_CREATE_IMAGE_SURFACE == ColorFillCases[dwTableIndex].Creator)
    {
        *ppParentObject = NULL;
        ResourceType = D3DMRTYPE_SURFACE;
    
        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            DebugOut(DO_ERROR, L"No valid surface formats found for image surface.");
            goto cleanup;
        }
        
        hr = pDevice->CreateImageSurface(dwWidth, dwHeight, Format, ppSurface);
    }
    else if (D3DQA_CREATE_TEXTURE == ColorFillCases[dwTableIndex].Creator)
    {
        ResourceType = D3DMRTYPE_TEXTURE;
        ResourcePool = ColorFillCases[dwTableIndex].ResourcePool;

        //
        // The reference driver doesn't support videomem textures, but we still
        // want to be able to generate reference images for comparison if the
        // production driver supports video memory. Therefore, use systemmem
        // textures to generate the reference image.
        //
        if (IsRefDriver(pDevice))
        {
            ResourcePool = D3DMPOOL_SYSTEMMEM;
        }

        //
        // Round the width and height to the nearest power of two.
        // Do this with all drivers so that the reference driver's results
        // match all other drivers.
        //
        if (!IsPowerOfTwo(dwWidth))
            dwWidth = NearestPowerOfTwo(dwWidth);
        if (!IsPowerOfTwo(dwHeight))
            dwHeight = NearestPowerOfTwo(dwHeight);

        //
        // Make sure the texture is square.
        //
        if (dwWidth > dwHeight)
            dwWidth = dwHeight;
        if (dwHeight > dwWidth)
            dwHeight = dwWidth;

        //
        // Clamp the texture size to meet requirements.
        //
        if (dwWidth > D3DMQA_MAXTEXTURESIZE || dwHeight > D3DMQA_MAXTEXTURESIZE)
        {
            dwWidth = dwHeight = D3DMQA_MAXTEXTURESIZE;
        }

        //
        // Make sure the texture is small enough for the device's maximum texture size.
        // If the texture is not small enough, skip and tell the user to use a smaller
        // resolution.
        //
        if (dwWidth > Caps.MaxTextureWidth || dwHeight > Caps.MaxTextureHeight)
        {
            DebugOut(L"Skipping because device does not support current texture size. Please choose smaller resolution when running this test.");
            DebugOut(L"To choose smaller resolution, use one of the /e* command line parameters.");
            hr = S_FALSE;
            goto cleanup;
        }

        //
        // If the driver doesn't support textures from the specified pool,
        // return S_FALSE to indicate to the test that it should log a skip.
        //
        if (D3DMPOOL_SYSTEMMEM == ResourcePool &&
            !(Caps.SurfaceCaps & D3DMSURFCAPS_SYSTEXTURE))
        {
            DebugOut(DO_ERROR, L"Device does not support system memory textures.");
            hr = S_FALSE;
            goto cleanup;
        }
        
        if (D3DMPOOL_VIDEOMEM == ResourcePool &&
            !(Caps.SurfaceCaps & D3DMSURFCAPS_VIDTEXTURE))
        {
            DebugOut(DO_ERROR, L"Device does not support video memory textures.");
            hr = S_FALSE;
            goto cleanup;
        }

        //
        // Get the format in which to create the texture.
        //
        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            DebugOut(DO_ERROR, L"No valid surface formats found for texture.");
            goto cleanup;
        }

        //
        // Create the texture.
        //
        if (FAILED(hr = pDevice->CreateTexture(dwWidth, dwHeight, 1, D3DMUSAGE_RENDERTARGET, Format, ResourcePool, &pTexture)))
        {
            DebugOut(DO_ERROR, L"Could not create texture for destination. hr = 0x%08x", hr);
            goto cleanup;
        }

        //
        // Get the unknown pointer for the texture. This will be used to release
        // the texture object when its surface is released in the main test.
        //
        if (FAILED(hr = pTexture->QueryInterface(IID_IUnknown, (void**) ppParentObject)))
        {
            DebugOut(DO_ERROR, L"Could not get unknown interface from texture. hr = 0x%08x", hr);
            goto cleanup;
        }

        //
        // Get the surface pointer.
        //
        hr = pTexture->GetSurfaceLevel(0, ppSurface);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not get surface from texture. hr = 0x%08x", hr);
            (*ppParentObject)->Release();
            *ppParentObject = NULL;
            goto cleanup;
        }
    }

    if (*ppSurface)
    {
        if (FAILED(hr = pDevice->ColorFill(*ppSurface, NULL, D3DMCOLOR_XRGB(0, 0, 0))))
        {
            DebugOut(DO_ERROR, L"Could not colorfill destination for preparation. hr = 0x%08x", hr);
            goto cleanup;
        }
    }
cleanup:
    if (pTexture)
        pTexture->Release();
    return hr;
}

HRESULT GetColorFillRects(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex,
    UINT uiIterations,
    LPRECT * ppRect,
    D3DMCOLOR * pColor)
{
    const UINT c_uiBigRectCount = 4;
    const UINT c_uiSmallRectCount = 16;
    
    HRESULT hr = S_OK;
    D3DMSURFACE_DESC d3dsd;
    UINT uiRectCount = 0;
    LONG WidthSurface;
    LONG HeightSurface;
    LONG X;
    LONG Y;
    LONG Width;
    LONG Height;
    UINT uiRed, uiBlue, uiGreen;
    
    //
    // Get the surface descriptions so we can determine what sizes to use.
    //
    if (FAILED(hr = pSurface->GetDesc(&d3dsd)))
    {
        DebugOut(DO_ERROR, L"Could not get surface description of source. hr = 0x%08x", hr);
        goto cleanup;
    }

    WidthSurface = d3dsd.Width;
    HeightSurface = d3dsd.Height;

    if (D3DQA_CFFULLSURFACE == ColorFillCases[dwTableIndex].ColorFillType)
    {
        if (uiIterations >= 1)
        {
            hr = S_FALSE;
            goto cleanup;
        }
        *ppRect = NULL;
        *pColor = D3DMCOLOR_XRGB(0xff, 0xff, 0xff);
        goto cleanup;
    }
    else if (D3DQA_CFBIGRECTS == ColorFillCases[dwTableIndex].ColorFillType)
    {
        uiRectCount = c_uiBigRectCount;
    }
    else if (D3DQA_CFSMALLRECTS == ColorFillCases[dwTableIndex].ColorFillType)
    {
        uiRectCount = c_uiSmallRectCount;
    }

    if (uiIterations >= uiRectCount * uiRectCount)
    {
        hr = S_FALSE;
        goto cleanup;
    }
    Width = WidthSurface / uiRectCount;
    Height = HeightSurface / uiRectCount;
    X = Width * (uiIterations % uiRectCount);
    Y = Height * (uiIterations / uiRectCount);
    (*ppRect)->left = X;
    (*ppRect)->right = X + Width;
    (*ppRect)->top = Y;
    (*ppRect)->bottom = Y + Height;

    uiRed = 0xff * uiIterations / (uiRectCount * uiRectCount);
    uiGreen = 0xaa * uiIterations / (uiRectCount * uiRectCount);
    uiBlue = 0x55 * uiIterations / (uiRectCount * uiRectCount);
    *pColor = D3DMCOLOR_XRGB(uiRed, uiGreen, uiBlue);
cleanup:
    return hr;
}

