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
//
// --------------------------------------------------------------------
//
//
// SD Memory Misc Tests Group is responsible for performing the following tests:
//
// Function Table Entries are:
//  TestGetBitSlice           Tests querying the host controller for its interface mode and clock rate
//  TestUnloadBusDriverTest   Tests querying an SD card for its interface mode and clock rate
//  TestSDIO1_1_BusReq        Verify that the SD Bus/Controller driver is working properly by issuing
//                            few (IOCTLs) to it.

#include "main.h"
#include "globals.h"
#include <sdcard.h>
#include "resource.h"
#include "dlg.h"
#include <auto_xxx.hxx>
#include <ceddk.h>
#include <bldver.h>
#include <sddetect.h>

extern DWORD g_deviceindex[10];

//////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                        ///////
///////                                Miscellaneous  Tests                                    ///////
///////                                                                                        ///////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////
//Helper: TestSimpleRequests
//Helper Function used by all the tests that test functions in SDMisc.cpp + one test in SDDebug.cpp
//
// Parameters:
//  PTEST_PARAMS pTest    Test parameters passed to driver. Contains return codes and output buffer, etc.
//  DWORD ioctl            As all the tests use different IOCTLs I need to pass in the IOCTL
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI TestMiscTests(PTEST_PARAMS pTest, DWORD ioctl)
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
//Tests: TestGetBitSlice
// This test verifies that GetBitSlice correctly returns the specified bits from a UCHAR string as a DWORD.
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
TESTPROCAPI TestGetBitSlice(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
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

    g_pKato->Comment(0, TEXT(" This test verifies that GetBitSlice correctly returns the specified bits from a UCHAR string as a DWORD. This test is "));
    g_pKato->Comment(0, TEXT("performed by simulating GetBitSlice in test code and comparing the results to what get bit slice returns."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("The test starts by calling srand using the seed passed in from tux. It then performs get bit slice, simulations and comparison of results on a"));
    g_pKato->Comment(0, TEXT("number of buffers containing random data. It checks get bit slice when bytes are aligned, or not aligned, when whole bytes are sought and"));
    g_pKato->Comment(0, TEXT("when partial bytes are sought. It gets individual bits to full DWORDs. It also varies the buffer size tested(large or small, and uses GetBitSlice"));
    g_pKato->Comment(0, TEXT(" to get whole (or almost whole) buffers, or just a fraction of a buffer. It also tries to get 0 bits from a buffer. Each buffer tested is unique in"));
    g_pKato->Comment(0, TEXT("size and contents from the one used previously."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Get bit slice is king of a weird function. The simulation created in test code does the following. First it allocates a temporary buffer to contain"));
    g_pKato->Comment(0, TEXT("only the bytes that contain bits that are being sought. It copies those bytes into the buffer. Then it reverses the bits in each byte. It does not"));
    g_pKato->Comment(0, TEXT("reverse the bytes, just the bits in each byte. Then it left shifts the bits the real offset value (offset mod 8). Then any bits that are not being"));
    g_pKato->Comment(0, TEXT("sought from GetBitSlice are masked in the last byte. This is calculated from the numBits value passed into the function. After that all the bits in"));
    g_pKato->Comment(0, TEXT("bytes are reversed again. and the DWORD is built with byte 0 being left-shifted 0, byte 1 (if it exists) being left-shifted 8, byte 2  (if it exists)"));
    g_pKato->Comment(0, TEXT("being left shifted 16, and byte 3 (if it exists) being left shifted 24. The resulting DWORD is then ready to be compared to the output of the real"));
    g_pKato->Comment(0, TEXT("GetBitSlice function."));
    g_pKato->Comment(0, TEXT(" "));
    g_pKato->Comment(0, TEXT("Only if the simulated and real values of GetBitSlice are the same in every case does the test pass."));
    g_pKato->Comment(0, TEXT(" "));

    return TestMiscTests(pTest, IOCTL_GET_BIT_SLICE_TST);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: TestSDIO1_1_BusReq
// This test verify that the SD Bus/Controller driver is working properly by issuing few (IOCTLs) to it.
// Such as Reset, Deselect and get driver version with valid and invalid buffer size, as well as
// sending an invalid IOCTL.
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
TESTPROCAPI TestSDIO1_1_BusReq(UINT uMsg, TPPARAM, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;

    BOOL   rVal = false;
    DWORD  dwSlot = 0;
    LPBYTE pBuffer = NULL;  //buffer with data to copy

    DEVMGR_DEVICE_INFORMATION BUSdi; //information about Host Controller device
    memset(&BUSdi, 0, sizeof(BUSdi));
    BUSdi.dwSize = sizeof(BUSdi);


    LPCTSTR searchString=_T("SDC*");
    ce::auto_hfile hfDevice = NULL;
    ce::auto_hfile hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, searchString, &BUSdi);
    if (hSearchDevice == INVALID_HANDLE_VALUE)
    {
        NKDbgPrintfW(_T("FindFirstDevice('%s') failed %d\r\n"), searchString, GetLastError());
        rVal = false;
        goto Done;
    }

    LPCTSTR pszBusName = BUSdi.szBusName;
    LPCTSTR pszDevName = BUSdi.szDeviceName;
    hfDevice = CreateFile(pszDevName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (!hfDevice.valid())
    {
        NKDbgPrintfW(_T("CreateFile('%s') failed %d\r\n"), pszDevName, GetLastError());
        rVal = false;
        goto Done;
    }

    pBuffer = (LPBYTE) LocalAlloc(LMEM_FIXED, 100);
    rVal = DeviceIoControl(hfDevice, IOCTL_SD_BUS_DRIVER_SLOT_CARD_RESET,
                  (PVOID) &dwSlot, sizeof(dwSlot), NULL, NULL, NULL, NULL);
    if(!rVal)
    {
        NKDbgPrintfW(_T("DeviceIoControl('%s': IOCTL_SD_BUS_DRIVER_SLOT_CARD_RESET) failed %d\r\n"), pszDevName, GetLastError());
        goto Done;
    }
    Sleep(2000);

    rVal = DeviceIoControl(hfDevice, IOCTL_SD_BUS_DRIVER_SLOT_CARD_DESELECT,
                  (PVOID) &dwSlot, sizeof(dwSlot), NULL, NULL, NULL, NULL);
    if(!rVal)
    {
        NKDbgPrintfW(_T("DeviceIoControl('%s': IOCTL_SD_BUS_DRIVER_SLOT_CARD_DESELECT) failed %d\r\n"), pszDevName, GetLastError());
        goto Done;
    }
    Sleep(3000);

    //Send an invalid IOCTL
    rVal = DeviceIoControl(hfDevice, (DWORD)-1, (PVOID) pszBusName, (_tcslen(pszDevName) + 1) * sizeof(*pszDevName), NULL, NULL, NULL, NULL);
    if(rVal)
    {
        NKDbgPrintfW(_T("DeviceIoControl('%s': BAD_IOCTL) succeeded\r\n"), pszDevName);
        rVal = FALSE;
        goto Done;
    }
    else
    {
        rVal = TRUE;
    }

    DWORD  version = 0;
    DWORD  length = 0;             // IOCTL return length
    //now get version
    if (!DeviceIoControl(hfDevice, IOCTL_SD_BUS_DRIVER_GET_VERSION, NULL, 0, &version, sizeof(DWORD), &length, NULL))
    {
          NKDbgPrintfW(TEXT("IOCTL Failed (%u) \n"), GetLastError());
          rVal = FALSE;
          goto Done;
    }

    DWORD dwBaselineVersion = MAKELONG(CE_MINOR_VER, CE_MAJOR_VER);
    if(dwBaselineVersion != version)
    {
         NKDbgPrintfW(TEXT("Version =  (%u). Should be (%u) \n"), version,dwBaselineVersion);
         rVal = FALSE;
         goto Done;
    }
    else
    {
        rVal = true;
    }

    //send a bad size (sizeof(DWORD) + 1) to IOCTL_SD_BUS_DRIVER_GET_VERSION
    PREFAST_SUPPRESS(26000, "The test case tests if DeviceIoControl can handle incorrect buffer size");
    if (DeviceIoControl(hfDevice, IOCTL_SD_BUS_DRIVER_GET_VERSION, NULL, 0, &version, sizeof(DWORD) + 1, &length, NULL))
    {
          NKDbgPrintfW(TEXT("FAILURE: IOCTL_SD_BUS_DRIVER_GET_VERSION succeeded with wrong argument length \n"), GetLastError());
          rVal = FALSE;
          goto Done;
    }
    else
    {
        if(GetLastError() != ERROR_INVALID_PARAMETER)
        {
            NKDbgPrintfW(TEXT("FAILURE:  IOCTL_SD_BUS_DRIVER_GET_VERSION with wrong argument length did returned %d, not %d (ERROR_INVALID_PARAMETER)\n"), GetLastError(),ERROR_INVALID_PARAMETER);
            rVal = FALSE;
            goto Done;
        }
        else
        {
            rVal = true;
        }
    }

Done:
    if(pBuffer)
    {
        LocalFree(pBuffer);
    }

    if(rVal)
    {
        return TPR_PASS;
    }
    else
    {
        return TPR_FAIL;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//Tests: TestUnloadBusDriverTest
// Tests unloading of the SD Bus Driver (sdbus.dll). To unload the sd bus driver, the test will
// unload ALL SD host controllers present in the system. Then, all drivers are reloaded.
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
TESTPROCAPI TestUnloadBusDriverTest (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY)
{
    NO_MESSAGES;
    DWORD dwResult = TPR_FAIL;
    PDEVMGR_DEVICE_INFORMATION pDiSDHC = NULL;

    if(g_ManualInsertEject)
    {
        //we can't run the test, because the user specified that we should never automatically load/unload
        g_pKato->Comment(0,TEXT("Skipping TestUnloadBusDriverTest because g_ManualInsertEject is TRUE"));

        return TPR_SKIP;
    }

    PTEST_PARAMS pTest = (PTEST_PARAMS) malloc(sizeof(TEST_PARAMS));
    if (!pTest)
    {
        g_pKato->Comment(0, TEXT("Not enough memory to run test!!!"));

        return TPR_FAIL;
    }
    ZeroMemory(pTest, sizeof(TEST_PARAMS));
    LPTPS_EXECUTE lpExecute = (LPTPS_EXECUTE)tpParam;
    pTest->dwSeed = lpExecute->dwRandomSeed;
    pTest->iResult = TPR_PASS;

    g_pKato->Comment(0, TEXT("This test tests the unloading of sdbus.dll."));
    g_pKato->Comment(0,TEXT(" **** PLEASE READ *****"));
    g_pKato->Comment(0,TEXT("*** To unload the sd bus driver, the test will unload ALL SD host controller ***"));
    g_pKato->Comment(0,TEXT("*** presents in the system, then reload them back. *** "));
    g_pKato->Comment(0,TEXT("*** If one of the host controller in the BSP is not meant to be reloadable ***"));
    g_pKato->Comment(0,TEXT("*** Strongly recommended to skip this test to avoid misbehaviour ***"));
    g_pKato->Comment(0,TEXT(" *********************"));
    g_pKato->Comment(0, TEXT(" "));

    DWORD dwNumOfHost = NumOfSDHostController();
    DEBUGMSG(TRUE,(_T("No. of Host controllers dwNumOfHost : %d"),dwNumOfHost));
    if (dwNumOfHost > 0)
    {
        pDiSDHC = (PDEVMGR_DEVICE_INFORMATION) LocalAlloc(LPTR, dwNumOfHost * sizeof(DEVMGR_DEVICE_INFORMATION));

        for (DWORD i = 0; i < dwNumOfHost; i++)
        {
            pDiSDHC[i].dwSize = sizeof(DEVMGR_DEVICE_INFORMATION);
            //first unload the HC
            if(!UnloadSDHostController(pDiSDHC[i], g_deviceindex[i]))
            {
                g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to unload the SDHC Driver.  Exiting..."));
                goto Done;
            }
        }
    }

    DEVMGR_DEVICE_INFORMATION BUSdi; //information about the bus device
    memset(&BUSdi, 0, sizeof(BUSdi));
    BUSdi.dwSize = sizeof(BUSdi);
    if(!UnloadSDBus(BUSdi))
    {
        g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to unload the SDBus Driver.  Exiting..."));
        goto Done;
    }

    //reload sdbus
    if (!LoadingDriver(BUSdi))
    {
        g_pKato->Comment(0,TEXT("ShellProc(SPM_LOAD_DLL):Unable to reload the SDBus Driver.  Exiting..."));
        goto Done;
    }

    //reload sdhc
    for (DWORD i = 0; i < dwNumOfHost; i++)
    {
        //reload the HC
        if(!LoadingDriver(pDiSDHC[i]))
        {
            g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):Unable to reload the HC Driver.  Exiting..."));
            goto Done;
        }
    }

    DWORD dwWait = 0;
    if (g_DriverLoadedEvent)
    {
        dwWait = WaitForSingleObject(g_DriverLoadedEvent, 100000);
    }
    else
    {
        g_pKato->Comment(0, TEXT("ShellProc(SPM_LOAD_DLL):CreateEvent() failed with error code (%d)."), GetLastError());
        goto Done;
    }

    if (dwWait == WAIT_TIMEOUT)
    {
        g_pKato->Comment(1, TEXT("*** TestUnloadBusDriverTest:Never signaled that Test Driver has loaded. Timed Out!!! ***"));
        goto Done;
    }

    g_hTstDriver = CreateFile(g_szDiskName, GENERIC_READ | GENERIC_WRITE,  0,  NULL, OPEN_EXISTING, 0, NULL);
    if (g_hTstDriver == INVALID_HANDLE_VALUE)
    {
        DWORD gle;
        gle = GetLastError();
        g_pKato->Comment(1, TEXT("Failure to get handle to test driver"));
        goto Done;
    }

    dwResult = TPR_PASS;

Done:
    if (pDiSDHC)
    {
        LocalFree(pDiSDHC);
    }

    return dwResult;
}
