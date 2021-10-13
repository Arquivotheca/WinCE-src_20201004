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
// =============================================================================
//
//  DDrawFunc TUX DLL
//  Copyright (c) 1999, Microsoft Corporation
//
//
//  Revision History:
//      01/10/2000  JonasK  Copied from DSndBVT
//
// =============================================================================

#include "main.h"

#include <QATestUty/WinApp.h>
#include <QATestUty/TuxDLLUty.h>
#include <DebugStreamUty.h>
#include <DDrawUty.h>
#include "TuxFunctionTable.h"

using namespace DebugStreamUty;

DEFINE_DllMain;
DEFINE_ShellProc;

typedef ITuxDLL::eShellProcReturnCode eShellProcReturnCode;

class CDDrawFunc : public DDrawUty::CDDrawTuxDLL
{
    typedef DDrawUty::CDDrawTuxDLL base_class;
    
public:
    CDDrawFunc() : base_class(g_lpFTE, false) 
    {
        m_pszModuleName = TEXT("DDrawFunc.DLL");
    }
    virtual void ProcessCommandLine(LPCTSTR pszDLLCmdLine);
    virtual void PrintUsage();
    virtual eShellProcReturnCode Load(OUT bool& fUnicode);
    virtual eShellProcReturnCode Unload();
};

CDDrawFunc g_Module;

void CDDrawFunc::ProcessCommandLine(LPCTSTR pszDLLCmdLine)
{
    // The only parameter we currently accept is /DisplayIndex,
    // and it should be used as "/DisplayIndex <int>"
    TCHAR * tszDisplayIndex = TEXT("/DisplayIndex");
    int iDisplayIndex = -1;

    // The default display index is -1 (for NULL).
    DDrawUty::g_DDConfig.SetSymbol(tszDisplayIndex,(DWORD) -1);

    if (NULL != pszDLLCmdLine)
    {
        if (_tcsstr(pszDLLCmdLine, tszDisplayIndex))
        {
            if (!swscanf_s(pszDLLCmdLine + _tcslen(tszDisplayIndex), TEXT(" %i"), &iDisplayIndex))
            {
                dbgout << "Could not find index" << endl;
                PrintUsage();
            }
            if (iDisplayIndex < -1 || iDisplayIndex > 100)
            {
                dbgout << iDisplayIndex << " is not a valid display index" << endl;
                PrintUsage();
            }
            DDrawUty::g_DDConfig.SetSymbol(tszDisplayIndex, iDisplayIndex);
        }
    }
}

void CDDrawFunc::PrintUsage()
{
    dbgout << "Usage: tux ... -d DDFunc -c \"/DisplayIndex <index>\"" << endl;
    dbgout << "If the index is not specified, the primary display will be used" << endl;
}

eShellProcReturnCode CDDrawFunc::Load(bool& fUnicode)
{
    using namespace DebugStreamUty;

    eShellProcReturnCode sprc = CDDrawTuxDLL::Load(fUnicode);

    // Set the DirectDraw window as the topmost one, so that no other window can deactivate it while
    // the tests are running.
    // Ignore Failure.
    if(!SetWindowPos(this->m_hWnd, HWND_TOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW))
    {
        dbgout << "Unable to set the Window as top most. GetLastError() = " 
               << HEX((DWORD)GetLastError()) << endl;
    }

    return sprc;
}

eShellProcReturnCode CDDrawFunc::Unload()
{
    using namespace DebugStreamUty;

    // Allow other windows to de-activate the DirectDraw window.
    // Ignore Failure.
    if(!SetWindowPos(this->m_hWnd, HWND_NOTOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE))
    {
        dbgout << "Unable to set the Window as non-topmost. GetLastError() = " 
               << HEX((DWORD)GetLastError()) << endl;
    }

    return CDDrawTuxDLL::Unload();
}