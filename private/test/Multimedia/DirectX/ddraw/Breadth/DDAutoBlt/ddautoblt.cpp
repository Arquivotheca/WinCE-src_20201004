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
//      02/12/2000  JonasK  Copied from DDrawBVT
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

class CDDAutoBlt : public DDrawUty::CDDrawTuxDLL
{
    typedef DDrawUty::CDDrawTuxDLL base_class;

public:
    CDDAutoBlt() : base_class(g_lpFTE, false) 
    {
        m_pszModuleName = TEXT("CDDAutoBlt.DLL");
    }
    virtual void ProcessCommandLine(LPCTSTR pszDLLCmdLine);
    virtual void PrintUsage();
};

CDDAutoBlt g_Module;

void CDDAutoBlt::ProcessCommandLine(LPCTSTR pszDLLCmdLine)
{
    // The only parameter we currently accept is /DisplayIndex,
    // and it should be used as "/DisplayIndex <int>"
    TCHAR * tszDisplayIndex = TEXT("/DisplayIndex");
    int iDisplayIndex = -1;

    // The default display index is -1 (for NULL).
    DDrawUty::g_DDConfig.SetSymbol(tszDisplayIndex, (DWORD)-1);

    if (NULL != pszDLLCmdLine)
    {
        if (_tcsstr(pszDLLCmdLine, tszDisplayIndex))
        {
            if (!_stscanf_s(pszDLLCmdLine + _tcslen(tszDisplayIndex), TEXT(" %i"), &iDisplayIndex))
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

void CDDAutoBlt::PrintUsage()
{
    dbgout << "Usage: tux ... -d DDAutoBlt -c \"/DisplayIndex <index>\"" << endl;
    dbgout << "If the index is not specified, the primary display will be used" << endl;
}

