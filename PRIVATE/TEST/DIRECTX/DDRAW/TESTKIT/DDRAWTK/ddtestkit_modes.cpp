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

        if(!m_piDD && DDERR_NODIRECTDRAWSUPPORT == g_DirectDraw.GetLastError())
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
        
        if(!m_piDD && DDERR_NODIRECTDRAWSUPPORT == g_DirectDraw.GetLastError())
            return trSkip;
        else CheckCondition(!m_piDD, "Get global DDraw Object", trFail);
        
        return trPass;
    }
    
    eTestResult CTestKit_ExclusiveMode::ConfigDirectDraw()
    {
        PrintDriverName();
        dbgout << "Using EXCLUSIVE Cooperative Level" << endl;
        g_DirectDraw.SetCooperativeLevel(DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);
        m_piDD = g_DirectDraw.GetObject();
        
        if(!m_piDD && DDERR_NODIRECTDRAWSUPPORT == g_DirectDraw.GetLastError())
            return trSkip;
        else CheckCondition(!m_piDD, "Get global DDraw Object", trFail);
        
        return trPass;
    }
}

