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
// File Name: WQMTestSuiteBase_t.hpp
//
// Description:  The WQMTestSuiteBase_t provide basic functionality
// for all the test suites, any test suite that need to be run under
// the WiFiMetrics framework need to inherit from this base class.
//
// ----------------------------------------------------------------------------

#pragma once
#ifndef _DEFINED_WQMTestSuiteBase_H_
#define _DEFINED_WQMTestSuiteBase_H_

#include <CmdArg_t.hpp>
#include <tux.h>

#include <WQMUtil.hpp>

namespace ce {
namespace qa {

struct WQMTestCase_Info
{
   TCHAR* TestCaseDesc;
   long   TestId;
   int    TotalTestRun;
   int    PassedTestRun;
   int    FailedTestRun;
   int    SkipTestRun;
   WQMTestCase_Info*  NextTestCaseInfo;
};

struct WQMTestSuite_Info
{
   TCHAR*   TestSuiteName;
   int      TotalTestCase;
   int      TotalTestPassed;
   int      TotalTestFailed;
   int      TotalTestSkiped;
   int      PassPercentage;
   int      PassCri;
   int      FailCri;
   double   TestWeight;
};

// Default Test suite passing criteria
#define WIFI_METRIC_TEST_PASS_CRI   ((int) 95)
#define WIFI_METRIC_TEST_FAIL_CRI   ((int) 60)
#define WIFI_METRIC_TEST_WEIGHT_MAX ((int) 100.0)
#define WIFI_METRIC_STR_TEST_PASS   (_T("PassCriteria"))
#define WIFI_METRIC_STR_TEST_FAIL   (_T("FailCriteria"))
#define WIFI_METRIC_STR_TEST_WEIGHT (_T("TestWeight"))

// forward declare
struct  WQMParamList;
class   WQMTestCase_t;
struct  WQMTstSuiteNode;

class WQMTestSuiteBase_t
{
public:


    WQMTestSuiteBase_t(IN HMODULE hModule, IN HANDLE logHandle);
    virtual ~WQMTestSuiteBase_t(void);

    // Init function:
    // Common code to initiate test suite structures
    BOOL Init(IN const WQMTstSuiteNode *pTestSuiteParams);      

    // PrepareToRun
    // This is the function to be call before any test case can be run,
    // derived class must override this function to setup specific
    // test enviorment
    virtual BOOL PrepareToRun(void);

    // Function to fill out a Tux function table entry:
    // For most cases, calling HasTest to decide if the test case belongs
    // here, as done by the default implementation, is sufficient.
    DWORD AddFunctionTableEntry(IN FUNCTION_TABLE_ENTRY *pFTE);

    // Execute is where each test suite will really handle its test cases:
    // The derived test suites must implement this.
    virtual int Execute(IN WQMTestCase_t *pTestCase) = 0;

    // The derived test suite should implement this to clean up specific
    // test suite related resources after all test cases have been run:
    virtual void Cleanup(void);

    // Again, hopefully this default implementation is enough for any 
    // test suites for reporting test results, but can be overidden.
    virtual void ReportTestResult(void);     

    // Verify a particular test case belong in this test suite:
    // The test case must have configuration field of the test suite name.
    // This function should be overridden under special cases such as Tux wrapper.
    virtual BOOL HasTest(IN WQMTestCase_t *pTestCase);

    // Return a summary of the test suites
    WQMTestSuite_Info *GetTestSuiteInfo(void);

    // Return a summary of the test suites
    WQMTestCase_Info *GetTestCasesInfo(void);    

    // The dll handle
    virtual HMODULE GetHandle(void) { return m_ModuleHandle; }

    // This helper function provide a way for the derived class to report
    // test result info for a particular test run ( Fail, Pass, Skip ).
    // Most likely it will be callled from Execute().
    void AddTestRun(IN long TestId, IN DWORD TestResult);     

    // Access Functions
    BOOL    GetStatus(void) { return m_TestSuiteStatus; }
    void    SetStatus(BOOL status) { m_TestSuiteStatus = status ; }
    WQMParamList* GetParamList(void) { return m_pTestSuiteParams; }

protected:

    CmdArgLong_t        m_PassingCriteria;
    CmdArgLong_t        m_FailCriteria;
    CmdArgDouble_t      m_TestWeight;
    CmdArgString_t      m_TestSuiteName;
    BOOL                m_Initialized;
    CRITICAL_SECTION    m_Critial;
    HMODULE             m_ModuleHandle;
    WQMParamList       *m_pTestSuiteParams;
    WQMTestCase_Info   *m_pTestCaseInfoList;
    WQMTestCase_Info   *m_pTestEndNode;
    WQMTestSuite_Info   m_TestSuiteInfo;
    BOOL                m_TestSuiteStatus;    
    WQMEleNode*         m_pConfigNode;

};
};
};

#endif // _DEFINED_WQMTestSuiteBase_H_

