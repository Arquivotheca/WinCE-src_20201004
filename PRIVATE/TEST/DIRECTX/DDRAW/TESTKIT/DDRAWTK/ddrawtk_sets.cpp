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
            stOffScrSys
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
            stOffScrSys
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
        m_vectPairsDst.push_back(SurfPixPair(stOverlayVid, pfNone)); 
        m_vectPairsDst.push_back(SurfPixPair(stOverlayVid, pfYUVYUYV));
        m_vectPairsDst.push_back(SurfPixPair(stOverlayVid, pfYUVUYVY));

        return trPass;
    }
    eTestResult CTest_SamePixelInteractive::PreSurfaceTest()
    {

        m_vectPairs.push_back(SurfPixPair(stOffScrVid, pfNone)); 
        m_vectPairsDst.push_back(SurfPixPair(stPrimary0Back, pfNone));

        return trPass;
    }
}
