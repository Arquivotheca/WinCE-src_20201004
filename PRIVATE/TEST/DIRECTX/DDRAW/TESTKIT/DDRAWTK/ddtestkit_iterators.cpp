//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "main.h"

#include "DDrawUty.h"
#include "DDTestKit_Iterators.h"

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
                                if(!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                                    !(g_DDConfig.HELCaps().ddsCaps.dwCaps & DDSCAPS_FLIP))
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
                    else if(DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() && 
                        (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                        ((!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                        !(g_DDConfig.HELCaps().ddsCaps.dwCaps & DDSCAPS_FLIP)) ||
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
                if (((m_cddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && (m_dwSurfCategories & scOffScrSys)) ||
                    ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)  && (m_dwSurfCategories & scOffScrVid)) ||
                    ((m_cddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY)      && (m_dwSurfCategories & scOverlay)))
                {
                    dbgout << "Testing Source Surface : " << szDescSurf << indent;
                    
                    // Handle this as a simple surface
                    hr = m_piDD->CreateSurface(&m_cddsd, m_piDDS.AsTestedOutParam(), NULL);
                    if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                    {
                        tr |= OutOfMemory_Helpers::VerifyOOMCondition(hr, m_piDD.AsInParam(), m_cddsd);
                    }
                    else if (DDERR_NOOVERLAYHW == hr &&
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
                    else QueryHRESULT(hr, "CreateSurface", tr |= trFail)
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

                                if(!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                                    !(g_DDConfig.HELCaps().ddsCaps.dwCaps & DDSCAPS_FLIP))
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
                    else if(DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() && 
                            (m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                            ((!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                            !(g_DDConfig.HELCaps().ddsCaps.dwCaps & DDSCAPS_FLIP)) ||
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
                if (((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) && (m_dwSurfCategoriesDst & scOffScrSys)) ||
                    ((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)  && (m_dwSurfCategoriesDst & scOffScrVid)) ||
                    ((m_cddsdDst.ddsCaps.dwCaps & DDSCAPS_OVERLAY)      && (m_dwSurfCategoriesDst & scOverlay)))
                {
                    dbgout << "Testing Destination Surface : " << szDescSurf << indent;
                    
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
                    
                    if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                    {
                        tr |= OutOfMemory_Helpers::VerifyOOMCondition(hr, m_piDD.AsInParam(), m_cddsdDst);
                    }
                    else if (DDERR_NOOVERLAYHW == hr &&
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
                    else if(DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() && 
                        (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                        ((!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                        !(g_DDConfig.HELCaps().ddsCaps.dwCaps & DDSCAPS_FLIP)) ||
                        !(m_cddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)))
                    {
                        dbgout << "Flipping unsupported, unable to create surface with backBuffers";
                    }
                    else QueryHRESULT(hr, "CreateSurface", tr |= trFail)
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
                else if(DDERR_NOFLIPHW == g_DDSPrimary.GetLastError() && 
                        (m_cddsd.ddsCaps.dwCaps & DDSCAPS_FLIP) &&
                        ((!(g_DDConfig.HALCaps().ddsCaps.dwCaps & DDSCAPS_FLIP) && 
                        !(g_DDConfig.HELCaps().ddsCaps.dwCaps & DDSCAPS_FLIP)) ||
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
};

