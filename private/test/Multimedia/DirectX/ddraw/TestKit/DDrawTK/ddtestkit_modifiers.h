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
#if !defined(__DDTEST_MODIFIERS_H__)
#define __DDTEST_MODIFIERS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTestKit_Base.h"

namespace TestKit_Surface_Modifiers
{
    namespace Surface_Fill_Helpers
    {
        DWORD SetColorVal(double dwRed, double dwGreen,double dwBlue, IDirectDrawSurface *piDDS);
        eTestResult FillSurfaceVertical(IDirectDrawSurface *piDDS, DWORD dwColor1, DWORD dwColor2);
    }
}

namespace TestKit_Surface_Modifiers
{
    namespace Surface_Fill_Helpers
    {
        
        DWORD PackYUV(DWORD dwFourCC, DWORD Y1, DWORD Y2, DWORD U, DWORD V);
        void UnpackYUV(DWORD dwFourCC, DWORD dwColor, DWORD * pY1, DWORD * pY2, DWORD * pU, DWORD * pV);
        HRESULT SetPlanarPixel(const DDrawUty::CDDSurfaceDesc * pcddsd, int iX, int iY, DWORD dwColor);
        BOOL IsPlanarFourCC(DWORD dwFourCC);
    }

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

