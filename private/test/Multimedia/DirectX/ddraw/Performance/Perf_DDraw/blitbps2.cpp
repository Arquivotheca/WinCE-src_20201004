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
 // Blitbps2.cpp - does a blt and a scaled blt  on surfaces in system and video memory
 ////////////////////////////////////////////////////////////////////////////////
#include "main.h"
#include <DDrawUty.h>
#include <QATestUty/WinApp.h>
#include "perfCases.h"
#include <ddraw.h>
#include "perfloggerapi.h"
#include "DdrawPerfTimer.h"

#define SampleSize 100

using namespace DDrawUty;
using namespace DebugStreamUty;
using namespace com_utilities;

extern DDrawPerfTimer g_pDDPerfTimer;

namespace
{
    ////////////////////////////////////////////////////////////////////////////////
    // BasicBlitTest - Runs 10*SampleSize blts, timing each group of 10 and reporting the results
    eTestResult BasicBlitTest(CComPointer<IDirectDrawSurface> *piDDSSource,
                                                CComPointer<IDirectDrawSurface> *piDDSDest,
                                                RECT rcSrcRect,
                                                RECT rcDstRect,
                                                DWORD dwMarker)
    {
        int i;
        eTestResult tr=trPass;
        HRESULT hr=DD_OK;

        for(i=0;i<SampleSize; i++)
        {
            g_pDDPerfTimer.BeginDuration(dwMarker);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            hr|=(*piDDSDest)->Blt(&rcDstRect,
                                                (*piDDSSource).AsInParam(),
                                                &rcSrcRect,
                                                DDBLT_WAITNOTBUSY,
                                                NULL);
            // endmarker imarker
            g_pDDPerfTimer.EndDuration(dwMarker);
        }

        if(hr!=DD_OK)
            tr=trFail;
        return tr;
    }

}

////////////////////////////////////////////////////////////////////////////////
// CDDPerfBps2::Test - does assorted timed blit tests
eTestResult CDDPerfBps2VidVid::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    DDCAPS ddCaps;

    RECT  rcScaledSrcRect,
                rcScaledDstRect,
                rcSrcRect,
                rcDstRect;
    int iSourceSizeScaled = 10;
    int iSourceSizeNotScaled = 100;
    int iMinDimension;

    // get the direct draw pointer
    piDD = g_DirectDraw.GetObject();

    memset(&ddCaps, 0x00, sizeof(ddCaps));
    ddCaps.dwSize = sizeof(ddCaps);
    piDD->GetCaps(&ddCaps, NULL);

    if (!(ddCaps.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
    {
        dbgout << "Driver does not support video memory. Skipping." << endl;
        tr = trSkip;
        goto Cleanup;
    }

    // when doing a scaled blt, scale it from 10x10
    // to 100x100, making sure they don't overlap

    iMinDimension = g_BlitPerf.m_dwSurfaceWidth;
    if (iMinDimension > g_BlitPerf.m_dwSurfaceHeight)
    {
        iMinDimension = g_BlitPerf.m_dwSurfaceHeight;
    }

    if (iSourceSizeScaled + iSourceSizeScaled * 10 > iMinDimension)
    {
        dbgout
            << "Using nonstandard size in scaling perf test: "
            << iSourceSizeScaled
            << " scaled to "
            << 10*iSourceSizeScaled
            << endl;
        iSourceSizeScaled = iMinDimension / 11;
    }

    if (iSourceSizeNotScaled * 2 > iMinDimension)
    {
        iSourceSizeNotScaled = iMinDimension / 2;
    }

    rcScaledSrcRect.left=0;
    rcScaledSrcRect.right=iSourceSizeScaled;
    rcScaledSrcRect.top=0;
    rcScaledSrcRect.bottom=iSourceSizeScaled;

    rcScaledDstRect.left=iSourceSizeScaled;
    rcScaledDstRect.right=iSourceSizeScaled + iSourceSizeScaled * 10;
    rcScaledDstRect.top=iSourceSizeScaled;
    rcScaledDstRect.bottom=iSourceSizeScaled + iSourceSizeScaled * 10;

    // standard blit from 100x100 to 100x100
    // non overlapping
    rcSrcRect.left=0;
    rcSrcRect.right=iSourceSizeNotScaled;
    rcSrcRect.top=0;
    rcSrcRect.bottom=iSourceSizeNotScaled;

    rcDstRect.left=iSourceSizeNotScaled;
    rcDstRect.right=iSourceSizeNotScaled * 2;
    rcDstRect.top=iSourceSizeNotScaled;
    rcDstRect.bottom=iSourceSizeNotScaled * 2;

    // run the test on two surfaces in video memory
    tr = InitSurface(
        &piDD,
        &piDDSSrc,
        DDSCAPS_VIDEOMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for Video did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }
    tr = InitSurface(
        &piDD,
        &piDDSDst,
        DDSCAPS_VIDEOMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Destination for Video did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    // Video to video blit with 1 surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcSrcRect,
                                rcDstRect,
                                MARK_BLIT_VIDVID_1SURF);

    // Video to video blit with 1 surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcScaledSrcRect,
                                rcScaledDstRect,
                                MARK_SBLIT_VIDVID_1SURF);

    // Video to video blit with 2 surfaces
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcSrcRect,
                                rcDstRect,
                                MARK_BLIT_VIDVID_2SURF);

    // Video to video blit with 2 surfaces
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcScaledSrcRect,
                                rcScaledDstRect,
                                MARK_SBLIT_VIDVID_2SURF);

Cleanup:
    // release the surfaces
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();
    // release the DD object
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}

////////////////////////////////////////////////////////////////////////////////
// CDDPerfBps2::Test - does assorted timed blit tests
eTestResult CDDPerfBps2SysVid::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    DDCAPS ddCaps;

    RECT  rcScaledSrcRect,
                rcScaledDstRect,
                rcSrcRect,
                rcDstRect;
    int iSourceSizeScaled = 10;
    int iSourceSizeNotScaled = 100;
    int iMinDimension;

    // get the direct draw pointer
    piDD = g_DirectDraw.GetObject();

    memset(&ddCaps, 0x00, sizeof(ddCaps));
    ddCaps.dwSize = sizeof(ddCaps);
    piDD->GetCaps(&ddCaps, NULL);

    if (!(ddCaps.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
    {
        dbgout << "Driver does not support video memory. Skipping." << endl;
        tr = trSkip;
        goto Cleanup;
    }

    // when doing a scaled blt, scale it from 10x10
    // to 100x100, making sure they don't overlap
    iMinDimension = g_BlitPerf.m_dwSurfaceWidth;
    if (iMinDimension > g_BlitPerf.m_dwSurfaceHeight)
    {
        iMinDimension = g_BlitPerf.m_dwSurfaceHeight;
    }

    if (iSourceSizeScaled + iSourceSizeScaled * 10 > iMinDimension)
    {
        dbgout
            << "Using nonstandard size in scaling perf test: "
            << iSourceSizeScaled
            << " scaled to "
            << 10*iSourceSizeScaled
            << endl;
        iSourceSizeScaled = iMinDimension / 11;
    }

    if (iSourceSizeNotScaled * 2 > iMinDimension)
    {
        iSourceSizeNotScaled = iMinDimension / 2;
    }

    rcScaledSrcRect.left=0;
    rcScaledSrcRect.right=iSourceSizeScaled;
    rcScaledSrcRect.top=0;
    rcScaledSrcRect.bottom=iSourceSizeScaled;

    rcScaledDstRect.left=iSourceSizeScaled;
    rcScaledDstRect.right=iSourceSizeScaled + iSourceSizeScaled * 10;
    rcScaledDstRect.top=iSourceSizeScaled;
    rcScaledDstRect.bottom=iSourceSizeScaled + iSourceSizeScaled * 10;

    // standard blit from 100x100 to 100x100
    // non overlapping
    rcSrcRect.left=0;
    rcSrcRect.right=iSourceSizeNotScaled;
    rcSrcRect.top=0;
    rcSrcRect.bottom=iSourceSizeNotScaled;

    rcDstRect.left=iSourceSizeNotScaled;
    rcDstRect.right=iSourceSizeNotScaled * 2;
    rcDstRect.top=iSourceSizeNotScaled;
    rcDstRect.bottom=iSourceSizeNotScaled * 2;

    // run the test on a surfaces in video and system memory
    tr = InitSurface(
        &piDD,
        &piDDSSrc,
        DDSCAPS_SYSTEMMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for System did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }
    tr = InitSurface(
        &piDD,
        &piDDSDst,
        DDSCAPS_VIDEOMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Destination for Video did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    // System to video blit with 2 surfaces
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcSrcRect,
                                rcDstRect,
                                MARK_BLIT_SYSVID_2SURF);

    // System to video blit with 2 surfaces
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcScaledSrcRect,
                                rcScaledDstRect,
                                MARK_SBLIT_SYSVID_2SURF);


Cleanup:
    // release the surfaces
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();
    // release the DD object
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}


////////////////////////////////////////////////////////////////////////////////
// CDDPerfBps2::Test - does assorted timed blit tests
eTestResult CDDPerfBps2SysSys::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;

    RECT  rcScaledSrcRect,
                rcScaledDstRect,
                rcSrcRect,
                rcDstRect;
    int iSourceSizeScaled = 10;
    int iSourceSizeNotScaled = 100;
    int iMinDimension;

    // get the direct draw pointer
    piDD = g_DirectDraw.GetObject();

    // when doing a scaled blt, scale it from 10x10
    // to 100x100, making sure they don't overlap
    iMinDimension = g_BlitPerf.m_dwSurfaceWidth;
    if (iMinDimension > g_BlitPerf.m_dwSurfaceHeight)
    {
        iMinDimension = g_BlitPerf.m_dwSurfaceHeight;
    }

    if (iSourceSizeScaled + iSourceSizeScaled * 10 > iMinDimension)
    {
        dbgout
            << "Using nonstandard size in scaling perf test: "
            << iSourceSizeScaled
            << " scaled to "
            << 10*iSourceSizeScaled
            << endl;
        iSourceSizeScaled = iMinDimension / 11;
    }

    if (iSourceSizeNotScaled * 2 > iMinDimension)
    {
        iSourceSizeNotScaled = iMinDimension / 2;
    }

    rcScaledSrcRect.left=0;
    rcScaledSrcRect.right=iSourceSizeScaled;
    rcScaledSrcRect.top=0;
    rcScaledSrcRect.bottom=iSourceSizeScaled;

    rcScaledDstRect.left=iSourceSizeScaled;
    rcScaledDstRect.right=iSourceSizeScaled + iSourceSizeScaled * 10;
    rcScaledDstRect.top=iSourceSizeScaled;
    rcScaledDstRect.bottom=iSourceSizeScaled + iSourceSizeScaled * 10;

    // standard blit from 100x100 to 100x100
    // non overlapping
    rcSrcRect.left=0;
    rcSrcRect.right=iSourceSizeNotScaled;
    rcSrcRect.top=0;
    rcSrcRect.bottom=iSourceSizeNotScaled;

    rcDstRect.left=iSourceSizeNotScaled;
    rcDstRect.right=iSourceSizeNotScaled * 2;
    rcDstRect.top=iSourceSizeNotScaled;
    rcDstRect.bottom=iSourceSizeNotScaled * 2;

    // run the test on two surfaces in system memory
    tr = InitSurface(
        &piDD,
        &piDDSSrc,
        DDSCAPS_SYSTEMMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for System did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }
    tr = InitSurface(
        &piDD,
        &piDDSDst,
        DDSCAPS_SYSTEMMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Destination for Video did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    // System to system blit with 1 surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcSrcRect,
                                rcDstRect,
                                MARK_BLIT_SYSSYS_1SURF);

    // System to system blit with 1 surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcScaledSrcRect,
                                rcScaledDstRect,
                                MARK_SBLIT_SYSSYS_1SURF);

    // System to system blit with 2 surfaces
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcSrcRect,
                                rcDstRect,
                                MARK_BLIT_SYSSYS_2SURF);

    // System to system blit with 2 surfaces
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcScaledSrcRect,
                                rcScaledDstRect,
                                MARK_SBLIT_SYSSYS_2SURF);

Cleanup:
    // release the surfaces
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();
    // release the DD object
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}

