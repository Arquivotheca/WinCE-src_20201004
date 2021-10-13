#pragma once
#if !defined(__IDDSURFACETESTS_H__)
#define __IDDSURFACETESTS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDTest_Base.h>

namespace Test_IDirectDrawSurface
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface \
    { \
    public: \
        virtual eTestResult TestIDirectDrawSurface(); \
    }

    DECLARE_TEST(AddRefRelease);
    DECLARE_TEST(QueryInterface);
    DECLARE_TEST(GetCaps);
    DECLARE_TEST(GetSurfaceDescription);
    DECLARE_TEST(GetPixelFormat);
    DECLARE_TEST(GetReleaseDeviceContext);
    DECLARE_TEST(GetSetPalette);
    DECLARE_TEST(LockUnlock);
    DECLARE_TEST(GetSetClipper);
    DECLARE_TEST(Flip);
    DECLARE_TEST(ColorFillBlts);
    
    #undef DECLARE_TEST
};

namespace Test_IDirectDrawSurface_TWO
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface_TWO \
    { \
    public: \
        virtual eTestResult TestIDirectDrawSurface_TWO(); \
    }

    DECLARE_TEST(Blt);
    DECLARE_TEST(BltFast);
    DECLARE_TEST(AlphaBlt);
    
    #undef DECLARE_TEST
};


#endif // header protection


