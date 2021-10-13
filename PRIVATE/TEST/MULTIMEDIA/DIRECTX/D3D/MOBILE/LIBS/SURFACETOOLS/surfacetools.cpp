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
#include "utils.h"
#include "DebugOutput.h"

//
// FullCoverageRECTs
//
//   Generates a set of RECTs that each cover the specified extents.
//
// Arguments:
// 
//   RECT **ppRects [out]: a pointer to the resultant RECT list
//   UINT uiNumRects [in]: number of RECTs desired
//   UINT uiWidth    [in]: surface width
//   UINT uiHeight   [in]: surface height
//
// Return Value:
//
//   HRESULT:  Indicates success or failure of function
// 
HRESULT FullCoverageRECTs(RECT **ppRects, UINT uiNumRects, UINT uiWidth, UINT uiHeight)
{

	//
	// Iterator for generating region data; pointer to "current" RECT
	//
	UINT uiCurRect;
	RECT *pCurRect;

	//
	// Bad parameter checking
	//
	if (NULL == ppRects)
		return E_FAIL;

	if (0 == uiNumRects || uiNumRects > UINT_MAX / sizeof(RECT))
		return E_FAIL;

	if ((0 == uiWidth) || (0 == uiHeight))
		return E_FAIL;

	//
	// Allocate adequate space for RECTs
	//
	PREFAST_SUPPRESS(419, "uiNumRects is checked above");
	*ppRects = (RECT*)malloc(sizeof(RECT)*uiNumRects);
	if (NULL == *ppRects)
	{
		return E_FAIL;
	}

	//
	// The following code produces a set of RECTs that is each cover the entire extents
	//
	for (uiCurRect = 0, pCurRect = *ppRects; uiCurRect < uiNumRects; uiCurRect++)
	{
		pCurRect->left   = 0;
		pCurRect->top    = 0;
		pCurRect->right  = uiWidth;
		pCurRect->bottom = uiHeight;

		pCurRect++;
	}

	return S_OK;
}


//
// CopySurfaceToBackBuffer
//
//   Copies the given surface directly to the backbuffer. Useful for testing
//   2D operations such as StretchRect or CopyRects.
//
// Arguments:
// 
//   LPDIRECT3DMOBILEDEVICE pDevice   [in]: The device to use.
//   LPDIRECT3DMOBILESURFACE pSurface [in]: The surface to copy to the device's first backbuffer.
//   BOOL fStretch                    [in]: If true, stretch or shrink to fit the backbuffer (otherwise copy the intersection).
//
// Return Value:
//
//   HRESULT:  Indicates success or failure of function
// 
HRESULT CopySurfaceToBackBuffer(
    LPDIRECT3DMOBILEDEVICE pDevice,
    LPDIRECT3DMOBILESURFACE pSurface,
    BOOL fStretch)
{

	//
	// Surface descriptions for source and destination of StretchRect call
	//
	D3DMSURFACE_DESC SourceDesc;
	D3DMSURFACE_DESC DestDesc;

	//
    // For CheckDeviceFormatConversion call
    //
    LPDIRECT3DMOBILE pD3D = NULL;

	// 
	// Device capabilities
	//
	D3DMCAPS Caps;

	HRESULT hr;
    LPDIRECT3DMOBILESURFACE pSurfaceBackBuffer = NULL;
    RECT RectSource, RectBackBuffer, RectIntersection;

    if (FAILED(hr = pDevice->GetBackBuffer(0, D3DMBACKBUFFER_TYPE_MONO, &pSurfaceBackBuffer)))
    {
        DebugOut(DO_ERROR, L"Could not get backbuffer surface. hr = 0x%08x", hr);
        goto cleanup;
    }

    //
    // If the surface is the backbuffer, there is no need to copy.
    //
    if (pSurfaceBackBuffer == pSurface)
    {
        hr = S_OK;
        goto cleanup;
    }

	//
	// Retrieve device capabilities
	//
	if (FAILED(hr = pDevice->GetDeviceCaps(&Caps))) // D3DMCAPS* pCaps
	{
		DebugOut(DO_ERROR, L"GetDeviceCaps failed. hr = 0x%08x", hr);
		goto cleanup;
	}

	//
	// Get description of source surface (for the purposes of format inspection)
	//
	if (FAILED(hr = pSurface->GetDesc(&SourceDesc))) // D3DMSURFACE_DESC *pDesc
	{
        DebugOut(DO_ERROR, L"LPDIRECT3DMOBILESURFACE->GetDesc failed. hr = 0x%08x", hr);
        goto cleanup;
	}

	//
	// Get description of destination surface (for the purposes of format inspection)
	//
	if (FAILED(hr = pSurfaceBackBuffer->GetDesc(&DestDesc))) // D3DMSURFACE_DESC *pDesc
	{
        DebugOut(DO_ERROR, L"LPDIRECT3DMOBILESURFACE->GetDesc failed. hr = 0x%08x", hr);
        goto cleanup;
	}

    //
    // Determine the rects for copying the surface to the backbuffer.
    //
    SetRect(&RectBackBuffer, 0, 0, DestDesc.Width, DestDesc.Height);
    SetRect(&RectSource, 0, 0, SourceDesc.Width, SourceDesc.Height);

    if (!fStretch)
    {
        IntersectRect(&RectIntersection, &RectSource, &RectBackBuffer);
        CopyRect(&RectSource, &RectIntersection);
        CopyRect(&RectBackBuffer, &RectIntersection);
    }

	//
	// Get Direct3D Mobile object
	//
	if (FAILED(hr = pDevice->GetDirect3D(&pD3D))) // IDirect3DMobile** ppD3DM
	{
		DebugOut(DO_ERROR, L"GetDirect3D failed. hr = 0x%08x", hr);
		goto cleanup;
	}

	//
	// Does the device support this format conversion?
	//
	if (FAILED(hr = pD3D->CheckDeviceFormatConversion(Caps.AdapterOrdinal, // UINT Adapter,
	                                                  Caps.DeviceType,     // D3DMDEVTYPE DeviceType,
	                                                  SourceDesc.Format,   // D3DMFORMAT SourceFormat,
	                                                  DestDesc.Format)))   // D3DMFORMAT DestFormat
	{
        DebugOut(DO_ERROR, L"LPDIRECT3DMOBILESURFACE->CheckDeviceFormatConversion failed. hr = 0x%08x", hr);
        goto cleanup;
	}

    //
    // Use the point filter to copy the surfaces.
    //
    if (FAILED(hr = pDevice->StretchRect(pSurface, &RectSource, pSurfaceBackBuffer, &RectBackBuffer, D3DMTEXF_POINT)))
    {
        DebugOut(DO_ERROR, L"StretchRect failed copying result surface to backbuffer. hr = 0x%08x", hr);
    }

cleanup:
    if (pSurfaceBackBuffer)
        pSurfaceBackBuffer->Release();
	if (pD3D)
		pD3D->Release();

    return hr;
}


//
// SurfaceGradient
//
//   Generates a gradient across the surface.
//   (Red max at left edge, blue max at right edge, and green max at bottom).
//
// Arguments:
// 
//   LPDIRECT3DMOBILESURFACE pSurface [in]: The surface to fill with the gradient.
//
// Return Value:
//
//   HRESULT:  Indicates success or failure of function
// 
HRESULT SurfaceGradient(
    LPDIRECT3DMOBILESURFACE pSurface,
    PFNGETCOLORS pfnGetColors)
{
    UINT Width = 0, Height = 0;
    D3DMFORMAT Format;
    D3DMSURFACE_DESC SurfaceDesc;
    D3DMLOCKED_RECT LockedRect = {0};
    HRESULT hr;
    LPDIRECT3DMOBILEDEVICE pDevice = NULL;
    LPDIRECT3DMOBILESURFACE pSurfaceTemp = NULL;

    //
    // These values will be needed to determine the colors in the gradient.
    //
    int iAlphaOffset;
    int iRedOffset;
    int iGreenOffset;
    int iBlueOffset;
    int iAlphaMax;
    int iRedMax;
    int iGreenMax;
    int iBlueMax;
    unsigned int uiAlphaMask;
    unsigned int uiRedMask;
    unsigned int uiGreenMask;
    unsigned int uiBlueMask;
    int iY, iX;

    if (FAILED(hr = pSurface->GetDesc(&SurfaceDesc)))
    {
        DebugOut(DO_ERROR, L"Could not get surface description of source surface to setup. hr = 0x%08x", hr);
        goto cleanup;
    }

    Width = SurfaceDesc.Width;
    Height = SurfaceDesc.Height;
    Format = SurfaceDesc.Format;

    if (FAILED(hr = pSurface->LockRect(&LockedRect, NULL, 0)))
    {
        //
        // The surface cannot be locked, so create a temporary image surface instead.
        // We will fill that surface with a gradient, and then copy over to the
        // test source surface.
        //
        if (FAILED(hr = pSurface->GetDevice(&pDevice)))
        {
            DebugOut(DO_ERROR, L"Could not get device with which to create temp image surface. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = pDevice->CreateImageSurface(Width, Height, Format, &pSurfaceTemp)))
        {
            DebugOut(DO_ERROR, L"Could not create temporary image surface. hr = 0x%08x", hr);
            goto cleanup;
        }

        if (FAILED(hr = pSurfaceTemp->LockRect(&LockedRect, NULL, 0)))
        {
            DebugOut(DO_ERROR, L"Could not lock image surface. hr = 0x%08x", hr);
            goto cleanup;
        }
    }

    //
    // Gradient fill code (copied from the Imaging test library)
    //
    GetPixelFormatInformation(
        Format,
        uiAlphaMask,
        uiRedMask,
        uiGreenMask,
        uiBlueMask,
        iAlphaOffset,
        iRedOffset,
        iGreenOffset,
        iBlueOffset);

    iAlphaMax = uiAlphaMask >> iAlphaOffset;
    iRedMax   = uiRedMask   >> iRedOffset;
    iGreenMax = uiGreenMask >> iGreenOffset;
    iBlueMax  = uiBlueMask  >> iBlueOffset;

    //
    // Fill in the surface with a gradient.
    //
    for (iY = 0; iY < (int) Height; iY++)
    {
        for (iX = 0; iX < (int) Width; iX++)
        {
            UINT uiPixel;
            int iRed;
            int iGreen;
            int iBlue;
            int iAlpha;
            if (pfnGetColors)
            {
                float rRed;
                float rGreen;
                float rBlue;
                float rAlpha;
                pfnGetColors(Format, iX, iY, Width, Height, &rRed, &rGreen, &rBlue, &rAlpha);
                iRed   = (int)(iRedMax   * rRed);
                iGreen = (int)(iGreenMax * rGreen);
                iBlue  = (int)(iBlueMax  * rBlue);
                iAlpha = (int)(iAlphaMax * rAlpha);
            }
            else
            {
                iRed = iX * iRedMax / Width;
                iGreen = iY * iGreenMax / Height;
                iBlue = iBlueMax - (iX * iBlueMax / Width);
                iAlpha = iAlphaMax;
            }
            uiPixel = 
                (iAlpha << iAlphaOffset) | 
                (iRed << iRedOffset) | 
                (iGreen << iGreenOffset) | 
                (iBlue << iBlueOffset);
            SetPixelLockedRect(Format, LockedRect, iX, iY, uiPixel);
        }
    }

    //
    // If we needed to create a temporary surface to construct the image,
    // copy the image back to the test surface.
    //
    if (pSurfaceTemp)
    {
        RECT rcSource = {0, 0, Width, Height}, rcDest = {0, 0, Width, Height};
        pSurfaceTemp->UnlockRect();
        pDevice->CopyRects(pSurfaceTemp, NULL, 0, pSurface, NULL);
    }
    else
    {
        pSurface->UnlockRect();
    }
    
cleanup:
    if (pSurfaceTemp)
        pSurfaceTemp->Release();
    if (pDevice)
        pDevice->Release();
    return hr;
}   

//
// SurfaceIntersection
//
//   Determines the intersection of up to two surfaces and the device mode.
//
// Arguments:
// 
//   LPDIRECT3DMOBILESURFACE pBackbuffer   [in]           : The device's backbuffer from which to determine the mode size
//   LPDIRECT3DMOBILESURFACE pSurface1     [in]           : The first surface to intersect with the device extents
//   LPDIRECT3DMOBILESURFACE pSurface2     [in, optional] : The second surface to intersect with the device extents (can be NULL)
//   RECT                   *pIntersection [out]          : The intersection of the surfaces with the device extents.
//
// Return Value:
//
//   HRESULT:  Indicates success or failure of function
// 
HRESULT SurfaceIntersection(LPDIRECT3DMOBILESURFACE pBackbuffer, LPDIRECT3DMOBILESURFACE pSurface1, LPDIRECT3DMOBILESURFACE pSurface2, RECT * pIntersection)
{
    RECT rcDevice, rc1, rc2, rcTemp;
    D3DMSURFACE_DESC SurfaceDesc;
    HRESULT hr;

    if (NULL == pBackbuffer || NULL == pSurface1 || NULL == pIntersection)
    {
        return E_POINTER;
    }


    hr = pBackbuffer->GetDesc(&SurfaceDesc);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SurfaceIntersection: Backbuffer GetDesc failed. hr = 0x%08x", hr);
        return hr;
    }
    
    SetRect(&rcDevice, 0, 0, SurfaceDesc.Width, SurfaceDesc.Height);

    hr = pSurface1->GetDesc(&SurfaceDesc);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SurfaceIntersection: Surface1 GetDesc failed. hr = 0x%08x", hr);
        return hr;
    }

    SetRect(&rc1, 0, 0, SurfaceDesc.Width, SurfaceDesc.Height);
    IntersectRect(&rcTemp, &rcDevice, &rc1);

    if (NULL == pSurface2)
    {
        CopyRect(pIntersection, &rcTemp);
        return S_OK;
    }

    hr = pSurface2->GetDesc(&SurfaceDesc);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SurfaceIntersection: Surface2 GetDesc failed. hr = 0x%08x");
        return hr;
    }

    SetRect(&rc2, 0, 0, SurfaceDesc.Width, SurfaceDesc.Height);

    IntersectRect(pIntersection, &rcTemp, &rc2);
    return S_OK;
}

