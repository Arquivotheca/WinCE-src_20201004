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
#ifndef UNDER_CE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>

#include <afx.h>
#include <afxtempl.h>

#include <winioctl.h>
#include <winsmcrd.h>

#include <specstrings.h>
#include <strsafe.h>

#include "ifdtest.h"
#include "ifdguid.h"
#else
#include <tux.h>
#include <tuxmain.h>
#include "ifdtest.h"
#include "pwrtstutil.h"
#include "InsertRemoveTests.h"
#include "MultiReaderTests.h"
#endif


class CCardProvider *CCardProvider::s_pFirst;

const UCHAR MAX_PARAMS                    = 16;

// Helper class used for managing power state.
CSysPwrStates* g_pCSysPwr = NULL;

// Global instance value used to read string resources.
HINSTANCE g_hInst = 0;

PWCHAR
CAtr::GetAtrString(
    __out_ecount(256) PWCHAR in_pchBuffer
    )
{
    memset(in_pchBuffer, 0, 256*sizeof(WCHAR)); 
    for (ULONG i = 0; i < m_uAtrLength && i < SCARD_ATR_LENGTH; i++)
    {
        // SCARD_ATR_LENGTH is the length of m_rgbAtr
        HRESULT hr = StringCchPrintf(in_pchBuffer + (i * 3), 256 - (i * 3), L"%02X ", m_rgbAtr[i]);
    }

    return in_pchBuffer;
}

CCardProvider::CCardProvider(
    void
    )
{
    m_pNext = NULL;
    m_uTestNo = 0;
    m_bCardTested = FALSE;
    m_bTestFailed = FALSE;
    m_pSetProtocol = NULL;
    m_pCardTest = NULL;
}

CCardProvider::CCardProvider(
    void (*in_pEntryFunction)(class CCardProvider&)
    )
/*++

Routine Description:
    Constructor for class CCardProvider.
    This constructor is called for every card that is to be tested.
    It creates a new instance and appends it to a singly linked list

Arguments:

    Pointer to function that registers all test functions

--*/
{
    class CCardProvider *l_pCardProvider;

    *this = CCardProvider();

    if (s_pFirst == NULL)
    {
        s_pFirst = new CCardProvider;
        l_pCardProvider = s_pFirst;

    }
    else
    {
        l_pCardProvider = s_pFirst;

        while (l_pCardProvider->m_pNext)
        {
            l_pCardProvider = l_pCardProvider->m_pNext;
        }

        l_pCardProvider->m_pNext = new CCardProvider;
        l_pCardProvider = l_pCardProvider->m_pNext;
    }

    if(l_pCardProvider)
    {
        (*in_pEntryFunction)(*l_pCardProvider);
    }
}

BOOLEAN
CCardProvider::CardsUntested(
    void
    )
{
    class CCardProvider *l_pCCardProvider = s_pFirst;

    while (l_pCCardProvider)
    {
        if (l_pCCardProvider->m_bCardTested == FALSE)
        {
            return TRUE;
        }
        l_pCCardProvider = l_pCCardProvider->m_pNext;
    }
    return FALSE;
}

void
CCardProvider::SetAllCardsUntested(
    void
    )
{
    class CCardProvider *l_pCCardProvider = s_pFirst;

    while (l_pCCardProvider)
    {
        // Reset pertinent variables.
        l_pCCardProvider->m_uTestNo = 0;
        l_pCCardProvider->m_bCardTested = FALSE;
        l_pCCardProvider->m_bTestFailed = FALSE;

        l_pCCardProvider = l_pCCardProvider->m_pNext;
    }
}

void
CCardProvider::CardTest(
    class CReader& io_pCReader,
    ULONG in_uTestNo
    )
/*++

Routine Description:

    Calls every registered card provider in the list until one of 
    the providers indicates that it recognized the card

Arguments:

    io_pCReader - reference to the test structure

Return Value:

    IFDSTATUS value

--*/
{
    class CCardProvider *l_pCCardProvider = s_pFirst;
    ULONG l_uStatus;

    while (l_pCCardProvider)
    {
        if ( l_pCCardProvider->IsValidAtr(io_pCReader.GetAtr()) )
        {
            if (l_pCCardProvider->m_bCardTested)
            {
                // We tested this card already
                LogMessage(LOG_CONSOLE, IDS_CARD_TESTED_ALREADY);
                return;
            }

            l_pCCardProvider->m_bCardTested = TRUE;
            LogMessage(
                LOG_CONSOLE,
                IDS_TESTING_CARD,
                (LPCTSTR) l_pCCardProvider->m_CCardName
                );

            if (l_pCCardProvider->m_pSetProtocol == NULL)
            {
                return;
            }

            // Call card provider function
            l_uStatus = (*l_pCCardProvider->m_pSetProtocol)(
                *l_pCCardProvider,
                io_pCReader
                );

            if (l_uStatus == IFDSTATUS_END)
            {
                return;
            }

            if (l_uStatus != IFDSTATUS_SUCCESS)
            {
                return;
            }

            // Check if the card test function pointer exists
            if (l_pCCardProvider->m_pCardTest == NULL)
            {
                return;
            }

            if (in_uTestNo)
            {
                // user wants to run only a single test
                l_pCCardProvider->m_uTestNo = in_uTestNo;

                LogMessage(
                    LOG_CONSOLE,
                    IDS_TEST_NO,
                    l_pCCardProvider->m_uTestNo
                    );

                // Call card provider function
                l_uStatus = (*l_pCCardProvider->m_pCardTest)(
                    *l_pCCardProvider,
                    io_pCReader
                    );
                return;
            }

            // run the whole test set
            for (l_pCCardProvider->m_uTestNo = 1; ;l_pCCardProvider->m_uTestNo++)
            {
                LogMessage(
                    LOG_CONSOLE,
                    IDS_TEST_NO,
                    l_pCCardProvider->m_uTestNo
                    );

                // Call card provider function
                l_uStatus = (*l_pCCardProvider->m_pCardTest)(
                    *l_pCCardProvider,
                    io_pCReader
                    );

                if (l_uStatus != IFDSTATUS_SUCCESS)
                {
                    return;
                }
            }
        }
        l_pCCardProvider = l_pCCardProvider->m_pNext;
    }

    PWCHAR l_rgbAtrBuffer = new WCHAR[256];

    LogMessage(LOG_CONSOLE, IDS_CARD_UNKNOWN);
    LogMessage(LOG_CONSOLE, IDS_CURRENT_CARD);
    if (l_rgbAtrBuffer)
    {
        LogMessage(
            LOG_CONSOLE,
            L"       %s",
            io_pCReader.GetAtrString(l_rgbAtrBuffer)
            );
    }

    for (l_pCCardProvider = s_pFirst;
         l_pCCardProvider;
         l_pCCardProvider = l_pCCardProvider->m_pNext)
    {
        if (l_pCCardProvider->m_bCardTested == FALSE)
        {
            LogMessage(
                LOG_CONSOLE,
                L"    *  %s",
                (LPCTSTR) l_pCCardProvider->m_CCardName
                );

            if (l_rgbAtrBuffer)
            {
                for (int i = 0; i < MAX_NUM_ATR && l_pCCardProvider->m_CAtr[i].GetLength(); i++)
                {
                    LogMessage(
                        LOG_CONSOLE,
                        L"       %s",
                        l_pCCardProvider->m_CAtr[i].GetAtrString(l_rgbAtrBuffer)
                        );
                }
            }
        }
    }

   delete [] l_rgbAtrBuffer;

   LogMessage(LOG_CONSOLE, IDS_REMOVE_CARD);
}

void
CCardProvider::ListUntestedCards(
    void
    )
/*++

Routine Description:
    Prints a list of all cards that have not been tested

--*/
{
    class CCardProvider *l_pCCardProvider = s_pFirst;

    while (l_pCCardProvider)
    {
        if (l_pCCardProvider->m_bCardTested == FALSE)
        {
            LogMessage(
                LOG_CONSOLE,
                L"    *  %s",
                (LPCTSTR) l_pCCardProvider->m_CCardName
                );
        }
        l_pCCardProvider = l_pCCardProvider->m_pNext;
    }
}

void
CCardProvider::SetAtr(
    const BYTE in_rgbAtr[],
    ULONG in_uAtrLength
    )
/*++

Routine Description:
    Sets the ATR of the card

Arguments:
    in_rgchAtr - the atr string
    in_uAtrLength - length of the atr

--*/
{
    for (int i = 0; i < MAX_NUM_ATR; i++)
    {
        if (m_CAtr[i].GetLength() == 0)
        {
           m_CAtr[i] = CAtr(in_rgbAtr, in_uAtrLength);
           return;
        }
    }
}

void
CCardProvider::SetCardName(
    __in const WCHAR in_rgchCardName[]
    )
/*++

Routine Description:
    Sets a friendly name for the card

Arguments:
    in_rgchCardName - Friendly name for the card

--*/
{
    m_CCardName = in_rgchCardName;
}

void
CheckCardMonitor(
    CReader &in_CReader
    )
{
    const UCHAR MAX_TIME_FOR_REPEAT_INSERTION = 15;
    const UCHAR MAX_REPEAT_INSERTION_ITERS    = 10;

    ULONG l_lResult, l_uReplyLength, l_lTestNo = 1;
    DWORD l_lStartTime;
    BOOL l_bResult;
    HANDLE l_hReader = in_CReader.GetHandle();


    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_PART_A);
    LogMessage(LOG_CONSOLE, L"=============================");

    LogMessage(LOG_CONSOLE, IDS_REM_ALL_CARDS);

    in_CReader.WaitForCardRemoval();
    TestStart(
        IDS_TEST_NO_INS_CARD,
        l_lTestNo++
        );

    l_lResult = in_CReader.WaitForCardInsertion();

    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED).GetBuffer(0),
        l_lResult
        );

    TestEnd();

    TestStart(
        IDS_SMART_CARD_PRESENT,
        l_lTestNo++
        );

    l_bResult = DeviceIoControl(
        l_hReader,
        IOCTL_SMARTCARD_IS_PRESENT,
        NULL,
        0,
        NULL,
        0,
        &l_uReplyLength,
        NULL
        );

    TestCheck(
        l_bResult == TRUE,
        IDS_SEVICE_CONTROL_IO_SHOULD_RET_TRUE
        );

    TestEnd();

    TestStart(
        IDS_TEST_NO_REM_CARD,
        l_lTestNo++
        );

    l_lResult = in_CReader.WaitForCardRemoval();
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_CARD_REM).GetBuffer(0),
        l_lResult
        );

    TestEnd();

    TestStart(
        IDS_SMART_CARD_ABSENT,
        l_lTestNo++
        );

    l_bResult = DeviceIoControl(
        l_hReader,
        IOCTL_SMARTCARD_IS_ABSENT,
        NULL,
        0,
        NULL,
        0,
        &l_uReplyLength,
        NULL
        );

    TestCheck(
        l_bResult == TRUE,
        IDS_DEV_IO_CTRL_SHOULD_RET_TRUE_CARD_REM
        );
    TestEnd();

    TestStart(
        IDS_INS_REM_SMART_CARD,
        l_lTestNo++
        );

    l_lStartTime = GetTickCount(); 
    for (BYTE l_byteIters = 0; l_byteIters<MAX_REPEAT_INSERTION_ITERS; l_byteIters++)
    {
        l_lResult = in_CReader.ColdResetCard();

        l_bResult = DeviceIoControl(
            l_hReader,
            IOCTL_SMARTCARD_IS_PRESENT,
            NULL,
            0,
            NULL,
            0,
            &l_uReplyLength,
            NULL
            );


        l_lResult = GetLastError();

        TestCheck(
            l_bResult == TRUE,
            IDS_READER_FAILED_WITH_RET_VAL,
            l_lResult
            );

        l_bResult = DeviceIoControl(
            l_hReader,
            IOCTL_SMARTCARD_IS_ABSENT,
            NULL,
            0,
            NULL,
            0,
            &l_uReplyLength,
            NULL
            );


        l_lResult = GetLastError();

        TestCheck(
            l_bResult == TRUE,
            IDS_READER_FAILED_CARD_REM_WITH_RET_VAL,
            l_lResult
            );

        LogMessage(LOG_CONSOLE, IDS_DETECTEDCARD_INSREM_ITER, l_byteIters+1);
        if (GetTickCount()-l_lStartTime >= (MAX_TIME_FOR_REPEAT_INSERTION*CLOCKS_PER_SEC))
        {
            break;
        }
    }

    TestEnd();
}

//***************************************************************************
// Purpose:    Thread that cancels a blocking IOCTL_SMARTCARD_* call.
// Arguments:
//   __in  HANDLE hReader     - Handle to a card reader.
// Returns:
//   ERROR_SUCCESS on success.
//   GetLastError() on failure.
//***************************************************************************
DWORD WINAPI
CancelIoThread(HANDLE hReader)
{
    BOOL l_bResult       = 0;
    DWORD l_uReplyLength = 0;

    Sleep(500);  // Make sure main thread proceeds first.
    l_bResult = DeviceIoControl (
        hReader,
        IOCTL_SMARTCARD_CANCEL_BLOCKING,
        NULL,
        0,
        NULL,
        0,
        &l_uReplyLength,
        NULL
    );
    
    if (l_bResult)
    {
        return ERROR_SUCCESS;
    }
    else
    {
        return GetLastError();
    }
}

//***************************************************************************
// Purpose:    Thread that opens and closes a reader handle.
// Arguments:
//   __in  CReader* in_pCReader   - Pointer to a card reader
// Returns:
//   The result of CReader::Open().
//***************************************************************************
DWORD WINAPI
CloseReopenReaderThread(__in CReader* in_pCReader)
{
    Sleep(500);  // Make sure main thread proceeds first.
    in_pCReader->Close();
    return in_pCReader->Open();
}


//***************************************************************************
// See header file for description.
//***************************************************************************
DWORD WINAPI
SuspendDeviceAndScheduleWakeupThread(const VOID * const in_lpvParam)
{
    // Recall CeSetUserNotification assumes a time >= 10s, otherwise we'll wake up
    // from suspend immediately.
    const DWORD TIME_BEFORE_RESUME = 15;
    
    DWORD in_dwSecsBeforeSuspend = (DWORD) in_lpvParam;
    DWORD l_dwRefMillis;
    DWORD l_dwRepeatSecs;
    DWORD l_dwRet;

    if (0==in_dwSecsBeforeSuspend)
    {
        in_dwSecsBeforeSuspend=1;
    }

    for (l_dwRepeatSecs = 0; 
         l_dwRepeatSecs < (DWORD)in_dwSecsBeforeSuspend; 
         l_dwRepeatSecs++)
    {
        l_dwRefMillis = GetTickCount();
        while (GetTickCount()-l_dwRefMillis < CLOCKS_PER_SEC)
        {
            ; // spin
        }
        LogMessage(LOG_DETAIL,
            L"%2ld ", in_dwSecsBeforeSuspend - l_dwRepeatSecs-1);
        wprintf(L"\r%2ld  ",
            in_dwSecsBeforeSuspend - l_dwRepeatSecs-1);
    }

    wprintf(L"\n");

    LogMessage(LOG_CONSOLE,
        TestLoadStringResource(IDS_SYSTEM_GOING_TO_SLEEP_NOW).GetBuffer(0));
    if(g_pCSysPwr->SetupRTCWakeupTimer(TIME_BEFORE_RESUME) == FALSE ){ //will wakeup after l_dwAlarmResolution s
        LogMessage(LOG_CONSOLE,
            TestLoadStringResource(IDS_COULD_NOT_SETUP_RTC).GetBuffer(0));
    }

    if((l_dwRet = SetSystemPowerState(_T("Suspend"), NULL, POWER_FORCE)) != ERROR_SUCCESS){
        TestCheck(
            FALSE,
            IDS_GET_SYSTEMPOWERSTATE_FAILED,
            l_dwRet
            );

        g_pCSysPwr->CleanupRTCTimer();
        return ERROR_GEN_FAILURE;
    }

    TCHAR szPowerStateOn[] = L"On";
    if(PollForSystemPowerChange(szPowerStateOn, 0, 5) == FALSE){
        TestCheck(
            FALSE,
            IDS_POLLING_ON_STATE_FAILED
            );

        g_pCSysPwr->CleanupRTCTimer();
        return ERROR_GEN_FAILURE;
    }
    LogMessage(LOG_CONSOLE, IDS_SYSTEM_HAS_RESUMED);

    g_pCSysPwr->CleanupRTCTimer();
    return ERROR_SUCCESS;
}

void
CheckReader(
    CReader &in_CReader
    )
/*++

Routine Description:

    Checks the attributes of a reader.
    Once with card inserted and then without

Arguments:

Return Value:

--*/
{
    BOOL l_bResult       = FALSE;
    ULONG l_iIndex          = 0;
    ULONG l_uReplyLength    = 0;
    ULONG l_uTestNo         = 1;
    ULONG l_uStart          = 0;
    ULONG l_uEnd            = 0;
    BOOLEAN l_bCardInserted = FALSE;
    UCHAR l_rgbReplyBuffer[512];
    HANDLE l_hReader = in_CReader.GetHandle();
    

#define ATTR(x) L#x, x

    struct ATTRIBUTE
    {
        PWCHAR m_pchName;
        ULONG m_uType;
    }l_aAttr[] =
    {
        ATTR(SCARD_ATTR_VENDOR_NAME),
        ATTR(SCARD_ATTR_VENDOR_IFD_TYPE),
        ATTR(SCARD_ATTR_DEVICE_UNIT),
        ATTR(SCARD_ATTR_ATR_STRING),
        ATTR(SCARD_ATTR_DEFAULT_CLK),
        ATTR(SCARD_ATTR_MAX_CLK),
        ATTR(SCARD_ATTR_DEFAULT_DATA_RATE),
        ATTR(SCARD_ATTR_MAX_DATA_RATE),
        ATTR(SCARD_ATTR_MAX_IFSD),
        ATTR(SCARD_ATTR_CURRENT_PROTOCOL_TYPE),
        ATTR(SCARD_ATTR_PROTOCOL_TYPES),
        0, 0,
        ATTR(SCARD_ATTR_ATR_STRING),
        ATTR(SCARD_ATTR_CURRENT_PROTOCOL_TYPE)
    };

    const DWORD NUM_TEST_ATTRIBUTES = sizeof(l_aAttr)/sizeof(ATTRIBUTE);


    LogMessage(LOG_CONSOLE, L"=======================");
    LogMessage(LOG_CONSOLE, IDS_PART_B);
    LogMessage(LOG_CONSOLE, L"=======================");


    LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
    in_CReader.WaitForCardRemoval();

    TestStart(IDS_DEV_NAME, l_uTestNo++);

    CString l_COperatingSystem = GetOperatingSystem();
    if (l_COperatingSystem == OS_WINNT4)
    {
        TestCheck(
            in_CReader.GetDeviceName().Left(12) == TestLoadStringResource(IDS_SC_READER),
            IDS_DEV_NAME_NOT_NT4_COMPLIANT
            );

    }
    else if (l_COperatingSystem == OS_WIN95 ||
             l_COperatingSystem == OS_WIN98)
    {
        // there is no special naming convention for Win9x

    }
    else if (l_COperatingSystem == OS_WINCE)
    {
        TestCheck(
            in_CReader.GetDeviceName().Left(3) == TestLoadStringResource(IDS_SC_WINCE_READER),
            IDS_DEV_NAME_NOT_WINCE_COMPLIANT
            );
    }
    else
    {
        TestCheck(
            in_CReader.GetDeviceName().Find(L"{50dd5230-ba8a-11d1-bf5d-0000f805f530}") != -1,
            IDS_DEV_NAME_NOT_WDM_PNP_COMPLIANT
            );
    }

    TestEnd();

    TestStart(
        IDS_NULL_PTR_CHK,
        l_uTestNo++
        );

    for (l_iIndex = 0;
         l_iIndex<NUM_TEST_ATTRIBUTES && l_aAttr[l_iIndex].m_pchName; 
         l_iIndex++)
    {
        // try to crash reader by using null pointers as arguments
        l_bResult = DeviceIoControl(
            l_hReader,
            IOCTL_SMARTCARD_GET_ATTRIBUTE,
            &l_aAttr[l_iIndex].m_uType,
            sizeof(ULONG),
            NULL,
            1000,
            &l_uReplyLength,
            NULL
            );

        ULONG l_lResult = GetLastError();

        TestCheck(
            l_lResult == ERROR_INSUFFICIENT_BUFFER ||
            l_lResult == ERROR_BAD_COMMAND,
            IDS_IOCTRL_SMARTCARD_GET_ATTRIBUTE_SHOULD_RET,
            l_aAttr[l_iIndex].m_uType & 0xFFFF,
            l_lResult,
            MapWinErrorToNtStatus(l_lResult),
            ERROR_INSUFFICIENT_BUFFER,
            MapWinErrorToNtStatus(ERROR_BAD_COMMAND),
            ERROR_BAD_COMMAND,
            MapWinErrorToNtStatus(ERROR_BAD_COMMAND)
            );
    }

    TestEnd();

    for (l_iIndex = 0; l_iIndex < sizeof(l_aAttr) / sizeof(l_aAttr[0]); l_iIndex++)
    {
        if (l_aAttr[l_iIndex].m_pchName == 0)
        {
            DWORD l_dwThId   = 0;
            HANDLE l_hThread = 0;
            DWORD l_dwReturn = 0;
            LONG l_lResult   = 0;

            TestStart(IDS_CLOSE_DRIVER_WITH_IO_REQ_PENDING, l_uTestNo++);

            // Check if the reader correctly terminates pending io-requests
            
            // Cancel style 1:
            // Create a thread to close and re-open the driver while there is a pending
            // IO-request.  Thread will give enough time for IOCTL_SMARTCARD_IS_PRESENT
            // to begin before closing and re-opening the driver.
            l_hThread = CreateThread(NULL, 
                                     0, 
                                     (LPTHREAD_START_ROUTINE) CloseReopenReaderThread,
                                     &in_CReader,
                                     0,
                                     &l_dwThId);

            l_bResult = DeviceIoControl(
                l_hReader,
                IOCTL_SMARTCARD_IS_PRESENT,
                NULL,
                0,
                NULL,
                0,
                &l_uReplyLength,
                NULL
                );

            l_lResult=GetLastError();
            TestCheck(
                l_bResult == FALSE && 
                    MapWinErrorToNtStatus(l_lResult)==STATUS_CANCELLED,
                IDS_RET_VAL,
                l_lResult,
                MapWinErrorToNtStatus(l_lResult)
                );

            TestCheck(
                WaitForSingleObject(l_hThread,MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                    GetExitCodeThread(l_hThread, &l_dwReturn) &&
                    TRUE==l_dwReturn,
                IDS_READER_FAILED_TO_TERMINATE_PENDING_IO_REQ
            );
            CloseHandle(l_hThread);

            TestEnd();


            // Cancel style 2:
            // Create a thread to cancel the blocking IO call by calling 
            // DeviceIoControl(IOCTL_SMARTCARD_CANCEL_BLOCKING)
            TestStart(
                IDS_CANCEL_BLOCKING_DEVICEIOCONTROL_CALL,
                l_uTestNo++
                );

            // Need to refresh handle since re-open of previous test.
            l_hReader = in_CReader.GetHandle(); 
            l_hThread = CreateThread(NULL, 
                                     0, 
                                     (LPTHREAD_START_ROUTINE) CancelIoThread,
                                     l_hReader,
                                     0,
                                     &l_dwThId);

            l_bResult = DeviceIoControl(
                l_hReader,
                IOCTL_SMARTCARD_IS_PRESENT,
                NULL,
                0,
                NULL,
                0,
                &l_uReplyLength,
                NULL
                );

            l_lResult = GetLastError();
            TestCheck(
                l_bResult == FALSE &&
                    MapWinErrorToNtStatus(l_lResult)==STATUS_CANCELLED,
                IDS_RET_VAL,
                l_lResult,
                MapWinErrorToNtStatus(l_lResult)
                );

            TestCheck(
                WaitForSingleObject(l_hThread,MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                    GetExitCodeThread(l_hThread, &l_dwReturn) &&
                    ERROR_SUCCESS==l_dwReturn,
                IDS_CANCELIO_RETURN_ERROR,
                l_dwReturn
            );
            CloseHandle(l_hThread);

            TestEnd();

            if (TestFailed())
            {
                // If the open failed we can't continue
                //LogClose();
                return;
            }

            l_hReader = in_CReader.GetHandle();

            LogMessage(LOG_CONSOLE, IDS_PLS_INS_SAMPLE_PC_SC_COMPLIANCE_CARD);
            in_CReader.WaitForCardInsertion();
            l_bCardInserted = TRUE;

            // Cold reset
            TestStart(IDS_COLD_RESET, l_uTestNo++);

            l_uStart = GetTickCount();
            l_lResult = in_CReader.ColdResetCard();
            l_uEnd = GetTickCount();

            TEST_CHECK_SUCCESS(
                TestLoadStringResource(IDS_COLD_RESET_FAILED).GetBuffer(0),
                l_lResult);

            TestCheck(
                (l_uEnd - l_uStart) / CLOCKS_PER_SEC <= 2,
                IDS_COLD_RESET_TOOK_TIME,
                (l_uEnd - l_uStart) / CLOCKS_PER_SEC,
                2
                );
            TestEnd();

            if (TestFailed())
            {
                return;
            }

            // Set protocol
            TestStart(
                IDS_SET_PROTOCOL_T0_T1,
                l_uTestNo++
                );

            l_lResult = in_CReader.SetProtocol(
                SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1
                );

            TEST_CHECK_SUCCESS(
                TestLoadStringResource(IDS_SET_PROTOCOL_FAILED).GetBuffer(0),
                l_lResult
                );

            TestEnd();
            continue;
        }

        TestStart(
            IDS_TESTNUM_TESTNAME,
            l_uTestNo++,
            l_aAttr[l_iIndex].m_pchName
            );

        SetLastError(0);
        *(reinterpret_cast<PULONG>(l_rgbReplyBuffer)) = 0;

        l_bResult = DeviceIoControl(
            l_hReader,
            IOCTL_SMARTCARD_GET_ATTRIBUTE,
            &l_aAttr[l_iIndex].m_uType,
            sizeof(ULONG),
            l_rgbReplyBuffer,
            sizeof(l_rgbReplyBuffer),
            &l_uReplyLength,
            NULL
        );  


        if (GetLastError() != ERROR_SUCCESS)
        {
            l_bResult = FALSE;
        }
        else
        {
            l_rgbReplyBuffer[sizeof(l_rgbReplyBuffer)-1] = 0;
        }

        LONG l_lResult = GetLastError();

        switch (l_aAttr[l_iIndex].m_uType)
        {
            case SCARD_ATTR_VENDOR_NAME:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_IOCTL_SCARD_ATT_VENDOR_NAME_FAILED).GetBuffer(0),
                    l_lResult
                    );
                TestCheck(
                    strlen((PCHAR) l_rgbReplyBuffer) != 0,
                    IDS_NO_VENDOR_NAME_DEFINED
                    );
                TestEnd();
                break;

            case SCARD_ATTR_VENDOR_IFD_TYPE:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_SCARD_ATTR_VENDOR_IFD_TYPE_FAILED).GetBuffer(0),
                    l_lResult
                    );
                TestCheck(
                    strlen((PCHAR) l_rgbReplyBuffer) != 0,
                    IDS_NO_IFD_TYPE_DEFINED
                    );
                TestEnd();
                break;

            case SCARD_ATTR_DEVICE_UNIT:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_SCARD_ATTR_DEVICE_UNIT_FAILED).GetBuffer(0),
                    l_lResult
                    );
                TestCheck(
                    *(PULONG) l_rgbReplyBuffer < 4,
                    IDS_INVALID_VAL_0_3,
                    *(PULONG) l_rgbReplyBuffer
                    );
                TestEnd();
               break;

            case SCARD_ATTR_ATR_STRING:
                if (l_bCardInserted)
                {
                    TEST_CHECK_SUCCESS(
                        TestLoadStringResource(IDS_SCARD_ATTR_ATR_STRING_FAILED).GetBuffer(0),
                        l_lResult
                        );
                }
                else
                {
                    TestCheck(
                        l_bResult == FALSE,
                        IDS_READER_RET_ATR_WITH_NO_CARD_INS
                        );
                }
                TestEnd();
               break;

            case SCARD_ATTR_DEFAULT_CLK:
            case SCARD_ATTR_MAX_CLK:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_SCARD_ATTR_DEFAULT_CLK_SCARD_ATTR_MAX_CLK_FAILED).GetBuffer(0),
                    l_lResult
                    );
                TestCheck(
                    *(PULONG) l_rgbReplyBuffer >= 1000 && *(PULONG) l_rgbReplyBuffer <= 20000,
                    IDS_INVALID_VAL_1000_20000,
                    *(PULONG) l_rgbReplyBuffer
                    );
                TestEnd();
                break;

            case SCARD_ATTR_DEFAULT_DATA_RATE:
            case SCARD_ATTR_MAX_DATA_RATE:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_SCARD_ATTR_DEFAULT_DATA_RATE_SCARD_ATTR_MAX_DATA_RATE_FAILED).GetBuffer(0),
                    l_lResult
                    );
                TestEnd();
               break;

            case SCARD_ATTR_MAX_IFSD:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_SCARD_ATTR_MAX_IFSD_FAILED).GetBuffer(0),
                    l_lResult
                    );
                TestCheck(
                    *(PULONG) l_rgbReplyBuffer >= 1 && *(PULONG) l_rgbReplyBuffer <= 254,
                    IDS_INVALID_VAL_1_254,
                    *(PULONG) l_rgbReplyBuffer
                    );
                TestEnd();
               break;

            case SCARD_ATTR_PROTOCOL_TYPES:
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_SCARD_ATTR_PROTOCOL_TYPES_FAILED).GetBuffer(0),
                    l_lResult
                    );

                // check if the reader at least supports T=0 and T=1
                TestCheck(
                    (*(PULONG) l_rgbReplyBuffer & SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1) ==
                    (SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1),
                    IDS_READER_MUST_SUPPORT_T_0_AND_1
                    );
                TestEnd();
                break;

            case SCARD_ATTR_CURRENT_PROTOCOL_TYPE:
                if (l_bCardInserted)
                {
                    TEST_CHECK_SUCCESS(
                        TestLoadStringResource(IDS_SCARD_ATTR_CURRENT_PROTOCOL_TYPE_FAILED).GetBuffer(0),
                        l_lResult
                        );

                    TestCheck(
                        *(PULONG) l_rgbReplyBuffer != 0,
                        IDS_READER_RETURNED_NO_PROTOCOL
                        );
                }
                else
                {
                    // Check that without a card the current protocol is set to 0
                    TestCheck(
                        l_bResult == FALSE,
                        SCARD_ATTR_CURRENT_PROTOCOL_TYPE_FAILED
                        );
                }
                TestEnd();
                break;

            default:
                TestCheck(
                    l_bResult,
                    IDS_IOCTL_RET,
                    GetLastError()
                    );
                TestEnd();
                break;
        }
    }

    LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
    in_CReader.WaitForCardRemoval();
    LogMessage(LOG_CONSOLE, IDS_PLS_INS_SMART_CARD);
    in_CReader.WaitForCardInsertion();

    TestStart(
        IDS_IOCTL_SMARTCARD_GET_STATE,
        l_uTestNo++
        );

    ULONG l_uState;

    l_bResult = DeviceIoControl(
        l_hReader,
        IOCTL_SMARTCARD_GET_STATE,
        NULL,
        0,
        &l_uState,
        sizeof(l_uState),
        &l_uReplyLength,
        NULL
        );

    LONG l_lResult = GetLastError();

    TestCheck(
        l_bResult,
        IDS_IOCTL_SMARTCARD_GET_STATE_FAILED,
        l_lResult,
        MapWinErrorToNtStatus(l_lResult),
        ERROR_SUCCESS,
        MapWinErrorToNtStatus(ERROR_SUCCESS)
        );

    TestCheck(
        l_uState <= SCARD_SWALLOWED,
        IDS_INVALID_READER_STATE,
        l_uState,
        SCARD_SWALLOWED
        );

    TestEnd();

    TestStart(
        IDS_COLD_RESET,
        l_uTestNo++
        );

    l_uStart = GetTickCount();
    l_lResult = in_CReader.ColdResetCard();
    l_uEnd = GetTickCount();

    TestCheck(
        l_lResult == ERROR_UNRECOGNIZED_MEDIA,
        IDS_COLD_RESET_FAILED_WITH_RET_VAL,
        l_lResult,
        MapWinErrorToNtStatus(l_lResult),
        ERROR_UNRECOGNIZED_MEDIA,
        MapWinErrorToNtStatus(ERROR_UNRECOGNIZED_MEDIA)
        );

    TestCheck(
       (l_uEnd - l_uStart) / CLOCKS_PER_SEC <= 2,
       IDS_COLD_RESET_TOOK_TIME,
       (l_uEnd - l_uStart) / CLOCKS_PER_SEC,
       2
       );

    TestEnd();
}

void
SimulateResMngr(
    CReader &in_CReader
    )
{
    BOOLEAN l_bWaitForPresent = FALSE, l_bWaitForAbsent = FALSE;
    BOOL l_bResult;
    BOOLEAN l_bMustWait = FALSE, l_bPoweredDown = FALSE;
    ULONG l_uState, l_uStatus, l_uReplyLength, l_uStateExpected = SCARD_ABSENT, l_lTestNo = 1;
    ULONG l_uMinorIoctl;
    HANDLE l_hReader = in_CReader.GetHandle();

    LogMessage(LOG_CONSOLE, L"===================================");
    LogMessage(LOG_CONSOLE, IDS_PART_C);
    LogMessage(LOG_CONSOLE, L"===================================");

    LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);

    in_CReader.WaitForCardRemoval();

    while (TRUE)
    {
        TestStart(
            IDS_IOCTL_SMARTCARD_GET_STATE,
            l_lTestNo++
            );

        l_bResult = DeviceIoControl(
            l_hReader,
            IOCTL_SMARTCARD_GET_STATE,
            NULL,
            0,
            &l_uState,
            sizeof(l_uState),
            &l_uReplyLength,
            NULL
            );

        LONG l_lResult = GetLastError();

        TestCheck(
            l_bResult,
            IDS_IOCTL_SMARTCARD_GET_STATE_FAILED,
            l_lResult,
            MapWinErrorToNtStatus(l_lResult),
            ERROR_SUCCESS,
            MapWinErrorToNtStatus(ERROR_SUCCESS)
            );

        TestEnd();

        if (l_bWaitForPresent)
        {
            TestStart(
                IDS_TEST_NO_INS_CARD,
                l_lTestNo++
                );

            l_bResult=(in_CReader.WaitForCardInsertion()==STATUS_SUCCESS);
            l_lResult = GetLastError();

            TestCheck(
                l_bResult,
                IDS_CARD_INS_FAILED_RET_VAL,
                l_lResult,
                MapWinErrorToNtStatus(l_lResult),
                ERROR_SUCCESS,
                MapWinErrorToNtStatus(ERROR_SUCCESS)
                );

            l_lResult = GetLastError();

            TestEnd();

            l_bWaitForPresent = FALSE;
            continue;
        }

        if (l_bWaitForAbsent)
        {
            if (l_bMustWait)
            {
                TestStart(
                    IDS_TEST_NO_REM_CARD,
                    l_lTestNo++
                    );

                // In Windows Embedded CE, there is no way to get an overlapped result,
                // so we do something different here compared to the desktop.
                // In Windows Embedded CE, we have to wait for card removal.
                // And below, we just get result of a cancel operation.
                in_CReader.WaitForCardRemoval();
            }
            else
            {
                TestStart(
                    IDS_GET_OVERLAPPED_RESULT,
                    l_lTestNo++
                    );
            }

            // There is no GetOverlappedResult in Windows Embedded CE
            // Instead, we just test cancellation of the request and expect to:
            // - see cancellation of a blocking request (when a card is in slot)
            // - see success of a blocking request (when a card is not in slot)
            DWORD l_dwThId = 0;
            HANDLE hThread = CreateThread(NULL, 
                                          0, 
                                          (LPTHREAD_START_ROUTINE) CancelIoThread,
                                          l_hReader,
                                          0,
                                          &l_dwThId);
            l_bResult = DeviceIoControl (
                                         l_hReader,
                                         IOCTL_SMARTCARD_IS_ABSENT,
                                         NULL, 
                                         0,
                                         NULL,
                                         0,
                                         &l_uReplyLength,
                                         NULL//&l_OvrWait
                                        );

            l_lResult = GetLastError();
            WaitForSingleObject(hThread,MAX_THREAD_TIMEOUT);
            CloseHandle(hThread);

            if (l_bMustWait == FALSE)
            {
                TestCheck(
                    FALSE == l_bResult &&
                        STATUS_CANCELLED == MapWinErrorToNtStatus(l_lResult),
                    IDS_SMART_CARD_NOT_REMOVED
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }
            }
            else
            {
                TestCheck(
                    l_bResult,
                    IDS_CARD_REM_FAILED_RET_VAL,
                    l_lResult,
                    MapWinErrorToNtStatus(l_lResult),
                    ERROR_SUCCESS,
                    MapWinErrorToNtStatus(ERROR_SUCCESS)
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }
                l_bWaitForAbsent = FALSE;
                continue;
            }
        }

        TestStart(
            IDS_CHECKING_READER_STATUS,
            l_lTestNo++
            );

        switch (l_uState)
        {
            case SCARD_UNKNOWN:
                TestCheck(
                    FALSE,
                    IDS_READER_RET_ILLEGAL_STATE
                    );
                TestEnd();
                return;

            case SCARD_ABSENT:
                TestCheck(
                    l_uStateExpected == SCARD_ABSENT,
                    IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                    l_uState,
                    l_uStateExpected
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }

                if (l_bMustWait)
                {
                    return;
                }

                // Check for the WinCE equivalent of ERROR_IO_PENDING.  
                // There is currently no card in the slot, so we set up the
                // driver to wait for an insertion, then we cancel the request
                // and make sure the original call to
                // IOCTL_SMARTCARD_IS_PRESENT fails and the correct error code
                // is returned.
                TestStart(
                    IDS_IOCTL_SMART_CARD_PRESENT,
                    l_lTestNo++
                    );
                {
                DWORD l_dwThId = 0;
                HANDLE hThread = CreateThread(NULL, 
                                              0,
                                              (LPTHREAD_START_ROUTINE) CancelIoThread,
                                              l_hReader,
                                              0,
                                              &l_dwThId);

                l_bResult = DeviceIoControl(
                    l_hReader,
                    IOCTL_SMARTCARD_IS_PRESENT,
                    NULL,
                    0,
                    NULL,
                    0,
                    &l_uReplyLength,
                    NULL//&l_OvrWait
                    );  

                DWORD l_dwReturn = GetLastError();
                TestCheck(
                    FALSE == l_bResult &&
                        STATUS_CANCELLED == MapWinErrorToNtStatus(l_dwReturn) &&
                        WaitForSingleObject(hThread,MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                        GetExitCodeThread(hThread, &l_dwReturn) &&
                        ERROR_SUCCESS == l_dwReturn,
                    IDS_CANCELIO_RETURN_ERROR, l_dwReturn
                    );
                CloseHandle(hThread);
                };
                TestEnd();

                if (TestFailed())
                {
                    return;
                }
                l_bWaitForPresent = TRUE;
                l_uStateExpected = SCARD_PRESENT;
                break;

            case SCARD_PRESENT:
            case SCARD_SWALLOWED:
            case SCARD_POWERED:
                if (l_bPoweredDown)
                {
                    TestCheck(
                        l_uStateExpected <= SCARD_POWERED,
                        IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                        l_uState,
                        l_uStateExpected
                        );

                    TestEnd();

                    if (TestFailed())
                    {
                        return;
                    }

                    l_bMustWait = TRUE;
                    l_uStateExpected = SCARD_ABSENT;
                    break;
                }

                TestCheck(
                    l_uStateExpected > SCARD_ABSENT,
                    IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                    l_uState,
                    l_uStateExpected
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }

                TestStart(
                    IDS_SMART_CARD_ABSENT,
                    l_lTestNo++
                    );
                {
                DWORD l_dwThId = 0;
                DWORD l_dwReturn = 0;
                HANDLE hThread = CreateThread(NULL,
                                              0,
                                              (LPTHREAD_START_ROUTINE) CancelIoThread,
                                              l_hReader,
                                              0,
                                              &l_dwThId);

                l_bResult = DeviceIoControl(
                    l_hReader,
                    IOCTL_SMARTCARD_IS_ABSENT,
                    NULL,
                    0,
                    NULL,
                    0,
                    &l_uReplyLength,
                    NULL//&l_OvrWait
                    );

                l_lResult = GetLastError();

                TestCheck(
                    WaitForSingleObject(hThread,MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                        GetExitCodeThread(hThread, &l_dwReturn) &&
                        ERROR_SUCCESS == l_dwReturn,
                    IDS_CANCELIO_RETURN_ERROR, 
                    l_dwReturn
                    );

                CloseHandle(hThread);
                };

                TestCheck(
                    l_bResult == FALSE &&
                        STATUS_CANCELLED == MapWinErrorToNtStatus(l_lResult) &&
                        IDS_IOCTL_SMARTCARD_IS_ABSENT_FAILED_RET_VAL,

                    l_lResult,

                    MapWinErrorToNtStatus(l_lResult),

                    STATUS_CANCELLED,

                    MapWinErrorToNtStatus((ULONG)STATUS_CANCELLED)
                    );

                TestEnd();

                l_bWaitForAbsent = TRUE;
                TestStart(
                    IDS_COLD_RESET_CARD,
                    l_lTestNo++
                    );

                l_uStatus = in_CReader.ColdResetCard();

                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_COLDRESET).GetBuffer(0),
                    l_uStatus
                    )

                // According to ISO/IEC 7816-3:1997(E), 6.6.1, 
                // After a cold reset and ATR, we can enter either
                // SCARD_NEGOTIABLE or SCARD_SPECIFIC.
                l_uStateExpected = SCARD_NEGOTIABLE;

                TestEnd();

                if (TestFailed())
                {
                    return;
                }
             break;

            case SCARD_NEGOTIABLE:
                TestCheck(
                    l_bPoweredDown == FALSE,
                    IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                    l_uState,
                    SCARD_PRESENT
                    );

                TestCheck(
                    l_uStateExpected == SCARD_NEGOTIABLE,
                    IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                    l_uState,
                    l_uStateExpected
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }

                TestStart(
                    IDS_SET_PROTOCOL_T0_T1,
                    l_lTestNo++
                    );

                l_uStatus = in_CReader.SetProtocol(
                    SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1
                    );

                TestCheck(
                    l_uStatus == ERROR_SUCCESS,
                    IDS_PROTOCOL_SEL_FAILED_WITH_ERROR,
                    GetLastError()
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }
                l_uStateExpected = SCARD_SPECIFIC;
                break;

            case SCARD_SPECIFIC:
                TestCheck(
                    l_bPoweredDown == FALSE,
                    IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                    l_uState,
                    SCARD_PRESENT
                    );

                // According to the spec, after a cold reset and ATR,
                // we can enter either SCARD_NEGOTIABLE or SCARD_SPECIFIC.
                TestCheck(
                    l_uStateExpected==SCARD_NEGOTIABLE || l_uStateExpected==SCARD_SPECIFIC,
                    IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
                    l_uState,
                    l_uStateExpected
                    );

                TestEnd();

                if (TestFailed())
                {
                    return;
                }

                TestStart(
                    IDS_SCARD_POWER_DOWN,
                    l_lTestNo++
                    );

                l_uMinorIoctl = SCARD_POWER_DOWN;
                SetLastError(0);

                l_bResult = DeviceIoControl(
                    l_hReader,
                    IOCTL_SMARTCARD_POWER,
                    &l_uMinorIoctl,
                    sizeof(l_uMinorIoctl),
                    NULL,
                    0,
                    &l_uReplyLength,
                    NULL
                    );

                l_lResult = GetLastError();

                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_IOCTL_SMARTCARD_POWER_FAILED).GetBuffer(0),
                    l_lResult
                    );

                TestEnd();

                l_uStateExpected = SCARD_PRESENT;
                l_bPoweredDown = TRUE;
             break;

            default:
               TestCheck(
                    FALSE,
                    IDS_READER_RET_INVALID_STATE,
                    l_uState
                    );
                TestEnd();
                return;
        }
    }
}

//***************************************************************************
// See header file for description.
//***************************************************************************
void 
CardProviderTest(
    CReader &in_CReader,
    BOOL    in_bTestOneCard,
    ULONG   in_uTestNo,
    BOOL    in_bReqCardRemoval,
    BOOL    in_bStress
    )
{
    LONG l_lResult = 0;
    CCardProvider l_CCardProvider;

    // Reset any previous results.
    CCardProvider::SetAllCardsUntested();

    // Main loop.
    while (l_CCardProvider.CardsUntested())
    {
        LogMessage(LOG_CONSOLE, L"================================");
        LogMessage(LOG_CONSOLE, IDS_PART_D);
        LogMessage(LOG_CONSOLE, L"================================");

        LogMessage(LOG_CONSOLE, IDS_INS_ANY_PC_SC_COMPLIANCE_CARD);
        l_CCardProvider.ListUntestedCards();

        // Insert the card
        LogMessage(LOG_CONSOLE, IDS_INSERT_CARD);
        if (in_CReader.WaitForCardInsertion() != ERROR_SUCCESS)
        {
            TEST_CHECK_SUCCESS(
                TestLoadStringResource(IDS_READER_FAILED).GetBuffer(0),
                !ERROR_SUCCESS);
            return;
        }
            
        // Reset the card
        // To make things easier for the user, let's not automatically fail if
        // if you can't reset the card.  Let the user try again.
        if (in_CReader.ColdResetCard() != ERROR_SUCCESS)
        {
            if (!in_bStress)
            {
                LogMessage(LOG_CONSOLE, IDS_UNABLE_TO_RESET_SMART_CARD);
                LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
                if (in_CReader.WaitForCardRemoval() != ERROR_SUCCESS)
                {
                    TEST_CHECK_SUCCESS(
                        TestLoadStringResource(IDS_READER_FAILED_CARD_REM).GetBuffer(0),
                        !ERROR_SUCCESS
                        );
                    return;
                }
            }
            else
            {
                TEST_CHECK_SUCCESS(
                    TestLoadStringResource(IDS_UNABLE_TO_RESET_SMART_CARD).GetBuffer(0),
                    !ERROR_SUCCESS
                    );
                return;
            }
        }
        else
        {
            // Try to run tests with this card
            l_CCardProvider.CardTest(in_CReader, in_uTestNo);

            // Remove card if necessary.
            if (in_bReqCardRemoval)
            {
                LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
                if (in_CReader.WaitForCardRemoval() != ERROR_SUCCESS)
                {
                    TEST_CHECK_SUCCESS(
                        TestLoadStringResource(IDS_READER_FAILED_CARD_REM).GetBuffer(0),
                        !ERROR_SUCCESS
                        );
                    return;
                }
            }

            // Quit the program if we only run one test or if we've failed.
            if (in_uTestNo != 0 || in_bTestOneCard || ReaderFailed())
            {
                break;
            }
        }
    } // while
}
                 



void
PowerManagementTest(
    CReader &in_CReader,
    ULONG in_uWaitTime
    )
{
    const DWORD MAX_RETRIES = 10;
    DWORD  l_dwCount   = 0;
    
    BOOL   l_bRet      = FALSE;
    HANDLE l_hThread   = 0;
    DWORD  l_dwThId    = 0;
    DWORD  l_dwRet     = 0;
    LONG   l_lResult   = 0;
    ULONG  l_uState    = 0;
    ULONG  l_uDuration = 10;

    if (in_uWaitTime > 0 && in_uWaitTime <= 120)
    {
        l_uDuration = in_uWaitTime;
    }

    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_PART_E);
    LogMessage(LOG_CONSOLE, L"=============================");

    LogMessage(LOG_CONSOLE, IDS_NOTE);
    LogMessage(LOG_CONSOLE, IDS_INSTRUCTION);

    // Instantiate utility class
    g_pCSysPwr = new CSysPwrStates();
    if (NULL == g_pCSysPwr)
    {
        TestCheck(
            FALSE,
            IDS_COULD_NOT_ALLOCATE_POWER_UTILITY
            );
        return;
    }

    // Make sure there's no card in the slot before we begin.
    LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
    l_lResult = in_CReader.WaitForCardRemoval();
    if (ERROR_SUCCESS != l_lResult) {
        TEST_CHECK_SUCCESS(
            TestLoadStringResource(IDS_READER_FAILED_CARD_REM).GetBuffer(0),
            l_lResult
        );
        goto cleanup;
    }


    // Test 1
    LogMessage(LOG_CONSOLE, IDS_TEST1);
    TestStart(IDS_HIBERNATING_NOW);

    // Spawn thread to suspend us and then wake us up.  The thread will wait
    // enough time for the following insert card command to start before continuing.
    l_hThread = CreateThread(NULL, 
                             0, 
                             (LPTHREAD_START_ROUTINE) SuspendDeviceAndScheduleWakeupThread, 
                             (LPVOID) l_uDuration,
                             0,
                             &l_dwThId);
    if (NULL == l_hThread) {
        TestCheck(
            NULL != l_hThread,
            IDS_CANNOT_CREATE_SUSPEND_WAKEUP_THREAD,
            GetLastError()
            );
        goto cleanup;
    }


    // This command will execute before we begin suspend.
    // Recall WaitForCardXXX is a blocking call.
    l_lResult = in_CReader.WaitForCardInsertion();
    TestCheck(STATUS_CANCELLED==MapWinErrorToNtStatus(l_lResult),
              IDS_WAIT_FOR_PRESENT_SUCCEED_UNEXPECTED);

    // Wait for suspend-wakeup thread to complete its operation.
    TestCheck(
        WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
            GetExitCodeThread(l_hThread, &l_dwRet) && 
            ERROR_SUCCESS==l_dwRet,
        IDS_SUSPEND_AND_WAKEUP_RETURN_ERROR, 
        l_dwRet
    );
    TestCheck(CloseHandle(l_hThread), IDS_CANNOT_CLOSE_THREAD_HANDLE, GetLastError());
    TestEnd();

    // If we failed, jump out early.
    if (TestFailed())
    {
        goto cleanup;
    }


    // Verify that old reader handle is no longer valid.
    TestStart(IDS_TEST_CHECK_INVALID_HANDLE);
    l_lResult = in_CReader.GetState(&l_uState);
    TestCheck(ERROR_DEVICE_REMOVED == l_lResult
                || ERROR_OPERATION_ABORTED == l_lResult,
              IDS_HANDLE_UNEXPECTEDLY_VALID, l_uState);

    // After suspending and resuming, the driver will re-enumerate.  We need
    // a new (valid) device handle.
    TestCheck(in_CReader.Close(), IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    for (l_dwCount = 0; l_dwCount < MAX_RETRIES; l_dwCount++)
    {
        l_bRet = in_CReader.Open();
        if (l_bRet)
        {
            break;
        }
        Sleep(1000);
    }
    TestCheck(l_bRet, IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();
    

    // Check that card state is still valid.
    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(
        SCARD_ABSENT == l_uState,
        IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();


    // Wait for the card to be inserted in preparation for test 2.
    TestStart(IDS_INSERT_CARD);
    l_lResult = in_CReader.WaitForCardInsertion();
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED).GetBuffer(0),
        l_lResult
        );
    TestEnd();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(
        l_uState > SCARD_ABSENT,
        IDS_INVALID_READER_RET_EXPECTED,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();


    // If we failed, jump out early.
    if (TestFailed())
    {
        goto cleanup;
    }


    // Test 2
    LogMessage(LOG_CONSOLE, IDS_TEST2);
    TestStart(IDS_CARD_IN_OUT);

    // Spawn thread to suspend us and then wake us up.  The thread will wait
    // enough time for the following insert card command to start before continuing.
    l_hThread = CreateThread(NULL, 
                             0, 
                             (LPTHREAD_START_ROUTINE) SuspendDeviceAndScheduleWakeupThread, 
                             (LPVOID) l_uDuration,
                             0,
                             &l_dwThId);
    if (NULL == l_hThread) {
        TestCheck(
            NULL != l_hThread,
            IDS_CANNOT_CREATE_SUSPEND_WAKEUP_THREAD,
            GetLastError()
            );
        goto cleanup;
    }

    // This command will execute before we suspend.
    // Recall WaitForCardXXX is a blocking call.
    l_lResult = in_CReader.WaitForCardRemoval();
    TestCheck(STATUS_CANCELLED==MapWinErrorToNtStatus(l_lResult),
              IDS_WAIT_FOR_ABSENT_SUCCEED_UNEXPECTED); 

    // Wait for suspend-wakeup thread to complete its operation.
    TestCheck(
        WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
            GetExitCodeThread(l_hThread, &l_dwRet) && 
            ERROR_SUCCESS==l_dwRet,
        IDS_SUSPEND_AND_WAKEUP_RETURN_ERROR, 
        l_dwRet            
    );
    TestCheck(CloseHandle(l_hThread), IDS_CANNOT_CLOSE_THREAD_HANDLE, GetLastError());
    TestEnd();

    // If we failed, jump out early.
    if (TestFailed())
    {
        goto cleanup;
    }

    
    // Verify that old reader handle is no longer valid.
    TestStart(IDS_TEST_CHECK_INVALID_HANDLE);
    l_lResult = in_CReader.GetState(&l_uState);
    TestCheck(ERROR_DEVICE_REMOVED == l_lResult
                || ERROR_OPERATION_ABORTED == l_lResult,
              IDS_HANDLE_UNEXPECTEDLY_VALID, l_uState);

    // After suspending and resuming, the driver will re-enumerate.  We need
    // a new (valid) device handle.
    TestCheck(in_CReader.Close(), IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    for (l_dwCount = 0; l_dwCount < MAX_RETRIES; l_dwCount++)
    {
        l_bRet = in_CReader.Open();
        if (l_bRet)
        {
            break;
        }
        Sleep(1000);
    }
    TestCheck(l_bRet, IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);

    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );

    TestCheck(
        l_uState == SCARD_ABSENT,
        IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();


    LogMessage(LOG_CONSOLE, IDS_INSERT_CARD);
    in_CReader.WaitForCardInsertion();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(
        l_uState > SCARD_ABSENT,
        IDS_INVALID_READER_RET_EXPECTED,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();


    // If we failed, jump out early.
    if (TestFailed())
    {
        goto cleanup;
    }


    // Test 3
    LogMessage(LOG_CONSOLE, IDS_TEST3);
    TestStart(IDS_CARD_IN_IN);

    // Spawn thread to suspend us and then wake us up.  The thread will wait
    // enough time for the following insert card command to start before continuing.
    l_hThread = CreateThread(NULL, 
                             0, 
                             (LPTHREAD_START_ROUTINE) SuspendDeviceAndScheduleWakeupThread, 
                             (LPVOID) l_uDuration,
                             0,
                             &l_dwThId);
    if (NULL == l_hThread) {
        TestCheck(
            NULL != l_hThread,
            IDS_CANNOT_CREATE_SUSPEND_WAKEUP_THREAD,
            GetLastError()
            );
        goto cleanup;
    }

    // This command will execute before we shutdown.
    // Recall WaitForCardXXX is a blocking call.
    l_lResult = in_CReader.WaitForCardRemoval();
    TestCheck(STATUS_CANCELLED==MapWinErrorToNtStatus(l_lResult),
              IDS_WAIT_FOR_ABSENT_SUCCEED_UNEXPECTED);

    // Wait for suspend-wakeup thread to complete its operation.
    TestCheck(
        WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
            GetExitCodeThread(l_hThread, &l_dwRet) && 
            ERROR_SUCCESS==l_dwRet,
        IDS_SUSPEND_AND_WAKEUP_RETURN_ERROR, 
        l_dwRet
    );
    TestCheck(CloseHandle(l_hThread), IDS_CANNOT_CLOSE_THREAD_HANDLE, GetLastError());
    TestEnd();

    // If we failed, jump out early.
    if (TestFailed())
    {
        goto cleanup;
    }


    // Verify that old reader handle is no longer valid.
    TestStart(IDS_TEST_CHECK_INVALID_HANDLE);
    l_lResult = in_CReader.GetState(&l_uState);
    TestCheck(ERROR_DEVICE_REMOVED == l_lResult
                || ERROR_OPERATION_ABORTED == l_lResult,
              IDS_HANDLE_UNEXPECTEDLY_VALID, l_uState);

    // After suspending and resuming, the driver will re-enumerate.  We need
    // a new (valid) device handle.
    TestCheck(in_CReader.Close(), IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    for (l_dwCount = 0; l_dwCount < MAX_RETRIES; l_dwCount++)
    {
        l_bRet = in_CReader.Open();
        if (l_bRet)
        {
            break;
        }
        Sleep(1000);
    }
    TestCheck(l_bRet, IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(
        l_uState > SCARD_ABSENT,
        IDS_INVALID_READER_RET_EXPECTED,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();


    LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
    in_CReader.WaitForCardRemoval();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(
        l_uState == SCARD_ABSENT,
        IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();
    

    // If we failed, jump out early.
    if (TestFailed())
    {
        goto cleanup;
    }


    // Test 4
    LogMessage(LOG_CONSOLE, IDS_TEST4);
    TestStart(IDS_CARD_OUT_IN);

    // Spawn thread to suspend us and then wake us up.  The thread will wait
    // enough time for the following insert card command to start before continuing.
    l_hThread = CreateThread(NULL, 
                             0, 
                             (LPTHREAD_START_ROUTINE) SuspendDeviceAndScheduleWakeupThread, 
                             (LPVOID) l_uDuration,
                             0,
                             &l_dwThId);
    if (NULL == l_hThread)
    {
        l_dwRet = GetLastError();
        LogMessage(LOG_CONSOLE, 
                   IDS_CANNOT_CREATE_SUSPEND_WAKEUP_THREAD,
                   l_dwRet);
        goto cleanup;
    }

    // This command will execute before we shutdown.
    // Recall WaitForCardXXX is a blocking call.
    l_lResult = in_CReader.WaitForCardInsertion();
    TestCheck(STATUS_CANCELLED==MapWinErrorToNtStatus(l_lResult),
              IDS_WAIT_FOR_PRESENT_SUCCEED_UNEXPECTED);

    // Wait for suspend-wakeup thread to complete its operation.
    TestCheck(
        WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
            GetExitCodeThread(l_hThread, &l_dwRet) && 
            ERROR_SUCCESS==l_dwRet,
        IDS_SUSPEND_AND_WAKEUP_RETURN_ERROR, 
        l_dwRet
    );
    TestCheck(CloseHandle(l_hThread), IDS_CANNOT_CLOSE_THREAD_HANDLE, GetLastError());
    TestEnd();

    // If we failed, jump out early.
    if (TestFailed()) 
    {
        goto cleanup;
    }


    // Verify that old reader handle is no longer valid.
    TestStart(IDS_TEST_CHECK_INVALID_HANDLE);
    l_lResult = in_CReader.GetState(&l_uState);
    TestCheck(ERROR_DEVICE_REMOVED == l_lResult
                || ERROR_OPERATION_ABORTED == l_lResult,
              IDS_HANDLE_UNEXPECTEDLY_VALID, l_uState);

    // After suspending and resuming, the driver will re-enumerate.  We need
    // a new (valid) device handle.
    TestCheck(in_CReader.Close(), IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    for (l_dwCount = 0; l_dwCount < MAX_RETRIES; l_dwCount++)
    {
        l_bRet = in_CReader.Open();
        if (l_bRet)
        {
            break;
        }
        Sleep(1000);
    }
    TestCheck(l_bRet, IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );

    TestCheck(
        l_uState > SCARD_ABSENT,
        IDS_INVALID_READER_RET_EXPECTED,
        l_uState,
        SCARD_ABSENT
        );

    TestEnd();

    // Have user remove card.
    // This prevents test from finishing before machine has fully returned 
    // from suspend.
    LogMessage(LOG_CONSOLE, IDS_REM_SMART_CARD);
    in_CReader.WaitForCardRemoval();


    TestStart(IDS_CHK_READER_STATUS);
    l_lResult = in_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(
        l_uState == SCARD_ABSENT,
        IDS_INVALID_READER_STATE_CUR_EXPECTED_STATE,
        l_uState,
        SCARD_ABSENT
        );
    TestEnd();

cleanup:
    delete g_pCSysPwr;
    g_pCSysPwr = 0;
}





class CArgv
{
    int m_iArgc;
    LPTSTR *m_pArgv;
    BOOL *m_pfRef;

public:

    CArgv(int in_iArgc, __in_ecount(in_iArgc) wchar_t **in_pArgv);

    int OptionExist(__in const WCHAR*);

    PWCHAR ParameterExist(__in const WCHAR*);

    PWCHAR CheckParameters(CString);

    PWCHAR CArgv::ParameterUnused(void);
};


CArgv::CArgv(
    int in_iArgc,
    __in_ecount(in_iArgc) wchar_t **in_pArgv
    )
{
    m_iArgc = in_iArgc;
    m_pArgv = in_pArgv;
    m_pfRef = new BOOL[in_iArgc];
    if (m_pfRef)
    {
        memset(m_pfRef, 0, sizeof(BOOL) * in_iArgc);
    }
}

int
CArgv::OptionExist(
    __in const WCHAR* in_pchParameter
    )
{
    for (int i = 0; i < m_iArgc; i++)
    {
        if (m_pArgv[i][0] == '-' || m_pArgv[i][0] == '/')
        {
            int j = 1;

            while (m_pArgv[i][j] && m_pArgv[i][j] != ' ')
            {
                if (wcsncmp(m_pArgv[i] + j, in_pchParameter, wcslen(m_pArgv[i] + j)) == 0) {

                    m_pfRef[i] = TRUE;
                    return i;
                }

                j++;
            }
        }
    }

    return 0;
}

PWCHAR
CArgv::ParameterExist(
    __in const WCHAR* in_pchParameter
    )
{
    if (int i = OptionExist(in_pchParameter))
    {
        m_pfRef[i + 1] = TRUE;
        return m_pArgv[i + 1];
    }

    return NULL;
}

PWCHAR
CArgv::CheckParameters(
    CString in_CParameters
    )
/*++

Routine Description:
    Checks if the command line includes an invalid/unknown parameter

--*/
{
    int i, l_iPos;

    for (i = 1; i < m_iArgc; i++)
    {
        if ((l_iPos = in_CParameters.Find(m_pArgv[i])) == -1)
        {
            return m_pArgv[i];
        }

        if (l_iPos + 3 < in_CParameters.GetLength() &&
            in_CParameters[l_iPos + 3] == '*')
        {
            // skip the next parameter
            i += 1;
        }
    }
    return NULL;
}

PWCHAR
CArgv::ParameterUnused(
    void
    )
{
    int i;

    for (i = 1; i < m_iArgc; i++)
    {
        if (m_pfRef[i] == FALSE)
        {
            return m_pArgv[i];
        }
    }
    return NULL;
}

CString &
GetOperatingSystem(
    void
    )
{
    static CString l_COperatingSystem;
    OSVERSIONINFO VersionInformation;

    if (l_COperatingSystem.GetLength() == 0)
    {
        VersionInformation.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        if (GetVersionEx(&VersionInformation) == FALSE)
        {
            l_COperatingSystem += TestLoadStringResource(IDS_UNKNOWN_STATE);

        }
        else
        {
            if (VersionInformation.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
            {
                if (VersionInformation.dwMinorVersion == 0)
                {
                    l_COperatingSystem += OS_WIN95;

                }
                else
                {
                    l_COperatingSystem += OS_WIN98;
                }

            }
            else if (VersionInformation.dwPlatformId == VER_PLATFORM_WIN32_NT)
            {
                if (VersionInformation.dwMajorVersion <= 4)
                {
                    l_COperatingSystem += OS_WINNT4;

                }
                else
                {
                    l_COperatingSystem += OS_WINNT5;
                }

            }
            else if (VersionInformation.dwPlatformId == VER_PLATFORM_WIN32_CE)
            {
                l_COperatingSystem += OS_WINCE;               
            }
            else
            {
                l_COperatingSystem += TestLoadStringResource(IDS_UNKNOWN_STATE);
            }
        }
    }
    return l_COperatingSystem;
}

CString &
SelectReader(
    void
    )
{
    CReaderList l_CReaderList;
    ULONG l_uIndex, l_uReader;
    ULONG l_uNumReaders = l_CReaderList.GetNumReaders();
    static CString l_CEmpty("");

    if (l_uNumReaders == 0)
    {
        return l_CEmpty;
    }

    if (l_uNumReaders == 1)
    {
        return l_CReaderList.GetDeviceName(0);
    }

    CString l_CLetter;

    LogMessage(LOG_CONSOLE, L"\n");
    LogMessage(LOG_CONSOLE,
        L"%s",
        TestLoadStringResource(IDS_VENDOR_IFDTYPE).GetBuffer(0)
        );
    LogMessage(LOG_CONSOLE,L" -----------------------------------------------------");

    for (l_uIndex = 0; l_uIndex < l_uNumReaders; l_uIndex++)
    {
        INT l_iLetterPos;
        INT l_iLength = l_CReaderList.GetVendorName(l_uIndex).GetLength();
        CString l_CVendorName = l_CReaderList.GetVendorName(l_uIndex);

        for (l_iLetterPos = 0;
             l_iLetterPos < l_CVendorName.GetLength();
             l_iLetterPos++)
        {
            WCHAR l_chLetter = l_CVendorName[l_iLetterPos];

            if (l_chLetter == L' ' || l_chLetter == L'x')
            {
                continue;
            }

            if (l_CLetter.Find(l_chLetter) == -1)
            {
                l_CLetter += l_chLetter;
                break;
            }
        }
        if (l_iLetterPos >= l_iLength)
        {
            l_CVendorName += (WCHAR) (l_uIndex + L'0') ;
            l_iLetterPos = l_iLength;
        }

        LogMessage(LOG_CONSOLE,
            L" %s[%c]%-*s %-20s %8s",
            (LPCTSTR) l_CVendorName.Left(l_iLetterPos),
            l_CVendorName[l_iLetterPos],
            20 - l_iLetterPos,
            (LPCTSTR) l_CVendorName.Right(l_iLength - l_iLetterPos - 1),
            (LPCTSTR) l_CReaderList.GetIfdType(l_uIndex),
            (LPCTSTR) l_CReaderList.GetPnPType(l_uIndex)
            );
    }

    putchar('\n');
    do
    {
        LogMessage(LOG_CONSOLE,
            L"%s",
            TestLoadStringResource(IDS_SELECT_READER).GetBuffer(0)
            );

        TCHAR l_chInput = _gettchar();

        if (l_chInput == 3)
        {
            //LogClose();
            return l_CEmpty;
        }

        l_uReader = l_CLetter.Find(l_chInput);
    } while(l_uReader == -1);

    wprintf(L"\n");

    return l_CReaderList.GetDeviceName(l_uReader);
}

CString
SelectReader(
    const CString &in_CVendorName
    )
{
    CReaderList l_CReaderList;
    ULONG l_uIndex;
    ULONG l_uNumReaders = l_CReaderList.GetNumReaders();
    CString l_CVendorName = in_CVendorName;

    l_CVendorName.MakeLower();

    for (l_uIndex = 0; l_uIndex < l_uNumReaders; l_uIndex++)
    {
        CString l_CVendorListName = l_CReaderList.GetVendorName(l_uIndex);
        l_CVendorListName.MakeLower();

        if (l_CVendorListName.Find(l_CVendorName) != -1) {

            return l_CReaderList.GetDeviceName(l_uIndex);
        }
    }

    return CString("");
}


int __cdecl 
main(                                        
    int argc, 
    __in_ecount(argc) wchar_t* argv[],
    DWORD in_dwTestID
    )
{
        CArgv   l_CArgv(argc, argv);
        BOOLEAN l_bSuccess          = FALSE;
        UCHAR   l_ucRet             = TPR_FAIL;
        BOOLEAN l_fInvalidParameter = FALSE;
        BOOLEAN l_bReaderRequired   = TRUE;

        if(const WCHAR* const l_pchArgv = l_CArgv.CheckParameters(TestLoadStringResource(IDS_VALID_CMD_LINE_PARAMETERS).GetBuffer(0)))
        {
            LogMessage(LOG_CONSOLE, IDS_INVALID_PARAMETER, l_pchArgv);
            l_fInvalidParameter = TRUE;
        }

        if (l_fInvalidParameter ||
            l_CArgv.OptionExist(L"h"))
        {
            LogMessage(LOG_CONSOLE, IDS_USAGE);
            LogMessage(LOG_CONSOLE, IDS_USAGE_AUTOIFD);
            LogMessage(LOG_CONSOLE, IDS_USAGE_N);
            LogMessage(LOG_CONSOLE, IDS_USAGE_P);
            LogMessage(LOG_CONSOLE, IDS_USAGE_WAITING_TIME);
            LogMessage(LOG_CONSOLE, IDS_USAGE_DUMPS_ALL);
            LogMessage(LOG_CONSOLE, IDS_USAGE_READER_USING_DEV_NAME);
            LogMessage(LOG_CONSOLE, IDS_USAGE_READER_USING_VENDOR_NAME);
            LogMessage(LOG_CONSOLE, IDS_USAGE_SPECIFIC_TEST);
            
            return TPR_SKIP;
        }


        // Some tests don't require a reader to be initially inserted.
        // If they don't, these tests should:
        // 1.  Set l_bReaderRequired to FALSE.
        // 2.  Set l_bReaderInitialized to FALSE (so that the next tux test 
        //     can (re-initialize) properly).
        switch (in_dwTestID) {
            case TEST_ID_INSERTREMOVEREADERTTEST:
            case TEST_ID_INTERRUPTREADERTTEST:
            case TEST_ID_REMOVEREADERWHENBLOCKED:
            case TEST_ID_INSERTREMOVEDURINGSUSPEND:
            case TEST_ID_MULTIREADER_AUTOIFD:
                l_bReaderRequired = FALSE;
                break;

            default:
                l_bReaderRequired = TRUE;
        }

        static BOOLEAN l_bReaderInitialized = FALSE;
        static CReader l_CReader;
        CString l_CDeviceName;

        if (l_bReaderRequired && !l_bReaderInitialized) 
        {
            if (const WCHAR* const l_pchReader = l_CArgv.ParameterExist(L"r"))
            {
                l_CDeviceName = CString(l_pchReader);
            }
            else if (const WCHAR* const l_pchVendorName = l_CArgv.ParameterExist(L"v"))
            {
                CReaderList l_CReaderList;
                l_CDeviceName = SelectReader(CString(l_pchVendorName));
            }
            else
            {
                CReaderList l_CReaderList;
                l_CDeviceName = SelectReader();
            }

            if (l_CDeviceName == L"")
            {
                LogMessage(LOG_CONSOLE, IDS_NO_READER_FOUND);
                return TPR_FAIL;
            }

            // At this point, the reader is considered set up.
            // Whether or not you can properly open it is another story.
            l_bReaderInitialized = TRUE;
        }

        if (l_bReaderRequired)
        {
            if (l_CDeviceName == L"")
            {
                // Reader was previously initialized.  We can just open it directly.
                l_bSuccess = l_CReader.Open();
            }
            else
            {
                l_bSuccess = l_CReader.Open(l_CDeviceName);
            }

            LogMessage(LOG_CONSOLE, L".");
            if (l_bSuccess == FALSE)
            {
                LogMessage(LOG_CONSOLE, IDS_CAN_NOT_OPEN_SMART_CARD_READER);
                return TPR_FAIL;
            }
        }


        if (l_CArgv.OptionExist(L"d"))
        {
            l_CReader.SetDump(TRUE);
        }

        SYSTEMTIME systime;  
        GetSystemTime(&systime);

        if (l_bReaderRequired)
        {
            LogMessage(
                LOG_CONSOLE,
                IDS_VENDOR,
                l_CReader.GetVendorName()
                );

            LogMessage(
                LOG_CONSOLE,
                IDS_READER,
                l_CReader.GetIfdType()
                );
        }

        LogMessage(
            LOG_CONSOLE,
            IDS_DATE,
            systime.wMonth, 
            systime.wDay, 
            systime.wYear
            );

        LogMessage(
            LOG_CONSOLE,
            IDS_TIME,
            systime.wHour, 
            systime.wMinute
            );

        LogMessage(
            LOG_CONSOLE,
            IDS_OS,
            (LPCTSTR) GetOperatingSystem()
            );


        // Reset reader result from previously run tests.
        ResetReaderFailedFlag();

        // Run the appropriate test.
        switch(in_dwTestID) {
            case TEST_ID_CHECKCARDMONITOR:
                CheckCardMonitor(l_CReader);
                break;

            case TEST_ID_CHECKREADER:
                CheckReader(l_CReader);
                break;

            case TEST_ID_SIMULATERESMNGR:
                SimulateResMngr(l_CReader);
                break;

            case TEST_ID_SMARTCARDPROVIDERTEST:
            {
                BOOL l_bTestOneCard      = FALSE;
                ULONG l_uTestNo          = 0;
                const WCHAR* l_pchTestNo = L"";

                l_pchTestNo = l_CArgv.ParameterExist(L"t");
                if (l_pchTestNo)
                {
                    // The user wants us to run only one test of the card provider test
                    l_uTestNo = _wtoi(l_pchTestNo);
                }

                if (l_CArgv.OptionExist(L"onecard"))
                {
                    // The user wants us to run only one card, not the full set
                    l_bTestOneCard = TRUE;
                }

                CardProviderTest(l_CReader, l_bTestOneCard, l_uTestNo, TRUE, FALSE);
                break;
            }
            
            case TEST_ID_POWERMANAGEMENTTEST:
            {
                ULONG l_uWaitTime = 0;

                if (const WCHAR* const l_pchWaitTime = l_CArgv.ParameterExist(L"w"))
                {
                    // How long to wait before suspend.
                    l_uWaitTime = _wtoi(l_pchWaitTime);
                }

                PowerManagementTest(l_CReader, l_uWaitTime);
                break;
            }
        
            case TEST_ID_INSERTREMOVEREADERTTEST:
            {
                DWORD l_dwFullCycles               = 1;                // Def: one cycle
                DWORD l_dwInsRemCyclesBeforeTest   = 5;                // Def: 5
                DWORD l_bAlternateCardInsertedDuringTest = TRUE;       // Def: TRUE
                BOOL  l_bUSBSwitchBox              = FALSE;            // Def: None.
                DWORD l_dwMillisBeforeReaderAction = 2*CLOCKS_PER_SEC; // Def: 2s
                
                if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"n"))
                {
                    l_dwFullCycles = _wtoi(l_pTmp);
                }
                if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"p"))
                {
                    l_dwInsRemCyclesBeforeTest = _wtoi(l_pTmp);
                }
                if (l_CArgv.OptionExist(L"c"))
                {
                    l_bAlternateCardInsertedDuringTest = FALSE;
                }

                if (l_CArgv.OptionExist(L"u"))
                {
                    l_bUSBSwitchBox = TRUE;

                    if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"w"))
                    {
                        l_dwMillisBeforeReaderAction = _wtoi(l_pTmp);
                    }
                }
                
                ReaderInsertionRemovalTest(l_dwFullCycles, 
                                           l_dwInsRemCyclesBeforeTest,
                                           l_bAlternateCardInsertedDuringTest,
                                           l_bUSBSwitchBox,
                                           l_dwMillisBeforeReaderAction);
                break;
            }
            
            case TEST_ID_INTERRUPTREADERTTEST:
            {
                DWORD l_dwFullCycles                  = 1;  // By default, do one cycle.
                DWORD l_dwInsRemCyclesBeforeTest      = 1;  
                BOOL  l_bUSBSwitchBox                 = FALSE; // Default: None
                DWORD l_dwMillisBeforeReaderAction    = 2*CLOCKS_PER_SEC;
                DWORD l_dwMaxMillisSurpriseRemoval    = 5*CLOCKS_PER_SEC;
                BOOL  l_bUseRandomSurpriseRemovalTime = TRUE;  
                
                if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"n"))
                {
                    l_dwFullCycles = _wtoi(l_pTmp);
                }
                if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"p"))
                {
                    l_dwInsRemCyclesBeforeTest = _wtoi(l_pTmp);
                }
                
                if (l_CArgv.OptionExist(L"u"))
                {
                    l_bUSBSwitchBox = TRUE;

                    if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"w"))
                    {
                        l_dwMillisBeforeReaderAction = _wtoi(l_pTmp);
                    }
                    if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"x"))
                    {
                        l_dwMaxMillisSurpriseRemoval = _wtoi(l_pTmp);
                    }
                    if (l_CArgv.OptionExist(L"y"))
                    {
                        l_bUseRandomSurpriseRemovalTime = FALSE;
                    }
                }
               
                ReaderSurpriseRemovalTest(l_dwFullCycles, 
                                          l_dwInsRemCyclesBeforeTest,
                                          l_bUSBSwitchBox,
                                          l_dwMillisBeforeReaderAction,
                                          l_dwMaxMillisSurpriseRemoval,
                                          l_bUseRandomSurpriseRemovalTime);
                break;
            }

            case TEST_ID_REMOVEREADERWHENBLOCKED:
            {
                DWORD l_dwFullCycles               = 5;  // By default, do five cycles.
                BOOL  l_bUSBSwitchBox              = FALSE;
                DWORD l_dwMillisBeforeReaderAction = 3000; // By default, 3 s.

                if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"n"))
                {
                    l_dwFullCycles = _wtoi(l_pTmp);
                }
                if (l_CArgv.OptionExist(L"u"))
                {
                    l_bUSBSwitchBox = TRUE;

                    if (const WCHAR* const l_pTmp = l_CArgv.ParameterExist(L"w"))
                    {
                        l_dwMillisBeforeReaderAction = _wtoi(l_pTmp);
                    }
                }

                RemoveReaderWhenBlocked(l_dwFullCycles,
                                        l_bUSBSwitchBox,
                                        l_dwMillisBeforeReaderAction);
                break;
            }

            case TEST_ID_INSERTREMOVEDURINGSUSPEND:
            {
                ULONG l_uWaitTime = 0;

                if (const WCHAR* const l_pchWaitTime = l_CArgv.ParameterExist(L"w"))
                {
                    // How long to wait before suspend.
                    l_uWaitTime = _wtoi(l_pchWaitTime);
                }

                SuspendResumeInsertRemoveTest(l_uWaitTime);
                break;
            }

            case TEST_ID_MULTIREADER_AUTOIFD:
            {
                BOOL l_bSimultaneousTest = FALSE;

                if (l_CArgv.OptionExist(L"s"))
                {
                    l_bSimultaneousTest = TRUE;
                }

                MultiReaderAutoIFDTest(l_bSimultaneousTest);
                break;
            }
                
            default:
                return TPR_NOT_HANDLED;
        } // switch


        if(ReaderFailed())
        {
            LogMessage(LOG_CONSOLE, IDS_READER_FAILED_TEST);
            l_ucRet = TPR_FAIL;
        }
        else
        {
            LogMessage(LOG_CONSOLE, IDS_READER_PASSED_TEST);
            l_ucRet = TPR_PASS;
        }


        if (l_bReaderRequired)
        {
            l_CReader.ColdResetCard();
            l_CReader.Close();
        }

    return l_ucRet;
}

#ifdef UNDER_CE
void ParseCommandLine(__inout LPTSTR pCmdLine, PTCHAR argv[], UCHAR *argc)
{
    PTCHAR pch;
    
    for (*argc = 1, pch = pCmdLine; *argc < MAX_PARAMS && *pch; (*argc)++)
    {
        // skip leading blanks
        while (*pch == ' ' || *pch == '\t')
        {
            pch++;
        }
        // Deal with params inside quotes
        if (*pch == '\"')
        {
            pch++;
            if (*pch)
            {
                argv[*argc]=pch;
                while (*pch!=0 && *pch!='\"')
                {
                    pch++;
                }
                if (*pch=='\"')
                {
                    *pch++=0;
                }
                else
                {
                    (*argc)++;
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else if (*pch)
        {
            argv[*argc] = pch;
            // null-terminate parameter
            while (*pch != ' ' && *pch != '\t' && *pch != 0)
            {
                pch++;
            }
            if (*pch)
            {
                *pch++ = 0;
            }
            else
            {
                (*argc)++;
                break;
            }
        }
        else
        {
            break;
        }
    }
}


extern SPS_SHELL_INFO *g_pShellInfo; // Defined in tuxmain.cpp

TESTPROCAPI TestDispatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
   // Check our message value to see why we have been called.
   if (uMsg == TPM_QUERY_THREAD_COUNT) {
       ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = lpFTE->dwUserData;
       return SPR_HANDLED;
   } else if (uMsg != TPM_EXECUTE) {
       return TPR_NOT_HANDLED;
   }

   static BOOLEAN bInitialized = FALSE;
   static TCHAR   szEditCmdLine[MAX_PATH];
   static UCHAR   argc = 0;
   static PTCHAR  argv[MAX_PARAMS];        // ProgName + max 15 parameters
   
   
   if (!bInitialized) {
       g_hInst = g_pShellInfo->hLib;

       // Make an editable version of the commandline parameters for parsing.
       StringCchCopy(szEditCmdLine, MAX_PATH, g_pShellInfo->szDllCmdLine);
       ParseCommandLine(szEditCmdLine, argv, &argc);
       
       bInitialized = TRUE;
   }

   // Put the testID in argv[0].
   CString l_sTmp = TestLoadStringResource(IDS_PROGRAM_NAME);
   argv[0] = l_sTmp.GetBuffer(0);

   return main(argc, argv, lpFTE->dwUniqueID);
}

#endif