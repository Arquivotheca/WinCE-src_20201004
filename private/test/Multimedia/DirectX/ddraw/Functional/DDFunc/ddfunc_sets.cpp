#include "main.h"
#include "DDFunc_Sets.h"

#include <QATestUty/PresetUty.h>
#include <STLExtensionUty.h>
#include <DDrawUty.h>

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_Surface_Sets
{
    eTestResult CTest_All::PreSurfaceTest()
    {
        CDirectDrawConfiguration::VectPixelFormat::const_iterator itrPixel;
        CDirectDrawConfiguration::VectSurfaceType::const_iterator itrSurface;

        // Add all primary surface descriptions, with NO pixel description
        for (itrSurface  = g_DDConfig.m_vstSupportedPrimary.begin();
             itrSurface != g_DDConfig.m_vstSupportedPrimary.end();
             itrSurface++)
        {
            m_vectPairs.push_back(SurfPixPair(*itrSurface, pfNone));
        }
        
        // Add all video surface types with all supported video memory
        // pixel formats
        for (itrSurface  = g_DDConfig.m_vstSupportedVidMem.begin();
             itrSurface != g_DDConfig.m_vstSupportedVidMem.end();
             itrSurface++)
        {
            for (itrPixel  = g_DDConfig.m_vpfSupportedVidMem.begin();
                 itrPixel != g_DDConfig.m_vpfSupportedVidMem.end();
                 itrPixel++)
            {
                m_vectPairs.push_back(SurfPixPair(*itrSurface, *itrPixel));
            }
        }
        
        // Add all system surface types with all supported system memory
        // pixel formats
        for (itrSurface  = g_DDConfig.m_vstSupportedSysMem.begin();
             itrSurface != g_DDConfig.m_vstSupportedSysMem.end();
             itrSurface++)
        {
            for (itrPixel  = g_DDConfig.m_vpfSupportedSysMem.begin();
                 itrPixel != g_DDConfig.m_vpfSupportedSysMem.end();
                 itrPixel++)
            {
                m_vectPairs.push_back(SurfPixPair(*itrSurface, *itrPixel));
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


