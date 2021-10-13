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
#if !defined(__IDDVIDEOPORTTESTS_H__)
#define __IDDVIDEOPORTTESTS_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <QATestUty/TestUty.h>
#include <DDTestKit_Base.h>

namespace Test_IDirectDrawVideoPort
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawVideoPort \
    { \
    public: \
        virtual eTestResult TestIDirectDrawVideoPort(); \
    }
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
};

#endif // header protection

