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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: dds.cpp
//          BVTs for IDirectDrawSurface interface functions.
//
//  Revision History:
//      10/21/96    mattbron    Created.
//      11/23/96    mattbron    Changed preprocessor definition UNDER_NK to
//                              UNDER_CE.
//      12/09/96    mattbron    Added conditional support for clipper tests.
//      02/06/97    lblanco     Enhanced error reporting.
//      02/13/97    mattbron    Modified GetDC(), Blt(), and FastBlt() tests.
//      02/14/97    mattbron    Fixed 2 unreferenced local warnings specific
//                              to CE.
//      02/21/97    mattbron    Added to Lock.
//      06/18/97    lblanco     Removed overlay tests.
//      06/22/97    lblanco     Fixed code style (hey, if I'm going to be
//                              maintaining this code, it's only fair! :)).
//      08/03/99    a-shende    syntax errors: FillSurface.
//      08/03/99    a-shende    Lock() errors on Overlays (uninit variable).
//      08/18/99    a-shende    Added bug #2147 - GetSurfaceDesc
//      10/21/99    a-shende    Checked for Overlays in GetSurfaceDesc
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "dds.h"
#pragma warning(disable : 4995)

////////////////////////////////////////////////////////////////////////////////
// Local macros

#define FOLDDWORD(x)            (((x)&0xFF)^(((x)>>8)&0xFF)^(((x)>>16)&0xFF)^(((x)>>24)&0xFF))
#define FILLDATA(x, y, i, c)    ((BYTE)((x*c + (y*y)%c + i)%256))
#define MAX_BLT_FAILURES        10

#if TEST_IDDS
////////////////////////////////////////////////////////////////////////////////
// Local types

struct DISPLAYMODE
{
    DWORD   m_dwWidth,
            m_dwHeight,
            m_dwDepth,
            m_dwFrequency;
    TCHAR   m_szName[32];
};

struct TWOMODES
{
    DISPLAYMODE m_mode1,
                m_mode2;
    int         m_nValidModes;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

#endif // TEST_IDDS
#if NEED_INITDDS

bool    Help_CreateSurfaces(void);

#endif // NEED_INITDDS

HRESULT WINAPI Help_IDDS_IsLost_EDM_Callback(DDSURFACEDESC *, void *);

#if NEED_INITDDS

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static IDDS             g_idds = { 0 };
static CFinishManager   *g_pfmDDS = NULL;

////////////////////////////////////////////////////////////////////////////////
// InitDirectDrawSurface
//  Creates various DirectDraw surface objects.
//
// Parameters:
//  pIDDS           Pointer to the structure that will hold the interfaces.
//  pfnFinish       Pointer to a FinishXXX function. If this parameter is not
//                  NULL, the system understands that whenever this object is
//                  destroyed, the FinishXXX function must be called as well.
//
// Return value:
//  true if successful, false otherwise.

bool InitDirectDrawSurface(LPIDDS &pIDDS, FNFINISH pfnFinish)
{
    // Clean out the pointer
    pIDDS = NULL;

    // Create the objects, if necessary
    if(!g_idds.m_pDD && !Help_CreateSurfaces())
    {
        FinishDirectDrawSurface();
        return false;
    }

    // Do we have a CFinishManager object?
    if(!g_pfmDDS)
    {
        g_pfmDDS = new CFinishManager;
        if(!g_pfmDDS)
        {
            FinishDirectDrawSurface();
            return false;
        }
    }

    // Add the FinishXXX function to the chain.
    g_pfmDDS->AddFunction(pfnFinish);
    pIDDS = &g_idds;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Help_CreateSurfaces
//  Creates all surfaces used by IDirectDrawSurface tests.
//
// Parameters:
//  None.
//
// Return value:
//  true if successful, false otherwise.

bool Help_CreateSurfaces(void)
{
    HRESULT hr;
    bool    bSuccess = false;
    RECT    rect;
    DWORD   dwWidth,
            dwHeight;

    DDSURFACEDESC   ddsd;

#if QAHOME_OVERLAY
    RECT    rcOverlay;
#endif // QAHOME_OVERLAY

    // Clear out the structure
    memset(&g_idds, 0, sizeof(g_idds));

    // Get the DirectDraw object
    if(!InitDirectDraw(g_idds.m_pDD, FinishDirectDrawSurface))
        return false;

    memset(&g_idds.m_ddcHAL, 0, sizeof(g_idds.m_ddcHAL));
    g_idds.m_ddcHAL.dwSize = sizeof(g_idds.m_ddcHAL);

    hr = g_idds.m_pDD->GetCaps(&g_idds.m_ddcHAL, NULL);
    if(FAILED(hr))
    {
        Debug(LOG_FAIL,TEXT("Failed to retrieve HAL/HEL caps"));
    }
    
    Debug(LOG_COMMENT, TEXT("Running InitDirectDrawSurface..."));

    // Create the primary
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize             = sizeof(ddsd);
    ddsd.dwFlags            = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP;
    ddsd.dwBackBufferCount  = 1;
    g_idds.m_pDDSPrimary = (IDirectDrawSurface *)PRESET;

    if(!(g_idds.m_ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(LOG_COMMENT, TEXT("Flipping unsupported, testing primary only"));
        ddsd.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps &= ~DDSCAPS_FLIP;
        ddsd.dwBackBufferCount  = 0;
    }

    hr = g_idds.m_pDD->CreateSurface(&ddsd, &g_idds.m_pDDSPrimary, NULL);

    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        g_idds.m_pDDSPrimary,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        TEXT("primary")))
    {
        g_idds.m_pDDSPrimary = NULL;
        return false;
    }
    g_idds.m_ddsdPrimary.dwSize = sizeof(DDSURFACEDESC);
    hr = g_idds.m_pDDSPrimary->GetSurfaceDesc(&g_idds.m_ddsdPrimary);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szGetSurfaceDesc,
            hr,
            NULL,
            TEXT("for primary"));
        return false;
    }

    // Save the device size
    dwWidth = g_idds.m_ddsdPrimary.dwWidth;
    dwHeight = g_idds.m_ddsdPrimary.dwHeight;

    // Create an offscreen system memory surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                          DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
    ddsd.dwWidth        = dwWidth;
    ddsd.dwHeight       = dwHeight;
    memcpy(
        &ddsd.ddpfPixelFormat,
        &g_idds.m_ddsdPrimary.ddpfPixelFormat,
        sizeof(DDPIXELFORMAT));
    g_idds.m_pDDSSysMem = (IDirectDrawSurface *)PRESET;
    hr = g_idds.m_pDD->CreateSurface(&ddsd, &g_idds.m_pDDSSysMem, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_COMMENTFAILURES | CRP_ALLOWNONULLOUT,
        g_idds.m_pDDSSysMem,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        TEXT("offscreen in system memory")))
    {
        g_idds.m_pDDSSysMem = NULL;
    }
    else
    {
        g_idds.m_ddsdSysMem.dwSize = sizeof(DDSURFACEDESC);
        hr = g_idds.m_pDDSSysMem->GetSurfaceDesc(&g_idds.m_ddsdSysMem);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr,
                NULL,
                TEXT("for offscreen in system memory"));
            return false;
        }

        if(FillSurface(rand(), g_idds.m_pDDSSysMem) != ITPR_PASS)
            return false;
    }

    // Create an offscreen video memory surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                          DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddsd.dwWidth        = dwWidth;
    ddsd.dwHeight       = dwHeight;
    memcpy(
        &ddsd.ddpfPixelFormat,
        &g_idds.m_ddsdPrimary.ddpfPixelFormat,
        sizeof(DDPIXELFORMAT));
    g_idds.m_pDDSVidMem = (IDirectDrawSurface *)PRESET;
    hr = g_idds.m_pDD->CreateSurface(&ddsd, &g_idds.m_pDDSVidMem, NULL);
    if(FAILED(hr))
    {
        // We can pass hr to CheckReturnedPointer. In that case, the function
        // will make sure that the returned pointer was NULL, and such
        CheckReturnedPointer(
            CRP_RELEASE | CRP_COMMENTFAILURES | CRP_ALLOWNONULLOUT,
            g_idds.m_pDDSVidMem,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            TEXT("full-size offscreen in video memory"),
            NULL,
            hr);

        // Try a smaller surface
        ddsd.dwWidth /= 10;
        ddsd.dwHeight /= 10;
        g_idds.m_pDDSVidMem = (IDirectDrawSurface *)PRESET;
        hr = g_idds.m_pDD->CreateSurface(
            &ddsd,
            &g_idds.m_pDDSVidMem,
            NULL);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_COMMENTFAILURES | CRP_ALLOWNONULLOUT,
            g_idds.m_pDDSVidMem,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            TEXT("small offscreen in video memory")))
        {
            g_idds.m_pDDSVidMem = NULL;
        }
    }
    else if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_COMMENTFAILURES | CRP_ALLOWNONULLOUT,
            g_idds.m_pDDSVidMem,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            TEXT("full-size offscreen in video memory")))
    {
        g_idds.m_pDDSVidMem = NULL;
    }

    if(g_idds.m_pDDSVidMem)
    {
        g_idds.m_ddsdVidMem.dwSize = sizeof(DDSURFACEDESC);
        hr = g_idds.m_pDDSVidMem->GetSurfaceDesc(&g_idds.m_ddsdVidMem);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr,
                NULL,
                TEXT("for offscreen in video memory"));
            return false;
        }

        rect.left   = 0;
        rect.top    = 0;
        rect.right  = g_idds.m_ddsdVidMem.dwWidth;
        rect.bottom = g_idds.m_ddsdVidMem.dwHeight;
        if(FillSurface(rand(), g_idds.m_pDDSVidMem, &rect) != ITPR_PASS)
            return false;
    }

#if QAHOME_OVERLAY
    // a-shende: fix unitialized rectangle
    rcOverlay.left     = 0;
    rcOverlay.top      = 0;

    // Create the first overlay surface
    rcOverlay.right     = rand()%(dwWidth/2) + dwWidth/2;
    rcOverlay.bottom    = rand()%(dwHeight/2) + dwHeight/2;

    // Aligning with the required source alignment size
    if (g_idds.m_ddcHAL.dwAlignSizeSrc) 
    {
        if ((rcOverlay.right % (g_idds.m_ddcHAL.dwAlignSizeSrc) != 0)) 
        {
            rcOverlay.right = ((rcOverlay.right/(g_idds.m_ddcHAL.dwAlignSizeSrc)) + 1) * g_idds.m_ddcHAL.dwAlignSizeSrc;    
        }
        if ((rcOverlay.bottom % (g_idds.m_ddcHAL.dwAlignSizeSrc) !=0 )) 
        {
            rcOverlay.bottom = ((rcOverlay.bottom/(g_idds.m_ddcHAL.dwAlignSizeSrc)) + 1) * g_idds.m_ddcHAL.dwAlignSizeSrc;  
        }   
    }
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize         = sizeof(DDSURFACEDESC);
    ddsd.dwFlags        = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                          DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY;
    ddsd.dwWidth        = rcOverlay.right;
    ddsd.dwHeight       = rcOverlay.bottom;
    memcpy(
        &ddsd.ddpfPixelFormat,
        &g_idds.m_ddsdPrimary.ddpfPixelFormat,
        sizeof(DDPIXELFORMAT));
    g_idds.m_pDDSOverlay1 = (IDirectDrawSurface *)PRESET;
    hr = g_idds.m_pDD->CreateSurface(&ddsd, &g_idds.m_pDDSOverlay1, NULL);
    if(FAILED(hr))
    {
        // There is no hardware support for overlays. Now make sure that
        // the returned pointer was NULL
        CheckReturnedPointer(
            CRP_RELEASE | CRP_COMMENTFAILURES | CRP_ALLOWNONULLOUT,
            g_idds.m_pDDSOverlay1,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            TEXT("overlay #1"),
            NULL,
            hr);
        g_idds.m_pDDSOverlay1 = NULL;
        g_idds.m_bOverlaysSupported = false;
    }
    else if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        g_idds.m_pDDSOverlay1,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        TEXT("overlay #1")))
    {
        g_idds.m_pDDSOverlay1 = NULL;
    }
    else
    {
        g_idds.m_bOverlaysSupported = true;

        g_idds.m_ddsdOverlay1.dwSize = sizeof(DDSURFACEDESC);
        hr = g_idds.m_pDDSOverlay1->GetSurfaceDesc(&g_idds.m_ddsdOverlay1);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr,
                NULL,
                TEXT("for overlay #1"));
            return false;
        }

        if(FillSurface(rand(), g_idds.m_pDDSOverlay1, &rcOverlay) != ITPR_PASS)
            return false;

        // Create the second overlay surface
        rcOverlay.right     = rand()%(dwWidth/2) + dwWidth/2;
        rcOverlay.bottom    = rand()%(dwHeight/2) + dwHeight/2;

        memset(&ddsd, 0, sizeof(DDSURFACEDESC));
        ddsd.dwSize         = sizeof(DDSURFACEDESC);
        ddsd.dwFlags        = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                              DDSD_PIXELFORMAT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY;
        ddsd.dwWidth        = rcOverlay.right;
        ddsd.dwHeight       = rcOverlay.bottom;
        memcpy(
            &ddsd.ddpfPixelFormat,
            &g_idds.m_ddsdPrimary.ddpfPixelFormat,
            sizeof(DDPIXELFORMAT));
        g_idds.m_pDDSOverlay2 = (IDirectDrawSurface *)PRESET;
        hr = g_idds.m_pDD->CreateSurface(&ddsd, &g_idds.m_pDDSOverlay2, NULL);        
        if(FAILED(hr))
        {
            g_idds.m_pDDSOverlay2 = NULL;
        }
        else if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            g_idds.m_pDDSOverlay2,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            TEXT("overlay #2")))
        {
            g_idds.m_pDDSOverlay2 = NULL;
            return false;
        }
        else
        {
            g_idds.m_ddsdOverlay2.dwSize = sizeof(DDSURFACEDESC);
            hr = g_idds.m_pDDSOverlay2->GetSurfaceDesc(&g_idds.m_ddsdOverlay2);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szGetSurfaceDesc,
                    hr,
                    NULL,
                    TEXT("for overlay #2"));
                return false;
            }

            if(FillSurface(rand(), g_idds.m_pDDSOverlay2, &rcOverlay) != ITPR_PASS)
                return false;
        }
    }
#endif // QAHOME_OVERLAY

#if TEST_ZBUFFER
    // Create a Z Buffer surface in system memory
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize             = sizeof(DDSURFACEDESC);
    ddsd.ddsCaps.dwCaps     = DDSCAPS_ZBUFFER | DDSCAPS_SYSTEMMEMORY;
    ddsd.dwFlags            = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
                              DDSD_ZBUFFERBITDEPTH;
    ddsd.dwWidth            = dwWidth/2;
    ddsd.dwHeight           = dwHeight/2;
    ddsd.dwZBufferBitDepth  = 16;
    g_idds.m_pDDSZBuffer = (IDirectDrawSurface *)PRESET;
    hr = g_idds.m_pDD->CreateSurface(&ddsd, &g_idds.m_pDDSZBuffer, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        g_idds.m_pDDSZBuffer,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        TEXT("Z Buffer")))
    {
        g_idds.m_pDDSZBuffer = NULL;
        return false;
    }
    g_idds.m_ddsdZBuffer.dwSize = sizeof(DDSURFACEDESC);
    hr = g_idds.m_pDDSZBuffer->GetSurfaceDesc(&g_idds.m_ddsdZBuffer);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szGetSurfaceDesc,
            hr,
            NULL,
            TEXT("for Z Buffer"));
        return false;
    }
#endif // TEST_ZBUFFER

    Debug(LOG_COMMENT, TEXT("Done with InitDirectDrawSurface"));

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FinishDirectDrawSurface
//  Releases all resources created by InitDirectDrawSurface.
//
// Parameters:
//  None.
//
// Return value:
//  None.

void FinishDirectDrawSurface(void)
{
    // Terminate any dependent objects
    if(g_pfmDDS)
    {
        g_pfmDDS->Finish();
        delete g_pfmDDS;
        g_pfmDDS = NULL;
    }

    if(g_idds.m_pDDSPrimary)
        g_idds.m_pDDSPrimary->Release();

    if(g_idds.m_pDDSSysMem)
        g_idds.m_pDDSSysMem->Release();

    if(g_idds.m_pDDSVidMem)
        g_idds.m_pDDSVidMem->Release();

#if QAHOME_OVERLAY
    if(g_idds.m_pDDSOverlay1)
        g_idds.m_pDDSOverlay1->Release();

    if(g_idds.m_pDDSOverlay2)
        g_idds.m_pDDSOverlay2->Release();
#endif // QAHOME_OVERLAY

#if TEST_ZBUFFER
    if(g_idds.m_pDDSZBuffer)
        g_idds.m_pDDSZBuffer->Release();
#endif // TEST_ZBUFFER

    memset(&g_idds, 0, sizeof(g_idds));
}

////////////////////////////////////////////////////////////////////////////////
// FillSurface
//  Fills a surface with data.
//
// Parameters:
//  dwCookie        A DWORD used to seed the contents of the surface.
//  pDDS            The surface to fill.
//  pRect           The area of the surface to be filled.
//
// Return value:
//  ITPR_PASS if successful, ITPR_ABORT otherwise.

ITPR FillSurface(DWORD dwCookie, IDirectDrawSurface *pDDS, RECT *pRect)
{
    HRESULT hr;
    int     i,
            x,
            y,
            nPitch,
            nPixelBytes;
    BYTE    *pbyte;
    RECT    rect;

    DDSURFACEDESC   ddsd;

    // Get a description of the target surface
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    hr = pDDS->GetSurfaceDesc(&ddsd);
    if(FAILED(hr))
    {
        Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szGetSurfaceDesc, hr);
        return ITPR_ABORT;
    }

    // If a target rectangle wasn't specified, assume the entire surface should
    // be filled
    if(!pRect)
    {
        // Get the size of the surface
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = ddsd.dwWidth;
        rect.bottom = ddsd.dwHeight;
        pRect = &rect;
    }

#ifndef UNDER_CE
    if(ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        // We can't lock the primary surface directly, so we'll have to flip
        // the primary surface, fill it as the back buffer, and then flip back
        IDirectDrawSurface  *pDDSBackBuffer;
        ITPR                nITPR;

        hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szFlip, hr);
            return ITPR_ABORT;
        }

        // Get the back buffer
        memset(&ddsd.ddsCaps, 0, sizeof(DDSCAPS));
        ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
        hr = pDDS->GetAttachedSurface(&ddsd.ddsCaps, &pDDSBackBuffer);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetAttachedSurface,
                hr,
                TEXT("back buffer"));
            return ITPR_ABORT;
        }

        // Fill the back buffer
        nITPR = FillSurface(dwCookie, pDDSBackBuffer, pRect);
        pDDSBackBuffer->Release();

        // Flip back
        hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szFlip,
                hr,
                NULL,
                TEXT("after filling surface"));
            nITPR |= ITPR_ABORT;
        }

        return nITPR;
    }
#endif // UNDER_CE

    // Lock the surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    hr = pDDS->Lock(
        pRect,
        &ddsd,
        DDLOCK_WAITNOTBUSY,
        NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("in FillSurface"));
        return ITPR_ABORT;
    }

    // On PowerVR architectures, there is no Z Buffer. As a result, we don't
    // get a valid pointer here. Thus, if the pointer is NULL, we don't do
    // anything
    if(ddsd.lpSurface)
    {
        // Fix the cookie
        dwCookie = FOLDDWORD(dwCookie);
        if(!dwCookie)
            dwCookie = 256;

        // Write bits to the surface
        nPixelBytes = (int)(ddsd.ddpfPixelFormat.dwRGBBitCount)/8;
        nPitch = ddsd.lPitch - ((pRect->right - pRect->left)*nPixelBytes);
        pbyte = (BYTE*)ddsd.lpSurface;

        for(y = pRect->top; y < pRect->bottom; y++)
        {
            for(x = pRect->left; x < pRect->right; x++)
            {
                for(i = 0; i < nPixelBytes; i++, pbyte++)
                {
                    DWORDWRITEBYTE(pbyte, FILLDATA(x, y, i, dwCookie));
                }
            }
            pbyte += nPitch;
        }
    }

    // Unlock the surface
    hr = pDDS->Unlock(pRect);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szUnlock,
            hr,
            NULL,
            TEXT("in FillSurface"));
        return ITPR_ABORT;
    }

    return ITPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// VerifySurfaceContents
//  Verifies that the given surface contains the same bits that would have been
//  filled in by the FillSurface function.
//
// Parameters:
//  dwCookie        The cookie passed to FillSurface.
//  pDDS            The surface.
//  prcCurrent      The rectangle to be checked.
//  prcFilled       The rectangle that was passed to FillSurface (this will be
//                  usually equal to rcCurrent, unless a Blt operation is being
//                  verified).
//
// Return value:
//  ITPR_PASS if the contents are correct, ITPR_FAIL if they are not, or
//  ITPR_ABORT if the surface contents can't be checked.

ITPR VerifySurfaceContents(
    DWORD               dwCookie,
    IDirectDrawSurface  *pDDS,
    RECT                *prcCurrent,
    RECT                *prcFilled)
{
    HRESULT hr;
    int     i,
            x1,
            y1,
            x2,
            y2,
            nPitch,
            nPixelBytes,
            nFailures = 0;
    BYTE    *pbyte,
            bValue;
    RECT    rect;
    ITPR    nITPR = ITPR_PASS;

    DDSURFACEDESC   ddsd;

    // Get a description of the target surface
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    hr = pDDS->GetSurfaceDesc(&ddsd);
    if(FAILED(hr))
    {
        Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szGetSurfaceDesc, hr);
        return ITPR_ABORT;
    }
    rect.left   = 0;
    rect.top    = 0;
    rect.right  = ddsd.dwWidth;
    rect.bottom = ddsd.dwHeight;

    // If a target rectangle wasn't specified, assume the entire surface should
    // be verified
    if(!prcCurrent)
        prcCurrent = &rect;

    if(!prcFilled)
        prcFilled = &rect;

#ifndef UNDER_CE
    if(ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        // We can't lock the primary surface directly, so we'll have to flip
        // the primary surface, check the back buffer, and then flip back
        IDirectDrawSurface  *pDDSBackBuffer;

        hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szFlip, hr);
            return ITPR_ABORT;
        }

        // Get the back buffer
        memset(&ddsd.ddsCaps, 0, sizeof(DDSCAPS));
        ddsd.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
        hr = pDDS->GetAttachedSurface(&ddsd.ddsCaps, &pDDSBackBuffer);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetAttachedSurface,
                hr,
                TEXT("back buffer"));
            return ITPR_ABORT;
        }

        // Check the back buffer
        nITPR = VerifySurfaceContents(
            dwCookie,
            pDDSBackBuffer,
            prcCurrent,
            prcFilled);
        pDDSBackBuffer->Release();

        // Flip back
        hr = pDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szFlip,
                hr,
                NULL,
                TEXT("after filling surface"));
            nITPR |= ITPR_ABORT;
        }

        return nITPR;
    }
#endif // UNDER_CE

    // Lock the surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    hr = pDDS->Lock(
        prcCurrent,
        &ddsd,
        DDLOCK_WAITNOTBUSY,
        NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("in VerifySurfaceContents"));
        return ITPR_ABORT;
    }

    // On PowerVR architectures, there is no Z Buffer. As a result, we don't
    // get a valid pointer here. Thus, if the pointer is NULL, we don't do
    // anything
    if(ddsd.lpSurface)
    {
        // Fix the cookie
        dwCookie = FOLDDWORD(dwCookie);
        if(!dwCookie)
            dwCookie = 256;

        // Check bits in the surface
        nPixelBytes = (int)(ddsd.ddpfPixelFormat.dwRGBBitCount)/8;
        nPitch = ddsd.lPitch - ((prcCurrent->right - prcCurrent->left)*
                 nPixelBytes);
        pbyte = (BYTE*)ddsd.lpSurface;

        for(y1 = prcCurrent->top, y2 = prcFilled->top;
            y1 < prcCurrent->bottom;
            y1++, y2++)
        {
            for(x1 = prcCurrent->left, x2 = prcFilled->left;
                x1 < prcCurrent->right;
                x1++, x2++)
            {
                for(i = 0; i < nPixelBytes; i++, pbyte++)
                {
                    bValue = DWORDREADBYTE(pbyte);
                    if(bValue != FILLDATA(x2, y2, i, dwCookie))
                    {
                        Debug(
                            LOG_FAIL,
                            TEXT("    VerifySurfaceContents: Pixel %d, %d")
                            TEXT(" (byte %d) does not match (expected 0x%02X,")
                            TEXT(" was 0x%02X)"),
                            x1,
                            y1,
                            i,
                            FILLDATA(x2, y2, i, dwCookie),
                            bValue);
                        nITPR |= ITPR_FAIL;
                        if(++nFailures > MAX_BLT_FAILURES)
                        {
                            // Force an exit from all loops
                            i = 1000000;
                            x1 = 1000000;
                            y1 = 1000000;
                        }
                    }
                }
            }
            pbyte += nPitch;
        }
    }

    // Unlock the surface
    hr = pDDS->Unlock(prcCurrent);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szUnlock,
            hr,
            NULL,
            TEXT("in VerifySurfaceContents"));
        nITPR |= ITPR_ABORT;
    }

    return nITPR;
}
#endif // NEED_INITDDS

#if TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_AddRef_Release
//  Tests the IDirectDrawSurface::AddRef and IDirectDrawSurface::Release
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_AddRef_Release(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    INT         nResult;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS_AddRef_Release, lpFTE);
        if(nResult == TPR_PASS)
        {
            Report(
                RESULT_SUCCESS,
                c_szIDirectDrawSurface,
                TEXT("AddRef and Release"));
        }
    }
    else if(pDDSTI->m_pDDS && !Test_AddRefRelease(pDDSTI->m_pDDS, c_szIDirectDrawSurface))
    {
        nResult = ITPR_FAIL;
    }
    else
    {
        nResult = ITPR_PASS;
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetCaps
//  Tests the IDirectDrawSurface::GetCaps method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetCaps(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    DDSCAPS     ddscaps;
    HRESULT     hr;
    bool        bSuccess = true;
    INT         nResult = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS_GetCaps, lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetCaps);
    }
    else if(pDDSTI->m_pDDS)
    {
        hr = pDDSTI->m_pDDS->GetCaps(&ddscaps);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetCaps,
                hr,
                NULL,
                pDDSTI->m_pszName);
            nResult |= TPR_FAIL;
        }
        else if(ddscaps.dwCaps != pDDSTI->m_pddsd->ddsCaps.dwCaps)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s was expected to return caps 0x%08x")
                TEXT(" %s (returned 0x%08x)"),
                c_szIDirectDrawSurface,
                c_szGetCaps,
                pDDSTI->m_pddsd->ddsCaps.dwCaps,
                pDDSTI->m_pszName,
                ddscaps.dwCaps);
            nResult |= TPR_FAIL;
        }
    }

    return nResult;
}

#if TEST_IDDC
////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Get_SetClipper
//  Tests the IDirectDrawSurface::GetClipper and IDirectDrawSurface::SetClipper
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_Get_SetClipper(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    HRESULT     hr = S_OK;
    IDDS        *pIDDS;
    int         nResult = ITPR_PASS;
    DDSTESTINFO *pDDSTI;
    ULONG       ulRefCount1,
                ulRefCount2,
                ulRefCount3;

    IDirectDrawClipper  *pDDC,
                        *pDDCReturned;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    if(!InitDirectDrawClipper(pDDC))
        return TPRFromITPR(g_tprReturnVal);

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(
            DDSI_TESTBACKBUFFER,
            Test_IDDS_Get_SetClipper,
            lpFTE,
            0);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetClipper);
    }
    else if(pDDSTI->m_pDDS)
    {      
#if QAHOME_INVALIDPARAMS
#define nITPR nResult
        INVALID_CALL(
            pDDSTI->m_pDDS->GetClipper(NULL),
            c_szIDirectDrawSurface,
            c_szGetClipper,
            TEXT("NULL"));
#undef nITPR
#endif // QAHOME_INVALIDPARAMS

        // Remove any attached clippers
        hr = pDDSTI->m_pDDS->SetClipper(NULL);

#ifdef KATANA
        if(FAILED(hr))
#else // not KATANA
        // a-shende(07.28.1999): watch for valid error
        if((FAILED(hr)) && (DDERR_NOCLIPPERATTACHED != hr))
#endif  // not KATANA
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szSetClipper,
                hr,
                TEXT("NULL"),
                TEXT("the first time"));
            nResult |= ITPR_FAIL;
        }

        // There should be no clipper attached now
        pDDCReturned = (IDirectDrawClipper *)PRESET;
        hr = pDDSTI->m_pDDS->GetClipper(&pDDCReturned);
        if(!CheckReturnedPointer(
            CRP_RELEASE | CRP_ALLOWNONULLOUT,
            pDDCReturned,
            c_szIDirectDrawSurface,
            c_szGetClipper,
            hr,
            NULL,
            TEXT("when no clipper was attached"),
            DDERR_NOCLIPPERATTACHED))
        {
            nResult |= ITPR_FAIL;
        }

        // Check the reference count of our clipper
        ulRefCount1 = pDDC->AddRef();
        pDDC->Release();

        // Associate our clipper with the surface
        hr = pDDSTI->m_pDDS->SetClipper(pDDC);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szSetClipper,
                hr);
            nResult |= ITPR_FAIL;
        }
        else
        {
            // Verify that the surface is now holding on to the clipper object
            PREFAST_ASSUME(pDDC);
            ulRefCount2 = pDDC->AddRef();
            pDDC->Release();

            if(ulRefCount2 <= ulRefCount1)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s was expected to increment")
                    TEXT(" the reference count of the clipper object")
                    TEXT(" (was %d, is now %d)"),
                    c_szIDirectDrawSurface,
                    c_szSetClipper,
                    ulRefCount1,
                    ulRefCount2);
                nResult |= ITPR_FAIL;
            }

            // Get the clipper back
            pDDCReturned = (IDirectDrawClipper *)PRESET;
            hr = pDDSTI->m_pDDS->GetClipper(&pDDCReturned);
            if(!CheckReturnedPointer(
                CRP_RELEASEONFAILURE | CRP_ALLOWNONULLOUT,
                pDDCReturned,
                c_szIDirectDrawSurface,
                c_szGetClipper,
                hr,
                NULL,
                TEXT("when a clipper was attached")))
            {
                nResult |= ITPR_FAIL;
            }
            else if(pDDCReturned != pDDC)
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s::%s was expected to return 0x%08x")
                    TEXT(" (returned 0x%08X)"),
                    c_szIDirectDrawSurface,
                    c_szGetClipper,
                    pDDC,
                    pDDCReturned);
                pDDCReturned->Release();
                nResult |= ITPR_FAIL;
            }
            else
            {
                // Check that the reference count was handled properly
                ulRefCount3 = pDDC->AddRef();
                pDDC->Release();

                if(ulRefCount3 <= ulRefCount2)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s was expected to increment")
                        TEXT(" the reference count of the clipper object")
                        TEXT(" (was %d, is now %d)"),
                        c_szIDirectDrawSurface,
                        c_szGetClipper,
                        ulRefCount2,
                        ulRefCount3);
                    nResult |= ITPR_FAIL;
                }

                // Release the clipper and check that the reference count goes
                // back to its previous value
                pDDC->Release();

                ulRefCount3 = pDDC->AddRef();
                pDDC->Release();

                if(ulRefCount3 != ulRefCount2)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s was expected to return")
                        TEXT(" the reference count of the clipper object to")
                        TEXT(" what it was before the call to %s::%s")
                        TEXT(" (was %s, is now %d)"),
                        c_szIDirectDrawClipper,
                        c_szRelease,
                        c_szIDirectDrawSurface,
                        c_szGetClipper,
                        ulRefCount2,
                        ulRefCount3);
                    nResult |= ITPR_FAIL;
                }
            }

            // Set the clipper back to NULL
            hr = pDDSTI->m_pDDS->SetClipper(NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szSetClipper,
                    hr,
                    TEXT("NULL"),
                    TEXT("after a clipper was set"));
                nResult |= ITPR_FAIL;
            }
            else
            {
                // The reference count should be back to what it was at the
                // beginning of the test
                ulRefCount2 = pDDC->AddRef();
                pDDC->Release();

                if(ulRefCount2 != ulRefCount1)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("%s::%s was expected to return")
                        TEXT(" the reference count of the clipper object to")
                        TEXT(" what it was before the call to %s::%s")
                        TEXT(" (was %s, is now %d)"),
                        c_szIDirectDrawClipper,
                        c_szRelease,
                        c_szIDirectDrawSurface,
                        c_szSetClipper,
                        ulRefCount1,
                        ulRefCount2);
                    nResult |= ITPR_FAIL;
                }
            }
        }
    }

    return nResult;
}

#endif // TEST_IDDC

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetPixelFormat
//  Tests the IDirectDrawSurface::GetPixelFormat method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetPixelFormat(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO     *pDDSTI;
    DDPIXELFORMAT   ddpf;
    HRESULT         hr;
    INT             nResult = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS_GetPixelFormat, lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetPixelFormat);
    }
    else if(pDDSTI->m_pDDS)
    {
        memset(&ddpf, 0, sizeof(ddpf));
        ddpf.dwSize = sizeof(ddpf);
        hr = pDDSTI->m_pDDS->GetPixelFormat(&ddpf);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetPixelFormat,
                hr);
            nResult |= ITPR_FAIL;
        }
        else if(memcmp(&pDDSTI->m_pddsd->ddpfPixelFormat, &ddpf, sizeof(ddpf)))
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s returned an incorrect pixel format")
                TEXT(" %s"),
                c_szIDirectDrawSurface,
                c_szGetPixelFormat,
                pDDSTI->m_pszName);
            nResult |= ITPR_FAIL;
        }
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetSurfaceDesc
//  Tests the IDirectDrawSurface::GetSurfaceDesc method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetSurfaceDesc(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    HRESULT     hr;
    INT         nResult = ITPR_PASS;
    DDCOLORKEY  ddck;
    UINT        nIndex;
    TCHAR       szMessage[64];

    IDirectDrawSurface  *pDDS;
    DDSURFACEDESC       ddsdInitial,
                        ddsd,
                        ddsdTemp;

     DDCAPS      ddDrvCaps;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS_GetSurfaceDesc, lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetSurfaceDesc);
    }
    else if(pDDSTI->m_pDDS)
    {
        // Get the current surface description
        memset(&ddsdInitial, 0, sizeof(ddsdInitial));
        ddsdInitial.dwSize = sizeof(ddsdInitial);
        hr = pDDSTI->m_pDDS->GetSurfaceDesc(&ddsdInitial);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr);
            nResult |= ITPR_FAIL;
        }

        // Make sure that all fields are filled in. The macro below simplifies
        // our work by checking each parameter in sequence
#define CHECK_PARAMETER_RAID(x, r) \
        if(!ddsdInitial.x) \
        { \
            Debug( \
                LOG_FAIL, \
                FAILURE_HEADER TEXT("%s%s::%s didn't fill in the %s field") \
                TEXT(" of the DDSURFACEDESC structure"), \
                r, \
                c_szIDirectDrawSurface, \
                c_szGetSurfaceDesc, \
                TEXT(#x)); \
            nResult |= ITPR_FAIL; \
        }
#define CHECK_PARAMETER(x)  CHECK_PARAMETER_RAID(x, TEXT(""))

        CHECK_PARAMETER(dwFlags)
        CHECK_PARAMETER(dwHeight)
        CHECK_PARAMETER(dwWidth)
        CHECK_PARAMETER(lPitch)
        CHECK_PARAMETER(ddsCaps.dwCaps)
#ifdef KATANA
#define BUGID   TEXT("(Keep #5728) ")
#else // KATANA
#define BUGID   TEXT("")
#endif // KATANA
        CHECK_PARAMETER_RAID(ddpfPixelFormat.dwFlags, BUGID)
        CHECK_PARAMETER_RAID(ddpfPixelFormat.dwRGBBitCount, BUGID)
        CHECK_PARAMETER_RAID(ddpfPixelFormat.dwRBitMask, BUGID)
        CHECK_PARAMETER_RAID(ddpfPixelFormat.dwGBitMask, BUGID)
        CHECK_PARAMETER_RAID(ddpfPixelFormat.dwBBitMask, BUGID)

        // Done with the macros
#undef CHECK_PARAMETER
#undef CHECK_PARAMETER_RAID
#undef BUGID

        // Create and add a back buffer to the current surface (unless it's
        // a Z buffer or the primary). The back buffer must be in the same
        // place as the surface it will be attached to.
        if(!(ddsdInitial.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
        {
            memset(&ddsd, 0, sizeof(ddsd));
            ddsd.dwSize         = sizeof(ddsd);
            ddsd.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
            ddsd.ddsCaps.dwCaps = (ddsdInitial.ddsCaps.dwCaps &
                                  (DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY));
            ddsd.dwWidth        = ddsdInitial.dwWidth;
            ddsd.dwHeight       = ddsdInitial.dwHeight;
            pDDS = (IDirectDrawSurface *)PRESET;
            hr = pDDSTI->m_pDD->CreateSurface(&ddsd, &pDDS, NULL);


            // If we can't create the surface we can't proceed with this test,
            // and we'll output a failure message. However, we won't fail the
            // test
            if(FAILED(hr))
            {
#ifdef KATANA
#define BUGID   0
#else // not KATANA
#define BUGID   2147
#endif // not KATANA

                // If we've hit a low memory condition, and we can get the available video memory, and the
                // available video memory is less than what we would need, then do nothing, otherwise log a failure
                DWORD dwFree = 0;
                IDirectDraw    *pDD2 = NULL;
                
                if(DDERR_OUTOFVIDEOMEMORY == hr && 
                   SUCCEEDED(pDDSTI->m_pDD->GetAvailableVidMem(&(ddsd.ddsCaps), NULL, &dwFree)) &&
                   dwFree < (pDDSTI->m_pddsd->lPitch * pDDSTI->m_pddsd->dwHeight))
                {
                    Debug( LOG_COMMENT, TEXT("Out of video memory creating surface, skipping this surface."));
                }
                else
                {
                    Report(
                        hr == DDERR_OUTOFVIDEOMEMORY ?
                            RESULT_FAILURE : RESULT_UNEXPECTED,
                        c_szIDirectDraw,
                        c_szCreateSurface,
                        hr,
                        NULL,
                        NULL,
                        hr,
                        RAID_HPURAID,
                        BUGID);
#undef BUGID
                    nResult |= ITPR_ABORT;
                }
            }
            else
            {
                // Get the description of the surface we just created
                memset(&ddsdTemp, 0, sizeof(ddsdTemp));
                ddsdTemp.dwSize = sizeof(ddsdTemp);
                hr = pDDS->GetSurfaceDesc(&ddsdTemp);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szGetSurfaceDesc,
                        hr,
                        NULL,
                        TEXT("for surface about to be attached"));
                    nResult |= ITPR_FAIL;

                    // The following will let us know later that this call
                    // failed
                    ddsdTemp.dwSize = 0;
                }

                // Attach this surface to the current surface
                // BUGBUG: Should figure out a better way to get this working
                //hr = pDDSTI->m_pDDS->AddAttachedSurface(pDDS);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szAddAttachedSurface,
                        hr);
                    nResult |= ITPR_ABORT;
                }
                else
                {
                    // Get the new description of the surface
                    memset(&ddsd, 0, sizeof(ddsd));
                    ddsd.dwSize = sizeof(ddsd);
                    hr = pDDSTI->m_pDDS->GetSurfaceDesc(&ddsd);
                    if(FAILED(hr))
                    {
                        Report(
                            RESULT_FAILURE,
                            c_szIDirectDrawSurface,
                            c_szGetSurfaceDesc,
                            hr,
                            NULL,
                            TEXT("after attaching a surface"));
                        nResult |= ITPR_FAIL;
                    }
                    else
                    {
                        // The caps may have changed; everything else must be
                        // the same
                        ddsd.ddsCaps.dwCaps = ddsdInitial.ddsCaps.dwCaps;

                        // If this surface was the primary, the number of back
                        // buffers will also have increased
                        if(ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
                        {
                            // BUGBUG: We never attached the surface
                            // ddsd.dwBackBufferCount--;
                        }

                        if(memcmp(&ddsdInitial, &ddsd, sizeof(DDSURFACEDESC)))
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("Parts of the surface")
                                TEXT(" description other than the caps were")
                                TEXT(" changed"));
                            nResult |= ITPR_FAIL;
                        }
                    }

                    // Get the new description of the attached surface
                    memset(&ddsd, 0, sizeof(ddsd));
                    ddsd.dwSize = sizeof(ddsd);
                    hr = pDDS->GetSurfaceDesc(&ddsd);
                    if(FAILED(hr))
                    {
                        Report(
                            RESULT_FAILURE,
                            c_szIDirectDrawSurface,
                            c_szGetSurfaceDesc,
                            hr,
                            NULL,
                            TEXT("for attached surface"));
                        nResult |= ITPR_FAIL;
                    }
                    else if(ddsdTemp.dwSize == sizeof(ddsdTemp))
                    {
                        // The attached surface is now a back buffer and part
                        // of a flipping chain

                        // a-shende: unless its an overlay
                        // louiscl: surfaces cannot be attached any more.
                        /*
                        if(! (ddsdInitial.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                        {
                            ddsdTemp.ddsCaps.dwCaps |= DDSCAPS_FLIP |
                                                       DDSCAPS_BACKBUFFER;
                        }*/

                        if(memcmp(&ddsdTemp, &ddsd, sizeof(DDSURFACEDESC)))
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("The new description of")
                                TEXT(" the attached surface differs from the")
                                TEXT(" old description"));
                            nResult |= ITPR_FAIL;
                        }
                    }

                    // Remove the attached surface
                    // louiscl: Surfaces cannot be attached or deleted any more.
                    // pDDSTI->m_pDDS->DeleteAttachedSurface(0, pDDS);
                }

                // Release the temporary surface
                pDDS->Release();
            }

            // a-shende(07.23.1999): GetCaps to verify HW support.
            memset(&ddDrvCaps, 0x0, sizeof(DDCAPS));
            ddDrvCaps.dwSize = sizeof(DDCAPS);
            if(pDDSTI->m_pDD)
            {
                hr = pDDSTI->m_pDD->GetCaps(&ddDrvCaps, NULL); 
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDraw,
                        c_szGetCaps,
                        hr,
                        TEXT("GetCaps"));
                    nResult |= ITPR_ABORT;
                }
            }


            // Next, change color keys
            for(nIndex = 0; nIndex < 4; nIndex++)
            {
                const TCHAR       *pszName=NULL,
                            *pszFlagsName=NULL;
                DWORD       dwCKFlags=0,
                            dwDDSDFlags=0;
                DDCOLORKEY  *pddckNew=NULL,
                            *pddckInitial=NULL;

                // a-shende(07.23.1999): tracking suppored HW features
                DWORD        dwHWCaps = 0;

                switch(nIndex)
                {
                case 0:
                    dwCKFlags = DDCKEY_SRCBLT;
                    dwDDSDFlags = DDSD_CKSRCBLT;
                    pszFlagsName = TEXT("DDCKEY_SRCBLT");
                    pszName = TEXT("source Blt color key");
                    pddckNew = &ddsd.ddckCKSrcBlt;
                    pddckInitial = &ddsdInitial.ddckCKSrcBlt;

                    dwHWCaps = DDCKEYCAPS_SRCBLT;
                    break;
                case 1:
                    dwCKFlags = DDCKEY_DESTBLT;
                    dwDDSDFlags = DDSD_CKDESTBLT;
                    pszFlagsName = TEXT("DDCKEY_DESTBLT");
                    pszName = TEXT("destination Blt color key");
                    pddckNew = &ddsd.ddckCKDestBlt;
                    pddckInitial = &ddsdInitial.ddckCKDestBlt;

                    dwHWCaps = DDCKEYCAPS_DESTBLT;
                    break;
                case 2:
                    dwCKFlags = DDCKEY_SRCBLT | DDCKEY_COLORSPACE;
                    dwDDSDFlags = DDSD_CKSRCBLT;
                    pszFlagsName = TEXT("DDCKEY_SRCBLT | DDCKEY_COLORSPACE");
                    pszName = TEXT("source Blt color key space");
                    pddckNew = &ddsd.ddckCKSrcBlt;
                    pddckInitial = &ddsdInitial.ddckCKSrcBlt;

                    dwHWCaps = DDCKEYCAPS_SRCBLT | DDCKEYCAPS_SRCBLTCLRSPACE;
                    break;
                case 3:
                    dwCKFlags = DDCKEY_DESTBLT | DDCKEY_COLORSPACE;
                    dwDDSDFlags = DDSD_CKDESTBLT;
                    pszFlagsName = TEXT("DDCKEY_DESTBLT | DDCKEY_COLORSPACE");
                    pszName = TEXT("destination Blt color key space");
                    pddckNew = &ddsd.ddckCKDestBlt;
                    pddckInitial = &ddsdInitial.ddckCKDestBlt;

                    dwHWCaps = DDCKEYCAPS_DESTBLT | DDCKEYCAPS_DESTBLTCLRSPACE;
                    break;
                }

                // Verify that color key information is returned correctly
                ddck.dwColorSpaceLowValue = PRESET + rand();
                ddck.dwColorSpaceHighValue = PRESET + rand();

                hr = pDDSTI->m_pDDS->SetColorKey(dwCKFlags, &ddck);

#ifdef KATANA
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szSetColorKey,
                        hr,
                        pszFlagsName);

                    nResult |= ITPR_ABORT;
                }
#else // not KATANA

                // a-shende(07.23.1999): return value depends on the flags
                // a-shende(10.12.1999): simplifying this check
                if(DDERR_NOCOLORKEYHW == hr || DDERR_INVALIDCAPS == hr)
                {
                    // Function not supported, do nothing
                    ;
                }
                else if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szSetColorKey,
                        hr,
                        pszFlagsName,
                        NULL,
                        S_OK,
                        RAID_HPURAID,
                        2233);

                    // a-shende(07.26.1999): don't abort upon failure
                    // nResult |= ITPR_ABORT;
                    nResult |= ITPR_FAIL;
                }
#endif // not  KATANA
                else
                {
                    // If we didn't specify DDCKEY_COLORSPACE, then only the
                    // low value will be used
                    if(!(dwCKFlags&DDCKEY_COLORSPACE))
                    {
                        ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
                    }

                    // Get the new description
                    memset(&ddsd, 0, sizeof(ddsd));
                    ddsd.dwSize = sizeof(ddsd);
                    hr = pDDSTI->m_pDDS->GetSurfaceDesc(&ddsd);
                    if(FAILED(hr))
                    {
                        wsprintf(
                            szMessage,
                            TEXT("after setting the %s"),
                            pszName);
                        Report(
                            RESULT_FAILURE,
                            c_szIDirectDrawSurface,
                            c_szGetSurfaceDesc,
                            hr,
                            NULL,
                            szMessage);
                        nResult |= ITPR_FAIL;
                    }
                    else
                    {
                        // The DDSD_CKSRCBLT value must be on
                        if(!(ddsd.dwFlags&dwDDSDFlags))
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("%s::%s was expected")
                                TEXT(" to set a flag in ddsd.dwFlags after")
                                TEXT(" the %s was set"),
                                c_szIDirectDrawSurface,
                                c_szGetSurfaceDesc,
                                pszName);
                            nResult |= ITPR_FAIL;
                        }

                        // The new description must include the new color key
                        if(memcmp(pddckNew, &ddck, sizeof(ddck)))
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("%s::%s was expected to")
                                TEXT(" return a %s of (low,")
                                TEXT(" high) = (0x%08x, 0x%08x) (returned")
                                TEXT(" (0x%08x, 0x%08x))"),
                                c_szIDirectDrawSurface,
                                c_szGetSurfaceDesc,
                                pszName,
                                ddck.dwColorSpaceLowValue,
                                ddck.dwColorSpaceHighValue,
                                pddckNew->dwColorSpaceLowValue,
                                pddckNew->dwColorSpaceHighValue);
                            nResult |= ITPR_FAIL;
                        }

                        // Everything else must be the same
                        ddsdInitial.dwFlags |= dwDDSDFlags;
                        ddsd.dwFlags |= dwDDSDFlags;
                        *pddckInitial = *pddckNew;
                        if(memcmp(&ddsd, &ddsdInitial, sizeof(ddsd)))
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("The surface description")
                                TEXT(" was modified beyond just the")
                                TEXT(" %s"),
                                pszName);
                            nResult |= ITPR_FAIL;
                        }
                    }
                }
            }
        }
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_IsLost
//  Tests the IDirectDrawSurface::IsLost method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_IsLost(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    //***************************************************************************
    DDSTESTINFO *pDDSTI;
    INT          nResult = ITPR_PASS;
    HRESULT     hr;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(
            DDSI_TESTBACKBUFFER,
            Test_IDDS_IsLost,
            lpFTE,
            NULL);

        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szIsLost);
    }
    else if(pDDSTI->m_pDDS)
    {       
        // Assume the method is a NOP. Make sure it doesn't crash
        __try
        {
            hr = pDDSTI->m_pDDS->IsLost();
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szIsLost,
                    hr,
                    NULL,
                    pDDSTI->m_pszName);
                nResult |= ITPR_FAIL;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            Report(
                RESULT_EXCEPTION,
                c_szIDirectDrawSurface,
                c_szIsLost,
                0,
                NULL,
                pDDSTI->m_pszName);
            nResult |= ITPR_FAIL;
        }
    }

    return nResult;

    /***************************************************************************
    //  The test below assumes that display modes can be changed while video
    //  memory surfaces exist. Currently, that's not the case on the Katana, so
    //  this code is commented out. If this behavior is later allowed,
    //  uncomment this code to enable this version of the test

    HRESULT     hr;
    ITPR        nITPR = ITPR_PASS;
    TWOMODES    tm;
    bool        bFirstModeSet = false;

    IDirectDraw        *pDD2;
    IDirectDrawSurface  *pDDSPrimary,
                        *pDDSBackBuffer;
    DDSURFACEDESC       ddsd;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    // We should release any existing surfaces before running this test
    FinishDirectDrawSurface();

    if(!InitDirectDraw(pDD2))
        return TPR_ABORT;

    // On the Katana, the only way a surface can be lost is if the display mode
    // changes. We will set the display mode to some random supported mode,
    // change it to another mode with a different resolution and/or depth, and
    // then test the method. To accomplish this, we will enumerate display
    // modes and select two different ones.
    memset(&tm, 0, sizeof(tm));
    hr = pDD2->EnumDisplayModes(
        0,
        NULL,
        (void *)&tm,
        Help_IDDS_IsLost_EDM_Callback);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDraw,
            c_szEnumDisplayModes,
            hr);
        nITPR |= ITPR_FAIL;
    }
    else
    {
        // Switch to the first display mode
        if(tm.m_nValidModes == 2)
        {
            hr = pDD2->SetDisplayMode(
                tm.m_mode1.m_dwWidth,
                tm.m_mode1.m_dwHeight,
                tm.m_mode1.m_dwDepth,
                tm.m_mode1.m_dwFrequency,
                0);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDraw,
                    c_szSetDisplayMode,
                    hr,
                    tm.m_mode1.m_szName);
                nITPR |= ITPR_ABORT;
            }
            else
            {
                bFirstModeSet = true;
            }
        }

        // Create the primary with one back buffer
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize             = sizeof(ddsd);
        ddsd.dwFlags            = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE |
                                  DDSCAPS_FLIP;
        ddsd.dwBackBufferCount  = 1;
        pDDSPrimary = (IDirectDrawSurface *)PRESET;
        hr = pDD2->CreateSurface(&ddsd, &pDDSPrimary, NULL);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            pDDSPrimary,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            TEXT("primary")))
        {
            nITPR |= ITPR_ABORT;
        }
        else
        {
            // Get a pointer to the back buffer
            pDDSBackBuffer = (IDirectDrawSurface *)PRESET;
            hr = pDDSPrimary->EnumAttachedSurfaces(
                &pDDSBackBuffer, 
                Help_GetBackBuffer_Callback);
            if(!CheckReturnedPointer(
                CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                pDDSBackBuffer,
                c_szIDirectDrawSurface,
                c_szGetAttachedSurface,
                hr,
                TEXT("back buffer")))
            {
                nITPR |= ITPR_ABORT;
            }
            else
            {
                // At this point we just created the surfaces, so they should
                // certainly not be lost. Try the primary first
                hr = pDDSPrimary->IsLost();
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szIsLost,
                        hr,
                        NULL,
                        TEXT("for primary before call to SetDisplayMode"));
                    nITPR |= ITPR_FAIL;
                }

                // Then do the back buffer
                hr = pDDSBackBuffer->IsLost();
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szIsLost,
                        hr,
                        NULL,
                        TEXT("for back buffer before call to SetDisplayMode"));
                    nITPR |= ITPR_FAIL;
                }

                // At this point, there isn't much else to be done unless we
                // have at least two different display modes to play with
                if(bFirstModeSet)
                {
                    // Switch to the second display mode
                    hr = pDD2->SetDisplayMode(
                        tm.m_mode2.m_dwWidth,
                        tm.m_mode2.m_dwHeight,
                        tm.m_mode2.m_dwDepth,
                        tm.m_mode2.m_dwFrequency,
                        0);
                    if(FAILED(hr))
                    {
                        Report(
                            RESULT_ABORT,
                            c_szIDirectDraw,
                            c_szSetDisplayMode,
                            hr,
                            tm.m_mode2.m_szName);
                        nITPR |= ITPR_ABORT;
                    }
                    else
                    {
                        // Both surfaces MUST be lost at this point. Try the
                        // primary first:
                        hr = pDDSPrimary->IsLost();
                        if(hr != DDERR_SURFACELOST)
                        {
                            Report(
                                RESULT_UNEXPECTED,
                                c_szIDirectDrawSurface,
                                c_szIsLost,
                                hr,
                                NULL,
                                TEXT("for primary after call to")
                                TEXT(" SetDisplayMode"),
                                DDERR_SURFACELOST);
                            nITPR |= ITPR_FAIL;
                        }

                        // Then, the back buffer:
                        hr = pDDSBackBuffer->IsLost();
                        if(hr != DDERR_SURFACELOST)
                        {
                            Report(
                                RESULT_UNEXPECTED,
                                c_szIDirectDrawSurface,
                                c_szIsLost,
                                hr,
                                NULL,
                                TEXT("for back buffer after call to")
                                TEXT(" SetDisplayMode"),
                                DDERR_SURFACELOST);
                            nITPR |= ITPR_FAIL;
                        }

#if QAHOME_INVALIDPARAMS
                        // The back buffer can't be restored (it is restored
                        // implicitly when the primary is restored)
                        INVALID_CALL_E(
                            pDDSBackBuffer->Restore(),
                            c_szIDirectDrawSurface,
                            c_szRestore,
                            TEXT("for back buffer"),
                            DDERR_IMPLICITLYCREATED);
#endif // QAHOME_INVALIDPARAMS

                        // Restore the primary
                        hr = pDDSPrimary->Restore();
                        if(FAILED(hr))
                        {
                            Report(
                                RESULT_ABORT,
                                c_szIDirectDrawSurface,
                                c_szRestore,
                                hr,
                                NULL,
                                TEXT("for primary"));
                            nITPR |= ITPR_ABORT;
                        }
                        else
                        {
                            // Neither the primary nor the back buffer should
                            // be lost at this point
                            hr = pDDSPrimary->IsLost();
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDrawSurface,
                                    c_szIsLost,
                                    hr,
                                    NULL,
                                    TEXT("for primary after call to")
                                    TEXT(" Restore"));
                                nITPR |= ITPR_FAIL;
                            }

                            // Now, the back buffer
                            hr = pDDSBackBuffer->IsLost();
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDrawSurface,
                                    c_szIsLost,
                                    hr,
                                    NULL,
                                    TEXT("for back buffer after call to")
                                    TEXT(" Restore"));
                                nITPR |= ITPR_FAIL;
                            }
                        }
                    }
                }

                pDDSBackBuffer->Release();
            }

            pDDSPrimary->Release();
        }
    }

    return TPRFromITPR(nITPR);
    */

}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_QueryInterface
//  Tests the IDirectDrawSurface::QueryInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_QueryInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    INT         nResult = ITPR_PASS;

    static INDEX_IID iidValid[] = {
        INDEX_IID_IDirectDrawSurface,
        INDEX_IID_IDirectDrawColorControl
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(
            DDSI_TESTBACKBUFFER,
            Test_IDDS_QueryInterface,
            lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szQueryInterface);
    }
    else if(pDDSTI->m_pDDS)
    {
        BYTE bCnt = countof(iidValid);
        SPECIAL spcFlag = SPECIAL_IDDS_QI;

#ifndef KATANA
        // ColorControl support must be checked 
        DDSCAPS ddscp;
        DDCAPS ddcp;
        HRESULT hr;

        // Don't flag for KATANA errors
        spcFlag = SPECIAL_NONE;

        memset(&ddscp, 0, sizeof(ddscp));
        memset(&ddcp, 0, sizeof(ddcp));
        ddcp.dwSize = sizeof(ddcp);

        // Get surface caps for ColorControlSupport check
        hr = pDDSTI->m_pDDS->GetCaps(&ddscp);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetCaps,
                hr,
                NULL,
                pDDSTI->m_pszName);
            return(ITPR_FAIL);
        }

        // Get driver caps for check
        hr = pDDSTI->m_pDD->GetCaps(&ddcp, NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szGetCaps,
                hr,
                NULL,
                pDDSTI->m_pszName);
            return(ITPR_FAIL);
        }

        // Check our capabilities
        if(((ddscp.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
            (ddcp.dwMiscCaps & DDMISCCAPS_COLORCONTROLPRIMARY)) ||
           ((ddscp.dwCaps & DDSCAPS_OVERLAY) &&
            (ddcp.dwMiscCaps & DDMISCCAPS_COLORCONTROLOVERLAY)))
        {
            // Capability is supported, do nothing
            ;
        }
        else
        {
            // Remove ColorControl from the list
            bCnt--;
        }

#endif // not KATANA

        if(Test_QueryInterface(
            pDDSTI->m_pDDS,
            c_szIDirectDrawSurface,
            iidValid,
            bCnt,
            spcFlag))
        {
            nResult = ITPR_PASS;
        }
        else
        {
            nResult = ITPR_FAIL;
        }
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Restore
//  Tests the IDirectDrawSurface::Restore method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_Restore(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    INT         nResult = ITPR_PASS;
    HRESULT     hr;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    // UPDATE: Merge these tests with the IsLost tests
    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(0, Test_IDDS_Restore, lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szRestore);
    }
    else if(pDDSTI->m_pDDS)
    {
        hr = pDDSTI->m_pDDS->Restore();
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szRestore,
                hr,
                NULL,
                pDDSTI->m_pszName);
            nResult = ITPR_FAIL;
        }
        else
        {
            nResult = ITPR_PASS;
        }
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Help_DDSIterate
//  Iterates through all surface types and runs the specified test for each
//  type.
//
// Parameters:
//  dwFlags         Determine the behavior of the function. No error checking
//                  is performed on this parameter. Currently supported flags
//                  are the following:
//                      DDSI_TESTBACKBUFFER     Iterates over the back buffer
//                                              of the primary.
//                  A value of zero means that none of the above behaviors are
//                  exhibited.
//  pfnTestProc     Address of the test function. This should be the same as the
//                  test function in the test function table. If the user data
//                  parameter is not NULL, the call originated from this
//                  function. Otherwise, it originated from the Tux shell.
//  pFTE            The function table entry that generated this call. This
//                  entry will be modified to contain a pointer to the
//                  DDSTESTINFO structure.
//  dwTestData      Additional data as set up by the test. The first time the
//                  test is called, this data is passed in. Afterwards, the
//                  test is free to modify this value, which is preserved
//                  throughout all iterations.
//
// Return value:
//  The code returned by the test function, or TPR_FAIL if some other error
//  condition arises.

INT Help_DDSIterate(
    DWORD                   dwFlags,
    DDSTESTPROC             pfnTestProc,
    LPFUNCTION_TABLE_ENTRY  pFTE,
    DWORD                   dwTestData)
{
    DDSTESTINFO ddsti;
    IDDS        *pIDDS;
    HRESULT     hr;
    int         nIndex;
    ITPR        nITPR = ITPR_PASS;

    IDirectDrawSurface      *pDDS = NULL;
    DDSURFACEDESC           ddsd;
    FUNCTION_TABLE_ENTRY    fte;

    if(!pfnTestProc || !pFTE)
    {
        Debug(
            LOG_ABORT,
            FAILURE_HEADER TEXT("Help_DDSIterate was passed NULL"));
        return TPR_ABORT;
    }

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    // if we don't support flipping, turn off flipping tests.
    if(!(g_idds.m_ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(LOG_COMMENT, TEXT("Flipping unsupported."));
        dwFlags &= ~DDSI_TESTBACKBUFFER;
    }

    fte = *pFTE;
    fte.dwUserData      = (DWORD)&ddsti;
    ddsti.m_pDD         = pIDDS->m_pDD;
    ddsti.m_dwTestData  = dwTestData;

#if QAHOME_OVERLAY
    int nCases;
    if(pIDDS->m_bOverlaysSupported)
        nCases = 7;
    else
#if TEST_ZBUFFER
        nCases = 4;
#else // TEST_ZBUFFER
        nCases = 3;
#endif // TEST_ZBUFFER
#else // QAHOME_OVERLAY
#if TEST_ZBUFFER
    #define nCases 4
#else // TEST_ZBUFFER
    #define nCases 3
#endif // TEST_ZBUFFER
#endif // QAHOME_OVERLAY

    for(nIndex = 0; nIndex < nCases; nIndex++)
    {
        switch(nIndex)
        {
        case 0:
            ddsti.m_pDDS    = pIDDS->m_pDDSPrimary;
            ddsti.m_pddsd   = &pIDDS->m_ddsdPrimary;
            ddsti.m_pszName = TEXT("for primary");
            break;

        case 1:
            if(!(dwFlags & DDSI_TESTBACKBUFFER))
                // Don't do the back buffer test
                continue;

            // Get the back buffer
            pDDS = (IDirectDrawSurface *)PRESET;
            hr = pIDDS->m_pDDSPrimary->EnumAttachedSurfaces(&pDDS, Help_GetBackBuffer_Callback);
            if(!CheckReturnedPointer(
                CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                pDDS,
                c_szIDirectDrawSurface,
                c_szGetAttachedSurface,
                hr,
                TEXT("back buffer"),
                TEXT("for primary")))
            {
                nITPR |= ITPR_ABORT;
                continue;
            }
            else
            {
                // Get the surface description
                memset(&ddsd, 0, sizeof(ddsd));
                ddsd.dwSize = sizeof(ddsd);
                hr = pDDS->GetSurfaceDesc(&ddsd);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szGetSurfaceDesc,
                        hr,
                        NULL,
                        TEXT("for back buffer"));
                    nITPR |= ITPR_ABORT;
                    continue;
                }
                else
                {
                    ddsti.m_pDDS    = pDDS;
                    ddsti.m_pddsd   = &ddsd;
                    ddsti.m_pszName = TEXT("for primary's back buffer");
                }
            }
            break;

        case 2:
            if(!pIDDS->m_pDDSSysMem)
            {
                // System memory unavailable
                Debug(
                    LOG_COMMENT,
                    TEXT("Off-screen surface in system memory unavailable"));
                continue;
            }
            ddsti.m_pDDS    = pIDDS->m_pDDSSysMem;
            ddsti.m_pddsd   = &pIDDS->m_ddsdSysMem;
            ddsti.m_pszName = TEXT("for off-screen surface in system memory");
            break;

        case 3:
            if(!pIDDS->m_pDDSVidMem)
            {
                // Video memory unavailable
                Debug(
                    LOG_COMMENT,
                    TEXT("Off-screen surface in video memory unavailable"));
                continue;
            }
            ddsti.m_pDDS    = pIDDS->m_pDDSVidMem;
            ddsti.m_pddsd   = &pIDDS->m_ddsdVidMem;
            ddsti.m_pszName = TEXT("for off-screen surface in video memory");
            break;

#if TEST_ZBUFFER
        case 4:
            ddsti.m_pDDS    = pIDDS->m_pDDSZBuffer;
            ddsti.m_pddsd   = &pIDDS->m_ddsdZBuffer;
            ddsti.m_pszName = TEXT("for Z buffer");
            break;
#endif // TEST_ZBUFFER

#if QAHOME_OVERLAY
        case 5:
            ddsti.m_pDDS    = pIDDS->m_pDDSOverlay1;
            ddsti.m_pddsd   = &pIDDS->m_ddsdOverlay1;
            ddsti.m_pszName = TEXT("for overlay #1");
            break;

        case 6:
            ddsti.m_pDDS    = pIDDS->m_pDDSOverlay2;
            ddsti.m_pddsd   = &pIDDS->m_ddsdOverlay2;
            ddsti.m_pszName = TEXT("for overlay #2");
            break;
#endif // QAHOME_OVERLAY
        }

        DebugBeginLevel(0, TEXT("Testing %s..."), ddsti.m_pszName + 4);

        __try
        {
            nITPR |= pfnTestProc(TPM_EXECUTE, 0, &fte);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            Debug(
                LOG_EXCEPTION,
                FAILURE_HEADER TEXT("An exception occurred during the")
                TEXT(" execution of this test"));
            nITPR |= TPR_FAIL;
        }

        // If this was the back buffer test, now is the time to release it
        if(nIndex == 1)
        {
            pDDS->Release();
        }

        DebugEndLevel(TEXT("Done with %s"), ddsti.m_pszName + 4);
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_IDDS_IsLost_EDM_Callback
//  Finds two different display modes for the IDDS::IsLost test.
//
// Parameters:
//  pddsd           Pointer to a DDSURFACEDESC structure describing a mode.
//  pContext        Pointer to a TWOMODES structure that we will fill.
//
// Return value:
//  DDENUMRET_OK to continue enumerating, DDENUMRET_CANCEL to stop.

HRESULT WINAPI Help_IDDS_IsLost_EDM_Callback(
    DDSURFACEDESC   *pddsd,
    void            *pContext)
{
    TWOMODES    *pTM = (TWOMODES *)pContext;

    switch(pTM->m_nValidModes)
    {
    case 0:
        // This is the first mode we see. It's good enough; just save it
        pTM->m_mode1.m_dwWidth      = pddsd->dwWidth;
        pTM->m_mode1.m_dwHeight     = pddsd->dwHeight;
        pTM->m_mode1.m_dwDepth      = pddsd->ddpfPixelFormat.dwRGBBitCount;
        pTM->m_mode1.m_dwFrequency  = pddsd->dwRefreshRate;
        pTM->m_nValidModes = 1;
        break;

    case 1:
        // We've seen at least one video mode. If the current mode is
        // different enough, keep it.
        if(pTM->m_mode1.m_dwWidth != pddsd->dwWidth ||
           pTM->m_mode1.m_dwHeight != pddsd->dwHeight ||
           pTM->m_mode1.m_dwDepth != pddsd->ddpfPixelFormat.dwRGBBitCount ||
           pTM->m_mode1.m_dwFrequency != pddsd->dwRefreshRate)
        {
            // Looks good. Keep it and stop enumerating
            pTM->m_mode2.m_dwWidth      = pddsd->dwWidth;
            pTM->m_mode2.m_dwHeight     = pddsd->dwHeight;
            pTM->m_mode2.m_dwDepth      = pddsd->ddpfPixelFormat.dwRGBBitCount;
            pTM->m_mode2.m_dwFrequency  = pddsd->dwRefreshRate;
            pTM->m_nValidModes = 2;
            return (HRESULT)DDENUMRET_CANCEL;
        }
        break;

    default:
        // We should really never get here...
        return (HRESULT)DDENUMRET_CANCEL;
    }

    return (HRESULT)DDENUMRET_OK;
}

#if TEST_IDDS && TEST_IDDS2 && TEST_IDDS3
#if QAHOME_INVALIDPARAMS

#if 0
////////////////////////////////////////////////////////////////////////////////
// Test_Keep222
//  Repro case for bug Keep #222
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_Keep222(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    IDDS2   *pIDDS2;
    IDDS3   *pIDDS3;
    HRESULT hr;
    HDC     hDC;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawSurface  *pDDS;
    IDirectDrawSurface2 *pDDS2;
    IDirectDrawSurface3 *pDDS3;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPR_ABORT;

    if(!InitDirectDrawSurface2(pIDDS2))
        return TPR_ABORT;

    if(!InitDirectDrawSurface3(pIDDS3))
        return TPR_ABORT;

    // V1 behavior
    pDDS = pIDDS->m_pDDSPrimary;

    // Get the DC
    hDC = (HDC)PRESET;
    hr = pDDS->GetDC(&hDC);
    if(!CheckReturnedPointer(
        CRP_NOTPOINTER | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        hDC,
        c_szIDirectDrawSurface,
        c_szGetDC,
        hr,
        NULL,
        TEXT("device context")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Unlock the surface
        hr = pDDS->Unlock(NULL);

        // For V1, this should succeed
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szUnlock,
                hr,
                NULL,
                TEXT("after call to GetDC"));
            nITPR |= ITPR_FAIL;
        }

        // Release the DC
        hr = pDDS->ReleaseDC(hDC);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szReleaseDC,
                hr,
                NULL,
                TEXT("after call to GetDC and Unlock"));
            nITPR |= ITPR_FAIL;
        }
    }

    // V2 behavior
    pDDS2 = pIDDS2->m_pDDS2Primary;

    // Get the DC
    hDC = (HDC)PRESET;
    hr = pDDS2->GetDC(&hDC);
    if(!CheckReturnedPointer(
        CRP_NOTPOINTER | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        hDC,
        c_szIDirectDrawSurface2,
        c_szGetDC,
        hr,
        NULL,
        TEXT("device context")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Unlock the Surface
        hr = pDDS2->Unlock(NULL);

        // For V2, this should succeed
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface2,
                c_szUnlock,
                hr,
                NULL,
                TEXT("after call to GetDC"));
            nITPR |= ITPR_FAIL;
        }

        // Release the DC
        hr = pDDS2->ReleaseDC(hDC);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface2,
                c_szReleaseDC,
                hr,
                NULL,
                TEXT("after call to GetDC and Unlock"));
            nITPR |= ITPR_FAIL;
        }
    }

    // V3 behavior
    pDDS3 = pIDDS3->m_pDDS3Primary;

    // Get the DC
    hDC = (HDC)PRESET;
    hr = pDDS3->GetDC(&hDC);
    if(!CheckReturnedPointer(
        CRP_NOTPOINTER | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        hDC,
        c_szIDirectDrawSurface3,
        c_szGetDC,
        hr,
        NULL,
        TEXT("device context")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Unlock the Surface
        hr = pDDS3->Unlock(NULL);

        // For V3, this should fail
        if(hr != DDERR_GENERIC)
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawSurface3,
                c_szUnlock,
                hr,
                NULL,
                TEXT("after call to GetDC"),
                DDERR_GENERIC);
            nITPR |= ITPR_FAIL;
        }

        // Release the DC
        hr = pDDS3->ReleaseDC(hDC);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface3,
                c_szReleaseDC,
                hr,
                NULL,
                TEXT("after call to GetDC and Unlock"));
            nITPR |= ITPR_FAIL;
        }
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, TEXT("Keep #222 repro case"));

    return TPRFromITPR(nITPR);
}
#endif // 0
#endif // QAHOME_INVALIDPARAMS
#endif // TEST_IDDS && TEST_IDDS2 && TEST_IDDS3
#endif // TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
