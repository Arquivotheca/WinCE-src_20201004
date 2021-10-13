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
#if !defined(__IDDSURFACETESTS_H__)
#define __IDDSURFACETESTS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDTestKit_Base.h>

namespace Test_IDirectDrawSurface
{
    #define DECLARE_TEST(METHODNAME, SURFACES) \
    class CTest_##METHODNAME : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface \
    { \
    public: \
        CTest_##METHODNAME() { m_dwSurfCategories = SURFACES; } \
        virtual eTestResult TestIDirectDrawSurface(); \
    }

    DECLARE_TEST(Flip, scFlippable);
    DECLARE_TEST(FlipInterlaced, scFlippable);
    DECLARE_TEST(ColorFillBlts, scAllSurfaces);
    DECLARE_TEST(SetGetPixel, scAllSurfaces);

    #undef DECLARE_TEST
};

namespace Test_IDirectDrawSurface_TWO
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface_TWO \
    { \
    public: \
        virtual eTestResult TestIDirectDrawSurface_TWO(); \
    }

    DECLARE_TEST(Blt);
    DECLARE_TEST(InteractiveBlt);
    DECLARE_TEST(InteractiveOverlayBlt);
    DECLARE_TEST(InteractiveColorFillOverlayBlts);
    DECLARE_TEST(InteractiveInterlacedOverlayFlip);

    #undef DECLARE_TEST
};

namespace Test_DeviceModeChange
{
    #define DECLARE_TEST(METHODNAME, SURFACES, DEVMODES) \
    class CTest_##METHODNAME : virtual public DDrawTestKitBaseClasses::CTestKit_DevModeChange \
    { \
    public: \
        CTest_##METHODNAME() { m_dwSurfCategories = SURFACES; m_dwDevModeChangesCategory = DEVMODES; } \
        virtual eTestResult TestDevModeChange(DEVMODE& dmOldDevMode, DEVMODE& dmCurrentDevMode); \
    }

    DECLARE_TEST(GetSurfaceDesc, scAllSurfaces, dmcSequentialChange);

    #undef DECLARE_TEST
};

#endif // header protection


