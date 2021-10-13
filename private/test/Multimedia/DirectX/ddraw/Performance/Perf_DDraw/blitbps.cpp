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
// Blitbps.cpp - does a full surface blt and a 1 pixel blt on surfaces in system and video memory
////////////////////////////////////////////////////////////////////////////////
#include "main.h"
#include <DDrawUty.h>
#include <QATestUty/WinApp.h>
#include <ddraw.h>
#include "perfCases.h"
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
    // BasicBlitTest - does a timed blit test on the surfaces given.  Does 10*SampleSize actual
    // blts, and reports the time for each set of 10.
    //
    //      returns trFailed if any of the blits fail.

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
            g_pDDPerfTimer.EndDuration(dwMarker);
        }

        if(hr!=DD_OK)
            tr|=trFail;
        return tr;
    }

}

////////////////////////////////////////////////////////////////////////////////
// CDDPerfBps::Test - the test function for blits per second.

eTestResult CDDPerfBpsVidVid::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    DDCAPS ddCaps;

    RECT  rcFullSrcRect,
              rcFullDstRect,
              rcOneSrcRect,
              rcOneDstRect;

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

    // initialize the rectangles to be used
    // the full surface copy must actually copy something
    // or the blt function will optimize it out if it is to the
    // same surface.
    // set the source to be to the right of the surface
    rcFullSrcRect.left=1;
    rcFullSrcRect.right=g_BlitPerf.m_dwSurfaceWidth;
    rcFullSrcRect.top=1;
    rcFullSrcRect.bottom=g_BlitPerf.m_dwSurfaceHeight;
    // set the dest to be to the left of the surface
    rcFullDstRect.left=0;
    rcFullDstRect.right=g_BlitPerf.m_dwSurfaceWidth - 1;
    rcFullDstRect.top=0;
    rcFullDstRect.bottom=g_BlitPerf.m_dwSurfaceHeight - 1;
    // The rectangles for the one pixel copy are set
    // to copy a different pixel, just in case the
    // blt function optimizes out copying a pixel
    // onto itself.
    rcOneSrcRect.left=0;
    rcOneSrcRect.right=1;
    rcOneSrcRect.top=0;
    rcOneSrcRect.bottom=1;
    rcOneDstRect.left=1;
    rcOneDstRect.right=2;
    rcOneDstRect.top=0;
    rcOneDstRect.bottom=1;


    // Run the test first with two surfaces in video memory
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

    // Video to video blit with 1 surface full surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcFullSrcRect,
                                rcFullDstRect,
                                MARK_BLIT_VIDVID_1SURF_FULLSURF);

    // Video to video blit with 1 surface one pixel
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcOneSrcRect,
                                rcOneDstRect,
                                MARK_BLIT_VIDVID_1SURF_1PIXEL);

    // Video to video blit with 2 surfaces full surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcFullSrcRect,
                                rcFullDstRect,
                                MARK_BLIT_VIDVID_2SURF_FULLSURF);

    // Video to video blit with 2 surfaces 1 pixel
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcOneSrcRect,
                                rcOneDstRect,
                                MARK_BLIT_VIDVID_2SURF_1PIXEL);

Cleanup:
    // relese the surfaces used.
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();

    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}
eTestResult CDDPerfBpsSysVid::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    DDCAPS ddCaps;

    RECT  rcFullSrcRect,
              rcFullDstRect,
              rcOneSrcRect,
              rcOneDstRect;

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

    // initialize the rectangles to be used
    // the full surface copy must actually copy something
    // or the blt function will optimize it out if it is to the
    // same surface.
    // set the source to be to the right of the surface
    rcFullSrcRect.left=1;
    rcFullSrcRect.right=g_BlitPerf.m_dwSurfaceWidth;
    rcFullSrcRect.top=1;
    rcFullSrcRect.bottom=g_BlitPerf.m_dwSurfaceHeight;
    // set the dest to be to the left of the surface
    rcFullDstRect.left=0;
    rcFullDstRect.right=g_BlitPerf.m_dwSurfaceWidth - 1;
    rcFullDstRect.top=0;
    rcFullDstRect.bottom=g_BlitPerf.m_dwSurfaceHeight - 1;
    // The rectangles for the one pixel copy are set
    // to copy a different pixel, just in case the
    // blt function optimizes out copying a pixel
    // onto itself.
    rcOneSrcRect.left=0;
    rcOneSrcRect.right=1;
    rcOneSrcRect.top=0;
    rcOneSrcRect.bottom=1;
    rcOneDstRect.left=1;
    rcOneDstRect.right=2;
    rcOneDstRect.top=0;
    rcOneDstRect.bottom=1;

    // run the test on surfaces in system and video memory
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

    // System to video blit with 2 surfaces full surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcFullSrcRect,
                                rcFullDstRect,
                                MARK_BLIT_SYSVID_2SURF_FULLSURF);

    // System to video blit with 2 surfaces, one pixel
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcOneSrcRect,
                                rcOneDstRect,
                                MARK_BLIT_SYSVID_2SURF_1PIXEL);

Cleanup:
    // release the surfaces and DD object (have to be released first
    // to prevent an exception)
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}
eTestResult CDDPerfBpsSysSys::Test()
{
    CComPointer<IDirectDrawSurface> piDDSSrc,
                                                         piDDSDst;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;

    RECT  rcFullSrcRect,
              rcFullDstRect,
              rcOneSrcRect,
              rcOneDstRect;

    // get the direct draw pointer
    piDD = g_DirectDraw.GetObject();

    // initialize the rectangles to be used
    // the full surface copy must actually copy something
    // or the blt function will optimize it out if it is to the
    // same surface.
    // set the source to be to the right of the surface
    rcFullSrcRect.left=1;
    rcFullSrcRect.right=g_BlitPerf.m_dwSurfaceWidth;
    rcFullSrcRect.top=1;
    rcFullSrcRect.bottom=g_BlitPerf.m_dwSurfaceHeight;
    // set the dest to be to the left of the surface
    rcFullDstRect.left=0;
    rcFullDstRect.right=g_BlitPerf.m_dwSurfaceWidth - 1;
    rcFullDstRect.top=0;
    rcFullDstRect.bottom=g_BlitPerf.m_dwSurfaceHeight - 1;
    // The rectangles for the one pixel copy are set
    // to copy a different pixel, just in case the
    // blt function optimizes out copying a pixel
    // onto itself.
    rcOneSrcRect.left=0;
    rcOneSrcRect.right=1;
    rcOneSrcRect.top=0;
    rcOneSrcRect.bottom=1;
    rcOneDstRect.left=1;
    rcOneDstRect.right=2;
    rcOneDstRect.top=0;
    rcOneDstRect.bottom=1;

    // run the test on surfaces in system memory
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
        dbgout << "InitSurface Destination for System did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    // System to system blit with 1 surface full surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcFullSrcRect,
                                rcFullDstRect,
                                MARK_BLIT_SYSSYS_1SURF_FULLSURF);

    // System to system blit with 1 surface 1 pixel
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSSrc,
                                rcOneSrcRect,
                                rcOneDstRect,
                                MARK_BLIT_SYSSYS_1SURF_1PIXEL);

    // System to system blit with 2 surfaces Full surface
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcFullSrcRect,
                                rcFullDstRect,
                                MARK_BLIT_SYSSYS_2SURF_FULLSURF);

    // System to system blit with 2 surfaces 1 pixel
    tr|=BasicBlitTest(&piDDSSrc,
                                &piDDSDst,
                                rcOneSrcRect,
                                rcOneDstRect,
                                MARK_BLIT_SYSSYS_2SURF_1PIXEL);

Cleanup:
    // release the surfaces and DD object (have to be released first
    // to prevent an exception)
    piDDSSrc.ReleaseInterface();
    piDDSDst.ReleaseInterface();
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}

