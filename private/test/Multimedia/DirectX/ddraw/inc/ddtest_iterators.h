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
#if !defined(__DDTEST_ITERATORS_H__)
#define __DDTEST_ITERATORS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>

#include "DDrawUty_Config.h"
#include "DDTest_Base.h"

namespace Test_DDraw_Iterators
{
    class CIterateDDraw : virtual public DDrawBaseClasses::CTest_IDirectDraw
    {
        typedef std::vector<DDrawUty::DISPLAYMODE> vectDispMode;
        typedef vectDispMode::const_iterator vectDispMode_itr;

    public:
        // Base class overrides
        virtual eTestResult Test();

        // Iterator specific methods
        virtual eTestResult PreDDrawTest() { return trPass; }
        virtual eTestResult PostDDrawTest() { return trPass; }

    protected:
        vectDispMode m_vectDispMode;
    };
};

namespace Test_Surface_Iterators
{
    class CIterateSurfaces : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface
    {
    protected:
        typedef std::pair<DDrawUty::CfgSurfaceType, DDrawUty::CfgPixelFormat> SurfPixPair;
        typedef std::vector<SurfPixPair> vectSurfPixPair;
        typedef vectSurfPixPair::const_iterator vectSurfPixPair_itr;
        
    public:
        // Base class overrides
        virtual eTestResult TestIDirectDraw();

        // Iterator specific methods
        virtual eTestResult PreSurfaceTest() { return trPass; }
        virtual eTestResult PostSurfaceTest() { return trPass; }

    protected:
        vectSurfPixPair m_vectPairs;
    };
    
    class CIterateSurfaces_TWO : virtual public DDrawBaseClasses::CTest_IDirectDrawSurface_TWO,
                                 virtual public Test_Surface_Iterators::CIterateSurfaces
    {
    protected:
        typedef std::pair<DDrawUty::CfgSurfaceType, DDrawUty::CfgPixelFormat> SurfPixPair;
        typedef std::vector<SurfPixPair> vectSurfPixPair;
        typedef vectSurfPixPair::const_iterator vectSurfPixPair_itr;
        
    public:
        virtual eTestResult TestIDirectDrawSurface();
        
    protected:
        vectSurfPixPair m_vectPairsDst;
    };
};

#endif // header protection

