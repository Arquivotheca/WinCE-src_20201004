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

    eTestResult LockReadTest(
        CComPointer<IDirectDrawSurface> piDDS,
        const POINT & ptFill,
        const DWORD  dwMarker)
    {
        CDDSurfaceDesc cddsd;
        HRESULT hr;
        DWORD * pbStop = NULL;
        DWORD * pbSurface = NULL;

        hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
        CheckHRESULT(hr, "Initial Lock", trAbort);
        piDDS->Unlock(NULL);

        DWORD dwDummy;
        LONG lDwordLineWidth = (cddsd.lXPitch * (ptFill.x) + 3) >> 2;
        LONG lDwordEndToBeginning = (cddsd.lPitch >> 2) - lDwordLineWidth;

        for(int i=0;i<SampleSize; i++)
        {
            g_pDDPerfTimer.BeginDuration(dwMarker);
            for (int j = 0; j < 10; ++j)
            {
                hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY | DDLOCK_READONLY, NULL);
                if (FAILED(hr))
                {
                    goto Cleanup;
                }

                pbSurface = (DWORD*)cddsd.lpSurface;

                // Read from the surface
                for (int iLine = 0; iLine < ptFill.y; ++iLine)
                {
                    pbStop = pbSurface + lDwordLineWidth;
                    while (pbStop != pbSurface)
                    {
                        dwDummy = *pbSurface;
                        ++pbSurface;
                    }
                    pbSurface += lDwordEndToBeginning;
                }
                piDDS->Unlock(NULL);
            }
            g_pDDPerfTimer.EndDuration(dwMarker);
        }
Cleanup:
        if (FAILED(hr))
        {
            g_pDDPerfTimer.EndDuration(dwMarker);
            return trFail;
        }
        return trPass;
    }

    eTestResult LockWriteTest(
        CComPointer<IDirectDrawSurface> piDDS,
        const POINT & ptFill,
        const DWORD  dwMarker)
    {
        CDDSurfaceDesc cddsd;
        HRESULT hr;
        DWORD * pbStop = NULL;
        DWORD * pbSurface = NULL;

        hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
        CheckHRESULT(hr, "Initial Lock", trAbort);
        piDDS->Unlock(NULL);

        LONG lDwordLineWidth = (cddsd.lXPitch * (ptFill.x) + 3) >> 2;
        LONG lDwordEndToBeginning = (cddsd.lPitch >> 2) - lDwordLineWidth;

        for(int i=0;i<SampleSize; i++)
        {
            g_pDDPerfTimer.BeginDuration(dwMarker);
            for (int j = 0; j < 10; ++j)
            {
                hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                if (FAILED(hr))
                {
                    goto Cleanup;
                }

                pbSurface = (DWORD*)cddsd.lpSurface;

                // Read from the surface
                for (int iLine = 0; iLine < ptFill.y; ++iLine)
                {
                    pbStop = pbSurface + lDwordLineWidth;
#pragma prefast(disable:394, "Potential buffer overrun while writing to buffer. The buffer pointer is being incremented inside a loop.");
                    while (pbStop != pbSurface)
                    {
                        *pbSurface = 0;
                        ++pbSurface;
                    }
#pragma prefast(enable:394, "Potential buffer overrun while writing to buffer. The buffer pointer is being incremented inside a loop.");
                    pbSurface += lDwordEndToBeginning;
                }
                piDDS->Unlock(NULL);
            }
            g_pDDPerfTimer.EndDuration(dwMarker);
        }
Cleanup:
        if (FAILED(hr))
        {
            g_pDDPerfTimer.EndDuration(dwMarker);
            return trFail;
        }
        return trPass;
    }
}
////////////////////////////////////////////////////////////////////////////////
// CDDPerfBps::Test - the test function for blits per second.

eTestResult CDDPerfLockVid::Test()
{
    CComPointer<IDirectDrawSurface> piDDS;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    DDCAPS ddCaps;

    POINT ptEmpty, pt1Pixel, ptFullSurface;

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

    // Initialize points
    ptEmpty.x = 0;
    ptEmpty.y = 0;
    pt1Pixel.x = 1;
    pt1Pixel.y = 1;
    ptFullSurface.x = g_BlitPerf.m_dwSurfaceWidth;
    ptFullSurface.y = g_BlitPerf.m_dwSurfaceHeight;

    // Run the test first with two surfaces in video memory
    tr = InitSurface(
        &piDD,
        &piDDS,
        DDSCAPS_VIDEOMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for Video did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    tr |= LockWriteTest(
        piDDS,
        ptEmpty,
        MARK_LOCK_VID_WRITE);

    tr |= LockReadTest(
        piDDS,
        ptEmpty,
        MARK_LOCK_VID_READ);

    tr |= LockWriteTest(
        piDDS,
        pt1Pixel,
        MARK_LOCK_VID_1PIXEL_WRITE);

    tr |= LockReadTest(
        piDDS,
        pt1Pixel,
        MARK_LOCK_VID_1PIXEL_READ);

    tr |= LockWriteTest(
        piDDS,
        ptFullSurface,
        MARK_LOCK_VID_FULLSURF_WRITE);

    tr |= LockReadTest(
        piDDS,
        ptFullSurface,
        MARK_LOCK_VID_FULLSURF_READ);


Cleanup:
    // relese the surfaces used.
    piDDS.ReleaseInterface();

    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}

////////////////////////////////////////////////////////////////////////////////
// CDDPerfBps::Test - the test function for blits per second.

eTestResult CDDPerfLockSys::Test()
{
    CComPointer<IDirectDrawSurface> piDDS;
    CComPointer<IDirectDraw> piDD;
    eTestResult tr = trPass;
    DDCAPS ddCaps;

    POINT ptEmpty, pt1Pixel, ptFullSurface;

    // get the direct draw pointer
    piDD = g_DirectDraw.GetObject();

    memset(&ddCaps, 0x00, sizeof(ddCaps));
    ddCaps.dwSize = sizeof(ddCaps);
    piDD->GetCaps(&ddCaps, NULL);

    if (!(ddCaps.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
    {
        dbgout << "Driver does not support system memory. Skipping." << endl;
        tr = trSkip;
        goto Cleanup;
    }

    // Initialize points
    ptEmpty.x = 0;
    ptEmpty.y = 0;
    pt1Pixel.x = 1;
    pt1Pixel.y = 1;
    ptFullSurface.x = g_BlitPerf.m_dwSurfaceWidth;
    ptFullSurface.y = g_BlitPerf.m_dwSurfaceHeight;

    // Run the test first with two surfaces in system memory
    tr = InitSurface(
        &piDD,
        &piDDS,
        DDSCAPS_SYSTEMMEMORY,
        g_BlitPerf.m_dwSurfaceWidth,
        g_BlitPerf.m_dwSurfaceHeight);
    if (trPass != tr)
    {
        dbgout << "InitSurface Source for System did not succeed. Non-recoverable." << endl;
        goto Cleanup;
    }

    tr |= LockWriteTest(
        piDDS,
        ptEmpty,
        MARK_LOCK_SYS_WRITE);

    tr |= LockReadTest(
        piDDS,
        ptEmpty,
        MARK_LOCK_SYS_READ);

    tr |= LockWriteTest(
        piDDS,
        pt1Pixel,
        MARK_LOCK_SYS_1PIXEL_WRITE);

    tr |= LockReadTest(
        piDDS,
        pt1Pixel,
        MARK_LOCK_SYS_1PIXEL_READ);

    tr |= LockWriteTest(
        piDDS,
        ptFullSurface,
        MARK_LOCK_SYS_FULLSURF_WRITE);

    tr |= LockReadTest(
        piDDS,
        ptFullSurface,
        MARK_LOCK_SYS_FULLSURF_READ);


Cleanup:
    // relese the surfaces used.
    piDDS.ReleaseInterface();

    piDD.ReleaseInterface();
    // Destroy global objects
    g_DirectDraw.ReleaseObject();

    return tr;
}

