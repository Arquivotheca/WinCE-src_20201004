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
//  Module: ddp_x.cpp
//          BVT for advanced palette functions.
//
//  Revision History:
//      09/29/97    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#pragma warning(disable : 4995)

#if TEST_IDDP

////////////////////////////////////////////////////////////////////////////////
// Local macros

#define NUM_PALETTE_BLT_TESTS       1
#define MAX_PALETTE_BLT_FAILURES    40

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

ITPR    Help_PaletteToRGB(PALETTEENTRY *, USHORT *, DDPIXELFORMAT &);
ITPR    Help_BltPaletteTo16Bpp(IDirectDraw *, IDirectDrawSurface *, HRESULT);

////////////////////////////////////////////////////////////////////////////////
// Test_Palette_PaletteTo16Bpp
//  Tests Blt's from palletized surfaces to 16bpp surfaces.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_Palette_PaletteTo16Bpp(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS    *pIDDS;
    ITPR    nITPR = ITPR_PASS;

    IDirectDrawSurface  *pDDSTarget = NULL;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface(pIDDS))
        return TPRFromITPR(g_tprReturnVal);

    return TPR_SKIP;

// BUGBUG: Palettes are not supported in Bowmore.
//    // We assume that any color converting blt should fail.
//    // REVIEW:
//    // It's possible that a driver could implement the conversion,
//    // but neither the HEL (used by the IGS, since the driver doesn't
//    // support Sys->Vid blts) nor the default DDGPE implementation
//    // (used by the S3) handles it.
//    // The caps bits don't help us, since there are no bits
//    // to indicate color conversion capabilities...
//    HRESULT hrExpected = DDERR_UNSUPPORTED;
//
//    // Target the primary
//    DebugBeginLevel(
//        0,
//        TEXT("Palletized Blt test targeting the primary..."));
//    nITPR |= Help_BltPaletteTo16Bpp(pIDDS->m_pDD, pIDDS->m_pDDSPrimary, 
//        hrExpected);
//    DebugEndLevel(TEXT(""));
//
//    // Get the back buffer
//    ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
//    pDDSTarget = (IDirectDrawSurface *)PRESET;
//    hr = pIDDS->m_pDDSPrimary->EnumAttachedSurfaces(&pDDSTarget, Help_GetBackBuffer_Callback);
//
//    if(DDERR_NOTFOUND == hr &&
//        !(pIDDS->m_ddcHAL.ddsCaps.dwCaps & DDSCAPS_FLIP))
//    {
//        pDDSTarget = NULL;
//    }
//    else if(!CheckReturnedPointer(
//        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
//        pDDSTarget,
//        c_szIDirectDrawSurface,
//        c_szGetAttachedSurface,
//        hr,
//        TEXT("back buffer")))
//    {
//        pDDSTarget = NULL;
//        nITPR |= ITPR_ABORT;
//    }
//    else
//    {
//        DebugBeginLevel(
//            0,
//            TEXT("Palletized Blt test targeting the back buffer..."));
//        nITPR |= Help_BltPaletteTo16Bpp(pIDDS->m_pDD, pDDSTarget, 
//            hrExpected);
//        DebugEndLevel(TEXT(""));
//        pDDSTarget->Release();
//    }
//
//    // Target another system memory surface
//    pDDSTarget = pIDDS->m_pDDSSysMem;
//    if(pDDSTarget)
//    {
//        DebugBeginLevel(
//            0,
//            TEXT("Palletized Blt test targeting a system memory off-screen")
//            TEXT(" surface..."));
//        nITPR |= Help_BltPaletteTo16Bpp(pIDDS->m_pDD, pDDSTarget, hrExpected);
//        DebugEndLevel(TEXT(""));
//    }
//    else
//    {
//        Debug(
//            LOG_COMMENT,
//            TEXT("A system memory off-screen surface is not available for")
//            TEXT(" this test"));
//    }
//
//    if(nITPR == ITPR_PASS)
//    {
//        Report(
//            RESULT_SUCCESS,
//            TEXT("Blt from palletized surface to 16bpp surface"));
//    }
//
//    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_PaletteToRGB
//  Converts a palette into an array of 256 16-bit RGB values, given the pixel
//  format.
//
// Parameters:
//  pe              The palette array.
//  pRGB            The output buffer.
//  ddpf            The pixel format.
//
// Return value:
//  ITPR_PASS if successful, ITPR_ABORT otherwise.

ITPR Help_PaletteToRGB(PALETTEENTRY *pe, USHORT *pRGB, DDPIXELFORMAT &ddpf)
{
    int     nRSL,
            nGSL,
            nBSL,
            nRSR,
            nGSR,
            nBSR;
    DWORD   dwMask;

    // Check that the pixel format is 16-bits
    if(ddpf.dwRGBBitCount != 16)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Help_PaletteToRGB expected a 16-bit pixel")
            TEXT(" format (was passed a %d-bit format)"),
            ddpf.dwRGBBitCount);
        return ITPR_ABORT;
    }

    // Check that the bitmasks are somewhat valid
    if(0 == ddpf.dwRBitMask || 0 == ddpf.dwGBitMask || 0 == ddpf.dwBBitMask)
    {
        Debug(
            LOG_FAIL,
            FAILURE_HEADER TEXT("Invalid pixel format in Help_PaletteToRGB"));
        return ITPR_ABORT;
    }

    // First, determine the formatting parameters
    nRSL = 0;
    dwMask = ddpf.dwRBitMask;
    while(!(dwMask & 0x1))
    {
        dwMask >>= 1;
        nRSL++;
    }
    nRSR = 8;
    do
    {
        dwMask >>= 1;
        nRSR--;
    } while(dwMask & 0x1);

    nGSL = 0;
    dwMask = ddpf.dwGBitMask;
    while(!(dwMask & 0x1))
    {
        dwMask >>= 1;
        nGSL++;
    }
    nGSR = 8;
    do
    {
        dwMask >>= 1;
        nGSR--;
    } while(dwMask & 0x1);

    nBSL = 0;
    dwMask = ddpf.dwBBitMask;
    while(!(dwMask & 0x1))
    {
        dwMask >>= 1;
        nBSL++;
    }
    nBSR = 8;
    do
    {
        dwMask >>= 1;
        nBSR--;
    } while(dwMask & 0x1);

    Debug(
        LOG_DETAIL,
        TEXT("Pixel format is RGB with mask RGB = (0x%04X, 0x%04X, 0x%04X)"),
        ddpf.dwRBitMask,
        ddpf.dwGBitMask,
        ddpf.dwBBitMask);

    // Iterate through the palette and transform the entries
    for(int nIndex = 0; nIndex < 256; nIndex++)
    {
        pRGB[nIndex] = (USHORT)(
                       (((DWORD)pe[nIndex].peRed >> nRSR) << nRSL) +
                       (((DWORD)pe[nIndex].peGreen >> nGSR) << nGSL) +
                       (((DWORD)pe[nIndex].peBlue >> nBSR) << nBSL));

#ifdef DEBUG
        // Output the first few conversions for later checking
        if(nIndex < 10)
        {
            Debug(
                LOG_DETAIL,
                TEXT("Palette entry %03d, (%02X, %02X, %02X), maps to 0x%04X"),
                nIndex,
                pe[nIndex].peRed,
                pe[nIndex].peGreen,
                pe[nIndex].peBlue,
                pRGB[nIndex]);
        }
#endif // DEBUG
    }

    return ITPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Help_BltPaletteTo16Bpp
//  Fills up a palletized surface with a randomly generated palette and Blt's
//  its contents to the specified target surface.
//
// Parameters:
//  pDD             The DirectDraw object.
//  pDDSTarget      The target surface.
//  hrExpected      Result received from the blt
//
// Return value:
//  true if successful, false otherwise.

ITPR Help_BltPaletteTo16Bpp(IDirectDraw *pDD, IDirectDrawSurface *pDDSTarget,
    HRESULT hrExpected)
{
    HRESULT hr;
    DDBLTFX ddbltfx;
    ITPR    nITPR = ITPR_PASS;

    DDSURFACEDESC       ddsd;
    DDPIXELFORMAT       ddpf;
    IDirectDrawSurface  *pDDSOffScreen = NULL;
    IDirectDrawPalette  *pDDP = NULL;
    DDSCAPS      ddDrvCaps;

    // If we determine Keep #599 is not active, this boolean will become
    // false. Until then, there is always the possibility. There are now two
    // ways in which this bug manifests itself, so we need to check them both
    bool    bKeep599 = true,
            bKeep599Type1 = true,
            bKeep599Type2 = true;

    if (!pDDSTarget)
    {
        Report(
            RESULT_ABORT,
            _T("Help_BltPaletteTo16Bpp"),
            _T("pDDSTarget"),
            E_POINTER,
            _T("NULL pointer"));
        nITPR |= ITPR_ABORT;
        return nITPR;
    }

    // a-shende(07.26.1999): Retrieve and store HW caps.
    memset(&ddDrvCaps, 0x0, sizeof(DDSCAPS));
    if(pDDSTarget)
    {
        hr = pDDSTarget->GetCaps(&ddDrvCaps); 
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetCaps,
                hr,
                TEXT("GetCaps"));
            nITPR |= ITPR_ABORT;
        }
    }


    __try
    {
        // Get the pixel format of the target surface
        memset(&ddpf, 0, sizeof(DDPIXELFORMAT));
        ddpf.dwSize = sizeof(DDPIXELFORMAT);
        hr = pDDSTarget->GetPixelFormat(&ddpf);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szGetPixelFormat,
                hr,
                NULL,
                TEXT("for target surface"));
            nITPR |= ITPR_ABORT;
            __leave;
        }

        // Create a palletized surface
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize         = sizeof(ddsd);
        ddsd.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
                              DDSD_PIXELFORMAT;
        ddsd.dwWidth        = 32;
        ddsd.dwHeight       = 32;

        ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;

        ddsd.ddpfPixelFormat.dwSize         = sizeof(DDPIXELFORMAT);
        ddsd.ddpfPixelFormat.dwFlags        = DDPF_PALETTEINDEXED | DDPF_RGB;
        ddsd.ddpfPixelFormat.dwRGBBitCount  = 8;

        pDDSOffScreen = (IDirectDrawSurface *)PRESET;
        hr = pDD->CreateSurface(&ddsd, &pDDSOffScreen, NULL);
        if(!CheckReturnedPointer(
            CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
            pDDSOffScreen,
            c_szIDirectDraw,
            c_szCreateSurface,
            hr,
            NULL,
            TEXT("for palletized off-screen surface")))
        {
            pDDSOffScreen = NULL;
            nITPR |= ITPR_ABORT;
            __leave;
        }

        // Create a palette
        // BUGBUG: need to examine what dwCaps is needed by CreatePalette
        if(!Help_CreateDDPalette(0, &pDDP))
        {
            pDDP = NULL;
            nITPR |= ITPR_ABORT;
            __leave;
        }

        // Associate our palette with the surface
        hr = pDDSOffScreen->SetPalette(pDDP);
        if(FAILED(hr))
        {
            Report(RESULT_ABORT, c_szIDirectDrawSurface, c_szSetPalette, hr);
            nITPR |= ITPR_ABORT;
            __leave;
        }

        // Fill our surface with some known data
        hr = pDDSOffScreen->Lock(NULL, &ddsd, 0, NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_ABORT,
                c_szIDirectDrawSurface,
                c_szLock,
                hr,
                NULL,
                TEXT("for palletized off-screen surface"));
            nITPR |= ITPR_ABORT;
            __leave;
        }

        BYTE    *pLine = (BYTE *)ddsd.lpSurface;
        int     nIndex = 0;
        for(int nLine = 0; nLine < (int)ddsd.dwHeight; nLine++)
        {
            BYTE    *pData = pLine,
                    *pLast = pData + ddsd.dwWidth;
            while(pData < pLast)
            {
                *pData++ = (BYTE)nIndex++;
            }
            pLine += ddsd.lPitch;
        }

        pDDSOffScreen->Unlock(NULL);

        // Do some setup work
        RECT    rcSrc = { 0, 0, 32, 32 },
                rcDst = { 304, 224, 336, 256 };

        // Create various palettes and Blt the surface to the target
        int nFailures = 0;
        for(int nTest = 0; nTest < NUM_PALETTE_BLT_TESTS; nTest++)
        {
            PALETTEENTRY    pe[256];
            USHORT          rgb[256];

            // Fill the palette
            for(nIndex = 0; nIndex < 256; nIndex++)
            {
                pe[nIndex].peRed = rand()%256;
                pe[nIndex].peGreen = rand()%256;
                pe[nIndex].peBlue = rand()%256;
            }

            hr = pDDP->SetEntries(0, 0, 256, pe);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawPalette,
                    c_szSetEntries,
                    hr);
                nITPR |= ITPR_ABORT;
                __leave;
            }

            // Clear the target surface
            memset(&ddbltfx, 0, sizeof(ddbltfx));
            ddbltfx.dwSize = sizeof(ddbltfx);
            ddbltfx.dwFillColor = 0;
            hr = pDDSTarget->Blt(
                &rcDst,
                NULL,
                NULL,
                DDBLT_COLORFILL | DDBLT_WAITNOTBUSY,
                &ddbltfx);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szBlt,
                    hr,
                    TEXT("DDBLT_COLORFILL"));
                nITPR |= ITPR_ABORT;
                __leave;
            }

            // Blt the contents of the surface into the target surface
            memset(&ddbltfx, 0, sizeof(ddbltfx));
            ddbltfx.dwSize = sizeof(ddbltfx);
            hr = pDDSTarget->Blt(
                &rcDst,
                pDDSOffScreen,
                &rcSrc,
                DDBLT_WAITNOTBUSY,
                &ddbltfx);

            // a-shende: check for unsupported funcs
            if(hr != hrExpected)
            {

                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szBlt,
                    hr,
                    NULL,
                    TEXT("from palletized surface to target surface"));
                nITPR |= ITPR_FAIL;
                __leave;
            }

            // Wait for the Blt to complete
            do
            {
                hr = pDDSTarget->GetBltStatus(DDGBS_ISBLTDONE);
            } while(hr == DDERR_WASSTILLDRAWING);

            // If we didn't exit because we got S_OK, something went wrong
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szGetBltStatus,
                    hr);
                nITPR |= ITPR_FAIL;
                __leave;
            }

            // a-shende: if Blt was not available, don't check contents
            if(S_OK != hrExpected)
            {
                Debug(LOG_COMMENT, 
                    TEXT("Blt failed as expected -- not validating\r\n"));
                __leave;
            }

            // Verify the contents of the target surface
            hr = pDDSTarget->Lock(
                &rcDst,
                &ddsd,
                0,
                NULL);
            if(FAILED(hr))
            {
                Report(
                    RESULT_ABORT,
                    c_szIDirectDrawSurface,
                    c_szLock,
                    hr,
                    NULL,
                    TEXT("for target surface"));
                nITPR |= ITPR_ABORT;
                __leave;
            }

            Help_PaletteToRGB(pe, rgb, ddpf);
            
            pLine = (BYTE *)ddsd.lpSurface;
            nIndex = 0;
            bKeep599 = true;
            bKeep599Type1 = true;
            bKeep599Type2 = true;
            bool bFailuresHere = false;
            for(nLine = 0; nLine < 32; nLine++)
            {
                USHORT  *pData = (USHORT *)pLine,
                        *pLast = pData + 32,
                        nTemp;
                while(pData < pLast)
                {
                    // Is there a problem?
                    if(*pData != rgb[nIndex])
                    {
                        // Check for this bug
                        if(bKeep599)
                        {
                            nTemp = ((*pData >> 11)&0x001F) |
                                    (*pData&0x07E0) |
                                    ((*pData << 11)&0xF800);
                            if(bKeep599Type1 && nTemp == rgb[nIndex])
                            {
                                // Record this failure
                                bFailuresHere = true;
                                bKeep599Type2 = false;
                                if(++nFailures == MAX_PALETTE_BLT_FAILURES)
                                    break;
                            }
                            else if(bKeep599Type2 && *pData == nIndex)
                            {
                                // Record this failure
                                bFailuresHere = true;
                                bKeep599Type1 = false;
                                if(++nFailures == MAX_PALETTE_BLT_FAILURES)
                                    break;
                            }
                            else
                            {
                                // This is something other than Keep #599.
                                // Output the previous failures
                                BYTE *pOldLine = (BYTE *)ddsd.lpSurface;
                                for(int nOldLine = 0; nOldLine < 32; nOldLine++)
                                {
                                    USHORT  *pOldData = (USHORT *)pOldLine,
                                            *pOldLast = pOldData + 32;
                                    int     nOldIndex = 0;
                                    while(pOldData < pOldLast)
                                    {
                                        Debug(
                                            LOG_FAIL,
                                            FAILURE_HEADER TEXT("Color")
                                            TEXT(" mismatch in destination")
                                            TEXT(" surface at (%d, %d)")
                                            TEXT(": expected color 0x%04X,")
                                            TEXT(" observed 0x%04X"),
                                            pOldData - (USHORT *)pOldLine,
                                            nOldLine,
                                            rgb[nOldIndex],
                                            *pOldData);
                                        nITPR |= ITPR_FAIL;

                                        if(++nOldIndex == 256)
                                            nOldIndex = 0;

                                        pOldData++;

                                        if(pOldData == pData)
                                            break;
                                    }
                                    if(pOldData == pData)
                                        break;
                                }

                                bKeep599 = false;
                            }
                        }
                        else
                        {
                            Debug(
                                LOG_FAIL,
                                FAILURE_HEADER TEXT("Color mismatch in")
                                TEXT(" destination surface at (%d, %d):")
                                TEXT(" expected color 0x%04X, observed 0x%04X"),
                                pData - (USHORT *)pLine,
                                nLine,
                                rgb[nIndex],
                                *pData);
                            nITPR |= ITPR_FAIL;
                            if(++nFailures == MAX_PALETTE_BLT_FAILURES)
                                break;
                        }
                    }
                    else
                    {
                        bKeep599 = false;
                    }

                    if(++nIndex == 256)
                        nIndex = 0;

                    pData++;
                }

                if(nFailures == MAX_PALETTE_BLT_FAILURES)
                    break;

                pLine += ddsd.lPitch;
            }

            if(bFailuresHere && bKeep599)
            {
                if(bKeep599Type1)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER
#ifdef KATANA
                        TEXT("(Keep #599) ")
#endif // KATANA
                        TEXT("The blue and red")
                        TEXT(" masks were Blt'ed in reverse to the destination")
                        TEXT(" surface"));
                    nITPR |= ITPR_FAIL;
                }
                else if(bKeep599Type2)
                {
                    Debug(
                        LOG_FAIL,
                        FAILURE_HEADER
#ifdef KATANA
                        TEXT("(Keep #599) ")
#else // not KATANA
                        TEXT("(HPURAID #3174) ")
#endif // not KATANA
                        TEXT("The palette indices")
                        TEXT(" were copied to the destination surface, instead")
                        TEXT(" of the palette values"));
                    nITPR |= ITPR_FAIL;
                }
            }

            pDDSTarget->Unlock(NULL);

            if(nFailures == MAX_PALETTE_BLT_FAILURES)
                break;
        }
    }
    __finally
    {
        if(pDDP)
            pDDP->Release();
        if(pDDSOffScreen)
            pDDSOffScreen->Release();
    }

    return nITPR;
}
#endif // TEST_IDDP

////////////////////////////////////////////////////////////////////////////////
