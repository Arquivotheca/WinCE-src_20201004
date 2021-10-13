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

    };

    class CTestKit_IDirectDrawSurface : virtual public CTestKit_IDirectDraw
    {
    public:
        CTestKit_IDirectDrawSurface() : m_dwSurfCategories(scAllSurfaces) { };
        
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

            scPrimBack      = 0x0003,       // Primary and backbuffers only
            scAllSurfaces   = 0x003F        // All surfaces
        };

        virtual eTestResult ConfigDirectDrawSurface() { return trPass; }
        virtual eTestResult TestIDirectDrawSurface() = 0;
        virtual eTestResult VerifyDirectDrawSurface() { return trPass; }
        
        DWORD m_dwSurfCategories;
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDS;
        DDrawUty::CDDSurfaceDesc m_cddsd;
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
    };
};

#endif  // header protection

