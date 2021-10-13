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
// The Memory List Test Group is responsible for testing the memory list APIs.
// See the ft.h file for more detail.
//
// Function Table Entries are:
//  TestMemoryList        Tests Basic Memory List APIs
//

#include "main.h"
#include "globals.h"
#include <sdcard.h>


//////////////////////////////////////////////////////////////////////////////////////////////////
//Helper: TestMemListTests
//Helper Function used by all the tests that test functions in SDMem.cpp
//
// Parameters:
//  PTEST_PARAMS pTest  Test parameters passed to driver. Contains return codes and output buffer, etc.
//  DWORD ioctl         As all the tests use different IOCTLs I need to pass in the IOCTL
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI TestMemListTests(PTEST_PARAMS pTest, DWORD ioctl)
{
    int     iRet = TPR_FAIL;
    BOOL    bRet = TRUE;
    DWORD   length = 0;
    DWORD   verbosity = 0;;
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

    bRet = DeviceIoControl(g_hTstDriver, ioctl, NULL, 0, pTest, sizeof(TEST_PARAMS), &length, NULL);
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
        ioctlName = TranslateIOCTLS(ioctl);
        if (ioctlName != NULL) 
        {
            g_pKato->Comment(0, TEXT("Can't find the IOCTL: 0x%x (%s) in the driver"), ioctl, ioctlName);
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
//Tests: TestMemoryList
// Tests the memory list APIs: SDCreateMemoryList, 
//                             SDAllocateFromMemList,
//                             SDFreeToMemList, and 
//                             SDDeleteMemList.
// Parameters:
//  uMsg        Message code.
//  tpParam     Additional message-dependent data.
//  lpFTE       Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//

TESTPROCAPI TestMemoryList(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY )
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

    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;

    g_pKato->Comment(0, TEXT("The following test tests the memory list APIs: SDCreateMemoryList,  SDAllocateFromMemList, SDFreeToMemList and SDDeleteMemList. The test passes"));
    g_pKato->Comment(0, TEXT("if all of the calls correctly allocate the amount of memory expected. This is verified by reasonable return values, svsutil_GetFixedStats returning expected"));
    g_pKato->Comment(0, TEXT("values, and by trying to use the memory that these functions manipulate."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test starts out by creating a memory list with a depth of 6, and an EntrySize of 1024 bytes. If the handle returned by SDCreateMemory is non-NULL,"));
    g_pKato->Comment(0, TEXT("svsutil_GetFixedStats is called to get the number of free and total blocks (A), otherwise the test concludes. If the test continues, the first allocation is"));
    g_pKato->Comment(0, TEXT("made from the memory list. The function that does this does the following: It allocates from the heap an alphanumeric UCHAR buffer to later write into the"));
    g_pKato->Comment(0, TEXT("memory that will be allocated from the memory list; It allocates memory form the memory list (w/SDAllocateFromMemList) ; It fills the memory form the memory"));
    g_pKato->Comment(0, TEXT(" list with the data from the alphanumeric UCHAR buffer with SDPerformSafeCopy, and then calls svsutil_GetFixedStats again (B). All but the last of these have"));
    g_pKato->Comment(0, TEXT(" the possibility of failing, and if that happens the test bails. Assuming the test continues. The memory just allocated from the memory list is freed with"));
    g_pKato->Comment(0, TEXT("SDFreeToMemList, and svsutil_GetFixedStats is called again (C). After that the function that memory is allocated from the memory list via the same function"));
    g_pKato->Comment(0, TEXT("that was used earlier 6 times to use the whole list. This means that for each allocation, the memory gets filled, and the memory stats are taken (D-I). After"));
    g_pKato->Comment(0, TEXT("that, the memory is freed back to the list in the order it was allocated. In addition, svsutil_GetFixedStats is called each time memory is freed (J-0)."));
    g_pKato->Comment(0, TEXT("  "));
    g_pKato->Comment(0, TEXT("Assuming all the allocations and releases to the memory list succeed, at this time the memory stats are displayed for points A-0. At point A both total and free "));
    g_pKato->Comment(0, TEXT("memory should be 6. At point B, after the first allocation from the list The Free memory should be 5 blocks and the Total memory should be 6 blocks. At point C,"));
    g_pKato->Comment(0, TEXT("after that memory is released to the list, both total and free memory should be 6. At points D-I, total memory should remain at 6 blocks, but free memory should be"));
    g_pKato->Comment(0, TEXT("decreasing to zero. For example, at point D free memory should be 5 blocks, at point E Free memory should be 4 blocks, etc. At points J-O, Total memory should"));
    g_pKato->Comment(0, TEXT("again remain at 6 blocks, but the amount of free memory should be increasing again towards 6. This means that at point J, free memory should be one block, at"));
    g_pKato->Comment(0, TEXT("point K free memory should be 2 blocks, etc. If the memory levels are all as stated, the test passes. At the very end SDDeleteMemList is called, and the list is freed."));
    g_pKato->Comment(0, TEXT(" "));

    return TestMemListTests(pTest, IOCTL_MEMLIST_TST);
}

