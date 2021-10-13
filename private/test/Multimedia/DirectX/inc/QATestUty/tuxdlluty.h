// =============================================================================
//
//  TuxDLLUty
//  Copyright (c) 1999, Microsoft Corporation
//
//  Declares the ITuxDLL interface and the CTuxDLL class.
//
//  Revision History:
//       8/02/1999  ericflee  created
//       1/04/2000  ericflee  (vis a vis a-jasonl) renamed spiSkip to sprSkip
//
// =============================================================================

#pragma once
#ifndef __TUXDLLUTY_H__
#define __TUXDLLUTY_H__

// External dependencies
#include <kato.h>
#include <tux.h>
#include <heaptest.h>
#include <QATestUty/WinApp.h>

class ITuxDLL 
{
public:
    enum eShellProcReturnCode
    {
        sprHandled = SPR_HANDLED,
        sprNotHandled = SPR_NOT_HANDLED,
        sprFail = SPR_FAIL,
        sprSkip = SPR_SKIP
    };

    virtual eShellProcReturnCode Load(OUT bool& fUnicode) = 0;
    virtual eShellProcReturnCode Unload() = 0;
    
    virtual eShellProcReturnCode ProcessShellInfo(HINSTANCE hProcessInstance, 
                                                  HINSTANCE hModuleInstance, 
                                                  HANDLE hevmTerminate, 
                                                  bool fUsingServer, 
                                                  const CWindowsModule::command_line_type pszDllCmdLine)=0;

    virtual eShellProcReturnCode Register(OUT LPFUNCTION_TABLE_ENTRY& pFunctionTable) = 0;

    virtual eShellProcReturnCode StartScript() = 0;

    virtual eShellProcReturnCode StopScript() = 0;

    virtual eShellProcReturnCode BeginGroup() = 0;

    virtual eShellProcReturnCode EndGroup() = 0;

    virtual eShellProcReturnCode BeginTest(const FUNCTION_TABLE_ENTRY *pFTE, 
                                           DWORD dwRandomSeed, 
                                           DWORD dwThreadCount) = 0;

    virtual eShellProcReturnCode EndTest(const FUNCTION_TABLE_ENTRY *pFTE, 
                                         DWORD dwResult, 
                                         DWORD dwRandomSeed, 
                                         DWORD dwThreadCount, 
                                         DWORD dwExecutionTime) = 0;

    virtual eShellProcReturnCode Exception(const FUNCTION_TABLE_ENTRY *pFTE, 
                                           DWORD dwExceptionCode, 
                                           IN OUT EXCEPTION_POINTERS *pExceptionPointers,
                                           IN OUT DWORD& dwExceptionFilter) = 0;
};

class CTuxDLL : virtual public CWindowsModule, virtual public ITuxDLL
{
public:
    CTuxDLL(FUNCTION_TABLE_ENTRY *pFunctionTable, bool fLoadHeapTest=true);
    virtual ~CTuxDLL();

    HINSTANCE m_hProcessInstance;
    HANDLE m_hTerminationEvent;
    bool m_fUsingServer;

protected:   
    virtual bool Initialize();

    virtual eShellProcReturnCode Load(OUT bool& fUnicode);
    virtual eShellProcReturnCode Unload();
    
    virtual eShellProcReturnCode ProcessShellInfo(HINSTANCE hProcessInstance, 
                                                  HINSTANCE hModuleInstance, 
                                                  HANDLE hevmTerminate, 
                                                  bool fUsingServer, 
                                                  const command_line_type pszDllCmdLine);

    virtual void ProcessCommandLine(command_line_type pszDLLCmdLine);

    virtual eShellProcReturnCode Register(OUT LPFUNCTION_TABLE_ENTRY& pFunctionTable);

    virtual eShellProcReturnCode StartScript();
    virtual eShellProcReturnCode StopScript();
    virtual eShellProcReturnCode BeginGroup();
    virtual eShellProcReturnCode EndGroup();
    virtual eShellProcReturnCode BeginTest(const FUNCTION_TABLE_ENTRY *pFTE, 
                                           DWORD dwRandomSeed, 
                                           DWORD dwThreadCount);
    virtual eShellProcReturnCode EndTest(const FUNCTION_TABLE_ENTRY *pFTE, 
                                         DWORD dwResult, 
                                         DWORD dwRandomSeed, 
                                         DWORD dwThreadCount, 
                                         DWORD dwExecutionTime);
    virtual eShellProcReturnCode Exception(const FUNCTION_TABLE_ENTRY *pFTE, 
                                           DWORD dwExceptionCode, 
                                           IN OUT EXCEPTION_POINTERS *pExceptionPointers,
                                           IN OUT DWORD& dwExceptionFilter);

    FUNCTION_TABLE_ENTRY *m_pFunctionTable;

protected:
    // =================
    // Heap Test Control
    // =================
    bool m_fLoadHeapTest;

    struct CHeapTestInfo
    {
        typedef HANDLE (WINAPI *pfnHTStartSession)(HANDLE, DWORD, DWORD, DWORD);
        typedef BOOL (WINAPI *pfnHTStopSession)(HANDLE);

        HINSTANCE m_hHeapTest;
        pfnHTStartSession m_HTStartSession;
        pfnHTStopSession m_HTStopSession;
        HANDLE m_hHTSession;

        CHeapTestInfo() : m_hHeapTest(NULL), m_HTStartSession(NULL), m_HTStopSession(NULL), m_hHTSession(NULL) {}
        void Load();
        void Unload();
    };

    CHeapTestInfo m_HeapTestInfo;

    virtual void LoadHeapTest();
    virtual void UnloadHeapTest();

public:
    static DWORD ShellProc(UINT uMsg, SPPARAM spParam);
};


#define DEFINE_ShellProc \
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)  \
{ \
    return CTuxDLL::ShellProc(uMsg, spParam);  \
} 


#endif
