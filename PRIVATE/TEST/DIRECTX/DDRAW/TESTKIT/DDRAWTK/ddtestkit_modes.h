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
#if !defined(__DDTEST_MODES_H__)
#define __DDTEST_MODES_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTestKit_Base.h"

namespace TestKit_DDraw_Modes
{
    #define DECLARE_TEST(MODE) \
    class CTestKit_##MODE : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDraw \
    { \
    public: \
        virtual eTestResult ConfigDirectDraw(); \
    };

    DECLARE_TEST(NoMode);
    DECLARE_TEST(NormalMode);
    DECLARE_TEST(ExclusiveMode);
    
    #undef DECLARE_TEST
};

#endif // header protection

