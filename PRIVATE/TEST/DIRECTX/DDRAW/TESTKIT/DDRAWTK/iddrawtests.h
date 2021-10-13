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
#if !defined(__IDDRAWTESTS_H__)
#define __IDDRAWTESTS_H__

// External Dependencies
// »»»»»»»»»»»»»»»»»»»»»
#include <QATestUty/TestUty.h>
#include <DDTestKit_Base.h>

namespace Test_IDirectDraw
{
    #define DECLARE_TEST(METHODNAME) \
    class CTest_##METHODNAME : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDraw \
    { \
    public: \
        virtual eTestResult TestIDirectDraw(); \
    }

    DECLARE_TEST(GetCaps);

    class CTest_EnumDisplayModes : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDraw
    {
    protected:
        DWORD m_dwPreset;
        eTestResult m_tr;
        int m_nCallbackCount;
        bool m_fCallbackExpected;

    public:
        CTest_EnumDisplayModes() :
            m_tr(trPass),
            m_nCallbackCount(0),
            m_fCallbackExpected(false)
        {
            PresetUty::Preset(m_dwPreset);
        }

        virtual eTestResult TestIDirectDraw();
        static HRESULT WINAPI Callback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext);
    };

    #undef DECLARE_TEST
};

#endif // header protection

