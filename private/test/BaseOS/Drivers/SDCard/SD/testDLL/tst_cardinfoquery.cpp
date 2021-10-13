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
// The SD Card Info Query Test Group is responsible for testing the simple bus
// commands listed.
//
// Function Table Entries are:
//   Test_CIQ_OCR        Test Querying the 32-bit Operating Conditions Registery (OCR)
//   Test_CIQ_CID        Test querying the 16 bytes long Card Identification (CID) register
//   Test_CIQ_CSD        Test querying the 16-bit Card Specific Data (CSD) register
//   Test_CIQ_RCA        Test querying the 16-bit Relative Card Address (RCA) register
//   Test_CIQ_CrdInfce   Test querying the SD_INFO_CARD_INTERFACE_EX struct
//   Test_CIQ_Status     Test querying the the card status, SD_INFO_CARD_STATUS
//   Test_CIQ_HIF_Caps   Test querying for the host interface capabilities
//   Test_CIQ_HBlk_Caps  Test querying the Host Block Capabilities that are closest match to the ones provided
//
//////////////////////////////////////////////////////////////////////////////////////
#include "main.h"
#include "globals.h"
#include <sdcard.h>

//////////////////////////////////////////////////////////////////////////////////////////////////
//Helper: TestSDCardInfoQuery
//Helper Function used by all the simple SDCardInfoQuery tests
//
// Parameters:
//  PTEST_PARAMS    Test parameters passed to driver. Contains return codes and output buffer, etc.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI TestSDCardInfoQuery(PTEST_PARAMS pTest)
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
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_SIMPLE_INFO_QUERY_TST, NULL, 0, pTest, sizeof(TEST_PARAMS), &length, NULL);
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
                    g_pKato->Log(LOG_FAIL, TEXT("Driver returned FAILED state.:"));
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
        } //if (pTest)

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
        ioctlName = TranslateIOCTLS(IOCTL_SIMPLE_INFO_QUERY_TST);
        if (ioctlName != NULL)
        {
            g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_SIMPLE_INFO_QUERY_TST, ioctlName);
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
//Tests: Test_CIQ_OCR
// Simple test for querying the 32-bit Operating Conditions Register (OCR).
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

TESTPROCAPI Test_CIQ_OCR(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
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
    pTest->iT = SD_INFO_REGISTER_OCR;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCardInfoQuery with an Info type of SD_INFO_REGISTER_OCR. This test passes if the"));
    g_pKato->Comment(0, TEXT("SDCardInfoQuery call succeeds and the returned buffer is not Zeroed."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a 4 byte memory buffer to store the OCR data. Assuming that works it zeros the buffer. After "));
    g_pKato->Comment(0, TEXT("that it sets the structSize variable to 4 bytes, and the pCardInfo pointer to the buffer just allocated, and it uses those variables as"));
    g_pKato->Comment(0, TEXT("parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("As stated earlier, If the call succeeds it checks if the retuned buffer does not equal zero. If it does not the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_CID
//Simple test for querying the 16 bytes long Card Identification (CID) register.
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

TESTPROCAPI Test_CIQ_CID(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    if (g_deviceType == SDDEVICE_MMC || g_deviceType == SDDEVICE_MMCHC)
    {
        return TPR_SKIP;
    }

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }
    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->iT = SD_INFO_REGISTER_CID;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test  makes a call to SDCardInfoQuery with an Info type of SD_INFO_REGISTER_CID. This test passes if the parsed"));
    g_pKato->Comment(0, TEXT("CSD register returned is non-zero, and the members are internally constant with the raw register data. This test does not verify a match with"));
    g_pKato->Comment(0, TEXT("a CMD10 bus request to get the CID register. That is done in the CMD10 bus request tests. The raw register data that it does compare the"));
    g_pKato->Comment(0, TEXT("structure members against is itself a member of the SD_INFO_REGISTER_CID structure."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a block of memory the size of the SD_PARSED_REGISTER_CID structure. Assuming that works"));
    g_pKato->Comment(0, TEXT("it zeros the buffer. After that it sets the structSize variable to the size of the SD_PARSED_REGISTER_CID structure, and the "));
    g_pKato->Comment(0, TEXT("pCardInfo pointer to the buffer just allocated for it. Then the test uses those variables as parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After that, the test then verifies that the call was succeeded by checking if a non-zero structure was returned. Assuming it is non-zero, the"));
    g_pKato->Comment(0, TEXT("test goes beyond that and checks if each member in the structure were correctly parsed out of the raw value, by re parsing the data, and"));
    g_pKato->Comment(0, TEXT("comparing against the value in the structure."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_CSD
//Simple test for querying the 16-bit Card Specific Data (CSD) register.
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

TESTPROCAPI Test_CIQ_CSD(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    if ( g_deviceType == SDDEVICE_MMC || g_deviceType == SDDEVICE_MMCHC)
    {
        return TPR_SKIP;
    }

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->iT = SD_INFO_REGISTER_CSD;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCardInfoQuery with an Info type of SD_INFO_REGISTER_CSD. This test passes if the parsed"));
    g_pKato->Comment(0, TEXT("CSD register returned is non-zero, and the members are internally constant with the raw register data. This test does not verify a match with"));
    g_pKato->Comment(0, TEXT("a CMD9 bus request to get the CSD register. That is done in the CMD9 bus request tests. The raw register data that it does compare the"));
    g_pKato->Comment(0, TEXT("structure members against is itself a member of the SD_INFO_REGISTER_CSD structure."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a block of memory the size of the SD_PARSED_REGISTER_CSD structure. Assuming that works"));
    g_pKato->Comment(0, TEXT("it zeros the buffer. After that it sets the structSize variable to the size of the SD_PARSED_REGISTER_CSD structure, and the "));
    g_pKato->Comment(0, TEXT("pCardInfo pointer to the buffer just allocated for it. Then the test uses those variables as parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After that, the test then verifies that the call was succeeded by checking if a non-zero structure was returned. Assuming it is non-zero, the"));
    g_pKato->Comment(0, TEXT("test goes beyond that and checks if each member in the structure were correctly parsed out of the raw value, by re parsing the data, and"));
    g_pKato->Comment(0, TEXT("comparing against the value in the structure."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_RCA
//Simple test for querying the 16-bit Relative Card Address (RCA) register.
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

TESTPROCAPI Test_CIQ_RCA(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
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
    pTest->iT = SD_INFO_REGISTER_RCA;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCardInfoQuery with an Info type of SD_INFO_REGISTER_RCA. This test passes if the"));
    g_pKato->Comment(0, TEXT("RCA register data returned is usable in making a simple synchronous bus request."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a 2 byte memory buffer to store the RCA data. Assuming that works it zeros the buffer. After "));
    g_pKato->Comment(0, TEXT("that it sets the structSize variable to 2 bytes, and the pCardInfo pointer to the buffer just allocated, and it uses those variables as"));
    g_pKato->Comment(0, TEXT("parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After that, the test then verifies that the call was successful by using it to get the card's status register through a CMD13 bus request."));
    g_pKato->Comment(0, TEXT("If that request succeeds, the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_CrdInfce
// Simple test for querying the SD_INFO_CARD_INTERFACE_EX struct which stores info on the physical
// attributes of the SD card and its interface.
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

TESTPROCAPI Test_CIQ_CrdInfce(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
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
    pTest->iT = SD_INFO_CARD_INTERFACE_EX;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCardInfoQuery with an Info type of SD_INFO_CARD_INTERFACE_EX. This test passes if the"));
    g_pKato->Comment(0, TEXT("structure passed in is filled with a non-zero value."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a SD_CARD_INTERFACE_EX structure on the heap. Assuming that works, it zeros the buffer. After "));
    g_pKato->Comment(0, TEXT("that it sets the structSize variable to the size of the SD_CARD_INTERFACE_EX structure, and the pCardInfo pointer to the structure allocated,"));
    g_pKato->Comment(0, TEXT("and it uses those variables as parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("If the call succeeds, another SD_CARD_INTERFACE_EX structure is allocated on the heap. It then zeros the new structures memory,"));
    g_pKato->Comment(0, TEXT(" and compares both structures with a memcmp call. If the SD_CARD_INTERFACE_EX structure is non-zero the test passes."));
    g_pKato->Comment(0, TEXT("If that request succeeds, the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_Status
//Simple test for querying the the card status, SD_INFO_CARD_STATUS.
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

TESTPROCAPI Test_CIQ_Status(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
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
    pTest->iT = SD_INFO_CARD_STATUS;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCardInfoQuery with an Info type of SD_INFO_CARD_STATUS. This test passes if the"));
    g_pKato->Comment(0, TEXT("SDCardInfoQuery call succeeds and the returned buffer is not Zeroed. This test does not verify a match with"));
    g_pKato->Comment(0, TEXT("a CMD13 bus request to get the Status register. That is done in the CMD13 bus request tests."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a 4 byte memory buffer to store the Status info. Assuming that works it zeros the buffer. After "));
    g_pKato->Comment(0, TEXT("that it sets the structSize variable to 4 bytes, and the pCardInfo pointer to the buffer just allocated, and it uses those variables as"));
    g_pKato->Comment(0, TEXT("parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("As stated earlier, If the call succeeds it checks if the retuned buffer does not equal zero. If it does not the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_HIF_Caps
//Simple test querying for the host interface capabilities in a card interface structure.
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

TESTPROCAPI Test_CIQ_HIF_Caps(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
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
    pTest->iT = SD_INFO_HOST_IF_CAPABILITIES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test uses makes a call to SDCardInfoQuery with an Info type of SD_INFO_HOST_IF_CAPABILITIES. This test passes if the"));
    g_pKato->Comment(0, TEXT("structure passed in is filled with a non-zero value, and if the clock rate returned in the card interface appears to be the real maximum"));
    g_pKato->Comment(0, TEXT("clock rate for the host and not just SD_FULL_SPEED_RATE."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a SD_CARD_INTERFACE_EX structure on the heap. Assuming that works, it zeros the buffer. After "));
    g_pKato->Comment(0, TEXT("that it sets the structSize variable to the size of the SD_CARD_INTERFACE_EX structure, and the pCardInfo pointer to the structure allocated,"));
    g_pKato->Comment(0, TEXT("and it uses those variables as parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("If the call succeeds, the test then allocates another SD_CARD_INTERFACE_EX structure on the heap. It then zeros the new structures memory,"));
    g_pKato->Comment(0, TEXT("and compares both structures with a memcmp call. If the SD_CARD_INTERFACE_EX structure is non-zero the test then uses SDCardInfoQuery"));
    g_pKato->Comment(0, TEXT("again this time to get the current card interface. The current card interface should be the maximum clock rate that is supported, as the"));
    g_pKato->Comment(0, TEXT("Standard Host by default, sets the clock rate to the maximum possible rate supported by the Host Controller card. If both clock rates, the"));
    g_pKato->Comment(0, TEXT("the one that is a part of the caps, and the one that is part of the current host conditions are the same, the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: Test_CIQ_HBlk_Caps
//Simple test querying the Host Block Capabilities that are closest match to the ones provided.
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

TESTPROCAPI Test_CIQ_HBlk_Caps(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
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
    pTest->iT = SD_INFO_HOST_BLOCK_CAPABILITY;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCardInfoQuery with an Info type of SD_INFO_HOST_BLOCK_CAPABILITY. This test passes if the"));
    g_pKato->Comment(0, TEXT("block sizes initially set, are correctly adjusted by this call. Unreasonably small and large values are used, and the API must correctly adjust the"));
    g_pKato->Comment(0, TEXT("structure contents."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a SD_HOST_BLOCK_CAPABILITY structure on the heap. Assuming that works, it zeros the buffer. After "));
    g_pKato->Comment(0, TEXT("that it sets structure's the read block size to 0, and the structure's write block size to 0xffff. Then it sets the structSize variable to the size"));
    g_pKato->Comment(0, TEXT("of a SD_HOST_BLOCK_CAPABILITY structure, and the pCardInfo pointer to the buffer just allocated. After that it uses those variables as "));
    g_pKato->Comment(0, TEXT("parameters in the SDCardInfoQuery call."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("If the call succeeds, the test then checks both block sizes in the returned structure. If the read block size is greater than zero, and the write"));
    g_pKato->Comment(0, TEXT("block sizes has been changed to something smaller than 0xffff, the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSDCardInfoQuery(pTest);
}
