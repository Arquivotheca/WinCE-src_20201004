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

#pragma once
#if !defined(__DDRAWBVT_BASICS_H__)
#define __DDRAWBVT_BASICS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDrawUty_Config.h>
#include <DDTestKit_Base.h>
#include <DDTestKit_Modes.h>
#include <DDTestKit_Iterators.h>
#include <DDTestKit_Modifiers.h>

#include "IDDrawTests.h"
#include "IDDSurfaceTests.h"
#include "IDDVideoPortTests.h"
#include "IDDVideoPortContainerTests.h"
#include "ddtestkit_iterators.h"

#if defined(DEBUG)
#   define CFG_TEST_INVALID_PARAMETERS  1
#else
#   define CFG_TEST_INVALID_PARAMETERS  0
#endif

namespace Test_Surface_Sets
{
    // Single Surface Sets
    class CTest_Basics : virtual public TestKit_Surface_Iterators::CIterateTKSurfaces
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };

    // Multiple Surface Sets
    class CTest_SamePixel : virtual public TestKit_Surface_Iterators::CIterateTKSurfaces_TWO
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };
    class CTest_SamePixelInteractive: virtual public TestKit_Surface_Iterators::CIterateTKSurfaces_TWO
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };
    class CTest_OverlayInteractive: virtual public TestKit_Surface_Iterators::CIterateTKSurfaces_TWO
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };

    // DevMode change Surface Sets
    class CTest_DevModeChange : virtual public Test_Surface_Sets::CTest_Basics,
                                virtual public TestKit_Surface_Iterators::CIterateDevModes
    {
    public:
        // Base class over-rides

        // From TestKit_Surface_Iterators::CIterateDevModes
        virtual eTestResult PreDevModeChangeTest();
        virtual eTestResult PostDevModeChangeTest();
    };
};

namespace Test_Normal_Mode
{
    namespace Test_DirectDraw
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
            virtual public Test_IDirectDraw::CTest_##METHODNAME { }

        DECLARE_TEST(EnumDisplayModes);
        DECLARE_TEST(GetCaps);
        DECLARE_TEST(VBIDSurfaceCreation);
        DECLARE_TEST(VBIDSurfaceData);
        DECLARE_TEST(CreateSurfaceVerification);

        #undef DECLARE_TEST
    };

    namespace Test_DirectDrawSurface
    {

        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
                                   virtual public Test_Surface_Sets::CTest_Basics, \
                                   virtual public Test_IDirectDrawSurface::CTest_##METHODNAME { }

        DECLARE_TEST(ColorFillBlts);
        DECLARE_TEST(Flip);
        DECLARE_TEST(FlipInterlaced);
        DECLARE_TEST(SetGetPixel);

        #undef DECLARE_TEST
    };

    namespace Test_DirectDrawSurface_TWO
    {
        #define DECLARE_TEST(MODIFIER, METHODNAME) \
        class CTest_##MODIFIER##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
                                             virtual public Test_Surface_Sets::CTest_SamePixel, \
                                             virtual public TestKit_Surface_Modifiers::CTestKit_##MODIFIER, \
                                             virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }

        DECLARE_TEST(Fill, Blt);
        DECLARE_TEST(ColorKey, Blt);

        #undef DECLARE_TEST
    };
    namespace Test_DirectDrawSurface_TWO
    {
        #define DECLARE_TEST(MODIFIER, METHODNAME) \
        class CTest_##MODIFIER##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
                                             virtual public Test_Surface_Sets::CTest_SamePixelInteractive, \
                                             virtual public TestKit_Surface_Modifiers::CTestKit_##MODIFIER, \
                                             virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }

        DECLARE_TEST(Fill, InteractiveBlt);

        #undef DECLARE_TEST
    };

    namespace Test_DirectDrawSurface_TWO
    {
        #define DECLARE_TEST(MODIFIER, METHODNAME) \
        class CTest_##MODIFIER##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
                                             virtual public Test_Surface_Sets::CTest_OverlayInteractive, \
                                             virtual public TestKit_Surface_Modifiers::CTestKit_##MODIFIER, \
                                             virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }

        DECLARE_TEST(Overlay, InteractiveOverlayBlt);
        DECLARE_TEST(Overlay, InteractiveColorFillOverlayBlts);
        DECLARE_TEST(ColorKeyOverlay, InteractiveOverlayBlt);
        DECLARE_TEST(Overlay, InteractiveInterlacedOverlayFlip);

        #undef DECLARE_TEST
    };

    namespace Test_DevModeChange
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
                                   virtual public Test_Surface_Sets::CTest_DevModeChange, \
                                   virtual public Test_DeviceModeChange::CTest_##METHODNAME { }

        DECLARE_TEST(GetSurfaceDesc);

        #undef DECLARE_TEST
    };
};

namespace Test_Exclusive_Mode
{
    namespace Test_DirectDrawSurface
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_ExclusiveMode, \
                                   virtual public Test_Surface_Sets::CTest_Basics, \
                                   virtual public Test_IDirectDrawSurface::CTest_##METHODNAME { }

        DECLARE_TEST(Flip);
        DECLARE_TEST(FlipInterlaced);
        DECLARE_TEST(ColorFillBlts);
        DECLARE_TEST(SetGetPixel);

        #undef DECLARE_TEST
    };

    namespace Test_DirectDrawSurface_TWO
    {
        #define DECLARE_TEST(MODIFIER, METHODNAME) \
        class CTest_##MODIFIER##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_ExclusiveMode, \
                                             virtual public Test_Surface_Sets::CTest_SamePixel, \
                                             virtual public TestKit_Surface_Modifiers::CTestKit_##MODIFIER, \
                                             virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }

        DECLARE_TEST(Fill, Blt);
        DECLARE_TEST(ColorKey, Blt);
        #undef DECLARE_TEST

    };
    namespace Test_DirectDrawSurface_TWO
    {
        #define DECLARE_TEST(MODIFIER, METHODNAME) \
        class CTest_##MODIFIER##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_ExclusiveMode, \
                                             virtual public Test_Surface_Sets::CTest_SamePixelInteractive, \
                                             virtual public TestKit_Surface_Modifiers::CTestKit_##MODIFIER, \
                                             virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }

        DECLARE_TEST(Fill, InteractiveBlt);
        #undef DECLARE_TEST

    };

    namespace Test_DirectDrawSurface_TWO
    {
        #define DECLARE_TEST(MODIFIER, METHODNAME) \
        class CTest_##MODIFIER##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_ExclusiveMode, \
                                             virtual public Test_Surface_Sets::CTest_OverlayInteractive, \
                                             virtual public TestKit_Surface_Modifiers::CTestKit_##MODIFIER, \
                                             virtual public Test_IDirectDrawSurface_TWO::CTest_##METHODNAME { }

        DECLARE_TEST(Overlay, InteractiveOverlayBlt);
        DECLARE_TEST(Overlay, InteractiveColorFillOverlayBlts);
        DECLARE_TEST(ColorKeyOverlay, InteractiveOverlayBlt);
        DECLARE_TEST(Overlay, InteractiveInterlacedOverlayFlip);

        #undef DECLARE_TEST

    };

    namespace Test_DevModeChange
    {
        #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : virtual public TestKit_DDraw_Modes::CTestKit_ExclusiveMode, \
                                   virtual public Test_Surface_Sets::CTest_DevModeChange, \
                                   virtual public Test_DeviceModeChange::CTest_##METHODNAME { }

        DECLARE_TEST(GetSurfaceDesc);

        #undef DECLARE_TEST
    };
}

namespace Test_DDVideoPortContainer
{
    class CTest_SimpleDDVideoPortContainer : virtual public DDrawTestKitBaseClasses::CTestKit_IDDVideoPortContainer
    {
    protected:
        virtual eTestResult TestIDirectDraw();
    };

#define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : \
        virtual public TestKit_DDraw_Modes::CTestKit_NormalMode, \
        virtual public Test_DDVideoPortContainer::CTest_SimpleDDVideoPortContainer, \
        virtual public Test_IDDVideoPortContainer::CTest_##METHODNAME { }

    DECLARE_TEST(CreateVideoPort);
    DECLARE_TEST(EnumVideoPorts);
    DECLARE_TEST(GetVideoPortConnectInfo);
    DECLARE_TEST(QueryVideoPortStatus);

#undef DECLARE_TEST
}

namespace Test_DirectDrawVideoPort
{
    class CTest_IterateVideoPortStates :  virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawVideoPort
    {
    protected:
        virtual eTestResult TestIDDVideoPortContainer();
    };

    namespace Test_StateIterators
    {
    #define DECLARE_TEST(METHODNAME) \
        class CTest_##METHODNAME : \
            virtual public TestKit_DDraw_Modes::CTestKit_ExclusiveMode, \
            virtual public Test_DDVideoPortContainer::CTest_SimpleDDVideoPortContainer, \
            virtual public Test_DirectDrawVideoPort::CTest_IterateVideoPortStates, \
            virtual public Test_IDirectDrawVideoPort::CTest_##METHODNAME { }

        DECLARE_TEST(GetBandwidthInfo);
        DECLARE_TEST(GetSetColorControls);
        DECLARE_TEST(GetInputOutputFormats);
        DECLARE_TEST(GetFieldPolarity);
        DECLARE_TEST(GetVideoLine);
        DECLARE_TEST(GetVideoSignalStatus);
        DECLARE_TEST(SetTargetSurface);
        DECLARE_TEST(StartVideo);
        DECLARE_TEST(StopVideo);
        DECLARE_TEST(UpdateVideo);
        DECLARE_TEST(WaitForSync);

    #undef DECLARE_TEST
    }

}



#endif // header protection
