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
#include "DDrawTK.h"

#include <DDrawUty.h>

using namespace DDrawUty;
using namespace DebugStreamUty;

namespace Test_Surface_Sets
{
    eTestResult CTest_Basics::PreSurfaceTest()
    {
        int i;

        // Configurations to test (if supported)
        CfgSurfaceType rgSurfs[] =
        {
            stPrimary0Back,
            stPrimary1Back,
            stPrimary2Back,
            stPrimary3Back,
            stOffScrVid,
            stOffScrSys,
            stOffScrSysOwnDc,
            stOverlayVid0Back,
            stOverlayVid1Back,
            stOverlayVid2Back,
        };

        for (i = 0; i < countof(rgSurfs); i++)
        {
            m_vectPairs.push_back(SurfPixPair(rgSurfs[i], pfNone));
        }

        return trPass;
    }

    eTestResult CTest_SamePixel::PreSurfaceTest()
    {
        int i;

        // Configurations to test (if supported)
        CfgSurfaceType rgSurfs[] =
        {
            stPrimary0Back,
            stPrimary1Back,
            stPrimary2Back,
            stPrimary3Back,
            stOffScrVid,
            stOffScrSys,
            stOffScrSysOwnDc
        };

        for (i = 0; i < countof(rgSurfs); i++)
        {
            m_vectPairs.push_back(SurfPixPair(rgSurfs[i], pfNone));
            m_vectPairsDst.push_back(SurfPixPair(rgSurfs[i], pfNone));
        }

        return trPass;
    }

    eTestResult CTest_OverlayInteractive::PreSurfaceTest()
    {
        // overlay only makes sense with the source as a primary
        m_vectPairs.push_back(SurfPixPair(stPrimary0Back, pfNone));
        // these are the overlay pixel formats we'll support in the test
        // same as the primary, some form of rgb
        for (CfgSurfaceType SurfaceType = stOverlayVid0Back; SurfaceType <= stOverlayVid2Back; ++(*(int*)&SurfaceType))
        {
            m_vectPairsDst.push_back(SurfPixPair(SurfaceType, pfNone));
            for (CfgPixelFormat PixelFormat = pfYUVYUYV; PixelFormat <= pfYUVYUY2; ++(*(int*)&PixelFormat))
            {
                m_vectPairsDst.push_back(SurfPixPair(SurfaceType, PixelFormat));
            }
        }

        return trPass;
    }
    eTestResult CTest_SamePixelInteractive::PreSurfaceTest()
    {

        m_vectPairs.push_back(SurfPixPair(stOffScrVid, pfNone));
        m_vectPairsDst.push_back(SurfPixPair(stPrimary0Back, pfNone));

        return trPass;
    }

    // CTest_DevModeChange functions.
    eTestResult CTest_DevModeChange::PreDevModeChangeTest()
    {
        using namespace DDrawUty::Surface_Helpers;

        HRESULT result = DD_OK;

        // Restore the surface if it has been lost.
        if(DDERR_SURFACELOST == m_piDDS->IsLost())
        {
            result = m_piDDS->Restore();
            if(DD_OK != result)
            {
                // If restore fails with an out of memory, pass the test.
                if(DDERR_OUTOFMEMORY == result || DDERR_OUTOFVIDEOMEMORY == result)
                {
                    dbgout << "Surface Restore Failed with an out of memory." << endl;
                    dbgout << "Skipping the test..." << endl;

                    return trSkip;
                }
                else
                {
                    dbgout(LOG_ABORT) << "Surface Restore Failed with error code = "
                                      << g_ErrorMap[result]<< "  (" << HEX((DWORD)result)
                                      << ")" << endl;

                    return trAbort;
                }
            }
        }

        // Set the srcblt color key with a random value.
        // verify that the color key is set in
        // CTest_GetSurfaceDesc::TestDevModeChange on restoring the
        // surface.
        DDCOLORKEY ddck;
        memset(&ddck, 0x0, sizeof(ddck));
        ddck.dwColorSpaceLowValue = 0xFEFF;
        ddck.dwColorSpaceHighValue = 0xFEFF;

        if(FAILED(m_piDDS->SetColorKey(DDCKEY_SRCBLT, &ddck)))
        {
            dbgout << "Setting a color key failed. Continuing..."
                   << endl;
        }
                
        LPRECT prcBoundRect = NULL;
        RECT rcBoundRect = {0, 0, m_cddsd.dwWidth, m_cddsd.dwHeight};

        // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
        if((m_cddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && (g_DDConfig.CooperativeLevel() == DDSCL_NORMAL))
        {
            eTestResult tr = GetClippingRectangle(m_piDDS.AsInParam(), rcBoundRect);
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

        // Lock the surface and
        // Store the surface description of the current surface prior to changing the
        // devmode.
        m_cddsd.dwSize = sizeof(m_cddsd);
        result = m_piDDS->Lock(prcBoundRect, &m_cddsd, DDLOCK_WAITNOTBUSY, NULL);
        if(FAILED(result))
        {
            dbgout(LOG_ABORT) << "Locking the surface failed with error = "
                              << g_ErrorMap[result]<< "  (" << HEX((DWORD)result)
                              << ")" << endl;
            return trAbort;
        }

        m_piDDS->Unlock(prcBoundRect);

        return trPass;
    }

    eTestResult CTest_DevModeChange::PostDevModeChangeTest()
    {
        // Do nothing

        return trPass;
    }
}
