//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#pragma once
#if !defined(__IDDSURFACETESTS_H__)
#define __IDDSURFACETESTS_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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

    DECLARE_TEST(Flip, scPrimary);
    DECLARE_TEST(FlipOverlay, scOverlay);
    DECLARE_TEST(ColorFillBlts, scAllSurfaces);
    
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
    
    #undef DECLARE_TEST
};


#endif // header protection


