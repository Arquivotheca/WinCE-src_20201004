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

#include "DDrawUty.h"

#include "DDTestKit_Modes.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace TestKit_DDraw_Modes
{
    void PrintDriverName(void)
    {
        TCHAR szDriverName[256];
        DWORD dwTmp=countof(szDriverName);

        if(g_DDConfig.GetDriverName(szDriverName, &dwTmp))        
            dbgout << "Video driver in use is " << szDriverName << endl;
        else dbgout << "No driver name in registry." << endl;

    } 
    eTestResult CTestKit_NoMode::ConfigDirectDraw()
    {
        PrintDriverName();
        dbgout << "Not setting cooperative level" << endl;
        g_DirectDraw.SetCooperativeLevel(0);
        m_piDD = g_DirectDraw.GetObject();
        m_hWnd = g_DirectDraw.m_hWnd;

        if(!m_piDD && DDERR_UNSUPPORTED == g_DirectDraw.GetLastError())
            return trSkip;
        else CheckCondition(!m_piDD, "Get global DDraw Object", trFail);
        
        return trPass;
    }
    
    eTestResult CTestKit_NormalMode::ConfigDirectDraw()
    {
        PrintDriverName();
        dbgout << "Using NORMAL Cooperative Level" << endl;
        g_DirectDraw.SetCooperativeLevel(DDSCL_NORMAL);
        m_piDD = g_DirectDraw.GetObject();
        m_hWnd = g_DirectDraw.m_hWnd;
        
        if(!m_piDD && DDERR_UNSUPPORTED == g_DirectDraw.GetLastError())
            return trSkip;
        else CheckCondition(!m_piDD, "Get global DDraw Object", trFail);
        
        return trPass;
    }
    
    eTestResult CTestKit_ExclusiveMode::ConfigDirectDraw()
    {
        PrintDriverName();
        dbgout << "Using EXCLUSIVE Cooperative Level" << endl;
        g_DirectDraw.SetCooperativeLevel(DDSCL_FULLSCREEN);
        m_piDD = g_DirectDraw.GetObject();
        m_hWnd = g_DirectDraw.m_hWnd;
        
        if(!m_piDD && DDERR_UNSUPPORTED == g_DirectDraw.GetLastError())
            return trSkip;
        else CheckCondition(!m_piDD, "Get global DDraw Object", trFail);
        
        return trPass;
    }
}

