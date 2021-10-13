#pragma once
#if !defined(__DDAUTOBLT_TESTS_H__)
#define __DDAUTOBLT_TESTS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDrawUty_Config.h>
#include <DDTest_Base.h>
#include <DDTest_Modes.h>
#include <DDTest_Iterators.h>
#include <DDTest_Modifiers.h>

#include "IDDrawTests.h"
#include "IDDSurfaceTests.h"
    
namespace Test_Surface_Sets
{
    // Single Surface Sets
    class CTest_Basics : virtual public Test_Surface_Iterators::CIterateSurfaces
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };

    // Multiple Surface Sets
    class CTest_SamePixel : virtual public Test_Surface_Iterators::CIterateSurfaces_TWO
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };
    
    class CTest_DifferentPixel : virtual public Test_Surface_Iterators::CIterateSurfaces_TWO
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };
};
namespace Test_Mode_Sets
{
    class CTest_All : virtual public Test_DDraw_Iterators::CIterateDDraw
    {
    public:
        virtual eTestResult PreDDrawTest();
    };
    
}
namespace Test_Normal_Mode
{
    namespace Test_DirectDraw
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public Test_DDraw_Modes::CTest_NormalMode, \
            virtual public Test_IDirectDraw::CTest_##METHODNAME { }

        DECLARE_TEST(EnumDisplayModes);
        DECLARE_TEST(GetCaps);
        DECLARE_TEST(CreateSurface);
        DECLARE_TEST(CreatePalette);
#if CFG_TEST_IDirectDrawClipper
        DECLARE_TEST(CreateClipper);
#endif // CFG_TEST_IDirectDrawClipper

        #undef DECLARE_TEST
    };

    namespace Test_DirectDrawSurface
    {

        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public Test_DDraw_Modes::CTest_NormalMode, \
                                   virtual public Test_Surface_Sets::CTest_Basics, \
                                   virtual public Test_IDirectDrawSurface::CTest_##METHODNAME { }

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
    
    namespace Test_DirectDrawSurface_TWO
    {
        namespace Test_SamePixel
        {
            #define DECLARE_TEST(MODIFIER, METHODNAME) \
            class CTest_##MODIFIER##METHODNAME : virtual public Test_DDraw_Modes::CTest_NormalMode, \
                                                 virtual public Test_Surface_Sets::CTest_SamePixel, \
                                                 virtual public Test_Surface_Modifiers::CTest_##MODIFIER, \
                                                 virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }
                                                 
            DECLARE_TEST(Fill, Blt);
            DECLARE_TEST(Fill, BltFast);
            DECLARE_TEST(Fill, AlphaBlt);
            
            DECLARE_TEST(ColorKey, Blt);
            DECLARE_TEST(ColorKey, BltFast);
            DECLARE_TEST(ColorKey, AlphaBlt);
            
            DECLARE_TEST(Overlay, Blt);
            DECLARE_TEST(Overlay, BltFast);
            DECLARE_TEST(Overlay, AlphaBlt);
            
            DECLARE_TEST(ColorKeyOverlay, Blt);
            DECLARE_TEST(ColorKeyOverlay, BltFast);
            DECLARE_TEST(ColorKeyOverlay, AlphaBlt);
            
            #undef DECLARE_TEST
        };
        
        namespace Test_DifferentPixel
        {
            #define DECLARE_TEST(MODIFIER, METHODNAME) \
            class CTest_##MODIFIER##METHODNAME : virtual public Test_DDraw_Modes::CTest_NormalMode, \
                                                 virtual public Test_Surface_Sets::CTest_DifferentPixel, \
                                                 virtual public Test_Surface_Modifiers::CTest_##MODIFIER, \
                                                 virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }
                                               
            DECLARE_TEST(Fill, Blt);
            DECLARE_TEST(Fill, BltFast);
            DECLARE_TEST(Fill, AlphaBlt);
            
            DECLARE_TEST(ColorKey, Blt);
            DECLARE_TEST(ColorKey, BltFast);
            DECLARE_TEST(ColorKey, AlphaBlt);
            
            DECLARE_TEST(Overlay, Blt);
            DECLARE_TEST(Overlay, BltFast);
            DECLARE_TEST(Overlay, AlphaBlt);
            
            DECLARE_TEST(ColorKeyOverlay, Blt);
            DECLARE_TEST(ColorKeyOverlay, BltFast);
            DECLARE_TEST(ColorKeyOverlay, AlphaBlt);
            
            #undef DECLARE_TEST
        };
    };
};

namespace Test_Exclusive_Mode
{

    namespace Test_DirectDraw
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public Test_DDraw_Modes::CTest_ExclusiveMode, \
                                   virtual public Test_Mode_Sets::CTest_All, \
                                   virtual public Test_IDirectDraw::CTest_##METHODNAME { }

        DECLARE_TEST(EnumDisplayModes);
        DECLARE_TEST(GetCaps);
        DECLARE_TEST(CreateSurface);
        DECLARE_TEST(CreatePalette);
#if CFG_TEST_IDirectDrawClipper
        DECLARE_TEST(CreateClipper);
#endif // CFG_TEST_IDirectDrawClipper

        #undef DECLARE_TEST
    };
    
    namespace Test_DirectDrawSurface
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public Test_DDraw_Modes::CTest_ExclusiveMode, \
                                   virtual public Test_Mode_Sets::CTest_All, \
                                   virtual public Test_Surface_Sets::CTest_Basics, \
                                   virtual public Test_IDirectDrawSurface::CTest_##METHODNAME { }

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
    
    namespace Test_DirectDrawSurface_TWO
    {
        namespace Test_SamePixel
        {
            #define DECLARE_TEST(MODIFIER, METHODNAME) \
            class CTest_##MODIFIER##METHODNAME : virtual public Test_DDraw_Modes::CTest_ExclusiveMode, \
                                                 virtual public Test_Mode_Sets::CTest_All, \
                                                 virtual public Test_Surface_Sets::CTest_SamePixel, \
                                                 virtual public Test_Surface_Modifiers::CTest_##MODIFIER, \
                                                 virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }
                                               
            DECLARE_TEST(Fill, Blt);
            DECLARE_TEST(Fill, BltFast);
            DECLARE_TEST(Fill, AlphaBlt);
            
            DECLARE_TEST(ColorKey, Blt);
            DECLARE_TEST(ColorKey, BltFast);
            DECLARE_TEST(ColorKey, AlphaBlt);
            
            DECLARE_TEST(Overlay, Blt);
            DECLARE_TEST(Overlay, BltFast);
            DECLARE_TEST(Overlay, AlphaBlt);
            
            DECLARE_TEST(ColorKeyOverlay, Blt);
            DECLARE_TEST(ColorKeyOverlay, BltFast);
            DECLARE_TEST(ColorKeyOverlay, AlphaBlt);
            
            #undef DECLARE_TEST
        };
        
        namespace Test_DifferentPixel
        {
            #define DECLARE_TEST(MODIFIER, METHODNAME) \
            class CTest_##MODIFIER##METHODNAME : virtual public Test_DDraw_Modes::CTest_ExclusiveMode, \
                                                 virtual public Test_Mode_Sets::CTest_All, \
                                                 virtual public Test_Surface_Sets::CTest_DifferentPixel, \
                                                 virtual public Test_Surface_Modifiers::CTest_##MODIFIER, \
                                                 virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }
                                               
            DECLARE_TEST(Fill, Blt);
            DECLARE_TEST(Fill, BltFast);
            DECLARE_TEST(Fill, AlphaBlt);
            
            DECLARE_TEST(ColorKey, Blt);
            DECLARE_TEST(ColorKey, BltFast);
            DECLARE_TEST(ColorKey, AlphaBlt);
            
            DECLARE_TEST(Overlay, Blt);
            DECLARE_TEST(Overlay, BltFast);
            DECLARE_TEST(Overlay, AlphaBlt);
            
            DECLARE_TEST(ColorKeyOverlay, Blt);
            DECLARE_TEST(ColorKeyOverlay, BltFast);
            DECLARE_TEST(ColorKeyOverlay, AlphaBlt);
            
            #undef DECLARE_TEST
        };
    };
};

#endif // header protection
