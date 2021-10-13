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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
//
// SD Set Card Feature Tests Group is responsible for carrying out the following tests
//
// Function Table Entries are:
//   Test_SCF_ClockRate            Test of SDSetCardFeature adjusting the clock Rate of Card & Host
//   Test_SCF_BusWidth             Test of SDSetCardFeature adjusting the bus width 1-Bit & 4-Bit
//   TestSetCardFeaturesForceReset Tests the SDSetCardFeature API's ability to reset a slot and verifies it by EnableIO
//   TestSetCardFeaturesDeselect   Tests selecting an SD card/slot and verifies with EnableIO
//   TestSetCardFeaturesSelect     Tests deselecting an SD card/slot
//

#include "main.h"
#include "globals.h"
#include <sdcard.h>

//////////////////////////////////////////////////////////////////////////////////////////////////
//Helper: TestSetCardFeature
//Helper Function used by all the SDSetCardFeatureTests
//
// Parameters:
//  PTEST_PARAMS    Test parameters passed to driver. Contains return codes and output buffer, etc.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI TestSetCardFeature(PTEST_PARAMS pTest)
{
    int     iRet = TPR_FAIL;
    BOOL    bRet = TRUE;
    DWORD   length = 0;
    DWORD   verbosity = 0;
    LPCTSTR ioctlName = NULL;
    LPCTSTR tok = NULL;
    LPTSTR  nextTok = NULL;
    TCHAR   sep[] = TEXT("\n");

    g_pKato->Comment(0, TEXT("The bulk of the following test is contained in the test driver: \"SDTst.dll\"."));
    g_pKato->Comment(0, TEXT("The final results will be logged to Kato, but for more information and step by step descriptions it is best if you have a debugger connected."));

    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    {
        g_pKato->Comment(0, TEXT("There is no client test driver.  This test will not be run!"));

        iRet = TPR_ABORT;
        goto Done;
    }

    pTest->sdDeviceType = g_deviceType;
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_SETCARDFEATURE_TST, NULL, 0, pTest, sizeof(TEST_PARAMS), &length, NULL);
    if (bRet)
    {
        if (pTest)
        {
            // Log test result
            iRet = pTest->iResult;
            switch (iRet)
            {
                case TPR_SKIP:
                    g_pKato->Log(LOG_SKIP, TEXT("Driver returned SKIPPED state."));
                    verbosity = LOG_SKIP;
                    break;

                case TPR_PASS:
                    g_pKato->Log(LOG_PASS, TEXT("Driver returned PASSED state."));
                    verbosity = LOG_PASS;
                    break;

                case TPR_FAIL:
                    g_pKato->Log(LOG_FAIL, TEXT("Driver returned FAILED state."));
                    verbosity = LOG_FAIL;
                    break;

                case TPR_ABORT:
                    g_pKato->Log(LOG_ABORT, TEXT("Driver returned ABORTED state."));
                    verbosity = LOG_ABORT;
                    break;

                default:
                    g_pKato->Log(LOG_EXCEPTION, TEXT("Driver returned UNKNOWN state. "));
                    verbosity = LOG_EXCEPTION;
            }

            // Log test messages
            tok = _tcstok_s(pTest->MessageBuffer, sep, &nextTok);
            g_pKato->Comment(verbosity, TEXT("Message returned: %s"), tok);
            tok = _tcstok_s(NULL, sep, &nextTok);
            while (tok != NULL)
            {
                g_pKato->Comment(verbosity, TEXT("\t%s"), tok);
                tok = _tcstok_s(NULL, sep, &nextTok);
            }
        }

        goto Done;
    } // if (bRet)

    // Diagnose why the driver failed to run the requested test
    if (GetLastError() == ERROR_INVALID_PARAMETER)
    {
        g_pKato->Log(TPR_FAIL, TEXT("You have passed in either bad test params, or an invalid size of those params"));
    }
    else if (GetLastError() == ERROR_INVALID_HANDLE)
    {
        g_pKato->Log(LOG_ABORT, TEXT("The Client Driver has unloaded, perhaps the disk has been ejected. Test Aborted."));

        iRet = TPR_ABORT;
        goto Done;
    }
    else
    {
        ioctlName = TranslateIOCTLS(IOCTL_SETCARDFEATURE_TST);
        if (ioctlName != NULL)
        {
            g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_SETCARDFEATURE_TST, ioctlName);
        }
        else
        {
            g_pKato->Log(LOG_FAIL, TEXT("You are calling an IOCTL that does not exist in either the driver or the test DLL"));
        }
    }

Done:
    if (pTest)
    {
        free(pTest);
    }

    return iRet;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_SCF_ClockRate
//Simple test of SDSetCardFeature adjusting the clock Rate
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE       Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI Test_SCF_ClockRate(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->TestFlags = SD_FLAG_CI_CLOCK_RATE;
    pTest->ft = SD_SET_CARD_INTERFACE;
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test calls SDSetCardFeature with the SD_SET_FEATURE_TYPE of type SD_SET_CARD_INTERFACE to adjust the clock rate."));
    g_pKato->Comment(0, TEXT("This test is in two parts. Part 1 verifies that the performance of the card/host is affected by adjusting the clock rate. Part 2 verifies that the clock"));
    g_pKato->Comment(0, TEXT("rates above or below valid increments are adjusted correctly."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Before starting in part 1, the test does the following: First, it verifies that the card is in the transfer state, through a SDCardInfoQuery call. Then it sets"));
    g_pKato->Comment(0, TEXT("the block size to 512 bytes through a synchronous bus request. Then the initial card interface value is gotten with a SDCardInfoQuery call. The clock"));
    g_pKato->Comment(0, TEXT("rate returned is assumed to be the maximum supported clock rate, as the Host should initialize the clock rate to its highest possible value. The test"));
    g_pKato->Comment(0, TEXT("then proceeds to part 1."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("In part 1, the first thing that happens is a 512 block buffer is allocated, and filled with random data. The test then 20 times writes the buffer to the card"));
    g_pKato->Comment(0, TEXT("with a CMD25 synchronous bus request. Each of these writes is timed with a pair of QueryPerformanceCounter calls. The difference between the"));
    g_pKato->Comment(0, TEXT("performance counter values is then converted into micro seconds by multiplying the difference by a million and dividing by the performance frequency."));
    g_pKato->Comment(0, TEXT("After all 20 bus requests, SDSetCardFeature is called to set the clock rate to the lowest possible value. If the value of clock rate returned by"));
    g_pKato->Comment(0, TEXT("SDSetCardFeature is equal to the initial rate/(2^8) the test proceeds. It again times 20 synchronous bus requests. After the second 20 bus requests,"));
    g_pKato->Comment(0, TEXT("the average times at each clock rate are calculated (This is the actually the sum of each time at a clock rate minus the high and low values divided by"));
    g_pKato->Comment(0, TEXT("18). If the average value of the time of the synchronous bus request at the highest clock rate is less than the average time at the slowest rate, this"));
    g_pKato->Comment(0, TEXT("part of the test passes."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("In part 2, a \"for loop\" is used to decrement \"k\" from 8 to 0. Within the loop (for each k) SDSetCardFeature is used to set the clock rate to 50 above the "));
    g_pKato->Comment(0, TEXT("\"initial rate/(2^k)\". For all values of \"k\", the returned value should be \"initial rate/(2^k)\". After each set call feature call the SDCardInfoQuery is called to"));
    g_pKato->Comment(0, TEXT("perform a second check on the adjusted value. It too must return a clock rate of  \"initial rate/(2^k)\". Also within the loop, SDSetCardFeature is called again,"));
    g_pKato->Comment(0, TEXT("this time attempting to set the clock rate to 50 below \"initial rate/(2^k)\". In all but the case where \"k\" equals 8 the returned values form SDSetCardFeatture"));
    g_pKato->Comment(0, TEXT("and SDCardInfoQuery should equal \"initial rate/(2^(k+ 1))\", i.e. half the value returned by setting to a value higher than \"initial rate/(2^k)\". In the case of"));
    g_pKato->Comment(0, TEXT("\"k\" equaling 8. the lowest rate, \"initial rate/(2^(8))\" should be the returned value. If the all the values are exactly as expected, this part passes."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("No matter what happens, at the end of the test, the clock rate is returned to its initial value with a final SDSetCardFeature call."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSetCardFeature(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_SCF_BusWidth
// Simple test of SDSetCardFeature adjusting the bus width 1-Bit & 4-Bit
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE       Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI Test_SCF_BusWidth(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));

        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->ft = SD_SET_CARD_INTERFACE;
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes calls to SDSetCardFeature with the SD_SET_FEATURE_TYPE of type SD_SET_CARD_INTERFACE to adjust the bus width."));
    g_pKato->Comment(0, TEXT("this test verifies that the performance of the card/host is affected by adjusting the bus width and timing writes in 1 an 4 bit modes."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Before starting the main part of the test, it does the following: First, it verifies that the card is in the transfer state, through a SDCardInfoQuery call. Then it"));
    g_pKato->Comment(0, TEXT("sets the block size to 512 bytes through a synchronous bus request. Then the initial card interface value is gotten with a SDCardInfoQuery call. The clock"));
    g_pKato->Comment(0, TEXT("rate returned is assumed to be the maximum supported clock rate, as the Host should initialize the clock rate to its highest possible value. The test"));
    g_pKato->Comment(0, TEXT("then proceeds to the next section of the test."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("In that section, the first thing that happens is a buffer is allocated to fill with the SCR register data. Then the RCA is obtained through a call to"));
    g_pKato->Comment(0, TEXT("SDCardInfoQuery. Then the RCA is used in a synchronous bus request to send the cad into App Command mode. Next a synchronous bus request is made "));
    g_pKato->Comment(0, TEXT("to get the SCR data (ACMD51). If byte 1 in the SCR data has the 1st and third bits (0x5) set, multiple bus widths are supported. If multiple bus widths are not"));
    g_pKato->Comment(0, TEXT("supported the test concludes here. Otherwise it proceeds to the main part of the test."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The next thing that happens is a 512 block buffer is allocated, and filled with random data. The test then 20 times writes the buffer to the card with a CMD25"));
    g_pKato->Comment(0, TEXT("synchronous bus request. Each of these writes is timed with a pair of QueryPerformanceCounter calls. The difference between the performance counter"));
    g_pKato->Comment(0, TEXT("values is then converted into micro seconds by multiplying the difference by a million and dividing by the performance frequency. After all 20 bus requests,"));
    g_pKato->Comment(0, TEXT("SDSetCardFeature is called to toggle the bus width (e.g. switched from 4 bit to 1 bit mode). This switch is verified by a call to SDCardInfoQuery to make sure "));
    g_pKato->Comment(0, TEXT("that the bus width has actually changed. If it has, the test proceeds. It again times 20 synchronous bus requests. After the second 20 bus requests, the "));
    g_pKato->Comment(0, TEXT("average times at each clock rate are calculated (This is the actually the sum of each time at a clock rate minus the high and low values divided by 18). If the"));
    g_pKato->Comment(0, TEXT("average value of the time of the synchronous bus request at 4 bit mode is faster than the average time at 1 bit mode the test passes."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("No matter what happens, at the end of the test, the bus width is returned to its initial value with a final SDSetCardFeature call."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSetCardFeature(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Test: TestSetCardFeaturesForceReset
//Tests the SDSetCardFeature API's ability to reset a slot and verifies it by EnableIO.
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE       Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestSetCardFeaturesForceReset(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->ft = SD_CARD_FORCE_RESET;
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test uses SD_CARD_FORCE_RESET to force reset of an SD card."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test verifies that when we call SD_CARD_FORCE_RESET, that we can enable IO for that card after the reset."));
    g_pKato->Comment(0, TEXT("The test first resets the slot, with SD_CARD_FORCE_RESET.  It then enables the slot and verifies"));
    g_pKato->Comment(0, TEXT("that IO can be enabled with EnableIO.  If EnableIO successfully enabled IO for the card, then a specific"));
    g_pKato->Comment(0, TEXT("bit will be set in the CCCR"));
    g_pKato->Comment(0, TEXT(" "));

    return TestSetCardFeature(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Test: TestSetCardFeaturesSelectDeselect
//Tests the SDSetCardFeature API's ability to select a slot and verifies by EnableIO
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE       Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestSetCardFeaturesSelect(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->ft = SD_CARD_SELECT_REQUEST;
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test uses SD_CARD_SELECT_REQUEST select an SD card."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test verifies that when we call SD_CARD_SELECT_REQUEST, that we can enable IO for that card."));
    g_pKato->Comment(0, TEXT("The test first disables the slot, with SD_CARD_DESELECT_REQUEST.  It then enables the slot and verifies"));
    g_pKato->Comment(0, TEXT("that IO can be enabled with EnableIO.  If EnableIO successfully enabled IO for the card, then a specific"));
    g_pKato->Comment(0, TEXT("bit will be set in the CCCR"));
    g_pKato->Comment(0, TEXT(" "));

    return TestSetCardFeature(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Test: TestSetCardFeaturesDeselect
//Tests the SDSetCardFeature API's ability to deselect a slot
//
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE       Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestSetCardFeaturesDeselect(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->ft = SD_CARD_DESELECT_REQUEST;
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test uses SD_CARD_DESELECT_REQUEST deselect an SD card."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test verifies that when we call SD_CARD_DESELECT_REQUEST that the correct client notifications are sent out."));
    g_pKato->Comment(0, TEXT("We cannot verify that the slot is truly disabled, because this results in DEBUGCHKs failing in the bus driver when"));
    g_pKato->Comment(0, TEXT("we try to send commands to a deselected slot."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSetCardFeature(pTest);
}
