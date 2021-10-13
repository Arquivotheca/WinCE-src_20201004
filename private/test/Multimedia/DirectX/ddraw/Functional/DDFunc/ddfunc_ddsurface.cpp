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
#include "DDFunc_DDSurface.h"

#include <QATestUty/PresetUty.h>
#include <DDrawUty.h>
#include <vector>
#pragma warning(disable:4995)
using namespace com_utilities;
using namespace DebugStreamUty;
using namespace KnownBugUty;
using namespace DDrawUty;
using namespace PresetUty;
using namespace std;

#define MAXFLIPSTATUSTRIES 1000
#define RANDOMPOSITIVE(max)       (rand()%(max)+1)     //generate a random factor between 1-20


namespace Test_IDirectDrawSurface
{
# if 0
//template <class CFG>
//class Test_IDirectDrawSurface
//{
    ////////////////////////////////////////////////////////////////////////////////
    // CSurfaceTest::Test
    //  Base class used to iterate possible surface configurations.  Implements
    // the "Test" method, and leaves it to the base class to implement the
    // "TestSurface" method.  Set up m_dwSurfaceCats in the base class before
    // calling this method to ensure that the virtual "TestSurface" is only called
    // with the desired surface categories.
    //
    eTestResult CSurfaceTest::Test()
    {
        CDirectDrawConfiguration::VectCooperativeLevel::iterator itrCoop;
        CDirectDrawConfiguration::VectPixelFormat::iterator itrPixel;
        CDirectDrawConfiguration::VectSurfaceType::iterator itrSurf;
        CComPointer<IDirectDrawSurface> piDDS;
        CComPointer<IDirectDraw> piDD;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsd;
        TCHAR szDescPixel[256],
              szDescCoop[256],
              szDescSurf[256];
        HRESULT hr;

        // Release any existing global objects, so we can create
        // them the way we want them.
        g_DDSPrimary.ReleaseObject();
        g_DirectDraw.ReleaseObject();

//        piDD = g_DirectDraw.GetObject();

        dbgout << "Iterating all cooperative levels" << endl;
        for (itrCoop  = g_DDConfig.m_vclSupported.begin();
             itrCoop != g_DDConfig.m_vclSupported.end();
             itrCoop++)
        {
            // Configure the global DirectDraw object
            hr = g_DDConfig.GetCooperativeLevelData(*itrCoop, g_DirectDraw.m_dwCooperativeLevel, szDescCoop, countof(szDescCoop));
            ASSERT(SUCCEEDED(hr));

            // Get the global DirectDraw object
            piDD = g_DirectDraw.GetObject();
            ASSERT(piDD);

            dbgout << "Testing cooperative level : " << szDescCoop << endl;

            // If the derived class wishes to be called with NULL, do so.
            // This is useful if the derived class wishes to do some
            // custom actions or testing (once) before getting called
            // with each surface pointer.
            if (m_dwSurfaceCats & scNull)
            {
                tr |= TestSurface(NULL, cddsd);
            }

            // Iterate all supported Primary surface types, if desired
            if (m_dwSurfaceCats & scPrimary)
            {
                dbgout << "Iterating all supported Primary surface descriptions" << endl;
                for (itrSurf  = g_DDConfig.m_vstSupportedPrimary.begin();
                     itrSurf != g_DDConfig.m_vstSupportedPrimary.end();
                     itrSurf++)
                {
                    // Get the description of the current mode
                    hr = g_DDConfig.GetSurfaceDescData(*itrSurf, g_DDSPrimary.m_cddsd, szDescSurf, countof(szDescSurf));
                    ASSERT(SUCCEEDED(hr));

                    // Create and get the Primary surface
                    piDDS = g_DDSPrimary.GetObject();
                    QueryCondition(piDDS == NULL, "Unable to create the primary", tr |= trFail)
                    else
                    {
                        // Run the test on the surface
                        dbgout << "Testing Surface : " << szDescSurf << endl;
                        tr |= TestSurface(piDDS.AsInParam(), g_DDSPrimary.m_cddsd);

                        if (m_dwSurfaceCats & scBackbuffer)
                        {
                            // TODO : Handle back-buffer iteration here.
                        }
                        else
                            dbgout << "Skipping Backbuffer..." << endl;

                        // Release our interface to the primary, and release
                        // the global primary
                        piDDS.ReleaseInterface();
                        g_DDSPrimary.ReleaseObject();
                    }


/*                    hr = g_DDConfig.GetSurfaceDescData(*itrSurf, cddsd, szDescSurf, countof(szDescSurf));
                    ASSERT(SUCCEEDED(hr));

                    // Attempt to create the surface
                    hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                    QueryHRESULT(hr, "CreateSurface", tr |= trFail)
                    else QueryCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", tr |= trFail)
                    else
                    {
                        // Run the test on the surface
                        dbgout << "Testing Surface : " << szDescSurf << endl;
                        tr |= TestSurface(piDDS.AsInParam(), cddsd);

                        if (m_dwSurfaceCats & scBackbuffer)
                        {
                            // TODO : Handle back-buffer iteration here.
                        }
                        else
                            dbgout << "Skipping Backbuffer..." << endl;

                        piDDS.ReleaseInterface();
                    }*/
                }
            }
            else
                dbgout << "Skipping Primary..." << endl;

            // Iterate all supported video memory surfaces, if desired
            if (m_dwSurfaceCats & scOffScrVid)
            {
                dbgout << "Iterating all video memory surfaces" << indent;
                for (itrSurf  = g_DDConfig.m_vstSupportedVidMem.begin();
                     itrSurf != g_DDConfig.m_vstSupportedVidMem.end();
                     itrSurf++)
                {
                    hr = g_DDConfig.GetSurfaceDescData(*itrSurf, cddsd, szDescSurf, countof(szDescSurf));
                    ASSERT(SUCCEEDED(hr));

                    // Check if this is an overlay, and if we want overlays
                    if ((cddsd.dwFlags & DDSD_CAPS) &&
                        (cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) &&
                        (!(m_dwSurfaceCats & scOverlay)))
                        continue;

                    if (m_dwSurfaceCats & scAllPixels)
                    {
                        // Iterate all supported pixel formats in video memory
                        for (itrPixel  = g_DDConfig.m_vpfSupportedVidMem.begin();
                             itrPixel != g_DDConfig.m_vpfSupportedVidMem.end();
                             itrPixel++)
                        {
                            // See if this is a valid surface/pixel combination
                            if (!g_DDConfig.IsValidSurfacePixelCombo(*itrSurf, *itrPixel))
                                continue;

                            // Get the pixel format into the surface descriptor
                            hr = g_DDConfig.GetPixelFormatData(*itrPixel, cddsd.ddpfPixelFormat, szDescPixel, countof(szDescPixel));
                            ASSERT(SUCCEEDED(hr));

                            // Mark the pixel format data as valid
                            cddsd.dwFlags |= DDSD_PIXELFORMAT;

                            // Attempt to create the surface
                            dbgout << "Testing Surface : " << szDescSurf << " (" << szDescPixel << ")" << endl;
                            hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                            QueryHRESULT(hr, "CreateSurface", tr |= trFail)
                            else QueryCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", tr |= trFail)
                            else
                            {
                                // Run the test on the surface
                                tr |= TestSurface(piDDS.AsInParam(), cddsd);
                                piDDS.ReleaseInterface();
                            }
                        }
                    }
                    else
                    {
                        // Attempt to create the surface
                        dbgout << "Testing Surface : " << szDescSurf << endl;
                        hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                        QueryHRESULT(hr, "CreateSurface", tr |= trFail)
                        else QueryCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", tr |= trFail)
                        else
                        {
                            // Run the test on the surface
                            tr |= TestSurface(piDDS.AsInParam(), cddsd);
                            piDDS.ReleaseInterface();
                        }
                    }
                }
                dbgout << unindent << "Done video memory surfaces" << endl;
            }
            else
                dbgout << "Skipping Video Memory Surfaces..." << endl;

            // Iterate all supported system memory surfaces, if desired
            if (m_dwSurfaceCats & scOffScrVid)
            {
                dbgout << "Iterating all system memory surfaces" << indent;
                for (itrSurf  = g_DDConfig.m_vstSupportedSysMem.begin();
                     itrSurf != g_DDConfig.m_vstSupportedSysMem.end();
                     itrSurf++)
                {
                    hr = g_DDConfig.GetSurfaceDescData(*itrSurf, cddsd, szDescSurf, countof(szDescSurf));
                    ASSERT(SUCCEEDED(hr));

                    // Check if this is an overlay, and if we want overlays
                    if ((cddsd.dwFlags & DDSD_CAPS) &&
                        (cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) &&
                        (!(m_dwSurfaceCats & scOverlay)))
                        continue;

                    if (m_dwSurfaceCats & scAllPixels)
                    {
                        // Iterate all supported pixel formats in system memory
                        for (itrPixel  = g_DDConfig.m_vpfSupportedSysMem.begin();
                             itrPixel != g_DDConfig.m_vpfSupportedSysMem.end();
                             itrPixel++)
                        {
                            // See if this is a valid surface/pixel combination
                            if (!g_DDConfig.IsValidSurfacePixelCombo(*itrSurf, *itrPixel))
                                continue;

                            // Get the pixel format into the surface descriptor
                            hr = g_DDConfig.GetPixelFormatData(*itrPixel, cddsd.ddpfPixelFormat, szDescPixel, countof(szDescPixel));
                            ASSERT(SUCCEEDED(hr));

                            // Mark the pixel format data as valid
                            cddsd.dwFlags |= DDSD_PIXELFORMAT;

                            // Attempt to create the surface
                            dbgout << "Testing Surface : " << szDescSurf << " (" << szDescPixel << ")" << endl;
                            hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                            QueryHRESULT(hr, "CreateSurface", tr |= trFail)
                            else QueryCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", trFail)
                            else
                            {
                                // Run the test on the surface
                                tr |= TestSurface(piDDS.AsInParam(), cddsd);
                                piDDS.ReleaseInterface();
                            }
                        }
                    }
                    else
                    {
                        // Attempt to create the surface
                        dbgout << "Testing Surface : " << szDescSurf << endl;
                        hr = piDD->CreateSurface(&cddsd, piDDS.AsTestedOutParam(), NULL);
                        QueryHRESULT(hr, "CreateSurface", tr |= trFail)
                        else QueryCondition(piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", tr |= trFail)
                        else
                        {
                            // Run the test on the surface
                            tr |= TestSurface(piDDS.AsInParam(), cddsd);
                            piDDS.ReleaseInterface();
                        }
                    }
                }
                dbgout << unindent << "Done system memory surfaces" << endl;
            }
            else
                dbgout << "Skipping System Memory Surfaces..." << endl;

            piDD.ReleaseInterface();
            g_DirectDraw.ReleaseObject();
        }

        return tr;
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////
    // namespace EnumSurfaceHelpers
    //  Helper methods/variables for CTest_AddDeleteAttachedSurface class.
    //
    namespace AttachedSurfaceHelpers
    {
        struct ENUMSURFACESTRUCT
        {
            std::vector< CComPointer<IDirectDrawSurface> > m_vectpiDDS;
            int m_nCount;

            ENUMSURFACESTRUCT() { m_nCount = 0; }
        };

        class op_release_DDS_ComPointer
        {
        public:
            void operator() (CComPointer<IDirectDrawSurface>& pi)
            {
                pi.ReleaseInterface();
            }
        };

        HRESULT WINAPI EnumAttachedCallback(LPDIRECTDRAWSURFACE lpdds, LPDDSURFACEDESC lpddsd, LPVOID lpContext)
        {
            ENUMSURFACESTRUCT *pess = (ENUMSURFACESTRUCT*)lpContext;
            ASSERT(pess);
            ASSERT(pess->m_nCount < (int)pess->m_vectpiDDS.size()-1);

            pess->m_vectpiDDS[pess->m_nCount++] = lpdds;

            lpdds->Release();
            return (HRESULT)DDENUMRET_OK;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_AddDeleteAttachedSurface::Test
    //  Tests the AddAttachedSurface and DeleteAttachedSurface Methods of IDirectDrawSurface.
    //
    eTestResult CTest_AddDeleteAttachedSurface::TestIDirectDrawSurface()
    {
        dbgout << "AddDeleteAttachedSurface no longer supported" << endl;
        return trSkip;

    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_AddOverlayDirtyRect::TestIDirectDrawSurface
    //  Tests the AddOverlayDirtyRect Method of IDirectDrawSurface.
    //
    eTestResult CTest_AddOverlayDirtyRect::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;
        HRESULT hr;

        // This method is not actually implemented, but lets call for a
        // sanity check.
        #if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Invalid Parameter Checking" << endl;
            hr = m_piDDS->AddOverlayDirtyRect(NULL);
            CheckForHRESULT(hr, DDERR_UNSUPPORTED, "AddOverlayDirtyRect(NULL)", trFail);
        #endif

        RECT rc = { 10, 11, 91, 102 };
        hr = m_piDDS->AddOverlayDirtyRect(&rc);
        CheckForHRESULT(hr, DDERR_UNSUPPORTED, "AddOverlayDirtyRect", trFail);

        return tr;
    }


#if defined(UNDER_CE)
    ////////////////////////////////////////////////////////////////////////////////
    // namespace priv_CTest_AlphaBlt
    //  Helper functions for running AlphaBlt tests.
    //
    namespace priv_CTest_AlphaBlt
    {
        using namespace DDrawUty::Surface_Helpers;

        eTestResult RunAlphaBltTests(IDirectDrawSurface *pDDSSrc,
                                     IDirectDrawSurface *pDDSDst,
                                     RECT rcBound,
                                     HRESULT hrExpected,
                                     HRESULT *phrStretch = NULL,
                                     BOOL fBug3547 = FALSE,
                                     BOOL fBug18765=FALSE)
        {
            RECT rcSrc, rcDst;
            eTestResult tr = trPass;
            HRESULT hr;
            int nCount;
            int i;

            // Shrink the bounding area by one
            rcBound.right--;
            rcBound.bottom--;

            dbgout << "Specific Rectangle Copying without stretching" << endl;
            for (i = 0; i < 10; i++)
            {
                GenRandRect(&rcBound, &rcSrc);
                // AlphaBlt handles clipping, but not overlapping rects
                rcDst.left = rcSrc.right;
                rcDst.top = rcSrc.bottom;
                rcDst.right = rcDst.left + rcSrc.right - rcSrc.left;
                rcDst.bottom = rcDst.top + rcSrc.bottom - rcSrc.top;
                nCount = 0;
                do
                {
                    hr = pDDSDst->AlphaBlt(&rcDst, pDDSSrc, &rcSrc, DDABLT_WAITNOTBUSY, NULL);
                    // QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
                    QueryCondition(SUCCEEDED(hr) != SUCCEEDED(hrExpected), "AlphaBlt " << rcSrc << rcDst, return trFail);
                    rcDst.top++; rcDst.bottom++;
                    rcDst.left++; rcDst.right++;
                } while (rcDst.left < rcBound.right && rcDst.top < rcBound.bottom && nCount++ < 20);
            }

            dbgout << "Specific Rectangle Copying with stretching" << endl;
            if (NULL != phrStretch) hrExpected = *phrStretch;
            for (i = 0; i < 10; i++)
            {
                GenRandRect(&rcBound, &rcSrc);
                GenRandRect(&rcBound, &rcDst);
                // Shift the source right by one
                OffsetRect(&rcSrc, 1, 0);
                // Align the top-right of the dest with the bottom-left of the source
                OffsetRect(&rcDst, rcSrc.left - rcDst.right, rcSrc.bottom - rcDst.top);
                nCount = 0;
                do
                {
                    hr = pDDSDst->AlphaBlt(&rcDst, pDDSSrc, &rcSrc, DDABLT_WAITNOTBUSY, NULL);
                    //QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
                    QueryCondition(SUCCEEDED(hr) != SUCCEEDED(hrExpected), "AlphaBlt " << rcSrc << rcDst, return trFail);
                    rcDst.top++; rcDst.bottom++;
                    rcDst.left--; rcDst.right--;
                } while (rcDst.right > rcBound.left && rcDst.top < rcBound.bottom && nCount++ < 20);
            }

            return tr;
        }

        HRESULT GetExpectedAlphaBltReturnValue(CDDSurfaceDesc& ddsdDst, CDDSurfaceDesc& ddsdSrc, HRESULT *pHRStretch = NULL)
        {
            using namespace DDrawUty::Surface_Helpers;

            const DWORD dwPalFlags = DDPF_PALETTEINDEXED;
            HRESULT hrExpected = S_OK;

            // AlphaBlt works if both source and dest are FourCC surfaces, and there's
            // no stretching.
            if ((ddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC) && (ddsdSrc.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
            {
                dbgout << "AlphaBlt works from FourCC to FourCC, but can't stretch" << endl;
                hrExpected = S_OK;
            }

            // AlphaBlt does work if both surfaces are paletted and have the same palette
            else if((ddsdSrc.ddpfPixelFormat.dwFlags & dwPalFlags) && (ddsdDst.ddpfPixelFormat.dwFlags & dwPalFlags))
            {
                // This test should check that both surfaces use the same palette, however I don't
                // see a way to test that here (and it's not currently tested to see if it fails anyway)
                dbgout << "AlphaBlt does work from paletted to paletted with the same palette" << endl;
                hrExpected = S_OK;
            }

//            // AlphaBlt does not work if destination is not premultiplied
//            else if ((ddsdDst.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) && !(ddsdDst.ddpfPixelFormat.dwFlags & DDPF_ALPHAPREMULT))
//            {
//                dbgout << "AlphaBlt does not work with non-premultiplied destination" << endl;
//                hrExpected = DDERR_INVALIDPARAMS;
//            }

            // AlphaBlt appears to work from a paletted surface to anything
            else if(ddsdSrc.ddpfPixelFormat.dwFlags & dwPalFlags)
            {
                dbgout << "AlphaBlt does work from paletted to anything" << endl;
                hrExpected = S_OK;
            }

            // AlphaBlt doesn't work if either source or dest is a FourCC (unless they're both fourcc)
            // or if the dst is a paletted surface (and the src isn't)
            else if ((ddsdDst.ddpfPixelFormat.dwFlags & DDPF_FOURCC || (ddsdSrc.ddpfPixelFormat.dwFlags & DDPF_FOURCC)) ||
                                    (ddsdDst.ddpfPixelFormat.dwFlags & dwPalFlags))
            {
                dbgout << "AlphaBlt does not work on FourCC or paletted surfaces." << endl;
                hrExpected = DDERR_INVALIDPARAMS;
            }

            // AlphaBlt does not work if source is non-premult and driver doesn't support this natively
            else if(   ((ddsdSrc.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) &&
                       !(ddsdSrc.ddpfPixelFormat.dwFlags & DDPF_ALPHAPREMULT)) &&
                    !(g_DDConfig.HALCaps().dwAlphaCaps & DDALPHACAPS_NONPREMULT))
            {
                dbgout << "Driver does not support AlphaBlt from NonPremult surface" << endl;
                hrExpected = DDERR_GENERIC;
            }
//            // AlphaBlt does not work if either source or dest is primary in system mem
//            else if ((ddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE && ddsdDst.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
//                     (ddsdSrc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE && ddsdSrc.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
//            {
//                dbgout << "AlphaBlt does not work on primary in system memory." << endl;
//                hrExpected = DDERR_GENERIC;
//                // Bug 9956 only appears for primary/backbuffers in system memory.
//            }

            if (NULL != pHRStretch)
            {
                *pHRStretch = hrExpected;
            }

            return hrExpected;
        }
    }
#endif // defined(UNDER_CE)

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_AlphaBlt::Test
    //  Tests the CTest_AlphaBlt Method of IDirectDrawSurface.
    //
    eTestResult CTest_AlphaBlt::TestIDirectDrawSurface()
    {
#if defined(UNDER_CE)
        using namespace DDrawUty::Surface_Helpers;
        using namespace priv_CTest_AlphaBlt;

        const DWORD dwPalFlags = DDPF_PALETTEINDEXED;
        CComPointer<IDirectDrawSurface> piDDSDst;
        eTestResult tr = trPass;
        HRESULT hrExpected = S_OK,
                hr = S_OK;

        DDALPHABLTFX alphabltfx;
        RECT rcDst;
        int i;

        alphabltfx.dwSize = sizeof(alphabltfx);

        // Add a palette to the surface if necessary
        tr |= AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDS.AsInParam());
        CheckCondition(tr != trPass, "AttachPaletteIfNeeded", tr);

        // Get the surface description
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // Detect known bugs
        bool fBug3547 = !!(cddsd.ddpfPixelFormat.dwFlags & dwPalFlags),
             fBug3546 = ((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE || cddsd.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) &&
                          cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY),
             fBug18765=((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY));

        bool fShouldFail;

        // If it's a YUV Overlay, return.
        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
        {
            dbgout << "FourCC Overlay detected -- Skipping AlphaBlt Test" << endl;
            return trPass;
        }

        // Determine the expected failure state
        sized_struct<CDDSurfaceDesc> temp;
        temp = CDDSurfaceDesc();
        fShouldFail = FAILED(hrExpected = GetExpectedAlphaBltReturnValue(cddsd, temp));
        dbgout << "Expecting to " << ((fShouldFail) ? "Fail" : "Succeed") << endl;

        // Color-fill blting
        dbgout << "Color fill Blting" << endl;
        alphabltfx.dwFillColor = 0;
        hr = m_piDDS->AlphaBlt(NULL, NULL, NULL, DDABLT_COLORFILL | DDABLT_WAITNOTBUSY, &alphabltfx);
        // QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
        QueryCondition(fShouldFail != FAILED(hr), "AlphaBlt Color Fill with 0", tr |= trFail);
        alphabltfx.dwFillColor = 0xFFFFFFFF;
        hr = m_piDDS->AlphaBlt(NULL, NULL, NULL, DDABLT_COLORFILL | DDABLT_WAITNOTBUSY, &alphabltfx);
        // QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
        QueryCondition(fShouldFail != FAILED(hr), "AlphaBlt Color Fill with 0xFFFFFFFF", tr |= trFail);

        // Save the height and width
        RECT rcBound = { 0, 0, cddsd.dwWidth, cddsd.dwHeight };

        // Update the bounding rectangle based on the Clip List
        tr = GetClippingRectangle(m_piDDS.AsInParam(), rcBound);
        if(trPass != tr)
        {
            dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
            return tr;
        }

        dbgout << "Specific Rectangle Filling" << endl;
        for (i = 0; i < 20; i++)
        {
            GenRandRect(&rcBound, &rcDst);
            alphabltfx.dwFillColor = rand();
            hr = m_piDDS->AlphaBlt(&rcDst, NULL, NULL, DDABLT_COLORFILL | DDABLT_WAITNOTBUSY, &alphabltfx);
            // QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
            QueryCondition(fShouldFail != FAILED(hr), "Color-filling rect" << rcDst << " with " << HEX(alphabltfx.dwFillColor) << "; returned " << g_ErrorMap[hr], trFail);
        }

        // AlphaBlt is known not to work on paletted or FourCC surfaces


        dbgout << "Surface to itself" << endl;
        // Determine the expected return code

        HRESULT hrStretch;
        fShouldFail = FAILED(hrExpected = GetExpectedAlphaBltReturnValue(cddsd, cddsd, &hrStretch));
        dbgout << "Expecting to " << ((fShouldFail) ? "Fail" : "Succeed") << endl;
        tr |= RunAlphaBltTests(m_piDDS.AsInParam(), m_piDDS.AsInParam(), rcBound, hrExpected, &hrStretch, fBug3547, fBug18765);

        // Since AlphaBlt can do color conversion, test surface->surface blts.
        // Surface->Surface Blting
        dbgout << "Surface -> Surface Blting" << endl;
        CDDSurfaceDesc cddsdWork;
        cddsdWork.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        cddsdWork.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
        cddsdWork.dwHeight = cddsd.dwHeight;
        cddsdWork.dwWidth  = cddsd.dwWidth;

        hr = m_piDD->CreateSurface(&cddsdWork, piDDSDst.AsTestedOutParam(), NULL);
        //CheckForHRESULT(hr, DDERR_OUTOFVIDEOMEMORY, "Insufficient Video Memory", trSkip);
        QueryCondition_LOG(LOG_COMMENT, DDERR_OUTOFVIDEOMEMORY == hr, "Insufficient Video Memory", return trSkip)
        else if (DDERR_INVALIDCAPS == hr && !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
        {
            dbgout << "Video Memory surfaces not supported . . . Continuing" << endl;
        }
        else
        {
            CheckHRESULT(hr, "CreateSurface", trAbort);
            CheckCondition(piDDSDst.InvalidOutParam(), "CreateSurface succeeded, but didn't fill out param", trAbort);

            // Get the desc (to get the correct pixel format)
            hr = piDDSDst->GetSurfaceDesc(&cddsdWork);
            CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

            // if both surfaces are paletted (we have a paletted primary), then let's not set the palette on this one, we test the situation
            // where they have different palettes alot.  Alphablt appears to set the palettes to match if one
            // surface does not have a palette, but the other does.
            // if just this surface is paletted, but the other isn't, then we HAVE TO set the paletted on this one.
            if((cddsdWork.ddpfPixelFormat.dwFlags & dwPalFlags) && !(m_cddsd.ddpfPixelFormat.dwFlags & dwPalFlags))
            {
                tr |= AttachPaletteIfNeeded(m_piDD.AsInParam(), piDDSDst.AsInParam());
                CheckCondition(tr != trPass, "AttachPaletteIfNeeded", tr);
            }

            // VidMem
#if (CFG_Skip_Postponed_Bugs)
            if (!fBug3546)
            {
#endif // CFG_Skip_Postponed_Bugs

                dbgout << "Full Surface TestSurface -> VidMem" << endl;

                // Determine the expected return code
                fShouldFail = FAILED(hrExpected = GetExpectedAlphaBltReturnValue(cddsdWork, cddsd));
                dbgout << "Expecting to " << ((fShouldFail) ? "Fail" : "Succeed") << endl;

                hr = piDDSDst->AlphaBlt(NULL, m_piDDS.AsInParam(), NULL, DDABLT_WAITNOTBUSY, NULL);

                //QueryForKnownWinCEBug(DDERR_EXCEPTION == hr && fBug3546, 3546, "AlphaBlt from Primary in system to Video Memory Surface excepts on S3", tr |= trFail)
                QueryCondition(fShouldFail != FAILED(hr), "AlphaBlt from tested surface to VidMem surface", trFail);
                tr |= RunAlphaBltTests(m_piDDS.AsInParam(), piDDSDst.AsInParam(), rcBound, hrExpected, NULL, fBug3547, fBug18765);

#if (CFG_Skip_Postponed_Bugs)
            }
#endif // CFG_Skip_Postponed_Bugs

            dbgout << "Full Surface VidMem -> TestSurface" << endl;

            // Determine the expected return code
            fShouldFail = FAILED(hrExpected = GetExpectedAlphaBltReturnValue(cddsd, cddsdWork));
            dbgout << "Expecting to " << ((fShouldFail) ? "Fail" : "Succeed") << endl;

            hr = m_piDDS->AlphaBlt(NULL, piDDSDst.AsInParam(), NULL, DDABLT_WAITNOTBUSY, NULL);
            QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
            else QueryCondition(fShouldFail != FAILED(hr), "AlphaBlt from VidMem surface to tested surface", tr |= trFail);
            tr |= RunAlphaBltTests(piDDSDst.AsInParam(), m_piDDS.AsInParam(), rcBound, hrExpected, NULL, fBug3547, fBug18765);
        }

        // System Memory
        cddsdWork.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        cddsdWork.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
        cddsdWork.dwHeight = cddsd.dwHeight;
        cddsdWork.dwWidth  = cddsd.dwWidth;
        hr = m_piDD->CreateSurface(&cddsdWork, piDDSDst.AsTestedOutParam(), NULL);
        //CheckForHRESULT(hr, DDERR_OUTOFMEMORY, "Insufficient System Memory", trSkip);
        QueryCondition_LOG(LOG_COMMENT, DDERR_OUTOFMEMORY == hr, "Insufficient System Memory", return trSkip);
        CheckHRESULT(hr, "CreateSurface", trAbort);
        CheckCondition(piDDSDst.InvalidOutParam(), "CreateSurface succeeded, but didn't fill out param", trAbort);

        // Get the desc (to get the correct pixel format)
        hr = piDDSDst->GetSurfaceDesc(&cddsdWork);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // if both surfaces are paletted (we have a paletted primary), then let's not set the palette on this one, we test the situation
        // where they have different palettes alot.  Alphablt appears to set the palettes to match if one
        // surface does not have a palette, but the other does.
        // if just this surface is paletted, but the other isn't, then we HAVE TO set the paletted on this one.
        if((cddsdWork.ddpfPixelFormat.dwFlags & dwPalFlags) && !(m_cddsd.ddpfPixelFormat.dwFlags & dwPalFlags))
        {
            tr |= AttachPaletteIfNeeded(m_piDD.AsInParam(), piDDSDst.AsInParam());
            CheckCondition(tr != trPass, "AttachPaletteIfNeeded", tr);
        }

        // SysMem
        dbgout << "Full Surface TestSurface -> SysMem" << endl;

        // Determine the expected return code
        fShouldFail = FAILED(hrExpected = GetExpectedAlphaBltReturnValue(cddsdWork, cddsd));
        dbgout << "Expecting to " << ((fShouldFail) ? "Fail" : "Succeed") << endl;

        hr = piDDSDst->AlphaBlt(NULL, m_piDDS.AsInParam(), NULL, DDABLT_WAITNOTBUSY, NULL);
        // QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
        QueryCondition(fShouldFail != FAILED(hr), "AlphaBlt from tested surface to SysMem surface", tr |= trFail);
        tr |= RunAlphaBltTests(m_piDDS.AsInParam(), piDDSDst.AsInParam(), rcBound, hrExpected, NULL, fBug3547, fBug18765);

        dbgout << "Full Surface SysMem -> TestSurface" << endl;

        // Determine the expected return code
        fShouldFail = FAILED(hrExpected = GetExpectedAlphaBltReturnValue(cddsd, cddsdWork));
        dbgout << "Expecting to " << ((fShouldFail) ? "Fail" : "Succeed") << endl;

        hr = m_piDDS->AlphaBlt(NULL, piDDSDst.AsInParam(), NULL, DDABLT_WAITNOTBUSY, NULL);
        // QueryForKnownWinCEBug(fBug18765 && S_OK == hr, 18765, "AlphaBlt to a primary in system memory succeeds when expected to fail", tr |= trFail)
        QueryCondition(fShouldFail != FAILED(hr), "AlphaBlt from SysMem surface to tested surface", tr |= trFail);
        tr |= RunAlphaBltTests(piDDSDst.AsInParam(), m_piDDS.AsInParam(), rcBound, hrExpected, NULL, fBug3547, fBug18765);

        return tr;
#else
        return trSkip;
#endif // defined(UNDER_CE)
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Blt::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_Blt::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT hr;

        CDDBltFX bltfx;
        RECT    rcSrc,
                rcDst;
        int i, nCount;

        // Get the surface description
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // Get the the device driver Caps
        DDCAPS ddCaps;
        hr=m_piDD->GetCaps(&ddCaps, NULL);
        CheckHRESULT(hr, "GetDDCAPS", trAbort);

        // When it is FourCC format and caps indicates fourcc color filling blit is not supported, color filling blt tests will be skipped
        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)&& (!(ddCaps.dwBltCaps & DDBLTCAPS_FILLFOURCC)))
        {
            dbgout << "FourCC Color Filling Blit not Supported for this hardware ... Skipping" << endl;
        }
        else
        {
            dbgout << "Color fill Blting" << endl;
            bltfx.dwFillColor = 0;
            hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAITNOTBUSY, &bltfx);
            CheckHRESULT(hr, "Blt Color Fill with 0", trFail);
            bltfx.dwFillColor = 0xFFFFFFFF;
            hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAITNOTBUSY, &bltfx);
            CheckHRESULT(hr, "Blt Color Fill with 0xFFFFFFFF", trFail);
        }

        // When it is FourCC format and caps indicates fourcc blit is not supported,blt tests will be skipped
        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)&& (!(ddCaps.dwBltCaps & DDBLTCAPS_COPYFOURCC)))
        {
            dbgout << "FourCC Blit not Supported for this hardware ... Skipping" << endl;
        }
        else
        {
            dbgout << "Whole-surface Blting" << endl;
            hr = m_piDDS->Blt(NULL, m_piDDS.AsInParam(), NULL, DDBLT_WAITNOTBUSY, NULL);
            QueryHRESULT(hr, "Full surface to itself", tr |= trFail);
        }
        // Save the height and width
        RECT rcBound = { 0, 0, cddsd.dwWidth, cddsd.dwHeight };

        // Update the bounding rectangle based on the Clip List
        tr = GetClippingRectangle(m_piDDS.AsInParam(), rcBound);
        if(trPass != tr)
        {
            dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
            return tr;
        }

                // When it is FourCC format and caps indicates fourcc color filling blit is not supported, color filling blt tests will be skipped
        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)&& (!(ddCaps.dwBltCaps & DDBLTCAPS_FILLFOURCC)))
        {
            dbgout << "FourCC Color Filling Blit not Supported for this hardware ... Skipping" << endl;
        }
        else
        {
            dbgout << "Specific Rectangle Filling" << endl;
            for (i = 0; i < 20; i++)
            {
                GenRandRect(&rcBound, &rcDst);
                bltfx.dwFillColor = rand();
                hr = m_piDDS->Blt(&rcDst, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAITNOTBUSY, &bltfx);
                QueryHRESULT(hr, "Color-filling rect" << rcDst << " with " << HEX(bltfx.dwFillColor), tr |= trFail);
            }
        }

        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)&& (!(ddCaps.dwBltCaps & DDBLTCAPS_COPYFOURCC)))
        {
            dbgout << "FourCC Color Filling Blit not Supported for this hardware ... Skipping" << endl;
        }
        else
        {
            dbgout << "Specific Rectangle Copying without stretching" << endl;
            for (i = 0; i < 10; i++)
            {
                GenRandRect(&rcBound, &rcSrc);
                GenRandRect(&rcBound, &rcDst);
                rcDst.left = rand() % (cddsd.dwWidth + 1 - (rcSrc.right - rcSrc.left));
                rcDst.top = rand() % (cddsd.dwHeight + 1 - (rcSrc.bottom - rcSrc.top));
                rcDst.right = rcDst.left + rcSrc.right - rcSrc.left;
                rcDst.bottom = rcDst.top + rcSrc.bottom - rcSrc.top;
                nCount = 0;
                do
                {
                    hr = m_piDDS->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY, NULL);
                    QueryHRESULT(hr, "Surface to itself" << rcSrc << rcDst, tr |= trFail);
                    rcDst.top++; rcDst.bottom++;
                    rcDst.left++; rcDst.right++;
                } while (rcDst.right <= (int)cddsd.dwWidth && rcDst.bottom <= (int)cddsd.dwHeight && nCount++ < 10);
            }
        }

        // When it is FourCC format and caps indicates fourcc color filling blit is not supported, color filling blt tests will be skipped
        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)&& (!(ddCaps.dwBltCaps & DDBLTCAPS_FILLFOURCC)))
        {
            dbgout << "FourCC Color Filling Blit not Supported for this hardware ... Skipping" << endl;
        }
        else
        {
            dbgout << "Specific Rectangle Filling" << endl;
            for (i = 0; i < 20; i++)
            {
                GenRandRect(&rcBound, &rcDst);
                bltfx.dwFillColor = rand();
                hr = m_piDDS->Blt(&rcDst, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAITNOTBUSY, &bltfx);
                QueryHRESULT(hr, "Color-filling rect" << rcDst << " with " << HEX(bltfx.dwFillColor), tr |= trFail);
            }
        }


        if ((cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)&& (!(ddCaps.dwBltCaps & DDBLTCAPS_COPYFOURCC)))
        {
            dbgout << "FourCC Color Filling Blit not Supported for this hardware ... Skipping" << endl;
        }
        else
        {
            dbgout << "Specific Rectangle Copying with stretching" << endl;
            for (i = 0; i < 10; i++)
            {
                GenRandRect(&rcBound, &rcSrc);
                GenRandRect(&rcBound, &rcDst);

                nCount = 0;
                do
                {
                    hr = m_piDDS->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY, NULL);

                    //when testing streched blt for YUV overlay surface in video memory
                    //a calling function will return DDERR_UNSUPPORTED, but the value returned to here will be E_FAIL (which is DDERR_GENERIC)
                    //for these cases, we will still let it pass and output "unsupported" message for reference.
                    if (hr == DDERR_GENERIC && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) && (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
                    {
                        dbgout << "YUV Stretch Blt apparently not supported" << endl;
                    }
                    else QueryHRESULT(hr, "Surface to itself" << rcSrc << rcDst, tr |= trFail);
                    rcDst.top++; rcDst.bottom++;
                    rcDst.left--; rcDst.right--;
                } while (rcDst.left >= 0 && rcDst.bottom <= (int)cddsd.dwHeight && nCount++ < 10);
            }
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_BltBatch::TestIDirectDrawSurface
    //  Tests the AddOverlayDirtyRect Method of IDirectDrawSurface.
    //
    eTestResult CTest_BltBatch::TestIDirectDrawSurface()
    {
        dbgout << "BltBatch no longer supported" << endl;
        return trSkip;


    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_BltFast::Test
    //  Tests the BltFast Method of IDirectDrawSurface.
    //
    eTestResult CTest_BltFast::TestIDirectDrawSurface()
    {
        dbgout << "BltFast no longer supported" << endl;
        return trSkip;


    }

    ////////////////////////////////////////////////////////////////////////////////
    // namespace EnumSurfaceHelpers
    //  Helper methods/variables for CTest_Flip class.
    //
    namespace EnumSurfaceHelpers
    {
        struct ENUMSURFACESTRUCT
        {
            std::vector< CComPointer<IDirectDrawSurface> > m_vectpiDDS;
            int m_nCount;

            ENUMSURFACESTRUCT() { m_nCount = 0; }
        };

        class op_release_DDS_ComPointer
        {
        public:
            void operator() (CComPointer<IDirectDrawSurface>& pi)
            {
                pi.ReleaseInterface();
            }
        };

        HRESULT WINAPI EnumAttachedCallback(LPDIRECTDRAWSURFACE lpdds, LPDDSURFACEDESC lpddsd, LPVOID lpContext)
        {
            ENUMSURFACESTRUCT *pess = (ENUMSURFACESTRUCT*)lpContext;
            ASSERT(pess);

//            // Check to see if we've gone full circle through all
//            // surfaces and are now looking at the first one again.
//            if (lpdds != pess->m_vectpiDDS[0].AsInParam())
//            {
//                pess->m_vectpiDDS[pess->m_nCount++] = lpdds;
//                lpdds->EnumAttachedSurfaces(lpContext, EnumAttachedCallback);
//            }


            pess->m_vectpiDDS[pess->m_nCount++] = lpdds;
            lpdds->Release();
            return (HRESULT)DDENUMRET_OK;
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumAttachedSurfaces::Test
    //  Tests the EnumAttachedSurfaces Method of IDirectDrawSurface.
    //
    eTestResult CTest_EnumAttachedSurfaces::TestIDirectDrawSurface()
    {
        using namespace EnumSurfaceHelpers;

        eTestResult tr = trPass;
        HRESULT hr;

        // See if this is the call for custom testing
        if (!m_piDDS)
        {
            // This is the section for custom testing.  If there is some sort of
            // surface that this test should be done on, but that it too strange
            // to be in the general configuration, create the surface here, and
            // call into this function recursively (with a non-NULL piDDS, of course)
            // Test a LOT of backbuffers in Video memory
            m_cddsd.dwFlags           = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_BACKBUFFERCOUNT;
            m_cddsd.dwBackBufferCount = 10;
            m_cddsd.dwWidth           = 100;
            m_cddsd.dwHeight          = 100;
            m_cddsd.ddsCaps.dwCaps    = DDSCAPS_VIDEOMEMORY | DDSCAPS_FLIP | DDSCAPS_OVERLAY;

            // Create the surface
            hr = m_piDD->CreateSurface(&m_cddsd, m_piDDS.AsTestedOutParam(), NULL);
            if((DDERR_NOFLIPHW == hr || DDERR_INVALIDCAPS == hr) &&
                  !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP))
            {
                dbgout << "Flipping unsupported, not testing." << endl;
            }
            else if ((DDERR_NOOVERLAYHW == hr || DDERR_INVALIDCAPS == hr) &&
                  !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_OVERLAY))
            {
                dbgout << "Overlay surface unsupported, not testing." << endl;
            }
            else
            {
                CheckHRESULT(hr, "CreateSurface", trFail);
                QueryCondition(m_piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", trFail);
                // Test the surface
                dbgout << "Testing video memory surface with " << m_cddsd.dwBackBufferCount << " backbuffers" << endl;
                tr |= RunSurfaceTest();
                m_piDDS.ReleaseInterface();
            }


        }
        else if ((m_cddsd.dwFlags & DDSD_BACKBUFFERCOUNT) && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER))
        {
            // We always need to have valid CAPS
            ASSERT(m_cddsd.dwFlags & DDSD_CAPS);
            ASSERT(m_cddsd.dwBackBufferCount > 0);

            // Create the ENUMSURFACESTRUCT structure to get filled in with all
            // of the backbuffer pointers.
            ENUMSURFACESTRUCT ess;
            ess.m_vectpiDDS.resize(m_cddsd.dwBackBufferCount + 1);

            for (int i = 0; i < 2; i++)
            {
                // Set the very first entry to the primary
                ess.m_vectpiDDS[0] = m_piDDS;
                ess.m_nCount = 1;

                // Get the backbuffer pointers
                hr = m_piDDS->EnumAttachedSurfaces((LPVOID)&ess, EnumAttachedCallback);
                QueryHRESULT(hr, "EnumAttachedSurfaces", tr |= trFail);
                QueryCondition((int)m_cddsd.dwBackBufferCount + 1 != ess.m_nCount, "EnumAttachedSurfaces didn't find enough surfaces", tr |= trFail);

                dbgout << "Iterated " << ess.m_nCount << " surfaces" << endl;

                // Release all the suface pointers that we got in the reverse
                // order that we got them.  (The reverse part shouldn't be
                // necessary)
                std::for_each(ess.m_vectpiDDS.rbegin(), ess.m_vectpiDDS.rend(), op_release_DDS_ComPointer());
            }
        }
        else
        {
            // TODO: Test for failure on non-flipping surfaces
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // namespace privCTest_Flip
    //  Helper methods/variables for CTest_Flip class.
    //
    namespace privCTest_Flip
    {
        using namespace EnumSurfaceHelpers;

        inline DWORD MagicNumber(int n) { return (n + 1) * 0x11111111; }

        eTestResult VerifyBufferContents(ENUMSURFACESTRUCT &ess, std::vector<DWORD> &vectdwData)
        {
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd;
            HRESULT hr;

            for (int i = 0; i < ess.m_nCount; i++)
            {
                // Verify the data
                hr = ess.m_vectpiDDS[i]->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                QueryHRESULT(hr, "Lock", tr |= trFail);
                if (*(DWORD*)cddsd.lpSurface != vectdwData[i])
                {
                    dbgout << "!!FAIL : " << i << " exp:" << HEX(vectdwData[i]) << " found:" << HEX(*(DWORD*)cddsd.lpSurface) << endl;
                    tr |= trFail;
                }
                else
                {
                    dbgout << "    ok : " << i << " exp:" << HEX(vectdwData[i]) << " found:" << HEX(*(DWORD*)cddsd.lpSurface) << endl;
                }
                hr = ess.m_vectpiDDS[i]->Unlock(NULL);
                QueryHRESULT(hr, "Unlock", tr |= trFail);
            }

            return tr;
        }

        eTestResult VerifyBufferContents(ENUMSURFACESTRUCT &ess, int nFlipCount)
        {
            std::vector<DWORD> vectdwData(ess.m_nCount);

            // Build a vector of the the known, magic data
            for (int i = 0; i < ess.m_nCount; i++)
                vectdwData[i] = MagicNumber((nFlipCount+i) % ess.m_nCount);

            return VerifyBufferContents(ess, vectdwData);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_Flip::Test
    //  Tests the Flip Method of IDirectDrawSurface.
    //
    eTestResult CTest_Flip::TestIDirectDrawSurface()
    {
        using namespace privCTest_Flip;

        eTestResult tr = trPass;
        CDDSurfaceDesc cddsd;
        DWORD dwTemp;
        HRESULT hr;
        int i;

        if (!m_piDDS)
        {
            // This is the section for custom testing.  If there is some sort of
            // surface that this test should be done on, but that it too strange
            // to be in the general configuration, create the surface here, and
            // call into this function recursively (with a non-NULL piDDS, of course)
            // Test a LOT of backbuffers in Video memory
            m_cddsd.dwFlags           = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_BACKBUFFERCOUNT;
            m_cddsd.dwBackBufferCount = 10;
            m_cddsd.dwWidth           = 100;
            m_cddsd.dwHeight          = 100;
            m_cddsd.ddsCaps.dwCaps    = DDSCAPS_VIDEOMEMORY | DDSCAPS_FLIP;

            // Create the surface
            hr = m_piDD->CreateSurface(&m_cddsd, m_piDDS.AsTestedOutParam(), NULL);
            CheckHRESULT(hr, "CreateSurface", trFail);
            QueryCondition(m_piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", trFail);

            // Test the surface
            dbgout << "Testing video memory surface with " << m_cddsd.dwBackBufferCount << " backbuffers" << endl;
            tr |= RunSurfaceTest();
            m_piDDS.ReleaseInterface();

            // Move the surface to system memory
            m_cddsd.ddsCaps.dwCaps    = DDSCAPS_SYSTEMMEMORY | DDSCAPS_FLIP;

            // Create the surface
            hr = m_piDD->CreateSurface(&m_cddsd, m_piDDS.AsTestedOutParam(), NULL);
            CheckHRESULT(hr, "CreateSurface", trFail);
            QueryCondition(m_piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", trFail);

            // Test the surface
            dbgout << "Testing system memory surface with " << m_cddsd.dwBackBufferCount << " backbuffers" << endl;
            tr |= RunSurfaceTest();
            m_piDDS.ReleaseInterface();
        }
        // If this is a flipping surface, then this is a valid test
        else if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) && !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER))
        {
            // We're assuming no more than 12 backbuffers
            const int nMaxBackBuffers = 12;

            // We always need to have valid CAPS
            ASSERT(m_cddsd.dwFlags & DDSD_CAPS);

            // If this is a primary is system memory, then we can't lock
            BOOL fCanLock = !((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                              (m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY));

            // Sanity check...
            ASSERT((m_cddsd.dwFlags & DDSD_BACKBUFFERCOUNT) && (m_cddsd.dwBackBufferCount > 0));

            // Create the ENUMSURFACESTRUCT structure to get filled in with all
            // of the backbuffer pointers.
            ENUMSURFACESTRUCT ess;
            ess.m_vectpiDDS.resize(m_cddsd.dwBackBufferCount + 1);
            ess.m_vectpiDDS[0] = m_piDDS;
            ess.m_nCount = 1;

            // Get the backbuffer pointers
            hr = m_piDDS->EnumAttachedSurfaces((LPVOID)&ess, EnumAttachedCallback);
            QueryHRESULT(hr, "EnumAttachedSurfaces", tr |= trFail);
            QueryCondition((int)m_cddsd.dwBackBufferCount + 1 != ess.m_nCount, "EnumAttachedSurfaces didn't find enough surfaces", tr |= trFail);

            // Output the surface descriptions
            for (i = 0; i < ess.m_nCount; i++)
            {
                CDDSurfaceDesc cddsd;
                hr = ess.m_vectpiDDS[i]->GetSurfaceDesc(&cddsd);
                QueryHRESULT(hr, "GetSurfaceDesc", tr |= trAbort);
                dbgout << i << " : " << cddsd << endl;
            }

            // Fill known data into all of the buffers
            dbgout << "Setting known data" << endl;
            for (i = 0; i < ess.m_nCount && fCanLock; i++)
            {
                // Fill in some known, magic data
                hr = ess.m_vectpiDDS[i]->Lock(NULL, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                QueryHRESULT(hr, "Lock", tr |= trFail);
                *(DWORD*)cddsd.lpSurface = MagicNumber(i);
                hr = ess.m_vectpiDDS[i]->Unlock(NULL);
                QueryHRESULT(hr, "Unlock", tr |= trFail);
            }

            int nFlipCount = 0;

            // Verify that the data we expect is present
            if (fCanLock)
            {
                dbgout << "Verifying data" << endl;
                tr |= VerifyBufferContents(ess, nFlipCount);
            }

            // Perform a number of flips, making sure that things
            // stay as we expect
            for (i = 0; i < 6; i++)
            {
                dbgout << "Flipping with DDFLIP_WAITNOTBUSY" << endl;
                hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                QueryHRESULT(hr, "Flip", tr |= trFail);
                nFlipCount++;

                // Verify that the data we expect is present
                if (fCanLock)
                {
                    dbgout << "Verifying data" << endl;
                    tr |= VerifyBufferContents(ess, nFlipCount);
                }
            }

            // Perform a number of flips, making sure that things
            // stay as we expect
            for (i = 0; i < 6; i++)
            {
                dbgout << "Flipping without DDFLIP_WAITNOTBUSY" << endl;
                while (1)
                {
                    hr = m_piDDS->Flip(NULL, 0);
                    if (hr != DDERR_WASSTILLDRAWING)
                        break;
                }
                QueryHRESULT(hr, "Flip", tr |= trFail);
                nFlipCount++;

                // Verify that the data we expect is present
                if (fCanLock)
                {
                    dbgout << "Verifying data" << endl;
                    tr |= VerifyBufferContents(ess, nFlipCount);
                }
            }

            // Build a vector of the the known, magic data
            std::vector<DWORD> vect(ess.m_nCount);
            for (i = 0; i < ess.m_nCount; i++)
                vect[i] = MagicNumber((nFlipCount+i) % ess.m_nCount);

            // Perform some flips with specific surface override pointers

            // This should not change anything
            dbgout << "Flipping with explicit surface pointer = Primary" << endl;
            hr = m_piDDS->Flip(ess.m_vectpiDDS[0].AsInParam(), DDFLIP_WAITNOTBUSY);
// BUGBUG: This should succeed, ignore failure for now
//            QueryHRESULT(hr, "Flip", tr |= trFail);
            if (fCanLock)
                tr |= VerifyBufferContents(ess, vect);

            for (int nCurrVect = 1; nCurrVect < nMaxBackBuffers+1; nCurrVect++)
            {
                if (nCurrVect < ess.m_nCount)
                {
                    // This flip should swap positions 0 and 1
                    dbgout << "Flipping with explicit surface pointer = Backbuffer #" << nCurrVect << endl;
                    hr = m_piDDS->Flip(ess.m_vectpiDDS[nCurrVect].AsInParam(), DDFLIP_WAITNOTBUSY);
                    QueryHRESULT(hr, "Flip", tr |= trFail);

                    // swap expected values and verify
                    dwTemp = vect[0]; vect[0] = vect[nCurrVect]; vect[nCurrVect] = dwTemp;
                    if (fCanLock)
                        tr |= VerifyBufferContents(ess, vect);

                    // Perform a simple flip - should rotate the current buffers (0, 2, 1)
                    dbgout << "Performing simple Flip" << endl;
                    hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                    QueryHRESULT(hr, "Flip", tr |= trFail);

                    // rotate expected values and verify
                    dwTemp = vect[0];
                    for (i = 0; i < ess.m_nCount - 1; i++) vect[i] = vect[i+1];
                    vect[ess.m_nCount - 1] = dwTemp;
                    if (fCanLock)
                        tr |= VerifyBufferContents(ess, vect);
                }
            }

            // We're done keeping track of what surface is
            // where, now we're going to test wait flags
            for (i = 0; i < 20; i++)
            {
                hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                CheckHRESULT(hr, "Flip", trFail);

                // Repeatedly try to flip without waiting until we can
                while (DDERR_WASSTILLDRAWING == (hr = m_piDDS->Flip(NULL, 0)))
                    Sleep(1);
                // Make sure that S_OK broke the loop, not
                // some error code
                CheckHRESULT(hr, "Flip", trFail);
            }

            // Try to flip one of the backbuffers
            hr = ess.m_vectpiDDS[1]->Flip(NULL, DDFLIP_WAITNOTBUSY);
            CheckForHRESULT(hr, DDERR_NOTFLIPPABLE, "Flip backbuffer", trFail);

            ess.m_vectpiDDS.clear();
        }
        else
        {
            dbgout << "Testing Flip on non-Flippable surface" << endl;

            // This is not a valid flipping primary
            hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
            QueryForHRESULT(hr, DDERR_NOTFLIPPABLE, "Non-Flipable surface didn't fail flip call", tr|=trFail);

            hr = m_piDDS->Flip(NULL, 0);
            QueryForHRESULT(hr, DDERR_NOTFLIPPABLE, "Non-Flipable surface didn't fail flip call", tr|=trFail);
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetAttachedSurface::Test
    //  Tests the GetAttachedSurface Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetAttachedSurface::TestIDirectDrawSurface()
    {
        dbgout << "GetAttachedSurface is no longer supported" << endl;
        return trSkip;


    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetBltStatus::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetBltStatus::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT hr;
        int nCount;
        bool fCanBltFast = true;

        // Don't test FOURCC Overlays (bug 3541)
        if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) && (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
            return tr;

        // Check for a clipper on the surface, since a clipper indicates
        // that BltFast is illegal.
        CComPointer<IDirectDrawClipper> piDDC;
        hr = m_piDDS->GetClipper(piDDC.AsTestedOutParam());
        if (S_OK == hr)
        {
            CheckCondition(piDDC.InvalidOutParam(), "GetClipper Succeeded, but didn't fill out param", trAbort);
            piDDC.ReleaseInterface();
            fCanBltFast = false;
        }
        else
            CheckForHRESULT(hr, DDERR_NOCLIPPERATTACHED, "GetClipper", trAbort);

        // Invalid parameter checks
        hr = m_piDDS->GetBltStatus(0);
        CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "GetBltStatus(0)", trFail);
        hr = m_piDDS->GetBltStatus(DDGBS_CANBLT | DDGBS_ISBLTDONE);
        CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "GetBltStatus(DDGBS_CANBLT | DDGBS_ISBLTDONE)", trFail);

        if (DDERR_NOBLTHW == m_piDDS->GetBltStatus(DDGBS_CANBLT) ||
            DDERR_NOBLTHW == m_piDDS->GetBltStatus(DDGBS_ISBLTDONE))
        {
            dbgout << "Driver has no blitting hardware, skipping" << endl;
            return trPass;
        }

        // Wait until we can blt
        while (DDERR_WASSTILLDRAWING == m_piDDS->GetBltStatus(DDGBS_CANBLT))
            Sleep(1);

        // Generate random rectangles
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);


        BOOL fBug3541 = (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC);
        BOOL fBug151323 = fBug3541 && (m_cddsd.ddpfPixelFormat.dwFourCC == MAKEFOURCC('Y','V','1','2'));

        // Save the height and width
        RECT rcBound = { 0, 0, cddsd.dwWidth, cddsd.dwHeight };

        // Update the bounding rectangle based on the Clip List
        tr = GetClippingRectangle(m_piDDS.AsInParam(), rcBound);
        if(trPass != tr)
        {
            dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
            return tr;
        }

        RECT rcSrc,
             rcDst;

        GenRandRect(&rcBound, &rcSrc);
        GenRandRect(&rcBound, &rcDst);

        CDDBltFX cddbltfx;
        cddbltfx.dwFillColor = 0;

        // perm3 has some kind of fifo thing where we'll never
        // get a wasstilldrawing error, check to see if we have that driver.
        bool fGotDriverName = FALSE;
        TCHAR szDriverName[256];
        DWORD dwSize=countof(szDriverName);

        fGotDriverName=g_DDConfig.GetDriverName(szDriverName, &dwSize);

        bool bDriverFifo = FALSE;
        if(fGotDriverName && (!_tcsicmp(szDriverName,TEXT("ddi_perm3.dll"))
                                || !_tcsicmp(szDriverName,TEXT("ddi_mq200.dll"))
                                || !_tcsicmp(szDriverName,TEXT("ddi_ati.dll"))))
        {
            bDriverFifo=TRUE;
            dbgout << "Testing on a driver that doesn't return DDERR_WASSTILLDRAWING" << endl;
        }

        // Do some color-filling Blts
        nCount = 0;
        for (int i = 0; i < 10; i++)
        {
            hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_COLORFILL, &cddbltfx);
            QueryHRESULT(hr, "Blt", tr |= trFail);
            while (DDERR_WASSTILLDRAWING == m_piDDS->GetBltStatus(DDGBS_CANBLT))
                nCount++;
            hr = m_piDDS->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, 0, &cddbltfx);
            QueryHRESULT(hr, "Blt", tr |= trFail);
            while (DDERR_WASSTILLDRAWING == m_piDDS->GetBltStatus(DDGBS_CANBLT))
                nCount++;
        }
        dbgout << "Counted " << nCount << " GetBltStatus(DDGBS_CANBLT) calls" << endl;

        // If this surface is in video memory, and the hardware is not
        // capable of doing queued Blt's, and the hardware doesn't do some fifo thing that
        // apparantly isn't a queued blt, but sorta acts like it, then there should be a non-
        // zero busy count
        // BUGBUG: BLTQUEUE no longer supported
        /*
        if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
            !(g_DDConfig.HALCaps().dwCaps & DDCAPS_BLTQUEUE) && !bDriverFifo)
            QueryCondition(0 == nCount, "Found a '0' wait count", tr |= trFail);
        */
        nCount = 0;
        for (i = 0; i < 10; i++)
        {
            hr = m_piDDS->Blt(NULL, NULL, NULL, DDBLT_COLORFILL, &cddbltfx);
            QueryHRESULT(hr, "Blt", tr |= trFail);
            while (DDERR_WASSTILLDRAWING == m_piDDS->GetBltStatus(DDGBS_ISBLTDONE))
                nCount++;
            hr = m_piDDS->Blt(&rcDst, m_piDDS.AsInParam(), &rcSrc, 0, &cddbltfx);
            QueryHRESULT(hr, "Blt", tr |= trFail);
            while (DDERR_WASSTILLDRAWING == m_piDDS->GetBltStatus(DDGBS_ISBLTDONE))
                nCount++;
        }
        dbgout << "Counted " << nCount << " GetBltStatus(DDGBS_ISBLTDONE) calls" << endl;

/*
        if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
            !(g_DDConfig.HALCaps().dwCaps & DDCAPS_BLTQUEUE) & !bDriverFifo)
            QueryCondition(0 == nCount, "Found a '0' wait count", tr |= trFail);
*/
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetCaps::Test
    //  Tests the Blt Method of IDirectDrawSurface.
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
    // CTest_GetSetClipper::Test
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

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSetColorKey::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSetColorKey::TestIDirectDrawSurface()
    {
        using namespace Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT hr;

        if (!m_piDDS)
        {
            // TODO: Create and test some surfaces with keys at creation time.
        }
        else
        {
            // We always need to have valid CAPS
            ASSERT(m_cddsd.dwFlags & DDSD_CAPS);

            // Some reasonable test cases for paletted surfaces
            // Note that (array[n] < 2^n) to make valid tests for different palette sizes
            const DWORD rgdwPaletteKeys[9] = { 0x00, 0x01, 0x03, 0x0A, 0x0F, 0x10, 0x55, 0xAA, 0xFF };

            const DWORD rgdwRGBKeys[] = { 0x00000000, 0x01234567, 0x89ABCDEF, 0xBAADF00D,
                                          0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF,
                                          0xAAAA5555, 0x5555AAAA, 0x18181818, 0xFFFFFFFF };

            // Mask to test if surface has a palette
            const DWORD dwPaletteMask = DDPF_PALETTEINDEXED;

            // Get flags indicating if this is a palettized surface
            const bool  fPalette = (bool)!!(m_cddsd.ddpfPixelFormat.dwFlags & dwPaletteMask);

            // Create vector of the test keys that we want to use
            std::vector<DWORD> vectdwKeys(fPalette ? BEGIN(rgdwPaletteKeys) : BEGIN(rgdwRGBKeys),
                                          fPalette ?   END(rgdwPaletteKeys) :   END(rgdwRGBKeys));

            // Create a vector of the flags valid for this surface type
            // and system caps
            std::vector<DWORD> vectdwFlags;
            hr = GetValidColorKeys(GVCK_SRC | GVCK_DST, m_cddsd, vectdwFlags);

            // Color key structure
            DDCOLORKEY ddck;

            //
            for (int i = 0; i < (int)vectdwFlags.size(); i++)
            {
                dbgout << "Testing " << g_GetSetColorKeyFlagMap[vectdwFlags[i]] << endl;

                for (int j = 0; j < (int)vectdwKeys.size(); j++)
                {
                    ddck.dwColorSpaceLowValue = ddck.dwColorSpaceHighValue = vectdwKeys[j];

                    hr = m_piDDS->SetColorKey(vectdwFlags[i], &ddck);
                    QueryHRESULT(hr, "SetColorKey", tr |= trFail);

                    // Change the color key structure to something different than
                    // what we just set
                    ddck.dwColorSpaceLowValue = ddck.dwColorSpaceHighValue = vectdwKeys[(j+1) % vectdwKeys.size()];

                    // Get and verify the color key
                    hr = m_piDDS->GetColorKey(vectdwFlags[i], &ddck);
                    QueryHRESULT(hr, "GetColorKey", tr |= trFail);

                    QueryCondition(ddck.dwColorSpaceLowValue  != vectdwKeys[j] ||
                                   ddck.dwColorSpaceHighValue != vectdwKeys[j],
                                   "Retrieved color key differs from the set one", tr |= trFail);

                    if (tr != trPass)
                    {
                        dbgout << "Failed with key = " << ddck << endl;
                        return tr;
                    }
                }
            }
        }

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

            memset(&lf, 0x00, sizeof(lf));

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
            wcsncpy_s(lf.lfFaceName, pszTypeface, sizeof(lf.lfFaceName)/sizeof(lf.lfFaceName[0])-1);

            return CreateFontIndirect(&lf);
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetDC::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetDC::TestIDirectDrawSurface()
    {
        using namespace privCTest_GetReleaseDeviceContext;
        using namespace DDrawUty::Surface_Helpers;

        const TCHAR szMessage[] = TEXT("Testing GetDC");
        RECT rc = { 0, 0, 0, 0 };
        eTestResult tr = trPass;
        HRESULT hr;
        HDC hdc,
            hdc2;


#if 0
        // If this is a primary is system mem, we can't GetDC
        if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
            m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
        {
            HDC hdc1;
            // REVIEW: Shouldn't this fail with DDERR_CANTLOCKSURFACE?
            dbgout << "Un-DC-able surface -- verifying that \"GetDC\" fails" << endl;
            hr = m_piDDS->GetDC(&hdc);
            CheckForHRESULT(hr, DDERR_GENERIC, "GetDC didn't fail as expected", trFail);
        }
        else
#endif

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
        if (FAILED(hr))
        {
            // If this is a primary is system mem or a primary in the normal mode, then failing to get the DC is OK.
            if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE && ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) || (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)))
                ||
                (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC && DDERR_CANTCREATEDC == hr))
            {
                dbgout << "GetDC Failed on un-DC-able surface.  OK." << endl;
                return trPass;
            }
            CheckHRESULT(hr, "GetDC", trFail);
        }
        else
        {
            // Make sure the DC is valid
            CheckCondition(PresetUty::IsPreset(hdc), "GetDC Failed to fill in DC", trFail);

            // Attempt to get the DC again
            hr = m_piDDS->GetDC(&hdc2);

            if(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC) // A surface that owns a dc can be queried for it's DC multiple times, without error.
            {
                QueryForHRESULT(hr, S_OK, "Getting DC Again", tr |= trFail);
            }
            else
            {
                QueryForHRESULT(hr, DDERR_DCALREADYCREATED, "Getting DC Again", tr |= trFail);
            }

            // Release and re-get the HDC
            dbgout << "Releasing and re-getting the DC" << endl;
            hr = m_piDDS->ReleaseDC(hdc);
            QueryHRESULT(hr, "Release", tr |= trFail);
            hr = m_piDDS->GetDC(&hdc);
            CheckHRESULT(hr, "GetDC", trFail);

            // Create a simple font
            dbgout << "Creating font" << endl;
            HFONT hfontNew = (HFONT)CreateSimpleFont(TEXT("Arial"), 12);
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
                    if (NULL == hbrushOld && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                    {
                        dbgout << "Could not select brush into DC on Overlay.  OK." << endl;
                        tr = trPass;
                    }
                    else
                    {
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
                        QueryCondition(hbrushNew != hbrushTest, "Unable to retrive custom brush", tr |= trFail);
                    }

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
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetFlipStatus::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetFlipStatus::TestIDirectDrawSurface()
    {
        const int nFlipTestCount = 50;
        eTestResult tr = trPass;
        int nFlipDoneFails = 0,
            nCanFlipFails = 0,
            nFlipFails = 0;
        int nFlipDoneWrong = 0,
            nCanFlipWrong = 0;
        int nFlipWasStillDrawing=0;
        HRESULT hr = S_OK;

        // We always need to have valid CAPS
        ASSERT(m_cddsd.dwFlags & DDSD_CAPS);

        if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP)
        {
            // Sanity check...
            ASSERT((m_cddsd.dwFlags & DDSD_BACKBUFFERCOUNT) && (m_cddsd.dwBackBufferCount > 0));

            // Test getting flip status unless it is not supported
            if (DDERR_UNSUPPORTED != m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE))
            {
                HRESULT hrExpected = S_OK;

                DWORD dwTimeoutCount;
                // Wait until we can flip
                for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES && (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE)); dwTimeoutCount++)
                    Sleep(1);

                CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);

                for (int i = 0; i < nFlipTestCount; i++)
                {
                    // Flip without waiting - test DDGFS_CANFLIP

                    // Now the surface should be done flipping and flip-able
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_ISFLIPDONE) Failed", nFlipDoneFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_CANFLIP) Failed", nCanFlipFails++);

                    // Try a flip without waiting (should work, since we shouldn't be
                    // flipping now.
                    hr = m_piDDS->Flip(NULL, 0);
                    QueryHRESULT(hr, "Flip without waiting", nFlipFails++);

                    for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES && (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_CANFLIP)); dwTimeoutCount++)
                        Sleep(1);

                    CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);

                    // Flip without waiting - test DDGFS_ISFLIPDONE

                    // Now the surface should be done flipping and flip-able
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_ISFLIPDONE) Failed", nFlipDoneFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_CANFLIP) Failed", nCanFlipFails++);

                    // Try a flip without waiting (should work, since we shouldn't be
                    // flipping now.
                    hr = m_piDDS->Flip(NULL, 0);
                    QueryHRESULT(hr, "Flip without waiting", nFlipFails++);

                    for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES && (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE)); dwTimeoutCount++)
                        Sleep(1);

                    CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);

                    // Flip with waiting - test DDGFS_CANFLIP

                    // Now the surface should be done flipping and flip-able
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_ISFLIPDONE) Failed", nFlipDoneFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_CANFLIP) Failed", nCanFlipFails++);

                    // Flip without waiting
                    hr = m_piDDS->Flip(NULL, 0);
                    QueryHRESULT(hr, "Flip without waiting", nFlipFails++);
                    // Flip with waiting
                    hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                    QueryHRESULT(hr, "Flip with waiting", nFlipFails++);

                    for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES && (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_CANFLIP)); dwTimeoutCount++)
                        Sleep(1);

                    CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);

                    // Flip with waiting - test DDGFS_ISFLIPDONE

                    // Now the surface should be done flipping and flip-able
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_ISFLIPDONE) Failed", nFlipDoneFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_CANFLIP) Failed", nCanFlipFails++);

                    // Flip without waiting
                    hr = m_piDDS->Flip(NULL, 0);
                    QueryHRESULT(hr, "Flip without waiting", nFlipFails++);
                    // Flip with waiting
                    hr = m_piDDS->Flip(NULL, DDFLIP_WAITNOTBUSY);
                    QueryHRESULT(hr, "Flip with waiting", nFlipFails++);

                    // Wait until we can flip
                    for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES && (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE)); dwTimeoutCount++)
                        Sleep(1);

                    CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);



                   // Flip with waiting for V-SYNC - test DDGFS_CANFLIP

                    // Now the surface should be done flipping and flip-able
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_ISFLIPDONE) Failed", nFlipDoneFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_CANFLIP) Failed", nCanFlipFails++);

                    // Flip with waiting for v-sync
                    hr = m_piDDS->Flip(NULL, DDFLIP_WAITVSYNC);
                    QueryHRESULT(hr, "Flip with waiting for v-sync", nFlipFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    if (hr == DDERR_WASSTILLDRAWING)
                    {
                        dbgout << "GetFlipStatus(DDGFS_CANFLIP) returns DERR_WASSTILLDRAWING, which is expected"
                               << endl;
                        nFlipWasStillDrawing++;
                    }
                    else QueryForHRESULT_LOG(LOG_COMMENT, hr, hrExpected, "GetFlipStatus(DDGFS_CANFLIP)", nCanFlipWrong++);

                    for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES ; dwTimeoutCount++)
                    {

                        // if get flip status return DDERR_WASSTILLDRAWING, update the counts and sleep
                        if (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_CANFLIP))
                        {
                            nFlipWasStillDrawing++;
                            Sleep(1);
                        }
                        else    //when it is not DDERR_WASSTILLDRAWING, terminate the loop
                            break;
                    }

                    CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);

                    // Flip with waiting for V-SYNC - test DDGFS_ISFLIPDONE

                    // Now the surface should be done flipping and flip-able
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_ISFLIPDONE) Failed", nFlipDoneFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
                    QueryHRESULT(hr, "GetFlipStatus(DDGFS_CANFLIP) Failed", nCanFlipFails++);

                    // Flip with waiting for v-sync
                    hr = m_piDDS->Flip(NULL, DDFLIP_WAITVSYNC);
                    QueryHRESULT(hr, "Flip with waiting for v-sync", nFlipFails++);
                    hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
                    if (hr == DDERR_WASSTILLDRAWING)
                    {
                        dbgout << "GetFlipStatus(DDGFS_ISFLIPDONE) returns DERR_WASSTILLDRAWING, which is expected"
                               << endl;
                        nFlipWasStillDrawing++;
                    }
                    else QueryForHRESULT_LOG(LOG_COMMENT, hr, hrExpected, "GetFlipStatus(DDGFS_ISFLIPDONE)", nFlipDoneWrong++);

                    // Wait until we can flip
                    for(dwTimeoutCount=0; dwTimeoutCount < MAXFLIPSTATUSTRIES ; dwTimeoutCount++)
                    {
                        // if get flip status return DDERR_WASSTILLDRAWING, update the counts and sleep
                        if (DDERR_WASSTILLDRAWING == m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE))
                        {
                            nFlipWasStillDrawing++;
                            Sleep(1);
                        }
                        else    //when it is not DDERR_WASSTILLDRAWING, terminate the loop
                            break;
                    }

                    CheckCondition(dwTimeoutCount==MAXFLIPSTATUSTRIES, "Exceeded maximum tries on GetFlipStatus", trFail);
                }

                dbgout
                    << "FlipDone Fails : " << nFlipDoneFails << endl
                    << "CanFlip Fails  : " << nCanFlipFails << endl
                    << "Flip Fails    : " << nFlipFails << endl
                    << "Number of returning DDERR_WASSTILLDRAWING : " << nFlipWasStillDrawing << endl;

                // Allow some tolerance for incorrect returns, but none for failed returns.
                if ((0 < (nFlipDoneFails + nCanFlipFails + nFlipFails)) || (nFlipTestCount/10 < (nCanFlipWrong + nFlipDoneWrong)))
                {
                    TCHAR szDriverName[256];
                    DWORD dwTmp=countof(szDriverName);

                    if(g_DDConfig.GetDriverName(szDriverName, &dwTmp) && !_tcsicmp(szDriverName, TEXT("ddi_mq200.dll")))
                    {
                        QueryForKnownWinCEBug(S_OK == hr, 21500, "GetFlipStatus always returns S_OK on mq200", tr |= trFail);
                    }
                    else
                        tr |= trFail;
                }
                //Fail the test case if GetFlipStatus is supported, but it will only return DD_OK (never return DDERR_WASSTILLDRAWING), for video memory surface
                if (nFlipWasStillDrawing ==0)
                {
                    if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)
                    {
                        dbgout (LOG_FAIL)
                            << "GetFlipStatus should return DDERR_WASSTILLDRAWING at least occasionally when surface is in video memory" << endl;
                        tr|=trFail;
                    }
                }
            }
            else
            {
                dbgout << "Driver doesn't support getting flip status. Skipping getting flip status tests" << endl;
            }
        }
        else
        {
            dbgout << "Testing Flip on non-Flippable surface" << endl;

            // This is not a valid flipping primary
            hr = m_piDDS->GetFlipStatus(DDGFS_ISFLIPDONE);
            QueryForHRESULT(hr, DDERR_INVALIDCAPS, "Non-Flipable surface didn't fail GetFlipStatus call", tr|=trFail);

            hr = m_piDDS->GetFlipStatus(DDGFS_CANFLIP);
            QueryForHRESULT(hr, DDERR_INVALIDCAPS, "Non-Flipable surface didn't fail GetFlipStatus call", tr|=trFail);
        }

        return tr;
    }

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
    // CTest_GetSetPalette::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSetPalette::TestIDirectDrawSurface()
    {
        using namespace privCTest_CreatePalette;

        dbgout << "Palettes not supported in Bowmore, Skipping" << endl;
        return trSkip;

//        const DWORD dwPalFlags = DDPF_PALETTEINDEXED;
//        CComPointer<IDirectDrawPalette> piDDP,
//                                        piDDPSave,
//                                        piDDPTest;
//        eTestResult tr = trPass;
//        CDDSurfaceDesc cddsd;
//        DWORD dwDummy;
//        HRESULT hr;
//
//        // See if their is an attached palette coming in
//        if (SUCCEEDED(m_piDDS->GetPalette(piDDPSave.AsTestedOutParam())))
//        {
//            dbgout << "Found Palette already attached -- saving" << endl;
//            CheckCondition(piDDPSave.InvalidOutParam(), "GetPalette Succeeded, but didn't fill in palette", trFail);
//
//            // Make a simple call on the interface to ensure that it's valid
//            hr = piDDPSave->GetCaps(&dwDummy);
//            CheckHRESULT(hr, "GetCaps", trFail);
//        }
//
//        // Get surface desc
//        hr = m_piDDS->GetSurfaceDesc(&cddsd);
//        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);
//        CheckCondition(!(cddsd.dwFlags & DDSD_PIXELFORMAT), "No Pixel Format", trAbort);
//
//        // Create a very simple 332 palette
//        PALETTEENTRY rgpe[256];
//        for (int i = 0; i < countof(rgpe); i++)
//        {
////            rgpe[i].peRed = rgpe[i].peGreen = rgpe[i].peBlue = i * 0xFF / (countof(rgpe) - 1);
////            rgpe[i].peFlags = 0;
//            rgpe[i].peRed = (BYTE) (((i >> 5) & 0x07) * 0xFF / 7);
//            rgpe[i].peGreen = (BYTE) (((i >> 2) & 0x07) * 0xFF / 7);
//            rgpe[i].peBlue = (BYTE) (((i >> 0) & 0x03) * 0xFF / 3);
//            rgpe[i].peFlags = (BYTE) 0;
//        }
//
//        //
//        const DWORD dwSurfPalFlags = cddsd.ddpfPixelFormat.dwFlags & dwPalFlags;
//        if (0 != dwSurfPalFlags)
//        {
//            dbgout << "Testing on a valid palettized surface" << endl;
//
//            bool fValid = false;
//            for (int i = 0; i < countof(rgpd); i++)
//            {
//                if (rgpd[i].dwSurfFlags == dwSurfPalFlags)
//                {
//                    // The surface flags are valid
//                    fValid = true;
//
//                    // Create a correctly sized palette
//                    dbgout << "Testing with a palette with flags : " << g_CapsPalCapsFlagMap[rgpd[i].dwPalFlags] << endl;
//                    hr = m_piDD->CreatePalette(rgpd[i].dwPalFlags, rgpe, piDDP.AsTestedOutParam(), NULL);
//                    QueryHRESULT(hr, "IDirectDraw::CreatePalette", tr |= trFail);
//                    QueryCondition(piDDP.InvalidOutParam(), "CreatePalette succeeded but failed to set the OUT parameter", tr |= trFail);
//
//                    // Attach the palette
//                    dbgout << "Attaching Palette (twice)" << endl;
//                    hr = m_piDDS->SetPalette(piDDP.AsInParam());
//                    CheckHRESULT(hr, "SetPalette", trFail);
//
//                    // TODO: Do some refcount checking here?
//                    // Set the same palette again
//                    hr = m_piDDS->SetPalette(piDDP.AsInParam());
//                    CheckHRESULT(hr, "SetPalette", trFail);
//
//                    // Get the palette
//                    dbgout << "Getting the Palette and verifying" << endl;
//                    hr = m_piDDS->GetPalette(piDDPTest.AsTestedOutParam());
//                    CheckHRESULT(hr, "GetPalette", trFail);
//                    CheckCondition(piDDPTest.InvalidOutParam(), "GetPalette succeeded, but didn't fill in palette", trFail);
//                    CheckCondition(piDDPTest != piDDP, "Incorrect Palette returned", trFail);
//
//                    // Release our interfaces to the palette, so that the
//                    // only reference is by the surface
//                    dbgout << "Releasing all references to the Palette" << endl;
//                    piDDP.ReleaseInterface();
//                    piDDPTest.ReleaseInterface();
//
//                    // Get references back to the palette again
//                    // Get it once
//                    dbgout << "Getting the Palette again and verifying" << endl;
//                    hr = m_piDDS->GetPalette(piDDP.AsTestedOutParam());
//                    CheckHRESULT(hr, "GetPalette", trFail);
//                    CheckCondition(piDDP.InvalidOutParam(), "GetPalette succeeded, but didn't fill in palette", trFail);
//
//                    // Get it again
//                    hr = m_piDDS->GetPalette(piDDPTest.AsTestedOutParam());
//                    CheckHRESULT(hr, "GetPalette", trFail);
//                    CheckCondition(piDDPTest.InvalidOutParam(), "GetPalette succeeded, but didn't fill in palette", trFail);
//
//                    // Compare the two
//                    CheckCondition(piDDPTest != piDDP, "Incorrect Palette returned", trFail);
//
//                    // Set the same palette again
//                    dbgout << "Setting same Palette again" << endl;
//                    hr = m_piDDS->SetPalette(piDDP.AsInParam());
//                    CheckHRESULT(hr, "SetPalette", trFail);
//
//                    // Release our copies of the palette again
//                    dbgout << "Releasing all references to the Palette" << endl;
//                    piDDP.ReleaseInterface();
//                    piDDPTest.ReleaseInterface();
//
//                    // Set the palette to NULL to force the surface to release
//                    // it reference to the palette (which should be the only one)
//                    dbgout << "Setting palette to NULL and verifying" << endl;
//                    hr = m_piDDS->SetPalette(NULL);
//
//                    // Verify that there is no longer a palette attached
//                    hr = m_piDDS->GetPalette(piDDPTest.AsTestedOutParam());
//                    CheckForHRESULT(hr, DDERR_NOPALETTEATTACHED, "GetPalette", trFail);
//                }
//            }
//            QueryCondition(!fValid, "Illegal palette flag combination found", tr |= trFail);
//        }
//        else
//        {
//            dbgout << "Testing on non-palettized surface" << endl;
//
//            hr = m_piDD->CreatePalette(0, rgpe, piDDP.AsTestedOutParam(), NULL);
//            CheckHRESULT(hr, "CreatePalette", trAbort);
//            CheckCondition(piDDP.InvalidOutParam(), "CreatePalette didn't fill out param", trAbort);
//
//            // This is not a palettized surface
//            hr = m_piDDS->SetPalette(piDDP.AsInParam());
//            QueryForHRESULT(hr, DDERR_INVALIDPIXELFORMAT, "SetPalette", tr |= trFail);
//            hr = m_piDDS->GetPalette(piDDPTest.AsTestedOutParam());
//            QueryForHRESULT(hr, DDERR_NOPALETTEATTACHED, "GetPalette", tr |= trFail);
//            QueryCondition(piDDPTest.InvalidOutParam() != NULL, "GetPalette didn't NULL out pointer", tr |= trFail);
//        }
//
//        // If we had a palette coming in, preserve it
//        if (piDDPSave != NULL)
//        {
//            dbgout << "Restoring original palette" << endl;
//            hr = m_piDDS->SetPalette(piDDPSave.AsInParam());
//            CheckHRESULT(hr, "SetPalette", trFail);
//
//            // We should still be able to call methods on the palette
//            hr = piDDPSave->GetCaps(&dwDummy);
//            CheckHRESULT(hr, "GetCaps", trFail);
//        }
//
//        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetPixelFormat::Test
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
    // CTest_GetSetFreePrivateData::TestIDirectDrawSurface
    //  Tests the GetPixelFormat Method of IDirectDrawSurface.
    //
    namespace priv_CTest_GetSetFreePrivateData
    {
        class CDummyUnknown : public IUnknown
        {
        public:
            CDummyUnknown() : m_cRef(0) { }

            virtual ULONG STDMETHODCALLTYPE AddRef() { return ++m_cRef; }
            virtual ULONG STDMETHODCALLTYPE Release() { return --m_cRef; }
            virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* obp) { return S_OK; }
            long m_cRef;
        };
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSetFreePrivateData::TestIDirectDrawSurface
    //  Tests the GetPixelFormat Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSetFreePrivateData::TestIDirectDrawSurface()
    {
#if 0
        using namespace priv_CTest_GetSetFreePrivateData;

        CComPointer<IDirectDrawSurface> piDDS;
        eTestResult tr = trPass;
        HRESULT hr;

        TCHAR szData1[128],
              szData2[256],
              szData3[256];
        DWORD dwQuery;
        GUID guid1,
             guid2;
        int i;

        for (i = 0; i < countof(szData1); i++)
            szData1[i] = rand();
        for (i = 0; i < countof(szData2); i++)
            szData2[i] = szData3[i] = rand();

        // Make two random GUIDs.
        guid1.Data1 = rand();
        guid1.Data2 = rand();
        guid1.Data3 = rand();
        for (i = 0; i < countof(guid1.Data4); i++)
            guid1.Data4[i] = rand();

        guid2.Data1 = rand();
        guid2.Data2 = rand();
        guid2.Data3 = rand();
        for (i = 0; i < countof(guid2.Data4); i++)
            guid2.Data4[i] = rand();

        #if 0
        // Crashes on ship builds
        #if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Invalid Parameter Checking" << endl;
            hr = piDDS5->SetPrivateData(guid1, NULL, sizeof(szData1), 0);
            CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "SetPrivateData(NULL, szData1, sizeof(szData1), 0)", trFail);
            hr = piDDS5->SetPrivateData(guid1, NULL, sizeof(szData1), DDSPD_IUNKNOWNPOINTER);
            CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "SetPrivateData(NULL, szData1, sizeof(szData1), DDSPD_IUNKNOWNPOINTER)", trFail);
            hr = piDDS5->SetPrivateData(guid1, szData1, 0, 0);
            CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "SetPrivateData(NULL, szData1, 0, 0)", trFail);
        #endif
        #endif

        dbgout << "Asking for non-existant data" << endl;
        dwQuery = sizeof(szData2);
        hr = piDDS5->GetPrivateData(guid1, szData2, &dwQuery);
        QueryForHRESULT(hr, DDERR_NOTFOUND, "GetPrivateData(nonsetGuid)", tr |= trFail);
        QueryCondition(memcmp(szData2, szData3, sizeof(szData2)), "GetPrivateData modified data when not supposed to", tr |= trFail);

        dbgout << "Setting Private Data" << endl;
        hr = piDDS5->SetPrivateData(guid1, szData1, sizeof(szData1), 0);
        CheckHRESULT(hr, "SetPrivateData(NULL, szData1, sizeof(szData1), 0)", trFail);

        dbgout << "Verifying Private Data" << endl;
        // 0 size
        dwQuery = 0;
        hr = piDDS5->GetPrivateData(guid1, NULL, &dwQuery);
        QueryForHRESULT(hr, DDERR_MOREDATA, "GetPrivateData(guid1, NULL, &(0))", tr |= trFail);
        QueryCondition(sizeof(szData1) != dwQuery, "GetPrivateData didn't update buffer size", tr |= trFail);

        // Not large enough
        dwQuery = sizeof(szData1)/2;
        hr = piDDS5->GetPrivateData(guid1, szData2, &dwQuery);
        QueryForHRESULT(hr, DDERR_MOREDATA, "GetPrivateData(guid1, NULL, &(sizeof(szData1)/2))", tr |= trFail);
        QueryCondition(sizeof(szData1) != dwQuery, "GetPrivateData didn't update buffer size", tr |= trFail);
        QueryCondition(memcmp(szData2, szData3, sizeof(szData2)), "GetPrivateData modifed dest buffer", tr |= trFail);

        // Just large enough
        hr = piDDS5->GetPrivateData(guid1, szData2, &dwQuery);
        QueryHRESULT(hr, "GetPrivateData", tr |= trFail);
        QueryCondition(memcmp(szData1, szData2, sizeof(szData1)), "GetPrivateData didn't fill correct info", tr |= trFail);
        QueryCondition(memcmp(szData2+countof(szData1), szData3+countof(szData1), sizeof(szData2)-sizeof(szData1)), "GetPrivateData overstepped bounds", tr |= trFail);

        // Extra space
        memcpy(szData2, szData3, sizeof(szData2));
        dwQuery = sizeof(szData2);
        hr = piDDS5->GetPrivateData(guid1, szData2, &dwQuery);
        QueryHRESULT(hr, "GetPrivateData", tr |= trFail);
        QueryCondition(memcmp(szData1, szData2, sizeof(szData1)), "GetPrivateData didn't fill correct info", tr |= trFail);
        QueryCondition(memcmp(szData2+countof(szData1), szData3+countof(szData1), sizeof(szData2)-sizeof(szData1)), "GetPrivateData overstepped bounds", tr |= trFail);

        // Set a second peice of data
        hr = piDDS5->SetPrivateData(guid2, szData3, sizeof(szData3), DDSPD_VOLATILE);
        memset(szData2, 0, sizeof(szData2));

        // Get and test both peices of data
        dwQuery = sizeof(szData3);
        hr = piDDS5->GetPrivateData(guid2, szData2, &dwQuery);
        QueryHRESULT(hr, "GetPrivateData", tr |= trFail);
        QueryCondition(memcmp(szData2, szData3, sizeof(szData3)), "GetPrivateData didn't fill correct info", tr |= trFail);

        dwQuery = sizeof(szData1);
        hr = piDDS5->GetPrivateData(guid1, szData2, &dwQuery);
        QueryHRESULT(hr, "GetPrivateData", tr |= trFail);
        QueryCondition(memcmp(szData1, szData2, sizeof(szData1)), "GetPrivateData didn't fill correct info", tr |= trFail);
        QueryCondition(memcmp(szData2+countof(szData1), szData3+countof(szData1), sizeof(szData2)-sizeof(szData1)), "GetPrivateData overstepped bounds", tr |= trFail);

        // Free the data
        dbgout << "Freeing Data" << endl;
        hr = piDDS5->FreePrivateData(guid1);
        QueryHRESULT(hr, "FreePrivateData", tr |= trFail);
        hr = piDDS5->FreePrivateData(guid2);
        QueryHRESULT(hr, "FreePrivateData", tr |= trFail);

        // Ask for the freed data
        memcpy(szData2, szData3, sizeof(szData2));
        dbgout << "Asking for non-existant data" << endl;
        dwQuery = sizeof(szData2);
        hr = piDDS5->GetPrivateData(guid1, szData2, &dwQuery);
        QueryForHRESULT(hr, DDERR_NOTFOUND, "GetPrivateData(freedID)", tr |= trFail);
        QueryCondition(memcmp(szData2, szData3, sizeof(szData2)), "GetPrivateData modified data when not supposed to", tr |= trFail);
        hr = piDDS5->GetPrivateData(guid2, szData2, &dwQuery);
        QueryForHRESULT(hr, DDERR_NOTFOUND, "GetPrivateData(freedID)", tr |= trFail);
        QueryCondition(memcmp(szData2, szData3, sizeof(szData2)), "GetPrivateData modified data when not supposed to", tr |= trFail);

// Yanked support for IUnknown object (Wince 3575)
#if 0
#if (!CFG_Skip_Postponed_Bugs)
        // Test using an Unknown object
        CDummyUnknown unk1;
        IUnknown *punkIn,
                 *punkOut;
        unk1.AddRef();

        dbgout << "Setting Private Data with IUnknown data" << endl;
        punkIn = (IUnknown*)&unk1;
        hr = piDDS5->SetPrivateData(guid1, punkIn, sizeof(&unk1), DDSPD_IUNKNOWNPOINTER);
        QueryForKnownWinCEBug(DDERR_INVALIDPARAMS == hr, 3575, "SetPrivateData(DDSPD_IUNKNOWNPOINTER) Crashes", tr |= trFail)
        else QueryHRESULT(hr, "SetPrivateData(DDSPD_IUNKNOWNPOINTER)", tr |= trFail)
        else QueryCondition(2 != unk1.m_cRef, "SetPrivateData didn't AddRef", tr |= trFail);

        Preset(punkOut);
        dwQuery = sizeof(punkOut);
        hr = piDDS5->GetPrivateData(guid1, &punkOut, &dwQuery);
        QueryHRESULT(hr, "GetPrivateData", tr |= trFail)
        else QueryCondition(IsPreset(punkOut), "GetPrivateData succeeded but didn't fill out param", tr |= trFail)
        else QueryCondition(punkOut != punkIn, "GetPrivateData filled wrong out param", tr |= trFail);

        QueryCondition(2 != unk1.m_cRef, "GetPrivateData AddRefed", tr |= trFail);

        // Free the private data
        hr = piDDS5->FreePrivateData(guid1);
        QueryHRESULT(hr, "FreePrivateData", tr |= trFail);

        QueryCondition(1 != unk1.m_cRef, "FreePrivateData didn't Release", tr |= trFail);
#endif // #if(!CFG_Skip_Postponed_Bugs)
#endif
        return tr;
#else
        return trSkip;
#endif // defined(UNDER_CE)
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetSurfaceDesc::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetSurfaceDesc::TestIDirectDrawSurface()
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
        dbgout << "cddsd = " << cddsd << endl;
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
    // CTest_GetChangeUniquenessValue::TestIDirectDrawSurface
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_GetChangeUniquenessValue::TestIDirectDrawSurface()
    {
#if 0
        using namespace DDrawUty::Surface_Helpers;

        CComPointer<IDirectDrawSurface5> piDDS5;
        eTestResult tr = trPass;
        HRESULT hr;

        // Get the IDirectDrawSurface5 interface
        hr = m_piDDS->QueryInterface(IID_IDirectDrawSurface5, (void**)piDDS5.AsTestedOutParam());
        CheckHRESULT(hr, "Surface QueryInterface", trAbort);
        CheckCondition(piDDS5.InvalidOutParam(), "Source QI succeeded, but didnt' fill interface", trAbort);

        #if 0
        // Crashes on ship builds
        #if CFG_TEST_INVALID_PARAMETERS
            dbgout << "Invalid Parameter Checking" << endl;
            hr = piDDS5->GetUniquenessValue(NULL);
            CheckForHRESULT(hr, DDERR_INVALIDPARAMS, "GetUniquenessValue(NULL)", trFail);
        #endif
        #endif

        DWORD dwValue1,
              dwValue2;

        dbgout << "Getting and comparing original uniqueness values" << endl;
        hr = piDDS5->GetUniquenessValue(&dwValue1);
        CheckHRESULT(hr, "GetUniquenessValue(&dwValue1)", trFail);
        hr = piDDS5->GetUniquenessValue(&dwValue2);
        CheckHRESULT(hr, "GetUniquenessValue(&dwValue2)", trFail);

        CheckCondition(dwValue1 != dwValue2, "GetUniquenessValue didn't match", trFail);

        // Change Uniqueness Value
        hr = piDDS5->ChangeUniquenessValue();
        CheckHRESULT(hr, "ChangeUniquenessValue", trFail);

        // Get new value
        hr = piDDS5->GetUniquenessValue(&dwValue1);
        CheckHRESULT(hr, "GetUniquenessValue(&dwValue1)", trFail);
        CheckCondition(dwValue1 == dwValue2, "GetUniquenessValue matched when shouldn't", trFail);

        // Get new value again
        hr = piDDS5->GetUniquenessValue(&dwValue2);
        CheckHRESULT(hr, "GetUniquenessValue(&dwValue2)", trFail);
        CheckCondition(dwValue1 != dwValue2, "GetUniquenessValue didn't match", trFail);

        // Get the surface description
        CDDSurfaceDesc2 cddsd;
        hr = piDDS5->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // Blting to surface to change it
        RECT rcSrc,
             rcDst;
        CDDBltFX bltfx;

        // Color Filling Blt
        dbgout << "Color Fill blt to surface to change it" << endl;
        GenRandRect(cddsd.dwWidth, cddsd.dwHeight, &rcDst);
        bltfx.dwFillColor = rand();
        hr = piDDS5->Blt(&rcDst, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAITNOTBUSY, &bltfx);
        //CheckCondition(hr == DDERR_UNSUPPORTED && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) && (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC), KnownWinCEBug(3541) << "Can't Blt FourCC to itself", trFail);
        QueryForKnownWinCEBug(hr == DDERR_UNSUPPORTED && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) && (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC), 3541, "Can't Blt FourCC to itself", tr |= trFail)
        else QueryHRESULT(hr, "Color-filling rect" << rcDst << " with " << HEX(bltfx.dwFillColor), tr |= trFail);

        // Get new value
        hr = piDDS5->GetUniquenessValue(&dwValue1);
        CheckHRESULT(hr, "GetUniquenessValue(&dwValue1)", trFail);
        CheckCondition(dwValue1 == dwValue2, "GetUniquenessValue matched when shouldn't", trFail);

        // Surface to itself blt
        dbgout << "Blt surface to itself to change it" << endl;
        GenRandRect(cddsd.dwWidth, cddsd.dwHeight, &rcSrc);
        GenRandRect(cddsd.dwWidth, cddsd.dwHeight, &rcDst);
        if (SUCCEEDED(piDDS5->Blt(&rcDst, piDDS5.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY, NULL)))
        {
            // Get new value
            hr = piDDS5->GetUniquenessValue(&dwValue2);
            CheckHRESULT(hr, "GetUniquenessValue(&dwValue2)", trFail);
            CheckCondition(dwValue1 == dwValue2, "GetUniquenessValue matched when shouldn't", trFail);
        }

        return tr;
#else
        return trSkip;
#endif // defined(UNDER_CE)
    }

    void PrintMemory(int iLineNumber)
    {
        MEMORYSTATUS ms;
        ms.dwLength = sizeof(ms);
        CompactAllHeaps();
        GlobalMemoryStatus(&ms);
        dbgout << "Line Number: " << iLineNumber << ";\tAvailable Physical Memory: " << ms.dwAvailPhys << endl;
        return;
    }
    eTestResult CTest_IsLostRestore::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;
        // To test IsLostRestore, we need to go through the ways that surfaces
        // can be lost. We can then verify that our surface is lost correctly.
        // If the surface is a primary or overlay or backbuffer of either, the
        // surface should be lost. Otherwise, if the surface is in system memory
        // it shouldn't be lost.
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsdBefore;
        CDDSurfaceDesc cddsdAfter;
        TCHAR tszDeviceName[MAX_PATH] = {0};
        DWORD dwNameSize = countof(tszDeviceName) - 1;
        HRESULT hr;
        int iX;
        int iY;
        DEVMODE dm;
        LONG lCDSResult;
        int iPrintCount = 10;
        const DWORD dwYUVPRESET = 0xababcdcd;
        RECT rcWindow;
        bool bWindowed = false;

        // This is the order that we will rotate in. This hits every possible
        // rotation change, which is not unreasonable since it's only 16 (plus
        // one rotation to get in a known state).
        enum EAngle {Angle0 = DMDO_0, Angle90 = DMDO_90, Angle180 = DMDO_180, Angle270 = DMDO_270, AngleOther};
        EAngle rgRotations[18] = {
            Angle0,
            Angle0,
            Angle90,
            Angle90,
            Angle180,
            Angle180,
            Angle270,
            Angle270,
            Angle0,
            Angle180,
            Angle0,
            Angle270,
            Angle90,
            Angle270,
            Angle180,
            Angle90,
            Angle0,
            AngleOther};
        const int c_iRotations = countof(rgRotations);

        // Get the surface description
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        LPRECT prcBoundRect = NULL;
        RECT rcClipRect = {0, 0, cddsd.dwWidth, cddsd.dwHeight};

        // If it is a Primary in Windowed mode, pass the window's bounding rectangle to Lock/Unlock calls.
        if((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
        {
            tr = GetClippingRectangle(m_piDDS.AsInParam(), rcClipRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }

            prcBoundRect = &rcClipRect;
        }

        //Determine the window's display area, the area that won't be overdrawn by the Shell on rotate
        IDirectDrawClipper * piDDC = NULL;
        if (SUCCEEDED(m_piDDS->GetClipper(&piDDC)))
        {
            HWND hwnd;
            if (SUCCEEDED(piDDC->GetHWnd(&hwnd)))
            {
                // TODO: Verify that GetClientRect returns a rect that doesn't extend over the top bar in MDG Shell
                if (GetClientRect(hwnd, &rcWindow))
                {
                    bWindowed = true;
                }
            }
            piDDC->Release();
        }

        for (int iRotation = 0; iRotation < c_iRotations; iRotation++)
        {
            PrintMemory(__LINE__);

            // For each surface, draw a pattern on the surface, but only if the
            // surface isn't lost
            if (SUCCEEDED(m_piDDS->IsLost()))
            {
                // Lock surface. Save the description for later.
                hr = m_piDDS->Lock(prcBoundRect, &cddsdBefore, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock", trAbort);
                PrintMemory(__LINE__);

                if (cddsdBefore.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
                {
                    // Just write a DWORD to the first four bytes of the surface.
                    // Will write the first four bytes one byte by one byte, to avoid alignment exception
                    // when it is not 32bpp case.

                    // TODO: Extend this to be more complicated
                    BYTE const* pYUVPRESET= (BYTE const*)&dwYUVPRESET;
                    // to make it safe, only the bytes fully occupied by the first pixel will be written and verified.
                    // therefore we only write and verify 1 byte when it is 12 bits pixel.
                    for (unsigned int i=0;i< cddsdBefore.ddpfPixelFormat.dwYUVBitCount/8;i++)
                    {
                        *(((BYTE*)cddsdBefore.lpSurface)+i) = *(pYUVPRESET+i);
                    }

                }
                else
                {
                    DWORD dwWidth = cddsdBefore.dwWidth;
                    DWORD dwHeight = cddsdBefore.dwHeight;
                    if((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
                    {
                        dwWidth = rcClipRect.right - rcClipRect.left;
                        dwHeight = rcClipRect.bottom - rcClipRect.top;
                    }

                    // Create a PixelCoverter with the data.
                    CPixelConverter pcConverter(cddsdBefore.ddpfPixelFormat);
                    // Fill the surface with known data.
                    for (iY = 0; iY <(int) dwHeight; iY++)
                    {
                        for (iX = 0; iX <(int) dwWidth; iX++)
                        {
                            float fR = ((float) iX) / ((float) dwWidth);
                            float fG = ((float) iY) / ((float) dwHeight);
                            float fB = ((float) iX + iY) / ((float) dwWidth + dwHeight);
                            DWORD dwR, dwG, dwB, dwA;
                            void * pvPixels = (void*)(((BYTE*)(cddsdBefore.lpSurface)) + iY * cddsdBefore.lPitch + iX * cddsdBefore.lXPitch);
                            pcConverter.UnNormalizeRGBA(fR, fG, fB, 1.0, dwR, dwG, dwB, dwA);
                            pcConverter.WriteRGBA(&pvPixels, dwR, dwG, dwB, dwA);
                        }
                    }
                }
                // Unlock the surface.
                hr = m_piDDS->Unlock(prcBoundRect);
                CheckHRESULT(hr, "Unlock", trAbort);
                PrintMemory(__LINE__);
            }

            // Rotate the screen or do something else that would cause the surface
            // to be lost.
            if (rgRotations[iRotation] != AngleOther)
            {
                memset(&dm, 0x00, sizeof(DEVMODE));
                dm.dmSize = sizeof(DEVMODE);
                g_DDConfig.GetDriverName(tszDeviceName, &dwNameSize);
                wcsncpy_s(dm.dmDeviceName, tszDeviceName, countof(dm.dmDeviceName) - 1);
                dm.dmFields = DM_DISPLAYORIENTATION;
                dm.dmDisplayOrientation = rgRotations[iRotation];
                PrintMemory(__LINE__);
                lCDSResult = ChangeDisplaySettingsEx(tszDeviceName, &dm, NULL, 0, NULL);
                if (lCDSResult != DISP_CHANGE_SUCCESSFUL)
                {
                    dbgout << "ChangeDisplaySettingsEx failed, continuing anyway ("
                           << lCDSResult << ")" << endl;
                }
                PrintMemory(__LINE__);

            }
            else
            {
                // Need to create another DDraw object if we aren't exclusive.
                // We can then set that object to exclusive mode, which should
                // make some of our surfaces lost.
                if (!(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
                {
                }
            }

            // If the surface is lost, verify the behavior of the lost surface, try to restore it, verify restored surface is
            // correct.
            if (FAILED(m_piDDS->IsLost()))
            {
                if (0 == iRotation)
                {
                    dbgout << "Surface lost in transition from original state to "
                           << (int)rgRotations[iRotation] << endl;
                }
                else if (iRotation > 0)
                {
                    dbgout << "Surface lost in transition from state "
                           << (int)rgRotations[iRotation - 1] << " to state "
                           << (int)rgRotations[iRotation] << endl;
                }

                // Verify the behavior of a lost surface.
                tr |= VerifyLostSurface(m_piDD.AsInParam(),
                                        cddsdBefore, m_piDDS.AsInParam(),
                                        g_DDSPrimary.GetObject().AsInParam()
                                        );

                PrintMemory(__LINE__);
                hr = m_piDDS->Restore();
                if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                {
                    dbgout << "Could not restore surface, not enough memory" << endl;
                }
                // DDraw should not be in a state now where the surface is not restorable,
                // except if there isn't enough memory.
                else QueryHRESULT(hr, "Restore", tr |= trFail);
                PrintMemory(__LINE__);

            }
            // If the surface isn't lost, verify that the pattern drawn to the surface
            // earlier hasn't changed.
            else
            {
                hr = m_piDDS->Lock(prcBoundRect, &cddsdAfter, DDLOCK_WAITNOTBUSY, NULL);
                PrintMemory(__LINE__);
                CPixelConverter pcConverter(cddsdAfter.ddpfPixelFormat);
                QueryHRESULT(hr, "Lock After", tr |= trFail);
                tr |= CompareSurfaceDescs(cddsdAfter, cddsdBefore);
                if (!(tr & trFail))
                {
                    if (cddsdBefore.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
                    {
                        // Read the DWORD from the first four bytes of the surface.
                        // TODO: Extend this to be more complicated
                        BOOL bfourCCVerification=true;
                        BYTE const* pYUVPRESET= (BYTE const*)&dwYUVPRESET;
                        for (int i=0;i< (int)cddsdBefore.ddpfPixelFormat.dwYUVBitCount/8;i++)
                        {
                            //if any one of the verification bytes is different with the preset value
                            if (*(((BYTE*)cddsdBefore.lpSurface)+i) != *(pYUVPRESET+i))
                            {
                                bfourCCVerification=false; //mark the flag as false
                            }
                        }
                        if (!bfourCCVerification)   //fail the test when bfourCCVerification is false
                        {
                            tr |= trFail;
                            dbgout(LOG_FAIL) << "After rotation, surface wasn't lost but was changed" << endl;
                            dbgout << "Expected to find " << HEX(dwYUVPRESET) << "; Found " << HEX(*((DWORD*)cddsdAfter.lpSurface)) << endl;
                        }
                    }
                    else
                    {
                        DWORD dwWidth = cddsdBefore.dwWidth;
                        DWORD dwHeight = cddsdBefore.dwHeight;
                        if((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
                        {
                            dwWidth = rcClipRect.right - rcClipRect.left;
                            dwHeight = rcClipRect.bottom - rcClipRect.top;
                        }

                        for (iY = 0; iY < (int)dwHeight; iY++)
                        {
                            for (iX = 0; iX < (int)dwWidth; iX++)
                            {
                                POINT pt = {iX, iY};
                                if (bWindowed && !PtInRect(&rcWindow, pt))
                                {
                                    continue;
                                }
                                float fR = ((float) iX) / ((float) dwWidth);
                                float fG = ((float) iY) / ((float) dwHeight);
                                float fB = ((float) iX + iY) / ((float) dwWidth + dwHeight);
                                float fA = 1.0;
                                DWORD dwR, dwG, dwB, dwA;
                                DWORD dwRExpected,
                                      dwGExpected,
                                      dwBExpected,
                                      dwAExpected;
                                void * pvPixels = (void*)(((BYTE*)(cddsdBefore.lpSurface)) + iY * cddsdBefore.lPitch + iX * cddsdBefore.lXPitch);
                                pcConverter.UnNormalizeRGBA(
                                    fR, fG, fB, 1.0,
                                    dwRExpected,
                                    dwGExpected,
                                    dwBExpected,
                                    dwAExpected);
                                pcConverter.ReadRGBA(dwR, dwG, dwB, dwA, &pvPixels);

                                // Ignore the Alpha value
                                dwA = dwAExpected;

                                if (!pcConverter.IsEqualRGBA(
                                        dwR, dwG, dwB, dwA,
                                        dwRExpected,
                                        dwGExpected,
                                        dwBExpected,
                                        dwAExpected,
                                        1)
                                     && iPrintCount)
                                {
                                    tr |= trFail;
                                    dbgout(LOG_FAIL) << "After rotation, surface wasn't lost but was changed" << endl;
                                    dbgout << "Expected RGBA: {"
                                        << dwRExpected << ", "
                                        << dwGExpected << ", "
                                        << dwBExpected << ", "
                                        << dwAExpected << "}" << endl;
                                    dbgout << "Found RGBA: {"
                                        << dwR << ", " << dwG << ", "
                                        << dwB << ", " << dwA << "}" << endl;
                                    iPrintCount--;
                                }

                            }
                        }
                    }
                }
                hr = m_piDDS->Unlock(prcBoundRect);
                CheckHRESULT(hr, "Unlock", trAbort);
                PrintMemory(__LINE__);
            }
        }
        return tr;
    }

    eTestResult CTest_IsAccessibleDuringLost::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;
        // To test IsAccessibleDuringLost, we need to go through the ways that surfaces
        // can be lost. We can then verify that our surface is accessible,
        // Because only primary windowed case will call this test.
        // the surface should always be lost.
        eTestResult tr = trPass;
        HRESULT hr;

        // Get the surface description
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        // We only do this test for Primary surface,
        if(cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            //variables used for devmode change
            TCHAR tszDeviceName[MAX_PATH] = {0};
            DWORD dwNameSize = countof(tszDeviceName) - 1;
            DEVMODE dm;
            LONG lCDSResult;
            DEVMODE dmSupported;
            DWORD currentOrientation = DMDO_0;

            memset(&dmSupported,0x00, sizeof(DEVMODE));
            dmSupported.dmSize = sizeof(DEVMODE);

            // Get the screen rotations that are supported.
            dmSupported.dmFields = DM_DISPLAYQUERYORIENTATION;
            lCDSResult = ChangeDisplaySettingsEx(NULL, &dmSupported, NULL, CDS_TEST, NULL);

            if (lCDSResult != DISP_CHANGE_SUCCESSFUL)
            {
                dbgout (LOG_ABORT) << "Getting supported devMode failed, abort the test("
                       << lCDSResult << ")" << endl;
                return trAbort;
            }

            // Getting supported rotation angle
            DWORD dwAnglesSupported = dmSupported.dmDisplayOrientation;
            dbgout << "dwAnglesSupported" << dwAnglesSupported << endl;

            // This is the order that we will rotate in. This hits every possible
            // rotation change, which is not unreasonable since it's only 16 (plus
            // one rotation to get in a known state), we start with 90 because 0->0
            // will not cause surface lost.
            enum EAngle {Angle0 = DMDO_0, Angle90 = DMDO_90, Angle180 = DMDO_180, Angle270 = DMDO_270, AngleNoChange};
            EAngle rgRotations[16] = {
                Angle90,
                Angle0,
                Angle180,
                Angle0,
                Angle270,
                Angle0,
                Angle90,
                Angle180,
                Angle90,
                Angle270,
                Angle90,
                Angle180,
                Angle270,
                Angle180,
                Angle0,
                AngleNoChange};
            const int c_iRotations = countof(rgRotations);

            for (int iRotation = 0; iRotation < c_iRotations; iRotation++)
            {
                // We will not test if the rotating angle is AngleNoChange
                // Angle0 won't be tested, it is always supported and it is 0, we can not use bitwise and
                // for testing
                if ((rgRotations[iRotation]!=AngleNoChange) && (rgRotations[iRotation]!=Angle0))
                {
                    // If any of the following mode is not supported, skip current iteration
                    if ( !(dwAnglesSupported & rgRotations[iRotation]))
                    {
                        dbgout << "Driver doesn't support rotate screen to Angle ";
                        switch (rgRotations[iRotation])
                        {
                            case Angle90:
                                dbgout << "90";
                                break;
                            case Angle180:
                                dbgout << "180";
                                break;
                            case Angle270:
                                dbgout << "270";
                                break;
                        }
                        dbgout << ", skip current iteration" << endl;
                        continue;
                    }
                }
                // if current orientation is equal to rotating orientation, skip the iteration, too. (The surface won't be lost)
                if ( currentOrientation == (DWORD)rgRotations[iRotation])
                {
                    dbgout << "Current Orientation and Rotating Orientation are the same angle:";
                    switch (rgRotations[iRotation])
                    {
                        case Angle0:
                            dbgout << "0";
                            break;
                        case Angle90:
                            dbgout << "90";
                            break;
                        case Angle180:
                            dbgout << "180";
                            break;
                        case Angle270:
                            dbgout << "270";
                            break;
                    }
                    dbgout <<", skip this iteration" << endl;
                    continue;
                }



                LPRECT prcBoundRect = NULL;
                RECT rcClipRect = {0, 0, cddsd.dwWidth, cddsd.dwHeight};

                // pass the window's bounding rectangle to Lock/Unlock calls.
                if(!(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
                {
                    tr = GetClippingRectangle(m_piDDS.AsInParam(), rcClipRect);
                    if(trPass != tr)
                    {
                        dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                        return tr;
                    }

                    prcBoundRect = &rcClipRect;
                }

                // For each surface, lock it, but only if the surface isn't lost
                if (SUCCEEDED(m_piDDS->IsLost()))
                {
                    // Lock surface. Save the description
                    hr = m_piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                    CheckHRESULT(hr, "Lock surface", trFail);
                    CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

                    //get dimension information before devmode changes
                    DWORD dwHeight    = prcBoundRect ? prcBoundRect->bottom - prcBoundRect->top : cddsd.dwHeight;
                    DWORD dwWidth     = prcBoundRect ? prcBoundRect->right - prcBoundRect->left: cddsd.dwWidth;

                    // Rotate the screen or do something else that would cause the surface
                    // to be lost.
                    if (rgRotations[iRotation] != AngleNoChange)
                    {
                        memset(&dm, 0x00, sizeof(DEVMODE));
                        dm.dmSize = sizeof(DEVMODE);
                        g_DDConfig.GetDriverName(tszDeviceName, &dwNameSize);
                        wcsncpy_s(dm.dmDeviceName, tszDeviceName, countof(dm.dmDeviceName) - 1);
                        dm.dmFields = DM_DISPLAYORIENTATION;
                        dm.dmDisplayOrientation = rgRotations[iRotation];
                        lCDSResult = ChangeDisplaySettingsEx(tszDeviceName, &dm, NULL, 0, NULL);
                        if (lCDSResult != DISP_CHANGE_SUCCESSFUL)
                        {
                            dbgout (LOG_ABORT) << "ChangeDisplaySettingsEx failed, abort the test ("
                                   << lCDSResult << ")" << endl;
                            return trAbort;
                        }
                        // after successfully changeDisplaySettings, save current orientation
                        currentOrientation = rgRotations[iRotation];
                    }

                    // If the surface is lost, verify the lost surface 's pointer is accesible by filling and reading the entire surface
                    if (FAILED(m_piDDS->IsLost()))
                    {
                        if (0 == iRotation)
                        {
                            dbgout << "Surface lost in transition from original state to "
                                   << (int)rgRotations[iRotation] << endl;
                        }
                        else if (iRotation > 0)
                        {
                            dbgout << "Surface lost in transition from state "
                                   << (int)rgRotations[iRotation - 1] << " to state "
                                   << (int)rgRotations[iRotation] << endl;
                        }

                        // Fill the surface
                        dbgout << "Filling entire surface data after surface lost" << endl;
                        BYTE *pbRow, *pb;
                        DWORD dwWork;
                        DWORD dwMiss;

                        int i,j;

                        dwWork = 0;
                        pbRow = (BYTE*)cddsd.lpSurface;

                        for (i = 0; i < (int)dwHeight; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwWidth; j++)
                                *(pb+j*cddsd.lXPitch) = (BYTE)dwWork++;
                            pbRow += cddsd.lPitch;
                        }

                        // Read the surface data
                        dbgout << "Reading entire surface data after surface lost" << endl;
                        dwWork = 0;
                        dwMiss = 0;
                        pbRow = (BYTE*)cddsd.lpSurface;
                        bool verifyResult = true;
                        for (i = 0; i < (int)dwHeight; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwWidth; j++)
                            {
                                if (*(pb+j*cddsd.lXPitch) != (BYTE)dwWork++)
                                {
                                    if (dwMiss <50)
                                    {
                                        dbgout << "current pixel value is " << *(pb+cddsd.lXPitch) << ", expect " << dwWork << endl;
                                    }
                                    dwMiss++;
                                }
                            }
                            pbRow += cddsd.lPitch;
                        }

                        if (dwMiss>0)
                        {
                            dbgout << "Data different than expected at " << dwMiss << " pixels, which is expeceted and will not fail the test" << endl;
                        }
                        else
                        {
                            dbgout << "Data verification passed" << endl;
                        }

                        // try to restore it,
                        hr = m_piDDS->Restore();
                        if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                        {
                            dbgout << "Could not restore surface, not enough memory" << endl;
                        }
                        // DDraw should not be in a state now where the surface is not restorable,
                        // except if there isn't enough memory.
                        else QueryHRESULT(hr, "Restore", tr |= trFail);
                    }
                    // If the surface isn't lost, it is not correct, windowed primary should always lost after devmode change
                    else
                    {
                        if (rgRotations[iRotation] != AngleNoChange)   //we are not doing any dev mode change when it is AngleOther, therefore surface won't lost
                        {
                            dbgout(LOG_FAIL)
                                << "The Primary Windowed Surface was not lost when supposed to."
                                << endl;
                            return trFail;
                        }
                    }
                }
            }
        }
        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // priv_CTestLockUnlock
    //
    namespace priv_CTestLockUnlock
    {

        void InflateRectRandomly (RECT * prc, int inflateFactor)
        {
            // generate a non-zero number which will have random 4-bit setup (1-15)
            // it must be non-zero or it is the original rect without any change
            LONG randomSeed= RANDOMPOSITIVE (15);
            // base on the bit setup, generate inflated rect
            if (randomSeed & 8)
            {
                prc->left=prc->left-inflateFactor;
            }
            if (randomSeed & 4)
            {
                prc->right=prc->right+inflateFactor;
            }
            if (randomSeed & 2)
            {
                prc->top=prc->top-inflateFactor;
            }
            if (randomSeed & 1)
            {
                prc->bottom=prc->bottom+inflateFactor;
            }
        }

        void OffsetRectRandomly (RECT * prc, int offsetFactor)
        {
             //generate a non-zero 2 bit number which will have random 2-bit setup (1-3)
            LONG randomSeed= RANDOMPOSITIVE(3);
            // base on the bit setup, generate offset rect
            // (notice that left-right pair and top-bottom pair needs to be changed together or you might get a valid smaller rect instead a offset one)
            if (randomSeed & 2)
            {
                prc->left=prc->left+offsetFactor;
                prc->right=prc->right+offsetFactor;
            }
            if (randomSeed & 1)
            {
                prc->top=prc->top+offsetFactor;
                prc->bottom=prc->bottom+offsetFactor;
            }
        }

        //taking a pointer of a RECT, return a 0 size RECT within the original boundary of the passing RECT
        RECT ZeroRectRandomly (RECT * prc)
        {
            RECT rcReturnRect = *prc;

            LONG width=rcReturnRect.right - rcReturnRect.left;
            LONG height=rcReturnRect.bottom - rcReturnRect.top;

            LONG zeroRectX = rcReturnRect.left + (rand() % width);
            LONG zeroRectY = rcReturnRect.top + (rand() % height);
            // setup the zeroRect located at a random location within the original Rect

            rcReturnRect.left=zeroRectX;
            rcReturnRect.right=zeroRectX;
            rcReturnRect.top=zeroRectY;
            rcReturnRect.bottom=zeroRectY;

            return rcReturnRect;
        }

        //taking a pointer of a RECT, modify it so it has at least one of the left/right, top/bottom pair value switched.
        void InvertRectRandomly (RECT * prc)
        {
             //generate a non-zero 2 bit number which will have random 2-bit setup (1-3)
            LONG randomSeed= RANDOMPOSITIVE(3);

            // base on the bit setup, switch pair(s)
            // (notice that left-right pair and top-bottom pair needs to be changed together or you might get a valid smaller rect instead a offset one)
            if (randomSeed & 2)     //top/bottom pair switch
            {
                LONG temp = prc->top;
                prc->top = prc->bottom;
                prc->bottom = temp;
            }
            if (randomSeed & 1)     //left/right pair switch
            {
                LONG temp = prc->left;
                prc->left = prc->right;
                prc->right = temp;
            }
        }


        eTestResult CheckSubRegionLock(IDirectDrawSurface *piDDS, RECT *pBoundingRc, RECT *plockingRc, RECT *psubRc)
        {
            using namespace DDrawUty::Surface_Helpers;
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd;
            HRESULT hr;
            const int nRepeat = 20;

            // output rectangle information
            dbgout << "Bounding Rect:" << *pBoundingRc << endl;;
            if (plockingRc)
            {
                dbgout << "Locked:" << *plockingRc << endl;
            }
            else
            {
                dbgout << "Locked Full Window" << endl;
            }
            dbgout << "Sub:" << *psubRc << endl;

            // Wait for a moment, in case any redrawing was triggered.
            Sleep(1000);

            RECT rcTestLockRect;      // the rectangle used for locking
            RECT rcZeroRect;          // a zero size rectangle used for testing

            // when plockingRc is not NULL, use it directly to generate zero size RC
            if (plockingRc)
            {
                rcZeroRect = ZeroRectRandomly(plockingRc);
            }
            // when we use NULL to lock full window, we should using pBoundingRc to generate zero size RC
            else
            {
                rcZeroRect = ZeroRectRandomly(pBoundingRc);
            }

            //////////////////////////////////////////////////////////////////////////
            // Now locking invalid region with sub region flag set
            //////////////////////////////////////////////////////////////////////////

            //  both cases should fail
            dbgout << "Attemping to lock invalid rects with sub region flag set" << endl;

            if (g_DDConfig.IsCompositorEnabled())   //compositor enabled
            {
                for (int i=0; i<nRepeat;i++)
                {
                    rcTestLockRect=*pBoundingRc;       //get the bounding rect
                    InflateRectRandomly (&rcTestLockRect, RANDOMPOSITIVE(5));  //get a rect which is ramdomly bigger than the bounding rect

                    hr = piDDS->Lock( &rcTestLockRect, &cddsd, DDLOCK_WAITNOTBUSY| DDLOCK_UNLOCKSUBRECT, NULL);
                    if (hr != DDERR_INVALIDRECT)
                    {
                        QueryForHRESULT(hr, DDERR_INVALIDRECT, "Locking bigger than bounding size rect", tr |= trFail);
                        dbgout << "Invalid Rect: " << rcTestLockRect << endl;
                        piDDS->Unlock(&rcTestLockRect);
                    }

                    rcTestLockRect=*pBoundingRc;       //get the bouding rect
                    OffsetRectRandomly (&rcTestLockRect, RANDOMPOSITIVE(5));    //randomly offset it

                    hr = piDDS->Lock( &rcTestLockRect, &cddsd, DDLOCK_WAITNOTBUSY| DDLOCK_UNLOCKSUBRECT , NULL);
                    if (hr != DDERR_INVALIDRECT)
                    {
                        QueryForHRESULT(hr, DDERR_INVALIDRECT, "Locking offset bounding rect", tr |= trFail);
                        dbgout << "Invalid Rect: " << rcTestLockRect << endl;
                        piDDS->Unlock(NULL);
                    }
                }
            }

            //////////////////////////////////////////////////////////////////////////
            // Now lock without sub region flag set, negative test
            //////////////////////////////////////////////////////////////////////////

            // Lock the surface without sub region flag set, should fail the case when trying to unlock sub or zero size rect

            // zero size rect case
            dbgout << "Locking Surface without UNLOCKSUBRECT set" << endl;
            hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock surface without UNLOCKSUBRECT set", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            // Unlock with zero region without setting sub region flag, should fail
            dbgout << "Unlocking zero size region:" << rcZeroRect << endl;
            hr = piDDS->Unlock(&rcZeroRect);
            QueryForHRESULT(hr, DDERR_INVALIDRECT, "Unlock with zero region rect", tr |= trFail);
            // Unlock with correct parameter
            hr = piDDS->Unlock(plockingRc);
            CheckHRESULT(hr, "Unlock", trFail);

            // sub size rect case
            dbgout << "Locking Surface without UNLOCKSUBRECT set" << endl;
            hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock surface without UNLOCKSUBRECT set", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            // Unlock with sub region without setting sub region flag, should fail
            dbgout << "Unlocking sub region:" << *psubRc << endl;
            hr = piDDS->Unlock(psubRc);
            QueryForHRESULT(hr, DDERR_INVALIDRECT, "Unlock sub size rect without sub region flag set", tr |= trFail);
            // Unlock with correct parameter
            hr = piDDS->Unlock(plockingRc);
            CheckHRESULT(hr, "Unlock", trFail);

            //////////////////////////////////////////////////////////////////////////
            // Now with sub region flag set, positive cases
            //////////////////////////////////////////////////////////////////////////

            // zero size rect case
            dbgout << "Locking Surface with UNLOCKSUBRECT set" << endl;
            hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY| DDLOCK_UNLOCKSUBRECT, NULL);
            CheckHRESULT(hr, "Lock surface", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            // Unlock zero region with sub region flag set, should success
            dbgout << "Unlocking zero size region:" << rcZeroRect << endl;
            hr = piDDS->Unlock(&rcZeroRect);
            CheckHRESULT(hr, "Unlock zero rect", trFail);

            // zero sub rect case
            dbgout << "Locking Surface with UNLOCKSUBRECT set" << endl;
            hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY| DDLOCK_UNLOCKSUBRECT, NULL);
            CheckHRESULT(hr, "Lock surface", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            // Unlock sub region with sub region flag set, should success
            dbgout << "Unlocking sub region:" << *psubRc << endl;
            hr = piDDS->Unlock(psubRc);
            CheckHRESULT(hr, "Unlock sub rect", trFail);

            // locking size rect case
            dbgout << "Locking Surface with UNLOCKSUBRECT set" << endl;
            hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY| DDLOCK_UNLOCKSUBRECT, NULL);
            CheckHRESULT(hr, "Lock surface", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            // Unlock locking size rect with sub region flag set, should success
            if (plockingRc)
            {
                dbgout << "Unlocking full locked size region:" << *plockingRc << endl;
            }
            else
            {
                dbgout << "Unlocking full locked size region:" << *pBoundingRc << endl;
            }
            hr = piDDS->Unlock(plockingRc);
            CheckHRESULT(hr, "Unlock locking size rect", trFail);


            //////////////////////////////////////////////////////////////////////////
            // Now with sub region flag set
            //////////////////////////////////////////////////////////////////////////

            for (int i=0; i<nRepeat;i++)
            {
                //test negative size region

                //first we get a smaller size region
                if (plockingRc)     // when plockingRc is not NULL,
                {
                    rcTestLockRect=*plockingRc;       // using locking rect
                }
                else
                {
                    rcTestLockRect=*pBoundingRc;       //when plockingRc is NULL, locking size is the bounding rect
                }

                // get smaller size region, we don't want to resize it when it is too small, otherwise we might end up with a larger region case
                // (like a 1x1 rectangle after negative inflation might become a 4x4, which will fail the test case)
                if (((rcTestLockRect.right-rcTestLockRect.left)>5) && ((rcTestLockRect.bottom-rcTestLockRect.top)>5))
                {
                    InflateRectRandomly (&rcTestLockRect, -RANDOMPOSITIVE(5));
                }
                // previous check made sure we will only have positve case and we randomly flip the value of top/bottom or left/right to make it negative size case
                InvertRectRandomly(&rcTestLockRect);

                // Lock the surface with sub region flag set
                dbgout << "Locking Surface with UNLOCKSUBRECT set" << endl;
                hr = piDDS->Lock( plockingRc, &cddsd, DDLOCK_WAITNOTBUSY | DDLOCK_UNLOCKSUBRECT, NULL);
                CheckHRESULT(hr, "Lock surface", trFail);
                CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

                // Unlock with negative size region should fail
                dbgout << "Unlocking negative size region:" << rcTestLockRect << endl;
                hr = piDDS->Unlock(&rcTestLockRect);
                CheckHRESULT(hr, "Unlock negative size region", trFail);

                //test larger size region

                if (plockingRc)     // when plockingRc is not NULL,
                {
                    rcTestLockRect=*plockingRc;       // using locking rect
                }
                else
                {
                    rcTestLockRect=*pBoundingRc;       //when plockingRc is NULL, locking size is the bounding rect
                }

                InflateRectRandomly (&rcTestLockRect, RANDOMPOSITIVE(5));  //get larger region than locking/bouding rc size

                // Lock the surface with sub region flag set
                dbgout << "Locking Surface with UNLOCKSUBRECT set" << endl;
                hr = piDDS->Lock( plockingRc, &cddsd, DDLOCK_WAITNOTBUSY | DDLOCK_UNLOCKSUBRECT, NULL);
                CheckHRESULT(hr, "Lock surface with UNLOCKSUBRECT set", trFail);
                CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

                // Unlock with larger size region should fail
                dbgout << "Unlocking larger than bounding size region:" << rcTestLockRect << endl;
                hr = piDDS->Unlock(&rcTestLockRect);
                QueryForHRESULT(hr, DDERR_INVALIDRECT, "Unlock with larger than bounding size rect", tr |= trFail);

                if (hr!=S_OK) // Unlock with correct parameter
                {
                    hr = piDDS->Unlock(plockingRc);
                    CheckHRESULT(hr, "Unlock", trFail);
                }

                //test offset region

                if (plockingRc)     // when plockingRc is not NULL,
                {
                    rcTestLockRect=*plockingRc;       // using locking rect
                }
                else
                {
                    rcTestLockRect=*pBoundingRc;       //when plockingRc is NULL, locking size is the bounding rect
                }

                OffsetRectRandomly (&rcTestLockRect, RANDOMPOSITIVE(5));  //get larger region than window rc size, the sizeFactor will be 1-20

                // Lock the surface with sub region flag set
                dbgout << "Locking Surface with UNLOCKSUBRECT set" << endl;
                hr = piDDS->Lock( plockingRc, &cddsd, DDLOCK_WAITNOTBUSY | DDLOCK_UNLOCKSUBRECT, NULL);
                CheckHRESULT(hr, "Lock surface with UNLOCKSUBRECT set", trFail);
                CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

                // Unlock with offset size region should fail
                dbgout << "Unlocking offset size region:" << rcTestLockRect << endl;
                hr = piDDS->Unlock(&rcTestLockRect);
                QueryForHRESULT(hr, DDERR_INVALIDRECT, "Unlock with offset window size rect", tr |= trFail);

                if (hr!=S_OK) // Unlock with correct parameter
                {
                    hr = piDDS->Unlock(plockingRc);
                    CheckHRESULT(hr, "Unlock", trFail);
                }

                if (tr == trFail)   // no need to keep looping when we get fail result
                {
                    break;
                }
            }
            return tr;
        }

        eTestResult CheckSubRegionLockEfficiency(IDirectDrawSurface *piDDS, RECT *pBoundingRc, RECT *plockingRc, RECT *psubRc)
        {
            using namespace DDrawUty::Surface_Helpers;
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd;
            HRESULT hr;

            // Check the number of ticks taken by Lock
            DWORD dwStartTick;
            DWORD dwEndTick;
            DWORD dwFullLockUnlockTick;
            DWORD dwSubRegionLockUnlockTick;

            const LONG lLockUnlockTimes = 50000;

            dbgout << "Now checking the locking and unlocking time with and without sub region flag set" << endl;

            // Full size lock unlock
            dwStartTick = GetTickCount();
            if (plockingRc)         //when we use NULL to lock full window, we should using pBoundingRc to generate zero size RC
            {
                dbgout << "Locking " << *plockingRc << endl;
                dbgout << "Unlocking" << *plockingRc << endl;
            }
            else
            {
                dbgout << "Locking " << *pBoundingRc << endl;
                dbgout << "Unocking " << *pBoundingRc << endl;
            }

            for (int i=0;i<lLockUnlockTimes;i++)   //lock and unlock for a lot of times to get average locking time
            {
                hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Locking without sub region flag set", trFail);
                hr = piDDS->Unlock(plockingRc);
                CheckHRESULT(hr, "Unlocking without sub region flag set", trFail);
            }
            dwEndTick = GetTickCount();
            dwFullLockUnlockTick = dwEndTick - dwStartTick;
            dbgout << "Full lock unlock " << lLockUnlockTimes << " times take " << dwFullLockUnlockTick << " ticks"<< endl;

            // Sub region size lock unlock
            dwStartTick = GetTickCount();
            if (plockingRc)         //when we use NULL to lock full window, we should using pBoundingRc to generate zero size RC
            {
                dbgout << "Locking " << *plockingRc << endl;
            }
            else
            {
                dbgout << "Locking " << *pBoundingRc << endl;
            }
            dbgout << "Unlocking" << *psubRc << endl;

            for (int i=0;i<lLockUnlockTimes;i++)
            {
                hr = piDDS->Lock(plockingRc, &cddsd, DDLOCK_WAITNOTBUSY| DDLOCK_UNLOCKSUBRECT, NULL);
                CheckHRESULT(hr, "Locking with sub region flag set", trFail);
                hr = piDDS->Unlock(psubRc);
                CheckHRESULT(hr, "Unlocking with sub region flag set", trFail);
            }
            dwEndTick = GetTickCount();
            dwSubRegionLockUnlockTick = dwEndTick - dwStartTick;
            dbgout << "Sub region lock unlock " << lLockUnlockTimes << " times take " << dwSubRegionLockUnlockTick << " ticks"<< endl;

            if(dwSubRegionLockUnlockTick > dwFullLockUnlockTick)
            {
             dbgout << "WARNING : Time taken by Sub Region Lock Unlock is longer than full lock unlock" << endl;
            }
            return tr;
        }

        eTestResult CheckLock(IDirectDrawSurface *piDDS, RECT *prc, RECT *prc2 = NULL)
        {
            using namespace DDrawUty::Surface_Helpers;
            eTestResult tr = trPass;
            CDDSurfaceDesc cddsd,
                           cddsd2;
            HRESULT hr = S_OK;
            int i, j;
            bool fPlanarFourCC = false;
            DWORD dwHeight,
                  dwWidth,
                  dwByteWidth,
                  dwHeight2 = 0,
                  dwWidth2 = 0,
                  dwByteWidth2 = 0;

            // Wait for a moment, in case any redrawing was triggered.
            Sleep(1000);

            // Try to lock invalid regions
            dbgout << "Attemping to lock invalid rects" << endl;
            RECT rcFail = { -1, 0, 10, 10 };
            hr = piDDS->Lock(&rcFail, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            if (hr != DDERR_INVALIDRECT)
            {
                QueryForHRESULT(hr, DDERR_INVALIDRECT, "Lock invalid rect", tr |= trFail);
                piDDS->Unlock(NULL);
            }

            // Set Surface Data
            // ================
            // Lock the surface
            dbgout << "Locking Entire Surface for filling" << endl;
            hr = piDDS->Lock(prc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock surface", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            dwHeight    = prc ? prc->bottom - prc->top : cddsd.dwHeight,
            dwWidth     = prc ? prc->right - prc->left : cddsd.dwWidth,
            dwByteWidth = dwWidth*cddsd.ddpfPixelFormat.dwRGBBitCount/8;

            if (cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC && IsPlanarFourCC(cddsd.ddpfPixelFormat.dwFourCC))
            {
                fPlanarFourCC = true;
            }

            // Fill the surface
            BYTE *pbRow, *pb;
            DWORD dwWork;
            DWORD dwMisses;

            dbgout << "Filling surface with data" << endl;
            dwWork = 0;
            pbRow = (BYTE*)cddsd.lpSurface;
            if (!fPlanarFourCC)
            {
                for (i = 0; i < (int)dwHeight; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwByteWidth; j++)
                        *(pb++) = (BYTE)dwWork++;
                    pbRow += cddsd.lPitch;
                }
            }
            else
            {
                // Fill first plane
                for (i = 0; i < (int)dwHeight; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwWidth; j++)
                        *(pb++) = (BYTE)dwWork++;
                    pbRow += dwWidth;
                }
                for (i = 0; i < (int)dwHeight; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwWidth/2; j++)
                        *(pb++) = (BYTE)dwWork++;
                    pbRow += dwWidth/2;
                }
            }

            if (prc2)
            {
                // Set Surface Data in second RECT
                // ===============================
                // Lock the surface
                dbgout << "Locking Entire Surface for filling" << endl;
                hr = piDDS->Lock(prc2, &cddsd2, DDLOCK_WAITNOTBUSY, NULL);
                QueryHRESULT(hr, "Lock surface", trFail)
                else QueryCondition(NULL == cddsd2.lpSurface, "Lock didn't fill lpSurface pointer", trFail)
                else
                {
                    dwHeight2    = prc2 ? prc2->bottom - prc2->top : cddsd2.dwHeight,
                    dwWidth2     = prc2 ? prc2->right - prc2->left : cddsd2.dwWidth,
                    dwByteWidth2 = dwWidth2*cddsd2.ddpfPixelFormat.dwRGBBitCount/8;

                    // Fill the surface
                    dbgout << "Filling surface with data" << endl;
                    dwWork = 0;
                    pbRow = (BYTE*)cddsd2.lpSurface;
                    if (!fPlanarFourCC)
                    {
                        for (i = 0; i < (int)dwHeight2; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwByteWidth2; j++)
                                *(pb++) = (BYTE)dwWork++;
                            pbRow += cddsd2.lPitch;
                        }
                    }
                    else
                    {
                        // Fill first plane
                        for (i = 0; i < (int)dwHeight2; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwWidth2; j++)
                                *(pb++) = (BYTE)dwWork++;
                            pbRow += dwWidth2;
                        }
                        for (i = 0; i < (int)dwHeight2; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwWidth2/2; j++)
                                *(pb++) = (BYTE)dwWork++;
                            pbRow += dwWidth2/2;
                        }
                    }

                    // Unlock
                    hr = piDDS->Unlock(NULL);
                    QueryHRESULT(hr, "Unlock", trFail);
                }
            }

            // Unlock
            hr = piDDS->Unlock(prc);
            CheckHRESULT(hr, "Unlock", trFail);

            // Verify Surface Data
            // ===================
            // Lock the surface
            dbgout << "Locking surface for verification" << endl;
            hr = piDDS->Lock(prc, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock surface", trFail);
            CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);

            // Verify the surface data
            dbgout << "Verifying surface data" << endl;
            dwWork = 0;
            dwMisses = 0;
            pbRow = (BYTE*)cddsd.lpSurface;
            if (!fPlanarFourCC)
            {
                for (i = 0; i < (int)dwHeight && dwMisses < 50; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwByteWidth && dwMisses < 50; j++)
                        QueryCondition(*(pb++) != (BYTE)dwWork++,
                            "Data different than expected at (" << j << "," << i << "), (found " << HEX(*(pb-1)) << "; expected " << HEX((BYTE)(dwWork-1)) << ");",
                            tr |= trFail;++dwMisses);
                    pbRow += cddsd.lPitch;
                }
            }
            else
            {
                for (i = 0; i < (int)dwHeight && dwMisses < 50; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwWidth && dwMisses < 50; j++)
                        QueryCondition(*(pb++) != (BYTE)dwWork++, "Data different than expected", tr |= trFail;++dwMisses);
                    pbRow += dwWidth;
                }
                for (i = 0; i < (int)dwHeight && dwMisses < 50; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwWidth/2 && dwMisses < 50; j++)
                        QueryCondition(*(pb++) != (BYTE)dwWork++, "Data different than expected", tr |= trFail;++dwMisses);
                    pbRow += dwWidth/2;
                }
            }

            if (prc2)
            {
                // Verify Surface Data in second RECT
                // ==================================
                // Lock the surface
                dbgout << "Locking Entire Surface for filling" << endl;
                hr = piDDS->Lock(prc2, &cddsd2, DDLOCK_WAITNOTBUSY, NULL);
                QueryHRESULT(hr, "Lock surface", tr |= trFail)
                else QueryCondition(NULL == cddsd2.lpSurface, "Lock didn't fill lpSurface pointer", tr |= trFail)
                else
                {
                    // Verify the surface data
                    dbgout << "Verifying surface data" << endl;
                    dwWork = 0;
                    dwMisses = 0;
                    pbRow = (BYTE*)cddsd2.lpSurface;
                    if (!fPlanarFourCC)
                    {
                        for (i = 0; i < (int)dwHeight2 && dwMisses < 50; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwByteWidth2 && dwMisses < 50; j++)
                                QueryCondition(*(pb++) != (BYTE)dwWork++, "Data different than expected", tr |= trFail;++dwMisses);
                            pbRow += cddsd.lPitch;
                        }
                    }
                    else
                    {
                        for (i = 0; i < (int)dwHeight2 && dwMisses < 50; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwWidth2 && dwMisses < 50; j++)
                                QueryCondition(*(pb++) != (BYTE)dwWork++, "Data different than expected", tr |= trFail;++dwMisses);
                            pbRow += dwWidth2;
                        }
                        for (i = 0; i < (int)dwHeight2 && dwMisses < 50; i++)
                        {
                            pb = pbRow;
                            for (j = 0; j < (int)dwWidth2/2 && dwMisses < 50; j++)
                                QueryCondition(*(pb++) != (BYTE)dwWork++, "Data different than expected", tr |= trFail;++dwMisses);
                            pbRow += dwWidth2/2;
                        }
                    }

                    // Unlock
                    hr = piDDS->Unlock(prc);
                    QueryHRESULT(hr, "Unlock", tr |= trFail);
                }
            }

            // Unlock
            hr = piDDS->Unlock(prc);
            CheckHRESULT(hr, "Unlock", trFail);

            return tr;
        }

    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_LockUnlock::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_LockUnlock::TestIDirectDrawSurface()
    {
        using namespace DDrawUty::Surface_Helpers;
        using namespace priv_CTestLockUnlock;

        eTestResult tr = trPass;
        HRESULT hr;
        RECT rc;

        // Get the surface description
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        LPRECT prcBoundRect = NULL;
        RECT rcClipRect = {0, 0, cddsd.dwWidth, cddsd.dwHeight};

        // If it is a Primary in Windowed mode, pass the window's bounding rectangle to Lock/Unlock calls.
        if((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
        {
            tr = GetClippingRectangle(m_piDDS.AsInParam(), rcClipRect);
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }

            prcBoundRect = &rcClipRect;
        }

        // attempt to grab lock
        dbgout << "Attempting to lock entire surface" << endl;
        hr = m_piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);

        // if successful
        if(hr==S_OK)
        {
            // unlock and run tests
            dbgout << "Lock succeded, continuing with tests" << endl;
            m_piDDS->Unlock(prcBoundRect);

            // Lock the entire surface
            dbgout << "Locking Entire Surface for filling" << endl;
            tr |= CheckLock(m_piDDS.AsInParam(), prcBoundRect);

            // Lock a single portion of the surface
            GenRandRect(&rcClipRect, &rc);
            dbgout << "Locking Entire Surface for filling" << endl;
            dbgout << rc << endl;
            tr |= CheckLock(m_piDDS.AsInParam(), &rc);

            RECT rcSub;
            // Lock full surface with sub reg unlock flag set
            GenRandRect(&rcClipRect, &rcSub);      // the smaller region
            dbgout << "Locking full surface with sub region unlock flag test" << endl;
            if (prcBoundRect)   //windowed primary case
            {
                // Get the bounding rectangle for the main window
                RECT rcWindowBound = {0, 0, 0, 0};
                if(!GetWindowRect(CWindowsModule::GetWinModule().m_hWnd, &rcWindowBound))
                {
                    dbgout(LOG_ABORT) << "Unable to get the window's rectangle. Error : ("
                                      << GetLastError() << ") " << g_ErrorMap[GetLastError()]
                                      << endl;
                    return trAbort;
                }
                dbgout << "Window Bounding Region" << rcWindowBound << endl;
                dbgout << "Full Region (Window Client) for locking" << *prcBoundRect << endl;
                dbgout << "Sub region for unlocking" << rcSub << endl;
                tr |= CheckSubRegionLock(m_piDDS.AsInParam(),&rcWindowBound, &rcClipRect, &rcSub);
                tr |= CheckSubRegionLockEfficiency(m_piDDS.AsInParam(),&rcWindowBound, &rcClipRect, &rcSub);
            }
            else
            {
                dbgout << "Bounding Region" << rcClipRect << endl;
                dbgout << "Use Full size for locking" << endl;
                dbgout << "Sub region for unlocking" << rcSub << endl;
                tr |= CheckSubRegionLock(m_piDDS.AsInParam(),&rcClipRect, NULL, &rcSub);
                tr |= CheckSubRegionLockEfficiency(m_piDDS.AsInParam(),&rcClipRect, NULL, &rcSub);
            }


            // Lock a portion of the surface with sub reg unlock flag set
            GenRandRect(&rcClipRect, &rc);
            GenRandRect(&rc, &rcSub);       // sub region of this portion of surface, for unlocking
            dbgout << "Locking a portion of the surface with sub region unlock flag set" << endl;
            if (prcBoundRect)   //windowed primary case
            {
                // Get the bounding rectangle for the main window
                RECT rcWindowBound = {0, 0, 0, 0};
                if(!GetWindowRect(CWindowsModule::GetWinModule().m_hWnd, &rcWindowBound))
                {
                    dbgout(LOG_ABORT) << "Unable to get the window's rectangle. Error : ("
                                      << GetLastError() << ") " << g_ErrorMap[GetLastError()]
                                      << endl;
                    return trAbort;
                }
                dbgout << "Window Bounding Region" << rcWindowBound << endl;
                dbgout << "A portion of the surface for locking" << rc << endl;
                dbgout << "Even smaller Sub region for unlocking" << rcSub << endl;
                tr |= CheckSubRegionLock(m_piDDS.AsInParam(),&rcWindowBound, &rc, &rcSub);
                tr |= CheckSubRegionLockEfficiency(m_piDDS.AsInParam(),&rcWindowBound, &rc, &rcSub);
            }
            else
            {
                dbgout << "Bounding Region" << rcClipRect << endl;
                dbgout << "A portion of the surface for locking" << rc << endl;
                dbgout << "Even smaller Sub region for unlocking" << rcSub << endl;
                tr |= CheckSubRegionLock(m_piDDS.AsInParam(),&rcClipRect, &rc, &rcSub);
                tr |= CheckSubRegionLockEfficiency(m_piDDS.AsInParam(),&rcClipRect, &rc, &rcSub);
            }


//            if (!(cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC && IsPlanarFourCC(cddsd.ddpfPixelFormat.dwFourCC)))
//            {
//            // Test simultaneously locking two non-intersecting rects
//            // Choose vertical or horizontal partition
//            RECT rc1,
//                 rc2;
//            GenRandNonOverlappingRects(cddsd.dwWidth, cddsd.dwHeight, &rc1, &rc2);
//            dbgout << rc1 << endl << rc2 << endl;
//            tr |= CheckLock(m_piDDS.AsInParam(), &rc1, &rc2);
//            }
        }

        // If this is a primary in system mem, and we can't lock, pass
        // some drivers appear to allow this case, however it's not supported
        // in directdraw spec.
        else if(SUCCEEDED(hr))
        {
            dbgout << "Unexpected successful error code: hr= " << g_ErrorMap[hr] << endl;
            // successful locking, however unexpected error code.
            // this situation shouldn't happen, but if it does it should probably
            // be investigated.
            tr |= trFail;
        }
        else if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
                m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
        {
            dbgout << "Lock failed on a primary in system memory, PASS: hr= " << g_ErrorMap[hr] << endl;
        }
        // we failed to obtain a lock.
        else
        {
            dbgout << "Failed to obtain lock on a lockable surface, hr= " << g_ErrorMap[hr] << endl;
            tr |= trFail;
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_ReleaseDC::Test
    //  Tests the Blt Method of IDirectDrawSurface.
    //
    eTestResult CTest_ReleaseDC::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;
        HRESULT hr;

#if CFG_TEST_INVALID_PARAMETERS
#endif

        // We don't have a DC, attempt to release nothing
        hr = m_piDDS->ReleaseDC(NULL);
        CheckForHRESULT(hr, DDERR_NODC, "ReleaseDC(NULL)", trFail);

        // If this is a primary is system mem or in windowed mode, we can't GetDC
        if (!(m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE &&
              (m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY || g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
           )
        {
            HDC hdc;

            // Get DC
            dbgout << "Getting DC" << endl;
            hr = m_piDDS->GetDC(&hdc);
            if (FAILED(hr))
            {
                // If this is a primary is system mem, then failing to get the DC is OK.
                if (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC && DDERR_CANTCREATEDC == hr)
                {
                    dbgout << "GetDC Failed on un-DC-able surface.  OK." << endl;
                    return trPass;
                }
                CheckHRESULT(hr, "GetDC", trFail);
            }

            // Release the DC
            dbgout << "Releasing the DC (twice)" << endl;
            hr = m_piDDS->ReleaseDC(hdc);
            CheckHRESULT(hr, "ReleaseDC", trFail);

            // Try releasing the DC again
            hr = m_piDDS->ReleaseDC(hdc);
            if(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)
            {
                // Calls to ReleaseDC() on a surface that owns a dc succeeds without error, even if called multiple times.
                // The DC will persist till the Surface is released.
                CheckForHRESULT(hr, S_OK, "ReleaseDC a second time", trFail);
            }
            else
            {
                CheckForHRESULT(hr, DDERR_NODC, "ReleaseDC a second time", trFail);
            }
        }

        return tr;
    }

    namespace priv_CTest_SetHWnd
    {
        // Verifies that the return values of Lock, Blt, AlphaBlt is hrExpected.
        eTestResult InvalidClipperTests(HRESULT hrExpected, IDirectDrawSurface* piddsWindowedPrimary, IDirectDrawSurface* piddsSurface)
        {
            if(!piddsWindowedPrimary || !piddsSurface)
            {
                return trFail;
            }

            HRESULT hr = S_OK;
            DDSURFACEDESC ddsd;
            memset(&ddsd, 0, sizeof(ddsd));
            ddsd.dwSize = sizeof(ddsd);

            // Get the bounding rectangle for the main window
            RECT rcBound = {0, 0, 0, 0};
            if(!GetWindowRect(CWindowsModule::GetWinModule().m_hWnd, &rcBound))
            {
                dbgout(LOG_ABORT) << "Unable to get the main window's rectangle. Error : ("
                                  << GetLastError() << ") " << g_ErrorMap[GetLastError()]
                                  << endl;
                return trAbort;
            }

            // Lock
            hr = piddsWindowedPrimary->Lock(&rcBound, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(S_OK == hr)
            {
                CheckCondition((piddsWindowedPrimary->Unlock(&rcBound) != S_OK), "Unlock with the correct rect should return S_OK", trFail);
            }
            if(hrExpected != hr)
            {
                dbgout(LOG_FAIL) << "Lock should return " << g_ErrorMap[hrExpected] << " instead it returned "
                                 << HEX((DWORD)hr) << " " <<  g_ErrorMap[hr]
                                 << endl;
                return trFail;
            }

            // Blt
            hr = piddsSurface->Blt(NULL, piddsWindowedPrimary, NULL, DDBLT_WAITNOTBUSY, NULL);
            if(hrExpected != hr)
            {
                dbgout(LOG_FAIL) << "Blt should return " << g_ErrorMap[hrExpected] << " instead it returned "
                                 << HEX((DWORD)hr) << " " <<  g_ErrorMap[hr]
                                 << endl;
                return trFail;
            }

            hr = piddsWindowedPrimary->Blt(NULL, piddsSurface, NULL, DDBLT_WAITNOTBUSY, NULL);
            if(hrExpected != hr)
            {
                dbgout(LOG_FAIL) << "Blt should return " << g_ErrorMap[hrExpected] << " instead it returned "
                                 << HEX((DWORD)hr) << " " <<  g_ErrorMap[hr]
                                 << endl;
                return trFail;
            }

            // AlphaBlt
            hr = piddsSurface->AlphaBlt(NULL, piddsWindowedPrimary, NULL, DDBLT_WAITNOTBUSY, NULL);
            if(hrExpected != hr)
            {
                dbgout(LOG_FAIL) << "AlphaBlt should return " << g_ErrorMap[hrExpected] << " instead it returned "
                                 << HEX((DWORD)hr) << " " <<  g_ErrorMap[hr]
                                 << endl;
                return trFail;
            }

            hr = piddsWindowedPrimary->AlphaBlt(NULL, piddsSurface, NULL, DDBLT_WAITNOTBUSY, NULL);
            if(hrExpected != hr)
            {
                dbgout(LOG_FAIL) << "AlphaBlt should return " << g_ErrorMap[hrExpected] << " instead it returned "
                                 << HEX((DWORD)hr) << " " <<  g_ErrorMap[hr]
                                 << endl;
                return trFail;
            }

            return trPass;
        }
        // Perform negative tests on a Windowed Primary.
        // Returns either trFail or trPass.
        eTestResult PerformNegativeTests(IDirectDraw* pidd,
                                         IDirectDrawSurface* piddsWindowedPrimary,
                                         IDirectDrawSurface* piddsSurface
                                        )
        {
            using namespace DDrawUty::Surface_Helpers;

            if(!pidd || !piddsWindowedPrimary || !piddsSurface)
            {
                dbgout(LOG_FAIL) << "NULL pointer passed in to PerformNegativeTests" << endl;
                return trFail;
            }

            HRESULT hr = S_OK;
            eTestResult tr = trPass;
            DDSURFACEDESC ddsd;
            memset(&ddsd, 0, sizeof(ddsd));
            ddsd.dwSize = sizeof(ddsd);

            // 1. Passing a NULL rectangle to Lock on a Windowed Primary should return
            //    S_OK
            hr = piddsWindowedPrimary->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(S_OK == hr)
            {
               piddsWindowedPrimary->Unlock(NULL);
            }
            CheckCondition((S_OK != hr), "Passing a NULL rectangle to Lock on a Windowed Primary should return S_OK", trFail);

            // 1A. Passing an invalid rectangle to Lock on a Windowed Primary should return
            //    DDERR_INVALIDRECT
            RECT invalidRect = {0, 0, 0, 0};
            hr = piddsWindowedPrimary->Lock(&invalidRect, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(S_OK == hr)
            {
               piddsWindowedPrimary->Unlock(NULL);
               dbgout(LOG_FAIL) << "Passing an invalid rectangle to Lock on a Windowed Primary is succeeded. So test should fail"
                                << endl;
                return trFail;
            }
            CheckCondition((DDERR_INVALIDRECT != hr), "Passing a invalid rectangle to Lock on a Windowed Primary should return DDERR_INVALIDRECT", trFail);


            // Get the bounding rectangle for the main window
            RECT rcBound = {0, 0, 0, 0};
            if(!GetWindowRect(CWindowsModule::GetWinModule().m_hWnd, &rcBound))
            {
                dbgout(LOG_ABORT) << "Unable to get the main window's rectangle. Error : ("
                                  << GetLastError() << ") " << g_ErrorMap[GetLastError()]
                                  << endl;
                return trAbort;
            }

            // Set the main window as the clipping rectangle
            tr = ClipToWindow(pidd, piddsWindowedPrimary,
                              CWindowsModule::GetWinModule().m_hWnd
                             );
            if(tr != trPass)
            {
                dbgout(LOG_FAIL) << "Unable to set the clipping window" << endl;
                return tr;
            }

            // 2. Lock should return DDERR_INVALIDRECT on passing a rectangle that exceeds the window rect.
            RECT rcBadRect = {0, 0, 0, 0};
            hr = piddsWindowedPrimary->GetSurfaceDesc(&ddsd); // get surface description
            CheckCondition((S_OK != hr), "GetSurfaceDesc should return S_OK", trFail);
            rcBadRect.bottom = ddsd.dwHeight + 1;
            rcBadRect.right = ddsd.dwWidth + 1;

            hr = piddsWindowedPrimary->Lock(&rcBadRect, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(S_OK == hr)
            {
               piddsWindowedPrimary->Unlock(&rcBadRect);
            }
            CheckCondition((DDERR_INVALIDRECT != hr), "Lock should return DDERR_INVALIDRECT on passing a rectangle that exceeds the window rect.", trFail);

            // 3. Unlock with a different rectangle should return DDERR_INVALIDRECT
            hr = piddsWindowedPrimary->Lock(&rcBound, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if(S_OK == hr)
            {
                hr = piddsWindowedPrimary->Unlock(&rcBadRect);

                CheckCondition((DDERR_INVALIDRECT != hr), "Unlock with a different rectangle should return DDERR_INVALIDRECT", trFail);

                // 4. SetClipper should return DD_OK when surface is locked by same thread.
                hr = piddsWindowedPrimary->SetClipper(NULL);  //delete clipper from surface
                CheckCondition((S_OK != hr), "SetClipper should return S_OK when surface is locked by same thread", trFail);
             

                // Create the clipper
                CComPointer<IDirectDrawClipper> piddc;
                hr = pidd->CreateClipper(0, piddc.AsTestedOutParam(), NULL);
                CheckHRESULT(hr, "CreateClipper", trFail);
                CheckCondition(piddc.InvalidOutParam(), "CreateClipper failed to fill Clipper object", trFail);

                // Set up the clip list
                hr = piddc->SetHWnd(0, CWindowsModule::GetWinModule().m_hWnd);
                CheckHRESULT(hr, "SetHWnd", trFail);

                // Set the new clipper
                hr = piddsWindowedPrimary->SetClipper(piddc.AsInParam());
                CheckCondition((S_OK != hr), "SetClipper should return S_OK when surface is locked by same thread", trFail);

                CheckCondition((piddsWindowedPrimary->Unlock(&rcBound) != S_OK), "Unlock with the correct rect should return S_OK", trFail);

            }
            else
            {
                CheckCondition((S_OK != hr), "Lock with a valid rectangle should return S_OK", trFail);
            }

            // Remove the clipper
            piddsWindowedPrimary->SetClipper(NULL);

            // Now set a clipper with no HWND on the Primary.

            // Create the clipper
            CComPointer<IDirectDrawClipper> piddcWithNoHwnd;
            hr = pidd->CreateClipper(0, piddcWithNoHwnd.AsTestedOutParam(), NULL);
            CheckHRESULT(hr, "CreateClipper", trFail);
            CheckCondition(piddcWithNoHwnd.InvalidOutParam(), "CreateClipper failed to fill Clipper object", trFail);

            // Set the new clipper
            hr = piddsWindowedPrimary->SetClipper(piddcWithNoHwnd.AsInParam());
            CheckCondition((S_OK != hr), "SetClipper should return S_OK", trFail);

            // Set a clipper with an invlaid window handle
            CComPointer<IDirectDrawClipper> piddcWithInvalidHwnd;
            hr = pidd->CreateClipper(0, piddcWithInvalidHwnd.AsTestedOutParam(), NULL);
            CheckHRESULT(hr, "CreateClipper", trFail);
            CheckCondition(piddcWithInvalidHwnd.InvalidOutParam(), "CreateClipper failed to fill Clipper object", trFail);

            // Set up the clip list with an invalid handle
            hr = piddcWithInvalidHwnd->SetHWnd(0, (HWND)(0));
            CheckHRESULT(hr, "SetHWnd", trFail);

            // Set the new clipper
            hr = piddsWindowedPrimary->SetClipper(piddcWithInvalidHwnd.AsInParam());
            CheckCondition((S_OK != hr), "SetClipper should return S_OK", trFail);

            // GetDC uses IDirectDrawSurface::Lock() internally hence it returns sucess when NULL is passed
            HDC hdc = NULL;
            hr = piddsWindowedPrimary->GetDC(&hdc);
            if(S_OK != hr)
            {
                dbgout(LOG_FAIL) << "GetDC on the Windowed Primary Failed."
                                 << endl;
                return trFail;
            }
            else
                piddsWindowedPrimary->ReleaseDC(hdc);

            return tr;
        }
    }

    void MessagePump(HWND hWnd)
    {
        MSG msg;
        const MSGPUMP_LOOPCOUNT = 10;
        const MSGPUMP_LOOPDELAY = 50; // msec

        for (INT i = 0; i < MSGPUMP_LOOPCOUNT; i++)
        {
            while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            Sleep(MSGPUMP_LOOPDELAY);
        }
    }

    HRESULT CreateWindows(vector<HWND>& rghwnd)
    {
        HRESULT hr = S_OK;
        const TCHAR tszWindowClass[] = _T("DDFUNC_HELPER_WINDOW");
        RECT g_rcWorkArea;
        HWND hwnd = NULL;
        WNDCLASS wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = (WNDPROC) DefWindowProc;
        wc.hInstance = (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance;
        wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wc.lpszClassName = tszWindowClass;

        RegisterClass(&wc);

        if(!SystemParametersInfo(SPI_GETWORKAREA, 0, &g_rcWorkArea, 0))
        {
            SetRect(&g_rcWorkArea, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
        }

        // Create a Fullscreen window which is maximized.
        hwnd = CreateWindowEx(
                              0,
                              tszWindowClass,
                              _T("DDFUNC_MAX_FS_WND"),
                              WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE|WS_SYSMENU,
                              g_rcWorkArea.left,
                              g_rcWorkArea.top,
                              g_rcWorkArea.right - g_rcWorkArea.left,
                              g_rcWorkArea.bottom - g_rcWorkArea.top,
                              NULL,
                              NULL,
                              (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance,
                              NULL
                             );
        if(NULL == hwnd)
        {
            dbgout << "Unable to create a Full Screen Window." << endl;
            return GetLastError();
        }
        else
        {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            if (!UpdateWindow(hwnd))
            {
                dbgout << "Unable to update the Full Screen Window." << endl;
                return GetLastError();
            }
            MessagePump(hwnd);

            rghwnd.push_back(hwnd);
        }

        // Create a popup window with size screenWidth/2 x screenHeight/2.
        hwnd = CreateWindowEx(
                              0,
                              tszWindowClass,
                              _T("DDFUNC_POPUP_WND"),
                              WS_POPUP,
                              0,
                              0,
                              (g_rcWorkArea.right - g_rcWorkArea.left)/2,
                              (g_rcWorkArea.bottom - g_rcWorkArea.top)/2,
                              NULL,
                              NULL,
                              (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance,
                              NULL
                             );
        if(NULL == hwnd)
        {
            dbgout << "Unable to create a (Full Screen/2) Window." << endl;
            return GetLastError();
        }
        else
        {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            if (!UpdateWindow(hwnd))
            {
                dbgout << "Unable to update the (Full Screen/2) Window." << endl;
                return GetLastError();
            }
            MessagePump(hwnd);

            rghwnd.push_back(hwnd);
        }

        // Create a 1x1 window.
        hwnd = CreateWindowEx(
                              0,
                              tszWindowClass,
                              _T("DDFUNC_1X1_WND"),
                              WS_POPUP,
                              0,
                              0,
                              1,
                              1,
                              NULL,
                              NULL,
                              (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance,
                              NULL
                             );
        if(NULL == hwnd)
        {
            dbgout << "Unable to create a 1X1 Window." << endl;
            return GetLastError();        }
        else
        {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            if (!UpdateWindow(hwnd))
            {
                dbgout << "Unable to update the 1X1 Window." << endl;
                return GetLastError();
            }
            MessagePump(hwnd);

            rghwnd.push_back(hwnd);
        }

        // Create an overlapped window ScreenWidth - 1 X ScreenHeight -1.
        hwnd = CreateWindowEx(
                              0,
                              tszWindowClass,
                              _T("DDFUNC_OVERLAPPED_WND"),
                              WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE|WS_SYSMENU,
                              g_rcWorkArea.left,
                              g_rcWorkArea.top,
                              (g_rcWorkArea.right - g_rcWorkArea.left - 1),
                              (g_rcWorkArea.bottom - g_rcWorkArea.top - 1),
                              NULL,
                              NULL,
                              (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance,
                              NULL
                             );
        if(NULL == hwnd)
        {
            dbgout << "Unable to create a (Full Screen-1) Window." << endl;
            return GetLastError();
        }
        else
        {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            if (!UpdateWindow(hwnd))
            {
                dbgout << "Unable to update the (Full Screen-1) Window." << endl;
                return GetLastError();
            }
            MessagePump(hwnd);

            rghwnd.push_back(hwnd);
        }

        // Create an overlapped window with the WS_EX_LAYOUTRTL style.
        hwnd = CreateWindowEx(
                              WS_EX_LAYOUTRTL,
                              tszWindowClass,
                              _T("DDFUNC_OVERLAPPEDRTL_WND"),
                              WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE|WS_SYSMENU,
                              g_rcWorkArea.left,
                              g_rcWorkArea.top,
                              (g_rcWorkArea.right - g_rcWorkArea.left - 1),
                              (g_rcWorkArea.bottom - g_rcWorkArea.top - 1),
                              NULL,
                              NULL,
                              (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance,
                              NULL
                             );
        if(NULL == hwnd)
        {
            dbgout << "Unable to create a WS_EX_LAYOUTRTL Window." << endl;
            return GetLastError();
        }
        else
        {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            if (!UpdateWindow(hwnd))
            {
                dbgout << "Unable to update the WS_EX_LAYOUTRTL Window." << endl;
                return GetLastError();
            }
            MessagePump(hwnd);

            rghwnd.push_back(hwnd);
        }

        // Create a Fullscreen window which is minimized.
        hwnd = CreateWindowEx(
                              0,
                              tszWindowClass,
                              _T("DDFUNC_MIN_FS_WND"),
                              WS_OVERLAPPED|WS_CAPTION|WS_BORDER|WS_VISIBLE|WS_SYSMENU,
                              g_rcWorkArea.left,
                              g_rcWorkArea.top,
                              g_rcWorkArea.right - g_rcWorkArea.left,
                              g_rcWorkArea.bottom - g_rcWorkArea.top,
                              NULL,
                              NULL,
                              (HINSTANCE) CWindowsModule::GetWinModule().m_hInstance,
                              NULL
                             );
        if(NULL == hwnd)
        {
            dbgout(LOG_FAIL) << "Unable to create a Full Screen Minimized Window." << endl;
            return GetLastError();
        }
        else
        {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            ShowWindow(hwnd, SW_HIDE);
            SetForegroundWindow(hwnd);
            if (!UpdateWindow(hwnd))
            {
                dbgout(LOG_FAIL) << "Unable to update the Full Screen Minimized Window." << endl;
                return GetLastError();
            }
            MessagePump(hwnd);

            rghwnd.push_back(hwnd);
        }

        return hr;
    }

     ////////////////////////////////////////////////////////////////////////////////
     // IsClipped
     // This function tests the coordinates compared to the clipping rectangle.
     //
    bool IsClipped(DWORD x, DWORD y, const RECT& rcClipRect)
    {
        // check the bounding rectangle, if outside then we're clipped.
        if(x<=(DWORD)rcClipRect.left || x>=(DWORD)rcClipRect.right || y<=(DWORD)rcClipRect.top || y>=(DWORD)rcClipRect.bottom)
        {
               return true;
        }

        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // VerifyColorFill
    // steps through the surface to verify that the rcRegionToVerify is filled with
    // dwfillcolor.
    // Copied from TestKit\DDrawTK\ddrawuty_verify.cpp
    eTestResult VerifyColorFill(DWORD dwfillcolor, IDirectDrawSurface* pidds)
    {
        using namespace DDrawUty::Surface_Helpers;

        HRESULT hr = S_OK;
        CDDSurfaceDesc ddsd;
        BYTE *dwSrc=NULL;
        BYTE *wSrc=NULL;
        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect;

        dbgout << "Verifying color fill" << endl;

        hr = pidds->GetSurfaceDesc(&ddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        DWORD dwHeight = ddsd.dwHeight, dwWidth = ddsd.dwWidth;

        if((ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)) // Primary in Windowed mode
        {
            eTestResult tr = GetClippingRectangle(pidds, rcBoundRect); // A clipper will always be set on a Windowed primary.
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
            else
            {
                prcBoundRect = &rcBoundRect;

                dwWidth = rcBoundRect.right - rcBoundRect.left;
                dwHeight = rcBoundRect.bottom - rcBoundRect.top;
            }
        }

        if(FAILED(pidds->Lock(prcBoundRect, &ddsd, DDLOCK_WAITNOTBUSY, NULL)))
        {
            dbgout(LOG_FAIL) << "Failure locking Destination surface for verification" << endl;
            return trFail;
        }

        __try
        {
            if(ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC)
            {
                dwSrc=(BYTE*)ddsd.lpSurface;
                for(int y=0;y<(int)dwHeight;y++)
                {
                    for(int x=0;x<(int)dwWidth;x++)
                    {
                        WORD wCurrentFill = (dwfillcolor >> (16 * (x%2))) & 0xffff;
                        // If the FourCC format is smaller than 16 BPP, accessing a word at a time could lead to data misalignment errors
                        if(memcmp((dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch), &wCurrentFill, sizeof(wCurrentFill)) && !IsClipped(x, y, rcBoundRect))
                            {
                            pidds->Unlock(prcBoundRect);
                            dbgout << "Color fill FAILED, color at dest " << HEX(((WORD*)(dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0]) << " expected color " << HEX(wCurrentFill) << endl;
                            dbgout << "location (x,y) " << x << " " << y << endl;
                            dbgout << "FAILED" << endl;
                            return trFail;
                            }
                    }
                }
            }
            else if(ddsd.ddpfPixelFormat.dwRGBBitCount == 16)
            {
                wSrc=(BYTE*)ddsd.lpSurface;
                for(int y=0;y<(int)dwHeight;y++)
                {
                    for(int x=0;x<(int)dwWidth;x++)
                    {
                        if((((WORD*)(wSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0] != (WORD)dwfillcolor) && !IsClipped(x, y, rcBoundRect))
                            {
                            pidds->Unlock(prcBoundRect);
                            dbgout << "Color fill FAILED, color at dest " << HEX(((WORD*)(wSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0]) << " expected color " << HEX((WORD)dwfillcolor) << endl;
                            dbgout << "location (x,y) " << x << " " << y << endl;
                            dbgout << "FAILED" << endl;
                            return trFail;
                            }
                    }
                }
            }
            else if(ddsd.ddpfPixelFormat.dwRGBBitCount == 32)
            {
                dwSrc=(BYTE*)ddsd.lpSurface;
                for(int y=0;y<(int)dwHeight;y++)
                {
                    for(int x=0;x<(int)dwWidth;x++)
                    {
                        if((((DWORD*)(dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0] != dwfillcolor) && !IsClipped(x, y, rcBoundRect))
                            {
                            pidds->Unlock(prcBoundRect);
                            dbgout << "Color fill FAILED, color at dest " << HEX(((DWORD*)(dwSrc + y * ddsd.lPitch + x * ddsd.lXPitch))[0]) << " expected color " << HEX(dwfillcolor) << endl;
                            dbgout << "location (x,y) " << x << " " << y << endl;
                            dbgout << "FAILED" << endl;
                            return trFail;
                            }
                    }
                }
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            dbgout(LOG_FAIL)
                << "Exception while trying to access surface memory."
                << endl;
            pidds->Unlock(prcBoundRect);
            return trFail;
        }

        dbgout << "Color fill successful." << endl;
        pidds->Unlock(prcBoundRect);
        return trPass;
    }

    // Copied & modified from TestKit\DDrawTK\Iddsurfacetests.cpp
    eTestResult SetColorVal(double dRed, double dGreen, double dBlue, IDirectDrawSurface *piDDS, DWORD *dwFillColor)
    {
        using namespace DDrawUty::Surface_Helpers;

        CDDSurfaceDesc cddsd;
        HRESULT hr;
        DWORD *dwSrc=NULL;
        CDDBltFX cddbltfx;
        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect;

        hr = piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        if((cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)) // Primary in Windowed mode
        {
            eTestResult tr = GetClippingRectangle(piDDS, rcBoundRect); // A clipper will always be set on a Windowed primary.
            if(trPass != tr)
            {
                dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                return tr;
            }
            else
            {
                prcBoundRect = &rcBoundRect;
            }
        }

        // Lock and Unlock rect to get the Window's surface desc.
        hr = piDDS->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
        CheckHRESULT(hr, "Locking Destination surface for verification", trFail);

        hr = piDDS->Unlock(prcBoundRect);
        CheckHRESULT(hr, "Unlocking Destination surface", trFail);

        if(cddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            *dwFillColor=cddbltfx.dwFillColor=((DWORD)(cddsd.ddpfPixelFormat.dwRBitMask * dRed) & cddsd.ddpfPixelFormat.dwRBitMask)+
                                              ((DWORD)(cddsd.ddpfPixelFormat.dwGBitMask * dGreen) & cddsd.ddpfPixelFormat.dwGBitMask)+
                                              ((DWORD)(cddsd.ddpfPixelFormat.dwBBitMask * dBlue) & cddsd.ddpfPixelFormat.dwBBitMask);
            dbgout << "Filling with " << HEX(cddbltfx.dwFillColor) << endl;
            hr = piDDS->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &cddbltfx);
            if (DDERR_UNSUPPORTED == hr)
            {
                dbgout << "Skipping: Blting is unsupported with this surface type." << endl;
                return trPass;
            }
            else CheckHRESULT(hr, "Blt with colorfill and wait", trFail);
        }
        else
        {
            dbgout << "Unknown pixel format, ddpf.dwflags is " << cddsd.ddpfPixelFormat.dwFlags << endl;
            return trFail;
        }

        return trPass;
    }

    eTestResult PerformTests(IDirectDrawSurface* piddsWindowedPrimary, IDirectDrawSurface* piddsSurface, IDirectDraw* pidd, HWND hwnd)
    {
         using namespace DDrawUty::Surface_Helpers;

         HRESULT hr = S_OK;
         eTestResult tr = trPass;

         if(!piddsWindowedPrimary || !piddsSurface)
         {
             dbgout(LOG_ABORT) << "Inavlid Params passed to PerformTests" << endl;
             return trAbort;
         }
         // Blt from/to a source surface should succeed.
         hr = piddsWindowedPrimary->Blt(NULL, piddsSurface, NULL, DDBLT_WAITNOTBUSY, NULL);
         if(S_OK != hr)
         {
             dbgout(LOG_FAIL) << "Blt to Windowed Primary failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                              << endl;
             tr = trFail;
         }

         hr = piddsSurface->Blt(NULL, piddsWindowedPrimary, NULL, DDBLT_WAITNOTBUSY, NULL);
         if(S_OK != hr)
         {
             dbgout(LOG_FAIL) << "Blt from Windowed Primary failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                              << endl;
             tr = trFail;
         }

         // AlphaBlt from/to a source surface should succeed.
         hr = piddsWindowedPrimary->AlphaBlt(NULL, piddsSurface, NULL, DDBLT_WAITNOTBUSY, NULL);
         if(S_OK != hr)
         {
             dbgout(LOG_FAIL) << "AlphaBlt to Windowed Primary failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                              << endl;
             tr = trFail;
         }

         hr = piddsSurface->AlphaBlt(NULL, piddsWindowedPrimary, NULL, DDBLT_WAITNOTBUSY, NULL);
         if(S_OK != hr)
         {
             dbgout(LOG_FAIL) << "AlphaBlt from Windowed Primary failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                              << endl;
             tr = trFail;
         }

         // Get the window's bounding rectangle
         RECT rcWindowRect;
         if(!GetWindowRect(hwnd, &rcWindowRect))
         {
             dbgout(LOG_ABORT) << "Unable to retrieve the window's bounding rectangle." << endl;
             return trAbort;
         }

         // Fill the surface with red color
         DWORD dwFillColor = 0;
         tr |= SetColorVal(1, 0, 0, piddsWindowedPrimary, &dwFillColor);

         // Now lock the surface and verify each pixel's color in the Primary's client rectangle.
         tr |= VerifyColorFill(dwFillColor, piddsWindowedPrimary);
         if(trPass != tr)
         {
              dbgout(LOG_FAIL) << "Failed filling the Primary."
                               << endl;
              return tr;
         }

         // Lock the primary
         DDSURFACEDESC ddsd;
         memset(&ddsd, 0, sizeof(ddsd));
         ddsd.dwSize = sizeof(ddsd);

         // Get the Windowed Primary's Clipping rect
         RECT rcClipRect;
         tr = GetClippingRectangle(piddsWindowedPrimary, rcClipRect);
         if(trPass != tr)
         {
            dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
            return tr;
         }

         // Check the number of ticks taken by Lock
         DWORD dwStartTick = GetTickCount();

         dbgout << "Locking " << rcClipRect << " on the Primary" << endl;

         hr = piddsWindowedPrimary->Lock(&rcClipRect, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
         if(S_OK != hr)
         {
             dbgout(LOG_FAIL) << "Lock on the Windowed Primary failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                              << endl;
             return trFail;
         }
         DWORD dwEndTick = GetTickCount();
         if((dwEndTick - dwStartTick) > 1)
         {
             dbgout << "WARNING : Time taken by Lock (" << (dwEndTick - dwStartTick) << ") exceeds max limit (1ms)"
                    << endl;
         }

         // Verify that the surface description is correct, when it is compositor enabled case,
         // the size returned by Lock() should be window size, otherwise it should be screen size
         if (g_DDConfig.IsCompositorEnabled())   //compositor enabled
         {
             if( (ddsd.dwWidth != (DWORD)(rcWindowRect.right - rcWindowRect.left))
                  ||
                 (ddsd.dwHeight != (DWORD)(rcWindowRect.bottom - rcWindowRect.top))
               )
             {
                 dbgout(LOG_FAIL) << "The height and width returned in the surface desc. does not match the locked rectangle."
                                  << endl
                                  << "Expected : " << (rcWindowRect.right - rcWindowRect.left) << " X " << (rcWindowRect.bottom - rcWindowRect.top)
                                  << endl
                                  << "Actual : " << ddsd.dwWidth << " X " << ddsd.dwHeight
                                  << endl;
                 tr = trFail;
             }
         }
         else   // compositor is not enabled,
         {
             // get screen size by calling GetDisplayMode()
             DDSURFACEDESC ddsd_displayMode;
             memset(&ddsd_displayMode, 0, sizeof(ddsd_displayMode));
             ddsd_displayMode.dwSize = sizeof(DDSURFACEDESC);
             hr = pidd->GetDisplayMode(&ddsd_displayMode);
             if(S_OK != hr)
             {
                 dbgout(LOG_FAIL) << "GetDisplayMode failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                                  << endl;
                 tr = trFail;
             }
             else
             {
                 if((ddsd.dwWidth != ddsd_displayMode.dwWidth) || (ddsd.dwHeight != ddsd_displayMode.dwHeight))
                 {
                     dbgout(LOG_FAIL) << "The height and width returned in the surface desc. does not match the screen size."
                                      << endl
                                      << "Expected : " << ddsd_displayMode.dwWidth << " X " << ddsd_displayMode.dwHeight
                                      << endl
                                      << "Actual : " << ddsd.dwWidth << " X " << ddsd.dwHeight
                                      << endl;
                     tr = trFail;
                 }
             }
         }

         hr = piddsWindowedPrimary->Unlock(&rcClipRect);
         if(S_OK != hr)
         {
             dbgout(LOG_FAIL) << "Unlock on the Windowed Primary failed with error " << g_ErrorMap[hr] << "(" << HEX((DWORD)hr) << ")"
                              << endl;
             tr = trFail;
         }

         return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_SetHWnd::Test
    // Tests the behavior of Primary surfaces in the Windowed mode by setting
    // different Windows.
    ////////////////////////////////////////////////////////////////////////////////
    eTestResult CTest_SetHWnd::TestIDirectDrawSurface()
    {
        using namespace priv_CTest_SetHWnd;
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT hr = S_OK;
        vector<HWND> rghwnd;
        RECT g_rcWorkArea;
        CComPointer<IDirectDrawSurface> piddsPrimary = g_DDSPrimary.GetObject();
        if(NULL == piddsPrimary)
        {
            dbgout << "Primary Surface could not be created. Aborting test" << endl;
            tr = trAbort;
            goto LPExit;
        }
        // Get the work area rectangle.
        if(!SystemParametersInfo(SPI_GETWORKAREA, 0, &g_rcWorkArea, 0))
        {
            SetRect(&g_rcWorkArea, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
        }

        // Create the Windows and retrieve the HWNDs
        hr = CreateWindows(rghwnd);
        if(S_OK != hr)
        {
            if(rghwnd.size() == 0)
            {
                dbgout(LOG_ABORT) << "Unable to create any of the windows (Error : " << HEX((DWORD)hr) << "). Abort"
                                  << endl;
            }
            else
            {
                dbgout << "Unable to create all the windows (Error : " << HEX((DWORD)hr) << "). Continue testing with the created ones."
                       << endl;
            }
        }

        // Set the DirectDraw Window as the non-top most, before executing the test on other windows.
        if(!SetWindowPos(CWindowsModule::GetWinModule().m_hWnd, HWND_NOTOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE))
        {
            dbgout(LOG_ABORT) << "Unable to set the Window as non-top most. GetLastError() = "
                              << HEX((DWORD)GetLastError()) << endl;
            tr = trAbort;
            goto LPExit;
        }

        // Iterate over the available windows.
        for(DWORD dwNextWnd=0; dwNextWnd<rghwnd.size() && (trPass == tr); ++dwNextWnd)
        {
             dbgout << "WINDOW #" << dwNextWnd << endl;
             dbgout << "========" << endl;

             // Set the Helper Window as the top most, before calling any DirectDraw APIs.
             // This ensures that no one else can deactivate it, while the test is executing.
             if(!SetWindowPos(rghwnd[dwNextWnd], HWND_TOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
             {
                 dbgout(LOG_ABORT) << "Unable to set the Window as top most. GetLastError() = "
                                   << HEX((DWORD)GetLastError()) << endl;
                 tr = trAbort;
                 goto LPExit;
             }

             tr = ClipToWindow(m_piDD.AsInParam(), piddsPrimary.AsInParam(), rghwnd[dwNextWnd]);
             if(trPass != tr)
             {
                 dbgout(LOG_FAIL) << "Unable to set the clipper to the hwnd. Continuing with the next window." << endl;
                 goto LPExit;
             }

             DWORD dwNumIterations = rand()%4 + 1; // try atleast once and 4 times max.
             while(dwNumIterations-- && (trPass == tr))
             {
                 // Try to resize the window randomly.
                 int width = 0, height = 0;
                 RECT rcWindowClient;
                 bool bIsEmptyClientArea=true;

                 // After resizing the window, check the client area to make sure it is not empty client area (which will cause empty clip list entry
                 // we will then keep resizing until we get a positive size client area
                 do
                 {
                     width = rand()%(g_rcWorkArea.right - g_rcWorkArea.left) + 1;
                     height = rand()%(g_rcWorkArea.bottom - g_rcWorkArea.top) + 1;

                     if(!MoveWindow(rghwnd[dwNextWnd], g_rcWorkArea.left, g_rcWorkArea.top, width, height, FALSE))
                     {

                         dbgout << "New Width = " << width << " New Height = " << height << endl;
                         dbgout << "Unable to resize the window (Error : " << HEX((DWORD)GetLastError()) << "). Continuing anyway.." << endl;
                     }

                     // Get the client Rect of current window
                     if (!GetClientRect(rghwnd[dwNextWnd], &rcWindowClient))
                     {
                        dbgout(LOG_FAIL) << "Unable to get the Window Client Area. GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                        goto LPExit;
                     }
                     // if either the width or height of the clientArea is less or eqaul to 0, set the flag to indicate the clientArea is empty and keep resizing
                     bIsEmptyClientArea = (rcWindowClient.right-rcWindowClient.left <=0) || (rcWindowClient.bottom-rcWindowClient.left <=0);

                 } while (bIsEmptyClientArea);

                 // Update the Window.
                 ShowWindow(rghwnd[dwNextWnd], SW_SHOWNORMAL);
                 if (!UpdateWindow(rghwnd[dwNextWnd]))
                 {
                     dbgout(LOG_FAIL) << "Unable to update the Window. GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                     goto LPExit;
                 }
                 MessagePump(rghwnd[dwNextWnd]);

                 // Perform tests on the surfaces.
                // tr = PerformTests(piddsPrimary.AsInParam(), m_piDDS.AsInParam(), m_piDD.AsInParam(), rghwnd[dwNextWnd]);
                 if(trFail == tr)
                 {
                     dbgout(LOG_FAIL) << "Window Tests Failed! GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                     break;
                 }
             }

             // Allow other windows to be the top most.
             if(!SetWindowPos(rghwnd[dwNextWnd], HWND_NOTOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
             {
                 dbgout(LOG_ABORT) << "Unable to set the Window as non-topmost. GetLastError() = "
                                   << HEX((DWORD)GetLastError()) << endl;
                 tr = trAbort;
                 goto LPExit;
             }
        }

        // Set the DirectDraw Window as the top most before performing negative tests.
        if(!SetWindowPos(CWindowsModule::GetWinModule().m_hWnd, HWND_TOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
        {
            dbgout(LOG_ABORT) << "Unable to set the Window as top most. GetLastError() = "
                              << HEX((DWORD)GetLastError()) << endl;
            tr = trAbort;
            goto LPExit;
        }

        if(trPass == tr)
        {
            // Perform negative tests on the primary surface.
            tr = PerformNegativeTests(m_piDD.AsInParam(), piddsPrimary.AsInParam(), m_piDDS.AsInParam());
            if(trFail == tr)
            {
                dbgout(LOG_FAIL) << "The Negative tests failed on the Primary surface." << endl;
                goto LPExit;
            }
        }

    LPExit:
        // Restore the DirectDraw Window as the top most before exit.
        if(!SetWindowPos(CWindowsModule::GetWinModule().m_hWnd, HWND_TOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
        {
            dbgout << "Unable to set the Window as top most. GetLastError() = "
                   << HEX((DWORD)GetLastError()) << endl;
            tr = trAbort;
        }

        // Restore the original window as the clipping rectangle.
        eTestResult trClip = trPass;
        trClip = ClipToWindow(m_piDD.AsInParam(), piddsPrimary.AsInParam(),
                              CWindowsModule::GetWinModule().m_hWnd
                             );
        if((tr == trPass) && (trClip != trPass))
        {
            tr = trClip;
        }

        // Destroy the created windows
        for(int i=0; i< (int)rghwnd.size(); ++i)
        {
            DestroyWindow(rghwnd[i]);
            MessagePump(rghwnd[i]);
        }

        rghwnd.clear();

        return tr;
    }

    eTestResult CTest_LockUnlockAfterWindowsOP::TestIDirectDrawSurface()
    {

        //Create Windows
        using namespace priv_CTest_SetHWnd;
        using namespace DDrawUty::Surface_Helpers;

        eTestResult tr = trPass;
        HRESULT hr = S_OK;
        vector<HWND> rghwnd;
        RECT g_rcWorkArea;

        // Get the surface description
        CDDSurfaceDesc cddsd;
        hr = m_piDDS->GetSurfaceDesc(&cddsd);
        CheckHRESULT(hr, "GetSurfaceDesc", trAbort);

        if (cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)  //only works for primary
        {
            CComPointer<IDirectDrawSurface> piddsPrimary = g_DDSPrimary.GetObject();
            if(NULL == piddsPrimary)
            {
                dbgout << "Primary Surface could not be created. Aborting test" << endl;
                tr = trAbort;
                goto LPExit;
            }

            // Get the work area rectangle.
            if(!SystemParametersInfo(SPI_GETWORKAREA, 0, &g_rcWorkArea, 0))
            {
                SetRect(&g_rcWorkArea, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
            }
            // Create the Windows and retrieve the HWNDs
            hr = CreateWindows(rghwnd);
            if(S_OK != hr)
            {
                if(rghwnd.size() == 0)
                {
                    dbgout(LOG_ABORT) << "Unable to create any of the windows (Error : " << HEX((DWORD)hr) << "). Abort"
                                      << endl;
                }
                else
                {
                    dbgout << "Unable to create all the windows (Error : " << HEX((DWORD)hr) << "). Continue testing with the created ones."
                           << endl;
                }
            }

            // Set the DirectDraw Window as the non-top most, before executing the test on other windows.
            if(!SetWindowPos(CWindowsModule::GetWinModule().m_hWnd, HWND_NOTOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE))
            {
                dbgout(LOG_ABORT) << "Unable to set the Window as non-top most. GetLastError() = "
                                  << HEX((DWORD)GetLastError()) << endl;
                tr = trAbort;
                goto LPExit;
            }

            // Iterate over the available windows.
            for(DWORD dwNextWnd=0; dwNextWnd<rghwnd.size() && (trPass == tr); ++dwNextWnd)
            {
                 dbgout << "WINDOW #" << dwNextWnd << endl;
                 dbgout << "========" << endl;

                 // Set the Helper Window as the top most, before calling any DirectDraw APIs.
                 // This ensures that no one else can deactivate it, while the test is executing.
                 if(!SetWindowPos(rghwnd[dwNextWnd], HWND_TOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
                 {
                     dbgout(LOG_ABORT) << "Unable to set the Window as top most. GetLastError() = "
                                       << HEX((DWORD)GetLastError()) << endl;
                     tr = trAbort;
                     goto LPExit;
                 }

                 tr = ClipToWindow(m_piDD.AsInParam(), piddsPrimary.AsInParam(), rghwnd[dwNextWnd]);
                 if(trPass != tr)
                 {
                     dbgout(LOG_FAIL) << "Unable to set the clipper to the hwnd. Continuing with the next window." << endl;
                     goto LPExit;
                 }

                 // Test lock, randomly moving/resizing then unlock
                 DWORD dwNumIterations = rand()%4 + 1; // try at least once and 4 times max.
                 while(dwNumIterations-- && (trPass == tr))
                 {
                    // Update the Window for correct lock.
                    ShowWindow(rghwnd[dwNextWnd], SW_SHOWNORMAL);
                    if (!UpdateWindow(rghwnd[dwNextWnd]))
                    {
                        dbgout(LOG_FAIL) << "Unable to update the Window. GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                        goto LPExit;
                    }
                    if (!SetForegroundWindow(rghwnd[dwNextWnd]))
                    {
                        dbgout(LOG_FAIL) << "Unable to set current Window as foreground window. GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                        goto LPExit;
                    }

                    MessagePump(rghwnd[dwNextWnd]);

                    // pass the window's bounding rectangle to Lock/Unlock calls.
                    LPRECT prcBoundRect = NULL;
                    RECT rcClipRect = {0, 0, 1, 1};


                    if(!(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
                    {
                        tr = GetClippingRectangle(m_piDDS.AsInParam(), rcClipRect);
                        if(trPass != tr)
                        {
                            dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                            goto LPExit;
                        }
                        prcBoundRect = &rcClipRect;
                    }

                    hr = piddsPrimary->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                    CheckHRESULT(hr, "Lock surface", trFail);
                    CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);
                    if ((hr==DD_OK)&&(cddsd.lpSurface!=NULL))
                    {
                        dbgout << "Successfully lock before moving/resizing Window #" << dwNextWnd << endl;
                    }

                    //get dimension information before destorying windows
                    DWORD dwHeight    = prcBoundRect ? prcBoundRect->bottom - prcBoundRect->top : cddsd.dwHeight;
                    DWORD dwWidth     = prcBoundRect ? prcBoundRect->right - prcBoundRect->left: cddsd.dwWidth;

                    // Try to resize the window randomly.
                    int width = 0, height = 0;
                    RECT rcWindowClient;
                    bool bIsEmptyClientArea=true;

                    // After resizing the window, check the client area to make sure it is not empty client area (which will cause empty clip list entry
                    // we will then keep resizing until we get a positive size client area
                    do
                    {
                        width = rand()%(g_rcWorkArea.right - g_rcWorkArea.left) + 1;
                        height = rand()%(g_rcWorkArea.bottom - g_rcWorkArea.top) + 1;

                        if(!MoveWindow(rghwnd[dwNextWnd], g_rcWorkArea.left, g_rcWorkArea.top, width, height, FALSE))
                        {

                            dbgout << "New Width = " << width << " New Height = " << height << endl;
                            dbgout << "Unable to resize the window (Error : " << HEX((DWORD)GetLastError()) << "). Continuing anyway.." << endl;
                        }

                        // Get the client Rect of current window
                        if (!GetClientRect(rghwnd[dwNextWnd], &rcWindowClient))
                        {
                            dbgout(LOG_FAIL) << "Unable to get the Window Client Area. GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                            goto LPExit;
                        }
                        // if either the width or height of the clientArea is less or eqaul to 0, set the flag to indicate the clientArea is empty and keep resizing
                        bIsEmptyClientArea = (rcWindowClient.right-rcWindowClient.left <=0) || (rcWindowClient.bottom-rcWindowClient.left <=0);

                    } while (bIsEmptyClientArea);

                    // Update the Window.
                    ShowWindow(rghwnd[dwNextWnd], SW_SHOWNORMAL);
                    if (!UpdateWindow(rghwnd[dwNextWnd]))
                    {
                        dbgout(LOG_FAIL) << "Unable to update the Window. GetLastError = " <<  HEX((DWORD)GetLastError()) << endl;
                        goto LPExit;
                    }
                    MessagePump(rghwnd[dwNextWnd]);

                    // Verify the pointer is still accessible
                    // Fill the surface
                    dbgout << "Filling entire surface data after moving/resizing Window #" << dwNextWnd << endl;
                    BYTE *pbRow, *pb;
                    DWORD dwWork;
                    DWORD dwMiss;

                    int i,j;

                    dwWork = 0;
                    pbRow = (BYTE*)cddsd.lpSurface;

                    for (i = 0; i < (int)dwHeight; i++)
                    {
                        pb = pbRow;
                        for (j = 0; j < (int)dwWidth; j++)
                            *(pb+j*cddsd.lXPitch) = (BYTE)dwWork++;
                        pbRow += cddsd.lPitch;
                    }

                    // Read the surface data
                    dbgout << "Reading entire surface data after moving/resizing Window #" << dwNextWnd << endl;
                    dwWork = 0;
                    dwMiss = 0;
                    pbRow = (BYTE*)cddsd.lpSurface;
                    bool verifyResult = true;
                    for (i = 0; i < (int)dwHeight; i++)
                    {
                        pb = pbRow;
                        for (j = 0; j < (int)dwWidth; j++)
                        {
                            if (*(pb+j*cddsd.lXPitch) != (BYTE)dwWork++)
                            {
                                dwMiss++;
                            }
                        }
                        pbRow += cddsd.lPitch;
                    }

                    if (dwMiss>0)
                    {
                        dbgout << "Data different than expected at " << dwMiss << " pixels, which is expeceted and will not fail the test" << endl;
                    }
                    else
                    {
                        dbgout << "Data verification passed" << endl;
                    }

                    //now unlock after moving/resizing windows, should always pass
                    hr = piddsPrimary->Unlock(prcBoundRect);
                    CheckHRESULT(hr, "Unlock", trFail);
                    if (hr==DD_OK)
                    {
                        dbgout << "Successfully unlock after moving/resizing Window #" << dwNextWnd << endl;
                    }

                 }

                LPRECT prcBoundRect = NULL;
                RECT rcClipRect = {0, 0, 1, 1};

                //since the location and size changes, we need to get clipping rectangle again
                if(!(DDSCL_FULLSCREEN & g_DDConfig.CooperativeLevel()))
                {
                    tr = GetClippingRectangle(m_piDDS.AsInParam(), rcClipRect);
                    if(trPass != tr)
                    {
                        dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                        goto LPExit;
                    }
                    prcBoundRect = &rcClipRect;
                }

                //lock before destorying the window
                hr = piddsPrimary->Lock(prcBoundRect, &cddsd, DDLOCK_WAITNOTBUSY, NULL);
                CheckHRESULT(hr, "Lock surface", trFail);
                CheckCondition(NULL == cddsd.lpSurface, "Lock didn't fill lpSurface pointer", trFail);
                if ((hr==DD_OK)&&(cddsd.lpSurface!=NULL))
                {
                    dbgout << "Successfully lock before destorying Window #" << dwNextWnd << endl;
                }
                //get dimension information before destorying windows
                DWORD dwHeight    = prcBoundRect ? prcBoundRect->bottom - prcBoundRect->top : cddsd.dwHeight;
                DWORD dwWidth     = prcBoundRect ? prcBoundRect->right - prcBoundRect->left: cddsd.dwWidth;

                DestroyWindow(rghwnd[dwNextWnd]);
                MessagePump(rghwnd[dwNextWnd]);


                // Verify the pointer is still accessible
                // Fill the surface
                dbgout << "Filling entire surface data after destorying Window #" << dwNextWnd << endl;
                BYTE *pbRow, *pb;
                DWORD dwWork;
                DWORD dwMiss;

                int i,j;

                dwWork = 0;
                pbRow = (BYTE*)cddsd.lpSurface;

                for (i = 0; i < (int)dwHeight; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwWidth; j++)
                        *(pb+j*cddsd.lXPitch) = (BYTE)dwWork++;
                    pbRow += cddsd.lPitch;
                }

                // Read the surface data
                dbgout << "Reading entire surface data after destorying Window #" << dwNextWnd << endl;
                dwWork = 0;
                dwMiss = 0;
                pbRow = (BYTE*)cddsd.lpSurface;
                bool verifyResult = true;
                for (i = 0; i < (int)dwHeight; i++)
                {
                    pb = pbRow;
                    for (j = 0; j < (int)dwWidth; j++)
                    {
                        if (*(pb+j*cddsd.lXPitch) != (BYTE)dwWork++)
                        {
                            dwMiss++;
                        }
                    }
                    pbRow += cddsd.lPitch;
                }
                if (dwMiss>0)
                {
                    dbgout << "Data different than expected at " << dwMiss << " pixels, which is expeceted and will not fail the test" << endl;
                }
                else
                {
                    dbgout << "Data verification passed" << endl;
                }

                //now unlock after destroying window, should always pass
                hr = piddsPrimary->Unlock(prcBoundRect);
                CheckHRESULT(hr, "Unlock", trFail);
                if (hr==DD_OK)
                {
                    dbgout << "Succesfully unlock after destorying Window #" << dwNextWnd << endl;
                }
            }
            LPExit:
            // Restore the DirectDraw Window as the top most before exit.
            if(!SetWindowPos(CWindowsModule::GetWinModule().m_hWnd, HWND_TOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
            {
                dbgout << "Unable to set the Window as top most. GetLastError() = "
                       << HEX((DWORD)GetLastError()) << endl;
                tr = trAbort;
            }

            // Restore the original window as the clipping rectangle.
            eTestResult trClip = trPass;
            trClip = ClipToWindow(m_piDD.AsInParam(), piddsPrimary.AsInParam(),
                                  CWindowsModule::GetWinModule().m_hWnd);

            if((tr == trPass) && (trClip != trPass))
            {
                tr = trClip;
            }

            rghwnd.clear();
        }
        return tr;
    }
};


