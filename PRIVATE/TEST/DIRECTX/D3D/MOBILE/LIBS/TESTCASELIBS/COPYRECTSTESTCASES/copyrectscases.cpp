//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "CopyRectsCases.h"
#include "QAD3DMX.h"
#include "utils.h"
#include <SurfaceTools.h>

HRESULT AcquireCRDestSurface(
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
        OutputDebugString(L"Cannot get device caps.\n");
        goto cleanup;
    }

    if (D3DQA_BACKBUFFER == CopyRectsCases[dwTableIndex].CreatorDest)
    {
        // If the primary is the requested surface, ignore the width and height.

        // The primary has no parent object that will need to be released.
        *ppParentObject = NULL;
        hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, ppSurface);
    }
    else if (D3DQA_CREATE_IMAGE_SURFACE == CopyRectsCases[dwTableIndex].CreatorDest)
    {
        *ppParentObject = NULL;
        ResourceType = D3DMRTYPE_SURFACE;
    
        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            OutputDebugString(L"No valid surface formats found for image surface.\n");
            goto cleanup;
        }
        
        hr = pDevice->CreateImageSurface(dwWidth, dwHeight, Format, ppSurface);
    }
    else if (D3DQA_CREATE_TEXTURE == CopyRectsCases[dwTableIndex].CreatorDest)
    {
        ResourceType = D3DMRTYPE_TEXTURE;
        ResourcePool = CopyRectsCases[dwTableIndex].ResourcePoolDest;

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
            OutputDebugString(L"Skipping because device does not support current texture size. Please choose smaller resolution when running this test.\n");
            OutputDebugString(L"To choose smaller resolution, use one of the /e* command line parameters.\n");
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
            OutputDebugString(L"Device does not support system memory textures.");
            hr = S_FALSE;
            goto cleanup;
        }
        
        if (D3DMPOOL_VIDEOMEM == ResourcePool &&
            !(Caps.SurfaceCaps & D3DMSURFCAPS_VIDTEXTURE))
        {
            OutputDebugString(L"Device does not support video memory textures.");
            hr = S_FALSE;
            goto cleanup;
        }

        //
        // Get the format in which to create the texture.
        //
        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            OutputDebugString(L"No valid surface formats found for texture.\n");
            goto cleanup;
        }

        //
        // Create the texture.
        //
        if (FAILED(hr = pDevice->CreateTexture(dwWidth, dwHeight, 1, D3DMUSAGE_RENDERTARGET, Format, ResourcePool, &pTexture)))
        {
            OutputDebugString(L"Could not create texture for destination.\n");
            goto cleanup;
        }

        //
        // Get the unknown pointer for the texture. This will be used to release
        // the texture object when its surface is released in the main test.
        //
        if (FAILED(hr = pTexture->QueryInterface(IID_IUnknown, (void**) ppParentObject)))
        {
            OutputDebugString(L"Could not get unknown interface from texture.\n");
            goto cleanup;
        }

        //
        // Get the surface pointer.
        //
        hr = pTexture->GetSurfaceLevel(0, ppSurface);
        if (FAILED(hr))
        {
            OutputDebugString(L"Could not get surface from texture.\n");
            (*ppParentObject)->Release();
            *ppParentObject = NULL;
            goto cleanup;
        }
    }

    if (*ppSurface)
    {
        if (FAILED(hr = pDevice->ColorFill(*ppSurface, NULL, D3DMCOLOR_XRGB(0, 0, 0))))
        {
            OutputDebugString(L"Could not colorfill destination for preparation");
            goto cleanup;
        }
    }
cleanup:
    if (pTexture)
        pTexture->Release();
    return hr;
}

HRESULT AcquireCRSourceSurface(
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
        OutputDebugString(L"Cannot get device caps.\n");
        goto cleanup;
    }

    if (D3DQA_BACKBUFFER == CopyRectsCases[dwTableIndex].CreatorSource)
    {
        // If the primary is the requested surface, ignore the width and height.

        // The primary has no parent object that will need to be released.
        *ppParentObject = NULL;
        hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, ppSurface);
    }
    else if (D3DQA_CREATE_IMAGE_SURFACE == CopyRectsCases[dwTableIndex].CreatorSource)
    {
        *ppParentObject = NULL;
        ResourceType = D3DMRTYPE_SURFACE;

        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            OutputDebugString(_T("No valid surface formats found.\n"));
            goto cleanup;
        }
        
        hr = pDevice->CreateImageSurface(dwWidth, dwHeight, Format, ppSurface);
    }
    else if (D3DQA_CREATE_TEXTURE == CopyRectsCases[dwTableIndex].CreatorSource)
    {
        ResourceType = D3DMRTYPE_TEXTURE;
        ResourcePool = CopyRectsCases[dwTableIndex].ResourcePoolSource;

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
            OutputDebugString(L"Skipping because device does not support current texture size. Please choose smaller resolution when running this test.\n");
            OutputDebugString(L"To choose smaller resolution, use one of the /e* command line parameters.\n");
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
            OutputDebugString(L"Device does not support system memory textures.");
            hr = S_FALSE;
            goto cleanup;
        }
        
        if (D3DMPOOL_VIDEOMEM == ResourcePool &&
            !(Caps.SurfaceCaps & D3DMSURFCAPS_VIDTEXTURE))
        {
            OutputDebugString(L"Device does not support video memory textures.");
            hr = S_FALSE;
            goto cleanup;
        }
        
        //
        // Get the format in which to create the texture.
        //
        if (FAILED(hr = GetBestFormat(pDevice, ResourceType, &Format)))
        {
            OutputDebugString(L"No valid surface formats found for texture.\n");
            goto cleanup;
        }

        //
        // Create the texture.
        //
        if (FAILED(hr = pDevice->CreateTexture(dwWidth, dwHeight, 1, D3DMUSAGE_RENDERTARGET, Format, ResourcePool, &pTexture)))
        {
            OutputDebugString(L"Could not create texture for destination.\n");
            goto cleanup;
        }

        //
        // Get the unknown pointer for the texture. This will be used to release
        // the texture object when its surface is released in the main test.
        //
        if (FAILED(hr = pTexture->QueryInterface(IID_IUnknown, (void**) ppParentObject)))
        {
            OutputDebugString(L"Could not get unknown interface from texture.\n");
            goto cleanup;
        }

        //
        // Get the surface pointer.
        //
        hr = pTexture->GetSurfaceLevel(0, ppSurface);
        if (FAILED(hr))
        {
            OutputDebugString(L"Could not get surface from texture.\n");
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

HRESULT SetupCRSourceSurface(
    LPDIRECT3DMOBILESURFACE pSurface,
    DWORD dwTableIndex)
{
    HRESULT hr;
    if (FAILED(hr = SurfaceGradient(pSurface)))
    {
        OutputDebugString(L"Could not setup source surface with gradient.\n");
    }
    return hr;
}   

HRESULT GetCopyRects(
    LPDIRECT3DMOBILESURFACE pSurfaceSource,
    LPDIRECT3DMOBILESURFACE pSurfaceDest,
    DWORD dwTableIndex,
    LPRECT rgRectSource,
    LPPOINT rgPointDest,
    UINT * pRectCount,
    UINT * pPointCount)
{
    HRESULT hr = S_OK;
    D3DMSURFACE_DESC d3dsdSource;
    D3DMSURFACE_DESC d3dsdDest;
    RECT RectSource;
    RECT RectDest;
    LONG WidthSource;
    LONG HeightSource;
    LONG WidthDest;
    LONG HeightDest;
    const int c_RectCount = 18;

    if (*pRectCount < c_RectCount || *pPointCount < c_RectCount)
    {
        OutputDebugString(L"GetCopyRects called with fewer than necessary rects and points");
        goto cleanup;
    }
    
    //
    // Get the surface descriptions so we can determine what sizes to use.
    //
    if (FAILED(hr = pSurfaceSource->GetDesc(&d3dsdSource)))
    {
        OutputDebugString(L"Could not get surface description of source.\n");
        goto cleanup;
    }

    SetRect(&RectSource, 0, 0, d3dsdSource.Width, d3dsdSource.Height);
    WidthSource = d3dsdSource.Width;
    HeightSource = d3dsdSource.Height;

    if (FAILED(hr = pSurfaceDest->GetDesc(&d3dsdDest)))
    {
        OutputDebugString(L"Could not get surface description of destination.\n");
        goto cleanup;
    }

    SetRect(&RectDest, 0, 0, d3dsdDest.Width, d3dsdDest.Height);
    WidthDest = d3dsdDest.Width;
    HeightDest = d3dsdDest.Height;

    if (D3DQA_FULLSURFACE == CopyRectsCases[dwTableIndex].CopyType)
    {
        //
        // The full screen will have destination 0,0
        //
        *pPointCount = 0;
        *pRectCount = 0;

        if (!EqualRect(&RectSource, &RectDest))
        {
            RECT RectUnion;
            UnionRect(&RectUnion, &RectSource, &RectDest);
            if (EqualRect(&RectUnion, &RectSource))
            {
                //
                // The source surface completely contains the dest surface.
                // We cannot copy the full surface because it won't fit in the dest.
                //
                hr = S_FALSE;
                OutputDebugString(L"Source is larger than Dest, cannot fullscreen copy");
                goto cleanup;
            }
            else if (EqualRect(&RectUnion, &RectDest))
            {
                //
                // The source surface is completely contained by the dest surface.
                // We can copy the full surface this way.
                //

                //
                // Since the source won't entirely fill the destination, create
                // four points such that four copies of the full source will
                // completely cover the destination surface.
                //

                rgPointDest[0].x = 0;
                rgPointDest[0].y = 0;
                rgPointDest[1].x = WidthDest - WidthSource;
                rgPointDest[1].y = 0;
                rgPointDest[2].x = 0;
                rgPointDest[2].y = HeightDest - HeightSource;
                rgPointDest[3].x = WidthDest - WidthSource;
                rgPointDest[3].y = HeightDest - HeightSource;
                *pPointCount = 4;
            }
            else
            {   
                hr = S_FALSE;
                OutputDebugString(L"Source is larger than Dest in at least one dimension, cannot fullscreen copy");
                goto cleanup;
            }
        }
    }
    else if (D3DQA_SOURCENODEST == CopyRectsCases[dwTableIndex].CopyType)
    {
        RECT Rect;
        UINT Width;
        UINT Height;
        *pPointCount = 0;
        *pRectCount = 17;

        //
        // Determine the rect that we can use for copying. This must be the
        // intersection of the source and the dest, since if any copy rects
        // fall in any part out of the dest or source, the copy call will fail.
        //

        IntersectRect(&Rect, &RectSource, &RectDest);
        Width = Rect.right - Rect.left;
        Height = Rect.bottom - Rect.top;

        //
        // Rect 0 (center copy)
        //
        SetRect(rgRectSource, 3 * Width / 12, 3 * Height / 12, 9 * Width / 12, 9 * Height / 12);

        //
        // Rect 1 - 4 (touching corners of center)
        SetRect(rgRectSource + 1, 2 * Width / 12, 2 * Height / 12,  4 * Width / 12,  4 * Height / 12);
        SetRect(rgRectSource + 2, 8 * Width / 12, 2 * Height / 12, 10 * Width / 12,  4 * Height / 12);
        SetRect(rgRectSource + 3, 2 * Width / 12, 8 * Height / 12,  4 * Width / 12, 10 * Height / 12);
        SetRect(rgRectSource + 4, 8 * Width / 12, 8 * Height / 12, 10 * Width / 12, 10 * Height / 12);

        //
        // Rect 5 - 8 (touching sides of center)
        SetRect(rgRectSource + 5, 2 * Width / 12, 5 * Height / 12,   4 * Width / 12,   7 * Height / 12);
        SetRect(rgRectSource + 6, 8 * Width / 12, 5 * Height / 12,  10 * Width / 12,   7 * Height / 12);
        SetRect(rgRectSource + 7, 5 * Width / 12, 2 * Height / 12,   7 * Width / 12,   4 * Height / 12);
        SetRect(rgRectSource + 8, 5 * Width / 12, 8 * Height / 12,   7 * Width / 12,  10 * Height / 12);

        //
        // Rect 9 - 12 (the four outer corners)
        SetRect(rgRectSource +  9,              0,               0,  3 * Width / 12,  3 * Height / 12);
        SetRect(rgRectSource + 10, 9 * Width / 12,               0, 12 * Width / 12,  3 * Height / 12);
        SetRect(rgRectSource + 11,              0, 9 * Height / 12,  3 * Width / 12, 12 * Height / 12);
        SetRect(rgRectSource + 12, 9 * Width / 12, 9 * Height / 12, 12 * Width / 12, 12 * Height / 12);

        //
        // Rect 13 - 16 (the four outer edges)
        SetRect(rgRectSource + 13,              0, 5 * Height / 12,  3 * Width / 12,  7 * Height / 12);
        SetRect(rgRectSource + 14, 9 * Width / 12, 5 * Height / 12, 12 * Width / 12,  7 * Height / 12);
        SetRect(rgRectSource + 15, 5 * Width / 12,               0,  7 * Width / 12,  3 * Height / 12);
        SetRect(rgRectSource + 16, 5 * Width / 12, 9 * Height / 12,  7 * Width / 12, 12 * Height / 12);
    }
    else if (D3DQA_SOURCEANDDEST == CopyRectsCases[dwTableIndex].CopyType)
    {
        RECT Rect;
        UINT Width;
        UINT Height;
        *pPointCount = 18;
        *pRectCount = 18;

        //
        // Determine the rect that we can use for copying. This must be the
        // intersection of the source and the dest, since if any copy rects
        // fall in any part out of the dest or source, the copy call will fail.
        //

        IntersectRect(&Rect, &RectSource, &RectDest);
        Width = Rect.right - Rect.left;
        Height = Rect.bottom - Rect.top;

        //
        // Rect,Point 0 (full copy)
        SetRect(rgRectSource, 0, 0, Width, Height);
        rgPointDest[0].x = 0;
        rgPointDest[0].y = 0;

        //
        // Rect 1 (center copy)
        SetRect(rgRectSource + 1, 1 * Width / 12, 1 * Height / 12, 7 * Width / 12, 7 * Height / 12);
        rgPointDest[1].x = 3 *  Width / 12;
        rgPointDest[1].y = 3 * Height / 12;

        //
        // Rect 2 - 5 (touching corners of center)
        SetRect(rgRectSource + 2, 2 * Width / 12, 2 * Height / 12,  4 * Width / 12,  4 * Height / 12);
        rgPointDest[2].x = 8 *  Width / 12;
        rgPointDest[2].y = 8 * Height / 12;
        SetRect(rgRectSource + 3, 8 * Width / 12, 2 * Height / 12, 10 * Width / 12,  4 * Height / 12);
        rgPointDest[3].x = 2 *  Width / 12;
        rgPointDest[3].y = 8 * Height / 12;
        SetRect(rgRectSource + 4, 2 * Width / 12, 8 * Height / 12,  4 * Width / 12, 10 * Height / 12);
        rgPointDest[4].x = 8 *  Width / 12;
        rgPointDest[4].y = 2 * Height / 12;
        SetRect(rgRectSource + 5, 8 * Width / 12, 8 * Height / 12, 10 * Width / 12, 10 * Height / 12);
        rgPointDest[5].x = 2 *  Width / 12;
        rgPointDest[5].y = 2 * Height / 12;

        //
        // Rect 6 - 9 (touching sides of center)
        SetRect(rgRectSource + 6, 2 * Width / 12, 5 * Height / 12,   4 * Width / 12,   7 * Height / 12);
        rgPointDest[6].x = 8 *  Width / 12;
        rgPointDest[6].y = 5 * Height / 12;
        SetRect(rgRectSource + 7, 8 * Width / 12, 5 * Height / 12,  10 * Width / 12,   7 * Height / 12);
        rgPointDest[7].x = 2 *  Width / 12;
        rgPointDest[7].y = 5 * Height / 12;
        SetRect(rgRectSource + 8, 5 * Width / 12, 2 * Height / 12,   7 * Width / 12,   4 * Height / 12);
        rgPointDest[8].x = 5 *  Width / 12;
        rgPointDest[8].y = 8 * Height / 12;
        SetRect(rgRectSource + 9, 5 * Width / 12, 8 * Height / 12,   7 * Width / 12,  10 * Height / 12);
        rgPointDest[9].x = 5 *  Width / 12;
        rgPointDest[9].y = 2 * Height / 12;

        //
        // Rect 10 - 13 (the four outer corners)
        SetRect(rgRectSource + 10,              0,               0,  3 * Width / 12,  3 * Height / 12);
        rgPointDest[10].x = 0;
        rgPointDest[10].y = 5 * Height / 12;
        SetRect(rgRectSource + 11, 9 * Width / 12,               0, 12 * Width / 12,  3 * Height / 12);
        rgPointDest[11].x = 9 *  Width / 12;
        rgPointDest[11].y = 5 * Height / 12;
        SetRect(rgRectSource + 12,              0, 9 * Height / 12,  3 * Width / 12, 12 * Height / 12);
        rgPointDest[12].x = 5 *  Width / 12;
        rgPointDest[12].y = 0;
        SetRect(rgRectSource + 13, 9 * Width / 12, 9 * Height / 12, 12 * Width / 12, 12 * Height / 12);
        rgPointDest[13].x = 5 *  Width / 12;
        rgPointDest[13].y = 9 * Height / 12;

        //
        // Rect 14 - 17 (the four outer edges)
        SetRect(rgRectSource + 14,              0, 5 * Height / 12,  3 * Width / 12,  7 * Height / 12);
        rgPointDest[14].x = 0;
        rgPointDest[14].y = 0;
        SetRect(rgRectSource + 15, 9 * Width / 12, 5 * Height / 12, 12 * Width / 12,  7 * Height / 12);
        rgPointDest[15].x = 9 *  Width / 12;
        rgPointDest[15].y = 0;
        SetRect(rgRectSource + 16, 5 * Width / 12,               0,  7 * Width / 12,  3 * Height / 12);
        rgPointDest[16].x = 0;
        rgPointDest[16].y = 9 * Height / 12;
        SetRect(rgRectSource + 17, 5 * Width / 12, 9 * Height / 12,  7 * Width / 12, 12 * Height / 12);
        rgPointDest[17].x = 9 *  Width / 12;
        rgPointDest[17].y = 9 * Height / 12;
    }

cleanup:
    return hr;
}

