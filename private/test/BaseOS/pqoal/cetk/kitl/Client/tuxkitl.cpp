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
#include <windows.h>
#include <tchar.h>
#include <tux.h>

#include <Kitlprot.h>
#include <Halether.h>

/*
 * pull in function defs for the Kitl tests
 */

#include "tuxKitl.h"
#include "KITLTestDriverDev.h"
#include "log.h"

#define KITL_BASE 1000 // Base to start the test at

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("Kitl Tests Usage Message")                        , 0, 0, 1, KitlTestsUsageMessage,
  _T("KITL Tests")                                  , 0, 0, 0, NULL, // Test suite
  _T("Vary packet sizes - 1 Thread + ucWindowSize: 1")        , 0, 1, KITL_BASE, testKitlStress,
  _T("Max outstanding packets- 1 Thread + ucWindowSize: 16")       , 0, 2, KITL_BASE+1, testKitlStress,
  _T("Concurrency Stress - 10 Thread+ ucWindowSize:16")        , 0, 3, KITL_BASE+2, testKitlStress,
  _T("Maximum KITL Perf")                               , 0, 4, KITL_BASE+3, testKitlStress,
  _T("Maximum roundtrip latency one one stream")        , 0, 5, KITL_BASE+4, testKitlStress,
   NULL                                                 , 0, 0, 0, NULL  // marks end of list
};

// Test settings have one to one mapping to KITL Test cases
TEST_SETTINGS g_testSettings[] = {
    // Test Case 1000
    {
        0x28, // Struct Size
        1, //dwThreadCount
        1, // dwMinPayLoadSize
        KITL_MAX_DATA_SIZE, //dwMaxPayLoadSize
        KITL_CFGFL_STOP_AND_WAIT, // ucFlags
        1, // ucWindowSize
        KITL_RCV_ITERATIONS,// dwRcvIterationsCount
        KITL_RCV_TIMEOUT, // dwRcvTimeout
        TRUE, // fVerifyRcv
        KITL_SEND_ITERATIONS, // dwSendIterationsCount
        KITL_PAYLOAD_DATA, // cData
        0
    },
    // Test Case 1001
    {
        0x28, // Struct Size
        1, //dwThreadCount
        1, // dwMinPayLoadSize
        KITL_MAX_DATA_SIZE, //dwMaxPayLoadSize
        KITL_CFGFL_STOP_AND_WAIT, // ucFlags
        KITL_MAX_WINDOW_SIZE, // ucWindowSize
        KITL_RCV_ITERATIONS,// dwRcvIterationsCount
        KITL_RCV_TIMEOUT, // dwRcvTimeout
        TRUE, // fVerifyRcv
        KITL_SEND_ITERATIONS, // dwSendIterationsCount
        KITL_PAYLOAD_DATA, // cData
        0
    },
    // Test Case 1002
    {
        0x28, // Struct Size
        10, //dwThreadCount
        1, // dwMinPayLoadSize
        KITL_MAX_DATA_SIZE, //dwMaxPayLoadSize
        KITL_CFGFL_STOP_AND_WAIT, // ucFlags
        KITL_MAX_WINDOW_SIZE, // ucWindowSize
        10,// dwRcvIterationsCount
        KITL_RCV_TIMEOUT, // dwRcvTimeout
        TRUE, // fVerifyRcv
        10, // dwSendIterationsCount
        KITL_PAYLOAD_DATA, // cData
        0
    },
    // Test Case 1003
    {
        0x28, // Struct Size
        1, //dwThreadCount
        KITL_MAX_DATA_SIZE-1, // dwMinPayLoadSize
        KITL_MAX_DATA_SIZE, //dwMaxPayLoadSize
        KITL_CFGFL_STOP_AND_WAIT, // ucFlags
        KITL_MAX_WINDOW_SIZE, // ucWindowSize
        10000,// dwRcvIterationsCount
        KITL_RCV_TIMEOUT, // dwRcvTimeout
        FALSE, // fVerifyRcv
        10000, // dwSendIterationsCount
        KITL_PAYLOAD_DATA, // cData
        0
    },
    // Test Case 1004
    {
        0x28, // Struct Size
        1, //dwThreadCount
        KITL_MAX_DATA_SIZE-1, // dwMinPayLoadSize
        KITL_MAX_DATA_SIZE, //dwMaxPayLoadSize
        KITL_CFGFL_STOP_AND_WAIT, // ucFlags
        1, // ucWindowSize
        10000,// dwRcvIterationsCount
        KITL_RCV_TIMEOUT, // dwRcvTimeout
        FALSE, // fVerifyRcv
        10000, // dwSendIterationsCount
        KITL_PAYLOAD_DATA, // cData
        0
    }

};

////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
 *
 *                                Usage Message
 *
 ******************************************************************************/
/*
 * Prints out the usage message for the Kitl tests. It tells the user
 * what the tests do and also specifies the input that the user needs to
 * provide to run the tests.
 */

/******************************************************************************
//    Function Name:  KitlTestsUsageMessage
//    Description:
//      Provides instructions on how to run the test
//
//    Input:
//      None
//
//    Output:
//      TPR_NOT_HANDLED when passed TPM_EXECUTE otherwise TPR_PASS
//
******************************************************************************/
TESTPROCAPI KitlTestsUsageMessage(
       UINT uMsg,
       TPPARAM /* tpParam */,
       LPFUNCTION_TABLE_ENTRY /* lpFTE */)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    LOG(L"To run this test, use tux to run this test");
    LOG(L"then run the desktop application.  For the two");
    LOG(L"component to connects properly, the desktop portion");
    LOG(L"of KITL test got to be passed the targetted device");
    LOG(L"name as argument");

    return (TPR_PASS);
}

/******************************************************************************
//    Function Name:  testKitlStress
//    Description:
//      Place holder
//
//    Input:
//      None
//
//    Output:
//      TPR_NOT_HANDLED when passed TPM_EXECUTE otherwise TPR_PASS
//
******************************************************************************/
TESTPROCAPI testKitlStress(
              UINT uMsg,
              TPPARAM /* tpParam */,
              LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HRESULT hr = E_FAIL;
    INT returnVal = TPR_FAIL;
    CKITLTestDriverDev *pTestDriver = NULL;

    // Only handle TPM_EXECUTE message
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    // Map test ID to test settings
    DWORD dwIndex = lpFTE->dwUniqueID - KITL_BASE;
    DWORD dwSettingsCount = sizeof(g_testSettings)/sizeof(*g_testSettings);
    if (dwIndex >= dwSettingsCount)
    {
        FAILLOGA(L"Test index %u is out of range.  Must be below %u",dwIndex,dwSettingsCount);
        return returnVal;
    }

    // Fill TestSettings structure
    // It is ok to rely here on the copy constructor
    TEST_SETTINGS testSettings = g_testSettings[dwIndex];

    // Increment ref count
    // This object is originally instantiated when KITL loads this dll
    hr = CKITLTestDriverDev::CreateInstance(&pTestDriver);
    if (FAILED(hr))
    {
        FAILLOG(L"Failed to create the test driver instance. hr = 0x%08x",hr);
        goto Exit;
    }

    // Start the test
    // Test will continue running till interrupted
    hr = pTestDriver->RunTest(testSettings, lpFTE);
    if (FAILED(hr))
    {
        FAILLOG(L"Test failed. hr = 0x%08x",hr);
        goto Exit;
    }

    returnVal = TPR_PASS;

Exit:
    // Decrement internal ref count
    if (pTestDriver)
    {
        hr = pTestDriver->Release();
        if (FAILED(hr))
        {
            FAILLOGA(L"Failed to release test driver hr = 0x%08x",hr);
            returnVal = TPR_FAIL;
        }
    }

  return (returnVal);

}



