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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//

#include "main.h"

#include <QATestUty/WinApp.h>
#include <QATestUty/TuxDLLUty.h>
#include <DebugStreamUty.h>
#include <DDrawUty.h>
#include "TuxFunctionTable.h"
#include <vector>

DEFINE_DllMain;
DEFINE_ShellProc;

typedef ITuxDLL::eShellProcReturnCode eShellProcReturnCode;

class CDDrawTK : public DDrawUty::CDDrawTuxDLL
{
    typedef DDrawUty::CDDrawTuxDLL base_class;
    
public:
    CDDrawTK() : base_class(g_lpFTE, false) 
    {
        m_pszModuleName = TEXT("DDrawTK.DLL");
    }

    virtual eShellProcReturnCode Load(OUT bool& fUnicode);
    virtual eShellProcReturnCode Unload();

    virtual void ProcessCommandLine(LPCTSTR pszDLLCmdLine);

private:
    std::vector<HWND> vHiddenWindows;

    static BOOL CALLBACK HideWindows(HWND hwnd, LPARAM param);
};

CDDrawTK g_Module;

BOOL CALLBACK CDDrawTK::HideWindows(HWND hwnd, LPARAM param)
{
    LONG lWindowStyle;
    CDDrawTK * pThis = (CDDrawTK*)param;

    if (hwnd == pThis->m_hWnd)
    {
        return TRUE;
    }
    
    lWindowStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (!(lWindowStyle & WS_VISIBLE))
    {
        return TRUE;
    }
    ShowWindow(hwnd, SW_HIDE);

    pThis->vHiddenWindows.push_back(hwnd);
    
    return TRUE;
}

eShellProcReturnCode CDDrawTK::Load(bool& fUnicode)
{
    eShellProcReturnCode sprc = CDDrawTuxDLL::Load(fUnicode);

    // Hide all the visible windows. This is so that during our windowed tests, any other windows
    // (such as the ClientSide.exe window) do not invalidate our comparisons.
    EnumWindows((WNDENUMPROC)HideWindows, (LPARAM)this);
    return sprc;
}

eShellProcReturnCode CDDrawTK::Unload()
{
    // Show all the windows we hid.
    for(std::vector<HWND>::iterator iter = vHiddenWindows.begin(); iter != vHiddenWindows.end(); iter++)
        ShowWindow(*iter, SW_SHOW);
    return CDDrawTuxDLL::Unload();
}

void CDDrawTK::ProcessCommandLine(LPCTSTR pszDLLCmdLine)
{
    DDrawUty::g_DDConfig.SetSymbol(TEXT("i"), 0);
    DDrawUty::g_DDConfig.SetSymbol(TEXT("d"), 0);
    DDrawUty::g_DDConfig.SetSymbol(TEXT("l"), 0);

    if (NULL != pszDLLCmdLine)
    {
        if (_tcsstr(pszDLLCmdLine, TEXT("i")))
        {
            DDrawUty::g_DDConfig.SetSymbol(TEXT("i"), 1);
        }
        if (_tcsstr(pszDLLCmdLine, TEXT("d")))
        {
            DDrawUty::g_DDConfig.SetSymbol(TEXT("d"), 1);
        }        
        if (_tcsstr(pszDLLCmdLine, TEXT("l")))
        {
            DDrawUty::g_DDConfig.SetSymbol(TEXT("l"), 1);
        }
    }
}

