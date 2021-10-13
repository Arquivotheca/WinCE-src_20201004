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

    eTestResult CTestKit_DevModeChange::RunDevModeChangeTest()
    {
        eTestResult tr = trPass;

        tr |= ConfigDevModeChange();
        if (trNotImplemented == tr)
            return trPass;
        if (trPass != tr)
            return tr;

        tr |= TestDevModeChange(m_oldDevMode, m_newDevMode);

        tr |= VerifyConfigDevModeChange();

        return tr;
    }
}
