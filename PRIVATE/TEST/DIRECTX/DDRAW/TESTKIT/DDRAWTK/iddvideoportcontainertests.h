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
#if !defined(__IDDVIDEOPORTCONTAINERTESTS_H__)
#define __IDDVIDEOPORTCONTAINERTESTS_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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

