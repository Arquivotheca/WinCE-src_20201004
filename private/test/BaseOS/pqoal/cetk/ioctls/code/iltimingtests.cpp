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


////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/
#include "tuxIoctls.h"
#include <Iltiming.h>

#include "commonIoctlTests.h"

////////////////////////////////////////////////////////////////////////////////
/******************************  Constants and Defines **************************/
// For use with ILTMING Ioctl testing
#define DEFAULT_TIMING_INTERVAL 5

////////////////////////////////////////////////////////////////////////////////
/*******************************  Helper Functions  ***************************/
/*
 * This test fills ILTIMING_MESSAGE structure with invalid
 * iltiming messages, and verifies if the Ioctl can handle them
 */
BOOL testInvalidMessages();

/*
 * This function will test the incorrect inbound parameters for the
 * IOCTLs. All combinations of incorrect values are passed into the IOCTL
 * function and the return values are verified. Any incorrect input parameters
 * should not crash the system.
 */
BOOL CheckIncorrectInboundParameters(DWORD dwIoctlCode,
                                     VOID* pCorrectInBuf, DWORD dwCorrectInSize);

/*
 * This function runs a single parameter checking test on the IOCTL function
 */
BOOL CheckInboundParamSingleTest(DWORD dwIoctlCode,
            ioctlParamCheck *pIoctlParamTest, VOID *pCorrectInBuf,
            DWORD dwCorrectInSize);


////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
 *
 *                                Usage Message
 *
 ******************************************************************************/
/*
 * Prints out the usage message for the iltiming tests. It tells the user
 * what the tests do and also specifies the input that the user needs to
 * provide to run the tests.
 */
TESTPROCAPI IltimingIoctlUsageMessage(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info (_T(""));
    Info (_T("The tests verify if ILTIMING is supported on the platform and if"));
    Info (_T("calls with different inputs yeild results. They verify if"));
    Info (_T("ILTIMING_MSG_ENABLE and ILTIMING_MSG_DISABLE can be called"));
    Info (_T("multiple times and if errant calls to the Ioctl do not crash it."));
    Info (_T(""));
    Info (_T("The tests need to be run in kernel mode. To do this,"));
    Info (_T("append \"-n\" to the command line."));
    Info (_T("The tests do not take any command line parameters."));
    Info (_T(""));

    return TPR_PASS;
}



/*******************************************************************************
 *
 *     ILTIMING IOCTL - CHECK IF IOCTL SUPPORTED
 *
 ******************************************************************************/
/*
 * This test verifies if the ILTIMING Ioctl is supported by the platform and
 * if the Ioctl is able to handle the necessary inputs.
 */
TESTPROCAPI testIfIltimingIoctlSupported(
               UINT uMsg,
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    ILTIMING_MESSAGE ITM;
    DWORD dwBytesRet;
    DWORD dwErrorCode;

    Info (_T(""));
    Info (_T("This test checks if ILTIMING Ioctl is supported by the platform"));
    Info (_T("and if the Ioctl is able to handle the necessary inputs and output the results."));
    Info (_T("The four inputs that IOCTL_HAL_ILTIMING should handle are:"));
    Info (_T("ILTIMING_MSG_ENABLE, ILTIMING_MSG_DISABLE, ILTIMING_MSG_GET_TIMES"));
    Info (_T("and ILTIMING_MSG_GET_PFN."));

    // Set Frequency to the default value
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    // Input ILTIMING_MSG_ENABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_ENABLE..."));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    if(!KernelIoControl( IOCTL_HAL_ILTIMING, & ITM, sizeof(ITM), NULL, 0, & dwBytesRet))
    {
        // retrieve the error code
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE, cannot enable ILTIMING."));
        Error (_T("GetLastError returned %u"), dwErrorCode);
        Error (_T(""));
        Error (_T("Check if IOCTL_HAL_ILTIMING is supported by your platform."));
        Error (_T(""));
        Error (_T("Also, check if you are running the test in kernel mode. To do this,"));
       Error (_T("append \"-n\" to the command line."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));


    // Input ILTIMING_MSG_GET_TIMES
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_GET_TIMES..."));
    ITM.wMsg = ILTIMING_MSG_GET_TIMES;
    if(!KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
    {
        dwErrorCode = GetLastError();
        Error (_T("The Ioctl call failed, cannot get the ISR start and end times."));
        Error (_T("GetLastError returned %u."), dwErrorCode);
        Error (_T("Please check your implementation of IOCTL_HAL_ILTIMING."));
        goto cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));
    Info (_T("The output is: dwIsrTime1 = %u, dwIsrTime2 = %u"),ITM.dwIsrTime1, ITM.dwIsrTime2);


    // Input ILTIMING_MSG_GET_PFN
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_GET_PFN..."));
    ITM.wMsg = ILTIMING_MSG_GET_PFN;
    if(!KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
    {
        dwErrorCode = GetLastError();
        Error (_T("The Ioctl call failed, cannot get the pointer to PerfCountSinceTick function."));
        Error (_T("GetLastError returned %u."), dwErrorCode);
        Error (_T("Please check your implementation of IOCTL_HAL_ILTIMING."));
        goto cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));


    // Input ILTIMING_MSG_DISABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_DISABLE..."));
    ITM.wMsg = ILTIMING_MSG_DISABLE;
    if(!KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
    {
        dwErrorCode = GetLastError();
        Error (_T("The Ioctl call failed, cannot disable ILTIMING."));
        Error (_T("GetLastError returned %u."), dwErrorCode);
        Error (_T("Please check your implementation of IOCTL_HAL_ILTIMING."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));


    // The test passed if we got here
    returnVal = TPR_PASS;

cleanAndReturn:
        Info (_T(""));
        return (returnVal);
}


/*******************************************************************************
 *
 *     ILTIMING IOCTL - TEST ILTIMING ENABLE DISABLE
 *
 ******************************************************************************/
/*
 * This test checks if enabling and disabling ILTIMING multiple times
 * does not cause a problem.
 */
TESTPROCAPI testIltimingEnableDisable(
               UINT uMsg,
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    ILTIMING_MESSAGE ITM = { 0 };
    DWORD dwBytesRet = 0;
    DWORD dwErrorCode = 0;

    Info (_T(""));
    Info (_T("The test calls IOCTL_HAL_ILTIMING multiple times with ILTIMING_MSG_ENABLE"));
    Info (_T("and ILTIMING_MSG_DISABLE set in the iltiming message. The test verifies if all"));
    Info (_T("calls succeed. The test sequence is as follows: enable, disable, enable,"));
    Info (_T("enable, disable, disable."));

    // Set Frequency to the default value
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    // Input ILTIMING_MSG_ENABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_ENABLE..."));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    if(!KernelIoControl( IOCTL_HAL_ILTIMING, & ITM, sizeof(ITM), NULL, 0, & dwBytesRet ))
    {
        // retrieve the error code
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE, cannot enable ILTIMING."));
        Error (_T("GetLastError returned %u"), dwErrorCode);
        Error (_T(""));
        Error (_T("Check if IOCTL_HAL_ILTIMING is supported by your platform."));
        Error (_T(""));
        Error (_T("Also, check if you are running the test in kernel mode. To do this,"));
        Error (_T("append \"-n\" to the command line."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));

    // Input ILTIMING_MSG_DISABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_DISABLE..."));
    ITM.wMsg = ILTIMING_MSG_DISABLE;
    if(!KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
    {
        dwErrorCode = GetLastError();
        Error (_T("The Ioctl call failed, cannot disable ILTIMING."));
        Error (_T("GetLastError returned %u."), dwErrorCode);
        Error (_T("Please check your implementation of IOCTL_HAL_ILTIMING."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));

    // Input ILTIMING_MSG_ENABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_ENABLE..."));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    if(!KernelIoControl( IOCTL_HAL_ILTIMING, & ITM, sizeof(ITM), NULL, 0, & dwBytesRet ))
    {
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE while the expected value is TRUE."));
        Error (_T("Cannot enable ILTIMING. GetLastError returned %u"), dwErrorCode);
        Error (_T("Check your implementation of IOCTL_HAL_ILTIMING."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));

    // Input ILTIMING_MSG_ENABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_ENABLE..."));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    if(!KernelIoControl( IOCTL_HAL_ILTIMING, & ITM, sizeof(ITM), NULL, 0, & dwBytesRet ))
    {
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE while the expected value is TRUE."));
        Error (_T("GetLastError returned %u"), dwErrorCode);
        Error (_T("Check your implementation of IOCTL_HAL_ILTIMING."));
        Error (_T("Multiple calls to Enable Iltiming should return TRUE."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));

    // Input ILTIMING_MSG_DISABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_DISABLE..."));
    ITM.wMsg = ILTIMING_MSG_DISABLE;
    if(!KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
    {
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE while the expected value is TRUE."));
        Error (_T("Cannot disable ILTIMING. GetLastError returned %u."), dwErrorCode);
        Error (_T("Check your implementation of IOCTL_HAL_ILTIMING."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));

    // Input ILTIMING_MSG_DISABLE
    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_DISABLE..."));
    ITM.wMsg = ILTIMING_MSG_DISABLE;
    if(!KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
    {
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE while the expected value is TRUE."));
        Error (_T("GetLastError returned %u."), dwErrorCode);
        Error (_T("Check your implementation of IOCTL_HAL_ILTIMING."));
        Error (_T("Multiple calls to Disable Iltiming should return TRUE."));
        goto  cleanAndReturn;
    }
    Info (_T("Ioctl returned true."));

    // If we got here, the test passed.
    returnVal = TPR_PASS;

cleanAndReturn:

    Info (_T(""));
    return (returnVal);
}


/*******************************************************************************
 *
 *     ILTIMING IOCTL - CHECK INBOUND PARAMETERS
 *
 ******************************************************************************/
/*
 * This test verifies calling the Ioctl with incorrect inbound parameters.
 * The Ioctl should handle them correctly.
 */
TESTPROCAPI
testIltimingIoctlInParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    ILTIMING_MESSAGE ITM;
    DWORD dwBytesRet, dwErrorCode;

    Info (_T(""));
    Info (_T("This test checks passing incorrect inbound parameters to the IOCTL."));
    Info (_T("Combinations of incorrect inbound values are passed into the IOCTL "));
    Info (_T("function and the return values are verified."));
    Info (_T("Any incorrect parameters should not crash the system."));

    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_ENABLE..."));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if(!KernelIoControl( IOCTL_HAL_ILTIMING, & ITM, sizeof(ITM), NULL, 0, & dwBytesRet ))
    {
    // retrieve the error code
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE, cannot enable ILTIMING."));
        Error (_T("GetLastError returned %u"), dwErrorCode);
        Error (_T(""));
        Error (_T("Check if IOCTL_HAL_ILTIMING is supported by your platform."));
        Error (_T(""));
        Error (_T("Also, check if you are running the test in kernel mode. To do this,"));
        Error (_T("append \"-n\" to the command line."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }
    Info (_T("The Ioctl is supported."));

    // Sample parameter to test: enable iltiming
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

   // Test incorrect inbound parameters
    Info (_T(""));
    Info (_T("Checking incorrect inbound parameters..."));
    if(!testInvalidMessages())
    {
        Error (_T(""));
        Error (_T("The Ioctl did not handle invalid iltiming messages correctly."));
        returnVal = TPR_FAIL;
    }

    if((!CheckIncorrectInboundParameters(IOCTL_HAL_ILTIMING,&ITM, sizeof(ITM))))
    {
        Error (_T(""));
        Error (_T("The Ioctl did not handle incorrect inbound parameters correctly."));
        returnVal = TPR_FAIL;
    }

cleanAndReturn:

    Info (_T(""));
    return (returnVal);
}


/*******************************************************************************
 *
 *     ILTIMING IOCTL - CHECK OUTBOUND PARAMETERS
 *
 ******************************************************************************/
/*
 * This test verifies calling the Ioctl with incorrect inbound and outbound parameters.
 * The Ioctl should handle them correctly.
 */

#define TEMP_LARGE_BUFFER 1024

TESTPROCAPI
testIltimingIoctlOutParam(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    ILTIMING_MESSAGE ITM;
    DWORD dwBytesRet, dwErrorCode;

    Info (_T(""));
    Info (_T("This test checks passing incorrect outbound parameters to the IOCTL."));
    Info (_T("Combinations of incorrect outbound values are passed into the IOCTL "));
    Info (_T("function and the return values are verified."));
    Info (_T("Any incorrect parameters should not crash the system."));

    Info (_T(""));
    Info(_T("Calling the Ioctl with ILTIMING_MSG_ENABLE..."));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    //Here we are just checking of IOCTL_HAL_ILTIMING is enabled.  We are not actually
    // performing the test yet.
    if(!KernelIoControl( IOCTL_HAL_ILTIMING, & ITM, sizeof(ITM), NULL, 0, & dwBytesRet ))
    {
    // retrieve the error code
        dwErrorCode = GetLastError();
        Error (_T("Calling the Ioctl returned FALSE, cannot enable ILTIMING."));
        Error (_T("GetLastError returned %u"), dwErrorCode);
        Error (_T(""));
        Error (_T("Check if IOCTL_HAL_ILTIMING is supported by your platform."));
        Error (_T(""));
        Error (_T("Also, check if you are running the test in kernel mode. To do this,"));
        Error (_T("append \"-n\" to the command line."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }
    Info (_T("The Ioctl is supported."));


    // Outbound parameters aren't required for IOCTL_HAL_ILTIMING.
    Info (_T(""));
    Info (_T("The ILTIMING IOCTL does not use the outbound buffer."));
    Info (_T("We will set the outbound parameters same as the inbound"));
    Info (_T("parameters and verify that this does not affect the Ioctl."));
    Info (_T(""));
    Info (_T("Testing ILTIMING_MSG_ENABLE"));
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                &ITM, sizeof(ITM), &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T("Testing ILTIMING_MSG_GET_TIMES"));
    ITM.wMsg = ILTIMING_MSG_GET_TIMES;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                &ITM, sizeof(ITM), &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T("Testing ILTIMING_MSG_GET_PFN"));
    ITM.wMsg = ILTIMING_MSG_GET_PFN;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                &ITM, sizeof(ITM), &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T("Testing ILTIMING_MSG_DISABLE"));
    ITM.wMsg = ILTIMING_MSG_DISABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                &ITM, sizeof(ITM), &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("Passing outbound parameters to IOCTL_HAL_ILTIMING did not"));
    Info (_T("have any effect on the KernelIoControl() call as expected."));
    Info (_T(""));
    // Outbound parameters aren't required for IOCTL_HAL_ILTIMING.
    Info (_T(""));
    Info (_T("The ILTIMING IOCTL does not use the outbound buffer."));
    Info (_T("We'll set it to a large buffer with an incorrect size arg"));
    Info (_T("and verify it does not have any affect."));
    Info (_T("Testing ILTIMING_MSG_ENABLE"));

    DWORD pBuffer[TEMP_LARGE_BUFFER] = {0};
    ITM.wMsg = ILTIMING_MSG_ENABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                pBuffer, sizeof(pBuffer)/3, &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T("Testing ILTIMING_MSG_GET_TIMES"));
    ITM.wMsg = ILTIMING_MSG_GET_TIMES;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                pBuffer, sizeof(pBuffer)/3, &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T("Testing ILTIMING_MSG_GET_PFN"));
    ITM.wMsg = ILTIMING_MSG_GET_PFN;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                pBuffer, sizeof(pBuffer)/3, &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T("Testing ILTIMING_MSG_DISABLE"));
    ITM.wMsg = ILTIMING_MSG_DISABLE;
    ITM.dwFrequency = DEFAULT_TIMING_INTERVAL;

    if (!KernelIoControl (IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM),
                pBuffer, sizeof(pBuffer)/3, &dwBytesRet))
    {
        Error (_T("")); //Blank line
        Error (_T("Calling the ILTIMING IOCTL function with outbound parameters set"));
        Error (_T("to random values returned FALSE, while the expected value is TRUE."));
        returnVal = TPR_FAIL;
        goto  cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("Passing outbound parameters to IOCTL_HAL_ILTIMING did not"));
    Info (_T("have any effect on the KernelIoControl() call as expected."));
    Info (_T(""));
cleanAndReturn:

    Info (_T(""));
    return (returnVal);
}


/*
 * This test fills ILTIMING_MESSAGE structure with invalid
 * iltiming messages, and verifies if the Ioctl can handle them
 */
BOOL testInvalidMessages()
{
    INT returnVal = TRUE;
    ILTIMING_MESSAGE ITM;
    DWORD dwBytesRet;
    WORD wMsg;

    Info (_T(""));
    Info (_T("Calling IOCTL_HAL_ILTIMING with invalid iltiming messages."));

    // Test invalid wMsgs
    for(wMsg = 5; wMsg < 100; wMsg++)  // skip 1-4 as they're defined
    {
        ITM.wMsg = wMsg;
        if(KernelIoControl(IOCTL_HAL_ILTIMING, &ITM, sizeof(ITM), NULL, 0, &dwBytesRet))
        {
            Error (_T(""));
            Error (_T("The Ioctl returned TRUE for the case where ILTIMING_MESSAGE.wMsg = %u."), ITM.wMsg);
            Error (_T("This is an invalid input and the Ioctl is expected to return FALSE."));
            returnVal = FALSE;
        }
    }
    return returnVal;
}


/*
 * This function will test the incorrect inbound parameters for the
 * IOCTLs. All combinations of incorrect values are passed into the IOCTL
 * function and the return values are verified. Any incorrect input parameters
 * should not crash the system.
 *
 * Arguments:
 *      Input: IOCTL code, Input Buffer, Input Buffer Size
 *      Output: None
 *
 * Return value:
 *      TRUE if successful, FALSE otherwise
 */
BOOL CheckIncorrectInboundParameters(DWORD dwIoctlCode,
                                     VOID* pCorrectInBuf, DWORD dwCorrectInSize)
{
    // Assume TRUE, until proven otherwise.
    BOOL returnVal = TRUE;

    Info (_T(""));
    Info (_T("Calling the ILTIMING Ioctl with incorrect inbound parameters."));

    ///////////////////////////Verify the parameters ////////////////////////////
    DWORD dwCount;
    DWORD dwNumTests;
    ioctlParamCheck *pIoctlParamTest;

    dwNumTests = dwNumParamTests-1;
    Info (_T("A total of %u tests will be run on the Inbound parameters."),
                                              dwNumTests);

    // Iterate through all the tests in the array
    for(dwCount=0; dwCount<dwNumTests; dwCount++)
    {
        Info (_T("")); //Blank line
        Info (_T("This is test %u"),dwCount + 1);

        pIoctlParamTest = &ioctlInParamTests[dwCount];

        if(!CheckInboundParamSingleTest(dwIoctlCode, pIoctlParamTest, pCorrectInBuf,
                    dwCorrectInSize))
        {
            Error (_T("The parameter checking test failed."));
            returnVal = FALSE;
        }
        else
        {
            Info (_T("The parameter checking test passed."));
        }
    }

    return returnVal;
}


/*
 * This function runs a single parameter checking test on the IOCTL function
 *
 * Arguments:
 *      Input: IOCTL code, pointer to the test struct, Input Buffer,
 *             Input Buffer Size, Output Buffer, Output Buffer Size
 *      Output: None
 * Return value:
 *      TRUE if print successful, FALSE otherwise
 */
BOOL CheckInboundParamSingleTest(DWORD dwIoctlCode,
            ioctlParamCheck *pIoctlParamTest, VOID *pCorrectInBuf,
            DWORD dwCorrectInSize)
{
    BOOL returnVal = TRUE;

    DWORD dwInBufSize = 0;
    DWORD dwBufSize = 0;

    VOID *pInBuf = NULL, *pAllocateInBuf = NULL;

    DWORD dwBytesRet = 0;
    DWORD *pdwBytesRet = &dwBytesRet;
    BOOL bOutValue = FALSE;

    // Check for null pointer in the input
    if(!pIoctlParamTest)
    {
        Error (_T("Invalid pointer in input arguments."));
        Error (_T("Cannot perform the tests."));
        return FALSE;
    }

    // Get the test description
    Info (_T("Description: %s"), pIoctlParamTest->szComment);
    Info (_T("Set up the inbound parameters..."));

    // Get the input buffer
    // Allocate memory for the max size of input buffer we will use for the test (dwCorrectInSize + 1)
    if(pIoctlParamTest ->dwInBuf != BUF_NULL)
    {
        DWORD dwMaxTestInBufSize = dwCorrectInSize + 1;
        if(dwMaxTestInBufSize < dwCorrectInSize)
        {
            Error (_T("Overflow occured when calculating the Maximum Test Input Buffer Size."));
            Error (_T("The input buffer size is too large %u."), dwCorrectInSize);
            returnVal = FALSE;
            goto cleanAndExit;
        }

        pAllocateInBuf = (VOID*)malloc (dwMaxTestInBufSize * sizeof(BYTE));

        if(!pAllocateInBuf)
        {
            Error (_T("Could not allocate memory for the input buffer."));
            returnVal = FALSE;
            goto cleanAndExit;
        }

        if(!memcpy(pAllocateInBuf, pCorrectInBuf, dwCorrectInSize))
        {
            Error (_T("Could not copy input to the buffer."));
            returnVal = FALSE;
            goto cleanAndExit;
        }
    }

    pInBuf = pAllocateInBuf;

    switch (pIoctlParamTest ->dwInBuf)
    {
    case BUF_NULL:
        dwBufSize = 0;
        break;

    case BUF_CORRECT:
        dwBufSize = dwCorrectInSize;
        break;

    case BUF_CORRECTSIZE_MINUS_ONE:
        dwBufSize = dwCorrectInSize - 1;
        break;

    case BUF_CORRECTSIZE_PLUS_ONE:
        dwBufSize = dwCorrectInSize + 1;
        break;
    }

    // Get the input size
    if(pIoctlParamTest -> dwInBufSize == SIZE_CORRECT)
    {
        dwInBufSize = dwCorrectInSize;
    }
    else if (pIoctlParamTest -> dwInBufSize == SIZE_BUF)
    {
        dwInBufSize = dwBufSize;
    }
    else
    {
        dwInBufSize = 0;
    }


    //Call IOCTL function now
    Info (_T("Call the IOCTL using KernelIoControl function now..."));
    bOutValue = KernelIoControl (dwIoctlCode, pInBuf, dwInBufSize, NULL,
                                               0, pdwBytesRet); 
    DWORD dwGetLastError = GetLastError();

    //Check returned value
    Info (_T("Check the returned value..."));
    if(bOutValue != pIoctlParamTest->bOutput)
    {
        if(bOutValue)
        {
            Error (_T("The IOCTL returned TRUE while the expected value is FALSE."));
            returnVal = FALSE;
        }
        else
        {
        Error (_T("The IOCTL returned FALSE while the expected value is TRUE."));
        returnVal = FALSE;
        }
    }

    //Check GetLastError
    if(pIoctlParamTest->dwErrorCode)
    {
        Info (_T("Check GetLastError..."));

        Info (_T("GetLastError is %u"),dwGetLastError);

        if(pIoctlParamTest->dwErrorCode != dwGetLastError)
        {
            Error (_T("The error code returned is %u while the expected value is %u."),
                                dwGetLastError, pIoctlParamTest->dwErrorCode);
            returnVal = FALSE;
        }
    }

cleanAndExit:

    return returnVal;

}



