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
#include "DebugOutput.h"
#include "MultiInstTestHelper.h"
#include "TextureTools.h"

template<class _T>
D3DMVALUE MakeVal(_T value)
{
    _T temp = value;
    return *((D3DMVALUE*)&temp);
}

template<class _T>
D3DMCOLORVALUE MakeColor(_T a, _T r, _T g, _T b)
{
    D3DMCOLORVALUE ret;
    ret.a = MakeVal(a);
    ret.r = MakeVal(r);
    ret.g = MakeVal(g);
    ret.b = MakeVal(b);
    return ret;
}

template<class _T>
D3DMVECTOR MakeVector(_T x, _T y, _T z)
{
    D3DMVECTOR ret;
    ret.x = MakeVal(x);
    ret.y = MakeVal(y);
    ret.z = MakeVal(z);
    return ret;
}

HRESULT MultiInstSetupRendering(
    IDirect3DMobileDevice * pDevice,
    MultiInstOptions * pOptions);
    
HRESULT MultiInstGenerateGeometry(
    IDirect3DMobileDevice * pDevice, 
    int cvX, 
    int cvY, 
    int iCurrentY, 
    DWORD dwFVF,
    BOOL UseAlpha,
    IDirect3DMobileVertexBuffer * pVBuffer);

HRESULT MultiInstRenderScene(IDirect3DMobileDevice * pDevice, MultiInstOptions * pOptions)
{
    // Create resources required
    // Need: 
    // vertex buffer
    DebugOut(L"Entering MultiInstRenderScene (PID: %p)", GetCurrentProcessId());
    IDirect3DMobileVertexBuffer * pVBuffer = NULL;
    HRESULT hr;
    D3DMPOOL TexturePool = D3DMPOOL_VIDEOMEM;
    D3DMCAPS Caps;

    const int cvX = 200;
    const int cvY = 200;
    int VBLength;
    int StripTriangleCount = cvX * 2;

    if (!pDevice || !pOptions)
    {
        hr = E_POINTER;
        DebugOut(DO_ERROR, L"MultiInstRenderScene called with invalid pointer.");
        goto cleanup;
    }

    VBLength = (StripTriangleCount + 2);
    if (D3DMMULTIINSTTEST_FVF_TEX == pOptions->FVF)
    {
        VBLength = VBLength * sizeof(D3DMMULTIINSTTEST_VERT_TEX);
    }
    else if (D3DMMULTIINSTTEST_FVF_NOTEX == pOptions->FVF)
    {
        VBLength = VBLength * sizeof(D3DMMULTIINSTTEST_VERT_NOTEX);
    }

    hr = pDevice->GetDeviceCaps(&Caps);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not get device caps. (hr = 0x%08x)", hr);
        goto cleanup;
    }

    if (!(Caps.SurfaceCaps & D3DMSURFCAPS_VIDTEXTURE))
    {
        TexturePool = D3DMPOOL_SYSTEMMEM;
    }
    
    hr = pDevice->CreateVertexBuffer(VBLength, 0, pOptions->FVF, TexturePool, &pVBuffer);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, 
            L"Could not create vertex buffer (Length %d, FVF 0x%08x, Pool 0x%08x). (hr = 0x%08x)",
            VBLength,
            pOptions->FVF,
            TexturePool,
            hr);
        goto cleanup;
    }

    hr = pDevice->SetStreamSource(0, pVBuffer, 0);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Could not set the vertex buffer as the stream source. (hr = 0x%08x)",
            hr);
        goto cleanup;
    }
    
    // Setup the Render States
    hr = MultiInstSetupRendering(pDevice, pOptions);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"MultiInstSetupRendering Failed. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    
    // Do the rendering
    hr = pDevice->BeginScene();
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Could not BeginScene. (hr = 0x%08x)",
            hr);
        goto cleanup;
    }

    hr = pDevice->Clear(
        0, 
        NULL, 
        D3DMCLEAR_TARGET | D3DMCLEAR_ZBUFFER, 
        D3DMCOLOR_XRGB(255, 0, 0), 
        1.0, 
        0);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Could not Clear. (hr = 0x%08x)", hr);
        goto cleanup;
    }
    
    
    // For each strip:
    for (int Strip = 0; Strip < cvY; ++Strip)
    {
        // (having the strip be generated each time is a large overhead, but this will be useful
        //  in causing more areas where two different processes could collide if a driver is written incorrectly).
        // Generate a strip
        hr = MultiInstGenerateGeometry(pDevice, cvX, cvY, Strip, pOptions->FVF, pOptions->VertexAlpha, pVBuffer);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"Could not generate geometry for strip %d of %d. (hr = 0x%08x)",
                Strip,
                cvY,
                hr);
            goto cleanup;
        }

        // Render the strip
        hr = pDevice->DrawPrimitive(D3DMPT_TRIANGLESTRIP, 0, StripTriangleCount);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR,
                L"DrawPrimitive failed for strip %d of %d. (hr = 0x%08x)",
                Strip,
                cvY,
                hr);
            goto cleanup;
        }
    }

    hr = pDevice->EndScene();
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"EndScene failed. (hr = 0x%08x)",
            hr);
        goto cleanup;
    }

    // Present!
    hr = pDevice->Present(NULL, NULL, NULL, NULL);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR,
            L"Present failed. (hr = 0x%08x)",
            hr);
        goto cleanup;
    }

    // Cleanup
cleanup:
    if (pDevice)
    {
        pDevice->SetTexture(0, NULL);
        if (pVBuffer)
        {
            pDevice->SetStreamSource(0, NULL, 0);
            pVBuffer->Release();
            pVBuffer = NULL;
        }
    }

    DebugOut(L"Leaving MultiInstRenderScene (PID: %p)", GetCurrentProcessId());
    return hr;
}

HRESULT MultiInstSetupRendering(
    IDirect3DMobileDevice * pDevice,
    MultiInstOptions * pOptions)
{
    HRESULT hr;
    // Set up Lighting
    D3DMLIGHT DirectionalLight;
    D3DMLIGHT PointLight;
    
    // Add a directional light
    DirectionalLight.Type = D3DMLIGHT_DIRECTIONAL;
    DirectionalLight.Diffuse = MakeColor(1.0f, 0.0f, 1.0f, 0.0f);
    DirectionalLight.Specular = MakeColor(1.0f, 0.0f, 0.0f, 0.0f);
    DirectionalLight.Ambient = MakeColor(1.0f, 0.5f, 0.0f, 0.0f);
    DirectionalLight.Position = MakeVector(0.0f, 0.0f, 0.0f);
    DirectionalLight.Direction = MakeVector(0.0f, 0.0f, 0.5f);
    DirectionalLight.Range = 0.0f;
    DirectionalLight.Attenuation0 = 0.0f;
    DirectionalLight.Attenuation1 = 0.0f;
    DirectionalLight.Attenuation2 = 0.0f;
    hr = pDevice->SetLight(0, &DirectionalLight, D3DMFMT_D3DMVALUE_FLOAT);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetLight with Directional Light failed. (hr = 0x%08x)", hr);
        return hr;
    }
    hr = pDevice->LightEnable(0, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"LightEnable 0 failed. (hr = 0x%08x)", hr);
        return hr;
    }
    
    
    // Add a positional light
    PointLight.Type = D3DMLIGHT_POINT;
    PointLight.Diffuse = MakeColor(1.0f, 0.0f, 0.0f, 1.0f);
    PointLight.Specular = MakeColor(1.0f, 0.0f, 0.0f, 0.0f);
    PointLight.Ambient = MakeColor(1.0f, 0.5f, 0.0f, 0.0f);
    PointLight.Position = MakeVector(-0.5f, 0.5f, 0.5f);
    PointLight.Direction = MakeVector(0.0f, 0.0f, 1.0f);
    PointLight.Range = 5.0f;
    PointLight.Attenuation0 = 0.0f;
    PointLight.Attenuation1 = 1.0f;
    PointLight.Attenuation2 = 0.0f;
    hr = pDevice->SetLight(1, &PointLight, D3DMFMT_D3DMVALUE_FLOAT);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetLight with Point Light failed. (hr = 0x%08x)", hr);
        return hr;
    }

    hr = pDevice->LightEnable(1, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"LightEnable 1 failed. (hr = 0x%08x)", hr);
        return hr;
    }
    
    // Set lighting render state
    hr = pDevice->SetRenderState(D3DMRS_LIGHTING, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(LIGHTING, TRUE) failed. (hr = 0x%08x)", hr);
        return hr;
    }

    hr = pDevice->SetRenderState(D3DMRS_COLORVERTEX, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(COLORVERTEX, TRUE) failed. (hr = 0x%08x)", hr);
        return hr;
    }
    
    hr = pDevice->SetRenderState(D3DMRS_DIFFUSEMATERIALSOURCE, D3DMMCS_COLOR1);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(DIFFUSEMATERIALSOURCE, COLOR1) failed. (hr = 0x%08x)", hr);
        return hr;
    }

    hr = pDevice->SetRenderState(D3DMRS_NORMALIZENORMALS, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(NORMALIZENORMALS, TRUE) failed. (hr = 0x%08x)", hr);
        return hr;
    }
    
    hr = pDevice->SetRenderState(D3DMRS_AMBIENT, D3DMCOLOR_XRGB(0xff, 0x00, 0x00));
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(AMBIENT) failed. (hr = 0x%08x)", hr);
        return hr;
    }
    

    // Set up shading
    // Choose between flat and Gouraud
    hr = pDevice->SetRenderState(D3DMRS_SHADEMODE, pOptions->Shading);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(SHADEMODE, 0x%08x) failed. (hr = 0x%08x)", pOptions->Shading, hr);
        return hr;
    }
    

    // Set up alpha blending
    // If supported
    if (pOptions->VertexAlpha)
    {
        hr = pDevice->SetRenderState(D3DMRS_ALPHABLENDENABLE, TRUE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(ALPHABLENDENABLE, TRUE) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetRenderState(D3DMRS_SRCBLEND, D3DMBLEND_SRCALPHA);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(SRCBLEND, SRCALPHA) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetRenderState(D3DMRS_DESTBLEND, D3DMBLEND_INVDESTALPHA);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(DESTBLEND, INVDESTALPHA) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetRenderState(D3DMRS_BLENDOP, D3DMBLENDOP_ADD);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(BLENDOP, ADD) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        
    }

    // Set up Depth buffering
    hr = pDevice->SetRenderState(D3DMRS_ZWRITEENABLE, TRUE);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"SetRenderState(ZWRITEENABLE, TRUE) failed. (hr = 0x%08x)", hr);
        return hr;
    }
    

    // Set up Culling

    // Set up Fog
    if (pOptions->TableFog || pOptions->VertexFog)
    {
        hr = pDevice->SetRenderState(D3DMRS_FOGENABLE, TRUE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(FOGENABLE, TRUE) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetRenderState(D3DMRS_FOGSTART, MakeVal(0.0f));
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(FOGSTART, 0.0f) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetRenderState(D3DMRS_FOGEND, MakeVal(1.0f));
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(FOGEND, 2.0f) failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetRenderState(D3DMRS_FOGCOLOR, D3DMCOLOR_XRGB(0xff, 0x80, 0x80));
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(FOGCOLOR) failed. (hr = 0x%08x)", hr);
            return hr;
        }
    }
    if (pOptions->VertexFog)
    {
        hr = pDevice->SetRenderState(D3DMRS_FOGVERTEXMODE, D3DMFOG_LINEAR);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_FOGVERTEXMODE, LINEAR) failed. (hr = 0x%08x)", hr);
            return hr;
        }
    }
    else if (pOptions->TableFog)
    {
        hr = pDevice->SetRenderState(D3DMRS_FOGTABLEMODE, D3DMFOG_LINEAR);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetRenderState(D3DMRS_FOGTABLEMODE, LINEAR) failed. (hr = 0x%08x)", hr);
            return hr;
        }
    }


    // Set up Texturing
    if (D3DMMULTIINSTTEST_FVF_NOTEX == pOptions->FVF)
    {
        hr = pDevice->SetTextureStageState(0, D3DMTSS_COLOROP, D3DMTOP_SELECTARG1);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState COLOROP SELECTARG1 failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_COLORARG1, D3DMTA_DIFFUSE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState COLORARG1 DIFFUSE failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAOP, D3DMTOP_SELECTARG1);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState ALPHAOP SELECTARG1 failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAARG1, D3DMTA_DIFFUSE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState ALPHAARG1 DIFFUSE (hr = 0x%08x)", hr);
            return hr;
        }
    }
    else 
    {
        hr = pDevice->SetTextureStageState(0, D3DMTSS_COLOROP, D3DMTOP_MODULATE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState COLOROP MODULATE failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_COLORARG1, D3DMTA_DIFFUSE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState COLORARG1 DIFFUSE failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_COLORARG2, D3DMTA_TEXTURE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState COLORARG1 TEXTURE failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAOP, D3DMTOP_SELECTARG1);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState ALPHAOP SELECTARG1 failed. (hr = 0x%08x)", hr);
            return hr;
        }
        hr = pDevice->SetTextureStageState(0, D3DMTSS_ALPHAARG1, D3DMTA_DIFFUSE);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTextureStageState ALPHAARG1 DIFFUSE (hr = 0x%08x)", hr);
            return hr;
        }

        // Add a texture to the device
        IDirect3DMobileTexture * pTexture = NULL;
        hr = GetGradientTexture(pDevice, 64, 64, &pTexture);
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"GetGradientTexture failed. (hr = 0x%08x)", hr);
            return hr;
        }

        hr = pDevice->SetTexture(0, pTexture);
        pTexture->Release();
        if (FAILED(hr))
        {
            DebugOut(DO_ERROR, L"SetTexture failed. (hr = 0x%08x)", hr);
            return hr;
        }
        
    }
    
    return hr;
}

// MultiInstGenerateGeometry:
// cvX, cvY are the number of quads in the X and Y direction
// iCurrentY indicates the y location of the current strip (from 0 to cvY-1)
HRESULT MultiInstGenerateGeometry(
    IDirect3DMobileDevice * pDevice, 
    int cvX, 
    int cvY, 
    int iCurrentY, 
    DWORD dwFVF,
    BOOL UseAlpha,
    IDirect3DMobileVertexBuffer * pVBuffer)
{
    HRESULT hr;
    
    // How many vertices will we need?
    int cVerts = cvX * 2 + 2;
    
    // Lock the vertex buffer
    int cbStride;
    bool Textured;
    Textured = D3DMMULTIINSTTEST_FVF_TEX == dwFVF;

    if (Textured)
    {
        cbStride = sizeof(D3DMMULTIINSTTEST_VERT_TEX);
    }
    else
    {
        cbStride = sizeof(D3DMMULTIINSTTEST_VERT_NOTEX);
    }

    void * pvVerts = NULL;

    hr = pVBuffer->Lock(0, 0, &pvVerts, 0);
    if (FAILED(hr))
    {
        DebugOut(DO_ERROR, L"Unable to lock vertex buffer. (hr = 0x%08x)", hr);
        return hr;
    }

    // Fill the vertex buffer
    float fX;
    float fXOffset;
    float fY;
    float fYOffset;
    fX = -1.0f;
    fXOffset = 2.0f / (float)cvX;
    fYOffset = 2.0f / (float)cvY;
    fY = -1.0f + ((float)iCurrentY) * fYOffset;
    for (int i = 0; i < cVerts; ++i)
    {
        D3DMMULTIINSTTEST_VERT_TEX * pVertex = (D3DMMULTIINSTTEST_VERT_TEX*)((BYTE*)pvVerts + cbStride * i);
        pVertex->x = fX;
        pVertex->y = fY;
        pVertex->z = 0.9f;
        //   If this is an odd vertex, add the vertex with a y offset (0 is even)
        if (i & 1)
        {
            pVertex->y += fYOffset;
        }
        pVertex->nx = 0.0f;
        pVertex->ny = 0.0f;
        pVertex->nz = -1.0f;
        pVertex->diffuse = 0xffffffff;
        if (UseAlpha)
        {
            DWORD Alpha = (BYTE)((-fX + 1.0f) * 127);
            pVertex->diffuse &= (Alpha << 24) | 0xffffff;
        }
        if (Textured)
        {
            // Texture coordinates are [0.0, 1.0] instead of [-1.0, 1.0]
            pVertex->u = (fX + 1.0f)/2.0f;
            pVertex->v = (fY + 1.0f)/2.0f;
        }
        //   Add to the x location
        if (i & 1)
        {
            fX += fXOffset;
        }
    }

    // Unlock the vertex buffer
    pVBuffer->Unlock();
    
    return hr;
}

