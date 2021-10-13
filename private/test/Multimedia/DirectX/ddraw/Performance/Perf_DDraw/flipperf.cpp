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
// Flipperf.cpp - runs timed tests on flipping a backbuffers to a primary surface
// currently supports MaxBackBufferCounts of 1-5
////////////////////////////////////////////////////////////////////////////////
#include "main.h"
#include <DDrawUty.h>
#include <QATestUty/WinApp.h>
#include "perfCases.h"
#include <ddraw.h>
#include "perfloggerapi.h"
#include "DdrawPerfTimer.h"

#define SampleSize 100
#define MaxBackBufferCount 1

using namespace DDrawUty;
using namespace DebugStreamUty;
using namespace com_utilities;

extern DDrawPerfTimer g_pDDPerfTimer;

namespace
{
     HRESULT ClearBackbuffers(LPDIRECTDRAWSURFACE pSurface, LPDDSURFACEDESC pddsd, LPVOID pvContext)
     {
        DDBLTFX ddbltfx;
        HRESULT hr;
        CComPointer<IDirectDrawSurface> * piDDBackBuffer = (CComPointer<IDirectDrawSurface> *) pvContext;

        memset(&ddbltfx, 0, sizeof(ddbltfx));
        ddbltfx.dwSize = sizeof(ddbltfx);
        ddbltfx.dwFillColor = 0;

        if (pddsd->ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER)
        {
            return DDENUMRET_OK;
        }
        // Find a pointer that's not initialized
        while ((*piDDBackBuffer).IsAssigned())
        {
            ++piDDBackBuffer;
        }
        // save the backbuffer
        *piDDBackBuffer = pSurface;
        // clear the backbuffer(s)
        hr=pSurface->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
        CheckHRESULT(hr, "ColorFill Blt", DDENUMRET_CANCEL);

        return DDENUMRET_OK;
     }

    ////////////////////////////////////////////////////////////////////////////////
    // InitComplexSurface - creates a primary surface with a specified number of backbuffers
    // backbuffers are specified by MaxBackBufferCount
    eTestResult InitComplexSurface( CComPointer<IDirectDraw> * piDD, CComPointer<IDirectDrawSurface> * piDDSurface, CComPointer<IDirectDrawSurface> piDDBackBuffer[MaxBackBufferCount], DWORD dwBackBufferCount,  DWORD dwInitCaps)
    {
        CDDSurfaceDesc ddsd;
        HRESULT hr;
        DDSCAPS ddscaps;
        DDBLTFX ddbltfx;

        // Configure the surface
        ddsd.dwSize=sizeof(ddsd);
        ddsd.dwFlags = DDSD_CAPS  | DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps = dwInitCaps  |DDSCAPS_PRIMARYSURFACE
                                                                | DDSCAPS_VIDEOMEMORY
                                                                | DDSCAPS_FLIP;
        ddsd.dwBackBufferCount=dwBackBufferCount;

        // create the surface
        hr = (*piDD)->CreateSurface(&ddsd, (*piDDSurface).AsOutParam(), NULL);

        CheckHRESULT(hr, "CreateSurface", trSkip);

        ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
        // get the backbuffer(s)
        hr = (*piDDSurface)->EnumAttachedSurfaces(piDDBackBuffer, ClearBackbuffers);
        CheckHRESULT(hr, "EnumAttachedSurfaces", trFail);

        memset(&ddbltfx, 0, sizeof(ddbltfx));
        ddbltfx.dwSize = sizeof(ddbltfx);
        ddbltfx.dwFillColor = 0x00F0;

        // flip the primary to the back and fill the back
        hr=(*piDDSurface)->Flip(NULL, DDFLIP_WAITNOTBUSY);
        CheckHRESULT(hr, "Flip", trFail);
        // clear the backbuffer that was flipped to the primary surface
        hr=(*piDDBackBuffer)->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &ddbltfx);
        CheckHRESULT(hr, "ColorFill Blt", trFail);

        return(trPass);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // FlipTest - does SampleSize*10 flips, timing each group of 10 flips
    eTestResult FlipTest(CComPointer<IDirectDrawSurface> *piDDSSource, DWORD dwMarker)
    {
        int i;
        eTestResult tr=trPass;
        HRESULT hr=DD_OK;

        for(i=0;i<SampleSize; i++)
        {
            g_pDDPerfTimer.BeginDuration(dwMarker);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            hr|=(*piDDSSource)->Flip(NULL, DDFLIP_WAITNOTBUSY);
            g_pDDPerfTimer.EndDuration(dwMarker);
        }
        if(hr!=DD_OK)
            tr|=trFail;
        return tr;
    }
}

////////////////////////////////////////////////////////////////////////////////
// CDDPerfFlip::Test - creates flipping chains and times flipping
eTestResult CDDPerfFlip::Test()
{
    CComPointer<IDirectDrawSurface> piDDSPrimary,
                                                         piDDSBackbuffer[MaxBackBufferCount];
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;

    DDCAPS ddcaps;
    HRESULT hr;

    int i;

    piDD = g_DirectDraw.GetObject();

    QueryHRESULT(g_DirectDraw.GetLastError(), "g_DirectDraw.GetObject", tr=trAbort; goto Cleanup);

    hr=piDD->GetCaps(&ddcaps, NULL);
    QueryHRESULT(hr, "GetCaps", tr=trFail; goto Cleanup);
    if(!(ddcaps.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        tr = trSkip;
        dbgout << "Flipping is not supported, Skipping." << endl;
        goto Cleanup;
    }

    for(int k = 1; k <= MaxBackBufferCount; k++)
    {
        // create the surface and backbuffers
        tr = InitComplexSurface(&piDD, &piDDSPrimary, piDDSBackbuffer, k, NULL);
        if (trPass != tr)
        {
            dbgout << "Could not create test surfaces" << endl;
            break;
        }

        // run the test
        tr = FlipTest(&piDDSPrimary, MARK_FLIP_FROM_BACKBUFFER_COUNT(k));
        // release all of the backbuffers

        // should be safe to release even if it wasn't initialized?
        for(i = 0; i < MaxBackBufferCount; i++)
        {
            piDDSBackbuffer[i].ReleaseInterface();
        }

        // release the primary surface
        piDDSPrimary.ReleaseInterface();

        if (trPass != tr)
        {
            dbgout << "FlipTest did not pass" << endl;
            break;
        }
    }

Cleanup:
    for(i = 0; i < MaxBackBufferCount; i++)
    {
        piDDSBackbuffer[i].ReleaseInterface();
    }

    // release the primary surface
    piDDSPrimary.ReleaseInterface();
    // Release local interface pointers
    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}
