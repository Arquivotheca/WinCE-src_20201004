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
#pragma once
#if !defined(__DDTEST_MODES_H__)
#define __DDTEST_MODES_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTest_Base.h"

namespace Test_DDraw_Modes
{
    #define DECLARE_TEST(MODE) \
    class CTest_##MODE : virtual public DDrawBaseClasses::CTest_IDirectDraw \
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

