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
#if !defined(__IDDVIDEOPORTCONTAINERTESTS_H__)
#define __IDDVIDEOPORTCONTAINERTESTS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDTestKit_Base.h>

namespace Test_IDDVideoPortContainer
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawTestKitBaseClasses::CTestKit_IDDVideoPortContainer \
    { \
    public: \
        virtual eTestResult TestIDDVideoPortContainer(); \
    }
    
    DECLARE_TEST(CreateVideoPort);
    DECLARE_TEST(EnumVideoPorts);
    DECLARE_TEST(GetVideoPortConnectInfo);
    DECLARE_TEST(QueryVideoPortStatus);
    
    #undef DECLARE_TEST
};

#endif // header protection

