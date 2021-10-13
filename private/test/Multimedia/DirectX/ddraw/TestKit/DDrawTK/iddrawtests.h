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
#if !defined(__IDDRAWTESTS_H__)
#define __IDDRAWTESTS_H__

// External Dependencies
// ---------------------
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
    DECLARE_TEST(VBIDSurfaceCreation);
    DECLARE_TEST(VBIDSurfaceData);
    DECLARE_TEST(CreateSurfaceVerification);

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

