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
#include "main.h"
#include <DDrawUty.h>
#include <QATestUty/WinApp.h>
#include "perfCases.h"
#include <ddraw.h>
#include "perfloggerapi.h"
#include "DdrawPerfTimer.h"

#define SampleSize 100


#define CKRED 0
#define CKGREEN 1
#define CKSPACING1 8
#define CKSPACING2 13

using namespace DDrawUty;
using namespace DebugStreamUty;
using namespace com_utilities;

extern DDrawPerfTimer g_pDDPerfTimer;

namespace
{
    eTestResult InitCKSurface( CComPointer<IDirectDraw> * piDD,
                                            CComPointer<IDirectDrawSurface> * piDDSurface,
                                            DWORD dwInitCaps, bool bFillGreen, DWORD dwBarWidth,
                                            DWORD dwSurfaceWidth, DWORD dwSurfaceHeight)
    {
        CDDSurfaceDesc ddsd;
        HRESULT hr;
        DDBLTFX ddbltfx;
        DWORD *pdwDest;
        DWORD dwVal, dwPixelsPerWrite;

        DDCOLORKEY ddckColorKey;

        dbgout
            << "Creating Test Surface: "
            << dwSurfaceWidth << "x" << dwSurfaceHeight
            << "(ddsCaps = " << hex(dwInitCaps) << ")"
            << endl;

        // Configure the surface
        ddsd.dwSize=sizeof(ddsd);
        ddsd.dwFlags = DDSD_CAPS;
        // if we aren't using the primary surface, declare the width and height also
        if(!(dwInitCaps & DDSCAPS_PRIMARYSURFACE))
            ddsd.dwFlags = ddsd.dwFlags | DDSD_WIDTH | DDSD_HEIGHT;
        ddsd.ddsCaps.dwCaps = dwInitCaps;
        ddsd.dwWidth = dwSurfaceWidth;
        ddsd.dwHeight = dwSurfaceHeight;

        // create the surface
        hr = (*piDD)->CreateSurface(&ddsd, (*piDDSurface).AsOutParam(), NULL);

        CheckHRESULT(hr, "CreateSurface", trFail);

        // clear the surface
        memset(&ddbltfx, 0, sizeof(ddbltfx));
        ddbltfx.dwSize = sizeof(ddbltfx);
        ddbltfx.dwFillColor = 0;

        hr=(*piDDSurface)->Blt(NULL,
                                                NULL,
                                                NULL,
                                                DDBLT_WAITNOTBUSY | DDBLT_COLORFILL,
                                                &ddbltfx);
        CheckHRESULT(hr, "Colorfill blit", trFail);

        // lock the surface, retrieve the surface desc.
        hr=(*piDDSurface)->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
        CheckHRESULT(hr, "surface lock", trFail);

        pdwDest=(DWORD *)ddsd.lpSurface;
        if(ddsd.ddpfPixelFormat.dwRGBBitCount==16)
        {
            // 16 bit pixel values, we can write two at a time here.
            if(bFillGreen)
                dwVal=0x07E007E0;
            else dwVal=0xF800F800;
            ddckColorKey.dwColorSpaceLowValue=0x00000000;
            ddckColorKey.dwColorSpaceHighValue=0x00000000;
            dwPixelsPerWrite=2;
        }
        else
        {
            if(bFillGreen)
                dwVal=0x0000FF00;
            else dwVal=0x00FF0000;
            ddckColorKey.dwColorSpaceLowValue=0x00000000;
            ddckColorKey.dwColorSpaceHighValue=0x00000000;
            dwPixelsPerWrite=1;
        }

        for(int y=0; y<(int)ddsd.dwHeight; y++)
        {
            for(int x=0; x< (int)(ddsd.dwWidth/dwPixelsPerWrite);x++)
            {
                if(((x)%dwBarWidth)>(dwBarWidth/2))
                   pdwDest[x]=dwVal;
                else pdwDest[x]=0x00000000;
            }
            pdwDest+=ddsd.lPitch/4;
        }

        (*piDDSurface)->Unlock(NULL);

        // attach a source color key to the surface, harmless if it's the dest
        hr=(*piDDSurface)->SetColorKey(DDCKEY_SRCBLT, &ddckColorKey);

        CheckHRESULT(hr, "SetColorKey", trFail);

        return(trPass);
    }


    eTestResult BasicBlitTest(CComPointer<IDirectDrawSurface> *piDDSSource,
                                                CComPointer<IDirectDrawSurface> *piDDSDest,
                                                RECT * rcSrcRect,
                                                RECT * rcDstRect,
                                                DWORD dwMarker)
    {
        int i;
        eTestResult tr=trPass;
        HRESULT hr=DD_OK;
        // with Source color keying the source has the color key, the dest
        // has some data, so we can do this over and over and
        // we'll overwrite the same pixel over itself, but to do that
        // the blit is going to do the same stuff it did the first time.

        for(i=0;i<SampleSize; i++)
        {
            g_pDDPerfTimer.BeginDuration(dwMarker);

            // color key blit
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            hr|=(*piDDSDest)->Blt(rcDstRect,
                                                    (*piDDSSource).AsInParam(),
                                                    rcSrcRect,
                                                    DDBLT_WAITNOTBUSY |DDBLT_KEYSRC ,
                                                    NULL);
            // endmarker imarker
            g_pDDPerfTimer.EndDuration(dwMarker);
        }

        if(hr!=DD_OK)
        {
            tr=trFail;
            dbgout << "Colorkey Blit failed" << endl;
        }
        return tr;
    }

}

eTestResult CDDPerfCkBlit::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    CDDSurfaceDesc ddsd;
    RECT  rcFullSrcRect,
             rcFullDstRect;
    DDCAPS ddcaps;
    HRESULT hr;

    // initialize the rectangles to be used
    rcFullSrcRect.left=0;
    rcFullSrcRect.right=g_BlitPerf.m_dwSurfaceWidth;
    rcFullSrcRect.top=0;
    rcFullSrcRect.bottom=g_BlitPerf.m_dwSurfaceHeight;

    rcFullDstRect.left=0;
    rcFullDstRect.right=g_BlitPerf.m_dwSurfaceWidth;
    rcFullDstRect.top=0;
    rcFullDstRect.bottom=g_BlitPerf.m_dwSurfaceHeight;

    // get the direct draw pointer
    piDD = g_DirectDraw.GetObject();

    piDD->GetCaps(&ddcaps, NULL);

    // test for the ability to colorkey blit (doesn't seem to work currently)
    if(!(ddcaps.dwCKeyCaps & DDCKEYCAPS_SRCBLT))
    {
        tr=trSkip;
        dbgout << "color key blitting not supported (srcblt). Skipping." << endl;
        goto Cleanup;
    }

    if (!(ddcaps.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
    {
        tr = trSkip;
        dbgout << "Video memory surfaces not supported. Skipping." << endl;
        goto Cleanup;
    }

    dbgout << "source surface in vid memory" << endl;
    tr=InitCKSurface(
        &piDD,
        &piDDSSrc,
        DDSCAPS_VIDEOMEMORY,
        CKRED,
        CKSPACING1,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for video memory source did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    dbgout << "dest surface, primary" << endl;
    tr=InitCKSurface(
        &piDD,
        &piDDSDst,
        DDSCAPS_PRIMARYSURFACE,
        CKGREEN,
        CKSPACING2,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for Primary destination did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    hr = piDD->SetCooperativeLevel(
        CWindowsModule::GetWinModule().m_hWnd,
        DDSCL_FULLSCREEN);
    if (FAILED(hr))
    {
        dbgout << "Unable to set cooperative level to fullscreen (" << hr << "). Proceeding with Normal cooperative level" << endl;
    }

    // Video to video blit with 1 surface full surface
    tr|=BasicBlitTest(
        &piDDSSrc,
        &piDDSDst,
        NULL,
        NULL,
        MARK_BLIT_VIDVID_CKBLT);

Cleanup:
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}
