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

#include "IDDSurfaceTests.h"
#include "IUnknownTests.h"
#pragma warning(disable:4995)
using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;
using namespace KnownBugUty;

namespace Test_IDirectDrawSurface
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_AddRefRelease::Test
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_AddRefRelease::TestIDirectDrawSurface()
    {
        CComPointer<IUnknown> punk;
        eTestResult tr = trPass;
        HRESULT hr;

        // Get the IUnknown Interface
        hr = m_piDDS->QueryInterface(IID_IUnknown, (void**)punk.AsTestedOutParam());
        CheckHRESULT(hr, "QI for IUnknown", trFail);
        CheckCondition(punk.InvalidOutParam(), "QI succeeded, but didn't fill out param", trFail);

        tr |= Test_IUnknown::TestAddRefRelease(punk.AsInParam());

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_QueryInterface::Test
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_QueryInterface::TestIDirectDrawSurface()
    {
        std::vector<IID> vectValidIIDs,
                         vectInvalidIIDs;
        CComPointer<IUnknown> punk;
        eTestResult tr = trPass;
        HRESULT hr;

        // The IIDs that should work
        vectValidIIDs.push_back(IID_IDirectDrawSurface);

        // The IIDs that shouldn't work
        vectInvalidIIDs.push_back(IID_IDirect3D);
        vectInvalidIIDs.push_back(IID_IDirect3D2);
        vectInvalidIIDs.push_back(IID_IDirect3D3);
        vectInvalidIIDs.push_back(IID_IDirect3DDevice);
        vectInvalidIIDs.push_back(IID_IDirect3DDevice2);
        vectInvalidIIDs.push_back(IID_IDirect3DDevice3);
        vectInvalidIIDs.push_back(IID_IDirect3DTexture);
        vectInvalidIIDs.push_back(IID_IDirect3DTexture2);
        vectInvalidIIDs.push_back(IID_IDirectDraw);
        vectInvalidIIDs.push_back(IID_IDirectDrawClipper);
        // KNOWNBUG: HPURAID 9060
        // If we QI for this, the RefCount gets inflated.
        vectInvalidIIDs.push_back(IID_IDirectDrawGammaControl);

        // See if this surface should support the color control
        if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            if (g_DDConfig.HALCaps().dwMiscCaps & DDMISCCAPS_COLORCONTROLPRIMARY)
                vectValidIIDs.push_back(IID_IDirectDrawColorControl);
            else
                vectInvalidIIDs.push_back(IID_IDirectDrawColorControl);
        }
        else if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY)
        {
            if (g_DDConfig.HALCaps().dwMiscCaps & DDMISCCAPS_COLORCONTROLOVERLAY)
                vectValidIIDs.push_back(IID_IDirectDrawColorControl);
            else
                vectInvalidIIDs.push_back(IID_IDirectDrawColorControl);
        }

        // Get the IUnknown Interface
        hr = m_piDDS->QueryInterface(IID_IUnknown, (void**)punk.AsTestedOutParam());
        CheckHRESULT(hr, "QI for IUnknown", trFail);
        CheckCondition(punk.InvalidOutParam(), "QI succeeded, but didn't fill out param", trFail);

        tr |= Test_IUnknown::TestQueryInterface(punk.AsInParam(), vectValidIIDs, vectInvalidIIDs);

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetCaps::TestIDirectDrawSurface
    //  Tests the GetCaps Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetCaps::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;
        DDSCAPS ddscaps;
        HRESULT hr;

        // We always need to have valid CAPS
        ASSERT(m_cddsd.dwFlags & DDSD_CAPS);

        // Get the caps
        dbgout << "Getting Caps" << endl;
        hr = m_piDDS->GetCaps(&ddscaps);
        CheckHRESULT(hr, "GetCaps", trFail);

        // It's valid for the returned caps to have extra bits, but
        // they must have at least the same bits we used to create
        // the surface
        dbgout  << "ddscaps = " << ddscaps << endl
                << "m_cddsd.ddsCaps = " << m_cddsd.ddsCaps << endl;
        CheckCondition((ddscaps.dwCaps & m_cddsd.ddsCaps.dwCaps) != m_cddsd.ddsCaps.dwCaps, "Caps don't match", trFail);

        // We should be able to get the surface description and have
        // the same caps as well
        dbgout << "Getting Surface Description" << endl;
        CDDSurfaceDesc cddsd;

        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trFail)
        CheckCondition(ddscaps.dwCaps != cddsd.ddsCaps.dwCaps, "Caps don't match those from GetSurfaceDesc", trFail);

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSurfaceDescription::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSurfaceDescription::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT hr;

        CDDSurfaceDesc cddsd,
                       cddsd2;

        // Get the surface description twice
        dbgout << "Getting surface description" << endl;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trFail);
        hr = m_piDDS->GetSurfaceDesc(&cddsd2);
        CheckHRESULT(hr, "GetSurfaceDesc", trFail);

        // Compare the results
        CheckCondition(memcmp(&cddsd, &cddsd2, sizeof(cddsd)), "GetSurfaceDesc returned different info in different calls", trFail);

        // See if what we got is compatable with what we
        // were passed
        tr |= CompareSurfaceDescs(cddsd, m_cddsd);

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetPixelFormat::TestIDirectDrawSurface
    //  Tests the GetPixelFormat Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetPixelFormat::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;
        CDDPixelFormat cddpf1,
                       cddpf2;
        HRESULT hr;

        // Get the pixel format twice, and compare the results
        dbgout << "Comparing the Pixel formats from two calls" << endl;
        hr = m_piDDS->GetPixelFormat(&cddpf1);
        CheckHRESULT(hr, "GetPixelFormat", trFail);
        hr = m_piDDS->GetPixelFormat(&cddpf2);
        CheckHRESULT(hr, "GetPixelFormat", trFail);
        CheckCondition(memcmp(&cddpf1, &cddpf2, sizeof(cddpf1)), "Differing Formats", trFail);

        // Verify the Pixel Format with what we get back
        // from GetSurfaceDesc
        dbgout << "Comparing the Pixel format from that from GetSurfaceDesc" << endl;
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        CheckCondition(!(cddsd.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled in", trAbort);
        CheckCondition(memcmp(&cddpf1, &cddsd.ddpfPixelFormat, sizeof(cddpf1)), "GetSurfaceDesc Differs from GetPixelFormat", trFail);

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // privCTest_GetReleaseDeviceContext
    //  Helper namespace for GetReleaseDeviceContext.
    //
    namespace privCTest_GetReleaseDeviceContext
    {
        HFONT CreateSimpleFont(LPTSTR pszTypeface, LONG lSize)
        {
            LOGFONT lf;

            // Setup the font
            lf.lfHeight         = lSize;
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
            _tcsncpy_s(lf.lfFaceName, pszTypeface, countof(lf.lfFaceName));
            lf.lfFaceName[countof(lf.lfFaceName) - 1] = 0;

            return CreateFontIndirect(&lf);
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetReleaseDeviceContext::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetReleaseDeviceContext::TestIDirectDrawSurface()
    {
        using namespace privCTest_GetReleaseDeviceContext;

        const TCHAR szMessage[] = TEXT("Testing GetDC");
        RECT rc = { 0, 0, 0, 0 };
        eTestResult tr = trPass;
        HRESULT hr;
        HDC hdc,
            hdc2;

        // Set up our destination rectangle coordiantes based our our
        // cooperative level
        if (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)
            GetWindowRect(g_DirectDraw.m_hWnd, &rc);
        else
        {
            CDDSurfaceDesc cddsd;

            // Get the height and width
            dbgout << "Getting surface description" << endl;
            hr = m_piDDS->GetSurfaceDesc(&cddsd);
            CheckCondition(!(cddsd.dwFlags & DDSD_HEIGHT) || !(cddsd.dwFlags & DDSD_WIDTH), "Height and Width not available", trAbort);

            rc.left = rc.top = 0;
            rc.right = cddsd.dwWidth;
            rc.bottom = cddsd.dwHeight;
        }

        dbgout << "Dest Rect = " << rc << endl;

        // Get the DC
        dbgout << "Getting the DC" << endl;
        PresetUty::Preset(hdc);
        hr = m_piDDS->GetDC(&hdc);
        if (DDERR_GENERIC == hr &&
            m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
            m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
        {
            // If we failed but it's a primary in system memory, then just get out
            dbgout << "Failed on Sysmem Primary -- Valid condition -- skipping" << endl;
            tr = trPass;
        }
        else if((DDERR_CANTCREATEDC == hr) &&
                (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL) &&
                (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
               )
        {
           // If we failed but it's a primary in windowed mode, then just get out
            dbgout << "Failed on Sysmem in Windowed mode -- Valid condition -- skipping" << endl;
            tr = trPass;
        }
        else
        {
            CheckHRESULT(hr, "GetDC", trFail);
            CheckCondition(PresetUty::IsPreset(hdc), "GetDC Failed to fill in DC", trFail);

            // Attempt to get the DC again
            hr = m_piDDS->GetDC(&hdc2);
            if(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)
            {
                // If the surface owns a dc, GetDC() should not return an error.
                CheckForHRESULT(hr, S_OK, "Getting DC Again", trFail);
            }
            else
            {
                CheckForHRESULT(hr, DDERR_DCALREADYCREATED, "Getting DC Again", trFail);
            }

            // Release and re-get the HDC
            dbgout << "Releasing and re-getting the DC" << endl;
            hr = m_piDDS->ReleaseDC(hdc);
            CheckHRESULT(hr, "Release", trFail);
            hr = m_piDDS->GetDC(&hdc);
            CheckHRESULT(hr, "GetDC", trFail);

            // Create a simple font
            dbgout << "Creating font" << endl;
            HFONT hfontNew = CreateSimpleFont((LPTSTR)TEXT("Arial"), 12);
            QueryCondition(NULL == hfontNew, "Unable to create font", tr |= trAbort)
            else
            {
                // Start using our new font
                HFONT hfontOld = (HFONT)SelectObject(hdc, hfontNew);

                // Create a background brush
                dbgout << "Creating brush" << endl;
                HBRUSH hbrushNew = CreateSolidBrush(RGB(0xFF, 0x00, 0x88));
                QueryCondition(NULL == hbrushNew, "Unable to create brush", tr |= trAbort)
                else
                {
                    // Start using our new brush
                    HBRUSH hbrushOld = (HBRUSH)SelectObject(hdc, hbrushNew);

                    // Draw a simple picture
                    dbgout << "Drawing the elipse" << endl;
                    Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);

                    // Do some interesting BitBlts between the HDC
                    BOOL fSuccess;
                    const int nHalfHeight = (rc.bottom-rc.top)/2;
                    const int nHalfWidth = (rc.right-rc.left)/2;

                    fSuccess = BitBlt(hdc, rc.left, rc.top, nHalfWidth, nHalfHeight, hdc, rc.left+nHalfWidth, rc.top+nHalfHeight, SRCCOPY);
                    QueryCondition(!fSuccess, "BitBlt bottom-right to top-left with SRCCOPY", tr |= trFail);

                    fSuccess = BitBlt(hdc, rc.left+nHalfWidth, rc.top, nHalfWidth, nHalfHeight, hdc, rc.left+nHalfWidth, rc.top+nHalfHeight, SRCINVERT);
                    QueryCondition(!fSuccess, "BitBlt bottom-right to top-right with SRCINVERT", tr |= trFail);

                    fSuccess = BitBlt(hdc, rc.left, rc.top+nHalfHeight, nHalfWidth, nHalfHeight, hdc, rc.left+nHalfWidth, rc.top+nHalfHeight, SRCERASE);
                    QueryCondition(!fSuccess, "BitBlt bottom-right to bottom-left with SRCERASE", tr |= trFail);

                    fSuccess = BitBlt(hdc, rc.left, rc.top, nHalfWidth, nHalfHeight, hdc, rc.left, rc.top, SRCCOPY);
                    QueryCondition(!fSuccess, "BitBlt top-left to top-left with SRCCOPY", tr |= trFail);

                    fSuccess = BitBlt(hdc, rc.left+(nHalfWidth/2), rc.top+(nHalfHeight/2), nHalfWidth, nHalfHeight, hdc, rc.left, rc.top, SRCPAINT);
                    QueryCondition(!fSuccess, "BitBlt top-left to center with SRCPAINT", tr |= trFail);

                    // Restore original brush, and  verify that we got back
                    // the brush we put in originally.
                    HBRUSH hbrushTest = (HBRUSH)SelectObject(hdc, hbrushOld);
                    QueryCondition(hbrushNew != hbrushTest, "Unable to retrive custom font", tr |= trFail);

                    // Delete the brush we created
                    DeleteObject(hbrushNew);
                }

                // Draw some Text
                dbgout << "Drawing text" << endl;
                ExtTextOut(hdc, 0, (rc.bottom + rc.top)/2, 0, NULL, szMessage, countof(szMessage)-1, NULL);

                // Restore original font, and  verify that we got back
                // the font we put in originally.
                HFONT hfontTest = (HFONT)SelectObject(hdc, hfontOld);
                QueryCondition(hfontNew != hfontTest, "Unable to retrive custom font", tr |= trFail);

                // Delete the font we created
                DeleteObject(hfontNew);
            }

            dbgout << "Releasing the DC" << endl;
            hr = m_piDDS->ReleaseDC(hdc);
            CheckHRESULT(hr, "Release", trFail);
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSetPalette::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSetPalette::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_LockUnlock::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_LockUnlock::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;

        const DWORD dwPreset = 0xBAADF00D;
        RECT rc = { 0, 0, 0, 0 };
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsd;
        HRESULT hr;
        LPRECT prcBoundRect = NULL; // Will be NULL for all surfaces except a Primary in Windowed mode.

        // Set up our destination rectangle coordinates for a windowed primary
        if((g_DDConfig.CooperativeLevel() == DDSCL_NORMAL) && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
        {
             tr = GetClippingRectangle(m_piDDS.AsInParam(), rc);
             if(trPass != tr)
             {
                 dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                 return tr;
             }

             // To eliminate the alighment exception, we need to make sure the RECT's left is not odd number
             if (rc.left%2 != 0)  // If rc.left is odd
             {
                rc.left=(rc.left/2)*2;  //make it even
             }

              prcBoundRect = &rc; // use the Window's bounding rectangle.

              // GetSurfaceDesc for a Primary in Windowed mode, would return
              // the height, width, pitch, xpitch and surface size for the actual primary.
              // Lock on the other hand would return the values for the locked region.
              // So compare only the pixel format and caps in a call to CompareSurfaceDesc.
              m_cddsd.dwFlags = DDSD_PIXELFORMAT | DDSD_CAPS; 
        }
        else
        {
            CDDSurfaceDesc cddsdTemp;

            // Get the height and width
            dbgout << "Getting surface description" << endl;
            hr = m_piDDS->GetSurfaceDesc(&cddsdTemp);
            CheckCondition(!(cddsdTemp.dwFlags & DDSD_HEIGHT) || !(cddsdTemp.dwFlags & DDSD_WIDTH), "Height and Width not available", trAbort);

            rc.left = rc.top = 0;
            rc.right = cddsdTemp.dwWidth;
            rc.bottom = cddsdTemp.dwHeight;
        }

        // Lock the surface with no rect and wait flag
        dbgout << "Lock with wait and no RECT" << endl;
        hr = m_piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
        if (DDERR_GENERIC == hr &&
            m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
            m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
        {
            // If we failed but it's a primary in system memory, then just get out
            dbgout << "Failed on Sysmem Primary -- Valid condition -- skipping" << endl;
            tr = trPass;
        }
        else
        {
            CheckHRESULT(hr, "Lock with no rect and DDLOCK_WAITNOTBUSY flag", trFail);
            // dbgout << "Got SurfaceDesc : " << cddsd << endl;

            // Make sure the description is compatable with the
            // one we were passed.
            tr |= CompareSurfaceDescs(cddsd, m_cddsd);

            // Verify that we have a valid surface pointer
            // Store a preset pixel value at the top left coordinate.
            *(DWORD*)cddsd.lpSurface = dwPreset;

            // Attempt to lock again
            dbgout << "Locking while already locked" << endl;
            hr = m_piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            QueryForHRESULT(hr, DDERR_SURFACEBUSY, "Lock while already locked", tr |= trFail);

            // Unlock
            dbgout << "Unlock" << endl;
            hr = m_piDDS->Unlock(prcBoundRect);
            CheckHRESULT(hr, "Unlock", trFail);

            // Lock using an explicit rect
            dbgout << "Lock with explicit RECT" << endl;
            hr = m_piDDS->Lock(&rc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock with explicit rect and flag", trFail);

            // Make sure the description is compatable with the
            // one we were passed.
            tr |= CompareSurfaceDescs(cddsd, m_cddsd);

            // 0, 0 of the passed rect should correspond with the top
            // of the whole surface which we wrote te earlier
            dbgout << "Verify surface contents" << endl;
            QueryCondition(*(DWORD*)cddsd.lpSurface != dwPreset, "Failed to find expected data", tr |= trFail);

            // Unlock
            dbgout << "Unlock" << endl;
            hr = m_piDDS->Unlock(&rc);
            CheckHRESULT(hr, "Unlock", trFail);

            // Lock without the wait flag
            dbgout << "Lock without wait and no RECT" << endl;
            while (DDERR_SURFACEBUSY == (hr = m_piDDS->Lock(prcBoundRect, &cddsd, 0, NULL)));
            CheckHRESULT(hr, "Lock without WAIT flag", trFail);

            // Make sure the description is compatable with the
            // one we were passed.
            tr |= CompareSurfaceDescs(cddsd, m_cddsd);

            // Unlock
            dbgout << "Unlock" << endl;
            hr = m_piDDS->Unlock(prcBoundRect);
            CheckHRESULT(hr, "Unlock", trFail);
        }

        return tr;
    }

#if CFG_TEST_IDirectDrawClipper
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSetClipper::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSetClipper::TestIDirectDrawSurface()
    {
        CComPointer<IDirectDrawClipper> piDDCSave,
                                        piDDCTest,
                                        piDDC;
        eTestResult tr = trPass;
        BOOL fChanged;
        HRESULT hr;

        // See if their is an attached clipper coming in
        if (SUCCEEDED(m_piDDS->GetClipper(piDDCSave.AsTestedOutParam())))
        {
            dbgout << "Found Clipper already attached -- saving" << endl;
            CheckCondition(piDDCSave.InvalidOutParam(), "GetClipper Succeeded, but didn't fill in clipper", trFail);

            // Make a simple call on the interface to ensure that it's valid
            hr = piDDCSave->IsClipListChanged(&fChanged);
            CheckHRESULT(hr, "IsClipListChanged", trFail);
        }

        // Create a clipper to work with
        dbgout << "Creating Clipper" << endl;
        hr = m_piDD->CreateClipper(0, piDDC.AsTestedOutParam(), NULL);
        CheckHRESULT(hr, "CreateClipper", trAbort);
        CheckCondition(piDDC.InvalidOutParam(), "CreateClipper succeeded, but didn't fill in clipper", trAbort);

        // Make some clipper calls
        dbgout << "Attaching Clipper (twice)" << endl;
        hr = m_piDDS->SetClipper(piDDC.AsInParam());
        CheckHRESULT(hr, "SetClipper", trFail);

        // TODO: Do some refcount checking here?
        // Set the same clipper again
        hr = m_piDDS->SetClipper(piDDC.AsInParam());
        CheckHRESULT(hr, "SetClipper", trFail);

        // Get the cliper
        dbgout << "Getting the Clipper and verifying" << endl;
        hr = m_piDDS->GetClipper(piDDCTest.AsTestedOutParam());
        CheckHRESULT(hr, "GetClipper", trFail);
        CheckCondition(piDDCTest.InvalidOutParam(), "GetClipper succeeded, but didn't fill in clipper", trFail);
        CheckCondition(piDDCTest != piDDC, "Incorrect Clipper returned", trFail);

        // Release our interfaces to the clipper, so that the
        // only reference is by the surface
        dbgout << "Releasing all references to the Clipper" << endl;
        piDDC.ReleaseInterface();
        piDDCTest.ReleaseInterface();

        // Get references back to the clipper again
        // Get it once
        dbgout << "Getting the Clipper again and verifying" << endl;
        hr = m_piDDS->GetClipper(piDDC.AsTestedOutParam());
        CheckHRESULT(hr, "GetClipper", trFail);
        CheckCondition(piDDC.InvalidOutParam(), "GetClipper succeeded, but didn't fill in clipper", trFail);

        // Get it again
        hr = m_piDDS->GetClipper(piDDCTest.AsTestedOutParam());
        CheckHRESULT(hr, "GetClipper", trFail);
        CheckCondition(piDDCTest.InvalidOutParam(), "GetClipper succeeded, but didn't fill in clipper", trFail);

        // Compare the two
        CheckCondition(piDDCTest != piDDC, "Incorrect Clipper returned", trFail);

        // Set the same clipper again
        dbgout << "Setting same Clipper again" << endl;
        hr = m_piDDS->SetClipper(piDDC.AsInParam());
        CheckHRESULT(hr, "SetClipper", trFail);

        // Release our copies of the clipper again
        dbgout << "Releasing all references to the Clipper" << endl;
        piDDC.ReleaseInterface();
        piDDCTest.ReleaseInterface();

        // Set the clipper to NULL to force the surface to release
        // it reference to the clipper (which should be the only one)
        dbgout << "Setting clipper to NULL and verifying" << endl;
        hr = m_piDDS->SetClipper(NULL);

        // Verify that there is no longer a clipper attached
        hr = m_piDDS->GetClipper(piDDCTest.AsTestedOutParam());
        CheckForHRESULT(hr, DDERR_NOCLIPPERATTACHED, "GetClipper", trFail);

        // If we had a clipper coming in, preserve it
        if (piDDCSave != NULL)
        {
            dbgout << "Restoring original clipper" << endl;
            hr = m_piDDS->SetClipper(piDDCSave.AsInParam());
            CheckHRESULT(hr, "SetClipper", trFail);

            // We should still be able to call methods on the clipper
            hr = piDDCSave->IsClipListChanged(&fChanged);
            CheckHRESULT(hr, "IsClipListChanged", trFail);
        }

        return tr;
    }
#endif // CFG_TEST_IDirectDrawClipper

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Flip::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_Flip::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;

        // Only test if this is a flipping primary surface
        if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
            (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP))
        {
            // If this is a primary is system memory, then we can't lock
            BOOL fCanLock = !((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                              (m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY));
            const DWORD dwPrimary    = 0x12345678,
                        dwBackbuffer = 0x9ABCDEF0;
            CDDSurfaceDesc cddsd;
            HRESULT hr;

            if (fCanLock)
            {
                // Lock and write known data to the initial primary
                dbgout << "Writing to primary" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                *(DWORD*)cddsd.lpSurface = dwPrimary;
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip to the first backbuffer
            dbgout << "Flipping to first backbuffer" << endl;
            hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            CheckHRESULT(hr, "Flip", trFail);

            if (fCanLock)
            {
                // Lock and write known data to the first backbuffer
                dbgout << "Writing to first backbuffer" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                *(DWORD*)cddsd.lpSurface = dwBackbuffer;
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip back to the original frontbuffer
            dbgout << "Flipping to primary" << endl;
            for (int i = 0; i < (int)m_cddsd.dwBackBufferCount; i++)
            {
                dbgout << "Flip #" << i << endl;
                hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                CheckHRESULT(hr, "Flip", trFail);
            }

            if (fCanLock)
            {
                // Verify the original data
                dbgout << "Verifying original primary data" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                QueryForKnownWinCEBug(dwPrimary != *(DWORD*)cddsd.lpSurface && (cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY), 1940, "Expected Primary Data not found", tr |= trFail)
                else QueryCondition(dwPrimary != *(DWORD*)cddsd.lpSurface, "Expected Primary Data not found", tr |= trFail);
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Flip back to the original backbuffer without waiting
            dbgout << "Flipping to first backbuffer without DDFLIP_WAITNOTBUSY" << endl;
            while (1)
            {
                hr = m_piDDS->Flip(NULL, 0);
                if (hr != DDERR_WASSTILLDRAWING)
                    break;
            }
            CheckHRESULT(hr, "Flip", trFail);

            if (fCanLock)
            {
                // Verify the original data
                dbgout << "Verifying original backbuffer data" << endl;
                hr = m_piDDS->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                QueryForKnownWinCEBug(dwBackbuffer != *(DWORD*)cddsd.lpSurface && (cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY), 1940, "Expected Backbuffer Data not found", tr |= trFail)
                else QueryCondition(dwBackbuffer != *(DWORD*)cddsd.lpSurface, "Expected Backbuffer Data not found", tr |= trFail);
                hr = m_piDDS->Unlock(NULL);
                CheckHRESULT(hr, "Unlock", trAbort);
            }

            // Do a few more flips for good measure...
            for (i = 0; i < 30; i++)
            {
                hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                CheckHRESULT(hr, "Flip", trFail);
            }
        }
        else
            dbgout << "Skipping Surface" << endl;

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_ColorFillBlts::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_ColorFillBlts::TestIDirectDrawSurface()
    {
        const DWORD dwFill = 0x12345678;
        eTestResult tr = trPass;
        HRESULT hr;

        CDDBltFX cddbltfx;

        // Pick an arbitrary color
        cddbltfx.dwFillColor = dwFill;

        // Do a filling Blt with waiting
        dbgout << "Filling with " << HEX(cddbltfx.dwFillColor) << endl;
        hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
        CheckHRESULT(hr, "Blt with colorfill and wait", trFail);

        // TODO: Lock and verify the surface colors.

        // Flip all the color's bits
        cddbltfx.dwFillColor = ~dwFill;

        // Do filling blts without waiting
        dbgout << "Filling with " << HEX(cddbltfx.dwFillColor) << endl;
        while (1)
        {
            hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_COLORFILL, &cddbltfx);
            if (hr != DDERR_WASSTILLDRAWING)
                break;
        }
        CheckHRESULT(hr, "Blt with colorfill and no wait", trFail);

        // TODO: Lock and verify the surface colors.

        // Do a number of color fills very quickly
        for (DWORD b = 0; b != 0xFFFFFFFF; b += 0x01010101)
        {
            cddbltfx.dwFillColor = b;
            hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
            CheckHRESULT(hr, "Blt with colorfill and wait", trFail);
        }

        // Do some filling with white and black through ROPS
        cddbltfx.dwFillColor = 0;

        cddbltfx.dwROP = WHITENESS;
        hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_ROP, &cddbltfx);
        CheckHRESULT(hr, "Blt with WHITENESS ROP colorfill ", trFail);

        cddbltfx.dwROP = BLACKNESS;
        hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_ROP, &cddbltfx);
        CheckHRESULT(hr, "Blt with BLACKNESS ROP colorfill ", trFail);

        return tr;
    }
};

namespace Test_IDirectDrawSurface_TWO
{
    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Blt::TestIDirectDrawSurface_TWO
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_Blt::TestIDirectDrawSurface_TWO()
    {
        using namespace DDrawUty::Surface_Helpers;

        const int nBltCount = 10;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsdSrc,
                       cddsdDst;
        bool bCanStretch = true;
        DWORD dwBltFlags = 0;
        HRESULT hr;

        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // There are some things that must be availale in the descripton
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        // Compare the pixel formats
        if (trPass != ComparePixelFormats(cddsdSrc.ddpfPixelFormat, cddsdDst.ddpfPixelFormat, LOG_COMMENT, false))
        {
            dbgout << "Blt between different pixel formats not supported ... Skipping test" << endl;
            return trPass;
        }

        if (0 != memcmp(&cddsdSrc.ddpfPixelFormat, &cddsdDst.ddpfPixelFormat, sizeof(cddsdSrc.ddpfPixelFormat)))
        {
            // TODO: Look into why this sometimes triggers.  The blocks of memory are
            // different, but the ComparePixelFormats passes.  Probably just some
            // random bits getting set (without the corresponding flags marking
            // that member as valid, so we pass), but why?
            dbgout << "Bitwise compare of pixelformats failed" << endl;
            //return trPass;
        }

        // See if we should pass any color key flags
        DDCOLORKEY ddck;
        if (SUCCEEDED(m_piDDS->GetColorKey(DDCKEY_SRCBLT, &ddck)))
            dwBltFlags |= DDBLT_KEYSRC;
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddck)))
            dwBltFlags |= DDBLT_KEYDEST;

        dbgout << "Trying to use Extra Blt Flags : " << g_BltFlagMap[dwBltFlags] << endl;

        HRESULT hrExpected = S_OK;

        // Get the HEL and HAL blt caps for the correct surface locations
        DWORD dwHALBltCaps;
        DWORD dwHALCKeyCaps;

        GetBltCaps(cddsdSrc, cddsdDst, GBC_HAL, &dwHALBltCaps, &dwHALCKeyCaps);

// Now that DDraw uses GDI's Blt path, all blts are supported (either through hw or sw)
//        
//        if ( ((cddsdSrc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && !(dwHALBltCaps & DDBLTCAPS_READSYSMEM))
//            || ((cddsdDst.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && !(dwHALBltCaps & DDBLTCAPS_WRITESYSMEM)))
//        {
//            // Neither HEL or HAL can blt between the surface, expect failure
//            hrExpected = DDERR_UNSUPPORTED;
//        }
//        else
//        {
//            // Either HEL or HAL supports Blting the surface, so check the stretching
//            // and color keying capability.  (This is where it gets hairy.)
//
//            bool bDone = false;
//            while (!bDone)
//            {
//                bool bHALCanBltCKey;
//                DWORD dwNeedCKeyCaps;
//
//                // See what color keys we currently need
//                dwNeedCKeyCaps = 0;
//                if (dwBltFlags & DDBLT_KEYSRC)  dwNeedCKeyCaps |= DDCKEYCAPS_SRCBLT;
//                if (dwBltFlags & DDBLT_KEYDEST) dwNeedCKeyCaps |= DDCKEYCAPS_DESTBLT;
//
//                bHALCanBltCKey = (dwNeedCKeyCaps == (dwHALCKeyCaps & dwNeedCKeyCaps));
//
//                if (bHALCanBltCKey)
//                {
//                    bCanStretch = true;
//                    bDone = true;
//                }
//                else
//                {
//                    // Neither HEL or HAL can handle the color keys we're looking for.
//                    // Remove some color keys and try again.
//                    if (dwBltFlags & DDBLT_KEYDEST) dwBltFlags &= ~DDBLT_KEYDEST;
//                    else if (dwBltFlags & DDBLT_KEYSRC) dwBltFlags &= ~DDBLT_KEYSRC;
//                    else
//                    {
//                        // We've removed both color keys and still can't blt.  Huh?
//                        dbgout(LOG_ABORT) << "Can't find any way to Blt." << endl;
//                    }
//                }
//            }
//        }

        // Special case YUV surfaces
        if (cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            // Can't stretch YUV blts
            bCanStretch = false;
            if ((cddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC) &&
                 !(dwHALBltCaps & DDBLTCAPS_COPYFOURCC))
                hrExpected = DDERR_UNSUPPORTED;
            if ((cddsdDst.ddpfPixelFormat.dwFlags & DDPF_RGB) &&
                 !(dwHALBltCaps & DDBLTCAPS_FOURCCTORGB))
                hrExpected = DDERR_UNSUPPORTED;
        }

        dbgout << "Using Extra Blt Flags : " << g_BltFlagMap[dwBltFlags] << endl;
        dbgout << ((bCanStretch) ? "Stretching" : "Can't Stretch") << endl;

        // Detect known bug of broken destination color keying from vid to vid
        BOOL fBug9113 = (cddsdSrc.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
                        (cddsdDst.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
                        (dwBltFlags & DDBLT_KEYDEST);

        CDDBltFX cddbltfxSRCCOPY;
        cddbltfxSRCCOPY.dwROP = SRCCOPY;

        RECT rcSrcBoundRect = {0, 0, cddsdSrc.dwWidth, cddsdSrc.dwHeight};

        // Use the ClipList if it's a Primary in the Windowed mode.
        // Blt/AlphaBlt would return an error if the Src Rect is out of the Window's bounding rectangle.
        if((cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDS.AsInParam(), rcSrcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
        }

        dbgout << "Performing random-rectangle Blts" << endl;
        for (int i = 0; i < nBltCount; i++)
        {
            // Perform some random rectangle blts
            RECT rcSrc,
                 rcDst;

            // Special case the YUV-YUV blt case
            if (bCanStretch)
            {
                // Simple stretch blt
                GenRandRect(&rcSrcBoundRect, &rcSrc);
                GenRandRect(cddsdDst.dwWidth, cddsdDst.dwHeight, &rcDst);
            }
            else
            {
                // Generate rects of the same size.
                GenRandRect(&rcSrcBoundRect, &rcSrc);

                // Get a shifted rect of the same size

                // Get height and width
                DWORD dwRectWidth  = rcSrc.right - rcSrc.left,
                      dwRectHeight = rcSrc.bottom - rcSrc.top;

                // Get random top-left point
                rcDst.left = rand() % (rcSrcBoundRect.right  - dwRectWidth  + 1);
                rcDst.top  = rand() % (rcSrcBoundRect.bottom - dwRectHeight + 1);

                // Get the bottom-right
                rcDst.right  = rcDst.left + dwRectWidth;
                rcDst.bottom = rcDst.top + dwRectHeight;
            }

            // Note, specific rectangle Blting from a surface to itself can result
            // in OOM errors, since the Blting requires an additional buffer.
            // This is acceptable.
            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryCondition_LOG(LOG_COMMENT, (hr == DDERR_OUTOFMEMORY) && (m_piDDSDst == m_piDDS), "OOM OK", hrExpected = hr)
            else QueryForKnownWinCEBug(DDERR_UNSUPPORTED == hr && fBug9113, 9113, "Destination ColorKeying Fails on Permedia", tr |= trFail)
            else QueryForHRESULT(hr, hrExpected, "Blt with rects, wait and NULL BltFX" << rcSrc << "->" << rcDst, tr |= trFail);

            while (DDERR_WASSTILLDRAWING == (hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, dwBltFlags, NULL)));
            QueryCondition_LOG(LOG_COMMENT, (hr == DDERR_OUTOFMEMORY) && (m_piDDSDst == m_piDDS), "OOM OK", hrExpected = hr)
            else QueryForKnownWinCEBug(DDERR_UNSUPPORTED == hr && fBug9113, 9113, "Destination ColorKeying Fails on Permedia", tr |= trFail)
            else QueryForHRESULT(hr, hrExpected, "Blt with rects, no wait and NULL BltFX" << rcSrc << "->" << rcDst, tr |= trFail);

            hr = m_piDDSDst->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY | DDBLT_ROP | dwBltFlags, &cddbltfxSRCCOPY);
            QueryCondition_LOG(LOG_COMMENT, (hr == DDERR_OUTOFMEMORY) && (m_piDDSDst == m_piDDS), "OOM OK", hrExpected = hr);
            QueryForKnownWinCEBug(DDERR_UNSUPPORTED == hr && fBug9113, 9113, "Destination ColorKeying Fails on Permedia", tr |= trFail)
            else QueryForHRESULT(hr, hrExpected, "Blt with rects, wait and SRCCOPY BltFX" << rcSrc << "->" << rcDst, tr |= trFail);
        }

        // If the two surfaces are the same size, do some
        // full-surface blts.
        if (cddsdSrc.dwWidth == cddsdDst.dwWidth && cddsdSrc.dwHeight == cddsdDst.dwHeight)
        {
            dbgout << "Performing full-surface Blts" << endl;

            hr = m_piDDSDst->Blt(NULL, m_piDDS.AsInParam(), NULL, DDBLT_WAITNOTBUSY | dwBltFlags, NULL);
            QueryForKnownWinCEBug(DDERR_UNSUPPORTED == hr && fBug9113, 9113, "Destination ColorKeying Fails on Permedia", tr |= trFail)
            else QueryForHRESULT(hr, hrExpected, "Blt with wait and NULL BltFX", trFail);

            while (DDERR_WASSTILLDRAWING == (hr = m_piDDSDst->Blt(NULL, m_piDDS.AsInParam(), NULL, dwBltFlags, NULL)));
            QueryForKnownWinCEBug(DDERR_UNSUPPORTED == hr && fBug9113, 9113, "Destination ColorKeying Fails on Permedia", tr |= trFail)
            else QueryForHRESULT(hr, hrExpected, "Blt without wait and NULL BltFX", trFail);

            hr = m_piDDSDst->Blt(NULL, m_piDDS.AsInParam(), NULL, DDBLT_WAITNOTBUSY | DDBLT_ROP | dwBltFlags, &cddbltfxSRCCOPY);
            QueryForKnownWinCEBug(DDERR_UNSUPPORTED == hr && fBug9113, 9113, "Destination ColorKeying Fails on Permedia", tr |= trFail)
            else QueryForHRESULT(hr, hrExpected, "Blt with wait and SRCCOPY BltFX", trFail);
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_BltFast::TestIDirectDrawSurface_TWO
    //  Tests the BltFast Method of IDirectDrawSurface.
    //
    eTestResult CTest_BltFast::TestIDirectDrawSurface_TWO()
    {
        dbgout << "BltFast no longer supported" << endl;
        return trSkip;
        /*
        using namespace DDrawUty::Surface_Helpers;

        const int nBltCount = 10;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsdSrc,
                       cddsdDst;
        CDDCaps cddHALCaps,
                cddHELCaps;
        DWORD dwBltFlags = 0;
        HRESULT hr;

        hr = m_piDD->GetCaps(&cddHALCaps, &cddHELCaps);
        CheckHRESULT(hr, "GetCaps", trAbort);

        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // There are some things that must be availale in the descripton
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_PIXELFORMAT), "Pixel Format not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        // Compare the pixel formats
        if (trPass != ComparePixelFormats(cddsdSrc.ddpfPixelFormat, cddsdDst.ddpfPixelFormat, LOG_COMMENT, false))
        //if (memcmp(&cddsdSrc.ddpfPixelFormat, &cddsdDst.ddpfPixelFormat, sizeof(cddsdSrc.ddpfPixelFormat)))
        {
            dbgout << "BltFast between different pixel formats not yet supported" << endl;
            return trPass;
        }

        // If either surface is not in display memory, we can't
        // do a bltfast
        if (!(cddsdSrc.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) ||
            !(cddsdDst.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
        {
            dbgout << "Skipping non-video combination" << endl;
            return trPass;
        }

        // Verify that the driver supports this blt
        if (cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_FOURCC &&
            !(cddHALCaps.dwCaps2 & DDCAPS2_COPYFOURCC) &&
            !(cddHELCaps.dwCaps2 & DDCAPS2_COPYFOURCC))
        {
            dbgout << "Driver doesn't support FOURCC copy, skipping" << endl;
            return trPass;
        }

        // If the destination has a clipper, BltFast won't work
        CComPointer<IDirectDrawClipper> piDDC;
        if (DDERR_NOCLIPPERATTACHED != (hr = m_piDDSDst->GetClipper(piDDC.AsTestedOutParam())))
        {
            CheckHRESULT(hr, "GetClipper", trAbort);
            CheckCondition(piDDC.InvalidOutParam(), "GetClipper didn't fill clipper", trAbort);
            dbgout << "Skipping Dest surface with Clipper" << endl;
            piDDC.ReleaseInterface();
            return trPass;
        }

        // See if we should pass any color key flags
        DDCOLORKEY ddck;
        if (SUCCEEDED(m_piDDS->GetColorKey(DDCKEY_SRCBLT, &ddck)))
            dwBltFlags |= DDBLTFAST_SRCCOLORKEY;
        if (SUCCEEDED(m_piDDSDst->GetColorKey(DDCKEY_DESTBLT, &ddck)))
            dwBltFlags |= DDBLTFAST_DESTCOLORKEY;

        // If there are no source or dest keys, use the NOCOLORKEY flag.
        // REVIEW: Should we test without this flag?
        if (0 == dwBltFlags)
            dwBltFlags = DDBLTFAST_NOCOLORKEY;

        dbgout << "Using Extra BltFast Flags : " << g_BltFastFlagMap[dwBltFlags] << endl;

        // TODO: Check the ROP CAPS bits to see if we can do this
        CDDBltFX cddbltfxSRCCOPY;
        cddbltfxSRCCOPY.dwROP = SRCCOPY;

        dbgout << "Performing random-rectangle Blts" << endl;
        for (int i = 0; i < nBltCount; i++)
        {
            // Perform some random rectangle blts
            RECT rcSrc;
            DWORD dwXDst,
                  dwYDst;

            GenRandRect(cddsdSrc.dwWidth, cddsdSrc.dwHeight, &rcSrc);
            dwXDst = rand() % (cddsdSrc.dwWidth  - (rcSrc.right - rcSrc.left) + 1);
            dwYDst = rand() % (cddsdSrc.dwHeight - (rcSrc.bottom - rcSrc.top) + 1);

            hr = m_piDDSDst->BltFast(dwXDst, dwYDst, m_piDDS.AsInParam(), &rcSrc, DDBLTFAST_WAITNOTBUSY | dwBltFlags);
            CheckHRESULT(hr, "BltFast with rects, wait and no COLORKEY", trFail);

            while (DDERR_WASSTILLDRAWING == (hr = m_piDDSDst->BltFast(dwXDst, dwYDst, m_piDDS.AsInParam(), &rcSrc, dwBltFlags)));
            CheckHRESULT(hr, "BltFast with rects, no wait and no COLORKEY", trFail);
        }

        // If the two surfaces are the same size, do some
        // full-surface blts.
        if (cddsdSrc.dwWidth == cddsdDst.dwWidth && cddsdSrc.dwHeight == cddsdDst.dwHeight)
        {
            dbgout << "Performing full-surface Blts" << endl;

            hr = m_piDDSDst->BltFast(0, 0, m_piDDS.AsInParam(), NULL, DDBLTFAST_WAITNOTBUSY | dwBltFlags );
            CheckHRESULT(hr, "BltFast with wait and no COLORKEY", trFail);

            while (DDERR_WASSTILLDRAWING == (hr = m_piDDSDst->BltFast(0, 0, m_piDDS.AsInParam(), NULL, dwBltFlags)));
            CheckHRESULT(hr, "Blt without wait and no COLORKEY", trFail);
        }

        return tr;
        */
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_AlphaBlt::TestIDirectDrawSurface_TWO
    //  Tests the AlphaBlt Method of IDirectDrawSurface.
    //
    eTestResult CTest_AlphaBlt::TestIDirectDrawSurface_TWO()
    {
#if defined(UNDER_CE)
        using namespace DDrawUty::Surface_Helpers;

        const int nBltCount = 10;
        CDDSurfaceDesc cddsdSrc,
                       cddsdDst;
        eTestResult tr = trPass;
        bool bCheckRes = true;
        bool bPassAlphaBlt=true,
               bPassAlphaBltFx=true;
        DWORD dwBltFlags = 0;

        const DWORD dwPalFlags = DDPF_PALETTEINDEXED;

        HRESULT hr;
        CComPointer<IDirectDrawPalette> pPalette1,
                                        pPalette2;

        hr = m_piDDS->GetSurfaceDesc(&cddsdSrc);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
        hr = m_piDDSDst->GetSurfaceDesc(&cddsdDst);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // There are some things that must be availale in the descripton
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_CAPS), "CAPS not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_HEIGHT), "Height not filled", trAbort);
        CheckCondition(!(cddsdSrc.dwFlags & cddsdDst.dwFlags & DDSD_WIDTH), "Width not filled", trAbort);

        // Ensure surface is not YUV
        if (cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_FOURCC ||
            cddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            dbgout << "Skipping YUV Surface" << endl;
            return trPass;
        }

        // Ensure surface is non-premultplied alpha
        if ( (cddsdDst.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) &&
            !(cddsdDst.ddpfPixelFormat.dwFlags & DDPF_ALPHAPREMULT))
        {
            dbgout << "Skipping Non-Premultiplied Surface" << endl;
            return trPass;
        }

        // HACKHACK: If either surface is a primary in system mem, then there's no way
        // to know what the hell will happen here.  On MQ200, it will work, but
        // on S3, IGS, and Perm3, it won't.  Just don't check the return code.
        if ((cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE && cddsdDst.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
            (cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE && cddsdSrc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
        {
            dbgout << "Not checking result for Primary in system memory" << endl;
            bCheckRes = false;
        }

        // See if we should pass any color key flags
        DDCOLORKEY ddck;
        if (SUCCEEDED(m_piDDS->GetColorKey(DDCKEY_SRCBLT, &ddck)))
            dwBltFlags |= 0; // BUGBUG: Is DDABLT_KEYSRC supported?

        dbgout << "Using Extra AlphaBlt Flags : " << g_AlphaBltFlagMap[dwBltFlags] << endl;

        // Some AlphaBlt Effect
        DDALPHABLTFX ddabltfx;
        ddabltfx.dwSize = sizeof(ddabltfx);
        ddabltfx.ddargbScaleFactors.alpha = 0x40;
        ddabltfx.ddargbScaleFactors.red   = 0x40;
        ddabltfx.ddargbScaleFactors.green = 0x40;
        ddabltfx.ddargbScaleFactors.blue  = 0x40;


        // if we have a paletted destination, but not a paletted source, then alphablt and alphabltfx is going to fail
        if((cddsdDst.ddpfPixelFormat.dwFlags & dwPalFlags) && !(cddsdSrc.ddpfPixelFormat.dwFlags & dwPalFlags))
        {
            bPassAlphaBlt=false;
            bPassAlphaBltFx=false;
        }
        // if we have two paletted surfaces, then we need to find out if the palette is the same...
        else if((cddsdDst.ddpfPixelFormat.dwFlags & dwPalFlags) && (cddsdSrc.ddpfPixelFormat.dwFlags & dwPalFlags))
        {
            // it doesn't appear that this works with two paletted surfaces at all
            bPassAlphaBltFx=false;

            // if we have nonmatching palettes, then we fail... (or if we can't retrieve palettes)
            CheckHRESULT(m_piDDS->GetPalette(pPalette1.AsTestedOutParam()), "Failure retrieving source palette information", tr|=trFail);
            CheckCondition(pPalette1.InvalidOutParam(), "GetPalette 1 succeeded, but didn't fill pointer", trAbort);
            CheckHRESULT(m_piDDSDst->GetPalette(pPalette2.AsTestedOutParam()), "Failure retrieving destination palette information", tr|=trFail);
            CheckCondition(pPalette2.InvalidOutParam(), "GetPalette 2 succeeded, but didn't fill pointer", trAbort);

            if(pPalette1!=pPalette2)
            {
                bPassAlphaBlt=false;
            }
        }

        // if we have a non-premult surface and the driver doesn't support this, we'll fail.
        if(   ((cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) &&
              !(cddsdSrc.ddpfPixelFormat.dwFlags & DDPF_ALPHAPREMULT)) &&
           !(g_DDConfig.HALCaps().dwAlphaCaps & DDALPHACAPS_NONPREMULT))
        {
            bPassAlphaBlt=false;
            bPassAlphaBltFx=false;
        }
        // we'll succeed otherwise.
        
        RECT rcSrcBoundRect = {0, 0, cddsdSrc.dwWidth, cddsdSrc.dwHeight};

        // Use the ClipList if it's a Primary in the Windowed mode.
        // Blt/AlphaBlt would return an error if the Src Rect is out of the Window's bounding rectangle.
        if((cddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            tr = GetClippingRectangle(m_piDDS.AsInParam(), rcSrcBoundRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
        }

        dbgout << "Performing random-rectangle Blts" << endl;
        for (int i = 0; i < 3*nBltCount; i++)
        {
            // Perform some random rectangle blts
            RECT rcSrc,
                 rcDst;

            // If these are different surfaces, just choose some
            // random rects
            if ((m_piDDS != m_piDDSDst))
            {
                GenRandRect(&rcSrcBoundRect, &rcSrc);
                GenRandRect(cddsdDst.dwWidth, cddsdDst.dwHeight, &rcDst);
            }
            else
            {
                // In the case of the same surface, we must ensure non-
                // overlapping rects
                GenRandNonOverlappingRects(&rcSrcBoundRect, &rcDst, &rcSrc);
            }

            // If the two surfaces are the same any exception is probably
            // due to HPURAID bug 9047
            if (m_piDDS == m_piDDSDst)
                m_nKnownBugNum = 9047;

            // It's no fun to alphablt to the same rect 3 times, so we iterate
            // 3 times as much, but only do one of these each time, instead of 3
            switch (i % 3)
            {
            case 0:
                hr = m_piDDSDst->AlphaBlt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDABLT_WAITNOTBUSY | dwBltFlags, NULL);
                if (bCheckRes) QueryCondition(bPassAlphaBlt != SUCCEEDED(hr), "AlphaBlt with wait and no ALPHABLTFX", tr|=trFail);
                break;

            case 1:
                while(DDERR_WASSTILLDRAWING == (hr = m_piDDSDst->AlphaBlt(&rcDst, m_piDDS.AsInParam(), &rcSrc, dwBltFlags, NULL)));
                if (bCheckRes) QueryCondition(bPassAlphaBlt != SUCCEEDED(hr), "AlphaBlt without wait and no ALPHABLTFX", tr|=trFail);
                break;

            case 2:
                hr = m_piDDSDst->AlphaBlt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDABLT_WAITNOTBUSY | dwBltFlags, &ddabltfx);

                if (bCheckRes) QueryCondition(bPassAlphaBltFx != SUCCEEDED(hr), "AlphaBlt with wait and ALPHABLTFX", tr|=trFail);
                break;

            default:
                ASSERT(FALSE);
                break;
            }

            m_nKnownBugNum = 0;
        }

        // If the two surfaces are the same size, but not the same
        // surface, do some full-surface blts.
        if (cddsdSrc.dwWidth == cddsdDst.dwWidth && cddsdSrc.dwHeight == cddsdDst.dwHeight &&
            (m_piDDS != m_piDDSDst))
        {
            dbgout << "Performing full-surface Blts" << endl;

            hr = m_piDDSDst->AlphaBlt(NULL, m_piDDS.AsInParam(), NULL, DDABLT_WAITNOTBUSY | dwBltFlags, NULL);
            if (bCheckRes) QueryCondition(bPassAlphaBlt != SUCCEEDED(hr), "AlphaBlt with wait and no ALPHABLTFX", tr|=trFail);

            while (DDERR_WASSTILLDRAWING == (hr = m_piDDSDst->AlphaBlt(NULL, m_piDDS.AsInParam(), NULL, dwBltFlags, NULL)));
            if (bCheckRes) QueryCondition(bPassAlphaBlt != SUCCEEDED(hr), "AlphaBlt without wait and no ALPHABLTFX", tr|=trFail);

            hr = m_piDDSDst->AlphaBlt(NULL, m_piDDS.AsInParam(), NULL, DDABLT_WAITNOTBUSY | dwBltFlags, &ddabltfx);

            if (bCheckRes) QueryCondition(bPassAlphaBltFx != SUCCEEDED(hr), "AlphaBlt with wait and ALPHABLTFX", tr|=trFail);
        }

        return tr;
#else
        return trSkip;
#endif // defined(UNDER_CE)
    }
};
