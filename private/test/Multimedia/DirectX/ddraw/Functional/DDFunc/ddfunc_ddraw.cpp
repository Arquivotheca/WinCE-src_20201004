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
#include "DDFunc_DDraw.h"

#include <QATestUty/PresetUty.h>
#include <QATestUty/WinApp.h>
#include <DDrawUty.h>

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace KnownBugUty;
using namespace DDrawUty;
using namespace PresetUty;

namespace Test_IDirectDrawSurface
{
    void MessagePump(HWND hWnd);
}

using namespace Test_IDirectDrawSurface;

namespace Test_IDirectDraw
{
#if CFG_TEST_IDirectDrawClipper
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateClipper::TestIDirectDraw
    //  Tests the CreateClipper API.
    //
    eTestResult CTest_CreateClipper::TestIDirectDraw()
    {
        CComPointer<IDirectDrawClipper> piDDC;
        eTestResult tr = trPass;
        HRESULT hr;
        BOOL fChanged = FALSE;

        // Create the DirectDraw Object
        dbgout << "Calling IDirectDraw::CreateClipper" << endl;
        hr = m_piDD->CreateClipper(0, piDDC.AsTestedOutParam(), NULL);
        CheckHRESULT(hr, "IDirectDraw::CreateClipper", trFail);
        CheckCondition(piDDC.InvalidOutParam(), "CreateClipper succeeded but failed to set the OUT parameter", trFail);

        // Verify that the pointer is valid
        dbgout << "Initializing the created DirectDrawClipper Object" << endl;
        hr = piDDC->IsClipListChanged(&fChanged);
        CheckHRESULT(hr, "IDirectDrawClipper::IsClipListChanged", trFail);

        return tr;
    }
#endif // CFG_TEST_IDirectDrawClipper

    ////////////////////////////////////////////////////////////////////////////////
    // privCTest_CreatePalette
    //  Helper namespace for the CTest_CreatePalette tests.
    //
    namespace privCTest_CreatePalette
    {
        struct PaletteDesc
        {
            int nBits;
            DWORD dwPalFlags;
            DWORD dwSurfFlags;
            int nLowerBound;
            int nUpperBound;
        };

        static PaletteDesc rgpd[] =
        {
            { 8, 0, DDPF_PALETTEINDEXED, 0, 256 }
        };
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreatePalette::TestIDirectDraw
    //  Tests the CreatePalette Method.
    //
    eTestResult CTest_CreatePalette::TestIDirectDraw()
    {
        using namespace privCTest_CreatePalette;
        
        dbgout << "Palettes not supported in Bowmore, skipping" << endl;
        return trSkip;
//        const DWORD dwPalCaps = (g_DDConfig.HALCaps().ddsCaps.dwCaps);
//        CComPointer<IDirectDrawPalette> piDDP;
//        PALETTEENTRY rgpe[257] = { 0 };
//        eTestResult tr = trPass;
//        HRESULT hr;
//        DWORD dwPalCaps2 = 0;
//
//        for(int i = 0; i < countof(rgpd); i++)
//        {
//            if(dwPalCaps & rgpd[i].dwPalFlags)
//            {
//                dbgout
//                    << "Calling IDirectDraw::CreatePalette with Flags = "
//                    << g_CapsPalCapsFlagMap[rgpd[i].dwPalFlags] << endl;
//                hr = m_piDD->CreatePalette(rgpd[i].dwPalFlags, rgpe, piDDP.AsTestedOutParam(), NULL);
//                QueryHRESULT(hr, "IDirectDraw::CreatePalette", tr |= trFail);
//                QueryCondition(piDDP.InvalidOutParam(), "CreatePalette succeeded but failed to set the OUT parameter", tr |= trFail);
//
//                // Verify that the pointer is valid
//                dbgout << "Initializing the created DirectDrawPalette Object" << endl;
//                hr = piDDP->GetCaps(&dwPalCaps2);
//                QueryHRESULT(hr, "IDirectDrawPalette::GetCaps", tr |= trFail);
//            }
//            else
//            {
//                dbgout
//                    << "Pallete " << g_CapsPalCapsFlagMap[rgpd[i].dwPalFlags]
//                    << " not supported" << endl;
//                dbgout << "Verifying that palette creation fails" << endl;
//                hr = m_piDD->CreatePalette(rgpd[i].dwPalFlags, rgpe, piDDP.AsTestedOutParam(), NULL);
//                QueryForKnownWinCEBug(S_OK == hr, 3532, "Remove support for 1-, 2- and 4-bit palettes", tr |= trFail)
//                else QueryForHRESULT(hr, DDERR_UNSUPPORTED, "IDirectDraw::CreatePalette", tr |= trFail)
//                else QueryCondition(piDDP != NULL, "CreatePalette failed but didn't NULL the OUT parameter", tr |= trFail);
//
//                CDDSurfaceDesc cddsd;
//                CComPointer<IDirectDrawSurface> piDDS;
//                
//                dbgout << "Verifying that surface creation fails" << endl;
//                cddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
//                cddsd.dwWidth = cddsd.dwHeight = 30;
//                cddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
//                cddsd.ddpfPixelFormat.dwSize = sizeof(cddsd.ddpfPixelFormat);
//                cddsd.ddpfPixelFormat.dwFlags = DDPF_RGB | rgpd[i].dwSurfFlags;
//                cddsd.ddpfPixelFormat.dwRGBBitCount = rgpd[i].nBits;
//
//                dbgout << "cddsd = " << cddsd << endl;
//                hr = m_piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
//
//                if (SUCCEEDED(hr))
//                {
//                    CDDSurfaceDesc cddsd2;
//                    piDDS->GetSurfaceDesc(&cddsd2);
//                    dbgout << "SUCCESS!!!" << cddsd2 << endl;
//                }
//                
//                QueryForKnownWinCEBug(S_OK == hr, 3532, "Remove support for 1-, 2- and 4-bit palettes", tr |= trFail)
//                else QueryForHRESULT(hr, DDERR_INVALIDPIXELFORMAT, "IDirectDraw::CreateSurface", tr |= trFail)
//                else QueryCondition(piDDS != NULL, "CreateSurface failed but didn't NULL the OUT parameter", tr |= trFail);
//            }
//        }
//        
//        return tr;
    }


    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateSurface::TestIDirectDraw
    //  Tests the CreateSurface Method.
    //
    namespace privCTest_CreateSurface
    {
        eTestResult CreateVerifySurface(
                                CComPointer<IDirectDraw> &piDD,
                                CComPointer<IDirectDrawSurface> &piDDS,
                                CDDSurfaceDesc &cddsd,
                                TCHAR *pszDescSurf,
                                TCHAR *pszDescPixel)
        {
            HRESULT hr;
            
            // Output the current surface description
            dbgout
                << "Attempting to create surface : " << pszDescSurf
                << " (" << pszDescPixel << ")" << endl;
            
            // Attempt to create a surface from the current
            // surface description.
            hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
            
            // Try creating a smaller surface if a TOOBIG error was thrown by the driver.
            if (DDERR_TOOBIGSIZE == hr || DDERR_TOOBIGWIDTH == hr || DDERR_TOOBIGHEIGHT == hr)
            {
                dbgout << "A " << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << ") was returned by CreateSurface." 
                       << endl
                       << "Retrying with a smaller surface." 
                       << endl;

                // On Ragexl, Creating Overlay surfaces in video memory with width > 720 returns DDERR_TOOBIGSIZE
                // Retry by halving the width and height.
                while(((cddsd.dwWidth/2) > 0 && (cddsd.dwHeight/2) > 0) && 
                      (DDERR_TOOBIGSIZE == hr || DDERR_TOOBIGWIDTH == hr || DDERR_TOOBIGHEIGHT == hr))
                {
                    cddsd.dwWidth/=2;
                    cddsd.dwHeight/=2;

                    hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                }
            }

#ifdef CFG_GENERIC_ERRORS
            if (DDERR_GENERIC == hr)
            {
                return trPass;
            }
            else
#endif
            if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
            {
                return OutOfMemory_Helpers::VerifyOOMCondition(hr, piDD.AsInParam(), cddsd);
            }
            else if ((DDERR_NOOVERLAYHW == hr || DDERR_INVALIDCAPS == hr)&&
                     ((cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
                      !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_OVERLAY)))
            {
                // If we got a no overlay hardware error, the test still passes as
                // long as either the surface is in system memory or if the HAL
                // doesn't support overlays.
                return trPass;
            }
            else if (DDERR_UNSUPPORTEDFORMAT == hr && (cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
            {
                // Unsupported formats are ok for overlay surfaces, since the
                // support is driver dependent.
                return trPass;
            }
            else if ((DDERR_UNSUPPORTEDFORMAT == hr) &&
                    (cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC) &&
                    !(cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
            {
                return trPass;
            }
            else if((DDERR_NOFLIPHW == hr || DDERR_INVALIDCAPS == hr) && 
                      (cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                      (!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) ||
                      !(cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
            {
                return trPass;
            }
            CheckHRESULT(hr, "CreateSurface", trFail);
            QueryCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", trFail);

            // Verify that the pointer is a valid surface
            CDDSurfaceDesc cddsdTest;
            dbgout << "Verifying that surface pointer is valid" << endl;
            hr = piDDS->GetSurfaceDesc(&cddsdTest);
            CheckHRESULT(hr, "GetSurfaceDesc", trFail);

            // See if what we got is compatable with what we 
            // were passed
            return DDrawUty::Surface_Helpers::CompareSurfaceDescs(cddsdTest, cddsd);
        }
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_CreateSurface::TestIDirectDraw
    //  Tests the CreateSurface Method.
    //
    eTestResult CTest_CreateSurface::TestIDirectDraw()
    {
        using namespace privCTest_CreateSurface;
        
        CDirectDrawConfiguration::VectSurfaceType::iterator itrPrimary,
                                                            itrTemp;
        CDirectDrawConfiguration::VectPixelFormat::iterator itrPixel;
        CComPointer<IDirectDrawSurface> piDDSPrimary,
                                        piDDSTemp;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsd;
        TCHAR szDescSurf[256],
              szDescPixel[256];
        HRESULT hr;

        // Release any existing primary (since we'll be
        // creating primary surfaces
        g_DDSPrimary.ReleaseObject();
        
        // Iterate all supported Primary surface types
        dbgout << "Iterating all supported Primary surface descriptions" << endl;
        for (itrPrimary  = g_DDConfig.m_vstSupportedPrimary.begin();
             itrPrimary != g_DDConfig.m_vstSupportedPrimary.end();
             itrPrimary++)
        {
            // Get the description of the current mode
            hr = g_DDConfig.GetSurfaceDescData(*itrPrimary, cddsd, szDescSurf, countof(szDescSurf));
            ASSERT(SUCCEEDED(hr));
            
            // Test that we can create a functional primary
            tr |= CreateVerifySurface(m_piDD, piDDSPrimary, cddsd, szDescSurf, TEXT("Unspecified"));

            // Iterate all supported video memory surfaces
            dbgout << "Iterating all video memory surfaces" << indent;
            for (itrTemp  = g_DDConfig.m_vstSupportedVidMem.begin();
                 itrTemp != g_DDConfig.m_vstSupportedVidMem.end();
                 itrTemp++)
            {
                hr = g_DDConfig.GetSurfaceDescData(*itrTemp, cddsd, szDescSurf, countof(szDescSurf));
                ASSERT(SUCCEEDED(hr));
                // Iterate all supported pixel formats in video memory
                for (itrPixel  = g_DDConfig.m_vpfSupportedVidMem.begin();
                     itrPixel != g_DDConfig.m_vpfSupportedVidMem.end();
                     itrPixel++)
                {
                    // See if this is a valid surface/pixel combination
                    if (g_DDConfig.IsValidSurfacePixelCombo(*itrTemp, *itrPixel))
                    {
                        // Get the pixel format into the surface descriptor
                        hr = g_DDConfig.GetPixelFormatData(*itrPixel, cddsd.ddpfPixelFormat, szDescPixel, countof(szDescPixel));
                        ASSERT(SUCCEEDED(hr));
                        // Mark the pixel format data as valid, and test the surface
                        cddsd.dwFlags |= DDSD_PIXELFORMAT;
                        tr |= CreateVerifySurface(m_piDD, piDDSTemp, cddsd, szDescSurf, szDescPixel);
                    }
                }
            }
            dbgout << unindent << "Done video memory surfaces" << endl;

            // Iterate all supported system memory surfaces
            dbgout << "Iterating all system memory surfaces" << indent;
            for (itrTemp  = g_DDConfig.m_vstSupportedSysMem.begin();
                 itrTemp != g_DDConfig.m_vstSupportedSysMem.end();
                 itrTemp++)
            {
                hr = g_DDConfig.GetSurfaceDescData(*itrTemp, cddsd, szDescSurf, countof(szDescSurf));
                ASSERT(SUCCEEDED(hr));
                // Iterate all supported pixel formats in system memory
                for (itrPixel  = g_DDConfig.m_vpfSupportedSysMem.begin();
                     itrPixel != g_DDConfig.m_vpfSupportedSysMem.end();
                     itrPixel++)
                {
                    // See if this is a valid surface/pixel combination
                    if (g_DDConfig.IsValidSurfacePixelCombo(*itrTemp, *itrPixel))
                    {
                        // Get the pixel format into the surface descriptor
                        hr = g_DDConfig.GetPixelFormatData(*itrPixel, cddsd.ddpfPixelFormat, szDescPixel, countof(szDescPixel));
                        ASSERT(SUCCEEDED(hr));
                        // Mark the pixel format data as valid, and test the surface
                        cddsd.dwFlags |= DDSD_PIXELFORMAT;
                        tr |= CreateVerifySurface(m_piDD, piDDSTemp, cddsd, szDescSurf, szDescPixel);
                    }
                }
            }
            dbgout << unindent << "Done system memory surfaces" << endl;
            
            // Release the interface (not really necessary,
            // since the next assignment will do it)
            piDDSPrimary.ReleaseInterface();
        }

        dbgout << "Testing all unsupported formats" << indent;
        for (int i = (int)stNone+1; i < (int)stCount; i++)
        {
            hr = g_DDConfig.GetSurfaceDescData((CfgSurfaceType)i, cddsd, szDescSurf, countof(szDescSurf));
            ASSERT(SUCCEEDED(hr));

            // You can never specify a pixel format for primary surfaces
            if (cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
                continue;
                    
            cddsd.dwFlags |= DDSD_PIXELFORMAT;
            for (int j = (int)pfNone+1; j < (int)pfCount; j++)
            {
                hr = g_DDConfig.GetPixelFormatData((CfgPixelFormat)j, cddsd.ddpfPixelFormat, szDescPixel, countof(szDescPixel));
                ASSERT(SUCCEEDED(hr));

                // FourCC codes are too complex.  Even if a FourCC code
                // is supported, there is no way to tell if it is
                // supported for overlay, offscreen in video, offscreen
                // in system, etc.
                if (cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
                    continue;
                    
                dbgout
                    << "Attempting to create surface : " << szDescSurf
                    << " (" << szDescPixel << ")" << endl;

                // If IsValidSurfacePixelCombo function can not find current surface/pixel pair in the supported vectors,
                // it means this particular pair is not supported, and will be tested.
                // pfPal8 (256 color palette) cases are exluded because even some pfPal8 cases are not supported, some 
                // hardware can successfully create the surface and therefore fail the test.
                if (!g_DDConfig.IsValidSurfacePixelCombo((CfgSurfaceType)i, (CfgPixelFormat)j) )
                {
                    {
                        hr = m_piDD->CreateSurface(&cddsd, piDDSTemp.AsTestedOutParam(), NULL);
                        QueryCondition(SUCCEEDED(hr), "CreateSurface didn't fail when supposed to" << cddsd, tr |= trFail)
                        else QueryCondition(piDDSTemp != NULL, "CreateSurface failed, but didn't NULL out param", tr |= trFail);
                    }
                }
                else
                {
                    tr |= CreateVerifySurface(m_piDD, piDDSTemp, cddsd, szDescSurf, szDescPixel);
                }
            }
        }
        dbgout << unindent << "Done invalid combinations" << endl;
        
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumDisplayModes::EnumModesCallback
    //  Callback function.
    //
    HRESULT WINAPI CTest_EnumDisplayModes::EnumModesCallback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext)
    {
        CheckCondition(lpContext == NULL, "Callback Called with invalid Context", (HRESULT)DDENUMRET_CANCEL);

        // Get a pointer to the test object that we passed in
        CTest_EnumDisplayModes *ptest = static_cast<CTest_EnumDisplayModes*>(lpContext);

        // Do a number of sanity checks
        CheckCondition(!IsPreset(ptest->m_dwPreset), "DirectDrawEnumerate Callback called with invalid Context", (HRESULT)DDENUMRET_CANCEL);
        QueryCondition(!ptest->m_fCallbackExpected, "DirectDrawEnumerate Callback called when not expected", ptest->m_tr |= trFail);
        QueryCondition(lpDDSurfaceDesc == NULL, "DirectDrawEnumerate Callback received lpcstrDescription == NULL", ptest->m_tr |= trFail);

        // Log the callback information
        dbgout
            << "Surface Description #" << ptest->m_nCallbackCount
            << ":" << *lpDDSurfaceDesc << endl;
            
        // Increment our callback count
        ptest->m_nCallbackCount++;

// This is known to not work.
//        // Make sure that we got a create-able Surface Description
//        CComPointer<IDirectDrawSurface> piDDS;
//        HRESULT hr;
//
//        hr = ptest->m_piDD->CreateSurface(lpDDSurfaceDesc, piDDS.AsTestedOutParam(), NULL);
//        QueryHRESULT(hr, "EnumDisplayModes Callback received non-creatable GUID", ptest->m_tr |= trFail)
//        else QueryCondition(piDDS.InvalidOutParam(), "EnumDisplayModes Callback received non-creatable GUID", ptest->m_tr |= trFail);

        // Even if we've failed, keep iterating (since we won't
        // loose the failure on the next iteration).
        return (HRESULT)DDENUMRET_OK;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumDisplayModes::TestIDirectDraw
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_EnumDisplayModes::TestIDirectDraw()
    {
        m_tr = trPass;
        HRESULT hr = S_OK;

        // IDirectDraw
        dbgout << "IDirectDraw" << endl;
        m_nCallbackCount = 0;
        dbgout << "Calling EnumDisplayModes with no flags and NULL SurfaceDesc" << endl;
        
        m_fCallbackExpected = true;
        hr = m_piDD->EnumDisplayModes(0, NULL, this, EnumModesCallback);
        m_fCallbackExpected = false;
        QueryHRESULT(hr, "EnumDisplayModes Failed", m_tr |= trFail);

        // Output the number of callbacks we got
        dbgout(LOG_DETAIL) << "Enumerated " << m_nCallbackCount << " modes(s)" << endl;

        // Make sure the callback was called at least once
        QueryCondition(m_nCallbackCount < 1, "EnumDisplayModes failed to call callback", m_tr |= trFail);

        return m_tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_FlipToGDISurface::TestIDirectDraw
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_FlipToGDISurface::TestIDirectDraw()
    {
        CComPointer<IDirectDrawSurface> piDDS;
        eTestResult tr = trPass;
        HRESULT hr;

        const DWORD dwNonGDI = 0x01234567,
                    dwGDI    = 0x89abcdef;
        DDSCAPS ddsc;
        memset(&ddsc, 0x00, sizeof(ddsc));
        
        piDDS = g_DDSPrimary.GetObject();
        if(piDDS && SUCCEEDED(piDDS->GetCaps(&ddsc)) && ddsc.dwCaps & DDSCAPS_FLIP)
        {
            CDDSurfaceDesc cddsd;
            hr = piDDS->GetSurfaceDesc(&cddsd);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

            // See if we're a sysmem primary
            BOOL fSysMem = !!(cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY);

            // Test simple FlipToGDISurface call
            dbgout << "Test simple FlipToGDISurface call" << endl;
            hr = m_piDD->FlipToGDISurface();
            CheckHRESULT(hr, "FlipToGDISurface", trFail);

            // Flip away from the GDI
            dbgout << "Flip away from the GDI" << endl;
            hr = piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            CheckHRESULT(hr, "Flip Primary after FlipToGDISurface", trFail);

            // Lock the primary (the non-GDI surface)
            dbgout << "Locking Primary (non-GDI)" << endl;
            Preset(cddsd.lpSurface);
            hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Primary after FlipToGDISurface", trFail);
            CheckCondition(IsPreset(cddsd.lpSurface), "Lock failed to fill lpSurface", trFail);

            // Write some data to the non-GDI surface and unlock
            dbgout << "Writing to primary and unlocking" << endl;
            *(DWORD*)cddsd.lpSurface = dwNonGDI;
            hr = piDDS->Unlock(NULL);
            CheckHRESULT(hr, "Unlock", trFail);

            // Flip back to the GDI Surface
            dbgout << "Flipping back to GDI Surface" << endl;
            hr = piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            CheckHRESULT(hr, "Flip Primary after lock/unlock", trFail);

            // Lock the primary (the GDI surface)
            dbgout << "Locking Primary (GDI)" << endl;
            Preset(cddsd.lpSurface);
            hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Primary after FlipToGDISurface", trFail);
            CheckCondition(IsPreset(cddsd.lpSurface), "Lock failed to fill lpSurface", trFail);
            
            // Write test data to the new primary (GDI Surface)
            dbgout << "Writing to primary and unlocking" << endl;
            *(DWORD*)cddsd.lpSurface = dwGDI;
            hr = piDDS->Unlock(NULL);
            CheckHRESULT(hr, "Unlock", trFail);

            // FlipToGDI again (should do nothing)
            dbgout << "Calling FlipToGDI again" << endl;
            hr = m_piDD->FlipToGDISurface();
            CheckHRESULT(hr, "FlipToGDISurface", trFail);

            // Check that we're on GDI
            dbgout << "Checking that we're still on GDI" << endl;
            hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Primary after FlipToGDISurface", trFail);
            CheckCondition(*(DWORD*)cddsd.lpSurface != dwGDI, "Did not find GDI when expected", trFail);

            // Unlock
            hr = piDDS->Unlock(NULL);
            CheckHRESULT(hr, "Unlock", trFail);
            
            // Flip away from the GDI
            dbgout << "Flip away from the GDI" << endl;
            hr = piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            CheckHRESULT(hr, "Flip Primary", trFail);

            // Check that we're on non-GDI
            dbgout << "Checking that we're on non-GDI" << endl;
            hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Primary after FlipToGDISurface", trFail);
            QueryForKnownWinCEBug(*(DWORD*)cddsd.lpSurface != dwNonGDI && fSysMem, 1940, "Bad Flip on Sysmem primary", tr |= trFail)
            else QueryCondition(*(DWORD*)cddsd.lpSurface != dwNonGDI, "Did not find GDI when expected", tr |= trFail);

            // Unlock
            hr = piDDS->Unlock(NULL);
            CheckHRESULT(hr, "Unlock", trFail);

            // FlipToGDI again (should perform a flip)
            dbgout << "Calling FlipToGDI again" << endl;
            hr = m_piDD->FlipToGDISurface();
            CheckHRESULT(hr, "FlipToGDISurface", trFail);

            // Check that we're on GDI
            dbgout << "Checking that we're now on GDI" << endl;
            hr = piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Primary after FlipToGDISurface", trFail);
            QueryForKnownWinCEBug(*(DWORD*)cddsd.lpSurface != dwNonGDI && fSysMem, 1940, "Bad Flip on Sysmem primary", tr |= trFail)
            else QueryCondition(*(DWORD*)cddsd.lpSurface != dwGDI, "Did not find GDI when expected", tr |= trFail);

            // Unlock
            hr = piDDS->Unlock(NULL);
            CheckHRESULT(hr, "Unlock", trFail);
        }
        else if(!(ddsc.dwCaps & DDSCAPS_FLIP) ||
                ((DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() || DDERR_INVALIDCAPS == g_DDSPrimary.GetLastError())&& 
                !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP)))
        {
                // HEL and HAL do not support flipping, it's a ok.
            dbgout << "Flipping not supported, skip" << endl;
            tr |= trSkip;
        }
        else 
        {
            dbgout << "Failed to create surface" << endl;
            tr |= trFail;
        }

        return tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetCaps::TestIDirectDraw
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_GetCaps::TestIDirectDraw()
    {
        CDDCaps ddcapsHAL1,
                ddcapsHAL2;
        eTestResult tr = trPass;
        HRESULT hr;
        
        // HAL Only
        dbgout << "Geting Caps for HAL only" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL1, NULL);
        CheckHRESULT(hr, "EnumDisplayModes HAL Only", trFail);
        
        // Both HEL and HAL
        dbgout << "Geting Caps for HAL again" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL2, NULL);
        CheckHRESULT(hr, "EnumDisplayModes HAL second time", trFail);

        // Compare the results from the two calls
        dbgout << "Comparing results from different calls" << endl;
        QueryCondition(memcmp(&ddcapsHAL1, &ddcapsHAL2, sizeof(ddcapsHAL1)), "HAL CAPs differed between calls", tr |= trFail);

        // Display the caps that we recieved
        dbgout(LOG_DETAIL)
            << indent
            << "HAL Caps:" << ddcapsHAL1 << endl
            << unindent;

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetDisplayMode::TestIDirectDraw
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_GetSetDisplayMode::TestIDirectDraw()
    {
        CDirectDrawConfiguration::VectDisplayMode::iterator itr;
        eTestResult tr = trPass;
        HRESULT hr;

        // Iterate all supported modes
        dbgout << "Iterating all supported modes" << endl;
        for(itr  = g_DDConfig.m_vdmSupported.begin();
            itr != g_DDConfig.m_vdmSupported.end();
            itr++)
        {

            // Set the mode
            dbgout << "Testing mode : " << itr->m_dwWidth << "x" 
                << itr->m_dwHeight << "x" << itr->m_dwDepth << "x" 
                << itr->m_dwFrequency << " Hz; vga (" << itr->m_dwFlags << ")" << endl;
            hr = m_piDD->SetDisplayMode(itr->m_dwWidth, itr->m_dwHeight, itr->m_dwDepth, itr->m_dwFrequency, itr->m_dwFlags);
            CheckHRESULT(hr, "SetDisplayMode", trFail);

            // Get the mode back
            CDDSurfaceDesc cddsd;
            dbgout << "Retrieving mode" << endl;
            hr = m_piDD->GetDisplayMode(&cddsd);
            CheckHRESULT(hr, "GetDisplayMode", trFail);
            dbgout << "Returned Surface Desc = " << cddsd << endl;

            // Check that we got back what we put in
            QueryCondition(
                !(DDSD_REFRESHRATE & cddsd.dwFlags) ||
                !(DDSD_PIXELFORMAT & cddsd.dwFlags) ||
                itr->m_dwWidth  != cddsd.dwWidth  ||
                itr->m_dwHeight != cddsd.dwHeight ||
                itr->m_dwDepth  != cddsd.ddpfPixelFormat.dwRGBBitCount,
                "GetDisplayMode Returned Different information that SetDisplayMode",
                tr |= trFail)
            else
                dbgout << "Success" << endl;
        }
            
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetDisplayMode::TestIDirectDraw
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_GetFourCCCodes::TestIDirectDraw()
    {
        eTestResult tr = trPass;
        HRESULT hr;
        DWORD dwCodes1[256],
              dwCodes2[256],
              dwCount,
              dwReportedCount;
        int i;

        // Query for the number of available codes
        Preset(dwCount);
        hr = m_piDD->GetFourCCCodes(&dwCount, NULL);
        CheckHRESULT(hr, "GetFourCCCodes", trFail);
        CheckCondition(IsPreset(dwCount), "GetFourCCCodes didn't fill Out Param", trFail);

        // Save the available codes
        dwReportedCount = dwCount;
        dbgout << "Found " << dwReportedCount << " Codes" << endl;

        if (0 == dwCount)
        {
            return trPass;
        }

        // Test the common case of having enough space
        for (i = 0; i < countof(dwCodes1); i++)
            dwCodes1[i] = i * PRESET;

        dwCount = dwReportedCount;
        hr = m_piDD->GetFourCCCodes(&dwCount, dwCodes1);
        CheckHRESULT(hr, "GetFourCCCodes", trFail);
        CheckCondition(dwCount != dwReportedCount, "GetFourCCCodes modified dwCount", trFail);
        for (i = dwCount; i < countof(dwCodes1); i++)
        {
            DWORD dwTmp=i*PRESET;
            CheckCondition(dwTmp != dwCodes1[i], "GetFourCCCodes overstepped array", trFail);\
        }

        // If there are multiple formats, test an array that's too small
        if (dwReportedCount > 1)
        {
            DWORD dwLower = dwReportedCount - 1;

            dbgout << "Testing asking for only " << dwLower << " entries" << endl;
            
            dwCount = dwLower;
            for (i = 0; i < countof(dwCodes2); i++)
                dwCodes2[i] = i * PRESET;
                
            hr = m_piDD->GetFourCCCodes(&dwCount, dwCodes2);
            QueryForKnownWinCEBug(S_OK == hr, 3585, "GetFourCCCodes does not correct a too-small NumCodes", tr |= trFail)
            else QueryForHRESULT(hr, DDERR_MOREDATA, "GetFourCCCodes", tr |= trFail)
            QueryForKnownWinCEBug(dwCount == dwLower, 3585, "GetFourCCCodes didn't update dwCount.  Passed " << dwLower
                                << ", got back " << dwCount << ", expected " << dwReportedCount, tr |= trFail)
            else QueryCondition(dwCount != dwReportedCount, "GetFourCCCodes didn't update dwCount.  Passed " << dwLower
                                << ", got back " << dwCount << ", expected " << dwReportedCount, tr |= trFail)
                                
            for (i = 0; i < (int)dwLower; i++)
                QueryCondition(dwCodes1[i] != dwCodes2[i], "Info Doesn't match : " << i, tr |= trFail);
            for (i = dwLower; i < countof(dwCodes2); i++)
            {
                DWORD dwTmp=i*PRESET;
                QueryCondition(dwTmp != dwCodes2[i], "GetFourCCCodes overstepped array : " << i, tr |= trFail);
            }
        }

        // Test the case with more space than necessary
        dbgout << "Testing with more space than necessary" << endl;
        for (i = 0; i < countof(dwCodes2); i++)
            dwCodes2[i] = i * PRESET;
            
        dwCount = countof(dwCodes2);
        hr = m_piDD->GetFourCCCodes(&dwCount, dwCodes2);
        CheckHRESULT(hr, "GetFourCCCodes", trFail);
        CheckCondition(dwCount != dwReportedCount, "GetFourCCCodes didn't modify dwCount", trFail);
        for (i = 0; i < (int)dwCount; i++)
            CheckCondition(dwCodes1[i] != dwCodes2[i], "Info Doesn't match", trFail);
        for (i = dwCount; i < countof(dwCodes2); i++)
        {
            DWORD dwTmp=i*PRESET;
            CheckCondition(dwTmp != dwCodes2[i], "GetFourCCCodes overstepped array", trFail);
        }

        // Output the FourCC codes that we found
        dbgout << "Found FourCC Codes: ";
        for (i = 0; i < (int)dwCount; i++)
            dbgout.output_formatted(TEXT("%c%c%c%c  "),
                (char)(BYTE)(dwCodes1[i] >> 0 ), (char)(BYTE)(dwCodes1[i] >> 8 ),
                (char)(BYTE)(dwCodes1[i] >> 16), (char)(BYTE)(dwCodes1[i] >> 24));
        dbgout << endl;

        return tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_RestoreDisplayMode::TestIDirectDraw
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_RestoreDisplayMode::TestIDirectDraw()
    {
        CDirectDrawConfiguration::VectDisplayMode::iterator itr;
        eTestResult tr = trPass;
        HRESULT hr;

        // Run the whole test more than once
        for (int i = 0; i < 2; i++)
        {
            // Iterate all supported modes
            dbgout << "Iterating all supported modes" << endl;
            for(itr  = g_DDConfig.m_vdmSupported.begin();
                itr != g_DDConfig.m_vdmSupported.end();
                itr++)
            {

                // Set the mode
                dbgout << "Testing mode : " << itr->m_dwWidth << "x" 
                    << itr->m_dwHeight << "x" << itr->m_dwDepth << "x" 
                    << itr->m_dwFrequency << " Hz; vga (" << itr->m_dwFlags << ")" << endl;
                hr = m_piDD->SetDisplayMode(itr->m_dwWidth, itr->m_dwHeight, itr->m_dwDepth, itr->m_dwFrequency, itr->m_dwFlags);
                CheckHRESULT(hr, "SetDisplayMode", trFail);

                // Restore the Display Mode
                dbgout << "Restoring display mode" << endl;
                hr = m_piDD->RestoreDisplayMode();
                CheckHRESULT(hr, "RestoreDisplayMode", trFail);
            }
        }
        
        return tr;
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_SetCooperativeLevel::TestIDirectDraw
    //  Tests the DirectDrawCreateClipper API.
    //
    eTestResult CTest_SetCooperativeLevel::TestIDirectDraw()
    {
        CComPointer<IDirectDraw> piDD;
        HWND hwnd = CWindowsModule::GetWinModule().m_hWnd; //g_DirectDraw.m_hwnd;
        eTestResult tr = trPass;
        HRESULT hr;
        RECT rc;
        int i=0, j=0;
        HINSTANCE hInstDDraw;      
        PFNDDRAWCREATE pfnDDCreate = NULL;
        
        // Save the window's original position, since setting to
        // fullscreen seems to resize the window to be fullscreen
        GetWindowRect(hwnd, &rc);

        // Release our private DDraw interface, since we're
        // destroying the global DDraw object
        m_piDD.ReleaseInterface();
        
        // Get rid of any global DirectDraw Object (it may have
        // already set the cooperative level)
        g_DirectDraw.ReleaseObject();

        if(NULL==(hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
        {
            dbgout << "No direct draw support on system";
            return trSkip;
        }

#ifdef UNDER_CE
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, TEXT("DirectDrawCreate"));
#else
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, "DirectDrawCreate");
#endif
        if(!pfnDDCreate)
        {
            dbgout << "Failure Creating a pointer to DirectDrawCreate" << endl;
            tr|=trSkip;
            goto Exit;
        }
        
        // Loop a couple of times for fun
        for (i = 0; i < 2; i++)
        {
            // Create a new DirectDraw Object
            hr = pfnDDCreate(NULL, piDD.AsTestedOutParam(), NULL);
            QueryCondition(DDERR_UNSUPPORTED==hr, "No direct draw support in driver", tr|=trSkip; goto Exit);
            QueryHRESULT(hr, "DirectDrawCreate", tr|=trFail; goto Exit);
            QueryCondition(piDD.InvalidOutParam(), "DirectDrawCreate succeeded but failed to set the OUT parameter", tr|=trFail; goto Exit);
            
            // Loop a couple of times for fun
            for (j = 0; j < 2; j++)
            {
                // Set the cooperative level to Normal with NULL HWND
                dbgout << "Testing Setting SetCooperativeLevel(NULL, DDSCL_NORMAL)" << endl;
                hr = piDD->SetCooperativeLevel(NULL, DDSCL_NORMAL);
                QueryHRESULT(hr, "SetCooperativeLevel", tr|=trFail; goto Exit);
 
                // Set the cooperative level to Normal with valid HWND
                dbgout << "Testing Setting SetCooperativeLevel(HWND, DDSCL_NORMAL)" << endl;
                hr = piDD->SetCooperativeLevel(CWindowsModule::GetWinModule().m_hWnd, DDSCL_NORMAL);
                QueryHRESULT(hr, "SetCooperativeLevel", tr|=trFail; goto Exit);

                // Set the cooperative level to Exclusive with valid HWND
                dbgout << "Testing Setting SetCooperativeLevel(HWND, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE)" << endl;
                hr = piDD->SetCooperativeLevel(CWindowsModule::GetWinModule().m_hWnd, DDSCL_FULLSCREEN);
                QueryHRESULT(hr, "SetCooperativeLevel", tr|=trFail; goto Exit);

            }

            // Release the DirectDraw interface that we got
            dbgout << "Releasing the DirectDraw Object" << endl;
            piDD.ReleaseInterface();
        }
        
        // Restore the window's original position
        MoveWindow(hwnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, false);

Exit:
        piDD.ReleaseInterface();
        FreeLibrary(hInstDDraw);
        return tr;
    }

//    namespace privCTest_GetSurfaceFromDC
//    {
//        typedef WINGDIAPI HBITMAP (WINAPI *CREATEBIMAPFROMPOINTERPROC)(CONST BITMAPINFO *, int, PVOID);
//        
//        WINGDIAPI HBITMAP WINAPI CallCreateBitmapFromPointer
//        (
//            CONST BITMAPINFO *  pbmi,
//            int                 iStride,
//            PVOID               pvBits
//        )
//        {
//            static CREATEBIMAPFROMPOINTERPROC pfnCreateBitmapFromPointer = NULL;
//        
//            if (!pfnCreateBitmapFromPointer)
//            {
//                static HINSTANCE hCoreDLL = NULL;
//        
//                if (hCoreDLL == NULL)
//                    hCoreDLL = LoadLibrary(TEXT("coredll.dll"));
//        
//                pfnCreateBitmapFromPointer
//                    = (CREATEBIMAPFROMPOINTERPROC)GetProcAddress(
//                                                            hCoreDLL,
//                                                            TEXT("CreateBitmapFromPointer"));
//            }
//        
//            if (pfnCreateBitmapFromPointer)
//            {
//                return pfnCreateBitmapFromPointer(pbmi, iStride, pvBits);
//            }
//        
//            return (HBITMAP)NULL;
//        }
//        
//        eTestResult TestGetSurfaceFromDC(IDirectDraw * piDD, HDC hdcTest, RECT * prcClient, bool fShouldSucceed)
//        {
//            dbgout.push_indent();
//            eTestResult tr = trPass;
//            HDC hdcFromSurface = NULL;
//            CDDSurfaceDesc cddsd;
//            HRESULT hr;
//            int iWidth = GetDeviceCaps(hdcTest, HORZRES);
//            int iHeight = GetDeviceCaps(hdcTest, VERTRES);
//            RECT rc = {0, 0, iWidth, iHeight};
//            if (prcClient)
//            {
//                rc = *prcClient;
//                iWidth = rc.right - rc.left;
//                iHeight = rc.bottom - rc.top;
//            }
//            int iBPP = GetDeviceCaps(hdcTest, BITSPIXEL);
//            int iBytesPP = iBPP/8;
//            CComPointer<IDirectDrawSurface> piDDS;
//            PatBlt(hdcTest, 0, 0, iWidth, iHeight, BLACKNESS);
//            hr = piDD->GetSurfaceFromDC(hdcTest, piDDS.AsTestedOutParam());
//            if (FAILED(hr) && !fShouldSucceed)
//            {
//                dbgout << "GetSurfaceFromDC failed, as expected, hr = " << hr << endl;
//                goto Exit;
//            }
//            else if (SUCCEEDED(hr) && !fShouldSucceed)
//            {
//                dbgout << "GetSurfaceFromDC succeeded, but should have failed" << endl;
//                tr|=trFail;
//                goto Exit;
//            }
//            QueryHRESULT(hr, "GetSurfaceFromDC", tr|=trFail; goto Exit);
//            
//            // Now that we have the surface, blt to the four corners and make sure
//            // that it shows up in the original and a subsequent GetDC'ed DC
//
//            // If possible, lock the surface using DDraw, and make sure the locked
//            // surface is correct.
//            hr = piDDS->Lock(&rc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
//            if (SUCCEEDED(hr))
//            {
//                int iOffset;
//                BYTE * pbSurface = (BYTE*)cddsd.lpSurface;
//                for (iOffset = 0; iOffset < iBytesPP; iOffset++)
//                {
//                    *(pbSurface + iOffset) = 0xff;
//                }
//                
//                for (iOffset = iBytesPP * (iWidth - 1); iOffset < iBytesPP * iWidth; iOffset++)
//                {
//                    *(pbSurface + iOffset) = 0xff;
//                }
//
//                for (iOffset = cddsd.lPitch * (iHeight - 1); 
//                    iOffset < iBytesPP + cddsd.lPitch * (iHeight - 1); 
//                    iOffset++)
//                {
//                    *(pbSurface + iOffset) = 0xff;
//                }
//
//                for (iOffset = cddsd.lPitch * (iHeight - 1) + iBytesPP * (iWidth - 1); 
//                    iOffset < cddsd.lPitch * (iHeight - 1) + iBytesPP * iWidth; iOffset++)
//                {
//                    *(pbSurface + iOffset) = 0xff;
//                }
//                hr = piDDS->Unlock(&rc);
//                QueryHRESULT(hr, "Unlock after successful Lock", tr|=trFail; goto Exit);
//                
//                // Check the pixels we set
//                QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, 0, 0), "GetPixel(0,0)", tr|=trFail);
//                QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, iWidth - 1, 0), "GetPixel(max,0)", tr|=trFail);
//                QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, 0, iHeight - 1), "GetPixel(0,max)", tr|=trFail);
//                QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, iWidth - 1, iHeight - 1), "GetPixel(max,max)", tr|=trFail);
//            }
//
//            // There currently is no support for blting in DDraw, so we'll blt to
//            // the GetDC'ed DC and check with the original.
//            // Get the DC from the surface
//            hr = piDDS->GetDC(&hdcFromSurface);
//            QueryHRESULT(hr, "GetDC", tr|=trFail; goto Exit);
//
//            // Set some pixels.
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.left, rc.top, RGB(0, 0, 0)), "SetPixel black (0,0)", tr|=trFail);
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.right - 1, rc.top, RGB(0, 0, 0)), "SetPixel black (max,0)", tr|=trFail);
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.left, rc.bottom - 1, RGB(0, 0, 0)), "SetPixel black (0,max)", tr|=trFail);
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.right - 1, rc.bottom - 1, RGB(0, 0, 0)), "SetPixel black (max,max)", tr|=trFail);
//            // Check the pixels we set
//            // 
//            QueryCondition(RGB(0x00, 0x00, 0x00) != GetPixel(hdcFromSurface, rc.left, rc.top), "GetPixel(0,0)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) != GetPixel(hdcFromSurface, rc.right - 1, rc.top), "GetPixel(max,0)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) != GetPixel(hdcFromSurface, rc.left, rc.bottom - 1), "GetPixel(0,max)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) != GetPixel(hdcFromSurface, rc.right - 1, rc.bottom - 1), "GetPixel(max,max)", tr|=trFail);
//
//            // Set some pixels.
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.left, rc.top, RGB(255, 255, 255)), "SetPixel white (0,0)", tr|=trFail);
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.right - 1, rc.top, RGB(255, 255, 255)), "SetPixel white (max,0)", tr|=trFail);
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.left, rc.bottom - 1, RGB(255, 255, 255)), "SetPixel white (0,max)", tr|=trFail);
//            QueryCondition(-1 == SetPixel(hdcFromSurface, rc.right - 1, rc.bottom - 1, RGB(255, 255, 255)), "SetPixel white (max,max)", tr|=trFail);
//            // Check the pixels we set
//            // 
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcFromSurface, rc.left, rc.top), "GetPixel(0,0)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcFromSurface, rc.right - 1, rc.top), "GetPixel(max,0)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcFromSurface, rc.left, rc.bottom - 1), "GetPixel(0,max)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcFromSurface, rc.right - 1, rc.bottom - 1), "GetPixel(max,max)", tr|=trFail);
//
//            hr = piDDS->ReleaseDC(hdcFromSurface);
//            QueryHRESULT(hr, "ReleaseDC", tr|=trFail; goto Exit);
//
//            piDDS.ReleaseInterface();
//            
//            // Check the pixels we set
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, 0, 0), "GetPixel(0,0)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, iWidth - 1, 0), "GetPixel(max,0)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, 0, iHeight - 1), "GetPixel(0,max)", tr|=trFail);
//            QueryCondition(RGB(0x00, 0x00, 0x00) == GetPixel(hdcTest, iWidth - 1, iHeight - 1), "GetPixel(max,max)", tr|=trFail);
//Exit:
//            dbgout.pop_indent();
//            piDDS.ReleaseInterface();
//            return tr;
//        }
//        
//        eTestResult EnumerateBitmapsWithDC(IDirectDraw * piDD, HDC hdcTest, HDC hdcOriginal)
//        {
//            dbgout.push_indent();
//            HBITMAP hbmp = NULL;
//            eTestResult tr = trPass;
//            int iWidth = 120;
//            int iHeight = 80;
//            int iIndex = 0;
//            void * pvBitmap = NULL;
//            RECT rc = {0, 0, iWidth, iHeight};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[2];
//            } bmi1 = {
//                {sizeof(bmi1), 120, 80, 1, 1, BI_RGB, 0, 0, 0, 0, 0}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[4];
//            } bmi2 = {{sizeof(bmi2), 120, 80, 1, 2, BI_RGB, 0, 0, 0, 0, 0}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[16];
//            } bmi4 = {{sizeof(bmi4), 120, 80, 1, 4, BI_RGB, 0, 0, 0, 0, 0}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[256];
//            } bmi8 = {{sizeof(bmi8), 120, 80, 1, 8, BI_RGB, 0, 0, 0, 0, 0}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[3];
//            } bmi15 = {{sizeof(bmi15), 120, 80, 1, 16, BI_BITFIELDS, 2 * 120 * 80, 0, 0, 0, 0},
//                {(COLORREF)0x7b00, (COLORREF)0x03e0, (COLORREF)0x1f}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[3];
//            } bmi16 = {{sizeof(bmi16), 120, 80, 1, 16, BI_BITFIELDS, 2 * 120 * 80, 0, 0, 0, 0},
//                {(COLORREF)0xf800, (COLORREF)0x07e0, (COLORREF)0x1f}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[1];
//            } bmi24 = {{sizeof(bmi24), 120, 80, 1, 24, BI_RGB, 0, 0, 0, 0, 0}};
//            struct
//            {
//                BITMAPINFOHEADER bmiHeader;
//                COLORREF bmiColors[3];
//            } bmi32 = {{sizeof(bmi32), 120, 80, 1, 32, BI_BITFIELDS, 4 * 120 * 80, 0, 0, 0, 0},
//                {(COLORREF)0xff0000, (COLORREF)0xff00, (COLORREF)0xff}};
//            
//            BITMAPINFO * rgpBI[] = {
//                (BITMAPINFO*)&bmi1, 
//                (BITMAPINFO*)&bmi2, 
//                (BITMAPINFO*)&bmi4, 
//                (BITMAPINFO*)&bmi8, 
//                (BITMAPINFO*)&bmi15, 
//                (BITMAPINFO*)&bmi16, 
//                (BITMAPINFO*)&bmi24, 
//                (BITMAPINFO*)&bmi32, 
//            };
//
//            // Try with compatible Bitmap
//            dbgout << "Using Bitmap from CreateCompatibleBitmap" << endl;
//            hbmp = CreateCompatibleBitmap(hdcOriginal, iWidth, iHeight);
//            QueryCondition(hbmp == NULL, "CreateCompatibleBitmap", tr|=trAbort; goto Exit);
//            hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//            tr |= TestGetSurfaceFromDC(piDD, hdcTest, &rc, true);
//            hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//            QueryCondition(!DeleteObject(hbmp), "DeleteObject", tr|=trFail);
//            hbmp = NULL;
//
//            dbgout << "Using Bitmap from CreateDIBSection";
//            dbgout.push_indent();
//            for (iIndex = 0; iIndex < countof(rgpBI); iIndex++)
//            {
//                UINT uiUsage;
//                void * pvBits;
//                dbgout << rgpBI[iIndex]->bmiHeader.biBitCount << " BPP" << endl;
//                if (rgpBI[iIndex]->bmiHeader.biBitCount > 8)
//                    uiUsage = DIB_RGB_COLORS;
//                else
//                    uiUsage = DIB_PAL_COLORS;
//                hbmp = CreateDIBSection(hdcTest, rgpBI[iIndex], uiUsage, &pvBits, NULL, 0);
//                QueryCondition(hbmp == NULL, "CreateDIBSection", tr|=trAbort; continue);
//                hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//                tr |= TestGetSurfaceFromDC(piDD, hdcTest, &rc, rgpBI[iIndex]->bmiHeader.biBitCount >= 8);
//                hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//                QueryCondition(!DeleteObject(hbmp), "DeleteObject", tr|=trFail);
//                hbmp = NULL;
//            }
//            dbgout.pop_indent();
//            
//            dbgout << "Using Bitmap from CreateBitmap";
//            dbgout.push_indent();
//            for (iIndex = 0; iIndex < countof(rgpBI); iIndex++)
//            {
//                dbgout << rgpBI[iIndex]->bmiHeader.biBitCount << " BPP" << endl;
//
//                hbmp = CreateBitmap(
//                    rgpBI[iIndex]->bmiHeader.biWidth, 
//                    rgpBI[iIndex]->bmiHeader.biHeight, 
//                    1,
//                    rgpBI[iIndex]->bmiHeader.biBitCount,
//                    NULL);
//                QueryCondition(hbmp == NULL, "CreateBitmap", tr|=trAbort; continue);
//                hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//                tr |= TestGetSurfaceFromDC(piDD, hdcTest, &rc, rgpBI[iIndex]->bmiHeader.biBitCount >= 8);
//                hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//                QueryCondition(!DeleteObject(hbmp), "DeleteObject", tr|=trFail);
//                hbmp = NULL;
//            }
//            dbgout.pop_indent();
//
//            dbgout << "Using Bitmap from CreateBitmapFromPointer";
//            dbgout.push_indent();
//            for (iIndex = 0; iIndex < countof(rgpBI); iIndex++)
//            {
//                dbgout << rgpBI[iIndex]->bmiHeader.biBitCount << " BPP" << endl;
//                int iStride = 
//                    ((rgpBI[iIndex]->bmiHeader.biWidth * rgpBI[iIndex]->bmiHeader.biBitCount + 7) / 8 + 3) & ~3; 
//                int iCurrentHeight = rgpBI[iIndex]->bmiHeader.biHeight;
//                if (((__int64)iStride) * ((__int64)iCurrentHeight) > 0x7fffffff)
//                {
//                    dbgout << "Bitmap size overflow" << endl;
//                    continue;
//                }
//                pvBitmap = LocalAlloc(LPTR, iStride * iCurrentHeight);
//                if (!pvBitmap)
//                {
//                    dbgout << "Could not allocate bitmap" << endl;
//                    continue;
//                }
//
//                hbmp = CallCreateBitmapFromPointer(
//                    rgpBI[iIndex],
//                    iStride,
//                    pvBitmap);
//                QueryCondition(hbmp == NULL, "CreateBitmapFromPointer", tr|=trAbort; continue);
//                hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//                tr |= TestGetSurfaceFromDC(piDD, hdcTest, &rc, rgpBI[iIndex]->bmiHeader.biBitCount >= 8);
//                hbmp = (HBITMAP) SelectObject(hdcTest, (HGDIOBJ)hbmp);
//                QueryCondition(!DeleteObject(hbmp), "DeleteObject", tr|=trFail);
//                hbmp = NULL;
//                LocalFree(pvBitmap);
//                pvBitmap = NULL;
//            }
//            dbgout.pop_indent();
//Exit:
//            if (hbmp)
//                QueryCondition(!DeleteObject(hbmp), "DeleteObject", tr|=trFail);
//            if (pvBitmap)
//                LocalFree(pvBitmap);
//            dbgout.pop_indent();
//            return tr;
//        }
//        eTestResult EnumerateDCTypes(IDirectDraw * piDD, HWND hwnd)
//        {
//            HDC hdcTest = NULL;
//            HDC hdcCompat = NULL;
//            eTestResult tr = trPass;
//            RECT rcClient;
//            POINT ptClientOffset;
//
//            // Test with DC created with CreateDC
//            dbgout << "Using CreateDC to get DC" << endl;
//            hdcTest = CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
//            if (NULL == hdcTest)
//            {
//                dbgout(LOG_ABORT) << "CreateDC returned NULL, GLE: " << GetLastError() << endl;
//                tr |= trAbort;
//            }
//            tr |= TestGetSurfaceFromDC(piDD, hdcTest, NULL, true);
//            DeleteDC(hdcTest);
//
//            // Test with DC created with GetDC(hwnd)
//            dbgout << "Using GetDC with window to get DC" << endl;
//            hdcTest = GetDC(hwnd);
//            if (NULL == hdcTest)
//            {
//                dbgout(LOG_ABORT) << "GetDC(hwnd) returned NULL, GLE: " << GetLastError() << endl;
//                tr |= trAbort;
//            }
//            GetClientRect(hwnd, &rcClient);
//            ptClientOffset.x = ptClientOffset.y = 0;
//            ClientToScreen(hwnd, &ptClientOffset);
//            OffsetRect(&rcClient, 
//                ptClientOffset.x, 
//                ptClientOffset.y);
//            tr |= TestGetSurfaceFromDC(piDD, hdcTest, &rcClient, true);
//            ReleaseDC(hwnd, hdcTest);
//
//            // Test with DC created with GetDC(NULL)
//            dbgout << "Using GetDC with NULL to get DC" << endl;
//            hdcTest = GetDC(NULL);
//            if (NULL == hdcTest)
//            {
//                dbgout(LOG_ABORT) << "GetDC(NULL) returned NULL, GLE: " << GetLastError() << endl;
//                tr |= trAbort;
//            }
//            tr |= TestGetSurfaceFromDC(piDD, hdcTest, NULL, true);
//            ReleaseDC(NULL, hdcTest);
//
//            // Test with compatible DC
//            dbgout << "Using CreateCompatibleDC to get DC" << endl;
//            hdcTest = GetDC(NULL);
//            if (NULL == hdcTest)
//            {
//                dbgout(LOG_ABORT) << "GetDC(NULL) returned NULL, GLE: " << GetLastError() << endl;
//                tr |= trAbort;
//            }
//            hdcCompat = CreateCompatibleDC(hdcTest);
//            if (NULL == hdcCompat)
//            {
//                dbgout(LOG_ABORT) << "CreateCompatibleDC returned NULL, GLE: " << GetLastError() << endl;
//                tr |= trAbort;
//            }
//            tr |= EnumerateBitmapsWithDC(piDD, hdcCompat, hdcTest);
//            ReleaseDC(NULL, hdcTest);
//            DeleteDC(hdcCompat);
//            return tr;
//        }
//    }
//    
//    ////////////////////////////////////////////////////////////////////////////////
//    // CTest_SetCooperativeLevel::TestIDirectDraw
//    //  Tests the DirectDrawCreateClipper API.
//    //
//    eTestResult CTest_GetSurfaceFromDC::TestIDirectDraw()
//    {
//        HWND hwnd = CWindowsModule::GetWinModule().m_hWnd; //g_DirectDraw.m_hwnd;
//        eTestResult tr = trPass;
//        HRESULT hr;
//        tr |= privCTest_GetSurfaceFromDC::EnumerateDCTypes(m_piDD.AsInParam(), hwnd);
//        return tr;
//    }

    namespace privCTest_ReceiveWMUnderFSE
    {
        int g_WMCounter=0; // the global counter used for CTest_ReceiveWMUnderFSE(), under the private namespace

        long FAR PASCAL
        WinTestProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            switch (message)
            {
                case WM_ACTIVATE:
                    if (wParam == WA_INACTIVE)
                    {
                        dbgout << "WMACTIVATE message received, with WA_INACTIVE, " ;
                    }
                    else if (wParam == WA_ACTIVE)
                    {                
                        dbgout << "WMACTIVATE message received, with WA_ACTIVE, " ;
                    }
                    g_WMCounter++;
                    dbgout << "increase g_WMCounter to " << g_WMCounter << endl;
                    break;
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }

        HRESULT OpenWindow(HINSTANCE hInstance, HWND& hWnd, WNDPROC WndProc, LPCWSTR name)
        {
            WNDCLASS wc;
            // Set up and register window class
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WndProc;
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hInstance = hInstance;
            wc.hIcon = NULL;
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
            wc.lpszMenuName = NULL;
            wc.lpszClassName = name;
            RegisterClass(&wc);

            // Create a window
            hWnd = CreateWindowEx(WS_EX_TOPMOST,
                                  name,
                                  name,
                                  WS_POPUP,
                                  0,
                                  0,
                                  GetSystemMetrics(SM_CXSCREEN),
                                  GetSystemMetrics(SM_CYSCREEN),
                                  NULL,
                                  NULL,
                                  hInstance,
                                  NULL);
            if (!hWnd)
                return FALSE;

            return true;
        }

        HRESULT SetupDDrawSurface(HWND hWnd, IDirectDraw* piDD, LPDIRECTDRAWSURFACE* ppDDS)
        {
            HRESULT                     hRet;
            DDSURFACEDESC               ddsdTest;

            // Set as exclusive mode
            hRet = piDD->SetCooperativeLevel(hWnd, DDSCL_FULLSCREEN);
            if (hRet != DD_OK)
            {
                dbgout (LOG_FAIL) << "SetCooperativeLevel FAILED" << endl;
                return trFail;
            }
            else
            {
                dbgout << "SetCooperativeLevel as FULLSCREEN succesfully" << endl;
            }

            memset(&ddsdTest, 0, sizeof(ddsdTest)); //fill all entries with 0
            ddsdTest.dwSize = sizeof(ddsdTest); //set size
            ddsdTest.dwFlags = DDSD_CAPS;
            ddsdTest.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;//
            hRet = piDD->CreateSurface(&ddsdTest, ppDDS, NULL);  //create surface using ddsd structure information
            if (hRet != DD_OK)
            {
                dbgout (LOG_FAIL) << "CreateSurface FAILED" << endl;
                return trFail;
            }
            return DD_OK;
        }   
    }

    eTestResult CTest_ReceiveWMUnderFSE::TestIDirectDraw()
    {
        using namespace             privCTest_ReceiveWMUnderFSE;
        HWND                        hWndTest = NULL;   // Full screen window
        HWND                        hWndTest2 = NULL;  // Dummy window
        eTestResult tr = trPass;
        HRESULT hr;

        CComPointer<IDirectDraw> piDD;
        CComPointer<IDirectDrawSurface> piDDS;
        HINSTANCE hInstDDraw = NULL;      
        PFNDDRAWCREATE pfnDDCreate = NULL;

        // Release our private DDraw interface, since we're
        // destroying the global DDraw object
        m_piDD.ReleaseInterface();
        
        // Get rid of any global DirectDraw Object (it may have
        // already set the cooperative level)
        g_DirectDraw.ReleaseObject();
        if(NULL==(hInstDDraw = LoadLibrary(TEXT("ddraw.dll"))))
        {
            dbgout << "No direct draw support on system";
            return trSkip;
        }

    #ifdef UNDER_CE
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, TEXT("DirectDrawCreate"));
    #else
        pfnDDCreate = (PFNDDRAWCREATE)GetProcAddress(hInstDDraw, "DirectDrawCreate");
    #endif
        if(!pfnDDCreate)
        {
            dbgout << "Fail to create a pointer to DirectDrawCreate" << endl;
            tr|=trSkip;
            goto Exit;
        }

        // Create test window
        if (!(OpenWindow(GetModuleHandle(NULL), hWndTest, WinTestProc, _T("Test Window"))))
        {
            dbgout (LOG_FAIL) << "Fail to create the test window" << endl;
            tr|= trFail;
            goto Exit;
        }
        ShowWindow(hWndTest, SW_SHOWNORMAL);
        SetForegroundWindow(hWndTest);


       // Create a new DirectDraw Object
        hr = pfnDDCreate(NULL, piDD.AsTestedOutParam(), NULL);
        QueryCondition(DDERR_UNSUPPORTED==hr, "No direct draw support in driver", tr|=trSkip; goto Exit);
        QueryHRESULT(hr, "DirectDrawCreate", tr|=trFail; goto Exit);
        QueryCondition(piDD.InvalidOutParam(), "DirectDrawCreate succeeded but failed to set the OUT parameter", tr|=trFail; goto Exit);

        // Setup cooperative level as exclusive and create a primary surface
        tr = SetupDDrawSurface (hWndTest, piDD.AsInParam(), piDDS.AsTestedOutParam());
        if (tr!= DD_OK)
        {
            dbgout (LOG_FAIL) << "Fail to set cooperative leven and create DDraw surface for the test window" << endl;
            goto Exit;
        }               

        // now handling message for WndTest, will get WM_ACTIVATE with WA_ACTIVATE 
        MessagePump(hWndTest);
        g_WMCounter = 0;                  //reset global counter
        dbgout << "Need to check we can get WM_ACTIVATE message after now, reset g_WMCounter to 0" << endl;

        // Create dummy window
        if (!(OpenWindow(GetModuleHandle(NULL), hWndTest2, DefWindowProc, _T("Dummy Window"))))
        {
            dbgout (LOG_FAIL) << "Fail to create the dummy window" << endl;
            tr|= trFail;
            goto Exit;
        }
        MessagePump(hWndTest2);
        
        SetForegroundWindow(hWndTest2);    // WM_ACTIVATE with WA_INACTIVATE is sent to hWndTest now

        if (g_WMCounter > 0)              // if the WM_ACTIVATE message is handled by WndTestProc, counter will be greater than 0
        {
            tr= trPass;
        }
        else                            // no WM_ACTIVATE message is received, fail the test case
        {
            tr= trFail;
        }

        // Release the DirectDraw interface that we got
Exit:

        if (hWndTest != NULL)
        {
            DestroyWindow(hWndTest);
            MessagePump(hWndTest);
        }

        if (hWndTest2 != NULL)
        {
            DestroyWindow(hWndTest2);
            MessagePump(hWndTest2);
        }

        if (piDDS!=NULL)
        {
            piDDS.ReleaseInterface();
        }

        if (piDD!=NULL)
        {
            piDD.ReleaseInterface();
        }

        if (hInstDDraw!=NULL)
        {
            FreeLibrary(hInstDDraw);
        }

        return tr;
    }
}
