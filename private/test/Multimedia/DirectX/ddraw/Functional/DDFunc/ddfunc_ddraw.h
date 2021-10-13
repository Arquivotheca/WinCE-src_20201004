#pragma once
#if !defined(__DDFUNC_DDRAW_H__) && CFG_TEST_IDirectDraw
#define __DDFUNC_DDRAW_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDTest_Base.h>

namespace Test_IDirectDraw
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawBaseClasses::CTest_IDirectDraw \
    { \
    public: \
        virtual eTestResult TestIDirectDraw(); \
    }
    
//    DECLARE_TEST(Compact);
#if CFG_TEST_IDirectDrawClipper
    DECLARE_TEST(CreateClipper);
#endif // CFG_TEST_IDirectDrawClipper
    DECLARE_TEST(CreatePalette);
    DECLARE_TEST(CreateSurface);
//    DECLARE_TEST(DuplicateSurface);

    //DECLARE_TEST(EnumDisplayModes);
    class CTest_EnumDisplayModes : virtual public DDrawBaseClasses::CTest_IDirectDraw
    {
    protected:
        DWORD m_dwPreset;
        eTestResult m_tr;
        int m_nCallbackCount;
        bool m_fCallbackExpected;
        
    public:
        CTest_EnumDisplayModes() :
            m_tr(trPass),
            m_nCallbackCount(0),
            m_fCallbackExpected(false)
        {
            PresetUty::Preset(m_dwPreset);
        }
        
        virtual eTestResult TestIDirectDraw();
        static HRESULT WINAPI EnumModesCallback (LPDDSURFACEDESC  lpDDSurfaceDesc, LPVOID lpContext);
    };
    
    DECLARE_TEST(FlipToGDISurface);
    DECLARE_TEST(GetCaps);
    DECLARE_TEST(GetSetDisplayMode);
    DECLARE_TEST(GetFourCCCodes);
//    DECLARE_TEST(GetGDISurface);
//    DECLARE_TEST(GetMonitorFrequency);
//    DECLARE_TEST(GetScanLine);
//    DECLARE_TEST(GetWaitForVerticalBlank);
//    DECLARE_TEST(Initialize);
    DECLARE_TEST(RestoreDisplayMode);
    DECLARE_TEST(SetCooperativeLevel);
//    DECLARE_TEST(GetSurfaceFromDC);
    DECLARE_TEST(ReceiveWMUnderFSE);

#undef DECLARE_TEST

}

#endif // header protection


