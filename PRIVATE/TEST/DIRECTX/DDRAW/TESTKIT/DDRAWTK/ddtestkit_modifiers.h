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
#if !defined(__DDTEST_MODIFIERS_H__)
#define __DDTEST_MODIFIERS_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTestKit_Base.h"

namespace TestKit_Surface_Modifiers
{
    // Multiple Surface Modifiers
    class CTestKit_Fill : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface_TWO
    {
    public:
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
    };
    
    class CTestKit_ColorKey : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface_TWO
    {
    public:
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
    };

    class CTestKit_Overlay : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface_TWO
    {
        typedef TestKit_Surface_Modifiers::CTestKit_Fill base_class;
        
    public:
        CTestKit_Overlay() {}
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
        
    private:
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDSOverlay;
    };

    class CTestKit_ColorKeyOverlay : virtual public TestKit_Surface_Modifiers::CTestKit_Overlay
    {
    public:
        virtual eTestResult ConfigDirectDrawSurface();
        virtual eTestResult ConfigDirectDrawSurface_TWO();
    };
};

#endif // header protection

