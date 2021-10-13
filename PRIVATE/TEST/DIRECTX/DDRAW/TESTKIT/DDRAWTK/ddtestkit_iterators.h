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
#if !defined(__DDTESTKIT_ITERATORS_H__)
#define __DDTESTKIT_ITERATORS_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTestKit_Base.h"

namespace TestKit_Surface_Iterators
{
    class CIterateTKSurfaces : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface
    {
        
    public:
        // Base class overrides
        virtual eTestResult TestIDirectDraw();

        // Iterator specific methods
        virtual eTestResult PreSurfaceTest() { return trPass; }
        virtual eTestResult PostSurfaceTest() { return trPass; }

    protected:
        typedef std::pair<DDrawUty::CfgSurfaceType, DDrawUty::CfgPixelFormat> SurfPixPair;
        typedef std::vector<SurfPixPair> vectSurfPixPair;
        typedef vectSurfPixPair::const_iterator vectSurfPixPair_itr;

        vectSurfPixPair m_vectPairs;
    };
    
    class CIterateTKSurfaces_TWO : virtual public DDrawTestKitBaseClasses::CTestKit_IDirectDrawSurface_TWO,
                                 virtual public TestKit_Surface_Iterators::CIterateTKSurfaces
    {
    public:
        virtual eTestResult TestIDirectDrawSurface();
        
    protected:
    	typedef std::pair<DDrawUty::CfgSurfaceType, DDrawUty::CfgPixelFormat> SurfPixPair;
        typedef std::vector<SurfPixPair> vectSurfPixPair;
        typedef vectSurfPixPair::const_iterator vectSurfPixPair_itr;

        vectSurfPixPair m_vectPairsDst;
    };
};

#endif // header protection

