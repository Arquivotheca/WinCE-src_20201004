#pragma once
#ifndef __DDTEST_BASE_H__
#define __DDTEST_BASE_H__

#include <com_utilities.h>
#include <QATestUty/TestUty.h>

#include "DDrawUty_Types.h"

namespace DDrawBaseClasses
{
    class CTest_IDirectDraw : virtual public TestUty::CTest
    {
    public:
        CTest_IDirectDraw() { m_nKnownBugNum = 0; }

        // CTest Overrides
        virtual eTestResult Test() { return RunDDrawTest(); }
        //virtual void HandleStructuredException(DWORD dwExceptionCode);

    protected:
        // DDraw Specific Methods
        virtual eTestResult RunDDrawTest();

        virtual eTestResult ConfigDirectDraw() { return trPass; }
        virtual eTestResult TestIDirectDraw() = 0;
        virtual eTestResult VerifyDirectDraw() { return trPass; }
    
        com_utilities::CComPointer<IDirectDraw> m_piDD;

        long m_nKnownBugNum;
    };

    class CTest_IDirectDrawSurface : virtual public CTest_IDirectDraw
    {
    public:
        CTest_IDirectDrawSurface() : m_dwSurfCategories(scAllSurfaces) { };
        
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
            scDCPrimary     = 0x0100,       // Use GetDC or CreateDC
            scDCOffscreen   = 0x0200,       // Use CreateCompatDC and CreateCompatBitmap
            scOffScrSysOwnDc = 0x0400,       // Get called for offscreen surfaces in system memory

            scPrimBack      = 0x0003,       // Primary and backbuffers only
            scAllSurfaces   = 0x07BF        // All surfaces
        };

        virtual eTestResult ConfigDirectDrawSurface() { return trPass; }
        virtual eTestResult TestIDirectDrawSurface() = 0;
        virtual eTestResult VerifyDirectDrawSurface() { return trPass; }
        
        DWORD m_dwSurfCategories;
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDS;
        DDrawUty::CDDSurfaceDesc m_cddsd;
    };

    class CTest_IDirectDrawSurface_TWO : virtual public CTest_IDirectDrawSurface
    {
    public:
        CTest_IDirectDrawSurface_TWO() : m_dwSurfCategoriesDst(scAllSurfaces) { };

    protected:                                                       
        virtual eTestResult RunSurfaceTest_TWO();
        
        virtual eTestResult ConfigDirectDrawSurface_TWO() { return trPass; }
        virtual eTestResult TestIDirectDrawSurface_TWO() = 0;
        virtual eTestResult VerifyDirectDrawSurface_TWO() { return trPass; }
        
        DWORD m_dwSurfCategoriesDst;
        com_utilities::CComPointer<IDirectDrawSurface> m_piDDSDst;
        DDrawUty::CDDSurfaceDesc m_cddsdDst;
    };

    class CTest_IDDVideoPortContainer : virtual public CTest_IDirectDraw
    {
    protected:
        // DDVideoPortContainer Specific Methods
        virtual eTestResult RunVideoPortContainerTest();
        
        virtual eTestResult ConfigVideoPortContainer() { return trPass; }
        virtual eTestResult TestIDDVideoPortContainer() = 0;
        virtual eTestResult VerifyVideoPortContainer() { return trPass; }
        com_utilities::CComPointer<IDDVideoPortContainer> m_piDDVPC;
    };

    class CTest_IDirectDrawVideoPort : virtual public CTest_IDDVideoPortContainer
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

