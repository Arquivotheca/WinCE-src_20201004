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
//  Module: dds_2.cpp
//          BVTs for IDirectDrawSurface interface functions.
//
//  Revision History:
//      12/05/1997  lblanco     Split off from dds.cpp.
//                              maintaining this code, it's only fair! :)).
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "dds.h"
#pragma warning(disable : 4995)
#if TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
// Local macros

#define NUM_FLIPS_FOR_GETFLIPSTATUS 100
#define MAX_BACKBUFFERS_FOR_FLIP    1
////////////////////////////////////////////////////////////////////////////////
// Local prototypes

ITPR    Help_TestFlip(IDirectDraw *, int, int);
HFONT   GetSimpleFont(LPTSTR, UINT);
bool    Help_GetSurfaceLocations(IDirectDrawSurface **, __in DWORD *, int, bool);
ITPR    Help_TestIDDSGetDC(
            IDirectDraw *,
            IDirectDrawSurface *,
            bool bValid = true);

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Flip
//  Tests the IDirectDrawSurface::Flip method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_Flip(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDirectDraw *pDD;
    int         nBuffers,
                nOverride;
    ITPR        nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    // Make sure that other surface tests don't interfere with us
    FinishDirectDrawSurface();

    // Get a DirectDraw object
    if(!InitDirectDraw(pDD))
        return TPRFromITPR(g_tprReturnVal);

    // Try several quantities of back buffers
#if QAHOME_INVALIDPARAMS
    nBuffers = 0;
#else // QAHOME_INVALIDPARAMS
    nBuffers = 1;
#endif // QAHOME_INVALIDPARAMS

    for(; nBuffers <= MAX_BACKBUFFERS_FOR_FLIP; nBuffers++)
    {
        DebugBeginLevel(
            0,
            TEXT("Testing %s::%s for a primary with %d back buffer%s"),
            c_szIDirectDrawSurface,
            c_szFlip,
            nBuffers,
            nBuffers == 1 ? TEXT("") : TEXT("s"));
        for(nOverride = 0; nOverride <= nBuffers; nOverride++)
        {
            if(nOverride)
            {
                Debug(
                    LOG_DETAIL,
                    TEXT("Overriding Flip with a pointer to back buffer #%d"),
                    nOverride);
            }
            else
            {
                Debug(
                    LOG_DETAIL,
                    TEXT("Using default Flip target buffer"));
            }

            // Do it
            __try
            {
                nITPR |= Help_TestFlip(pDD, nBuffers, nOverride);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Debug(
                    LOG_EXCEPTION,
                    FAILURE_HEADER TEXT("An exception occurred during the")
                    TEXT(" execution of this test"));
                nITPR |= ITPR_FAIL;
            }
        }
        DebugEndLevel(TEXT(""));
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szFlip);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_TestIDDSGetDC
//  Verifies the correlation between a DirectDraw surface and a GDI DC. This
//  is used by the IDirectDrawSurface::GetDC method test.
//
// Parameters:
//  pDD             Pointer to the DirectDraw object.
//  pDDS            Pointer to the surface to be verified.
//  bValid          True if we expect success from GetDC, false otherwise.
//
// Return value:
//  ITPR_PASS if the test passes, ITPR_PASS if the test fails, or possibly other
//  special conditions.

ITPR Help_TestIDDSGetDC(IDirectDraw *pDD, IDirectDrawSurface *pDDS, bool bValid)
{
    HDC     hdc = NULL,
#if QAHOME_INVALIDPARAMS
            hdc2 = NULL,
#endif // QAHOME_INVALIDPARAMS
            hdcMem = NULL;
    HFONT   hFont = NULL,
            hOldFont = NULL;
    RECT    rc      = { 0, 0, 0, 0 },
            rcMem   = { 0, 0, 0, 0 };
    DWORD   dwDepth;
    DWORD   dwMem;
    HRESULT hr,
            hrExpected;
    UINT    nBugID;
    ITPR    nITPR = ITPR_PASS,
            nITPRFill;

    IDirectDrawSurface  *pDDSMem = NULL;
    DDSURFACEDESC       ddsd;
    static LPCTSTR      c_szBitBlt = TEXT("BitBlt");

    // Get a description of the surface. This will help us create an
    // off-screen surface for testing purposes
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    hr = pDDS->GetSurfaceDesc(&ddsd);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szGetSurfaceDesc,
            hr);
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Expect failure?
        if(bValid)
        {
            hrExpected = S_OK;
            nBugID = 0;
        }
        else
        {
            // BUGBUG: Not sure if this is correct
            hrExpected = DDERR_INVALIDOBJECT;
            nBugID = 0;
        }

        // Get the DC on the surface
        hdc = (HDC)PRESET;
        hr = pDDS->GetDC(&hdc);
        if(!CheckReturnedPointer(
            CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
            hdc,
            c_szIDirectDrawSurface,
            c_szGetDC,
            hr,
            NULL,
            NULL,
            hrExpected,
            RAID_HPURAID,
            nBugID))
        {
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // If we didn't expect the call to succeed, then we are done.
            // Otherwise, keep going with the test
            if(bValid)
            {
#if QAHOME_INVALIDPARAMS
                // Try a second GetDC() on the surface
                hdc2 = (HDC)PRESET;
                hr = pDDS->GetDC(&hdc2);

                if(!CheckReturnedPointer(
                    CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
                    hdc2,
                    c_szIDirectDrawSurface,
                    c_szGetDC,
                    hr,
                    NULL,
                    NULL,
                    DDERR_DCALREADYCREATED,
                    IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
                    IFKATANAELSE(2836, 2411)))
                {
                    nITPR |= ITPR_FAIL;
                }

                // Try to lock the surface
                ddsd.lpSurface = (void *)PRESET;
                DDSURFACEDESC ddsdTemp = ddsd;
                hr = pDDS->Lock(NULL, &ddsdTemp, 0, NULL);
                if(!CheckReturnedPointer(
                    CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
                    ddsdTemp.lpSurface,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    NULL,
                    TEXT("after call to IDirectDrawSurface::GetDC"),
                    DDERR_SURFACEBUSY))
                {
                    nITPR |= ITPR_FAIL;
                }
#endif // QAHOME_INVALIDPARAMS

                // Create a font and select it into the DC
                hFont = GetSimpleFont((LPTSTR)TEXT("Arial"), 12);
                hOldFont = (HFONT)SelectObject(hdc, hFont);

                // Save some of the surface's settings for later use
                rcMem.right = rc.right = ddsd.dwWidth;
                rcMem.bottom = rc.bottom = ddsd.dwHeight;
                dwDepth = ddsd.ddpfPixelFormat.dwRGBBitCount;

                // See if there is support for wide surfaces
                {
                    DDCAPS  ddhalcaps;

                    memset(&ddhalcaps, 0, sizeof(ddhalcaps));
                    ddhalcaps.dwSize = sizeof(ddhalcaps);

                    hr = pDD->GetCaps(&ddhalcaps, NULL);

                    if(FAILED(hr))
                    {
                        Report(
                            RESULT_ABORT,
                            c_szIDirectDraw,
                            c_szGetCaps,
                            hr);
                        nITPR |= ITPR_ABORT;
                    }
                    else
                    {
                        // If we support wide surfaces, increase the
                        // memory surface width by 10
                        // louiscl: this caps bit is depracated
//                        if(ddhalcaps.dwCaps2 & DDCAPS_WIDESURFACES)
                            rcMem.right += 10;
                    }
                }
                rcMem.bottom += 10;

                // Create another surface with the same format and in the same
                // location as the given surface
                //dwMem = ddsd.ddsCaps.dwCaps & (DDSCAPS_SYSTEMMEMORY | DDSCAPS_VIDEOMEMORY);
                // Create another surface, but don't specify sys or vid memory
                dwMem = 0;
                memset(&ddsd, 0, sizeof(DDSURFACEDESC));
                ddsd.ddsCaps.dwCaps = 0;
                ddsd.dwSize         = sizeof(DDSURFACEDESC);
                ddsd.dwFlags        = DDSD_HEIGHT | DDSD_WIDTH;
                ddsd.dwWidth        = rcMem.right;
                ddsd.dwHeight       = rcMem.bottom;

                ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);

                // If we end up supporting more than 16bpp surfaces, we should
                // vary this so we do some bitblting from one depth to another,
                // but leave it the same as the given surface depth for now
                ddsd.ddpfPixelFormat.dwRGBBitCount = dwDepth;

                // Create it...
                pDDSMem = (IDirectDrawSurface *)PRESET;
                hr = pDD->CreateSurface(&ddsd, &pDDSMem, NULL);
                if (DDERR_OUTOFMEMORY == hr || DDERR_OUTOFVIDEOMEMORY == hr)
                {
                    Debug(
                        LOG_DETAIL,
                        TEXT("Unable to create secondary surface due to OOM condition"));
                }
                else if(!CheckReturnedPointer(
                    CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                    pDDSMem,
                    c_szIDirectDraw,
                    c_szCreateSurface,
                    hr,
                    NULL,
                    NULL,
                    S_OK,
                    IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
                    IFKATANAELSE(0, 3243)))
                {
                    nITPR |= ITPR_FAIL;
                }
                else
                {
                    // Fill the surface with some known data
                    nITPRFill = FillSurface(rand(), pDDSMem, &rc);
                    nITPR |= nITPRFill;
                    if(nITPRFill == ITPR_PASS)
                    {
                        // Get the DC on the offscreen surface
                        hdcMem = (HDC)PRESET;
                        hr = pDDSMem->GetDC(&hdcMem);
                        if(!CheckReturnedPointer(
                            CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
                            hdcMem,
                            c_szIDirectDrawSurface,
                            c_szGetDC,
                            hr,
                            NULL,
                            TEXT("for off-screen surface")))
                        {
                            nITPR |= ITPR_FAIL;
                        }
                        else
                        {

#if QAHOME_INVALIDPARAMS
                            // Try to fastblt to the surface. This should fail
                            // because we are currently holding an implicit
                            // lock to the surface
                            INVALID_CALL_E(
                                pDDS->Blt(
                                    &rc,
                                    pDDSMem,
                                    &rc,
                                    DDBLT_WAITNOTBUSY,
                                    NULL),
                                c_szIDirectDrawSurface,
                                c_szBltFast,
                                TEXT("while surface was locked"),
                                DDERR_SURFACEBUSY);
#endif // QAHOME_INVALIDPARAMS

        /*
        // Try to GDI delete the DC
        if(DeleteDC(hdc))
        {
            Log(
                LOG_FAIL,
                TEXT("Succeeded in doing a GDI DeleteDC() on the surface DC"));
            nTPR = ResolveResult(nTPR, TPR_FAIL);

            // Try to release the deleted dc
            nTPR = ResolveResult(
                nTPR,
                LogResult(
                    pDDS->ReleaseDC(hdc),
                    TEXT("Attempting to DDraw ReleaseDC() the GDI")
                    TEXT(" deleted DC")));

            // Try to GetDC() and ReleaseDC() the surface to see if the GDI
            // DeleteDC() actually released the surface

            // Try to lock and unlock the surface to see if the GDI DeleteDC()
            // actually released the surface
            nTPR = ResolveResult(
                nTPR,
                LogResult(
                    pDDS->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR, NULL),
                    TEXT("Attempting to IDDS::Lock() the surface after the")
                    TEXT(" DeleteDC()"),
                    DDERR_SURFACEBUSY));
            nTPR = ResolveResult(
                nTPR,
                LogResult(
                    pDDS->Unlock(NULL),
                    TEXT("Attempting to IDDS::Unlock() the surface after the")
                    TEXT(" DeleteDC()"),
                    DDERR_SURFACEBUSY));

            // Try to fastblt to the surface
            nTPR = ResolveResult(
                nTPR,
                LogResult(
                    pDDS->BltFast(
                        0,
                        0,
                        pDDSMem,
                        &rc,
                        DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAITNOTBUSY),
                    TEXT("Attempting to IDDS::BltFast() the surface after")
                    TEXT(" deleting its DC")));

            // Try to release the deleted dc
            nTPR = ResolveResult(
                nTPR,
                LogResult(
                    pDDS->ReleaseDC(hdc),
                    TEXT("Attempting to DDraw ReleaseDC() the GDI")
                    TEXT(" deleted DC")));
        }
        else
        {
            Log(LOG_PASS, TEXT("Correctly failed to DeleteDC() the DC"));
        }
        */
                            const TCHAR *szMsg = TEXT("Testing GetDC()");
                            // Write text to the DC
                            if(!ExtTextOut(
                                hdc,
                                0,
                                0,
                                0,
                                &rc,
                                szMsg,
                                _tcslen(szMsg),
                                NULL))
                            {
                                Report(
                                    RESULT_FAILURENOHRESULT,
                                    TEXT("ExtTextOut"),
                                    NULL,
                                    0);
                                nITPR |= ITPR_FAIL;
                            }

                        // BitBlt from one half of the DC to the other
                        if(!BitBlt(
                            hdc,
                            0,
                            0,
                            rc.right/2,
                            rc.bottom,
                            hdc,
                            rc.right/2,
                            0,
                            SRCCOPY))
                        {
                            Report(
                                RESULT_FAILURENOHRESULT,
                                c_szBitBlt,
                                NULL,
                                0,
                                NULL,
                                TEXT("given the same source and")
                                TEXT(" destination DCs"));
                            nITPR |= ITPR_FAIL;
                        }

                        // Next we will do eight Blt operations, four being
                        // from our surface to the off-screen surface, four
                        // being the other way. We'll do one pair of Blts for
                        // each corner of the screen. To simplify the code, the
                        // macros below do all of the work uniformly
#define DO_BLT_TEST_GENERAL(a, b, c, d, e, f, g, h, i) \
                        do { \
                            if(!BitBlt(a, b, c, d, e, f, g, h, SRCCOPY)) \
                            { \
                                Report( \
                                    RESULT_FAILURENOHRESULT, \
                                    c_szBitBlt, \
                                    NULL, \
                                    0, \
                                    NULL, \
                                    i); \
                                nITPR |= ITPR_FAIL; \
                            } \
                        } while(0)

#define DO_BLT_TEST_TO(a, b, c) \
                        DO_BLT_TEST_GENERAL( \
                            hdc, \
                            a rc.right/2, \
                            b rc.bottom/2, \
                            rc.right, \
                            rc.bottom, \
                            hdcMem, \
                            0, \
                            0, \
                            TEXT("to ") c)
#define DO_BLT_TEST_FROM(a, b, c) \
                        DO_BLT_TEST_GENERAL( \
                            hdcMem, \
                            0, \
                            0, \
                            rcMem.right, \
                            rcMem.bottom, \
                            hdc, \
                            a rc.right/2, \
                            b rc.bottom/2, \
                            TEXT("from ") c)

                            // The tests
                            DO_BLT_TEST_TO  (-, -, TEXT("upper left"));
                            DO_BLT_TEST_FROM(-, -, TEXT("upper left"));
                            DO_BLT_TEST_TO  (+, -, TEXT("upper right"));
                            DO_BLT_TEST_FROM(+, -, TEXT("upper right"));
                            DO_BLT_TEST_TO  (-, +, TEXT("lower left"));
                            DO_BLT_TEST_FROM(-, +, TEXT("lower left"));
                            DO_BLT_TEST_TO  (+, +, TEXT("lower right"));
                            DO_BLT_TEST_FROM(+, +, TEXT("lower right"));

                            DO_BLT_TEST_GENERAL(
                                hdcMem,
                                -5,
                                -5,
                                rcMem.right,
                                rcMem.bottom,
                                hdc,
                                0,
                                0,
                                TEXT("for total surface overlap"));
                            DO_BLT_TEST_GENERAL(
                                hdc,
                                0,
                                0,
                                rcMem.right,
                                rcMem.bottom,
                                hdcMem,
                                0,
                                0,
                                TEXT("to a larger region than the surface"));

        // Get rid of the macros we don't need anymore
#undef DO_BLT_TEST_FROM
#undef DO_BLT_TEST_TO
#undef DO_BLT_TEST_GENERAL

                            // Try to unlock the surface. This should work on
                            // a V1 interface
                            hr = pDDS->Unlock(NULL);
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

                            // Release the memory surface's DC
                            hr = pDDSMem->ReleaseDC(hdcMem);
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDrawSurface,
                                    c_szReleaseDC,
                                    hr,
                                    NULL,
                                    TEXT("for the off-screen surface"));
                                nITPR |= ITPR_FAIL;
                            }
                        }
                    }

                    // Release the memory surface
                    pDDSMem->Release();
                }
            }

            // Release the font we created
            if(hFont)
            {
                SelectObject(hdc, hOldFont);
                DeleteObject(hFont);
                hFont = NULL;
            }

            // Release the DC on the surface
            hr = pDDS->ReleaseDC(hdc);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szReleaseDC,
                    hr,
                    NULL,
                    NULL,
                    0,
                    RAID_HPURAID,
                    IFKATANAELSE(0, 2411));
                nITPR |= ITPR_FAIL;
            }
        }
    }

    return nITPR;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetDC ***
//  Tests the IDirectDrawSurface::GetDC method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetDC(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    HDC     hdc;
    HFONT   hFont;
    RECT    rc = {0, 0, 100, 100};
    DDSCAPS ddscaps;
    HRESULT hr;
    ITPR    nRet,
            nITPR = ITPR_PASS;

    DDSURFACEDESC       ddsd;
    IDirectDrawSurface  *pDDS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    // Primary surface
    DebugBeginLevel(0, TEXT("Testing primary..."));
    nRet = Help_TestIDDSGetDC(pIDDS->m_pDD, pIDDS->m_pDDSPrimary);
    if(nRet == ITPR_PASS)
    {
        Report(RESULT_SUCCESS, TEXT("Tests for primary"));
    }
    DebugEndLevel(TEXT("Done with primary"));
    nITPR |= nRet;

    // Back Buffer surface
    memset(&ddscaps, 0, sizeof(DDSCAPS));
    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pIDDS->m_pDDSPrimary->EnumAttachedSurfaces(
        &pDDS, Help_GetBackBuffer_Callback);

    if((DDERR_NOTFOUND == hr || DDERR_GENERIC == hr ) &&
        !(pIDDS->m_ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(LOG_COMMENT, TEXT("Flipping unsupported."));
    }
    else if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDrawSurface,
        c_szGetAttachedSurface,
        hr,
        NULL,
        TEXT("for back buffer")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        DebugBeginLevel(0, TEXT("Testing back buffer..."));
        nRet = Help_TestIDDSGetDC(pIDDS->m_pDD, pDDS);
        if(nRet == ITPR_PASS)
        {
            Report(RESULT_SUCCESS, TEXT("Tests for back buffer"));
        }
        DebugEndLevel(TEXT("Done with back buffer"));
        pDDS->Release();
        nITPR |= nRet;
    }

    // System memory surface
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwWidth = 100;
    ddsd.dwHeight = 100;
    ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    ddsd.ddpfPixelFormat.dwRGBBitCount = 16;

    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pIDDS->m_pDD->CreateSurface(&ddsd, &pDDS, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        NULL,
        TEXT("for off-screen in system memory")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        DebugBeginLevel(0, TEXT("Testing system memory surface..."));
        nRet = Help_TestIDDSGetDC(pIDDS->m_pDD, pDDS);
        if(nRet == ITPR_PASS)
        {
            Report(RESULT_SUCCESS, TEXT("Tests for system memory surface"));
        }
        DebugEndLevel(TEXT("Done with system memory surface"));
        pDDS->Release();
        nITPR |= nRet;

        // System memory surface with early release
        memset(&ddsd, 0, sizeof(DDSURFACEDESC));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
        ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        ddsd.dwWidth = 100;
        ddsd.dwHeight = 100;
        ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
    }

    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pIDDS->m_pDD->CreateSurface(&ddsd, &pDDS, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        NULL,
        TEXT("the second time for off-screen in system memory")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        hdc = (HDC)PRESET;
        hr = pDDS->GetDC(&hdc);
        if(!CheckReturnedPointer(
            CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
            hdc,
            c_szIDirectDrawSurface,
            c_szGetDC,
            hr,
            NULL,
            TEXT("for system memory surface")))
        {
            nITPR |= ITPR_FAIL;
        }
        else
        {
            hFont = GetSimpleFont((LPTSTR)TEXT("Arial"), 12);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            // Release the surface without first releasing the DC
            pDDS->Release();

            // Attempt to do a ExtTextOutW() to the released surface
            if(ExtTextOut(
                hdc,
                0,
                0,
                0,
                &rc,
                TEXT("Testing GetDC()"),
                sizeof(TEXT("Testing GetDC()")) / sizeof(TCHAR),
                NULL))
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifndef KATANA
                    TEXT("(HPURAID #2425) ")
#endif // !KATANA
                    TEXT("ExtTextOut succeeded on a")
                    TEXT(" released system memory surface"));
                nITPR |= ITPR_FAIL;
            }
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);

            // Attempt to do a bitblt to the released surface
            if(BitBlt(hdc, 0, 0, 50, 100, hdc, 50, 0, SRCCOPY))
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifndef KATANA
                    TEXT("(HPURAID #2425) ")
#endif // !KATANA
                    TEXT("BitBlt() succeeded on a")
                    TEXT(" released system memory surface"));
                nITPR |= ITPR_FAIL;
            }
        }

        // System memory surface with a font larger than the surface
        memset(&ddsd, 0, sizeof(DDSURFACEDESC));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
        ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        ddsd.dwWidth = 5;
        ddsd.dwHeight = 5;
        ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        ddsd.ddpfPixelFormat.dwRGBBitCount = 16;

        pDDS = (IDirectDrawSurface *)PRESET;
        hr = pIDDS->m_pDD->CreateSurface(&ddsd, &pDDS, NULL);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            pDDS,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            NULL,
            TEXT("the third time for off-screen in system memory")))
        {
            nITPR |= ITPR_ABORT;
        }
        else
        {
            hdc = (HDC)PRESET;
            hr = pDDS->GetDC(&hdc);
            if(!CheckReturnedPointer(
                CRP_DONTRELEASE | CRP_ALLOWNONULLOUT,
                hdc,
                c_szIDirectDrawSurface,
                c_szGetDC,
                hr,
                NULL,
                TEXT("for system memory surface")))
            {
                nITPR |= ITPR_FAIL;
            }
            else
            {
                hFont = GetSimpleFont((LPTSTR)TEXT("Arial"), 48);
                HFONT hOldFont = (HFONT)SelectObject(hdc, (HGDIOBJ)hFont);

                // Attempt to do a ExtTextOutW() to the surface
                if(!ExtTextOut(
                    hdc,
                    0,
                    0,
                    0,
                    &rc,
                    TEXT("Testing GetDC()"),
                    sizeof(TEXT("Testing GetDC()")) / sizeof(TCHAR),
                    NULL))
                {
                    Report(
                        RESULT_FAILURENOHRESULT,
                        TEXT("ExtTextOut"),
                        NULL,
                        0,
                        NULL,
                        TEXT("on a system memory surface smaller than the font"));
                    nITPR |= ITPR_FAIL;
                }
                SelectObject(hdc, hOldFont);
                DeleteObject(hFont);

                hr = pDDS->ReleaseDC(hdc);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szReleaseDC,
                        hr);
                    nITPR |= ITPR_FAIL;
                }
            }

            // Release the surface
            pDDS->Release();
        }
    }

    // Video memory surface
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwWidth = 100;
    ddsd.dwHeight = 100;
    ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    ddsd.ddpfPixelFormat.dwRGBBitCount = 16;

    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pIDDS->m_pDD->CreateSurface(&ddsd, &pDDS, NULL);
    if (DDERR_OUTOFVIDEOMEMORY == hr ||
        (DDERR_INVALIDCAPS == hr && !(pIDDS->m_ddcHAL.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
    {
        Debug(LOG_COMMENT, TEXT("Received DDERR_OUTOFVIDEOMEMORY while creating video memory surface, continuing."));
    }
    else if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        NULL,
        TEXT("for off-screen in video memory")))
    {
        nITPR |= ITPR_ABORT;
    }
    else
    {
        DebugBeginLevel(0, TEXT("Testing video memory surface..."));
        nRet = Help_TestIDDSGetDC(pIDDS->m_pDD, pDDS);
        if(nRet == ITPR_PASS)
        {
            Report(RESULT_SUCCESS, TEXT("Tests for video memory surface"));
        }
        pDDS->Release();
        DebugEndLevel(TEXT("Done with video memory surface"));
        nITPR |= nRet;
    }

#if QAHOME_OVERLAY
    // Overlay surface
    if(pIDDS->m_bOverlaysSupported)
    {
        DebugBeginLevel(0, TEXT("Testing overlay..."));
        nRet = Help_TestIDDSGetDC(
            pIDDS->m_pDD,
            pIDDS->m_pDDSOverlay1,
            true);
        if(nRet == ITPR_PASS)
        {
            Report(RESULT_SUCCESS, TEXT("Tests for overlay"));
        }
        DebugEndLevel(TEXT("Done with overlay"));
        nITPR |= nRet;
    }
#endif // QAHOME_OVERLAY

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetDC);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetFlipStatus
//  Tests the IDirectDrawSurface::GetFlipStatus method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetFlipStatus(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    HRESULT hr,
            hrFailure0 = S_OK,
            hrFailure2 = S_OK;
    int     nFlips,
            nFailures[3];
    LPCTSTR pszHeader,
            pszOnly;
    ITPR    nITPR = ITPR_PASS;

    KATOVERBOSITY   nFailCode;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    if(!(pIDDS->m_ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
    {
        Debug(
            LOG_COMMENT,
            TEXT("Flipping unsupported, test skipped"));
        return TPR_SKIP;
    }

    // We expect WASSTILLDRAWING unless the primary is in system memory.
    // If the primary is in system memory, then we can always "flip".
    DDSURFACEDESC ddsd;
    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    hr = pIDDS->m_pDDSPrimary->GetSurfaceDesc(&ddsd);
    HRESULT hrExpected1 = DDERR_WASSTILLDRAWING;
    if (ddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) hrExpected1 = S_OK;

    // Initialize the flag to be passed to Flip(), based on the hardware capabilities.
    DWORD dwFlipFlags = DDFLIP_WAITNOTBUSY;
    if((pIDDS->m_ddcHAL.dwMiscCaps & DDMISCCAPS_FLIPVSYNCWITHVBI))
    {
        dwFlipFlags |= DDFLIP_WAITVSYNC;
    }
    else
    {
        hrExpected1 = S_OK;
    }

    memset(nFailures, 0, sizeof(nFailures));
    for(nFlips = 0; nFlips < NUM_FLIPS_FOR_GETFLIPSTATUS; nFlips++)
    {
        hr = pIDDS->m_pDDSPrimary->GetFlipStatus(DDGFS_CANFLIP);
        if(FAILED(hr))
        {
            nFailures[0]++;
            hrFailure0 = hr;
        }

        hr = pIDDS->m_pDDSPrimary->Flip(NULL, dwFlipFlags);
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szFlip, hr);
            nITPR |= ITPR_ABORT;

            // If we can't flip, that's really bad...
            break;
        }
        else
        {
            hr = pIDDS->m_pDDSPrimary->GetFlipStatus(DDGFS_ISFLIPDONE);
            if(hr != hrExpected1)
                nFailures[1]++;

            Sleep(25);

            hr = pIDDS->m_pDDSPrimary->GetFlipStatus(DDGFS_ISFLIPDONE);
            if(FAILED(hr))
            {
                nFailures[2]++;
                hrFailure2 = hr;

                // Sleep for another 1/4 of a second to make sure that the
                // flip completes. Otherwise, the GetFlipStatus for the next
                // iteration will likely fail as well.
                Sleep(250);
            }
        }
    }

    // There should be no failures before Flip
    if(nFailures[0])
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s failed %d time%s before Flip (last")
            TEXT(" failure returned %s: %s)"),
            c_szIDirectDrawSurface,
            c_szGetFlipStatus,
            nFailures[0],
            nFailures[0] == 1 ? TEXT("") : TEXT("s"),
            GetErrorName(hrFailure0),
            GetErrorDescription(hrFailure0));
        nITPR |= ITPR_FAIL;
    }

    // We'll allow up to a 10% failure rate for calls right after Flip
    if(nFailures[1]*10 >= NUM_FLIPS_FOR_GETFLIPSTATUS)
    {
        nFailCode = LOG_FAIL;
        pszHeader = FAILURE_HEADER;
        nITPR |= ITPR_FAIL;
        pszOnly = TEXT("");
    }
    else
    {
        nFailCode = LOG_DETAIL;
        pszHeader = TEXT("");
        pszOnly = TEXT(" only");
    }

    // Log this only if there were any failures at all
    if(nFailures[1])
    {
        Debug(
            nFailCode,
            TEXT("%s%s::%s failed%s %d out of %d times immediately after Flip")
            TEXT(" by not returning %s"),
            pszHeader,
            c_szIDirectDrawSurface,
            c_szGetFlipStatus,
            pszOnly,
            nFailures[1],
            NUM_FLIPS_FOR_GETFLIPSTATUS,
            GetErrorName(DDERR_WASSTILLDRAWING));
    }

    // Allow no failures after we've waited 1/40th of a second:
    if(nFailures[2])
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("%s::%s failed %d time%s 1/40th of a second")
            TEXT(" after Flip (last failure returned %s: %s)"),
            c_szIDirectDrawSurface,
            c_szGetFlipStatus,
            nFailures[2],
            nFailures[2] == 1 ? TEXT("") : TEXT("s"),
            GetErrorName(hrFailure2),
            GetErrorDescription(hrFailure2));
        nITPR |= ITPR_FAIL;
    }

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetFlipStatus);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Lock_Unlock
//  Tests the IDirectDrawSurface::Lock and IDirectDrawSurface::Unlock methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_Lock_Unlock(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    // If we are going to use overlays, then we have six test surfaces.
    // Otherwise, we only have four.
#if QAHOME_OVERLAY
#define NUM_SURFACES    6
#else // QAHOME_OVERLAY
#define NUM_SURFACES    4
#endif // QAHOME_OVERLAY

    IDDS    *pIDDS;
    RECT    rect;
    int     index;
    LPCTSTR pszName[NUM_SURFACES];
    HRESULT hr = S_OK;
    TCHAR   szConfig[64];
    ITPR    nITPR = ITPR_PASS;
    DWORD   dwData;
    bool    bWritten;
    LPCTSTR pszBugID;
    UINT    nBugID;

    DDSURFACEDESC       ddsd1,
                        ddsd2,
                        *pddsd[NUM_SURFACES];
    IDirectDrawSurface  *pDDS[NUM_SURFACES];

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    // If we support overlays, it may still be that the hardware doesn't. In
    // that case we need a variable that's set at run-time. If we don't support
    // overlays, we don't need a variable. Here's how we solve this:
#if QAHOME_OVERLAY
    int nSurfaces;
    if(pIDDS->m_bOverlaysSupported)
        nSurfaces = 6;
    else
        nSurfaces = 4;
#else // QAHOME_OVERLAY
#define nSurfaces 4
#endif // QAHOME_OVERLAY

    // Set up our test array
#if TEST_ZBUFFER
    pDDS[0] = pIDDS->m_pDDSZBuffer;
#endif // TEST_ZBUFFER
    pDDS[1] = pIDDS->m_pDDSPrimary;
    pDDS[2] = pIDDS->m_pDDSSysMem;
    pDDS[3] = pIDDS->m_pDDSVidMem;
#if TEST_ZBUFFER
    pddsd[0] = &pIDDS->m_ddsdZBuffer;
#endif // TEST_ZBUFFER
    pddsd[1] = &pIDDS->m_ddsdPrimary;
    pddsd[2] = &pIDDS->m_ddsdSysMem;
    pddsd[3] = &pIDDS->m_ddsdVidMem;
#if TEST_ZBUFFER
    pszName[0] = TEXT("for Z buffer");
#endif // TEST_ZBUFFER
    pszName[1] = TEXT("for primary");
    pszName[2] = TEXT("for system memory");
    pszName[3] = TEXT("for video memory");
#if QAHOME_OVERLAY
    if(pIDDS->m_bOverlaysSupported)
    {
        pDDS[4] = pIDDS->m_pDDSOverlay1;
        pDDS[5] = pIDDS->m_pDDSOverlay2;
        pddsd[4] = &pIDDS->m_ddsdOverlay1;
        pddsd[5] = &pIDDS->m_ddsdOverlay2;
        pszName[4] = TEXT("for overlay #1");
        pszName[5] = TEXT("for overlay #2");
        nSurfaces = 6;
    }
    else
    {
        nSurfaces = 4;
    }
#endif // QAHOME_OVERLAY

    memset(&ddsd1, 0, sizeof(ddsd1));
    ddsd1.dwSize = sizeof(DDSURFACEDESC);
    memset(&ddsd2, 0, sizeof(ddsd2));
    ddsd2.dwSize = sizeof(DDSURFACEDESC);

#if TEST_ZBUFFER
#ifdef KATANA
    // The Z Buffer should not be lockable on PowerVR chipsets
    if(pDDS[0])
    {
        hr = pDDS[0]->Lock(NULL, &ddsd1, DDLOCK_SURFACEMEMORYPTR, NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ALLOWNONULLOUT,
            ddsd1.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            TEXT("for Z Buffer"),
            DDERR_CANTLOCKSURFACE,
            RAID_KEEP,
            3727))
        {
            nITPR |= ITPR_FAIL;

            // If the surface was locked in error, we should attempt to unlock
            // it to prevent problems later
            if(SUCCEEDED(hr))
            {
                pDDS[0]->Unlock(ddsd1.lpSurface);
            }
        }
    }
#endif // KATANA
#endif // TEST_ZBUFFER

    for(index = 1; index < nSurfaces; index++)
    {
        if(!pDDS[index])
        {
            continue;
        }
/*        hr = pDDS[index]->GetSurfaceDesc(&ddsd1);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr);
            nITPR |= ITPR_ABORT;
        }
        else
        {
            if(ddsd1.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE && ddsd1.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
            {
                Debug(
                    LOG_COMMENT,
                    TEXT("Skipping System Memory Primary"));
                continue;
            }
        }*/

        Debug(
            LOG_COMMENT,
            TEXT("Testing %s"),
            pszName[index] + 4);

#if QAHOME_INVALIDPARAMS
        // Unlock before locking
        hr = pDDS[index]->Unlock(NULL);
        if(hr != DDERR_NOTLOCKED)
        {
            _stprintf_s(szConfig, TEXT("%s when not locked"), pszName[index]);
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawSurface,
                c_szUnlock,
                hr,
                NULL,
                szConfig,
                DDERR_NOTLOCKED);
            nITPR |= ITPR_FAIL;
        }
#endif // QAHOME_INVALIDPARAMS

        // NULL rect
        hr = pDDS[index]->Lock(NULL, &ddsd1, 0, NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ALLOWNONULLOUT,
            ddsd1.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            TEXT("NULL"),
            pszName[index]))
        {
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // Try to write some data to the surface
            dwData = rand()*rand();
            __try
            {
                *(DWORD *)ddsd1.lpSurface = dwData;
                bWritten = true;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                bWritten = false;
#ifndef KATANA
                if(index == 2) // System memory surface
                    pszBugID = TEXT("(Keep #10292) ");
                else
                    pszBugID = TEXT("");
#else // KATANA
                pszBugID = TEXT("");
#endif // KATANA
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%sWriting data to the returned")
                    TEXT(" surface location threw an exception %s"),
                    pszBugID,
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }

            // Try to read the data back from the surface
            __try
            {
                if(*(DWORD *)ddsd1.lpSurface != dwData && bWritten)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("The data read from the surface")
                        TEXT(" is different from the data we just wrote to")
                        TEXT(" the surface %s"),
                        pszName[index]);
                    nITPR |= ITPR_FAIL;
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
#ifndef KATANA
                if(index == 2) // System memory surface
                    pszBugID = TEXT("(Keep #10292) ");
                else
                    pszBugID = TEXT("");
#else // KATANA
                pszBugID = TEXT("");
#endif // KATANA
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%sReading data from the returned")
                    TEXT(" surface location threw an exception %s"),
                    pszBugID,
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }

            hr = pDDS[index]->Unlock(NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    TEXT("NULL-locked"),
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }
        }

        // Non-NULL rect
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = pddsd[index]->dwWidth;
        rect.bottom = pddsd[index]->dwHeight;

        hr = pDDS[index]->Lock(
            &rect,
            &ddsd1,
            0,
            NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ALLOWNONULLOUT,
            ddsd1.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            pszName[index]))
        {
            nITPR |= ITPR_FAIL;
        }
        else
        {
            // Try to write some data to the surface
            dwData = rand()*rand();
            __try
            {
                *(DWORD *)ddsd1.lpSurface = dwData;
                bWritten = true;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                bWritten = false;
#ifndef KATANA
                if(index == 2) // System memory surface
                    pszBugID = TEXT("(Keep #10292) ");
                else
                    pszBugID = TEXT("");
#else // KATANA
                pszBugID = TEXT("");
#endif // KATANA
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%sWriting data to the returned")
                    TEXT(" surface location threw an exception %s"),
                    pszBugID,
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }

            // Try to read the data back from the surface
            __try
            {
                if(*(DWORD *)ddsd1.lpSurface != dwData && bWritten)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER TEXT("The data read from the surface")
                        TEXT(" is different from the data we just wrote to")
                        TEXT(" the surface %s"),
                        pszName[index]);
                    nITPR |= ITPR_FAIL;
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
#ifndef KATANA
                if(index == 2) // System memory surface
                    pszBugID = TEXT("(Keep #10292) ");
                else
                    pszBugID = TEXT("");
#else // KATANA
                pszBugID = TEXT("");
#endif // KATANA
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%sReading data from the returned")
                    TEXT(" surface location threw an exception %s"),
                    pszBugID,
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }

            hr = pDDS[index]->Unlock(&rect);
            if(FAILED(hr))
            {
#ifdef KATANA
                nBugID = 0;
#else // KATANA
                if(index == 2) // System memory surface
                    nBugID = 10294;
                else
                    nBugID = 0;
#endif // KATANA
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    TEXT("non-NULL-locked"),
                    pszName[index],
                    S_OK,
                    RAID_KEEP,
                    nBugID);
                nITPR |= ITPR_FAIL;
            }
        }
    }

    // Try locking two surfaces simultaneously
    Debug(LOG_COMMENT, TEXT("Testing simultaneous lock"));
    if(pIDDS->m_pDDSSysMem)
    {
        hr = pIDDS->m_pDDSSysMem->Lock(
            NULL,
            &ddsd1,
            0,
            NULL);
    }
    if(!CheckReturnedPointer(
        CRP_NOTINTERFACE | CRP_ALLOWNONULLOUT,
        ddsd1.lpSurface,
        c_szIDirectDrawSurface,
        c_szLock,
        hr,
        TEXT("NULL"),
        TEXT("for system memory")))
    {
        nITPR |= ITPR_FAIL;
    }
    else
    {
        if(pIDDS->m_pDDSPrimary)
        {
            hr = pIDDS->m_pDDSPrimary->Lock(
            NULL,
            &ddsd2,
            0,
            NULL);
        }
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ALLOWNONULLOUT,
            ddsd2.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            TEXT("NULL"),
            TEXT("for primary when the system memory surface was locked")))
        {
            nITPR |= ITPR_FAIL;
        }
        else
        {
            if(pIDDS->m_pDDSPrimary)
            {
                hr = pIDDS->m_pDDSPrimary->Unlock(NULL);
            }
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    NULL,
                    TEXT("for off-screen in system memory when the system")
                    TEXT(" memory surface was locked"));
                nITPR |= ITPR_FAIL;
            }
        }

        if(pIDDS->m_pDDSSysMem)
        {
            hr = pIDDS->m_pDDSSysMem->Unlock(NULL);
        }
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szUnlock,
                hr,
                NULL,
                TEXT("for the system memory surface after unlocking the")
                TEXT(" primary"));
            nITPR |= ITPR_FAIL;
        }
    }

    for(index = 1; index < nSurfaces; index++)
    {

        if(!pDDS[index])
        {
            continue;
        }
        // Try to simultaneously lock adjacent regions on a surface
        // (with no overlap)
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = pddsd[index]->dwWidth/2;
        rect.bottom = pddsd[index]->dwHeight;

        hr = pDDS[index]->Lock(&rect, &ddsd1, 0, NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szLock,
                hr,
                TEXT("left half"),
                pszName[index]);
            nITPR |= ITPR_FAIL;
        }
        else
        {
            RECT rect2;
            rect2.left   = pddsd[index]->dwWidth/2;
            rect2.top    = 0;
            rect2.right  = pddsd[index]->dwWidth;
            rect2.bottom = pddsd[index]->dwHeight;

//            hr = pDDS[index]->Lock(
//                &rect2,
//                &ddsd2,
//                0,
//                NULL);
//            if(FAILED(hr))
//            {
//                Report(
//                    RESULT_FAILURE,
//                    c_szIDirectDrawSurface,
//                    c_szLock,
//                    hr,
//                    TEXT("non-overlapping right half"),
//                    pszName[index]);
//                nITPR |= ITPR_FAIL;
//            }
//            else
//            {
//                hr = pDDS[index]->Unlock(&rect2);
//                if(FAILED(hr))
//                {
//                    Report(
//                        RESULT_FAILURE,
//                        c_szIDirectDrawSurface,
//                        c_szUnlock,
//                        hr,
//                        TEXT("non-overlapping right half"),
//                        pszName[index]);
//                    nITPR |= ITPR_FAIL;
//                }
//            }

            // Try to simultaneously lock overlapping regions on a surface. The
            // first region is the same left half as above, which we haven't
            // unlocked yet.
            rect2.left   = 0;
            rect2.top    = 0;
            rect2.right  = pddsd[index]->dwWidth;
            rect2.bottom = pddsd[index]->dwHeight/2;

            hr = pDDS[index]->Lock(
                &rect2,
                &ddsd2,
                0,
                NULL);
            if(hr != DDERR_SURFACEBUSY)
            {
                Report(
                    RESULT_UNEXPECTED,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    TEXT("overlapping region"),
                    pszName[index],
                    DDERR_SURFACEBUSY);
                nITPR |= ITPR_FAIL;
            }
            if(SUCCEEDED(hr))
            {
                hr = pDDS[index]->Unlock(&rect2);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szUnlock,
                        hr,
                        TEXT("overlapping region"),
                        pszName[index]);
                    nITPR |= ITPR_FAIL;
                }
            }

            // Unlock the first region
            hr = pDDS[index]->Unlock(&rect);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    TEXT("left half"),
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }
        }

        // Try to read-only lock a surface more than once
        hr = pDDS[index]->Lock(NULL, &ddsd1, DDLOCK_READONLY, NULL);
        if(FAILED(hr))
        {
            _stprintf_s(szConfig, TEXT("%s, first attempt"), pszName[index]);
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szLock,
                hr,
                TEXT("DDLOCK_READONLY"),
                szConfig);
            nITPR |= ITPR_FAIL;
        }
        else
        {
            hr = pDDS[index]->Lock(NULL, &ddsd2, DDLOCK_READONLY, NULL);
            if(hr != DDERR_SURFACEBUSY)
            {
                _stprintf_s(szConfig, TEXT("%s, second attempt"), pszName[index]);
                Report(
                    RESULT_UNEXPECTED,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    TEXT("DDLOCK_READONLY"),
                    szConfig,
                    DDERR_SURFACEBUSY);
                nITPR |= ITPR_FAIL;
            }
            if(SUCCEEDED(hr))
            {
                hr = pDDS[index]->Unlock(NULL);
                if(FAILED(hr))
                {
                    _stprintf_s(
                        szConfig,
                        TEXT("%s, for second lock"),
                        pszName[index]);
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szUnlock,
                        hr,
                        szConfig);
                    nITPR |= ITPR_FAIL;
                }
            }

            // Try to read-only lock overlapping regions on a surface
            rect.left   = 0;
            rect.top    = 0;
            rect.right  = pddsd[index]->dwWidth;
            rect.bottom = pddsd[index]->dwHeight/2;

            hr = pDDS[index]->Lock(&rect, &ddsd2, DDLOCK_READONLY, NULL);
            if(hr != DDERR_SURFACEBUSY)
            {
                Report(
                    RESULT_UNEXPECTED,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    TEXT("overlapping read-only region"),
                    pszName[index],
                    DDERR_SURFACEBUSY);
                nITPR |= ITPR_FAIL;
            }
            if(SUCCEEDED(hr))
            {
                hr = pDDS[index]->Unlock(&rect);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szUnlock,
                        hr,
                        TEXT("overlapping read-only region"),
                        pszName[index]);
                    nITPR |= ITPR_FAIL;
                }
            }

            hr = pDDS[index]->Unlock(NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    TEXT("read-only surface"));
                nITPR |= ITPR_FAIL;
            }
        }

#if QAHOME_INVALIDPARAMS
        IDirectDrawSurface  *pDDSTemp;

        // Perform Blts on a read-only locked surface. First, create a small
        // work surface
        memset(&ddsd1, 0, sizeof(DDSURFACEDESC));
        ddsd1.dwSize            = sizeof(DDSURFACEDESC);
        ddsd1.dwFlags           = DDSD_HEIGHT | DDSD_WIDTH;
        ddsd1.dwWidth           = 10;
        ddsd1.dwHeight          = 10;
        hr = pIDDS->m_pDD->CreateSurface(&ddsd1, &pDDSTemp, NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szCreateSurface,
                hr,
                TEXT("10x10 off-screen"));
            nITPR |= ITPR_FAIL;
        }
        else
        {
            rect.left   = 0;
            rect.top    = 0;
            rect.right  = 10;
            rect.bottom = 10;
            hr = pDDS[index]->Lock(&rect, &ddsd1, DDLOCK_READONLY, NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    TEXT("DDLOCK_READONLY"),
                    pszName[index]);
                nITPR |= ITPR_FAIL;
            }
            else
            {
                // Try to Blt from a read-only locked surface (if it's possible
                // the docs are wrong)
                hr = pDDSTemp->Blt(
                    &rect,
                    pDDS[index],
                    &rect,
                    DDBLT_WAITNOTBUSY,
                    NULL);
                if(hr != DDERR_SURFACEBUSY)
                {
                    _stprintf_s(szConfig, TEXT("%s as a source"), pszName[index]);
                    Report(
                        RESULT_UNEXPECTED,
                        c_szIDirectDrawSurface,
                        c_szBltFast,
                        hr,
                        NULL,
                        szConfig,
                        DDERR_SURFACEBUSY);
                    nITPR |= ITPR_FAIL;
                }

                // Try to Blt to a read-only locked surface
                hr = pDDS[index]->Blt(
                    &rect,
                    pDDSTemp,
                    &rect,
                    DDBLT_WAITNOTBUSY,
                    NULL);
                if(hr != DDERR_SURFACEBUSY)
                {
                    _stprintf_s(
                        szConfig,
                        TEXT("%s as a destination"),
                        pszName[index]);
                    Report(
                        RESULT_UNEXPECTED,
                        c_szIDirectDrawSurface,
                        c_szBltFast,
                        hr,
                        NULL,
                        szConfig,
                        DDERR_SURFACEBUSY);
                    nITPR |= ITPR_FAIL;
                }

                // Try to Blt to a region directly adjacent to a locked region
                // on the surface

                RECT rect2 = {10, 0, 20, 10};
                hr = pDDS[index]->Blt(
                    &rect,
                    pDDSTemp,
                    &rect2,
                    DDBLT_WAITNOTBUSY,
                    NULL);
                if(hr != DDERR_SURFACEBUSY)
                {
                    _stprintf_s(
                        szConfig,
                        TEXT("%s as a destination, with no overlap"),
                        pszName[index]);
                    Report(
                        RESULT_UNEXPECTED,
                        c_szIDirectDrawSurface,
                        c_szBltFast,
                        hr,
                        NULL,
                        szConfig);
                    nITPR |= ITPR_FAIL;
                }

                // Unlock the surface
                hr = pDDS[index]->Unlock(&rect);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawSurface,
                        c_szUnlock,
                        hr,
                        NULL,
                        pszName[index]);
                    nITPR |= ITPR_FAIL;
                }
            }

            // Release our work surface
            pDDSTemp->Release();
        }
#endif // QAHOME_INVALIDPARAMS
    }

#undef NUM_SURFACES
#if !QAHOME_OVERLAY
#undef nSurfaces
#endif // !QAHOME_OVERLAY

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, TEXT("Lock and Unlock"));

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_ReleaseDC
//  Tests the IDirectDrawSurface::ReleaseDC method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_ReleaseDC(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    HRESULT     hr;
    HFONT       hFont;
    HDC         hDC;
    RECT        rc = { 0, 0, 100, 100 };
    INT         nResult = ITPR_PASS;;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        // Create a font
        hFont = GetSimpleFont((LPTSTR)TEXT("Arial"), 12);
        if(!hFont)
        {
            Report(
                RESULT_FAILURENOHRESULT | RESULT_ABORTFAILURES,
                TEXT("CreateFontIndirect"));
            nResult = TPR_ABORT;
        }
        else
        {
            // Run the tests
            nResult = Help_DDSIterate(
                DDSI_TESTBACKBUFFER,
                Test_IDDS_ReleaseDC,
                lpFTE,
                (DWORD)hFont);

            // Delete the font
            DeleteObject(hFont);

            if(nResult == TPR_PASS)
                Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szReleaseDC);
        }
    }
    else if(pDDSTI->m_pDDS)
    {
        hFont = (HFONT)pDDSTI->m_dwTestData;

        // Get the DC
        hDC = (HDC)PRESET;
        hr = pDDSTI->m_pDDS->GetDC(&hDC);
        if(!CheckReturnedPointer(
            CRP_DONTRELEASE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            hDC,
            c_szIDirectDrawSurface,
            c_szGetDC,
            hr,
            NULL,
            pDDSTI->m_pszName))
        {
            nResult |= TPR_ABORT;
        }
        else
        {
            // Select the font for the DC
            SelectObject(hDC, hFont);

            // Release the DC
            hr = pDDSTI->m_pDDS->ReleaseDC(hDC);
            if(FAILED(hr))
            {
#ifdef KATANA
#define BUGID   0
#else // not KATANA
#define BUGID   2434
#endif // not KATANA

                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szReleaseDC,
                    hr,
                    NULL,
                    pDDSTI->m_pszName,
                    0,
                    RAID_HPURAID,
                    BUGID);
#undef BUGID
                nResult |= ITPR_FAIL;
            }
            else
            {
                if(ExtTextOut(
                    hDC,
                    0,
                    0,
                    0,
                    &rc,
                    TEXT("Testing GetDC()"),
                    sizeof(TEXT("Testing GetDC()")) / sizeof(TCHAR),
                    NULL))
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER
#ifndef KATANA
                        TEXT("(HPURAID #2425) ")
#endif // !KATANA
                        TEXT("ExtTextOut succeeded on a")
                        TEXT(" released system memory surface"));
                    nResult |= ITPR_FAIL;
                }

#if QAHOME_INVALIDPARAMS
                // Release the DC a second time
                hr = pDDSTI->m_pDDS->ReleaseDC(hDC);
                if(hr != DDERR_NODC)
                {
                    Report(
                        RESULT_UNEXPECTED,
                        c_szIDirectDrawSurface,
                        c_szReleaseDC,
                        hr,
                        TEXT("already released DC"),
                        pDDSTI->m_pszName,
                        DDERR_NODC);
                    nResult |= ITPR_FAIL;
                }
#endif // QAHOME_INVALIDPARAMS
            }
        }
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Help_TestFlip
//  Tests the IDirectDrawSurface::Flip method for various number of back buffers
//  for the primary.
//
// Parameters:
//  pDD             Pointer to a DirectDraw object.
//  nBackBuffers    Number of back buffers for the primary.
//  nOverride       Index of the override buffer, starting at 1. For example, if
//                  the value is 2 then the primary is flipped with the second
//                  back buffer instead of the first one. If this value is zero,
//                  the default flip is performed.
//
// Return value:
//  ITPR_PASS if the test passes, ITPR_FAIL if it fails, or ITPR_ABORT if the
//  test cannot be run to completion.

ITPR Help_TestFlip(IDirectDraw *pDD, int nBackBuffers, int nOverride)
{
    HRESULT hr;
    int     nIndex;
    DWORD   dwBefore[MAX_BACKBUFFERS_FOR_FLIP + 1],
            dwAfter[MAX_BACKBUFFERS_FOR_FLIP + 1];
    TCHAR   szError[64];
    ITPR    nITPR = ITPR_PASS;
    bool    bFlipSuccess;

    DDSURFACEDESC       ddsd;
    IDirectDrawSurface  *pDDS[MAX_BACKBUFFERS_FOR_FLIP + 1];

    DDCAPS ddcHAL;

    memset(&ddcHAL, 0, sizeof(ddcHAL));
    ddcHAL.dwSize = sizeof(ddcHAL);

    memset(pDDS, 0, sizeof(pDDS));
    memset(dwBefore, 0, sizeof(dwBefore));
    memset(dwAfter, 0, sizeof(dwAfter));

    if (nBackBuffers > MAX_BACKBUFFERS_FOR_FLIP)
    {
        Debug(LOG_FAIL,TEXT("Invalid Backbuffer count in Help_TestFlip: %d, Reverting to maximum"), nBackBuffers);
        nITPR |= TPR_FAIL;
        nBackBuffers = MAX_BACKBUFFERS_FOR_FLIP;
    }

    hr = pDD->GetCaps(&ddcHAL, NULL);
    if(FAILED(hr))
    {
        Debug(LOG_FAIL,TEXT("Failed to retrieve HAL/HEL caps"));
        nITPR |= TPR_FAIL;
    }

    __try
    {
        // Prepare to create a primary with the requested number of back buffers
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize                 = sizeof(ddsd);

        // Set the caps accordingly
        if(nBackBuffers == 0)
        {
            ddsd.dwFlags            = DDSD_CAPS;
            ddsd.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE;
        }
        else
        {
            ddsd.dwFlags            = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
            ddsd.ddsCaps.dwCaps     = DDSCAPS_PRIMARYSURFACE |
                                      DDSCAPS_FLIP;
            ddsd.dwBackBufferCount  = nBackBuffers;
        }

        // Create it
        pDDS[0] = (IDirectDrawSurface *)PRESET;
        hr = pDD->CreateSurface(&ddsd, &pDDS[0], NULL);

        if((DDERR_NOFLIPHW == hr || DDERR_INVALIDCAPS == hr) &&
            ddsd.ddsCaps.dwCaps & DDSCAPS_FLIP &&
            !(ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
        {
            Debug(
                LOG_COMMENT,
                TEXT("Flipping unsupported, can't test surface"));
            nITPR |= ITPR_SKIP;
            pDDS[0] = NULL;
            __leave;
        }
        else if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            pDDS[0],
            c_szIDirectDraw,
            c_szCreateSurface,
            hr))
        {
            pDDS[0] = NULL;
            nITPR |= ITPR_ABORT;
            __leave;
        }

        // Get pointers to the back buffers, if any
        _tcscpy_s(szError, TEXT("for primary"));
        for(nIndex = 1; nIndex <= nBackBuffers; nIndex++)
        {
            pDDS[nIndex] = (IDirectDrawSurface *)PRESET;
            hr = pDDS[nIndex - 1]->EnumAttachedSurfaces(
                &pDDS[nIndex],
                Help_GetBackBuffer_Callback);
            if(!CheckReturnedPointer(
                CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
                pDDS[nIndex],
                c_szIDirectDrawSurface,
                c_szGetAttachedSurface,
                hr,
                TEXT("DDSCAPS_BACKBUFFER"),
                szError))
            {
                pDDS[nIndex] = NULL;
                nITPR |= ITPR_ABORT;
                __leave;
            }

            // Prepare the error string for the next iteration
            _stprintf_s(szError, TEXT("for back buffer #%d"), nIndex);
        }

        // Get the surface description
        hr = pDDS[0]->GetSurfaceDesc(&ddsd);
        if (FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr);
            nITPR |= ITPR_ABORT;
            __leave;
        }

        if(nBackBuffers)
        {
            for(nIndex = 0; nIndex <= nBackBuffers; nIndex++)
            {
                dwBefore[nIndex] = nIndex;
            }

            if(!Help_GetSurfaceLocations(
                pDDS,
                dwBefore,
                nBackBuffers + 1,
                true))
            {
                // If we don't have the locations, there isn't much we can do
                nITPR |= ITPR_ABORT;
                __leave;
            }
        }

        // Flip (with override, if requested)
        if(nOverride)
        {
            hr = pDDS[0]->Flip(pDDS[nOverride], DDFLIP_WAITNOTBUSY);
        }
        else
        {
            hr = pDDS[0]->Flip(NULL, DDFLIP_WAITNOTBUSY);
        }

        // If there were no back buffers, we were expecting this call to fail
        if(!nBackBuffers)
        {
            if(hr != DDERR_NOTFLIPPABLE)
            {
                Report(
                    RESULT_UNEXPECTED,
                    c_szIDirectDrawSurface,
                    c_szFlip,
                    hr,
                    NULL,
                    NULL,
                    DDERR_NOTFLIPPABLE);
                nITPR |= ITPR_FAIL;
            }

            // With no back buffers, the test ends here
            __leave;
        }

        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szFlip,
                hr);
            nITPR |= ITPR_FAIL;
            __leave;
        }

        DWORD dwStartTick = GetTickCount();

        // Wait until the flip is done
        do
        {
            hr = pDDS[0]->GetFlipStatus(DDGFS_ISFLIPDONE);
            if(hr != S_OK && hr != DDERR_WASSTILLDRAWING)
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szGetFlipStatus,
                    hr);
                nITPR |= ITPR_ABORT;
                break;
            }

            DWORD dwCurrentTick = GetTickCount();
            if((dwCurrentTick - dwStartTick) > 100) 
            {
                Debug(LOG_DETAIL, TEXT("Exceeded time limit of 100ms on GetFlipStatus"));
                nITPR |= ITPR_FAIL;
                break;
            }
        } while(hr == DDERR_WASSTILLDRAWING);

        // Determine the new location of all buffers
        if(!Help_GetSurfaceLocations(
            pDDS,
            dwAfter,
            nBackBuffers + 1,
            false))
        {
            // If we don't have the locations, there isn't much we can do
            nITPR |= ITPR_ABORT;
            __leave;
        }

        Debug(LOG_DETAIL, TEXT("Surface/Before/After (0 is the primary)"));
        for(nIndex = 0; nIndex <= nBackBuffers; nIndex++)
        {
            Debug(
                LOG_DETAIL,
                TEXT("    %d/%d/%d"),
                nIndex,
                dwBefore[nIndex],
                dwAfter[nIndex]);
        }



/*


        // Flip (with override, if requested)
        if(nOverride)
        {
            hr = pDDS[0]->Flip(pDDS[nOverride], DDFLIP_WAITNOTBUSY);
        }
        else
        {
            hr = pDDS[0]->Flip(NULL, DDFLIP_WAITNOTBUSY);
        }

        // If there were no back buffers, we were expecting this call to fail
        if(!nBackBuffers)
        {
            if(hr != DDERR_NOTFLIPPABLE)
            {
                Report(
                    RESULT_UNEXPECTED,
                    c_szIDirectDrawSurface,
                    c_szFlip,
                    hr,
                    NULL,
                    NULL,
                    DDERR_NOTFLIPPABLE);
                nITPR |= ITPR_FAIL;
            }

            // With no back buffers, the test ends here
            __leave;
        }

        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szFlip,
                hr);
            nITPR |= ITPR_FAIL;
            __leave;
        }

        // Wait until the flip is done
        do
        {
            hr = pDDS[0]->GetFlipStatus(DDGFS_ISFLIPDONE);
            if(hr != S_OK && hr != DDERR_WASSTILLDRAWING)
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szGetFlipStatus,
                    hr);
                nITPR |= ITPR_ABORT;
                break;
            }
        } while(hr == DDERR_WASSTILLDRAWING);

        // Determine the new location of all buffers
        if(!Help_GetSurfaceLocations(
            pDDS,
            dwAfter,
            nBackBuffers + 1,
            false))
        {
            // If we don't have the locations, there isn't much we can do
            nITPR |= ITPR_ABORT;
            __leave;
        }

        Debug(LOG_DETAIL, TEXT("Surface/Before/After (0 is the primary)"));
        for(nIndex = 0; nIndex <= nBackBuffers; nIndex++)
        {
            Debug(
                LOG_DETAIL,
                TEXT("    %d/%d/%d"),
                nIndex,
                dwBefore[nIndex],
                dwAfter[nIndex]);
        }




*/



        // The correct result now depends on whether we had an override or not
        bFlipSuccess = true;
        if(nOverride)
        {
            // With override, the primary and the override change pointers,
            // but everything else stays the same
            if(dwAfter[0] != dwBefore[nOverride])
            {
                Debug(
                    LOG_FAIL,
                    TEXT("The primary is not")
                    TEXT(" pointing to the data that belonged to")
                    TEXT(" back buffer #%d before the Flip"),
                    nOverride);
                bFlipSuccess = false;
            }

            if(dwAfter[nOverride] != dwBefore[0])
            {
                Debug(
                    LOG_FAIL,
                    TEXT("Back buffer #%d is not")
                    TEXT(" pointing to the data that belonged to")
                    TEXT(" the primary before the Flip"),
                    nOverride);
                bFlipSuccess = false;
            }

            // The following statement just allows us to check the rest of the
            // pointers a bit faster
            dwAfter[nOverride] = dwBefore[nOverride];

            for(nIndex = 1; nIndex <= nBackBuffers; nIndex++)
            {
                if(dwAfter[nIndex] != dwBefore[nIndex])
                {
                    Debug(
                        LOG_FAIL,
                        TEXT("Back buffer #%d is pointing to")
                        TEXT(" some different data than it was before the")
                        TEXT(" Flip"),
                        nIndex);
                    bFlipSuccess = false;
                }
            }
        }
        else
        {
            for(nIndex = 0; nIndex < nBackBuffers; nIndex++)
            {
                if(dwAfter[nIndex] != dwBefore[nIndex + 1])
                {
                    if(nIndex)
                    {
                        Debug(
                            LOG_FAIL,
                            TEXT("Back buffer #%d is not")
                            TEXT(" pointing to the data that belonged to")
                            TEXT(" back buffer #%d before the Flip"),
                            nIndex,
                            nIndex + 1);
                    }
                    else
                    {
                        Debug(
                            LOG_FAIL,
                            TEXT("The primary is not")
                            TEXT(" pointing to the data that belonged to")
                            TEXT(" back buffer #1 before the Flip"));
                    }
                    bFlipSuccess = false;
                }
            }

            // Don't forget the last case
            if(dwAfter[nBackBuffers] != dwBefore[0])
            {
                if(nBackBuffers)
                {
                    Debug(
                        LOG_FAIL,
                        TEXT("Back buffer #%d is not")
                        TEXT(" pointing to the data that belonged to")
                        TEXT(" the primary before the Flip"),
                        nBackBuffers);
                }
                else
                {
                    Debug(
                        LOG_FAIL,
                        TEXT("The primary is pointing to some")
                        TEXT(" different data than it was before the Flip"));
                }
                bFlipSuccess = false;
            }
        }

        if(!bFlipSuccess)
        {
            if (ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
                ddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
            {
                Debug(
                    LOG_DETAIL,
                    TEXT("Flip incorrectly performed on sysmem primary.")
                    TEXT("  Known bug (WINCE 1940), not failing."));
            }
            else
            {
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER
#ifdef KATANA
                    TEXT("(Keep #1925) ")
#else // KATANA
                    TEXT("(HPURAID #2758) ")
#endif // KATANA
                    TEXT("The flip wasn't performed correctly"));
                nITPR |= ITPR_FAIL;
            }
        }
    }
    __finally
    {
        // Release all surfaces we've created
        for(nIndex = nBackBuffers; nIndex >= 0; nIndex--)
        {
            if(nIndex >= countof(pDDS))
                continue;

            if(pDDS[nIndex])
            {
                Debug(
                    LOG_DETAIL,
                    TEXT("Releasing surface with index %d..."),
                    nIndex);
                pDDS[nIndex]->Release();
            }
        }
    }

    return nITPR;
}

////////////////////////////////////////////////////////////////////////////////
// Help_GetSurfaceLocations
//  Given an array of surface pointers, retrieves the location of each surface's
//  data. The first pointer is assumed to be the primary, and the rest are its
//  back buffers.
//
// Parameters:
//  pDDS            Array of surface pointers.
//  pdwData         Array that holds the data.
//  nSurfaces       Number of surfaces.
//  bWrite          true to write data to the buffers, false otherwise.
//
// Return value:
//  true if successful, false otherwise.

bool Help_GetSurfaceLocations(
    IDirectDrawSurface  *pDDS[],
    __in DWORD               *pdwData,
    int                 nSurfaces,
    bool                bWrite)
{
    LPCTSTR         pszWhen = bWrite ? TEXT("before") : TEXT("after");
    int             nIndex;
    DDSURFACEDESC   ddsd;
    HRESULT         hr;
    TCHAR           szError[50];
    bool            bSuccess = true;

    _stprintf_s(szError, TEXT("for primary, %s Flip"), pszWhen);
    for(nIndex = 0; nIndex < nSurfaces; nIndex++)
    {
        // Lock this surface
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        ddsd.lpSurface = (void *)PRESET;
        hr = pDDS[nIndex]->Lock(
            NULL,
            &ddsd,
            DDLOCK_WAITNOTBUSY,
            NULL);
        if(!CheckReturnedPointer(
            CRP_NOTINTERFACE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            ddsd.lpSurface,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            NULL,
            szError))
        {
            bSuccess = false;
            break;
        }

        // Read/write the data
        if(bWrite)
        {
            *(DWORD *)ddsd.lpSurface = pdwData[nIndex];
        }
        else
        {
            pdwData[nIndex] = *(DWORD *)ddsd.lpSurface;
        }

        // Unlock the surface
        hr = pDDS[nIndex]->Unlock(NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szUnlock,
                hr,
                NULL,
                szError);
            bSuccess = false;
        }

        // Prepare the error string for the next iteration
        _stprintf_s(
            szError,
            TEXT("for back buffer #%d, %s Flip"),
            nIndex + 1,
            pszWhen);
    }

    return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// GetSimpleFont
//  Creates a simple font resource based on the given parameters.
//
// Parameters:
//  pszTypeface     The name of the desired typeface.
//  uiSize          Average character height.
//
// Return value:
//  A handle to the newly created font resource if successful, NULL otherwise.

HFONT GetSimpleFont(LPTSTR pszTypeface, UINT uiSize)
{
    LOGFONT lf;
    memset(&lf, 0x00, sizeof(lf));

    // Setup the font
    lf.lfHeight         = uiSize;
    lf.lfWidth          = 0;
    lf.lfEscapement     = 0;
    lf.lfOrientation    = 0;
    lf.lfWeight         = FW_NORMAL;
    lf.lfItalic         = false;
    lf.lfUnderline      = false;
    lf.lfStrikeOut      = false;
    lf.lfCharSet        = ANSI_CHARSET;
    lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = VARIABLE_PITCH;
    if (pszTypeface)
    {
        _tcsncpy_s(lf.lfFaceName, pszTypeface, countof(lf.lfFaceName));
    }

    return CreateFontIndirect(&lf);
}
#endif // TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
