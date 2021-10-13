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
#ifndef __DDTEST_BASE_H__
#define __DDTEST_BASE_H__

#include <com_utilities.h>
#include <QATestUty/TestUty.h>

#include "DDrawUty_Types.h"

namespace DDrawTestKitBaseClasses
{
    class CTestKit_IDirectDraw : virtual public TestUty::CTest
    {
    public:
        CTestKit_IDirectDraw() { }

        // CTest Overrides
        virtual eTestResult Test() { return RunDDrawTest(); }

    protected:
        // DDraw Specific Methods
        virtual eTestResult RunDDrawTest();

        virtual eTestResult ConfigDirectDraw() { return trPass; }
        virtual eTestResult TestIDirectDraw() = 0;
        virtual eTestResult VerifyDirectDraw() { return trPass; }

        com_utilities::CComPointer<IDirectDraw> m_piDD;
        HWND m_hWnd;

    };

    class CTestKit_IDirectDrawSurface : virtual public CTestKit_IDirectDraw
    {
    public:
        CTestKit_IDirectDrawSurface() : m_dwSurfCategories(scAllSurfaces), m_hdc(NULL) { };

    protected:
        virtual eTestResult RunSurfaceTest();

        // Flags indicating which categories of surfaces this test
        // is valid for
        enum SurfCategories
        {
            // Categories
            scPrimary       = 0x0001,       // Get called for all flavors of primary (0 - 3 backbuffers)
            scBackbuffer    = 0x0002,       // NYI
            scOffScrVid     = 0x0004,       // Get called for offscreen surfaces in video memory
            scOffScrSys     = 0x0008,       // Get called for offscreen surfaces in system memory
            scOverlay       = 0x0010,       // Get called for overlay surfaces
            scZBuffer       = 0x0020,       // NYI
            scNull          = 0x0040,       // Get called with a NULL pointer once
            scOffScrSysOwnDc = 0x0080,       // Get called for offscreen surfaces in system memory

            scPrimBack      = 0x0003,       // Primary and backbuffers only
            scFlippable     = 0x0011,       // Primary and Overlay only
            scAllSurfaces   = 0x00BF        // All surfaces
        };

        virtual eTestResult ConfigDirectDrawSurface() { return trPass; }
        virtual eTestResult TestIDirectDrawSurface() = 0;
        virtual eTestResult VerifyDirectDrawSurface() { return trPass; }

        DWORD m_dwSurfCategories;
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDS;
        DDrawUty::CDDSurfaceDesc m_cddsd;
        HDC m_hdc;
    };

    class CTestKit_IDirectDrawSurface_TWO : virtual public CTestKit_IDirectDrawSurface
    {
    public:
        CTestKit_IDirectDrawSurface_TWO() : m_dwSurfCategoriesDst(scAllSurfaces) { };

    protected:
        virtual eTestResult RunSurfaceTest_TWO();

        virtual eTestResult ConfigDirectDrawSurface_TWO() { return trPass; }
        virtual eTestResult TestIDirectDrawSurface_TWO() = 0;
        virtual eTestResult VerifyDirectDrawSurface_TWO() { return trPass; }

        DWORD m_dwSurfCategoriesDst;
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDSDst;
        DDrawUty::CDDSurfaceDesc m_cddsdDst;
    };

    class CTestKit_IDDVideoPortContainer : virtual public CTestKit_IDirectDraw
    {
    protected:
        // DDVideoPortContainer Specific Methods
        virtual eTestResult RunVideoPortContainerTest();

        virtual eTestResult ConfigVideoPortContainer() { return trPass; }
        virtual eTestResult TestIDDVideoPortContainer() = 0;
        virtual eTestResult VerifyVideoPortContainer() { return trPass; }

        com_utilities::CComPointer<IDDVideoPortContainer> m_piDDVPC;
    };

    class CTestKit_IDirectDrawVideoPort : virtual public CTestKit_IDDVideoPortContainer
    {
    public:
        enum eVideoPortState
        {
            vpsNoSurface,
            vpsNoSignal,
            vpsSignalStarted,
            vpsSignalStopped
        };

    protected:
        // VideoPort Specific Methods
        virtual eTestResult RunVideoPortTest();

        virtual eTestResult ConfigDirectDrawVideoPort() { return trPass; }
        virtual eTestResult TestIDirectDrawVideoPort() = 0;
        virtual eTestResult VerifyDirectDrawVideoPort() { return trPass; }

        com_utilities::CComPointer<IDirectDrawVideoPort> m_piDDVP;
        DDrawUty::CDDVideoPortCaps m_cddvpcaps;
        DDrawUty::CDDSurfaceDesc m_cddsdOverlay;
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDSOverlay;
        eVideoPortState m_State;
        DWORD m_dwInputFourCC;
        DWORD m_dwOutputFourCC;
    };

    class CTestKit_DevModeChange : virtual public CTestKit_IDirectDrawSurface
    {
    public:
        CTestKit_DevModeChange() : m_dwDevModeChangesCategory(dmcAllPossibilities) { };

    protected: // Enums
        // Flags indicating which categories of dev mode changes will be applied.
        enum DevModeChangeCategories
        {
            // Categories
            dmcAllPossibilities      = 0x0000,  // All possible DevMode change 
                                                // combinations will be applied.
            dmcSequentialChange      = 0x0001   // Move sequentially from the 
                                                // first dev mode to the last one.
            
        };

    protected: // Functions
        virtual eTestResult RunDevModeChangeTest();

        virtual eTestResult ConfigDevModeChange() { return trPass; }
        virtual eTestResult TestDevModeChange(DEVMODE& dmOldDevMode, DEVMODE& dmNewDevMode) = 0;
        virtual eTestResult VerifyConfigDevModeChange() { return trPass; }

    protected: // Data
        DWORD m_dwDevModeChangesCategory;
        DEVMODE m_oldDevMode;
        DEVMODE m_newDevMode;
    };
};

#endif  // header protection

