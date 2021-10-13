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

#include <QATestUty/WinApp.h>
#include <QATestUty/TuxDLLUty.h>
#include <DebugStreamUty.h>
#include <DDrawUty.h>
#include "TuxFunctionTable.h"

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

    virtual void ProcessCommandLine(LPCTSTR pszDLLCmdLine);
};

CDDrawTK g_Module;

void CDDrawTK::ProcessCommandLine(LPCTSTR pszDLLCmdLine)
{
    DDrawUty::g_DDConfig.SetSymbol(TEXT("i"), 0);
    DDrawUty::g_DDConfig.SetSymbol(TEXT("d"), 0);

    if (NULL != pszDLLCmdLine)
    {
        if (_tcsstr(pszDLLCmdLine, TEXT("i")))
        {
            DDrawUty::g_DDConfig.SetSymbol(TEXT("i"), 1);
        }
         else if (_tcsstr(pszDLLCmdLine, TEXT("d")))
        {
            DDrawUty::g_DDConfig.SetSymbol(TEXT("d"), 1);
        }
    }
}

