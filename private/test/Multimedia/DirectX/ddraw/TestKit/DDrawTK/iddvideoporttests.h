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
#if !defined(__IDDVIDEOPORTTESTS_H__)
#define __IDDVIDEOPORTTESTS_H__

// External Dependencies
// ---------------------
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

