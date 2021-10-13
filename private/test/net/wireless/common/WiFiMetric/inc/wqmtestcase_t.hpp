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
// Definitions and declarations for the WQMTestCase_t classes.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WQMTestCase_t_
#define _DEFINED_WQMTestCase_t_
#pragma once


#include "litexml.h"
namespace ce {
namespace qa {

struct WQMParamList;
class  WQMTestSuiteBase_t;

#define WIFI_TEST_CASE_START         (DWORD)0    // This is reserved
#define WIFI_TEST_CASE_END           (DWORD)100000  // This is reserved
#define WIFI_TEST_CASE_IDLE          (DWORD)101     // Tux test case ID for Idle
#define WIFI_TEST_CASE_CHKPOINT      (DWORD)102     // Tux test case ID for CheckPoint
#define WIFI_TEST_CASE_REPEAT        (DWORD)201     // Tux test case ID for Repeat test case
#define WIFI_TEST_CASE_PARALLEL      (DWORD)202     // Tux test case ID for Repeat test case
#define WIFI_TEST_RealCASE_START     (DWORD)1000    // This is where real test case starts


#define WIFI_METRIC_STR_TSTCASESTART    (_T("TstCaseIDStart"))
#define WIFI_METRIC_STR_TESTID_OFFSET   (_T("TuxTestIDOffset"))
#define WIFI_METRIC_STR_ITERATION       (_T("NumberTestPasses"))
#define WIFI_METRIC_STR_COMMENT         (_T("Comment"))
#define WIFI_METRIC_STR_DESC            (_T("TuxTestDescription"))
#define WIFI_METRIC_STR_SUITENAME       (_T("TestSuiteName"))
#define WIFI_METRIC_STR_GRPNAME         (_T("TestGroupName"))


// ----------------------------------------------------------------------------
//
// WQMTestCase_t Generic test case class for all common test cases, it's also
// the base class for any special test cases, like Idle,
//
class WQMTestCase_t
{
private:

protected:
    
    // TestId for tux
    DWORD   m_TuxTestID;
    BOOL    m_BvtRun;
    
    // Repeat index for test cases that include a repeat count
    // Each repeat get individual tux ID
    int     m_RepeatCount;

    // Decide if the test is parallel case
    BOOL    m_Parallel;

    // Total number of sub test cases within the jumble test
    int     m_TotalSubTestCases;
    
    
public:

    // All the other members set to public
    // Param List from the configuration file
    WQMParamList*           m_pParamList;
    
    // Test suite object
    WQMTestSuiteBase_t*     m_pTstSuite;    

    litexml::XmlBaseElement_t* m_pXMLNode;


    // Constructor/Destructor:
    WQMTestCase_t(LPSNODE paramList, 
                         WQMParamList* parentSuite,
                         int tuxTestRepeatIndex); 
    
    virtual
   ~WQMTestCase_t(void);    

    // Get the Tux test ID for the test
    virtual DWORD
    GetTuxTestID(DWORD* tuxTestID, BOOL bvtTest);

    virtual BOOL SkipTest(BOOL bvtRun);    

    virtual DWORD
    GetTuxDesc(LPCTSTR* tuxDesc);

    BOOL
    IsParallel(void) { return m_Parallel;}

    void
    SetParallel(void) { m_Parallel = TRUE;}
    
    // The function to actaully run the test
    virtual DWORD
    Execute(void);

    int
    GetRepeatIndex(void) { return m_RepeatCount; }

    void
    SetSubTestCount(int subTestNo) { m_TotalSubTestCases = subTestNo; }

    int
    GetSubTestCount(void) { return m_TotalSubTestCases; }

};

// ----------------------------------------------------------------------------
//
// WQMTestCaseIdle_t: Idle test case
//
class WQMTestCaseIdle_t : public WQMTestCase_t
{
private:

public:

    // Constructor/Destructor:
    WQMTestCaseIdle_t(LPSNODE paramList, WQMParamList* parentSuite); 
    
  __override virtual
   ~WQMTestCaseIdle_t(void);

    virtual DWORD
    GetTuxDesc(LPCTSTR* tuxDesc);  

    virtual BOOL SkipTest(BOOL bvtRun){ return FALSE; }

    // The function to actaully run the test
    virtual DWORD
    Execute(void);
};

// ----------------------------------------------------------------------------
//
// WQMTestCaseCheckPoint_t: CheckPoint test case
//
class WQMTestCaseCheckPoint_t : public WQMTestCase_t
{
private:

public:

    // Constructor/Destructor:
    WQMTestCaseCheckPoint_t(LPSNODE paramList, WQMParamList* parentSuite); 
    
  __override virtual
   ~WQMTestCaseCheckPoint_t(void);

    virtual DWORD
    GetTuxDesc(LPCTSTR* tuxDesc);  

    virtual BOOL SkipTest(BOOL bvtRun){ return FALSE; }    

    // The function to actaully run the test
    virtual DWORD
    Execute(void);
    
};

/*
// ----------------------------------------------------------------------------
//
// WQMTestCaseRepeat_t: Repeat test case
//
class WQMTestCaseRepeat_t : public WQMTestCase_t
{
private:

public:

    // Index of the repeat count
    int   m_RepeatIndex;    

    // Constructor/Destructor:
    WQMTestCaseRepeat_t(
        LPSNODE paramList, 
        WQMTestSuiteBase_t* parentSuite, 
        int repeatIndex); 
    
  __override virtual
   ~WQMTestCaseRepeat_t(void);


    virtual DWORD
    GetTuxTestID(DWORD* TuxTestID, BOOL bvtTest);  

};

// ----------------------------------------------------------------------------
//
// WQMTestCaseParallel_t: Parallel test case
//
class WQMTestCaseParallel_t : public WQMTestCaseRepeat_t
{
private:

public:



    // Constructor/Destructor:
    WQMTestCaseParallel_t(
        LPSNODE paramList, 
        WQMTestSuiteBase_t* parentSuite, 
        int repeatIndex);

  __override virtual
   ~WQMTestCaseParallel_t(void);

    // Setting the thread object
    void
    SetThread(WQMThread* thrd) { m_pThread = thrd; return ; }  
    
};

*/

};
};
#endif /* _DEFINED_WQMTestCase_t_ */
// ----------------------------------------------------------------------------

