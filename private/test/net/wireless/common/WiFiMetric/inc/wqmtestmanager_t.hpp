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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
// 
// File Name:	WQMTestManager_t.h
// 
// The main controller for the RealLifeScn test tool, The WQMTestManager_t reads in the test config
// file, create test suites and test cases based on the information provided in the config file.
// Run all the test cases and report test results.
// 
// ----------------------------------------------------------------------------

#pragma once
#ifndef __WQMTESTMANAGER_h__
#define __WQMTESTMANAGER_h__

#include "WQMIniFileParser_t.hpp"

namespace ce {
namespace qa {

class  WQMTestSuiteBase_t;
struct WQMTstCaseNode;  

// Test worker thread definition
struct WQMThread
{
    HANDLE  threadHandle;
    HANDLE  msgQRHandle;
    HANDLE  msgQWHandle;
    BOOL    bad;
    BOOL    interimResult;
    BOOL    toExit;
};

#define MAX_TEST_SUITE_NO     ((int) 32)

class WQMTestManager_t
{
public:

    
    WQMTestManager_t();
    ~WQMTestManager_t();

    DWORD    ParseCommandLine(int argc, TCHAR *argv[]);
    void     PrintUsage();    
    DWORD    GetFunctionTable(FUNCTION_TABLE_ENTRY **ppFunctionTable);
    int      RunTest(FUNCTION_TABLE_ENTRY *pFTE);
    void     ReportTestResult(void);

private:
     
    BOOL              Init();
    WQMTstCaseNode*   GetNextTestUnit();
    void              Cleanup();   

    FUNCTION_TABLE_ENTRY* MakeFuncTable(void);

    BOOL              FillCmdLineHashTables(int argc, TCHAR* argv[]);

    litexml::XmlElement_t*     ParseTestGroupResult(WQMTstGroupNode* testGroup, int* groupPerc);
    litexml::XmlElement_t*     ParseTestSuiteResult(WQMTestSuiteBase_t* testSuite);
    litexml::XmlElement_t*     ParseTestCaseResult(WQMTestCase_Info* testCase);
    
    
    static TESTPROCAPI
    TestProc(
       UINT uMsg,                   // TPM_QUERY_THREAD_COUNT or TPM_EXECUTE
       TPPARAM tpParam,             // TPS_QUERY_THREAD_COUNT or not used
       FUNCTION_TABLE_ENTRY *pFTE); // test to be executed

    WQMTestSuiteBase_t*        m_pTestSuiteList[MAX_TEST_SUITE_NO];
    WQMTestSuiteBase_t*        m_pCurTestSuite;
    int                        m_TotalTestSuite;
    WQMTstCaseNode*            m_pTestList;
    WQMTstCaseNode*            m_pCurTestUnit;
    int                        m_TotalExeUnits;
    WQMThread*                 m_pWorkerThread;
    WQMIniFileParser_t         m_FileParser;
    DWORD                      m_StartTime;
    CmdArgBool_t               m_IsRandom;
    CmdArgBool_t               m_MultiThrd;
    CmdArgBool_t               m_BvtTestRun;
    CmdArgLong_t               m_PassCriteria;
    CmdArgLong_t               m_FailCriteria;
    CmdArgLong_t               m_MaxContFailCount;
    CmdArgString_t             m_LocalFileName;
    CmdArgString_t             m_DeviceName;
    long                       m_ContFailCount;
    // Tux function table:
    FUNCTION_TABLE_ENTRY*      m_pFunctionTable;
    long                       m_FunctionTableSize;
    long                       m_FunctionTableEnd;

    // Lookup tables created form command lines and will be used during configuration
    // file parsing
    WQMNameHash                m_TstSetupCmdLineLkUpTable;
    WQMTstSuiteHash            m_TstSuiteCmdLineLkUpTables[MAX_TEST_SUITE_NO];
    int                        m_NoOfTstSuiteLkUpTables;

};

WQMTestManager_t*
GetTestManager(void);

void
DestroyTestManager(void);    
};
};

#endif // __WQMTESTMANAGER_h__

