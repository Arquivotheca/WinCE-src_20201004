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
#if !defined(__DDTEST_MODIFIERS_H__)
#define __DDTEST_MODIFIERS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTest_Base.h"

namespace Test_Surface_Modifiers
{
    // Single Surface Modifiers

    // Multiple Surface Modifiers
    class CTest_Fill : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface_TWO
    {
    public:
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
    };
    
    class CTest_ColorKey : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface_TWO
    {
    public:
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
    };

    class CTest_Overlay : virtual public Test_Surface_Modifiers::CTest_Fill
    {
        typedef Test_Surface_Modifiers::CTest_Fill base_class;
        
    public:
        CTest_Overlay() { m_dwSurfCategoriesDst = scPrimary; }
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
        virtual eTestResult VerifyDirectDrawSurface_TWO();
        
    private:
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDSOverlay;
    };

    class CTest_ColorKeyOverlay : virtual public Test_Surface_Modifiers::CTest_ColorKey,
                                  virtual public Test_Surface_Modifiers::CTest_Overlay
    {
    public:
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
    };
};

#endif // header protection

