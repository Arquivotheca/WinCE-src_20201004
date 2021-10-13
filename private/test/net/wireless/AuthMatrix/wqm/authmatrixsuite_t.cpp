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
// Implementation of the AuthMatrixSuite class.
// Also includes DllMain and CreateTestSuite DLL export method.
//
// ----------------------------------------------------------------------------

#include "AuthMatrixSuite_t.hpp"
#include "AuthMatrix_t.hpp"

#include <assert.h>
#include <cmnprint.h>
#include <logwrap.h>
#include <winsock2.h>

#include <WiFUtils.hpp>

#include "WQMUtil.hpp"
#include "WQMTestCase_t.hpp"

using namespace ce::qa;
        
// ----------------------------------------------------------------------------
//
// Local data:
//

// ----------------------------------------------------------------------------
//
// Constructor / Destructor:
//
AuthMatrixSuite_t::
AuthMatrixSuite_t(HMODULE hModule, IN HANDLE logHandle)
    : WQMTestSuiteBase_t(hModule, logHandle),
      m_pTestDesc  (NULL),
      m_pIsBadModes(NULL),
      m_pIsBadKey  (NULL),
      m_pIsStressTest(NULL)
{
    // nothing to do
}

AuthMatrixSuite_t::
~AuthMatrixSuite_t(void)
{
    Cleanup();
}

// ----------------------------------------------------------------------------
//
//
BOOL
AuthMatrixSuite_t::
PrepareToRun(void)
{
    if(m_Initialized)
        return TRUE;
    
    DWORD result;

    // Initialize winsock
    WSADATA wsData;
    int retcode = WSAStartup(MAKEWORD(2,2), &wsData);
    if (retcode != 0)
    {
        LogError(TEXT("Could not initialize Winsock: %d"),
                 WSAGetLastError());
        return FALSE;
    }

    // Initialize the static environment.
    AuthMatrix_t::StartupInitialize();

    // Initialize the test-suite configuration parameter list.
    if (NULL == AuthMatrix_t::GetCmdArgList())
        return FALSE;

    // Initialize the test-case configuration parameter lists. 
    m_pTestDesc 
        = new CmdArgString_t(WIFI_METRIC_STR_DESC,
                             TEXT("tTestDesc"),
                             TEXT("Test Description"),
                             TEXT(""));
    result = m_TestConfigList1.Add(m_pTestDesc);
    if(result != NO_ERROR)
        return FALSE;      

    ce::auto_ptr<CmdArgList_t> pConfigArgs
        = m_Config.GenerateCmdArgList(TEXT("STAConfig"),
                                      TEXT("sta"),
                                      TEXT("STA WiFi Config"));
    result = m_TestConfigList1.Add(pConfigArgs);
    if (NO_ERROR != result)
        return FALSE;    
    
    ce::auto_ptr<CmdArgList_t> pAPConfigArgs
        = m_APConfig.GenerateCmdArgList(TEXT("APConfig"),
                                        TEXT("ap"),
                                        TEXT("AP WiFi Config"));
    result = m_TestConfigList2.Add(pAPConfigArgs);
    if (NO_ERROR != result)
        return FALSE;    
    
    m_pIsBadModes
        = new CmdArgBool_t(TEXT("IsBadModes"),
                           TEXT("fBadModes"),
                           TEXT("Bad security modes flag"),
                           false);
    result = m_TestConfigList2.Add(m_pIsBadModes);
    if(result != NO_ERROR)
        return FALSE;    
    
    m_pIsBadKey
        = new CmdArgBool_t(TEXT("IsBadKey"),
                           TEXT("fBadKey"),
                           TEXT("Bad security modes flag"),
                           false);
    result = m_TestConfigList2.Add(m_pIsBadKey);
    if(result != NO_ERROR)
        return FALSE;

    m_pIsStressTest
        = new CmdArgBool_t(TEXT("IsStressTest"),
                         TEXT("fStress"),
                         TEXT("Stress test Case"),
                         false);
    result = m_TestConfigList2.Add(m_pIsStressTest);
    if (result != NO_ERROR)
        return FALSE;         
    

    return m_Initialized = WQMTestSuiteBase_t::PrepareToRun();
}

// ----------------------------------------------------------------------------
//
//
int
AuthMatrixSuite_t::
Execute(
    IN WQMTestCase_t* pTestCase)
{
    DWORD result = NO_ERROR;

    assert(pTestCase);

    LogDebug(TEXT("AuthMatrix -- Execute Top"));

    // Update the test-suite configuration parameters.
    CmdArgList_t *pArgList = AuthMatrix_t::GetCmdArgList();
    if (NULL == pArgList)
    {
        assert(NULL != pArgList);
        result = ERROR_OUTOFMEMORY;
    }
    else
    {
        //result = ParseConfigurationNode(m_pTestSuiteParams, pArgList);
        result = ParseConfigurationNode(pTestCase->m_pParamList,pArgList);
    }
    if (NO_ERROR != result)
    {
        LogError(TEXT("Failed parsing test suite \"%s\" configuration: %s"),
                 m_TestSuiteName.GetValue(),
                 Win32ErrorText(result));
        return TPR_FAIL;
    }

    // Update the first set of test-case configuration parameters.
    result = ParseConfigurationNode(pTestCase->m_pParamList, &m_TestConfigList1);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Failed parsing test case \"%s - %s\" configuration: %s"),
                 m_TestSuiteName.GetValue(),
                 m_pTestDesc->GetValue(),
                 Win32ErrorText(result));
        return TPR_FAIL;
    }  

    // Copy the STA configuration over the AP configuration.
    m_APConfig = m_Config;

    // Update the second set of test-case configuration parameters.
    result = ParseConfigurationNode(pTestCase->m_pParamList, &m_TestConfigList2);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Failed parsing test case \"%s - %s\" configuration: %s"),
                 m_TestSuiteName.GetValue(),
                 m_pTestDesc->GetValue(),
                 Win32ErrorText(result));
        return TPR_FAIL;
    }  

    DWORD testId = 0;
    result = pTestCase->GetTuxTestID(&testId, FALSE);
    if (NO_ERROR != result)
    {
        LogError(_T("failed in getting the test ID"));
        return TPR_FAIL;
    }     

    // Allocate the AuthMatrix object.
    ce::auto_ptr<AuthMatrix_t> pTest = new AuthMatrix_t(testId,
                                                        m_pTestDesc->GetValue(),
                                                        m_APConfig,
                                                        m_Config,
                                                        m_pIsBadModes->GetValue(),
                                                        m_pIsBadKey->GetValue(),
                                                        m_pIsStressTest->GetValue());
    if (pTest == NULL)
    {
        LogError(TEXT("Can't allocate %u byte AuthMatrix object"),
                 sizeof(AuthMatrix_t));
        return TPR_FAIL;
    }

    // Run the test.
    return pTest->Execute(testId, (*m_pTestDesc));
}

// ----------------------------------------------------------------------------
//
//
void
AuthMatrixSuite_t::
Cleanup(void)
{
    if(!m_Initialized)
        return;
    
    // All args will be deleted by cmd-arg lists.
    m_pTestDesc   = NULL;
    m_pIsBadModes = NULL;
    m_pIsBadKey   = NULL;
    m_pIsStressTest = NULL;

    // Clean up the test cases.
    WQMTestSuiteBase_t::Cleanup();

    // Clean up the static environment.
    AuthMatrix_t::ShutdownCleanup();

    // Shut down winsock.
    WSACleanup();
}

// ----------------------------------------------------------------------------
//
// DLL entry points.
//

#ifdef UNDER_CE
BOOL WINAPI
DllMain(
    HANDLE hInstance,
    ULONG  dwReason,
    LPVOID lpReserved)
{
   return TRUE;
}
#endif /* ifdef UNDER_CE */

extern "C"
WQMTestSuiteBase_t *
CreateAuthMatrixTestSuite(
    IN HMODULE hModule, 
    IN HANDLE  hLogger)
{

    WQMTestSuiteBase_t* tmpNode = new AuthMatrixSuite_t(hModule, hLogger);
    if (NULL == tmpNode)
    {
        LogError(TEXT("Cannot allocate AuthMatrixSuite object: %s"),
                 Win32ErrorText(GetLastError()));
    }

    return tmpNode;
}




