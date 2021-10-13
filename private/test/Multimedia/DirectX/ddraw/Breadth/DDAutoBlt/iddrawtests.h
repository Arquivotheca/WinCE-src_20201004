#pragma once
#if !defined(__IDDRAWTESTS_H__)
#define __IDDRAWTESTS_H__

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
    
    DECLARE_TEST(AddRefRelease);
    DECLARE_TEST(QueryInterface);

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
        static HRESULT WINAPI Callback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext);
    };
    
    DECLARE_TEST(SetCooperativeLevel);
    DECLARE_TEST(GetCaps);
    DECLARE_TEST(CreateSurface);
    DECLARE_TEST(CreatePalette);
#if CFG_TEST_IDirectDrawClipper
    DECLARE_TEST(CreateClipper);
#endif // CFG_TEST_IDirectDrawClipper
    
    #undef DECLARE_TEST
};

#endif // header protection

