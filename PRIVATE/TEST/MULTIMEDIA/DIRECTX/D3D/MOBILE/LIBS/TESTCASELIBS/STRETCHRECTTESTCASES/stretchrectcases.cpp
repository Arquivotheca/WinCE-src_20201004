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
#include <windows.h>
#include <d3dm.h>
#include <SurfaceTools.h>
#include "QAD3DMX.h"
#include "StretchRectCases.h"
#include "utils.h"
#include "DebugOutput.h"

HRESULT AcquireDestSurface(
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
        DebugOut(DO_ERROR, L"Cannot get device caps. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (D3DQA_BACKBUFFER == StretchRectCases[dwTableIndex].CreatorDest)
    {
        // If the primary is the requested surface, ignore the width and height.

        // The primary has no parent object that will need to be released.
        *ppParentObject = NULL;
        hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, ppSurface);
    }
    else if (D3DQA_CREATE_IMAGE_SURFACE == StretchRectCases[dwTableIndex].CreatorDest)
    {
        *ppParentObject = NULL;
        ResourceType = D3DMRTYPE_SURFACE;
    
        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            DebugOut(DO_ERROR, L"No valid surface formats found for image surface. (hr = 0x%08x)", hr);
            goto cleanup;
        }
        
        hr = pDevice->CreateImageSurface(dwWidth, dwHeight, Format, ppSurface);
    }
    else if (D3DQA_CREATE_TEXTURE == StretchRectCases[dwTableIndex].CreatorDest)
    {
        ResourceType = D3DMRTYPE_TEXTURE;
        ResourcePool = StretchRectCases[dwTableIndex].ResourcePoolDest;

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
            DebugOut(DO_ERROR, L"Could not create texture for destination. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        //
        // Get the unknown pointer for the texture. This will be used to release
        // the texture object when its surface is released in the main test.
        //
        if (FAILED(hr = pTexture->QueryInterface(IID_IUnknown, (void**) ppParentObject)))
        {
            DebugOut(DO_ERROR, L"Could not get unknown interface from texture. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        //
        // Get the surface pointer.
        //
        hr = pTexture->GetSurfaceLevel(0, ppSurface);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not get surface from texture. (hr = 0x%08x)", hr);
            (*ppParentObject)->Release();
            *ppParentObject = NULL;
            goto cleanup;
        }
    }
cleanup:
    if (pTexture)
        pTexture->Release();
    return hr;
}

HRESULT AcquireSourceSurface(
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
        DebugOut(DO_ERROR, L"Cannot get device caps. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (D3DQA_BACKBUFFER == StretchRectCases[dwTableIndex].CreatorSource)
    {
        // If the primary is the requested surface, ignore the width and height.

        // The primary has no parent object that will need to be released.
        *ppParentObject = NULL;
        hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, ppSurface);
    }
    else if (D3DQA_CREATE_IMAGE_SURFACE == StretchRectCases[dwTableIndex].CreatorSource)
    {
        *ppParentObject = NULL;
        ResourceType = D3DMRTYPE_SURFACE;

        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            DebugOut(DO_ERROR, L"No valid surface formats found. (hr = 0x%08x)", hr);
            goto cleanup;
        }
        
        hr = pDevice->CreateImageSurface(dwWidth, dwHeight, Format, ppSurface);
    }
    else if (D3DQA_CREATE_TEXTURE == StretchRectCases[dwTableIndex].CreatorSource)
    {
        ResourceType = D3DMRTYPE_TEXTURE;
        ResourcePool = StretchRectCases[dwTableIndex].ResourcePoolSource;

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
            DebugOut(DO_ERROR, L"No valid surface formats found for texture. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        //
        // Create the texture.
        //
        if (FAILED(hr = pDevice->CreateTexture(dwWidth, dwHeight, 1, 0, Format, ResourcePool, &pTexture)))
        {
            DebugOut(DO_ERROR, L"Could not create texture for destination. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        //
        // Get the unknown pointer for the texture. This will be used to release
        // the texture object when its surface is released in the main test.
        //
        if (FAILED(hr = pTexture->QueryInterface(IID_IUnknown, (void**) ppParentObject)))
        {
            DebugOut(DO_ERROR, L"Could not get unknown interface from texture. (hr = 0x%08x)", hr);
            goto cleanup;
        }

        //
        // Get the surface pointer.
        //
        hr = pTexture->GetSurfaceLevel(0, ppSurface);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"Could not get surface from texture. (hr = 0x%08x)", hr);
            (*ppParentObject)->Release();
            *ppParentObject = NULL;
            goto cleanup;
        }
    }

cleanup:
    if (pTexture)
        pTexture->Release();
    return hr;
}

HRESULT SetupSourceSurface(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex)
{
    HRESULT hr;
    if (FAILED(hr = SurfaceGradient(pSurface, StretchRectCases[dwTableIndex].pfnGetColors)))
    {
        DebugOut(DO_ERROR, L"Could not fill source surface with gradient. (hr = 0x%08x)", hr);
    }
    return hr;
}   

HRESULT GetStretchRects(
    LPDIRECT3DMOBILESURFACE pSurfaceSource,
    LPDIRECT3DMOBILESURFACE pSurfaceDest,
    DWORD dwIteration,
    DWORD dwTableIndex,
    LPRECT pRectSource,
    LPRECT pRectDest)
{
    HRESULT hr = S_OK;
    DWORD dwTotalIterations = StretchRectCases[dwTableIndex].Iterations;
    D3DMSURFACE_DESC d3dsdSource;
    D3DMSURFACE_DESC d3dsdDest;
    LONG WidthSource;
    LONG HeightSource;
    LONG WidthDest;
    LONG HeightDest;
    LONG WidthDelta;
    LONG HeightDelta;
    
    //
    // S_FALSE tells the test that there are no more stretch rects in this test
    // case.
    //
    if (dwTotalIterations == dwIteration)
    {
        hr = S_FALSE;
        goto cleanup;
    }

    //
    // Get the surface descriptions so we can determine what sizes to use.
    //
    if (FAILED(hr = pSurfaceSource->GetDesc(&d3dsdSource)))
    {
        DebugOut(DO_ERROR, L"Could not get surface description of source. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (FAILED(hr = pSurfaceDest->GetDesc(&d3dsdDest)))
    {
        DebugOut(DO_ERROR, L"Could not get surface description of destination. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    // 
    // Fill in our rects with the full surface sizes.
    //
    pRectSource->left = pRectSource->top = 0;
    pRectDest->left = pRectDest->top = 0;
    
    pRectSource->right = d3dsdSource.Width;
    pRectSource->bottom = d3dsdSource.Height;

    pRectDest->right = d3dsdDest.Width;
    pRectDest->bottom = d3dsdDest.Height;

    WidthSource = pRectSource->right - pRectSource->left;
    HeightSource = pRectSource->bottom - pRectSource->top;
    
    WidthDest = pRectDest->right - pRectDest->left;
    HeightDest = pRectDest->bottom - pRectDest->top;

    if (D3DQA_STRETCH == StretchRectCases[dwTableIndex].StretchType)
    {
        // Stretching

        // Use a ninth of the source.
        WidthSource /= 3;
        HeightSource /= 3;

        InflateRect(pRectSource, -WidthSource, -HeightSource);

        // Recalculate to eliminate rounding problems.
        WidthSource = pRectSource->right - pRectSource->left;
        HeightSource = pRectSource->bottom - pRectSource->top;

        // Calculate the amount of room to stretch on each side
        WidthDelta = (WidthDest - WidthSource) / 2;
        HeightDelta = (HeightDest - HeightSource) / 2;

        // Calculate the Dest rect based on the delta and the current iteration
        WidthDelta = WidthDelta * dwIteration / dwTotalIterations;
        HeightDelta = HeightDelta * dwIteration / dwTotalIterations;

        InflateRect(pRectDest, -WidthDelta, -HeightDelta);
    }
    else if (D3DQA_SHRINK == StretchRectCases[dwTableIndex].StretchType)
    {
        // Shrinking

        // Use a ninth of the destination ultimately.
        WidthDest /= 3;
        HeightDest /= 3;

        // Calculate the amount of room to stretch on each side
        WidthDelta = (WidthSource - WidthDest) / 2;
        HeightDelta = (HeightSource - HeightDest) / 2;

        // Calculate the Dest rect based on the delta and the current iteration
        WidthDelta = WidthDelta * dwIteration / dwTotalIterations;
        HeightDelta = HeightDelta * dwIteration / dwTotalIterations;

        InflateRect(pRectDest, -WidthDelta, -HeightDelta);
    }
    else if (D3DQA_COPY == StretchRectCases[dwTableIndex].StretchType)
    {
        RECT rcTemp;
        IntersectRect(&rcTemp, pRectSource, pRectDest);
        CopyRect(pRectSource, &rcTemp);
        CopyRect(pRectDest, &rcTemp);
    }

cleanup:
    return hr;
}

HRESULT GetFilterType(
    LPDIRECT3DMOBILEDEVICE pDevice,
    D3DMTEXTUREFILTERTYPE * d3dtft,
    DWORD dwTableIndex)
{
    D3DMCAPS Caps;

    pDevice->GetDeviceCaps(&Caps);

    *d3dtft = StretchRectCases[dwTableIndex].FilterType;

    //
    // If the driver doesn't support this type of filter with this operation,
    // return S_FALSE to indicate to the test that the test case should log a skip.
    //
    if (D3DMTEXF_LINEAR == *d3dtft &&
        D3DQA_STRETCH == StretchRectCases[dwTableIndex].StretchType &&
        !(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFLINEAR))
    {
        return S_FALSE;
    }
    
    if (D3DMTEXF_LINEAR == *d3dtft &&
        D3DQA_SHRINK == StretchRectCases[dwTableIndex].StretchType &&
        !(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFLINEAR))
    {
        return S_FALSE;
    }

    if (D3DMTEXF_POINT == *d3dtft &&
        D3DQA_STRETCH == StretchRectCases[dwTableIndex].StretchType &&
        !(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MAGFPOINT))
    {
        return S_FALSE;
    }
    
    if (D3DMTEXF_POINT == *d3dtft &&
        D3DQA_SHRINK == StretchRectCases[dwTableIndex].StretchType &&
        !(Caps.StretchRectFilterCaps & D3DMPTFILTERCAPS_MINFPOINT))
    {
        return S_FALSE;
    }
    return S_OK;
}

HRESULT GetColorsSquare(
    D3DMFORMAT Format,
    int iX,
    int iY,
    int Width,
    int Height,
    float * prRed,
    float * prGreen,
    float * prBlue,
    float * prAlpha)
{
    HRESULT hr = S_OK;
    LONG WidthSource;
    LONG HeightSource;
    static RECT RectSource = {0, 0, 0, 0};

    if (0 == iX && 0 == iY)
    {
        // 
        // Fill in our rects with the full surface sizes.
        //
        RectSource.left = RectSource.top = 0;

        RectSource.right = Width;
        RectSource.bottom = Height;

        WidthSource = RectSource.right - RectSource.left;
        HeightSource = RectSource.bottom - RectSource.top;

        // Use a ninth of the source.
        WidthSource /= 3;
        HeightSource /= 3;

        InflateRect(&RectSource, -WidthSource, -HeightSource);

        // Recalculate to eliminate rounding problems.
        WidthSource = RectSource.right - RectSource.left;
        HeightSource = RectSource.bottom - RectSource.top;
    }

    if (iX >= RectSource.left && iX < RectSource.right &&
        iY >= RectSource.top && iY < RectSource.bottom)
    {
        *prRed = *prGreen = *prBlue = 1.0;
    }
    else
    {
        *prRed = *prGreen = *prBlue = 0.0;
    }

    *prAlpha = 1.0;

    return hr;
}
