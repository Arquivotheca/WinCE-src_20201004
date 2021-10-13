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
//  Module: dds_blt.cpp
//          BVTs for the IDirectDrawSurface interface Blt-related functions.
//
//  Revision History:
//      12/05/1997  lblanco     Split off from dds.cpp.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "dds.h"
#pragma warning(disable : 4995)
#if TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
// Constants

#define OVERLAPPED_SLACK    20
#define NUM_BLT_ATTEMPTS    10

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef _PREFAST_
// warning 4068: unknown pragma
// This is disabled because the prefast pragma gives a warning.
#pragma warning(disable:4068)
#endif

////////////////////////////////////////////////////////////////////////////////
// Local types

enum OVERLAPPEDBLT_DIRECTION
{
    OVERLAPPEDBLT_N = 0,
    OVERLAPPEDBLT_NE,
    OVERLAPPEDBLT_E,
    OVERLAPPEDBLT_SE,
    OVERLAPPEDBLT_S,
    OVERLAPPEDBLT_SW,
    OVERLAPPEDBLT_W,
    OVERLAPPEDBLT_NW
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

ITPR    Help_TestBlt(IDDS *, bool);
ITPR    Help_TestOverlappedBlt(
            IDirectDrawSurface *,
            OVERLAPPEDBLT_DIRECTION,
            bool);
ITPR    Help_Blt(
            IDirectDraw *,
            IDirectDrawSurface *,
            IDirectDrawSurface *,
            IDirectDrawSurface *,
            bool,
            HRESULT hrExpected = S_OK,
            SPECIAL nSpecial = SPECIAL_NONE);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static LPCTSTR  g_pszDirection[] = {
    TEXT("north"),
    TEXT("norteast"),
    TEXT("east"),
    TEXT("southeast"),
    TEXT("south"),
    TEXT("southwest"),
    TEXT("west"),
    TEXT("northwest")
};

/*
////////////////////////////////////////////////////////////////////////////////
// EqualSurfaceRect
//  Compares two surface areas.
//
// Parameters:
//  pDD             Pointer to the DirectDraw object.
//  pDDSSrc         Pointer to the source surface.
//  rectSrc         Rectangle in the source surface.
//  pDDSDst         Pointer to the destination surface.
//  rectDst         Rectangle in the destination surface.
//
// Return value:
//  ITPR_PASS if the areas contain the same bits, ITPR_FAIL if they dont, or
//  ITPR_ABORT if they can't be compared.

ITPR EqualSurfaceRect(
    IDirectDraw         *pDD,
    IDirectDrawSurface  *pDDSSrc,
    RECT                rectSrc,
    IDirectDrawSurface  *pDDSDst,
    RECT                rectDst)
{
    HRESULT hr;
    int     i,
            x,
            y,
            nFailures = 0,
            nBPP;
    BYTE    *pbSrc,
            *pbDst,
            *pbSrcLine,
            *pbDstLine;
    bool    bRelease = false;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawSurface  **ppDDS = &pDDSSrc;
    DDSURFACEDESC       ddsdSrc,
                        ddsdDst;
#ifndef UNDER_CE
    DDSURFACEDESC       ddsdTemp;
#endif

#ifndef UNDER_CE
    // If either surface is a primary surface, flip it to the back buffer so we
    // can get at its bits
    ddsdTemp.dwSize = sizeof(DDSURFACEDESC);
    for(i = 0; i < 2; i++)
    {
        hr = (*ppDDS)->GetSurfaceDesc(&ddsdTemp);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetSurfaceDesc,
                hr,
                NULL,
                TEXT("in EqualSurfaceRect"));
            return ITPR_ABORT;
        }

        if((ddsdTemp.dwFlags & (DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
            DDSD_PIXELFORMAT)) !=
            (DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT))
        {
            Debug(
                LOG_ABORT,
                FAILURE_HEADER TEXT("EqualSurfaceRect(): Invalid ddsCaps,")
                TEXT(" ddpfPixelFormat, dwWidth, and/or dwHeight members of")
                TEXT(" ddsdTemp"));
            return ITPR_ABORT;
        }

        // Is the surface a primary surface?
        if(ddsdTemp.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            IDirectDrawSurface  *pDDS;
            DDSURFACEDESC       ddsd;
            RECT                rect;

            // Create an offscreen plain surface to copy the primary surface to
            memset(&ddsd, 0, sizeof(DDSURFACEDESC));
            ddsd.dwSize         = sizeof(DDSURFACEDESC);
            ddsd.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
                                  DDSD_PIXELFORMAT;
            ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
            ddsd.dwWidth        = ddsdTemp.dwWidth;
            ddsd.dwHeight       = ddsdTemp.dwHeight;
            memcpy(
                &ddsd.ddpfPixelFormat,
                &ddsdTemp.ddpfPixelFormat,
                sizeof(ddsd.ddpfPixelFormat));

            hr = pDD->CreateSurface(&ddsd, &pDDS, NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDraw,
                    c_szCreateSurface,
                    hr,
                    TEXT("off-screen plain"),
                    TEXT("in EqualSurfaceRect"));
                return ITPR_ABORT;
            }

            // Blt the primary surface to the offscreen plain surface
            rect.left   = 0;
            rect.top    = 0;
            rect.right  = ddsd.dwWidth;
            rect.bottom = ddsd.dwHeight;

            hr = pDDS->BltFast(
                0,
                0,
                *ppDDS,
                &rect,
                DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAITNOTBUSY);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szBltFast,
                    hr,
                    NULL,
                    TEXT("in EqualSurfaceRect"));
                pDDS->Release();
                return ITPR_ABORT;
            }

            // Set the primary surface pointer to the offscreen surface
            *ppDDS = pDDS;
            bRelease = true;
            break;
        }

        // Try the other surface
        ppDDS = &pDDSDst;
    }
#endif // UNDER_CE

    // Lock the source surface
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC);
    hr = pDDSSrc->Lock(
        &rectSrc,
        &ddsdSrc,
        DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAITNOTBUSY,
        NULL);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szLock,
            hr,
            TEXT("source surface"),
            TEXT("in EqualSurfaceRect"));
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Lock the destination surface
        ddsdDst.dwSize = sizeof(DDSURFACEDESC);
        hr = pDDSDst->Lock(
            &rectDst,
            &ddsdDst,
            DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAITNOTBUSY,
            NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szLock,
                hr,
                TEXT("destination surface"),
                TEXT("in EqualSurfaceRect"));
            nITPR |= ITPR_ABORT;
        }
        else
        {
            // Compare the surfaces
            pbSrcLine = (BYTE *)ddsdSrc.lpSurface;
            pbDstLine = (BYTE *)ddsdDst.lpSurface;
            nBPP = (int)(ddsdSrc.ddpfPixelFormat.dwRGBBitCount)/8;
            for(y = rectSrc.top; y < rectSrc.bottom; y++ )
            {
                pbSrc = pbSrcLine;
                pbDst = pbDstLine;
                for(x = rectSrc.left; x < rectSrc.right; x++ )
                {
                    for(i = 0; i < nBPP; i++, pbSrc++, pbDst++)
                    {
                        if(*pbSrc != *pbDst)
                        {
                            Debug(
                                LOG_FAIL,
                                TEXT("EqualSurfaceRect(): Pixel")
                                TEXT(" %d, %d (byte %d) does not match.")
                                TEXT(" Src = 0x%02X, Dst = 0x%02X"),
                                x - rectSrc.left,
                                y - rectSrc.top,
                                i,
                                *pbSrc,
                                *pbDst);
                            nITPR |= ITPR_FAIL;
                            if(++nFailures > MAX_BLT_FAILURES)
                            {
                                // Force an exit from all loops
                                i = 1000000;
                                x = 1000000;
                                y = 1000000;
                            }
                        }
                    }
                }
                pbSrcLine += ddsdSrc.lPitch;
                pbDstLine += ddsdDst.lPitch;
            }

            // Unlock the destination surface
            hr = pDDSDst->Unlock((LPVOID)ddsdDst.lpSurface);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szUnlock,
                    hr,
                    TEXT("destination surface"),
                    TEXT("in EqualSurfaceRect"));
                nITPR |= ITPR_ABORT;
            }
        }

        // Unlock the source surface
        hr = pDDSSrc->Unlock((LPVOID)ddsdSrc.lpSurface);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szUnlock,
                hr,
                TEXT("source surface"),
                TEXT("in EqualSurfaceRect"));
            nITPR |= ITPR_ABORT;
        }
    }

    // Release the offscreen surface (if created) or the back buffer
    if(bRelease)
    {
        (*ppDDS)->Release();
    }

    return nITPR;
}
*/

////////////////////////////////////////////////////////////////////////////////
// Help_Blt
//  Helper function to the IDirectDrawSurface::Blt method test.
//
// Parameters:
//  pDD             Pointer to the DirectDraw object.
//  pDDSPrimary     Pointer to the primary surface.
//  pDDSSrc         Pointer to the source surface.
//  pDDSDst         Pointer to the destination surface.
//  bFast           true to perform a BltFast, false for a regular Blt.
//  hrExpected      The expected result of the operation.
//  nSpecial        Special handling code. If zero, no special handling is
//                  required (the default). Otherwise, this code says what has
//                  to be done differently to report a particular bug ID.
//
// Return value:
//  ITPR_PASS if the test passes, ITPR_FAIL if it fails, or ITPR_ABORT if it
//  cannot run to completion.

ITPR Help_Blt(
    IDirectDraw         *pDD,
    IDirectDrawSurface  *pDDSPrimary,
    IDirectDrawSurface  *pDDSSrc,
    IDirectDrawSurface  *pDDSDst,
    bool                bFast,
    HRESULT             hrExpected,
    SPECIAL             nSpecial)
{
    DDCAPS  ddcHAL;
    RECT    srcRect,
            destRect;
    DWORD   dwWidth,
            dwHeight;
    DDBLTFX DDBltFx;
    int     i;
    LPCTSTR pszFormat;
    HRESULT hr;
    bool    bCanStretch= false;
    ITPR    nITPR = ITPR_PASS,
            nITPRTemp;
    DWORD   dwCookie;
    UINT    nBugID;

    DDSURFACEDESC   ddsdSrc,
                    ddsdDst;

#if QAHOME_INVALIDPARAMS
    HRESULT hrStretch;
#endif // QAHOME_INVALIDPARAMS

    memset(&ddsdSrc, 0, sizeof(DDSURFACEDESC));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC);
    memset(&ddsdDst, 0, sizeof(DDSURFACEDESC));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC);

    if(!bFast)
    {
        // Determine if we'll be able to perform stretch operations
        memset(&ddcHAL, 0, sizeof(ddcHAL));
        ddcHAL.dwSize = sizeof(ddcHAL);
        hr = pDD->GetCaps(&ddcHAL, NULL);
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szIDirectDraw, c_szGetCaps, hr);
            nITPR |= ITPR_ABORT;
            // Assume no stretching capabilities
            bCanStretch = false;
        }
        else
        {
            // See what the capabilities say about stretching
            bCanStretch = true;
//                          ((ddcHAL.dwFXCaps & (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY))
//                                           == (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY)) ||
//                          ((ddcHEL.dwFXCaps & (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY))
//                                           == (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY));
        }
        memset(&DDBltFx, 0, sizeof(DDBltFx));
        DDBltFx.dwSize = sizeof(DDBltFx);
        DDBltFx.dwROP = SRCCOPY;
    }

    // Gather information about the source and destination surfaces
    hr = pDDSSrc->GetSurfaceDesc(&ddsdSrc);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szGetSurfaceDesc,
            hr,
            NULL,
            TEXT("for source surface"));
        return nITPR | ITPR_ABORT;
    }

    hr = pDDSDst->GetSurfaceDesc(&ddsdDst);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szGetSurfaceDesc,
            hr,
            NULL,
            TEXT("for destination surface"));
        return nITPR | ITPR_ABORT;
    }

    dwWidth = (ddsdSrc.dwWidth < ddsdDst.dwWidth) ?
              ddsdSrc.dwWidth : ddsdDst.dwWidth - 1;
    dwHeight = (ddsdSrc.dwHeight < ddsdDst.dwHeight) ?
               ddsdSrc.dwHeight : ddsdDst.dwHeight - 1;

    // Write some data to the source surface
    dwCookie = rand();
    nITPRTemp = FillSurface(dwCookie, pDDSSrc);
    nITPR |= nITPRTemp;
    if(nITPRTemp != ITPR_PASS)
        return nITPR;

    for(i = 0; i < NUM_BLT_ATTEMPTS; i++ )
    {
        srcRect.left    = rand() % (ddsdSrc.dwWidth - 1);
        srcRect.top     = rand() % (ddsdSrc.dwHeight - 1);
        srcRect.right   = srcRect.left + (rand()%min(ddsdSrc.dwWidth -
                          srcRect.left - 1, dwWidth)) + 1;
        srcRect.bottom  = srcRect.top + (rand()%min(ddsdSrc.dwHeight -
                          srcRect.top - 1, dwHeight)) + 1;
        destRect.left   = rand()%(ddsdDst.dwWidth -
                          (srcRect.right - srcRect.left));
        destRect.top    = rand()%(ddsdDst.dwHeight -
                          (srcRect.bottom - srcRect.top));
        destRect.right  = destRect.left + (srcRect.right - srcRect.left);
        destRect.bottom = destRect.top + (srcRect.bottom - srcRect.top);

        // Log a line with the coordinates of the Blt or BltFast operation
        if(bFast)
        {
            pszFormat = TEXT("Performing %s from (%d, %d), %dx%d to (%d, %d)");
        }
        else
        {
            pszFormat = TEXT("Performing %s from (%d, %d), %dx%d")
                TEXT(" to (%d, %d), %dx%d");
        }
        Debug(
            LOG_DETAIL,
            pszFormat,
            bFast ? c_szBltFast : c_szBlt,
            srcRect.left,
            srcRect.top,
            srcRect.right - srcRect.left,
            srcRect.bottom - srcRect.top,
            destRect.left,
            destRect.top,
            destRect.right - destRect.left,
            destRect.bottom - destRect.top);
        hr = pDDSDst->Blt(
                &destRect,
                pDDSSrc,
                &srcRect,
                DDBLT_ROP | DDBLT_WAITNOTBUSY, &DDBltFx);
        if((hrExpected == S_OK && FAILED(hr)) ||
           (hrExpected != S_OK && hr != hrExpected))
        {

#ifdef KATANA
            nBugID = 0;
#else // not KATANA
            // a-shende(07.19.1999): identify known bugs
            if(SPECIAL_BUG_FLAG == nSpecial)
            {
                nBugID = 1921;
            }
            else
            {
                nBugID = 0;
            }
#endif // not KATANA
            Report(
                hrExpected == S_OK ? RESULT_FAILURE : RESULT_UNEXPECTED,
                c_szIDirectDrawSurface,
                bFast ? c_szBltFast : c_szBlt,
                hr,
                NULL,
                NULL,
                hrExpected,
                RAID_HPURAID,
                nBugID);
#undef BUGID
            return nITPR | ITPR_FAIL;
        }

        // Verify that the source bits weren't corrupted
        nITPRTemp = VerifySurfaceContents(
            dwCookie,
            pDDSSrc,
            &srcRect,
            &srcRect);
        nITPR |= nITPRTemp;
        if(nITPRTemp != ITPR_PASS)
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s::%s corrupted the source surface"),
                c_szIDirectDrawSurface,
                bFast ? c_szBltFast : c_szBlt);
        }

        // Destination surface verification needs to be heuristic; until this
        // is possible, we'll need to comment this code out for Katana
#ifndef KATANA
        // Verify the bits were copied correctly
        if(hrExpected == S_OK)
        {
//          nITPRTemp = EqualSurfaceRect(
//              pDD,
//              pDDSSrc,
//              srcRect,
//              pDDSDst,
//              destRect);
            nITPRTemp = VerifySurfaceContents(
                dwCookie,
                pDDSDst,
                &destRect,
                &srcRect);
            nITPR |= nITPRTemp;
            if(nITPRTemp == ITPR_FAIL)
            {
                LPCTSTR pszBugID;
                if(nSpecial == SPECIAL_BLT_VIDEO_TO_PRIMARY)
                {
#ifdef KATANA
                    pszBugID = TEXT("(Keep #3532) ");
#else // KATANA
                    pszBugID = TEXT("");
#endif // KATANA
                }
                else if(nSpecial == SPECIAL_BLT_TO_VIDEO)
                {
#ifdef KATANA
                    pszBugID = TEXT("(Keep #6656) ");
#else // KATANA
                    pszBugID = TEXT("");
#endif // KATANA
                }
                else
                {
                    pszBugID = TEXT("");
                }
                Debug(
                    LOG_FAIL,
                    FAILURE_HEADER TEXT("%s%s::%s reported success, but")
                    TEXT(" the bits were incorrectly copied"),
                    pszBugID,
                    c_szIDirectDrawSurface,
                    bFast ? c_szBltFast : c_szBlt);
            }
        }
#endif // !KATANA

#if QAHOME_INVALIDPARAMS
        if(!bFast)
#else // QAHOME_INVALIDPARAMS
        if(!bFast && bCanStretch)
#endif // QAHOME_INVALIDPARAMS
        {
            // Stretch Blt
            srcRect.left    = rand() % (ddsdSrc.dwWidth - 1);
            srcRect.top     = rand() % (ddsdSrc.dwHeight - 1);
            srcRect.right   = srcRect.left + (rand()%
                              (ddsdSrc.dwWidth - srcRect.left - 1)) + 1;
            srcRect.bottom  = srcRect.top + (rand()%
                              (ddsdSrc.dwHeight - srcRect.top - 1)) + 1;

            do
            {
                destRect.left   = rand() % (ddsdDst.dwWidth - 1);
                destRect.top    = rand() % (ddsdDst.dwHeight - 1);
                destRect.right  = destRect.left +
                                  (rand() % (ddsdDst.dwWidth - destRect.left - 1)) + 1;
                destRect.bottom = destRect.top +
                                  (rand() % (ddsdDst.dwHeight - destRect.top - 1)) + 1;

            } while(destRect.left   == srcRect.left  &&
                    destRect.top    == srcRect.top   &&
                    destRect.right  == srcRect.right &&
                    destRect.bottom == srcRect.bottom);

#if QAHOME_INVALIDPARAMS
            hrStretch = bCanStretch ? hrExpected : DDERR_NOSTRETCHHW;
#else // QAHOME_INVALIDPARAMS
#define hrStretch hrExpected
#endif // QAHOME_INVALIDPARAMS

            // Log the coordinates
            Debug(
                LOG_DETAIL,
                TEXT("Performing stretch Blt from (%d, %d), %dx%d")
                TEXT(" to (%d, %d), %dx%d"),
                srcRect.left,
                srcRect.top,
                srcRect.right - srcRect.left,
                srcRect.bottom - srcRect.top,
                destRect.left,
                destRect.top,
                destRect.right - destRect.left,
                destRect.bottom - destRect.top);

            // Do the Blt
            hr = pDDSDst->Blt(
                    &destRect,
                    pDDSSrc,
                    &srcRect,
                    DDBLT_ROP | DDBLT_WAITNOTBUSY,
                    &DDBltFx);

            if((hrStretch == S_OK && FAILED(hr)) ||
               (hrStretch != S_OK && hr != hrStretch))
            {

#ifdef KATANA
#define BUGID   0
#else // not KATANA
#define BUGID   1920
#endif // not KATANA
                Report(
                    hrStretch == S_OK ? RESULT_FAILURE : RESULT_UNEXPECTED,
                    c_szIDirectDrawSurface,
                    c_szBlt,
                    hr,
                    NULL,
                    NULL,
                    hrStretch,
                    RAID_HPURAID,
                    BUGID);
#undef BUGID
                return nITPR | ITPR_FAIL;
            }

            // If we used a macro to replace hrStretch by hrExpected, delete
            // that macro now
#if QAHOME_INVALIDPARAMS
#undef hrStretch
#endif // QAHOME_INVALIDPARAMS
        }
    }

    return nITPR;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Blt
//  Tests the IDirectDrawSurface::Blt method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_Blt(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    INT         nResult;
    IDDS        *pIDDS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = ITPR_PASS;

        if(!InitDirectDrawSurface(pIDDS))
            return TPRFromITPR(g_tprReturnVal);

        // Regular surface-to-surface Blt tests
        nResult |= Help_TestBlt(pIDDS, false);

        // Overlapping Blt tests
        switch(Help_DDSIterate(
            DDSI_TESTBACKBUFFER,
            Test_IDDS_Blt,
            lpFTE))
        {
        case TPR_FAIL:
            nResult |= ITPR_FAIL;
            break;
        case TPR_ABORT:
            nResult |= ITPR_ABORT;
            break;
        case TPR_SKIP:
            nResult |= ITPR_SKIP;
            break;
        }

        if(nResult == ITPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szBlt);

        nResult = TPRFromITPR(nResult);
    }
    else
    {
        IDirectDrawSurface  *pDDS = pDDSTI->m_pDDS;
        nResult = ITPR_PASS;

        if(pDDS)
        {
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_N,  false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_NE, false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_E,  false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_SE, false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_S,  false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_SW, false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_W,  false);
            nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_NW, false);
        }
    }

    return nResult;
}

#if 0
////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_BltBatch
//  Tests the IDirectDrawSurface::BltBatch method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_BltBatch(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS        *pIDDS;
    HRESULT     hr;
    ITPR        nITPR = ITPR_PASS;
    RECT        rcSrc = {  10,  10, 100, 100 },
                rcDst = { 110, 110, 200, 200 };
    DDBLTFX     ddbltfx;
    DDBLTBATCH  ddbltbatch;
    
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);
        
    // Set up the bltbatch structure
    ddbltbatch.lprDest   = &rcDst;
    ddbltbatch.lpDDSSrc  = pIDDS->m_pDDSSysMem;
    ddbltbatch.lprSrc    = &rcSrc;
    ddbltbatch.dwFlags   = DDBLT_DDFX;
    ddbltbatch.lpDDBltFx = &ddbltfx;
    
    // Set up the bltfx structure
    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

    // Make the call
#ifdef QAHOME_INVALIDPARAMS
    INVALID_CALL_E_RAID(
        pIDDS->m_pDDSPrimary->BltBatch(&ddbltbatch, 1, 0),
        c_szIDirectDrawSurface,
        c_szBltBatch,
        NULL,
        DDERR_UNSUPPORTED,
        IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
        IFKATANAELSE(0, 7414));
#endif // QAHOME_INVALIDPARAMS

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szBltBatch);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_BltFast
//  Tests the IDirectDrawSurface::BltFast method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_BltFast(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    INT         nResult;
    IDDS        *pIDDS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = ITPR_PASS;

        if(!InitDirectDrawSurface(pIDDS))
            return TPRFromITPR(g_tprReturnVal);

        // Regular surface-to-surface Blt tests
        nResult |= Help_TestBlt(pIDDS, true);

        // Overlapping Blt tests
        switch(Help_DDSIterate(
            DDSI_TESTBACKBUFFER,
            Test_IDDS_BltFast,
            lpFTE))
        {
        case TPR_FAIL:
            nResult |= ITPR_FAIL;
            break;
        case TPR_ABORT:
            nResult |= ITPR_ABORT;
            break;
        case TPR_SKIP:
            nResult |= ITPR_SKIP;
            break;
        }

        if(nResult == ITPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szBltFast);

        nResult = TPRFromITPR(nResult);
    }
    else
    {
        IDirectDrawSurface  *pDDS = pDDSTI->m_pDDS;
        nResult = ITPR_PASS;

        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_N,  true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_NE, true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_E,  true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_SE, true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_S,  true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_SW, true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_W,  true);
        nResult |= Help_TestOverlappedBlt(pDDS, OVERLAPPEDBLT_NW, true);
    }

    return nResult;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_GetBltStatus
//  Tests the IDirectDrawSurface::GetBltStatus method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_GetBltStatus(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDSTESTINFO *pDDSTI;
    IDDS        *pIDDS;
    RECT        rcSource;
    HRESULT     hr;
    RECT        rcDest;
    TCHAR       szConfig[128];
    INT         nResult = ITPR_PASS;

    IDirectDrawSurface  *pDDSSource;
    DDSURFACEDESC       *pDDSDSource;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    pDDSTI = (DDSTESTINFO *)lpFTE->dwUserData;
    if(!pDDSTI)
    {
        nResult = Help_DDSIterate(
            DDSI_TESTBACKBUFFER,
            Test_IDDS_GetBltStatus,
            lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szGetBltStatus);
    }
    else if(pDDSTI->m_pDDS)
    {
        if(!InitDirectDrawSurface(pIDDS))
            return TPRFromITPR(g_tprReturnVal);

        if(pIDDS->m_pDDSSysMem)
        {
            pDDSSource = pIDDS->m_pDDSSysMem;
            pDDSDSource = &pIDDS->m_ddsdSysMem;
        }
        else if(pIDDS->m_pDDSVidMem)
        {
            pDDSSource = pIDDS->m_pDDSVidMem;
            pDDSDSource = &pIDDS->m_ddsdVidMem;
        }
        else
        {
            Debug(
                LOG_ABORT,
                TEXT("This test requires an off-screen surface, which is")
                TEXT(" unavailable"));
            return ITPR_ABORT;
        }

        hr = pDDSTI->m_pDDS->GetBltStatus(DDGBS_CANBLT);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetBltStatus,
                hr,
                TEXT("DDGBS_CANBLT"),
                TEXT("before a Blt"));
            nResult |= ITPR_FAIL;
        }

        // Force the upper left of our rectangle to be at least one pixel
        // from the botom left of the screen.
        rcSource.left   = rand()%(pDDSTI->m_pddsd->dwWidth - 1);
        rcSource.top    = rand()%(pDDSTI->m_pddsd->dwHeight - 1);
        // Get a valid lower right corner
        rcSource.right  = rcSource.left +
                          (rand()%(pDDSTI->m_pddsd->dwWidth - rcSource.left - 1)) + 1;
        rcSource.bottom = rcSource.top +
                          (rand()%(pDDSTI->m_pddsd->dwHeight - rcSource.top - 1)) + 1;

        rcDest.left = rand()%(pDDSTI->m_pddsd->dwWidth -
                 (rcSource.right - rcSource.left));
        rcDest.top = rand()%(pDDSTI->m_pddsd->dwHeight -
                 (rcSource.bottom - rcSource.top));
        rcDest.bottom = rcDest.top + rcSource.bottom - rcSource.top;
        rcDest.right = rcDest.left + rcSource.right - rcSource.left;
        hr = pDDSTI->m_pDDS->Blt(
            &rcDest,
            pDDSSource,
            &rcSource,
            DDBLT_WAITNOTBUSY,
            NULL);
        if(FAILED(hr))
        {
            _stprintf_s(
                szConfig,
                TEXT("from (%d, %d), %dx%d to (%d, %d)"),
                rcSource.left,
                rcSource.top,
                rcSource.right - rcSource.left,
                rcSource.bottom - rcSource.top,
                rcDest.left,
                rcDest.top);
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szBltFast,
                hr,
                szConfig);
            nResult |= ITPR_ABORT;
        }
        else
        {
            do
            {
                hr = pDDSTI->m_pDDS->GetBltStatus(DDGBS_ISBLTDONE);
            } while(hr == DDERR_WASSTILLDRAWING);

            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szGetBltStatus,
                    hr,
                    TEXT("DDGBS_ISBLTDONE"));
                nResult |= ITPR_FAIL;
            }
        }
    }

    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Help_TestBlt
//  Performs Blt or BltFast operations between various surface types.
//
// Parameters:
//  pIDDS           Pointer to a structure containing the various surfaces to
//                  test.
//  bFast           true to perform BltFast, false for Blt.
//
// Return value:
//  ITPR_PASS if all tests pass, ITPR_FAIL if at least one fails, or ITPR_ABORT
//  if the tests can't be completed.

ITPR Help_TestBlt(IDDS *pIDDS, bool bFast)
{
    ITPR    nITPR = ITPR_PASS;

    if(pIDDS->m_pDDSSysMem)
    {
        DebugBeginLevel(
            0,
            TEXT("Testing off-screen surface in system memory to primary"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSSysMem,
            pIDDS->m_pDDSPrimary,
            bFast,
            S_OK,
            SPECIAL_BLT_TO_VIDEO);
        DebugEndLevel(TEXT(""));

        DebugBeginLevel(
            0,
            TEXT("Testing primary to off-screen surface in system memory"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSSysMem,
            bFast);
        DebugEndLevel(TEXT(""));
    }

    if(pIDDS->m_pDDSVidMem)
    {
#if 0//def UNDER_CE
        // This procedure should not be necessary on the Set 5

        RECT    rc = { 0, 0, 10, 10 };
        // Because the first frame is rendered incorrectly on the Set 4, we need
        // to do a dummy Blt before running the real test. This causes the first
        // frame to be done with, allowing the system a chance to perform
        // correctly for the test. We must also flip in order for this to work
        if(bFast)
        {
            pIDDS->m_pDDSPrimary->BltFast(
                    0,
                    0,
                    pIDDS->m_pDDSVidMem,
                    &rc,
                    DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAITNOTBUSY);
        }
        else
        {
            pIDDS->m_pDDSPrimary->Blt(
                    &rc,
                    pIDDS->m_pDDSVidMem,
                    &rc,
                    DDBLT_WAITNOTBUSY,
                    NULL);
        }
        pIDDS->m_pDDSPrimary->Flip(NULL, DDFLIP_WAITNOTBUSY);
#endif // UNDER_CE
        DebugBeginLevel(
            0,
            TEXT("Testing off-screen surface in video memory to primary"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSVidMem,
            pIDDS->m_pDDSPrimary,
            bFast,
            S_OK,
            SPECIAL_BLT_VIDEO_TO_PRIMARY);
        DebugEndLevel(TEXT(""));

        DebugBeginLevel(
            0,
            TEXT("Testing primary to off-screen surface in video memory"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSVidMem,
            bFast,
            S_OK,
            SPECIAL_BLT_TO_VIDEO);
        DebugEndLevel(TEXT(""));
    }

    if(pIDDS->m_pDDSSysMem && pIDDS->m_pDDSVidMem)
    {
        DebugBeginLevel(
            0,
            TEXT("Testing off-screen surface in system memory to")
            TEXT(" off-screen surface in video memory"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSSysMem,
            pIDDS->m_pDDSVidMem,
            bFast,
            S_OK,
            SPECIAL_BLT_TO_VIDEO);
        DebugEndLevel(TEXT(""));

        DebugBeginLevel(
            0,
            TEXT("Testing off-screen surface in video memory to")
            TEXT(" off-screen surface in system memory"));

        // a-shende(07.19.1999): use special flag to mark this bug
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSVidMem,
            pIDDS->m_pDDSSysMem,
            bFast,
            S_OK,
            SPECIAL_BUG_FLAG);
        DebugEndLevel(TEXT(""));
    }

#if QAHOME_OVERLAY
    if(pIDDS->m_bOverlaysSupported)
    {
        DebugBeginLevel(
            0,
            TEXT("Testing overlay #1 to primary"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSOverlay1,
            pIDDS->m_pDDSPrimary,
            bFast);
        DebugEndLevel(TEXT(""));

        DebugBeginLevel(
            0,
            TEXT("Testing primary to overlay #1"));
        nITPR |= Help_Blt(
            pIDDS->m_pDD,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSPrimary,
            pIDDS->m_pDDSOverlay1,
            bFast);
        DebugEndLevel(TEXT(""));
    }
#endif // QAHOME_OVERLAY

#if TEST_ZBUFFER
#if QAHOME_INVALIDPARAMS
    DebugBeginLevel(0, TEXT("Testing Z buffer to primary"));
    nITPR |= Help_Blt(
        pIDDS->m_pDD,
        pIDDS->m_pDDSPrimary,
        pIDDS->m_pDDSZBuffer,
        pIDDS->m_pDDSPrimary,
        bFast,
        DDERR_UNSUPPORTED);
    DebugEndLevel(TEXT(""));

    DebugBeginLevel(0, TEXT("Testing primary to Z buffer"));
    nITPR |= Help_Blt(
        pIDDS->m_pDD,
        pIDDS->m_pDDSPrimary,
        pIDDS->m_pDDSPrimary,
        pIDDS->m_pDDSZBuffer,
        bFast,
        DDERR_UNSUPPORTED);
    DebugEndLevel(TEXT(""));
#endif // QAHOME_INVALIDPARAMS
#endif // TEST_ZBUFFER

    return nITPR;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Get_SetColorKey
//  Tests the IDirectDrawSurface::GetColorKey and
//  IDirectDrawSurface::SetColorKey methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message - dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS_Get_SetColorKey(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS        *pIDDS;
    int         index,
                nSurface,
                iKey[6] = {0, 1, 2, 0, 0, 0},
                nCK;
    DDCOLORKEY  ddck1,
                ddck2;
    HRESULT     hr;
    ITPR        nITPR = ITPR_PASS;
    bool        fZBuffer = false;
    bool        fValidCombo;
#if QAHOME_INVALIDPARAMS
    HRESULT     hrExpected;
    int         nBugID;
#endif // QAHOME_INVALIDPARAMS
    DWORD       dwCK[] = {
#if QAHOME_OVERLAY
        DDCKEY_SRCOVERLAY,
        DDCKEY_DESTOVERLAY,
#endif // QAHOME_OVERLAY
#if QAHOME_INVALIDPARAMS
        DDCKEY_DESTBLT,
#endif // QAHOME_INVALIDPARAMS
        DDCKEY_SRCBLT
    };
    DWORD       dwSCK[] = {
#if QAHOME_OVERLAY
        SET_COLORKEY_SRCOVERLAY,
        SET_COLORKEY_DESTOVERLAY,
#endif // QAHOME_OVERLAY
#if QAHOME_INVALIDPARAMS
        SET_COLORKEY_DESTBLT,
#endif // QAHOME_INVALIDPARAMS
        SET_COLORKEY_SRCBLT
    };
    LPCTSTR     pszCKName[] = {
#if QAHOME_OVERLAY
        TEXT("DDCKEY_SRCOVERLAY"),
        TEXT("DDCKEY_DESTOVERLAY"),
#endif // QAHOME_OVERLAY
#if QAHOME_INVALIDPARAMS
        TEXT("DDCKEY_DESTBLT"),
#endif // QAHOME_INVALIDPARAMS
        TEXT("DDCKEY_SRCBLT")
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    IDirectDrawSurface  *pDDS[] = {
#if TEST_ZBUFFER
#if QAHOME_INVALIDPARAMS
        pIDDS->m_pDDSZBuffer,
#endif // QAHOME_INVALIDPARAMS
#endif // TEST_ZBUFFER
        pIDDS->m_pDDSPrimary,
        pIDDS->m_pDDSSysMem,
        pIDDS->m_pDDSVidMem
#if QAHOME_OVERLAY
        ,pIDDS->m_pDDSOverlay1,
        pIDDS->m_pDDSOverlay2
#endif // QAHOME_OVERLAY
    };
    LPCTSTR             pszName[] = {
#if TEST_ZBUFFER
#if QAHOME_INVALIDPARAMS
        TEXT("for Z buffer"),
#endif // QAHOME_INVALIDPARAMS
#endif // TEST_ZBUFFER
        TEXT("for primary"),
        TEXT("for system memory"),
        TEXT("for video memory")
#if QAHOME_OVERLAY
        ,TEXT("for overlay #1"),
        TEXT("for overlay #2")
#endif // QAHOME_OVERLAY
    };

    int nSurfaceCount = countof(pDDS);
#if QAHOME_OVERLAY
    // If we support overlays, it may still be that the hardware doesn't. In
    // that case the overlay entries still exist in the above arrays, but
    // we move the upper bound down so we never use them.
    if(!pIDDS->m_bOverlaysSupported)
    {
        nSurfaceCount -= 2;
    }
#endif // QAHOME_OVERLAY

    for(index = 0; index < 3; index++ )
    {
        iKey[index + 3] = -iKey[index] - 1 + (int)pow(
                2.0,
                pIDDS->m_ddsdPrimary.ddpfPixelFormat.dwRGBBitCount);
    }

    for(nSurface = 0; nSurface < nSurfaceCount; nSurface++)
    {
        if(!pDDS[nSurface])
        {
            continue;
        }
#if TEST_ZBUFFER
        fZBuffer = (0 == nSurface);
#endif
        for(index = 0; index < 7; index++ )
        {
            for(nCK = 0; nCK < countof(dwCK); nCK++)
            {
                if(index == 6)
                {
                    ddck1.dwColorSpaceLowValue = dwSCK[nCK];
                    ddck1.dwColorSpaceHighValue = dwSCK[nCK];
                }
                else
                {
#pragma prefast(suppress:201, "iKey is never accessed with index==6 (see above)")
                    ddck1.dwColorSpaceLowValue = iKey[index];
#pragma prefast(suppress:201, "iKey is never accessed with index==6 (see above)")
                    ddck1.dwColorSpaceHighValue = iKey[index];
                }

                __try
                {
                    hr = pDDS[nSurface]->SetColorKey(dwCK[nCK], &ddck1);
#if QAHOME_INVALIDPARAMS
                    // If this was the Z Buffer, we expect failure
                    if(dwCK[nCK] == DDCKEY_DESTBLT)
                    {
                        hrExpected = DDERR_NOCOLORKEYHW;
                        nBugID = 0;
                        if(hr != hrExpected)
                        {
                            Report(
                                RESULT_UNEXPECTED,
                                c_szIDirectDrawSurface,
                                c_szSetColorKey,
                                hr,
                                pszCKName[nCK],
                                pszName[nSurface],
                                hrExpected);
                            nITPR |= ITPR_FAIL;
                        }

                        __try
                        {
                            // For GetColorKey, we expect a different error than
                            // for SetColorKey, when DDCKEY_DESTBLT is used.
                            if(!fZBuffer)
                            {
                                hrExpected = DDERR_COLORKEYNOTSET;
                            }

                            hr = pDDS[nSurface]->GetColorKey(dwCK[nCK], &ddck2);
                            if(hr != hrExpected)
                            {
                                Report(
                                    RESULT_UNEXPECTED,
                                    c_szIDirectDrawSurface,
                                    c_szGetColorKey,
                                    hr,
                                    pszCKName[nCK],
                                    pszName[nSurface],
                                    hrExpected,
                                    IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
                                    nBugID);
                                nITPR |= ITPR_FAIL;
                            }
                        }
                        __except(EXCEPTION_EXECUTE_HANDLER)
                        {
                            Report(
                                RESULT_EXCEPTION,
                                c_szIDirectDrawSurface,
                                c_szGetColorKey,
                                0,
                                pszCKName[nCK],
                                pszName[nSurface]);
                            nITPR |= ITPR_FAIL;
                        }
                    }
                    else
#endif // QAHOME_INVALIDPARAMS
                    if(FAILED(hr))
                    {
                        // Skip any known invalid combinations
                        fValidCombo = true;
                        // DESTOVERLAY is only valid for the primary surface, and
                        // only if the hardware supports overlays
                        if(DDCKEY_DESTOVERLAY == dwCK[nCK])
                        {
                            fValidCombo = ((pDDS[nSurface] == pIDDS->m_pDDSPrimary) &&
                                            pIDDS->m_bOverlaysSupported) &&
                                            ((pIDDS->m_ddcHAL.dwOverlayCaps & DDOVERLAYCAPS_CKEYDEST) ||
                                              (pIDDS->m_ddcHAL.dwOverlayCaps & DDOVERLAYCAPS_CKEYBOTH));
                        }
#if QAHOME_OVERLAY
                        // SRCOVERLAY is only valid for overlay surfaces in video mem.
                        else if(DDCKEY_SRCOVERLAY == dwCK[nCK])
                        {
                            fValidCombo = false;
                            if ((((pDDS[nSurface] == pIDDS->m_pDDSOverlay1) &&
                                  (pIDDS->m_ddsdOverlay1.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)) ||
                                 ((pDDS[nSurface] == pIDDS->m_pDDSOverlay2) &&
                                  (pIDDS->m_ddsdOverlay2.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))) &&
                                ((pIDDS->m_ddcHAL.dwOverlayCaps & DDOVERLAYCAPS_CKEYSRC) ||
                                  (pIDDS->m_ddcHAL.dwOverlayCaps & DDOVERLAYCAPS_CKEYBOTH)))
                                  
                            {
                                fValidCombo = true;
                            }
                        }
#endif // QAHOME_OVERLAY

                        if(fValidCombo)
                        {
                            Report(
                                RESULT_FAILURE,
                                c_szIDirectDrawSurface,
                                c_szSetColorKey,
                                hr,
                                pszCKName[nCK],
                                pszName[nSurface],
                                0,
                                IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
                                IFKATANAELSE(2835, 0));
                            nITPR |= ITPR_FAIL;
                        }
                    }
                    else
                    {
                        __try
                        {
                            hr = pDDS[nSurface]->GetColorKey(dwCK[nCK], &ddck2);
                            if(FAILED(hr))
                            {
                                Report(
                                    RESULT_FAILURE,
                                    c_szIDirectDrawSurface,
                                    c_szGetColorKey,
                                    hr,
                                    pszCKName[nCK],
                                    pszName[nSurface],
                                    0,
                                    IFKATANAELSE(RAID_KEEP, RAID_HPURAID),
                                    IFKATANAELSE(2835, 6930));
                                nITPR |= ITPR_FAIL;
                            }
                            else
                            {
                                if(ddck1.dwColorSpaceLowValue !=
                                   ddck2.dwColorSpaceLowValue ||
                                   ddck1.dwColorSpaceHighValue !=
                                   ddck2.dwColorSpaceHighValue)
                                {
                                    Debug(
                                        LOG_FAIL,
                                        FAILURE_HEADER TEXT("%s::%s was")
                                        TEXT(" expected to  return color key")
                                        TEXT(" 0x%08x%08x after call to %s::%s")
                                        TEXT("(%s) (returned 0x%08x%08x)"),
                                        c_szIDirectDrawSurface,
                                        c_szGetColorKey,
                                        ddck1.dwColorSpaceHighValue,
                                        ddck1.dwColorSpaceLowValue,
                                        c_szIDirectDrawSurface,
                                        c_szSetColorKey,
                                        pszCKName[nCK],
                                        ddck2.dwColorSpaceHighValue,
                                        ddck2.dwColorSpaceLowValue);
                                    nITPR |= ITPR_FAIL;
                                }
                            }
                        }
                        __except(EXCEPTION_EXECUTE_HANDLER)
                        {
                            Report(
                                RESULT_EXCEPTION,
                                c_szIDirectDrawSurface,
                                c_szGetColorKey,
                                0,
                                pszCKName[nCK],
                                pszName[nSurface]);
                            nITPR |= ITPR_FAIL;
                        }
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    Report(
                        RESULT_EXCEPTION,
                        c_szIDirectDrawSurface,
                        c_szGetColorKey,
                        0,
                        pszCKName[nCK],
                        pszName[nSurface]);
                    nITPR |= ITPR_FAIL;
                }
            }
        }
    }

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDrawSurface,
            TEXT("GetColorKey and SetColorKey"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS_Get_SetPalette
//  Tests the IDirectDrawSurface::GetPalette and IDirectDrawSurface::SetPalette
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

TESTPROCAPI Test_IDDS_Get_SetPalette(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    
    ITPR    nITPR = ITPR_PASS;
    
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    return TPR_SKIP;

}

////////////////////////////////////////////////////////////////////////////////
// Help_TestOverlappedBlt
//  Tests Blt's from different overlapping regions in a single surface.
//
// Parameters:
//  pDDS            The surface pointer.
//  nDirection      The direction vector from the source to the destination
//                  rectangles.
//  bFast           true to use BltFast, false to use Blt.
//
// Return value:
//  ITPR_PASS if the test passes, ITPR_FAIL if it fails, or ITPR_ABORT if it
//  cannot be run.

ITPR Help_TestOverlappedBlt(
    IDirectDrawSurface      *pDDS,
    OVERLAPPEDBLT_DIRECTION nDirection,
    bool                    bFast)
{
    HRESULT hr;
    void    *pLine,
            *pPixel;
    UINT    x, y,
            nPixelSize;
    DWORD   dwValue = 0,
            dwExpectedValue,
            nPixelMask;
    RECT    rcSrc,
            rcDst;
    int     nOffsetX,
            nOffsetY;
    ITPR    nITPR = ITPR_PASS;
    UINT    nBugID;

    DDSURFACEDESC   ddsd;

    // The following macro defines the function that maps a pixel coordinate to
    // its expected value.
#define PIXEL_VALUE(a, b)   DWORD((DWORD(a)*DWORD(a) + DWORD(b)*5) & nPixelMask)

    // Log detail
    Debug(
        LOG_DETAIL,
        TEXT("Testing overlapped Blt%s going towards %s"),
        bFast ? TEXT("Fast") : TEXT(""),
        g_pszDirection[nDirection]);

    // Lock the surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    hr = pDDS->Lock(
        NULL,
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
            bFast ? TEXT("before BltFast") : TEXT("before Blt"));
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Calculate the size of a pixel in bytes and the bit mask. Note that
        // we only support 8, 16 and 32bpp surfaces
        if(ddsd.ddpfPixelFormat.dwRGBBitCount != 8 &&
           ddsd.ddpfPixelFormat.dwRGBBitCount != 16 &&
           ddsd.ddpfPixelFormat.dwRGBBitCount != 32)
        {
            Debug(
                LOG_ABORT,
                TEXT("We can't run the overlapping Blt%s test for a %dbpp")
                TEXT(" surface"),
                bFast ? TEXT("Fast") : TEXT(""),
                ddsd.ddpfPixelFormat.dwRGBBitCount);
            nITPR |= ITPR_ABORT;

            // Unlock the surface
            pDDS->Unlock(NULL);
        }
        else
        {
            nPixelSize = ddsd.ddpfPixelFormat.dwRGBBitCount/8;
            nPixelMask = ddsd.ddpfPixelFormat.dwRBitMask |
                         ddsd.ddpfPixelFormat.dwGBitMask |
                         ddsd.ddpfPixelFormat.dwBBitMask |
                         ddsd.ddpfPixelFormat.dwRGBAlphaBitMask;

            // Fill the surface with some known data
            pLine = ddsd.lpSurface;
            for(y = 0; y < ddsd.dwHeight; y++)
            {
                pPixel = pLine;
                for(x = 0; x < ddsd.dwWidth; x++)
                {
                    dwValue = PIXEL_VALUE(x, y);
                    switch(nPixelSize)
                    {
                    case 1:
                        *(BYTE *)pPixel = (BYTE)dwValue;
                        break;

                    case 2:
                        *(WORD *)pPixel = (WORD)dwValue;
                        break;

                    case 4:
                        *(DWORD *)pPixel = (DWORD)dwValue;
                        break;
                    }
                    pPixel = (void *)((BYTE *)pPixel + nPixelSize);
                }
                pLine = (void *)((BYTE *)pLine + ddsd.lPitch);
            }

            // Unlock the surface
            pDDS->Unlock(NULL);

            // Set up the source and destination rectangles.
            rcSrc.left      = rcDst.left    = 0;
            rcSrc.right     = rcDst.right   = ddsd.dwWidth;
            rcSrc.top       = rcDst.top     = 0;
            rcSrc.bottom    = rcDst.bottom  = ddsd.dwHeight;
            switch(nDirection)
            {
            case OVERLAPPEDBLT_N:
                rcSrc.top       += OVERLAPPED_SLACK;
                rcDst.bottom    -= OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_NE:
                rcSrc.top       += OVERLAPPED_SLACK;
                rcDst.bottom    -= OVERLAPPED_SLACK;
                rcSrc.right     -= OVERLAPPED_SLACK;
                rcDst.left      += OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_E:
                rcSrc.right     -= OVERLAPPED_SLACK;
                rcDst.left      += OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_SE:
                rcSrc.bottom    -= OVERLAPPED_SLACK;
                rcDst.top       += OVERLAPPED_SLACK;
                rcSrc.right     -= OVERLAPPED_SLACK;
                rcDst.left      += OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_S:
                rcSrc.bottom    -= OVERLAPPED_SLACK;
                rcDst.top       += OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_SW:
                rcSrc.bottom    -= OVERLAPPED_SLACK;
                rcDst.top       += OVERLAPPED_SLACK;
                rcSrc.left      += OVERLAPPED_SLACK;
                rcDst.right     -= OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_W:
                rcSrc.left      += OVERLAPPED_SLACK;
                rcDst.right     -= OVERLAPPED_SLACK;
                break;

            case OVERLAPPEDBLT_NW:
                rcSrc.top       += OVERLAPPED_SLACK;
                rcDst.bottom    -= OVERLAPPED_SLACK;
                rcSrc.left      += OVERLAPPED_SLACK;
                rcDst.right     -= OVERLAPPED_SLACK;
                break;
            }

            Debug(
                LOG_COMMENT,
                TEXT("Blitting from: (%d, %d) to (%d, %d)\r\n"),
                rcSrc.left,
                rcSrc.top,
                rcDst.left,
                rcDst.top);

            hr = pDDS->Blt(
                &rcDst,
                pDDS,
                &rcSrc,
                DDBLT_WAITNOTBUSY,
                NULL);

            if(FAILED(hr))
            {
                nBugID = 0;
#ifndef KATANA
                switch(nDirection)
                {
                case OVERLAPPEDBLT_E :
                    nBugID = 1922;
                    break;

                case OVERLAPPEDBLT_SE:
                    nBugID = 1923;
                    break;

                case OVERLAPPEDBLT_S :
                    nBugID = 1924;
                    break;

                case OVERLAPPEDBLT_SW:
                    nBugID = 1925;
                    break;
                }
#endif // !KATANA

                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    bFast ? c_szBltFast : c_szBlt,
                    hr,
                    g_pszDirection[nDirection],
                    NULL,
                    0,
                    RAID_HPURAID,
                    nBugID);
                nITPR |= ITPR_FAIL;
            }
            else
            {
                // Wait for the Blt to be completed
                while(pDDS->GetBltStatus(DDGBS_ISBLTDONE) ==
                      DDERR_WASSTILLDRAWING)
                    ;

                // Lock the surface again
                memset(&ddsd, 0, sizeof(ddsd));
                ddsd.dwSize = sizeof(ddsd);
                hr = pDDS->Lock(
                    NULL,
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
                        bFast ? TEXT("after BltFast") : TEXT("after Blt"));
                    nITPR |= ITPR_ABORT;
                }
                else
                {

                    bool bFoundBug24659 = false;
                    int i=0;
                    // Check the bits
                    do
                    {
                        i++;
                        nOffsetX = rcSrc.left - rcDst.left;
                        nOffsetY = rcSrc.top - rcDst.top;
                        pLine = (void *)((BYTE *)ddsd.lpSurface +
                                rcDst.top*ddsd.lPitch);
                        for(y = rcDst.top; y < (UINT)rcDst.bottom; y++)
                        {
                            pPixel = (void *)((BYTE *)pLine +
                                     rcDst.left*nPixelSize);
                            for(x = rcDst.left; x < (UINT)rcDst.right; x++)
                            {
                                switch(nPixelSize)
                                {
                                case 1:
                                    dwValue = *(BYTE *)pPixel;
                                    break;

                                case 2:
                                    dwValue = *(WORD *)pPixel;
                                    break;

                                case 4:
                                    dwValue = *(DWORD *)pPixel;
                                    break;
                                }

                                dwExpectedValue = PIXEL_VALUE(
                                    x + nOffsetX,
                                    y + nOffsetY);
                                if(dwValue != dwExpectedValue)
                                {
                                    if(!bFoundBug24659)
                                    {
                                        bFoundBug24659=true;
                                        Debug(LOG_COMMENT, TEXT("Found possible occurance of Bug 24659, retrying"));
                                    }
                                    else 
                                    {                                
                                        Debug(
                                            LOG_FAIL,
                                            FAILURE_HEADER
#ifdef KATANA
                                            TEXT("(Keep #2021) ")
#endif // KATANA
                                            TEXT("Expected data at coordinates")
                                            TEXT(" (%d, %d) to be 0x%08X (was 0x%08X)"),
                                            x,
                                            y,
                                            dwExpectedValue,
                                            dwValue);

                                        nITPR |= ITPR_FAIL;
                                    }
                                    // Exit both loops
                                    x = 1000000;
                                    y = 1000000;
                                }

                                pPixel = (void *)((BYTE *)pPixel + nPixelSize);
                            }
                            pLine = (void *)((BYTE *)pLine + ddsd.lPitch);
                        }
                        // Unlock the surface
                        pDDS->Unlock(NULL);
                        if(bFoundBug24659)
                            Sleep(20);
                    } while(i<2 && bFoundBug24659);  // loop twice if there's a failure
                    if(bFoundBug24659 && nITPR != ITPR_FAIL)
                        Debug(LOG_COMMENT, TEXT("Bug 24659 was found.  MQ200 does not flush outstanding blt's on lock."));
                }
            }
        }
    }

    // Don't need this macro anymore
#undef PIXEL_VALUE

    return nITPR;
}
#endif // TEST_IDDS

////////////////////////////////////////////////////////////////////////////////
