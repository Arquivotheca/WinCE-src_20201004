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
//

#include "main.h"

#include "DDrawUty.h"
#include "DDTestKit_Iterators.h"
#include <vector>

using namespace std;
using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace TestKit_Surface_Iterators
{
    ////////////////////////////////////////////////////////////////////////////////
    // CIterateSurfaces::TestIDirectDraw
    //
    //
    eTestResult CIterateTKSurfaces::TestIDirectDraw()
    {
        eTestResult tr = trPass;
        vectSurfPixPair_itr itr;
        TCHAR szDescSurf[128];
        HRESULT hr;

        // Call the Pre-Test configuration method
        tr |= PreSurfaceTest();
        if (trPass != tr)
            return tr;

        // Pick up the SurfCategories from the string table, if defined.
        g_DDConfig.GetStringValue(DDrawUty::wstring(TEXT("Surfaces")), &m_dwSurfCategories);

        // Handle the case that the test wants to be called with a
        // NULL surface pointer.  This is usually used by the test
        // to create and test unusual surfaces, such as large flipping
        // chains.
        if (m_dwSurfCategories & scNull)
        {
            m_piDDS = NULL;
            dbgout << "Testing Surface : NULL" << indent;
            tr |= RunSurfaceTest();
            dbgout << unindent << "Done" << endl;
        }

        // Iterate all desired surfaces
        for (itr = m_vectPairs.begin(); itr != m_vectPairs.end(); itr++)
        {
            hr = g_DDConfig.GetSurfaceDesc((*itr).first, (*itr).second, m_cddsd, szDescSurf, countof(szDescSurf));
            // Skip any unsupported surface types
            if (S_OK != hr)
            {
                dbgout << "Surface Unsupported" << endl;
                continue;
            }

            // See if this is a primary surface
            if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                // See if the test wants to be called for primary surfaces
                if ((m_dwSurfCategories & scPrimary) || (m_dwSurfCategories & scBackbuffer))
                {
                    dbgout << "Testing Source Surface : " << szDescSurf << indent;

                    // Handle this as a primary surface
                    g_DDSPrimary.m_cddsd = m_cddsd;
                    m_piDDS = g_DDSPrimary.GetObject();

                    if (m_piDDS != NULL)
                    {
                        // Get the actual surface description
                        m_piDDS->GetSurfaceDesc(&m_cddsd);

                        // Run the test on the primary, if requested
                        if (m_dwSurfCategories & scPrimary)
                        {
                            dbgout << "Testing Source Surface : Primary" << indent;
                            tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDS.AsInParam());
                            if(tr==trPass)
                            {
                                tr |= RunSurfaceTest();
                            }
                            m_piDDS.ReleaseInterface();
                            dbgout << unindent << "Done" << endl;
                        }

                        // If there is a backbuffer and the test wants to
                        // be called for backbuffers
                        if ((m_cddsd.dwBackBufferCount > 0) && (m_dwSurfCategories & scBackbuffer))
                        {
                            dbgout << "Testing Source Surface : BackBuffer 1" << indent;
                            m_piDDS = g_DDSBackbuffer.GetObject();

                            if (m_piDDS != NULL)
                            {
                                m_piDDS->GetSurfaceDesc(&m_cddsd);
                                tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDS.AsInParam());
                                if(!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP))
                                {
                                    dbgout << "Creation of a surface with BackBuffers succeded, but flipping unsupported" << endl;
                                    tr |= trFail;
                                }
                                if(tr==trPass)
                                {
                                    tr |= RunSurfaceTest();
                                }
                                m_piDDS.ReleaseInterface();
                            }
                            else
                                tr |= trAbort;
                            g_DDSBackbuffer.ReleaseObject();
                            dbgout << unindent << "Done" << endl;
                        }
                    }
                    else if((m_cddsd.dwBackBufferCount > 0) && ((g_DDSPrimary.GetLastError() == DDERR_OUTOFMEMORY) || (g_DDSPrimary.GetLastError() == DDERR_OUTOFVIDEOMEMORY)))
                    {
                        // if there is a back buffer and an out of memory error, skip test
                        dbgout << "Out of memory creating primary surface with back buffer(s)" << indent;
                    }
                    else if((DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() || DDERR_INVALIDCAPS == g_DDSPrimary.GetLastError()) &&
                        (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                        (!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) ||
                        !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
                    {
                        dbgout << "Flipping unsupported, unable to create surface with backBuffers";
                    }
                    else
                        tr |= trAbort;

                    g_DDSPrimary.ReleaseObject();
                    dbgout << unindent << "Done" << endl;
                }
            }
            else
            {
                // See if the surface matches the test's desired criteria
                if (((m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC) && (m_dwSurfCategories & scOffScrSys)) ||
                    (((m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)) && (m_dwSurfCategories & scOffScrSysOwnDc)) ||
                    ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)  && (m_dwSurfCategories & scOffScrVid)) ||
                    ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY)      && (m_dwSurfCategories & scOverlay)))
                {
                    dbgout << "Testing Source Surface : " << szDescSurf << indent;

                    bool fRetrySmaller;
                    do
                    {
                        fRetrySmaller = false;
                        // Handle this as a simple surface
                        hr = m_piDD->CreateSurface(&m_cddsd, m_piDDS.AsTestedOutParam(), NULL);
                        if (DDERR_GENERIC == hr)
                        {
                            dbgout << "Surface not supported" << endl;
                        } else
                        if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                        {
                            tr |= OutOfMemory_Helpers::VerifyOOMCondition(hr, m_piDD.AsInParam(), m_cddsd);
                        }
                        else if ((DDERR_NOOVERLAYHW == hr || DDERR_INVALIDCAPS == hr) &&
                                 ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
                                  !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_OVERLAY)))
                        {
                            // If we got a no overlay hardware error, the test still passes as
                            // long as either the surface is in system memory or if the HAL
                            // doesn't support overlays.
                            dbgout << "Overlay not supported ... Skipping" << endl;
                        }
                        else if ((DDERR_INVALIDPIXELFORMAT == hr || DDERR_UNSUPPORTEDFORMAT == hr) && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                        {
                            // Unsupported formats are ok for overlay surfaces, since the
                            // support is driver dependent.
                            dbgout << "Overlay of this pixel format not supported, please verify that this should not be supported. Skipping" << endl;
                        }
                        else if ((DDERR_UNSUPPORTEDFORMAT == hr) &&
                            !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY) &&
                            (m_cddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC))
                        {
                            // FourCC formats can be only supported for overlays
                            dbgout << "FourCC format not supported on non-overlay surface ... Skipping" << endl;
                        }
                        else if ((DDERR_TOOBIGHEIGHT == hr || DDERR_TOOBIGWIDTH == hr || DDERR_TOOBIGSIZE == hr) &&
                            !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
                        {
                            dbgout << "Surface too large (received " << hr << "). Will attempt smaller surface" << endl;
                            m_cddsd.dwWidth = (m_cddsd.dwWidth * 3 / 4) & (~0x3f);
                            m_cddsd.dwHeight = (m_cddsd.dwHeight * 3 / 4) & (~0x3f);
                            if (0 == m_cddsd.dwWidth || 0 == m_cddsd.dwHeight)
                            {
                                dbgout(LOG_FAIL) << "Unable to find small enough surface." << endl;
                                tr |= trFail;
                            }
                            else
                            {
                                dbgout << "Trying with surface of size " << m_cddsd.dwWidth << "x" << m_cddsd.dwHeight << endl;
                                fRetrySmaller = true;
                            }
                        }
                        else if (FAILED(hr))
                        {
                            QueryHRESULT(hr, "CreateSurface", tr |= trFail);
                            dbgout(LOG_FAIL)<< "NOTE: DirectDraw driver must now handle creation of system memory surfaces" << endl;
                        }
                        else QueryCondition(m_piDDS.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", tr |= trFail)
                        else
                        {
                            tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDS.AsInParam());
                            if(tr==trPass)
                            {
                                // Run the test on the surface
                                tr |= RunSurfaceTest();
                            }
                            m_piDDS.ReleaseInterface();
                        }
                    } while (fRetrySmaller);
                    dbgout << unindent << "Done" << endl;
                }
            }
        }

        // Call the post-test method
        tr |= PostSurfaceTest();

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CIterateSurfaces_TWO::TestIDirectDrawSurface
    //
    //
    eTestResult CIterateTKSurfaces_TWO::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;
        vectSurfPixPair_itr itr;
        TCHAR szDescSurf[128];
        HRESULT hr;

        for (itr = m_vectPairsDst.begin(); itr != m_vectPairsDst.end(); itr++)
        {
            hr = g_DDConfig.GetSurfaceDesc((*itr).first, (*itr).second, m_cddsdDst, szDescSurf, countof(szDescSurf));
            // Skip any unsupported surface types
            if (S_OK != hr)
            {
                dbgout << "Surface Unsupported" << endl;
                continue;
            }

            // If our current destination is a primary
            if (m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            {
                // If the source is not a primary and not a backbuffer, we can
                // create primary surfaces as the destination, if the
                // test has requested it
                if (!(m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
                    !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) &&
                    ((m_dwSurfCategoriesDst & scPrimary) || (m_dwSurfCategoriesDst & scBackbuffer)))
                {
                    dbgout << "Testing Destination Surface : " << szDescSurf << indent;

                    g_DDSPrimary.m_cddsd = m_cddsdDst;
                    m_piDDSDst = g_DDSPrimary.GetObject();

                    if (m_piDDSDst != NULL)
                    {
                        // Get the actual surface description
                        m_piDDSDst->GetSurfaceDesc(&m_cddsdDst);

                        // Run the test on the surface, if requested
                        if (m_dwSurfCategoriesDst & scPrimary)
                        {
                            dbgout << "Testing Destination Surface : Primary" << indent;
                            tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDSDst.AsInParam());
                            if(tr==trPass)
                            {
                                tr |= RunSurfaceTest_TWO();
                            }
                            m_piDDSDst.ReleaseInterface();
                            dbgout << unindent << "Done" << endl;
                        }

                        // If there is a backbuffer and the test is to be run
                        // on the backbuffer
                        if ((m_cddsdDst.dwBackBufferCount > 0) && (m_dwSurfCategoriesDst & scBackbuffer))
                        {
                            dbgout << "Testing Destination Surface : BackBuffer 1" << indent;
                            m_piDDSDst = g_DDSBackbuffer.GetObject();
                            if (m_piDDSDst != NULL)
                            {
                                m_piDDSDst->GetSurfaceDesc(&m_cddsdDst);
                                tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDSDst.AsInParam());

                                if(!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP))
                                {
                                    dbgout << "Creation of a surface with BackBuffers succeded, but flipping unsupported" << endl;
                                    tr |= trFail;
                                }

                                if(tr==trPass)
                                {
                                    tr |= RunSurfaceTest_TWO();
                                }
                                m_piDDSDst.ReleaseInterface();
                            }
                            else
                                tr |= trAbort;
                            g_DDSBackbuffer.ReleaseObject();
                            dbgout << unindent << "Done" << endl;
                        }
                    }
                    else if((m_cddsd.dwBackBufferCount > 0) && ((g_DDSPrimary.GetLastError() == DDERR_OUTOFMEMORY) || (g_DDSPrimary.GetLastError() == DDERR_OUTOFVIDEOMEMORY)))
                    {
                        // if there is a back buffer and an out of memory error, skip test
                        dbgout << "Out of memory creating primary surface with back buffer(s)" << indent;
                    }
                    else if((DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() || DDERR_INVALIDCAPS == g_DDSPrimary.GetLastError()) &&
                            (m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                            (!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) ||
                            !(m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
                    {
                        dbgout << "Flipping unsupported, unable to create surface with backBuffers";
                    }
                    else
                        tr |= trAbort;

                    g_DDSPrimary.ReleaseObject();
                    dbgout << unindent << "Done Destination" << endl;
                }
            }
            else
            {
                // See if the surface matches the test's desired criteria
                if (((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC) && (m_dwSurfCategoriesDst & scOffScrSys)) ||
                    (((m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && (m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)) && (m_dwSurfCategories & scOffScrSysOwnDc)) ||
                    ((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)  && (m_dwSurfCategoriesDst & scOffScrVid)) ||
                    ((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY)      && (m_dwSurfCategoriesDst & scOverlay)))
                {
                    dbgout << "Testing Destination Surface : " << szDescSurf << indent;

                    bool fRetrySmaller;
                    do
                    {
                        fRetrySmaller = false;
                        // Handle this as a simple surface
                        hr = m_piDD->CreateSurface(&m_cddsdDst, m_piDDSDst.AsTestedOutParam(), NULL);

                        // Give it another try with a smaller overlay if we didn't have enough memory the first time.
                        if (m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY &&
                            (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr))
                        {
                            m_cddsdDst.dwHeight /= 2;
                            m_cddsdDst.dwWidth /= 2;
                            hr = m_piDD->CreateSurface(&m_cddsdDst, m_piDDSDst.AsTestedOutParam(), NULL);
                        }

                        if (DDERR_GENERIC == hr)
                        {
                            dbgout << "Surface not supported" << endl;
                        } else
                        if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                        {
                            tr |= OutOfMemory_Helpers::VerifyOOMCondition(hr, m_piDD.AsInParam(), m_cddsdDst);
                        }
                        else if ((DDERR_NOOVERLAYHW == hr || DDERR_INVALIDCAPS == hr) &&
                                 ((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
                                  !(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_OVERLAY)))
                        {
                            // If we got a no overlay hardware error, the test still passes as
                            // long as either the surface is in system memory or if the HAL
                            // doesn't support overlays.
                            dbgout << "Overlay not supported ... Skipping" << endl;
                        }
                        else if (DDERR_UNSUPPORTEDFORMAT == hr && (m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                        {
                            // Unsupported formats are ok for overlay surfaces, since the
                            // support is driver dependent.
                            dbgout << "Overlay not supported ... Skipping" << endl;
                        }
                        else if((DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() || DDERR_NOFLIPHW == hr) &&
                            (m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                            (!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) ||
                            !(m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
                        {
                            dbgout << "Flipping unsupported, unable to create surface with backBuffers";
                        }
                        else if ((DDERR_TOOBIGHEIGHT == hr || DDERR_TOOBIGWIDTH == hr || DDERR_TOOBIGSIZE == hr) &&
                            !(m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
                        {
                            dbgout << "Surface too large (received " << hr << "). Will attempt smaller surface" << endl;
                            m_cddsdDst.dwWidth = (m_cddsdDst.dwWidth * 3 / 4) & (~0x3f);
                            m_cddsdDst.dwHeight = (m_cddsdDst.dwHeight * 3 / 4) & (~0x3f);
                            if (0 == m_cddsdDst.dwWidth || 0 == m_cddsdDst.dwHeight)
                            {
                                dbgout(LOG_FAIL) << "Unable to find small enough surface." << endl;
                                tr |= trFail;
                            }
                            else
                            {
                                dbgout << "Trying with surface of size " << m_cddsdDst.dwWidth << "x" << m_cddsdDst.dwHeight << endl;
                                fRetrySmaller = true;
                            }
                        }
                        else if (FAILED(hr))
                        {
                            QueryHRESULT(hr, "CreateSurface", tr |= trFail);
                            dbgout(LOG_FAIL)<< "NOTE: DirectDraw driver must now handle creation of system memory surfaces" << endl;
                        }
                        else QueryCondition(m_piDDSDst.InvalidOutParam(), "CreateSurface succeeded but failed to set the OUT parameter", tr |= trFail)
                        else
                        {
                            tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDSDst.AsInParam());
                            if(tr==trPass)
                            {
                                // Run the test on the surface
                                tr |= RunSurfaceTest_TWO();
                            }
                            m_piDDSDst.ReleaseInterface();
                        }
                    }while(fRetrySmaller);
                    dbgout << unindent << "Done Destination" << endl;
                }
            }

        }

        // Some special, not-in-the-loop cases
        //====================================

        // If the source is a primary, we can get its backbuffer
        if (m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            // If there is a backbuffer, and the test wants to be
            // run with a backbuffer destination
            if ((m_cddsd.dwBackBufferCount > 0) && (m_dwSurfCategoriesDst & scBackbuffer))
            {
                dbgout << "Testing Destination Surface : BackBuffer 1" << indent;
                m_piDDSDst = g_DDSBackbuffer.GetObject();
                m_cddsdDst = g_DDSBackbuffer.m_cddsd;
                if (m_piDDSDst != NULL)
                {
                    tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDSDst.AsInParam());
                    if(tr==trPass)
                    {
                        tr |= RunSurfaceTest_TWO();
                    }
                    m_piDDSDst.ReleaseInterface();
                }
                else if((DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() || DDERR_INVALIDCAPS == g_DDSPrimary.GetLastError()) &&
                        (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                        (!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) ||
                        !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
                    {
                        dbgout << "Flipping unsupported, unable to create surface with backBuffers" << endl;
                    }
                else
                    tr |= trAbort;
                g_DDSBackbuffer.ReleaseObject();
                dbgout << unindent << "Done" << endl;
            }
        }
        else if ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) && (m_dwSurfCategoriesDst & scPrimary))
        {
            // If the source is a backbuffer, and the test wants to test
            // the primary as a destination, we can get it and run the,
            // test, but DON'T destroy the primary!
            dbgout << "Testing Destination Surface : Primary" << indent;
            m_piDDSDst = g_DDSPrimary.GetObject();
            m_cddsdDst = g_DDSPrimary.m_cddsd;
            if (m_piDDSDst != NULL)
            {
                tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDSDst.AsInParam());
                if(tr==trPass)
                {
                    tr |= RunSurfaceTest_TWO();
                }
                m_piDDSDst.ReleaseInterface();
            }
            else
                tr |= trAbort;
            dbgout << unindent << "Done" << endl;
        }

        // Last, test the case that we're using the same surface as
        // both source and destination.  This must be last, since
        // it's destructive for the source surface.
        m_cddsdDst = m_cddsd;
        m_piDDSDst = m_piDDS;
        dbgout << "Testing Destination == Source" << indent;

        tr |=Surface_Helpers::AttachPaletteIfNeeded(m_piDD.AsInParam(), m_piDDSDst.AsInParam());
        if(tr==trPass)
        {
            tr |= RunSurfaceTest_TWO();
        }
        dbgout << unindent << "Done" << endl;
        m_piDDSDst.ReleaseInterface();

        if (tr & trAbort)
        {
            dbgout << "### Test Aborted" << endl;
        }

        return tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // GetSurfaceCategory
    //
    // A helper function that returns the surface category based on the surface caps.
    ////////////////////////////////////////////////////////////////////////////////
    DWORD CIterateDevModes::GetSurfaceCategory(DWORD dwSurfaceCaps)
    {
        DWORD dwSurfCategory = 0;
        if(dwSurfaceCaps & DDSCAPS_PRIMARYSURFACE)
        {
            dwSurfCategory = scPrimary;
        }
        else if(dwSurfaceCaps & DDSCAPS_OVERLAY)
        {
            dwSurfCategory = scOverlay;
        }
        else if(dwSurfaceCaps & DDSCAPS_BACKBUFFER)
        {
            dwSurfCategory = scPrimary | scBackbuffer;
        }
        else if(dwSurfaceCaps & DDSCAPS_SYSTEMMEMORY)
        {
            dwSurfCategory = scOffScrSys | scOffScrSysOwnDc;   
        }
        else if(dwSurfaceCaps & DDSCAPS_VIDEOMEMORY)
        {
            dwSurfCategory = scOffScrVid;
        }
        
        return dwSurfCategory;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CIterateDevModes::TestIDirectDrawSurface
    //
    // Iterates through all possible device mode changes.
    ////////////////////////////////////////////////////////////////////////////////
    eTestResult CIterateDevModes::TestIDirectDrawSurface()
    {
        eTestResult tr = trPass;
        LONG result = 0;
        DWORD dwTmpFields = 0; // Temp. Var to store the dmFields value.

        // Get all the valid DevModes for the current display.
        vector<DEVMODE> rgdmValidDevModes;
        DWORD cNumDisplaySettings = 0;

        DEVMODE dmTemp;
        memset(&dmTemp, 0x00, sizeof(dmTemp));
        dmTemp.dmSize = sizeof(dmTemp);

        // Get the current orientation
        DWORD dwCurrentOrientation;
        dmTemp.dmFields = DM_DISPLAYORIENTATION;
        // Ignore the return value.
        ChangeDisplaySettingsEx(NULL, &dmTemp, NULL, CDS_TEST, NULL);
        dwCurrentOrientation = dmTemp.dmDisplayOrientation;

        // Calling EnumDisplaySettings with cNumDisplaySettings = 0
        // initializes & caches the information.
        while(EnumDisplaySettings(NULL, cNumDisplaySettings, &dmTemp))
        {
            DEVMODE dmSaved = dmTemp;

            // Get the screen rotations that are supported.
            dmTemp.dmFields = DM_DISPLAYQUERYORIENTATION;
            result = ChangeDisplaySettingsEx(NULL, &dmTemp, NULL, CDS_TEST, NULL);

            // Add separate DEVMODEs for each supported angle.
            DWORD dwAnglesSupported = dmTemp.dmDisplayOrientation;

            // Restore the saved DEVMODE.
            dmTemp = dmSaved;
            dmTemp.dmFields |= DM_DISPLAYORIENTATION;
  
            // Get the un-rotated width and height.
            DWORD dwUnrotatedWidth = dmTemp.dmPelsWidth;
            DWORD dwUnrotatedHeight = dmTemp.dmPelsHeight;
            if(dwCurrentOrientation & DMDO_90 || dwCurrentOrientation & DMDO_270)
            {
                dwUnrotatedWidth = dmTemp.dmPelsHeight;
                dwUnrotatedHeight = dmTemp.dmPelsWidth;
            }
            
            // Assume that orientation 0 is always supported and add it to the list.
            // There is no way to query if orientation 0 is supported, using ChangeDisplaySettingsEx,
            // as DMDO_0 = 0.
            dmTemp.dmPelsWidth = dwUnrotatedWidth;
            dmTemp.dmPelsHeight = dwUnrotatedHeight;
            dmTemp.dmDisplayOrientation = DMDO_0;
            rgdmValidDevModes.push_back(dmTemp);

            // Add the DEVMODEs with rotated width/height to the vector.
            static const DWORD c_rgAngles[] = {DMDO_90, DMDO_180, DMDO_270};
            for(int i = 0;i < sizeof(c_rgAngles)/sizeof(DWORD);++i)
            {
                if(dwAnglesSupported & c_rgAngles[i])
                {
                    dmTemp.dmDisplayOrientation = c_rgAngles[i];

                    // Swap width and height if angles are 90 / 270, as ChangeDisplaySettingsEx expects 
                    // rotated width and height for these orientations.
                    if((c_rgAngles[i] & DMDO_90) || (c_rgAngles[i] & DMDO_270))
                    {
                        dmTemp.dmPelsWidth = dwUnrotatedHeight;
                        dmTemp.dmPelsHeight = dwUnrotatedWidth;
                    }
                    else // Use the un-rotated width/height in 0 / 180 orientations.
                    {
                        dmTemp.dmPelsWidth = dwUnrotatedWidth;
                        dmTemp.dmPelsHeight = dwUnrotatedHeight;
                    }

                    rgdmValidDevModes.push_back(dmTemp);
                }
            }
            ++cNumDisplaySettings;
        }

        DWORD cNumDevModes = rgdmValidDevModes.size();
        dbgout << "Number of valid DevModes : "
               << cNumDevModes
               << endl;

        // If the symbols "DevModeFrom" & "DevModeTo" are defined,
        // shift only between these devmodes.
        // Assuming that EnumDisplaySettings will return the DEVMODEs in the same order
        // each time.
        DWORD dwDevModeFrom = static_cast<DWORD> (-1); 
        DWORD dwDevModeTo = static_cast<DWORD> (-1); 
        g_DDConfig.GetStringValue(DDrawUty::wstring(TEXT("DevModeFrom")), &dwDevModeFrom);
        g_DDConfig.GetStringValue(DDrawUty::wstring(TEXT("DevModeTo")), &dwDevModeTo);

        // If the number of Dev mode is 0, simply return the function call without any
        // further testing. The number == 1 case will be handled in later double for loop
        if( cNumDevModes <= 0 )
        {
            dbgout << "There are no DevMode combinations to test."
                   << endl;
            return tr;
        }

        // If the surface owns a DC, store the DC in m_hdc.
        // This can be used in the function TestDevModeChange implemented by the test,
        // if it chooses to.
        // The DC (m_hdc) need not be explicitly released.
        // It will be released when the surface(m_piDDS) is deleted.
        if(m_cddsd.ddsCaps.dwCaps & DDSCAPS_OWNDC)
        {
            HRESULT hr = m_piDDS->GetDC(&m_hdc);

            QueryForHRESULT(hr, S_OK, "Failure code returned from GetDC() for a DDSCAPS_OWNDC surface.", tr|=trAbort);

            if(NULL == m_hdc || trPass != tr)
            {
                dbgout(LOG_FAIL) << "Aborting test case. Could not retrieve a DC to a DDSCAPS_OWNDC surface."
                                 << endl;
                return trAbort;
            }
        }

        // Save the current DevMode. This will be restored at the end of the test.
        DEVMODE dmToBeRestored;
        if(trPass != DevMode_Helpers::GetCurrentDevMode(dmToBeRestored))
        {
            dbgout << "Unable to retrieve the current DevMode. Continuing anyway.."
                   << endl;
        }

        // Test for all possible combinations of device mode changes.
        for(DWORD i = 0;i < cNumDevModes && (trPass == tr);++i)
        {
            m_oldDevMode = rgdmValidDevModes[i];

            for(DWORD j = 0;j < cNumDevModes && (trPass == tr);++j)
            {
                // When testing all possibilities, we don't test any oldDevMode=newDevMode case.
                if((dmcAllPossibilities == m_dwDevModeChangesCategory) && (i == j)) 
                {
                    continue;
                }
                else if(dmcSequentialChange == m_dwDevModeChangesCategory)
                {

                    // We always want to test the case for changing from DevMode0 to DevMode0 (the first DevMode in the vector)
                    // for only one Devmode device, this is the only case, and we want to handle it. 

                    // When it is not (i=j=0) case, we only test the sequential change case.
                    if (!((i==0) && (j==0)))
                    {
                        // The newDevMode(j) must be oldDevMode(i)'s next element in the vector, otherwise we won't test for this combination
                        if (j != (i+1)%cNumDevModes)
                        {
                            continue;
                        }
                    }
                }

                // When a debug command line has been sent to the test,
                // shift only from dwDevModeFrom to dwDevModeTo. (if defined)
                if((dwDevModeFrom != -1) && (dwDevModeTo != -1))
                {
                    if((i != dwDevModeFrom) || (j != dwDevModeTo))
                    {
                        continue;
                    }
                }

                m_newDevMode = rgdmValidDevModes[j];

                // Set the ith display setting
                tr |= DevMode_Helpers::SetDevMode(m_oldDevMode);
                if(trPass != tr)
                {
                    break;
                }

                tr |= PreDevModeChangeTest();
                if(trPass == tr) // Run the surface tests only if the PreDevModeChangeTest passes.
                {
                    tr |= DevMode_Helpers::SetDevMode(m_newDevMode);
                    if(trPass != tr)
                    {
                        break;
                    }

                    tr |= RunDevModeChangeTest();
   
                    // Do this even when RunDevModeChangeTest fails. 
                    // Any cleanup needed for operations performed in PreDevModeChangeTest() will be
                    // executed here.
                    tr |= PostDevModeChangeTest();

                    if(trFail == tr)
                    {
                        dbgout(LOG_FAIL) << "Test Failed : Try running [s tux -d ddrawtk -o -x <260 or 360>"
                                         << " -c \"dbg "
                                         << "DevModeFrom " << i << " "
                                         << "DevModeTo " << j << " "
                                         << "Surfaces " << GetSurfaceCategory(m_cddsd.ddsCaps.dwCaps) << "\"]"
                                         << " to reproduce this error faster."
                                         << endl;
                    }
                }
                else if(trSkip == tr)
                {
                    // Reset the test result, as only this particular test has been skipped.
                    // The tests will try to continue with other dev modes.
                    tr = trPass; 
                }
                else // A failure
                {
                    break; 
                }     
            } // for j
        } // for i

        // Restore the initial DevMode. Ignore the return value.
        if(trPass != DevMode_Helpers::SetDevMode(dmToBeRestored))
        {
            dbgout << "Unable to set the initial DevMode. Continuing anyway...."
                   << endl;
        }

        return tr;
    }
};
