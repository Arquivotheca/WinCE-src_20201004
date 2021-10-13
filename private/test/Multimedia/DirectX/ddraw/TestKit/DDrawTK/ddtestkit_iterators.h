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
#if !defined(__DDTESTKIT_ITERATORS_H__)
#define __DDTESTKIT_ITERATORS_H__

// External Dependencies
// ---------------------
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

    class CIterateDevModes : virtual public DDrawTestKitBaseClasses::CTestKit_DevModeChange,
                             virtual public TestKit_Surface_Iterators::CIterateTKSurfaces
    {
    public:
        // Base class overrides
        virtual eTestResult TestIDirectDrawSurface();

        // Iterator specific methods
        virtual eTestResult PreDevModeChangeTest() { return trPass; }
        virtual eTestResult PostDevModeChangeTest() { return trPass; }
    private:
        DWORD GetSurfaceCategory(DWORD dwSurfaceCaps);
    };
};
#endif // header protection

