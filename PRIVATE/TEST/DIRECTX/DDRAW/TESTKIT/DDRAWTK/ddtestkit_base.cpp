//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#include "main.h"

#include <DebugStreamUty.h>

#include "DDTestKit_Base.h"

namespace DDrawTestKitBaseClasses
{
    eTestResult CTestKit_IDirectDraw::RunDDrawTest()
    {
        eTestResult tr = trPass;

        tr |= ConfigDirectDraw();
        if (tr == trNotImplemented)
            return trPass;
        if (tr != trPass)
            return tr;

        tr |= TestIDirectDraw();

        tr |= VerifyDirectDraw();

        return tr;
    }

    eTestResult CTestKit_IDirectDrawSurface::RunSurfaceTest()
    {
        eTestResult tr = trPass;

        tr |= ConfigDirectDrawSurface();
        if (tr == trNotImplemented)
            return trPass;
        if (tr != trPass)
            return tr;

        tr |= TestIDirectDrawSurface();

        tr |= VerifyDirectDrawSurface();

        return tr;
    }
    
    eTestResult CTestKit_IDirectDrawSurface_TWO::RunSurfaceTest_TWO()
    {
        eTestResult tr = trPass;

        tr |= ConfigDirectDrawSurface_TWO();
        if (tr == trNotImplemented)
            return trPass;
        if (tr != trPass)
            return tr;

        tr |= TestIDirectDrawSurface_TWO();

        tr |= VerifyDirectDrawSurface_TWO();

        return tr;
    }

    eTestResult CTestKit_IDDVideoPortContainer::RunVideoPortContainerTest()
    {
        eTestResult tr = trPass;

        tr |= ConfigVideoPortContainer();
        if (tr == trNotImplemented)
            return trPass;
        if (tr != trPass)
            return tr;

        tr |= TestIDDVideoPortContainer();

        tr |= VerifyVideoPortContainer();

        return tr;
    }
    
    eTestResult CTestKit_IDirectDrawVideoPort::RunVideoPortTest()
    {
        eTestResult tr = trPass;

        tr |= ConfigDirectDrawVideoPort();
        if (tr == trNotImplemented)
            return trPass;
        if (tr != trPass)
            return tr;

        tr |= TestIDirectDrawVideoPort();

        tr |= VerifyDirectDrawVideoPort();

        return tr;
    }

}
