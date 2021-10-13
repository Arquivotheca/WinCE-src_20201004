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

#include <QATestUty/WinApp.h>
#include <QATestUty/TuxDLLUty.h>
#include <DebugStreamUty.h>
#include <DDrawUty.h>
#include "TuxFunctionTable.h"
#include <vector>
#include <stdio.h>

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
    bool ProcessDbgCmdLine(LPCTSTR pszDllCmdLine);
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
    using namespace DebugStreamUty;

    eShellProcReturnCode sprc = CDDrawTuxDLL::Load(fUnicode);

    // Hide all the visible windows. This is so that during our windowed tests, any other windows
    // (such as the ClientSide.exe window) do not invalidate our comparisons.
    EnumWindows((WNDENUMPROC)HideWindows, (LPARAM)this);

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

eShellProcReturnCode CDDrawTK::Unload()
{
    using namespace DebugStreamUty;

    // Allow other windows to de-activate the DirectDraw window.
    // Ignore Failure.
    if(!SetWindowPos(this->m_hWnd, HWND_NOTOPMOST, 0,0, 0,0, SWP_NOSIZE | SWP_NOMOVE))
    {
        dbgout << "Unable to set the Window as non-topmost. GetLastError() = " 
               << HEX((DWORD)GetLastError()) << endl;
    }

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
        // If there is no debug command line passed in, check for i,d & l.
        if(!ProcessDbgCmdLine(pszDLLCmdLine))
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
}

// Returns true if the command line starts with "dbg ",
// after processing the name value pairs in the debug command line.
bool CDDrawTK::ProcessDbgCmdLine(LPCTSTR pszDllCmdLine)
{   
    using namespace DebugStreamUty;
    using namespace DDrawUty;

    bool bIsDbgFound = false;

    // Statics
    static const TCHAR DBG_CMD[] = TEXT("dbg ");
    static const TCHAR SPACE_SEPARATOR[] = _T(" ");

    // If the command line starts with "dbg ", treat the string following "dbg "
    // as <string: name> <int: value> space separated pairs.
    // For example, s tux -d ddrawtk -o -c "dbg surf 0 from 0 to 1"
    // Blindly take each such pair and add it to the Symbol table for any test to pick up.
    LPCTSTR pszNext = _tcsstr(pszDllCmdLine, DBG_CMD);
    if(pszNext == pszDllCmdLine) // The cmd line string starts with the DBG_CMD
    {
        bIsDbgFound = true;

        // Jump over the DBG_CMD characters.
        pszNext += _tcslen(DBG_CMD);

        dbgout << "A Debug Command Line has been passed in : " << pszNext <<  endl;

        // Break the debug command line, using spaces as separators.
        LPTSTR pszToken = NULL;
        LPTSTR pszNextToken = NULL;        
        LPTSTR pszTokenStart = NULL;
        DWORD dwTokenLength = 0;
        pszToken = _tcstok_s((LPTSTR)pszNext, SPACE_SEPARATOR, &pszNextToken);        
        
        bool bIsName = true;
        while(pszToken)
        {                                
            if(bIsName)
            {                        
                pszTokenStart = pszToken; // Store the name token for future use.
                bIsName = false; // Next token is an int.
            }
            else
            {
                DWORD dwValue = 0;

                // Get the integer value of the token string.
                if(!_stscanf_s(pszToken, _T("%d"), &dwValue))
                {
                    dbgout << "WARNING : No associated value for " << pszTokenStart << "." << endl;
                    dbgout << "WARNING : Please check the dbg commandline for errors." << endl;
                }

                // Set the name value pair in the global symbol table.
                dbgout << "DBG : " << pszTokenStart << " = " << dwValue << endl;
                g_DDConfig.SetStringValue(wstring(pszTokenStart), dwValue);

                bIsName = true; // Next token is a name.
            }
            pszToken = _tcstok_s(NULL, SPACE_SEPARATOR, &pszNextToken);
        } 
    } // if the cmd line starts with a DBG_CMD

    return bIsDbgFound;
}