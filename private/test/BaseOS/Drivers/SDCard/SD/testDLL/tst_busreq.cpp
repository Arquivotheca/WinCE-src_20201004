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
// The SD Bus Request Test Group is responsible for testing the simple bus
// command listed below, both synchronously and asynchronosuly. When dwUserData
// is set to 0 the test runs synchronously and when it is set to 1 it runs
// asynchronosuly. See the ft.h file for more detail.
//
// Function Table Entries are:
//   TestCMD7        Simple Test of CMD7 (SELECT_DESELECT_CARD)
//   TestCMD9        Simple Test of CMD9 (SEND_CSD)
//   TestCMD10       Simple Test of CMD10 (SEND_CID)
//   TestCMD12       Simple Test of CMD12 after write (SD_CMD_STOP_TRANSMISSION)
//   TestCMD12_2     Simple Test of CMD12_2 after read (SD_CMD_STOP_TRANSMISSION)
//   TestCMD13       Simple Test of CMD13 (SEND_STATUS) 
//   TestCMD16       Simple Test of CMD16 (SET_BLOCKLEN) 
//   TestCMD17       Simple Test of CMD17 (READ_SINGLE_BLOCK)
//   TestCMD18       Simple Test of CMD18 (READ_MULTIPLE_BLOCK)
//   TestCMD24       Simple Test of CMD24 (WRITE_BLOCK)
//   TestCMD25       Simple Test of CMD25 (WRITE_MULTIPLE_BLOCK) 
//   TestCMD32       Simple Test of CMD32 (ERASE_WR_BLK_START) 
//   TestCMD33       Simple Test of CMD33 (ERASE_WR_BLK_END) 
//   TestCMD38       Simple Test of CMD38 (ERASE) 
//   TestACMD13      Simple Test of ACMD13 (SD_STATUS)
//   TestACMD51      Simple Test of ACMD51 (SEND_SCR)
//
//////////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <sdcard.h>
#include "resource.h"
#include "dlg.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
//Helper: TestSimpleRequests
//Helper Function used by all the simple bus request tests
//
// Parameters:
//  PTEST_PARAMS    Test parameters passed to driver. Contains return codes and output buffer, etc.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI TestSimpleRequests(PTEST_PARAMS pTest)
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

    // Globals and argument validation
    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    {
        g_pKato->Comment(0, TEXT("There is no client test driver. This test will not be run!"));

        iRet = TPR_ABORT;
        goto Done;
    }

    // Instruct the driver to carry out the equested test
    pTest->sdDeviceType = g_deviceType;
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_SIMPLE_BUSREQ_TST, NULL, 0, pTest, sizeof(TEST_PARAMS), &length, NULL);
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

            // Log the messages
            tok = _tcstok_s(pTest->MessageBuffer, sep, &nextTok);
            if (tok != NULL)
            {
                g_pKato->Comment(verbosity, TEXT("Message returned: %s"), tok);

                tok = _tcstok_s(NULL, sep, &nextTok);
                while (tok != NULL)
                {
                    g_pKato->Comment(verbosity, TEXT("\t%s"), tok);
                    tok = _tcstok_s(NULL, sep, &nextTok);
                }
            }

            goto Done;
        }
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
        ioctlName = TranslateIOCTLS(IOCTL_SIMPLE_BUSREQ_TST);
        if (ioctlName != NULL)
        {
            g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_SIMPLE_BUSREQ_TST, ioctlName);
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
//Tests:TestCMD7
//Simple Synchronous and Asynchronous Bus Request Tests using CMD7, (SELECT_DESELECT_CARD
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

TESTPROCAPI TestCMD7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }

    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_SELECT_DESELECT_CARD;

    g_pKato->Comment(0, TEXT("The following test uses a CMD7 (SELECT_DESELECT_CARD) bus request to reselect the current SD Memory card. The bus request selecting"));
    g_pKato->Comment(0, TEXT("the card in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test actually makes two CMD7 bus requests. The first deselects the cads, but only if the card was initially selected (which it should be). Before"));
    g_pKato->Comment(0, TEXT("the deselection, the test verifies that the card is in the TRANSFER state (through a call to  SDCardInfoQuery). If it is, it performs the deselection "));
    g_pKato->Comment(0, TEXT("with a %S bus request. If that succeeds, or if the card just happened to already be in the STANDBY state, the test proceeds. The heart of "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("the test is the %S bus request to reselect the card. That request happens next."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("It then verifies that the CMD7 request succeeded, by getting the status register again through a call to SDCardInfoQuery. The current state is gleaned"));
    g_pKato->Comment(0, TEXT("from the returned status info. If the card is in the TRANSFER state again the test succeeds."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD9
// Simple Synchronous and Asynchronous Bus Request Tests using CMD9, (SEND_CSD, to obtain
// the SD Card Specific Data (CSD) register.
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

TESTPROCAPI TestCMD9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_SEND_CSD;

    g_pKato->Comment(0, TEXT("The following test uses a CMD9 (SEND_CSD) bus request to get the contents of the CSD register from an inserted SD memory card. "));
    g_pKato->Comment(0, TEXT("The bus request requesting the CSD register in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first  makes a SDCardInfoQuery call to get the card's relative address. Then it uses that value to send a bus request to get the CSD "));
    g_pKato->Comment(0, TEXT("register from the card. Next, it gets the status register to determine what state the card is in. In order for a CMD9 bus request to be performed "));
    g_pKato->Comment(0, TEXT("the card must be in the STANDBY state. if the card is in the TRANSFER state, the test puts the card into the STANDBY state by performing a"));
    g_pKato->Comment(0, TEXT("CMD7 to deselect the card. If the card was not in the TRANFER or STANDBY state the test is skipped. Otherwise, once in the STANDBY state"));
    g_pKato->Comment(0, TEXT("the CMD9 is performed to get the CSD register."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("It then compares the value obtained from the card with the value cached in the bus register. A mismatch would be one of many reasons this test"));
    g_pKato->Comment(0, TEXT("could fail. After the comparison the test puts the card back into the TRANSFER state by reselecting the card."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD10
// Simple Synchronous or Asynchronous Bus Request Tests using CMD10, to obtain the content of the
// SD card Id register.
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

TESTPROCAPI TestCMD10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_SEND_CID;

    g_pKato->Comment(0, TEXT("The following test uses a CMD10 (SEND_CID) bus request to get the contents of the CID register from an inserted SD memory card. "));
    g_pKato->Comment(0, TEXT("The bus request requesting the CID register in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first  makes a SDCardInfoQuery call to get the card's relative address. Then it uses that value to send a bus request to get the CID "));
    g_pKato->Comment(0, TEXT("register from the card. Next, it gets the status register to determine what state the card is in. In order for a CMD10 bus request to be performed"));
    g_pKato->Comment(0, TEXT("the card must be in the STANDBY state. if the card is in the TRANSFER state, the test puts the card into the STANDBY state by performing a"));
    g_pKato->Comment(0, TEXT("CMD7 to deselect the card. If the card was not in the TRANFER or STANDBY state the test is skipped. Otherwise, once in the STANDBY state"));
    g_pKato->Comment(0, TEXT("the CMD10 is performed to get the CID register."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("It then compares the value obtained from the card with the value cached in the bus register. A mismatch would be one of many reasons this test"));
    g_pKato->Comment(0, TEXT("could fail. After the comparison the test puts the card back into the TRANSFER state by reselecting the card."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD12
// Simple Synchronous and Asynchronous Bus Request Tests using CMD12 after a multi-block write, 
// SD_CMD_STOP_TRANSMISSION.
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

TESTPROCAPI TestCMD12(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }
    
    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->TestFlags = SD_FLAG_WRITE;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags |= SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_STOP_TRANSMISSION;

    g_pKato->Comment(0, TEXT("The following test uses a CMD12 (SD_CMD_STOP_TRANSMISSION) to return the card to the TRANS state from the RCV state. The bus request"));
    g_pKato->Comment(0, TEXT("sending the stop transmission command in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first allocates a response buffer. Then makes a %S CMD16 bus request to set the block length to 512 bytes. Then it allocates a 1536 "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("byte buffer which it fills with random bytes. Assuming that allocation was successful, it makes a %S CMD25 bus request to perform a 3 block"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("write to the SDMemory card. One special thing about that bus request is that the SD_AUTO_ISSUE_CMD12 was NOT set. Before moving on to"));
    g_pKato->Comment(0, TEXT("performing the %S CMD12 bus request, the test verifies that the card is still in the RECEIVING state (via SDCardInfoQuery). If it is not the test fails,"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("and bails. Otherwise, the CMD12 bus request is made."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After the CMD12 bus request, SDCardInfoQuery is again called. If the card is back in the TRANSFER state the test passes, if it is not the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD12_2
// Simple Synchronous and Asynchronous Bus Request Tests using CMD12 after a multi-block read, 
// SD_CMD_STOP_TRANSMISSION.
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

TESTPROCAPI TestCMD12_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_STOP_TRANSMISSION;

    g_pKato->Comment(0, TEXT("The following test uses a CMD12 (SD_CMD_STOP_TRANSMISSION) to return the card to the TRANS state from the DATA state. The bus request"));
    g_pKato->Comment(0, TEXT("sending the stop transmission command in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first allocates a response buffer. Then makes a %S CMD16 bus request to set the block length to 512 bytes. Then it allocates a 1536 "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("byte buffer which it zeros. Assuming that allocation was successful, it makes a %S CMD25 bus request to perform a 3 block read from the"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("SDMemory card. One special thing about that bus request is that the SD_AUTO_ISSUE_CMD12 was NOT set. Before moving on to performing the"));
    g_pKato->Comment(0, TEXT("%S CMD12 bus request, the test verifies that the card is still in the DATA state (via SDCardInfoQuery). If it is not the test fails, and bails."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("Otherwise, the CMD12 bus request is made."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After the CMD12 bus request, SDCardInfoQuery is again called. If the card is back in the TRANSFER state the test passes, if it is not the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD13
//Simple Synchronous and Asynchronous Bus Request Tests using CMD13
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

TESTPROCAPI TestCMD13(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));

    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_SEND_STATUS;

    g_pKato->Comment(0, TEXT("The following test uses a CMD13 (SEND_STATUS) bus request to get the contents of the Status register from an inserted SD memory card. "));
    g_pKato->Comment(0, TEXT("The bus request requesting the Status register in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first  makes a SDCardInfoQuery call to get the card's relative address. The Relative address will be used in the CMD13 bus request as a"));
    g_pKato->Comment(0, TEXT("part of the argument. Then it allocates memory for the bus request response. If that call fails the test fails because the bus request response is the"));
    g_pKato->Comment(0, TEXT("means that a CMD13 transfers status bits to the caller. Then the %S CMD13 bus request is performed."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("If that request succeeds, the status bits are parsed from the R1 response. If the result is non-zero the test passes. The test will also dump the "));
    g_pKato->Comment(0, TEXT("status register's contents into the log so further examination can be made."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD16
//Simple Synchronous and Asynchronous Bus Request Tests using CMD16, (SET_BLOCKLEN, to set the
// block length for the block read/write.
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

TESTPROCAPI TestCMD16(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if ( (g_deviceType == SDDEVICE_MMC) || (g_deviceType == SDDEVICE_MMCHC))
    {
        return TPR_SKIP;
    }

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_SET_BLOCKLEN;

    g_pKato->Comment(0, TEXT("The following test uses a CMD16 (SET_BLOCKLEN) bus request to set the length of blocks for block memory reads and writes. "));
    g_pKato->Comment(0, TEXT("The bus request setting the block length in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test determines an acceptable block length based on card an host controller capabilities. It tries to find a non-easy (something other than "));
    g_pKato->Comment(0, TEXT("512 byte) block length, but it will use that length if no other ones are supported. It is possible that the card will only support multiples of 512"));
    g_pKato->Comment(0, TEXT("byte size blocks. In that case it will try if the card and Host Controller will support it, to use a 1024-byte size block. Ideally, it will try to use "));
    g_pKato->Comment(0, TEXT("a block size that is not a multiple of 512. "));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After setting the block size it will attempt to verify that the block size is used correctly by writing three blocks of random data to the SD card,"));
    g_pKato->Comment(0, TEXT(" and reading them back. If the input data matched the output data the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD17
//Simple Synchronous and Asynchronous Bus Request Tests using CMD17, (READ_SINGLE_BLOCK
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

TESTPROCAPI TestCMD17(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_READ_SINGLE_BLOCK;

    g_pKato->Comment(0, TEXT("The following test uses a CMD17 (READ_SINGLE_BLOCK) bus request to read a single block of data from a memory card. The bus request"));
    g_pKato->Comment(0, TEXT("reading from card memory in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test sets the block lengths to be used by the read and write operation in the test to 512 bytes, as all cards must support that"));
    g_pKato->Comment(0, TEXT("length. Then it writes three blocks of random data to memory. Finally, it reads back the second block."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After reading the block it attempts to verify a successful read, by comparing the data to the second block that was written originally. As this is"));
    g_pKato->Comment(0, TEXT(" a very basic test it does not try reading data across block lines on the card."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD18
//Simple Synchronous and Asynchronous Bus Request Tests using CMD18, (READ_MULTIPLE_BLOCK
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

TESTPROCAPI TestCMD18(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_READ_MULTIPLE_BLOCK;

    g_pKato->Comment(0, TEXT("The following test uses a CMD18 (READ_MULTIPLE_BLOCK) bus request to read a multiple blocks of data from a memory card. The bus request"));
    g_pKato->Comment(0, TEXT("reading from card memory in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test sets the block lengths to be used by the read and write operation in the test to 512 bytes, as all cards must support that"));
    g_pKato->Comment(0, TEXT("length. Then it writes five blocks of random data to memory. Finally, it reads back the second and third blocks back from memory. The flag on the"));
    g_pKato->Comment(0, TEXT("bus request that reads multiple blocks from the card, is set to: SD_AUTO_ISSUE_CMD12. This is so I do not have to worry about returning the"));
    g_pKato->Comment(0, TEXT("card to the transfer state."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After reading the block it attempts to verify a successful read, by comparing the data to the second and third blocks that were written originally."));
    g_pKato->Comment(0, TEXT("As this is a very basic test it does not try reading data across block lines on the card."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD24
//Simple Synchronous and Asynchronous Bus Request Tests using CMD24, (WRITE_BLOCK
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

TESTPROCAPI TestCMD24(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_WRITE_BLOCK;

    g_pKato->Comment(0, TEXT("The following test uses a CMD24 (WRITE_BLOCK) bus request to a single block of data to a SD memory card. The bus request writing to"));
    g_pKato->Comment(0, TEXT("the card in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test sets the block lengths to be used by the read and write operation in the test to 512 bytes, as all cards must support that length. Then"));
    g_pKato->Comment(0, TEXT("it erases the first five blocks of memory on the card. Then it writes to the second block on the memory card (block # 1) a blocks worth of "));
    g_pKato->Comment(0, TEXT("random data."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After writing the block it attempts to verify a successful write, by reading back the first five blocks from the card. It then compares that buffer"));
    g_pKato->Comment(0, TEXT("to a simulation that has first had all the bits set low or high (depending on what the card's SCR register says is it does in erase operations), "));
    g_pKato->Comment(0, TEXT("and then had the buffer that was written copied in at the second block through a memcpy operation. As this is a very basic test it does not"));
    g_pKato->Comment(0, TEXT("try writing data across block lines on the card."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD25
//Simple Synchronous and Asynchronous Bus Request Tests using CMD25, (WRITE_MULTIPLE_BLOCK
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

TESTPROCAPI TestCMD25(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;

    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_WRITE_MULTIPLE_BLOCK;

    g_pKato->Comment(0, TEXT("The following test uses a CMD24 (WRITE_MULTIPLE_BLOCK) bus request to a three blocks of data to a SD memory card. The bus request"));
    g_pKato->Comment(0, TEXT("writing to the card in this test is %S. "), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test sets the block lengths to be used by the read and write operation in the test to 512 bytes, as all cards must support that length. Then"));
    g_pKato->Comment(0, TEXT("it erases the first five blocks of memory on the card. Then it writes to the 2nd-4th blocks on the memory card (block #s 1-5) three blocks worth "));
    g_pKato->Comment(0, TEXT("of random data."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After writing the blocks it attempts to verify a successful write, by reading back the first five blocks from the card. It then compares that buffer"));
    g_pKato->Comment(0, TEXT("to a simulation that has first had all the bits set low or high (depending on what the card's SCR register says is it does in erase operations), "));
    g_pKato->Comment(0, TEXT("and then had the buffer that was written copied in at the second block through a memcpy operation. As this is a very basic test it does not"));
    g_pKato->Comment(0, TEXT("try writing data across block lines on the card."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD32
//Simple Synchronous and Asynchronous Bus Request Tests using CMD32, (ERASE_WR_BLK_START
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

TESTPROCAPI TestCMD32(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_ERASE_WR_BLK_START;

    g_pKato->Comment(0, TEXT("The following test uses a CMD32 (ERASE_WR_BLK_START) bus request to set the beginning bound of an erase region in the SD card's"));
    g_pKato->Comment(0, TEXT("memory. The bus request that sets this address is %S. The test never actually performs an erase. It sets the erase start block, but then"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("cancels the erase."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test actually does very little before the %S bus request to set the start block of an erase. Because erase calls deal in memory units"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("of blocks, it first performs a bus request to set the block length of the card to 512 blocks. It then sets variables that will be used as parameters"));
    g_pKato->Comment(0, TEXT("in the CMD32 bus request, including the argument value that sets the memory address of this block to be zero. The test than implements the CMD32"));
    g_pKato->Comment(0, TEXT("bus request."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test verifies that the request works somewhat indirectly. It performs another bus request (another set block length call), and then studies the"));
    g_pKato->Comment(0, TEXT("status info from the response. If the ERASE_RESET flag is set, the test passes"));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD33
//Simple Synchronous and Asynchronous Bus Request Tests using CMD33, ERASE_WR_BLK_END
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

TESTPROCAPI TestCMD33(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_ERASE_WR_BLK_END;

    g_pKato->Comment(0, TEXT("The following test uses a CMD33 (ERASE_WR_BLK_END) bus request to set the ending bound of an erase region in the SD card's"));
    g_pKato->Comment(0, TEXT("memory. The bus request that sets this address is %S. The test, unlike the CMD32 tests, does actually perform an erase."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test has to do very little before the CMD33 bus request. It sets the block lengths to be used in the later bus requests. Then it sets the start"));
    g_pKato->Comment(0, TEXT("erase block. Then it sets the address of the end erase block (In this case 512, meaning that 2 blocks will be erased later). Then the %S CMD33"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("bus request is made."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test verifies that the request works by performing an erase. It then checks that the blocks were erased, by performing an ACMD51 bus request to"));
    g_pKato->Comment(0, TEXT("get the erase bit value. Then it reads the blocks that were erased back from memory, and checks if the bytes in the first two blocks are  uniformly high "));
    g_pKato->Comment(0, TEXT("in all there bytes or low, based on the results of the previous ACMD51 bus request. This test does not check outside the erase bounds to verify if more"));
    g_pKato->Comment(0, TEXT(" blocks have been erased. For that you will need to run the \"Simple %S Test of CMD38\" test."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestCMD38
//Simple Synchronous and Asynchronous Bus Request Tests using CMD38
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

TESTPROCAPI TestCMD38(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS;
    }
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_CMD_ERASE;

    g_pKato->Comment(0, TEXT("The following test uses a CMD38 (ERASE) bus request to set erase blocks of memory between (and including) two previously set erase bounds. The"));
    g_pKato->Comment(0, TEXT("bus request that performs the erase is %S."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first attempts to allocate a response buffer, then it sets the block length to 512 bytes using a %S bus request. Then it allocates"), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT("memory for 20 blocks worth of data, and fills it with a non random character string. Next it writes those twenty blocks of data to the SD memory card"));
    g_pKato->Comment(0, TEXT("with a CMD25 bus request starting at an address of zero. Then because there have been issues with certain SDMemory cards, it allocates memory for,"));
    g_pKato->Comment(0, TEXT("and then reads data into a buffer from the memory card. If the written and read data are identical, the test proceeds onto the next step. That step"));
    g_pKato->Comment(0, TEXT("involves setting the start an end erase limits through CMD32 and CMD33 bus requests. The start erase block is set to an address of 7168 (the 15th"));
    g_pKato->Comment(0, TEXT("block). The end erase block is set to an address of 8704 (the 18th block). The %S CMD38 bus request is then made."), (lpFTE->dwUserData == 0) ? "synchronous" : "asynchronous");
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After the ERASE bus request, the test moves to verify that the erase happened correctly, and in the correct location by performing a simulation within"));
    g_pKato->Comment(0, TEXT("a new memory buffer. First the test determines if the erase bits go high or low on the memory card by performing an SDCardInfoQuery call to get the"));
    g_pKato->Comment(0, TEXT("relative card address. Then it uses the RCA to send a CMD55 bus request to put the card into APP command mode. Finally, it issues an ACMD51 bus"));
    g_pKato->Comment(0, TEXT("request to get the SCR register, and from that it gleans whether bits go high or low on erases. After that is determined, the first 20 blocks are read"));
    g_pKato->Comment(0, TEXT("again into the buffer used earlier for that purpose. Then a third and final buffer of the same size (20 blocks) is created. This simulation buffer is "));
    g_pKato->Comment(0, TEXT("filled with the data from the buffer that was used to write to the card originally. Then all the bits in blocks 15-18 (inclusive) are set high or low in that"));
    g_pKato->Comment(0, TEXT("simulation buffer based on the earlier querying of the SCR register. The test then succeeds or fails if the simulation matches bit for bit the buffer"));
    g_pKato->Comment(0, TEXT("containing the blocks that had been read from the card"));

    return TestSimpleRequests(pTest);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestACMD13
//Simple Synchronous and Asynchronous Bus Request Tests using ACMD13, (SD_STATUS
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

TESTPROCAPI TestACMD13(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;

    if ( (g_deviceType == SDDEVICE_MMC) || (g_deviceType == SDDEVICE_MMCHC))
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
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_ACMD_SD_STATUS;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS | SD_FLAG_ALT_CMD;
    }
    else
    {
        pTest->TestFlags = SD_FLAG_ALT_CMD;
    }

    g_pKato->Comment(0, TEXT("The following test uses a ACMD13 (SD_STATUS) bus request to get the contents of the expanded Status register info from an"));
    g_pKato->Comment(0, TEXT("inserted SD memory card. The bus request requesting the expanded Status register info in this test is synchronous."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first  makes a SDCardInfoQuery call to get the card's relative address. The Relative address will be used in the CMD55 bus request as"));
    g_pKato->Comment(0, TEXT("a part of the argument. Then it allocates memory for the bus request response. Unlike the CMD13 tests, if that call fails the test does not fail"));
    g_pKato->Comment(0, TEXT("because the bus request response is not necessary to get the status bits. The test then performs a CMD55 bus request to set the card into"));
    g_pKato->Comment(0, TEXT("Application command mode. After that, it allocates memory for a 64-byte buffer to contain the supplemental status bits. Assuming this allocation"));
    g_pKato->Comment(0, TEXT("succeeds, the synchronous ACMD13 bus request is made."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("If that request works, the status bits are parsed. The test then determines if a valid bus width is returned in the filled buffer. If a width"));
    g_pKato->Comment(0, TEXT("of 1 or 4 bits are returned the test passes. And in that case, the supplemental status register info is dumped into the log so further examination"));
    g_pKato->Comment(0, TEXT("can be made."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestACMD51
//Simple Synchronous and Asynchronous Bus Request Tests using ACMD51, (SEND_SCR
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

TESTPROCAPI TestACMD51(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if ( (g_deviceType == SDDEVICE_MMC) || (g_deviceType == SDDEVICE_MMCHC))
    {
        return TPR_SKIP;
    }

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->BusRequestCode = SD_ACMD_SEND_SCR;
    if (lpFTE->dwUserData == 0)
    {
        pTest->TestFlags = SD_FLAG_SYNCHRONOUS | SD_FLAG_ALT_CMD;
    }
    else
    {
        pTest->TestFlags = SD_FLAG_ALT_CMD;
    }

    g_pKato->Comment(0, TEXT("The following test uses a ACMD51 (SEND_SCR) bus request to get the contents of the SCR register. The bus request used to get the"));
    g_pKato->Comment(0, TEXT("contents of the SCR register in this test is synchronous."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first  makes a SDCardInfoQuery call to get the card's relative address. The Relative address will be used in the CMD55 bus request as"));
    g_pKato->Comment(0, TEXT("a part of the argument. Then it allocates memory for the bus request response. The test then performs a CMD55 bus request to set the card into"));
    g_pKato->Comment(0, TEXT("Application command mode. After that, it allocates memory for a 64-byte buffer to contain the SCR register bits. Assuming this allocation"));
    g_pKato->Comment(0, TEXT("succeeds, the synchronous ACMD51 bus request is made."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The SCR register contains a lot of information, including details on if the bits go high or low during erase operations. If ACMD51 request works,"));
    g_pKato->Comment(0, TEXT("the second most significant bit on the second byte is checked. If it is high the erase bits should go high, if it is low the erase bits should go low. "));
    g_pKato->Comment(0, TEXT("In order to verify that the SCR bits have been returned successfully the test sets the block length to the universally supported 512 bytes, erases"));
    g_pKato->Comment(0, TEXT("the first block on the card, allocates memory to read the first block from the card into, and reads the block back from the card. If the bits in the"));
    g_pKato->Comment(0, TEXT("first byte on the read back buffer are all high or low as appropriate, the test passes."));
    g_pKato->Comment(0, TEXT(" "));

    return TestSimpleRequests(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                        ///////
///////                                  Cancel Bus Request Tests                              ///////
///////                                                                                        ///////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
//Test: CancelBusRequestTest
//Test of canceling bus requests
//
// Parameters:
//  PTEST_PARAMS    Test parameters passed to driver. Contains return codes and output buffer, etc.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestCancelBusRequest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    int     iRet = TPR_FAIL;
    BOOL    bRet = TRUE;
    DWORD   length;
    DWORD   verbosity = 0;;
    LPCTSTR ioctlName = NULL;
    LPCTSTR tok = NULL;
    LPTSTR  nextTok = NULL;
    TCHAR   sep[] = TEXT("\n");
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;

    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    {
        g_pKato->Comment(0, TEXT("There is no client test driver, test will not run!"));

        return TPR_ABORT;
    }

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->iT = SD_INFO_TYPE_COUNT;
    pTest->sdDeviceType= g_deviceType;

    g_pKato->Comment(0, TEXT("The following test makes a call to SDCancelBusRequest to cancel queued synchronous bus requests. It queues two requests, and if it can cancel"));
    g_pKato->Comment(0, TEXT("at least the second request the test passes"));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("This test first allocates two string buffers. It then fills the buffers with 2 unique names to be used later in creating named events. These names"));
    g_pKato->Comment(0, TEXT("are a concatenation of 'BusRequestComplete' and an index number. Assuming the names were allocated correctly, they are then used to create two "));
    g_pKato->Comment(0, TEXT("distinct events. These events will be used later by the completion callback to signal to the test function that the callback has been run. After the"));
    g_pKato->Comment(0, TEXT("events are created, a response buffer is allocated. Then a synchronous CMD16 bus request is made to set the block length. After that, a 5120 byte"));
    g_pKato->Comment(0, TEXT("memory buffer is created to later use to write to the memory card. Then the priority of the test driver's thread is moved up. To do this a pseudo handle"));
    g_pKato->Comment(0, TEXT("for the current thread is gotten with GetCurrentThread, then the current thread's priority is gotten (and cached) with CeGetThreadPriority, and finally"));
    g_pKato->Comment(0, TEXT("the driver's thread priority is set with CeSetThreadPriority. After the thread's priority is reset, two asynchronous CMD25 bus requests are queued with"));
    g_pKato->Comment(0, TEXT("SDBusRequest. If they both succeed, SDCancelBusRequest is used to attempt to cancel both of them."));
    g_pKato->Comment(0, TEXT(""));
    g_pKato->Comment(0, TEXT("If the second request is canceled, the test succeeds. The first request is likely not cancelable, and if that call fails the test does not fail. After the"));
    g_pKato->Comment(0, TEXT("calls to SDCancelBusRequest, the tread priority is reset to its original value with another CeSetThreadPriority call. Then one last check is made."));
    g_pKato->Comment(0, TEXT("The test waits on both events set at the beginning to return. If the canceled request returns SD_API_STATUS_CANCELED , an the non-canceled"));
    g_pKato->Comment(0, TEXT("request succeeds, the test passes."));
    g_pKato->Comment(0, TEXT(""));
    g_pKato->Comment(0, TEXT("The bulk of the following test is contained in the test driver: \"SDTst.dll\"."));
    g_pKato->Comment(0, TEXT("The final results will be logged to Kato, but for more information and step by step descriptions it is best if you have a debugger connected."));

    // Instruct the driver to carry out the equested test
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_CANCEL_BUSREQ_TST, NULL, 0, pTest, sizeof(TEST_PARAMS), &length, NULL);
    if (bRet)
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

        // Log the messages
        tok = _tcstok_s(pTest->MessageBuffer, sep, &nextTok);
        g_pKato->Comment(verbosity, TEXT("Message returned: %s"), tok);
        tok = _tcstok_s(NULL, sep, &nextTok);
        while (tok != NULL)
        {
            g_pKato->Comment(verbosity, TEXT("\t%s"), tok);
            tok = _tcstok_s(NULL, sep, &nextTok);
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
        ioctlName = TranslateIOCTLS(IOCTL_CANCEL_BUSREQ_TST);
        if (ioctlName != NULL)
        {
            g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_CANCEL_BUSREQ_TST, ioctlName);
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

//////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                        ///////
///////                         ACMD Retry Request Test                                        ///////
///////                                                                                        ///////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
//Test: ACMDRetryTest
//Test of canceling bus requests
//
// Parameters:
//  PTEST_PARAMS    Test parameters passed to driver. Contains return codes and output buffer, etc.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

//////////////////////////////////////////////////////////////////////////////////////////////////
//Helper: TestOddRWs
//Helper Function used by all the simple bus request tests
//
// Parameters:
//  PTEST_PARAMS    Test parameters passed to driver. Contains return codes and output buffer, etc.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI TestOddRWs(PTEST_PARAMS pTest)
{
    int iRet = TPR_FAIL;
    DWORD   length;
    LPCTSTR ioctlName = NULL;
    BOOL bRet = TRUE;
    TCHAR sep[] = TEXT("\n");
    LPCTSTR tok = NULL;
    LPTSTR nextTok = NULL;
    DWORD verbosity = 0;

    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    { 
        g_pKato->Comment(0, TEXT("There is no client test driver, test will not run!"));

        iRet = TPR_ABORT;
        goto Done;
    }

    g_pKato->Comment(0, TEXT("The bulk of the following test is contained in the test driver: \"SDTst.dll\"."));
    g_pKato->Comment(0, TEXT("The final results will be logged to Kato, but for more information and step by step descriptions it is best if you have a debugger connected."));

    pTest->sdDeviceType = g_deviceType;
    bRet = DeviceIoControl(g_hTstDriver, IOCTL_RW_MISALIGN_OR_PARTIAL_TST, NULL, 0, pTest, sizeof(TEST_PARAMS), &length, NULL);
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
        }

         goto Done;
    }

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
        ioctlName = TranslateIOCTLS(IOCTL_RW_MISALIGN_OR_PARTIAL_TST);
        if (ioctlName != NULL)
        {
            g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), IOCTL_RW_MISALIGN_OR_PARTIAL_TST, ioctlName);
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
//Tests:TestReadBlockPartial
//Test using CMD17 Read to get partial block from SDMemory Card
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

TESTPROCAPI TestReadBlockPartial(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a series of CMD17 bus requests reading data from an SDMemory card in progressively larger increments (from 1 byte to 512 byte). Partial block"));
    g_pKato->Comment(0, TEXT("reads are a required feature of all SDMemory cards. Note: All the bus requests in this test are synchronous."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first checks what state the card is in with an SDCardInfoQuery call. If the Card is in the transfer State, the test continues. Next, the test allocates a response buffer. "));
    g_pKato->Comment(0, TEXT("then the block length is set to 512 bytes with a CMD16 Synchronous bus request, and the first two blocks (at address 0) are \"erased\". This erasure is either done with actual"));
    g_pKato->Comment(0, TEXT("erase bus requests (CMD32, CMD33, and CMD38), or a CMD25 Write to set the blocks high or low, based on the value of the erase state returned from an earlier ACMD51 bus request"));
    g_pKato->Comment(0, TEXT("getting the SCR register. After the \"erase\" an SDCardInfoQuery call is made to get the contents of the CSD register. If the ReadBlockPartial field in the parsed CSD register is true"));
    g_pKato->Comment(0, TEXT("the test proceeds. If it is false the test fails at this point since it is a required feature."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Assuming the test continues, at this point three 512 byte uchar buffers are allocated. The first of these is filled with valid alphanumeric data, but the later two are zeroed. The first buffer"));
    g_pKato->Comment(0, TEXT("is then used to write 1 block to the card at address 0 with a CMD25 multi-block write. Then  in a for loop with c incrementing from 1 to 512 (inclusive) the following is done. First the block"));
    g_pKato->Comment(0, TEXT("length is resized to \"c\" with a CMD16 bus request. Then a CMD17 read is made to the card, to at least partially fill one of the zeroed buffers created earlier. Then a simulation is made, by"));
    g_pKato->Comment(0, TEXT("Copying the first \"c\" bytes to the second zeroed buffer. Then the two buffers are compared. If they are identical, that iteration succeeds. The buffers are then re-zeroed, and c is"));
    g_pKato->Comment(0, TEXT("incremented by 1."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After exiting the for loop, the test cleans up by deallocating the uchar buffers, checking the state of the card again with another SDCardInfoQuery, and then freeing the response buffer."));
    g_pKato->Comment(0, TEXT(" "));

    return TestOddRWs(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestWriteBlockPartial
//Test using CMD24 Write to write partial block to SDMemory Card
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

TESTPROCAPI TestWriteBlockPartial(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->TestFlags = SD_FLAG_WRITE;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a series of CMD24 bus requests reading data from an SDMemory card in progressively larger increments (from 1 byte to 512 byte). Partial block"));
    g_pKato->Comment(0, TEXT("writes are not a required feature of all SDMemory cards. Note: All the bus requests in this test are synchronous."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first checks what state the card is in with an SDCardInfoQuery call. If the Card is in the transfer State, the test continues. Next, the test allocates a response buffer. "));
    g_pKato->Comment(0, TEXT("then the block length is set to 512 bytes with a CMD16 Synchronous bus request, and the first two blocks (at address 0) are \"erased\". This erasure is either done with actual"));
    g_pKato->Comment(0, TEXT("erase bus requests (CMD32, CMD33, and CMD38), or a CMD25 Write to set the blocks high or low, based on the value of the erase state returned from an earlier ACMD51 bus request"));
    g_pKato->Comment(0, TEXT("getting the SCR register. After the \"erase\" an SDCardInfoQuery call is made to get the contents of the CSD register. If the WriteBlockPartial field in the parsed CSD register is true"));
    g_pKato->Comment(0, TEXT("the test proceeds. If it is false the test skips at this point since it is not a required feature."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Assuming the test continues, at this point three 512 byte uchar buffers are allocated. The first of these is filled with valid alphanumeric data, but the later two are zeroed. Then  in a for loop"));
    g_pKato->Comment(0, TEXT("with c incrementing from 1 to 512 (inclusive) the following is done. First the block length is resized to \"c\" with a CMD16 bus request. Then a CMD24 write is made to the card, to write \"c\""));
    g_pKato->Comment(0, TEXT("bytes to the card. Then the block length is reset to 512 bytes and one block is read back from the card using a CMD18 multi-block write into one of the zeroed buffers created earlier. Then a"));
    g_pKato->Comment(0, TEXT("simulation is made, by first setting the bits high(in the second zeroed buffer) if the earlier call to the SCR register said erased bits are high, or the buffer is left zeroed if the SCR indicated"));
    g_pKato->Comment(0, TEXT(" that the erased bits are set low. Then the first \"c\" bytes in the alphanumeric buffer are copied to the simulation buffer. Then the two buffers are compared. If they are identical that iteration"));
    g_pKato->Comment(0, TEXT("succeeds. The buffers are then re-zeroed, the first 2 blocks on the SDMemory card are \"erased\" as described earlier, and c is incremented by 1."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After exiting the for loop, the test cleans up by deallocating the uchar buffers, checking the state of the card again with another SDCardInfoQuery, and then freeing the response buffer."));
    g_pKato->Comment(0, TEXT(" "));

    return TestOddRWs(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestReadBlockMisalign
//Test using CMD17 Read to get block from SDMemory Card in a misaligned fashion
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

TESTPROCAPI TestReadBlockMisalign(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }
    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->TestFlags = SD_FLAG_MISALIGN;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a series of CMD17 bus requests reading 1 block (512 bytes) of data from an SDMemory card with progressively larger offsets from address 0 (from 1 byte"));
    g_pKato->Comment(0, TEXT(" to 512 byte). Misaligned block reads are an optional feature of SDMemory cards. Note: All the bus requests in this test are synchronous."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first checks what state the card is in with an SDCardInfoQuery call. If the Card is in the transfer State, the test continues. Next, the test allocates a response buffer. "));
    g_pKato->Comment(0, TEXT("then the block length is set to 512 bytes with a CMD16 Synchronous bus request, and the first two blocks (at address 0) are \"erased\". This erasure is either done with actual"));
    g_pKato->Comment(0, TEXT("erase bus requests (CMD32, CMD33, and CMD38), or a CMD25 Write to set the blocks high or low, based on the value of the erase state returned from an earlier ACMD51 bus request"));
    g_pKato->Comment(0, TEXT("getting the SCR register. After the \"erase\" an SDCardInfoQuery call is made to get the contents of the CSD register. If the ReadBlockMisalign field in the parsed CSD register is true"));
    g_pKato->Comment(0, TEXT("the test proceeds. If it is false the test skips at this point since it is not a required feature."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Assuming the test continues, at this point three  uchar buffers are allocated. The first of these is filled with valid alphanumeric data, but the later two are zeroed. The alpha numeric"));
    g_pKato->Comment(0, TEXT(" buffer is 1024 bytes, and the zeroed buffers are ach 512 bytes. The first buffer is then used to write 2 blocks to the card at address 0 with a CMD25 multi-block write. Then  in a for"));
    g_pKato->Comment(0, TEXT("loop with c incrementing from 0 to 512 (inclusive) the following is done. First, a CMD17 read is made to the card to read one block of data from address \"c\" into one of the zeroed buffers"));
    g_pKato->Comment(0, TEXT("created earlier. Then a simulation is made, by copying 512 bytes (starting at byte \"c\" in the alpha numeric buffer) from the alphanumeric buffer into the second zeroed buffer. Then the"));
    g_pKato->Comment(0, TEXT(" two buffers are compared. If they are identical, that iteration succeeds. The buffers are then re-zeroed, and c is incremented by 1."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After exiting the for loop, the test cleans up by deallocating the uchar buffers, checking the state of the card again with another SDCardInfoQuery, and then freeing the response buffer."));
    g_pKato->Comment(0, TEXT(" "));

    return TestOddRWs(pTest);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests:TestWriteBlockMisalign
//Test using CMD24 Write to write block to SDMemory Card in a misaligned fashion
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

TESTPROCAPI TestWriteBlockMisalign(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));
        return TPR_FAIL;
    }

    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    pTest->iResult = TPR_PASS;
    pTest->TestFlags = SD_FLAG_MISALIGN | SD_FLAG_WRITE;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test makes a series of CMD24 bus requests writing 1 block (512 bytes) of data from an SDMemory card with progressively larger offsets from address 0 (from 1 byte"));
    g_pKato->Comment(0, TEXT(" to 512 byte). Misaligned block writes are an optional feature of SDMemory cards. Note: All the bus requests in this test are synchronous."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test first checks what state the card is in with an SDCardInfoQuery call. If the Card is in the transfer State, the test continues. Next, the test allocates a response buffer. "));
    g_pKato->Comment(0, TEXT("then the block length is set to 512 bytes with a CMD16 Synchronous bus request, and the first two blocks (at address 0) are \"erased\". This ensure is either done with actual"));
    g_pKato->Comment(0, TEXT("erase bus requests (CMD32, CMD33, and CMD38), or a CMD25 Write to set the blocks high or low, based on the value of the erase state returned from an earlier ACMD51 bus request"));
    g_pKato->Comment(0, TEXT("getting the SCR register. After the \"erase\" an SDCardInfoQuery call is made to get the contents of the CSD register. If the WriteBlockMisalign field in the parsed CSD register is true"));
    g_pKato->Comment(0, TEXT("the test proceeds. If it is false the test skips at this point since it is not a required feature."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Assuming the test continues, at this point three  uchar buffers are allocated. The first of these is filled with valid alphanumeric data, but the later two are zeroed. The alpha numeric"));
    g_pKato->Comment(0, TEXT("buffer is 512 bytes, and the zeroed buffers are each 1024 bytes. Then in a for loop with c incrementing from 0 to 512 (inclusive) the following is done. First a CMD24 write is made to the"));
    g_pKato->Comment(0, TEXT("card to write one block of data to address \"c\" on the SDMemory card. Then the first two blocks of data are read from the card wit ha CMD18 multi-block read into one of the zeroed buffers."));
    g_pKato->Comment(0, TEXT("Next, a simulation is made, by first setting all the bits high in the second zeroed buffer (the simulation buffer) if the SCR register (queried earlier in the test) indicated that erased bits are high,"));
    g_pKato->Comment(0, TEXT("or leaving the bits zeroed if it indicates that is the correct state. After that the contents of the alpha numeric buffer are copied into the simulation buffer  at an offset of \"c\"bytes in the"));
    g_pKato->Comment(0, TEXT("simulation buffer. Then the two buffers (simulation and read) are compared. If they are identical, that iteration succeeds. The buffers are then re-zeroed, the first tow blocks on the SDMemory"));
    g_pKato->Comment(0, TEXT("cards are erased again (as described earlier), and c is incremented by 1."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("After exiting the for loop, the test cleans up by deallocating the uchar buffers, checking the state of the card again with another SDCardInfoQuery, and then freeing the response buffer."));
    g_pKato->Comment(0, TEXT(" "));

    return TestOddRWs(pTest);
}
