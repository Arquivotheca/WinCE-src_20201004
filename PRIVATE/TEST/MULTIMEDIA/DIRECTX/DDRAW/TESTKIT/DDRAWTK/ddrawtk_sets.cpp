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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//

#include "main.h"
#include "DDrawTK.h"

#include <DDrawUty.h>

using namespace DDrawUty;

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
}
