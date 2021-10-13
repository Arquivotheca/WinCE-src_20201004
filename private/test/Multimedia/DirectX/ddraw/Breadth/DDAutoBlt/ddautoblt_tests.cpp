#include "main.h"
#include "DDAutoBlt_Tests.h"

#include <DDrawUty.h>

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_Surface_Sets
{
    eTestResult CTest_Basics::PreSurfaceTest()
    {
        int i;
        
        // Configurations to test (if supported)
        CfgSurfaceType rgPairsPrimary[] = 
        {
            stPrimary0Back,
            stPrimary1Back,
            stPrimary2Back,
            stPrimary3Back,
//            // These DC surfaces will all have the same pixel format as the primary
//            stDCCreateDC,
//            stDCGetDCHwnd,
//            stDCGetDCNull,
//            stDCCompatBmp
        };
        CfgSurfaceType rgPairsSimple[] = 
        {
            stOffScrVid,
            stOffScrSys,
            stOffScrSysOwnDc, 
            stOverlayVid0Back,
            stOverlaySys0Back
        };

        for (i = 0; i < countof(rgPairsPrimary); i++)
            m_vectPairs.push_back(SurfPixPair(rgPairsPrimary[i], pfNone));
            
        for (i = 0; i < countof(rgPairsSimple); i++)
            m_vectPairs.push_back(SurfPixPair(rgPairsSimple[i], pfNone));

        return trPass;
    }

    eTestResult CTest_SamePixel::PreSurfaceTest()
    {
        int i;
        
        // Configurations to test (if supported)
        CfgSurfaceType rgPairsPrimary[] = 
        {
            stPrimary0Back,
            stPrimary1Back,
            stPrimary2Back,
            stPrimary3Back,
            // These DC surfaces will all have the same pixel format as the primary
//            stDCCreateDC,
//            stDCGetDCHwnd,
//            stDCGetDCNull,
//            stDCCompatBmp
        };
        CfgSurfaceType rgPairsSimple[] = 
        {
            stOffScrVid,
            stOffScrSys,
            stOffScrSysOwnDc, 
            stOverlayVid0Back,
            stOverlaySys0Back
        };

        for (i = 0; i < countof(rgPairsPrimary); i++)
        {
            m_vectPairs.push_back(SurfPixPair(rgPairsPrimary[i], pfNone));
            m_vectPairsDst.push_back(SurfPixPair(rgPairsPrimary[i], pfNone));
        }
            
        for (i = 0; i < countof(rgPairsSimple); i++)
        {
            m_vectPairs.push_back(SurfPixPair(rgPairsSimple[i], pfNone));
            m_vectPairsDst.push_back(SurfPixPair(rgPairsSimple[i], pfNone));
        }
        
        return trPass;
    }
    
    eTestResult CTest_DifferentPixel::PreSurfaceTest()
    {
        int i, j;
        
        // Configurations to test (if supported)
        CfgSurfaceType rgPairsPrimary[] = 
        {
            stPrimary0Back,
            stPrimary1Back,
            stPrimary2Back,
            stPrimary3Back,
            // These DC surfaces will all have the same pixel format as the primary
//            stDCCreateDC,
//            stDCGetDCHwnd,
//            stDCGetDCNull,
//            stDCCompatBmp
        };
        CfgSurfaceType rgPairsSimple[] = 
        {
            stOffScrVid,
            stOffScrSys,
            stOffScrSysOwnDc,
            stOverlayVid0Back,
            stOverlaySys0Back
        };
        CfgPixelFormat rgPixelFormatsSimple[] = 
        {
            pfNone,
//            pfPal8,
            // 16 bits - no Alpha
            pfRGB555,
            pfRGB565,
            pfBGR565,
            // 16 bits - Alpha
            pfARGB1555pm,
            pfARGB4444pm,
            // 24 bits
//            pfRGB888,
//            pfBGR888,
            // 32 bits - no Alpha
            pfRGB0888,
            pfBGR0888,
            // 32 bits - Alpha
            pfARGB8888pm,
            pfABGR8888pm,
            // YUV Formats
            pfYUVYUYV,
            pfYUVUYVY
        };

        for (i = 0; i < countof(rgPairsPrimary); i++)
        {
            m_vectPairs.push_back(SurfPixPair(rgPairsPrimary[i], pfNone));
            m_vectPairsDst.push_back(SurfPixPair(rgPairsPrimary[i], pfNone));
        }
            
        for (i = 0; i < countof(rgPairsSimple); i++)
        {
            for (j = 0; j < countof(rgPixelFormatsSimple); j++)
            {
                m_vectPairs.push_back(SurfPixPair(rgPairsSimple[i], rgPixelFormatsSimple[j]));
                m_vectPairsDst.push_back(SurfPixPair(rgPairsSimple[i], rgPixelFormatsSimple[j]));
            }
        }

        

        return trPass;
    }
};
namespace Test_Mode_Sets
{
    eTestResult CTest_All::PreDDrawTest()
    {
        m_vectDispMode=g_DDConfig.m_vdmSupported;
        return trPass;
    }
};

