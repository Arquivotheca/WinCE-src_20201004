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
#include "USBReaderSwitchBox.h"
#include "ifdtest.h"
#include "pwrtstutil.h"
#include <bcrypt.h>

extern CSysPwrStates* g_pCSysPwr;


//***************************************************************************
// Purpose:    Input structure for ConnectDisconnectReaderThread.
//***************************************************************************
struct tagReadersSwitchBox
{
    // Reference to USB Switch Box
    USBReaderSwitchBox & m_CUSBbox;

    // Configuration for each port
    BYTE                 m_bPortA; // [1..4]
    BYTE                 m_bPortB; // [1..4]
    BYTE                 m_bPortC; // [1..4]
    BYTE                 m_bPortD; // [1..4]
    
    // Number of ms for thread to sleep before connect/disconnect
    DWORD                m_dwMillisBeforeAction;

    tagReadersSwitchBox(
        USBReaderSwitchBox& usbBox,
        BYTE portA,
        BYTE portB,
        BYTE portC,
        BYTE portD,
        DWORD msBeforeAction)
        :
        m_CUSBbox(usbBox),
        m_bPortA(portA),
        m_bPortB(portB),
        m_bPortC(portC),
        m_bPortD(portD),
        m_dwMillisBeforeAction(msBeforeAction)
        {
        }

        tagReadersSwitchBox& operator=(const tagReadersSwitchBox& rhs)
        {
            if (this != &rhs)
            {
                m_CUSBbox = rhs.m_CUSBbox;
                m_bPortA  = rhs.m_bPortA;
                m_bPortB  = rhs.m_bPortB;
                m_bPortC  = rhs.m_bPortC;
                m_bPortD  = rhs.m_bPortD;
                m_dwMillisBeforeAction = rhs.m_dwMillisBeforeAction;
            }

            return *this;
        }
};

 typedef tagReadersSwitchBox S_READERSWITCHBOX;

//***************************************************************************
// Purpose:    Thread that connects/disconnects reader to/from CE Device
//
// Arguments:
//   LPVOID in_s  - S_READERSWITCHBOX struct containing input parameters.
// 
// Returns:
//   Result of USBReaderSwitchBox::Configure.
//***************************************************************************
DWORD WINAPI
ConnectDisconnectReaderThread(__in LPVOID in_ps)
{
    S_READERSWITCHBOX * in_psSwitchBox = (S_READERSWITCHBOX *) in_ps;

    // Delay some time before performing dis/connect.
    Sleep(in_psSwitchBox->m_dwMillisBeforeAction);

    return in_psSwitchBox->m_CUSBbox.Configure(in_psSwitchBox->m_bPortA, 
                                               in_psSwitchBox->m_bPortB,
                                               in_psSwitchBox->m_bPortC,
                                               in_psSwitchBox->m_bPortD);
}

//***************************************************************************
// Purpose:    Cause removal/insertion of card reader, and wait for removal/
//             insertion to be visible by system via CreateFile().
//
// Arguments:
//   BOOL  in_bConnect                   - Desired connect (TRUE)/
//                                         disconnect(FALSE) operation.
//   DWORD in_dwMillisBeforeReaderAction - Milliseconds to sleep before thread
//                                         inserts/removes reader.  (Only valid
//                                         with in_bUseReaderSwitchBox==TRUE).
//   DWORD in_dwVerifyAttempts           - Number of attempts to check if
//                                         connect/disconnect succeeded.
//   DWORD in_dwMillisBetweenVerifyAttempts - Number of milliseconds between
//                                            verification attempts.
//   BOOL in_bUseReaderSwitchBox - TRUE if a USB Reader Switch Box is being 
//                                 used.  Otherwise, this test is manual.
//   S_READERSWITCHBOX & in_sRSB - Structure containing reference to
//                                 USB Switch Box.
//
// Returns:
//   The name of the device that was connected.  Return value is only valid
//   when in_bConnect == TRUE.
//***************************************************************************
CString WaitForReaderConnectDisconnect(
        const BOOL          in_bConnect,
        const DWORD         in_dwMillisBeforeAction,
        const DWORD         in_dwVerifyAttempts,
        const DWORD         in_dwMillisBetweenVerifyAttempts,
        const BOOL          in_bUseUSBReaderSwitchBox,
        S_READERSWITCHBOX & in_sRSB)
{
    HANDLE l_hThread      = 0;
    CString l_CDeviceName;
        
    if (in_bUseUSBReaderSwitchBox)
    {
        // Spawn a thread to get switch box to perform desired connect/removal.
        in_sRSB.m_bPortA               = (in_bConnect) ? 1 : 0;
        in_sRSB.m_dwMillisBeforeAction = in_dwMillisBeforeAction;
        l_hThread = CreateThread(
                        NULL, 
                        0, 
                        (LPTHREAD_START_ROUTINE) ConnectDisconnectReaderThread,
                        &in_sRSB,
                        0,
                        NULL);

        if (!l_hThread)
        {
            TestCheck(NULL != l_hThread, 
                      IDS_CANNOT_CREATE_SWITCHBOX_THREAD,
                      GetLastError());

            return l_CDeviceName;  // Empty at this point.
        }
    }

    DWORD l_dwRetryCount  = 0;
    wprintf(L"\n");
    // This loop verifies that the desired action has been performed.
    do 
    {
        CReaderList l_CReaderList;
        l_CDeviceName = SelectReader();
        
        if (    (in_bConnect && l_CDeviceName != L"")
            || (!in_bConnect && l_CDeviceName == L""))
        {
            break;
        }
        else
        {
            LogMessage(LOG_DETAIL,
                       L"%2ld ", 
                       in_dwVerifyAttempts - l_dwRetryCount -1);
            wprintf(L"\r%2ld  ",
                    in_dwVerifyAttempts - l_dwRetryCount -1);

            Sleep(in_dwMillisBetweenVerifyAttempts);
        }
    } while (++l_dwRetryCount < in_dwVerifyAttempts);


    // See if we timed out.
    DWORD dwErrorID = (in_bConnect) ? IDS_NO_READER_FOUND 
                                    : IDS_READER_STILL_FOUND;
    TestCheck(in_dwVerifyAttempts != l_dwRetryCount, dwErrorID);
    

    if (in_bUseUSBReaderSwitchBox)
    {
        // Ensure that thread terminated properly
        DWORD l_dwReturn = 0;
        TestCheck(
            WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                GetExitCodeThread(l_hThread, &l_dwReturn) &&
                ERROR_SUCCESS==l_dwReturn,
            IDS_SWITCHBOX_THREAD_FAILED
        );
        CloseHandle(l_hThread);
    }

    return l_CDeviceName;
}

//***************************************************************************
// See header file for description
//***************************************************************************
void ReaderInsertionRemovalTest(const DWORD in_dwFullCycles,
                                const DWORD in_dwInsRemBeforeTest,
                                const BOOL  in_bAlternateCardInserted,
                                const BOOL  in_bUseReaderSwitchBox,
                                const DWORD in_dwMillisBeforeReaderAction) 
{
    const DWORD MAX_RETRIES           = 10;  // Also ~equals timeout seconds.
    DWORD l_dwFullCycleCount          = 0;
    BOOL  l_bReqCardRemovalAfterTest  = FALSE;
    const BOOL l_bStress              = (0 == in_dwFullCycles) ? TRUE : FALSE;
    DWORD l_dwCardFailures            = 0;

    // For now, let's assume we only have one reader.
    CReader l_CReader;
    CString l_CDeviceName;

    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_TEST_INSERTREMOVE);
    LogMessage(LOG_CONSOLE, L"=============================");


    USBReaderSwitchBox l_USBbox;
    S_READERSWITCHBOX l_sRSB(l_USBbox, 0,0,0,0, CLOCKS_PER_SEC);  
    if (in_bUseReaderSwitchBox)
    {
        if (!l_USBbox.Initialize(1))
        {
            LogMessage(LOG_CONSOLE, IDS_CANNOT_INITIALIZE_SWITCHBOX);
        }
    }


    // Main test loop
    while(TRUE)
    {
        LogMessage(LOG_CONSOLE, IDS_TEST_START_ITERATIONNUM, l_dwFullCycleCount++);

        if (l_bStress)
        {
            LogMessage(LOG_CONSOLE, IDS_STRESS_FAILURE_COUNT, l_dwCardFailures);
        }

        // Do the required number of insertions/removals before sanity check.
        for (DWORD i=0; i<in_dwInsRemBeforeTest; i++)
        {
            //-------------------------------------
            // PART 1: Remove any inserted readers
            //-------------------------------------
            TestStart(IDS_TEST_REMOVEREADERS);

            WaitForReaderConnectDisconnect(
                FALSE,                         // in_bConnect
                in_dwMillisBeforeReaderAction, // in_dwMillisBeforeAction,
                MAX_RETRIES,                   // in_dwVerifyAttempts
                CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
                in_bUseReaderSwitchBox,        // in_bUseUSBReaderSwitchBox,
                l_sRSB                         // in_sRSB
            );

            TestEnd();
            if (TestFailed())
            {
                break;
            }
        
        
            //-------------------------------------
            // PART 2: Insert reader
            //-------------------------------------
            TestStart(IDS_TEST_INSERTREADER);

            l_CDeviceName = WaitForReaderConnectDisconnect(
                TRUE,                          // in_bConnect
                in_dwMillisBeforeReaderAction, // in_dwMillisBeforeAction,
                MAX_RETRIES,                   // in_dwVerifyAttempts
                CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
                in_bUseReaderSwitchBox,        // in_bUseUSBReaderSwitchBox,
                l_sRSB                         // in_sRSB
            );
            
            TestEnd();
            if (TestFailed())
            {
                break;
            }
        } // for (DWORD i=0; i<in_dwInsRemBeforeTest; i++)

        // Check to see if we've already failed at this point.
        if (ReaderFailed())
        {
            break;
        }


        //-------------------------------------
        // Part 3: Open reader and run sanity check.
        //-------------------------------------
        TestStart(IDS_TEST_OPENREADER);
        BOOL l_bRet = l_CReader.Open(l_CDeviceName);
        TestCheck(TRUE == l_bRet,
                 IDS_CAN_NOT_OPEN_SMART_CARD_READER);
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        // Run AutoIFD for one card.
        LogMessage(LOG_CONSOLE, L"\n");
        CardProviderTest(l_CReader, TRUE, 0, l_bReqCardRemovalAfterTest, l_bStress);
        if (ReaderFailed())
        {
            if (l_bStress)
            {
                l_dwCardFailures++;
                ResetReaderFailedFlag(); 
            }
            else
            {
                break;
            }
        }


        //-------------------------------------
        // Part 4: Close reader.
        //-------------------------------------
        TestStart(IDS_TEST_CLOSEREADER);
        TestCheck(l_CReader.Close() != 0,
                 IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        // Toggle between having card inserted/uninserted during reader
        // removal/insertion.  Idea is to test if reader behaves differently
        // if it's inserted with a card vs. empty.
        if (in_bAlternateCardInserted)
        {
            l_bReqCardRemovalAfterTest = !l_bReqCardRemovalAfterTest;
        }

        // Terminate?
        if (ReaderFailed() ||
            (!l_bStress && l_dwFullCycleCount >= in_dwFullCycles))
        {
            break;
        }
    } // while(TRUE)


    if (in_bUseReaderSwitchBox)
    {
        l_USBbox.Close();
    }
}


//***************************************************************************
// See header file for description
//***************************************************************************
void ReaderSurpriseRemovalTest(const DWORD in_dwFullCycles,
                               const DWORD in_dwInsRemBeforeTest,
                               const BOOL  in_bUseReaderSwitchBox,
                               const DWORD in_dwMillisBeforeReaderAction,
                               const DWORD in_dwMaxMillisSurpriseRemoval,
                               const BOOL  in_bRandomSurpriseRemovalTime) 
{
    const DWORD MAX_RETRIES          = 10;  // Also ~equals seconds until timeout.
    DWORD l_dwFullCycleCount         = 0;
    DWORD l_dwReturn                 = 0;
    const BOOL l_bStress             = (0 == in_dwFullCycles) ? TRUE : FALSE;
    DWORD l_dwCardFailures           = 0;
    const BOOL l_bReqCardRemovalAfterTest = FALSE;

    // Let's assume we only have one reader.
    CReader l_CReader;
    CString l_CDeviceName;

    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_TEST_INTERRUPTREADER);
    LogMessage(LOG_CONSOLE, L"=============================");


    USBReaderSwitchBox l_USBbox;
    S_READERSWITCHBOX l_sRSB(l_USBbox, 0,0,0,0, CLOCKS_PER_SEC);
    if (in_bUseReaderSwitchBox)
    {
        if (!l_USBbox.Initialize(1))
        {
            LogMessage(LOG_DETAIL, L"Problems initializing USB Box.");
        }
    }


    while(TRUE)
    {
        LogMessage(LOG_CONSOLE, 
                   IDS_TEST_START_ITERATIONNUM, 
                   l_dwFullCycleCount++);

        if (l_bStress)
        {
            LogMessage(LOG_CONSOLE, IDS_STRESS_FAILURE_COUNT, l_dwCardFailures);
        }

        // Do the required number of insertions/removals before sanity check.
        for (DWORD i=0; i<in_dwInsRemBeforeTest; i++)
        {
            //-------------------------------------
            // PART 1: Remove any inserted readers
            //-------------------------------------
            TestStart(IDS_TEST_REMOVEREADERS);

            WaitForReaderConnectDisconnect(
                FALSE,                         // in_bConnect
                in_dwMillisBeforeReaderAction, // in_dwMillisBeforeAction,
                MAX_RETRIES,                   // in_dwVerifyAttempts
                CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
                in_bUseReaderSwitchBox,        // in_bUseUSBReaderSwitchBox,
                l_sRSB                         // in_sRSB
            );

            TestEnd();
            if (TestFailed())
            {
                break;
            }
        

            //-------------------------------------
            // PART 2: Insert reader
            //-------------------------------------
            TestStart(IDS_TEST_INSERTREADER);
            
            l_CDeviceName = WaitForReaderConnectDisconnect(
                TRUE,                          // in_bConnect
                in_dwMillisBeforeReaderAction, // in_dwMillisBeforeAction,
                MAX_RETRIES,                   // in_dwVerifyAttempts
                CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
                in_bUseReaderSwitchBox,        // in_bUseUSBReaderSwitchBox,
                l_sRSB                         // in_sRSB
            );
            
            TestEnd();
            if (TestFailed())
            {
                break;
            }
        } // for (DWORD i=0; i<in_dwInsRemBeforeTest; i++)

        // Check to see if we've already failed at this point.
        if (ReaderFailed())
        {
            break;
        }


        //-------------------------------------
        // Part 3: Open reader and start a test to be interrupted by removal
        //         of the card reader.
        //         If a USB switch box is available, spawn a thread to remove
        //         the reader during the test.
        //-------------------------------------
        TestStart(IDS_TEST_OPENREADER);
        BOOL l_bRet = l_CReader.Open(l_CDeviceName);
        TestCheck(TRUE == l_bRet,
                 IDS_CAN_NOT_OPEN_SMART_CARD_READER);
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        // Either spawn a thread or print instructions to the output.
        HANDLE l_hThread = 0;
        if (in_bUseReaderSwitchBox)
        {
            DWORD l_dwMillisBeforeSurpriseRemoval = in_dwMaxMillisSurpriseRemoval;

            if (in_bRandomSurpriseRemovalTime)
            {
                // Generate a random number between [0, in_dwMaxMillisSurpriseRemoval].
                BYTE byteRand = (BYTE) (GetTickCount() & 0xFF);
                BCryptGenRandom(NULL, (PUCHAR) &byteRand, sizeof(BYTE), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                l_dwMillisBeforeSurpriseRemoval = (l_dwMillisBeforeSurpriseRemoval >> 8) 
                                                  * byteRand;
            }

            // Spawn a thread to get USB switch box to remove the reader.
            l_sRSB.m_bPortA               = 0;  // Disconnect
            l_sRSB.m_dwMillisBeforeAction = l_dwMillisBeforeSurpriseRemoval;
            LogMessage(LOG_CONSOLE, 
                       IDS_READER_INTERRUPT_TIME,
                       l_dwMillisBeforeSurpriseRemoval);
            l_hThread = CreateThread(NULL, 
                                     0, 
                                     (LPTHREAD_START_ROUTINE) ConnectDisconnectReaderThread,
                                     &l_sRSB,
                                     0,
                                     NULL);

            if (!l_hThread)
            {
                TestCheck(NULL != l_hThread, 
                          IDS_CANNOT_CREATE_SWITCHBOX_THREAD,
                          GetLastError());
                break;
            }
        }
        else
        {
            // Tell the user to remove the reader during the CardProvider test.
            LogMessage(LOG_CONSOLE, IDS_TEST_REMOVEREADER_DURING_TEST);
            Sleep(5000); // Enough time for message to be read.
        }

        // Run AutoIFD for one card.  This will be interrupted.  We don't
        // care about the actual test result here, so reset the failed flag.
        LogMessage(LOG_CONSOLE, L"\n");
        CardProviderTest(l_CReader, TRUE, 0, l_bReqCardRemovalAfterTest, l_bStress);
        if (!ReaderFailed())
        {
            TestCheck(ReaderFailed() == TRUE,
                      IDS_AUTOIFD_PASS_UNEXPECTED);
        }
        else
        {
            ResetReaderFailedFlag(); 
        }
        

        // Close the handle of the reader that detached during the test.
        TestCheck(l_CReader.Close() != 0,
                  IDS_CAN_NOT_CLOSE_SMART_CARD_READER);


        if (in_bUseReaderSwitchBox)
        {
            // Ensure that thread terminated properly
            TestCheck(
                WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                    GetExitCodeThread(l_hThread, &l_dwReturn) &&
                    ERROR_SUCCESS==l_dwReturn,
                IDS_SWITCHBOX_THREAD_FAILED
            );
            CloseHandle(l_hThread);
        }

        if (ReaderFailed())
        {
            break;
        }


        //-------------------------------------
        // PART 4: Insert reader
        //-------------------------------------
        LogMessage(LOG_CONSOLE, L"\n");
        TestStart(IDS_TEST_INSERTREADER);
        
        l_CDeviceName = WaitForReaderConnectDisconnect(
                TRUE,                          // in_bConnect
                in_dwMillisBeforeReaderAction, // in_dwMillisBeforeAction,
                MAX_RETRIES,                   // in_dwVerifyAttempts
                CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
                in_bUseReaderSwitchBox,        // in_bUseUSBReaderSwitchBox,
                l_sRSB                         // in_sRSB
            );
        
        TestEnd();
        if (TestFailed())
        {
            break;
        }


        //-------------------------------------
        // Part 5: Open reader and run sanity check.
        //-------------------------------------
        TestStart(IDS_TEST_OPENREADER);
        l_bRet = l_CReader.Open(l_CDeviceName);
        TestCheck(TRUE == l_bRet,
                 IDS_CAN_NOT_OPEN_SMART_CARD_READER);
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        // Run AutoIFD for one card.
        LogMessage(LOG_CONSOLE, L"\n");
        CardProviderTest(l_CReader, TRUE, 0, l_bReqCardRemovalAfterTest, l_bStress);
        if (ReaderFailed())
        {
            if (l_bStress)
            {
                l_dwCardFailures++;
                ResetReaderFailedFlag(); 
            }
            else
            {
                break;
            }
        }


        //-------------------------------------
        // Part 6: Close reader.
        //-------------------------------------
        TestStart(IDS_TEST_CLOSEREADER);
        TestCheck(l_CReader.Close() != 0,
                 IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        // Terminate?
        if (ReaderFailed() || 
            (!l_bStress && l_dwFullCycleCount >= in_dwFullCycles))
        {
            break;
        }
    } // while(TRUE)


    if (in_bUseReaderSwitchBox)
    {
        l_USBbox.Close();
    }
}


//***************************************************************************
// See header file for description
//***************************************************************************
void RemoveReaderWhenBlocked(const DWORD in_dwFullCycles, 
                             const BOOL in_bUseReaderSwitchBox,
                             const DWORD in_dwMillisBeforeReaderAction)
{
    const DWORD MAX_RETRIES   = 10;  // Also ~equals seconds until timeout.
    DWORD l_dwFullCycleCount  = 0;
    DWORD l_dwReturn          = 0;
    LONG  l_lResult           = 0;
    BOOL  l_bCardInDuringTest = TRUE;


    CReader l_CReader;
    CString l_CDeviceName;

    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_TEST_READERREMOVALWHENBLOCKED);
    LogMessage(LOG_CONSOLE, L"=============================");


    USBReaderSwitchBox l_USBbox;
    S_READERSWITCHBOX l_sRSB(l_USBbox, 0,0,0,0, CLOCKS_PER_SEC);
    if (in_bUseReaderSwitchBox)
    {
        if (!l_USBbox.Initialize(1))
        {
            LogMessage(LOG_DETAIL, L"Problems initializing USB Box.");
        }
    }


    while(TRUE)
    {
        LogMessage(LOG_CONSOLE, 
                   IDS_TEST_START_ITERATIONNUM, 
                   l_dwFullCycleCount++);
       
        
        //-------------------------------------
        // PART 1: Insert a reader
        //-------------------------------------
        TestStart(IDS_TEST_INSERTREADER);
        l_CDeviceName = WaitForReaderConnectDisconnect(
            TRUE,                          // in_bConnect
            in_dwMillisBeforeReaderAction, // in_dwMillisBeforeAction,
            MAX_RETRIES,                   // in_dwVerifyAttempts
            CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
            in_bUseReaderSwitchBox,        // in_bUseUSBReaderSwitchBox,
            l_sRSB                         // in_sRSB
            );
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        //-------------------------------------
        // PART 2: Open reader
        //-------------------------------------
        TestStart(IDS_TEST_OPENREADER);
        TestCheck(l_CReader.Open(l_CDeviceName) == TRUE,
                  IDS_CAN_NOT_OPEN_SMART_CARD_READER);
        TestEnd();
        if (TestFailed())
        {
            break;
        }

        //-------------------------------------
        // PART 3: Alternate whether or not card
        //         is inserted during the test.
        //-------------------------------------
        l_bCardInDuringTest = !l_bCardInDuringTest;
        if (l_bCardInDuringTest)
        {
            TestStart(IDS_INSERT_CARD);
            l_lResult = l_CReader.WaitForCardInsertion();
            TEST_CHECK_SUCCESS(
                TestLoadStringResource(IDS_WAIT_FOR_INSERT_FAILED).GetBuffer(0),
                l_lResult
                );
            TestEnd();
        }
        else
        {
            TestStart(IDS_REM_SMART_CARD);
            l_lResult = l_CReader.WaitForCardRemoval();
            TEST_CHECK_SUCCESS(
                TestLoadStringResource(IDS_WAIT_FOR_REMOVE_FAILED).GetBuffer(0),
                l_lResult
                );
            TestEnd();
        }
        
        //-------------------------------------
        // PART 4: Now, with the card in the right
        //         state, remove the reader.
        //-------------------------------------
        TestStart(IDS_TEST_REMOVEREADERS);

        // Spawn a thread 
        HANDLE l_hThread = 0;
        if (in_bUseReaderSwitchBox)
        {
            // Spawn a thread to get USB switch box to remove the reader.
            l_sRSB.m_bPortA               = 0;  // Disconnect
            l_sRSB.m_dwMillisBeforeAction = in_dwMillisBeforeReaderAction;
            l_hThread = CreateThread(NULL, 
                                     0, 
                                     (LPTHREAD_START_ROUTINE) ConnectDisconnectReaderThread,
                                     &l_sRSB,
                                     0,
                                     NULL);

            if (!l_hThread)
            {
                TestCheck(NULL != l_hThread, 
                          IDS_CANNOT_CREATE_SWITCHBOX_THREAD,
                          GetLastError());
                break;
            }
        }

        // The following calls will block.
        if (l_bCardInDuringTest)
        {
            l_lResult = l_CReader.WaitForCardRemoval();
            TestCheck(STATUS_CANCELLED==MapWinErrorToNtStatus(l_lResult),
                      IDS_WAIT_FOR_ABSENT_SUCCEED_UNEXPECTED); 
        }
        else
        {
            l_lResult = l_CReader.WaitForCardInsertion();
            TestCheck(STATUS_CANCELLED==MapWinErrorToNtStatus(l_lResult),
                      IDS_WAIT_FOR_PRESENT_SUCCEED_UNEXPECTED);
        }

        if (in_bUseReaderSwitchBox)
        {
            // Ensure that thread terminated properly
            TestCheck(
                WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                    GetExitCodeThread(l_hThread, &l_dwReturn) &&
                    ERROR_SUCCESS==l_dwReturn,
                IDS_SWITCHBOX_THREAD_FAILED
            );
            CloseHandle(l_hThread);
        }
        TestEnd();


        //-------------------------------------
        // Part 5: Close reader.
        //-------------------------------------
        TestStart(IDS_TEST_CLOSEREADER);
        TestCheck(l_CReader.Close() != 0,
                 IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
        TestEnd();


        // Terminate?
        if (ReaderFailed() ||
            (0 != in_dwFullCycles && l_dwFullCycleCount >= in_dwFullCycles))
        {
            break;
        }
    } // while (TRUE)


    if (in_bUseReaderSwitchBox)
    {
        l_USBbox.Close();
    }
}

//***************************************************************************
// Purpose:   This function simply suspends and resumes the device.
//
// Arguments: 
//   in_uWaitTime - Amount of time before device suspends.
//
// Returns:
//   void - Result is obtained via TestFailed()
//***************************************************************************
void SimpleSuspendResume(ULONG in_uWaitTime)
{
    DWORD  l_dwThId    = 0;
    HANDLE l_hThread   = 0;
    DWORD  l_dwRet     = 0;

    ULONG  l_uDuration = 10;
    if (in_uWaitTime > 0 && in_uWaitTime <= 120)
    {
        l_uDuration = in_uWaitTime;
    }

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
    }

    // Wait for suspend-wakeup thread to complete its operation.
    TestCheck(
        WaitForSingleObject(l_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
            GetExitCodeThread(l_hThread, &l_dwRet) && 
            ERROR_SUCCESS==l_dwRet,
        IDS_SUSPEND_AND_WAKEUP_RETURN_ERROR, 
        l_dwRet
    );
    TestCheck(CloseHandle(l_hThread), IDS_CANNOT_CLOSE_THREAD_HANDLE, GetLastError());
}


//***************************************************************************
// See header file for description
//***************************************************************************
void SuspendResumeInsertRemoveTest(const ULONG in_uWaitTime)
{
    const DWORD MAX_RETRIES   = 10;  // Also ~equals seconds until timeout.
    BOOL   l_bRet             = FALSE;
    LONG   l_lResult          = 0;
    ULONG  l_uState           = 0;
    DWORD  l_dwTestCount      = 0;

    CReader l_CReader;
    CString l_CDeviceName;


    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_TEST_SUSPENDRESUME_INSERTREMOVE);
    LogMessage(LOG_CONSOLE, L"=============================");

    LogMessage(LOG_CONSOLE, IDS_TEST_SUSPENDRESUME_INSERTREMOVE_NOTE);
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

    USBReaderSwitchBox l_sDummyBox;
    S_READERSWITCHBOX l_sDummyRSB(l_sDummyBox, 0,0,0,0, CLOCKS_PER_SEC);  

    //-------------------------------------
    // Part 1a: Make sure there's a reader connected
    //-------------------------------------
    TestStart(IDS_TEST_INSERTREADER);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        TRUE,                          // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 1b: Open reader and see if you can get state (quick sanity check)
    //-------------------------------------
    TestStart(IDS_TEST_OPENREADER);
    l_bRet = l_CReader.Open(l_CDeviceName);
    TestCheck(TRUE == l_bRet,
             IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    l_lResult = l_CReader.GetState(&l_uState);
    TEST_CHECK_SUCCESS(
        TestLoadStringResource(IDS_READER_FAILED_IOCTL_SMARTCARD_GET_STATE).GetBuffer(0),
        l_lResult
        );
    TestCheck(l_CReader.Close(), IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 2: Display instructions and suspend/resume
    //-------------------------------------
    TestStart(IDS_REMOVEINSERT_DURING_SUSPEND, ++l_dwTestCount);
    SimpleSuspendResume(in_uWaitTime);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 3: Make sure there's a reader connected after resume.
    //-------------------------------------
    TestStart(IDS_TEST_INSERTREADER);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        TRUE,                          // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 4: Open reader and run AutoIFD
    //-------------------------------------
    TestStart(IDS_TEST_OPENREADER);
    l_bRet = l_CReader.Open(l_CDeviceName);
    TestCheck(TRUE == l_bRet,
             IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    // Run AutoIFD for one card.
    CardProviderTest(l_CReader, TRUE, 0, FALSE, FALSE);
    if (ReaderFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 5: Close reader.
    //-------------------------------------
    TestStart(IDS_TEST_CLOSEREADER);
    TestCheck(l_CReader.Close() != 0,
             IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 6: Display instructions and suspend/resume
    //-------------------------------------
    TestStart(IDS_REMOVEINSERT_DIFFERENTPORT_DURING_SUSPEND, ++l_dwTestCount);
    SimpleSuspendResume(in_uWaitTime);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 7: Make sure there's a reader connected after resume.
    //-------------------------------------
    TestStart(IDS_TEST_INSERTREADER);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        TRUE,                          // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 8: Open reader and run AutoIFD
    //-------------------------------------
    TestStart(IDS_TEST_OPENREADER);
    l_bRet = l_CReader.Open(l_CDeviceName);
    TestCheck(TRUE == l_bRet,
             IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    // Run AutoIFD for one card.
    CardProviderTest(l_CReader, TRUE, 0, FALSE, FALSE);
    if (ReaderFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 9: Close reader.
    //-------------------------------------
    TestStart(IDS_TEST_CLOSEREADER);
    TestCheck(l_CReader.Close() != 0,
             IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 10: Display instructions and suspend/resume
    //-------------------------------------
    TestStart(IDS_REMOVE_NOINSERT_DURINGSUSPEND_SAMEPORT, ++l_dwTestCount);
    SimpleSuspendResume(in_uWaitTime);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 11: Wait for reader to be connected after resume
    //-------------------------------------
    TestStart(IDS_TEST_INSERTREADER);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        TRUE,                          // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 12: Open reader and run AutoIFD
    //-------------------------------------
    TestStart(IDS_TEST_OPENREADER);
    l_bRet = l_CReader.Open(l_CDeviceName);
    TestCheck(TRUE == l_bRet,
             IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    // Run AutoIFD for one card.
    CardProviderTest(l_CReader, TRUE, 0, FALSE, FALSE);
    if (ReaderFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 13: Close reader.
    //-------------------------------------
    TestStart(IDS_TEST_CLOSEREADER);
    TestCheck(l_CReader.Close() != 0,
             IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 14: Display instructions and suspend/resume
    //-------------------------------------
    TestStart(IDS_REMOVE_NOINSERT_DURINGSUSPEND_DIFFPORT, ++l_dwTestCount);
    SimpleSuspendResume(in_uWaitTime);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 15: Wait for reader to be connected after resume
    //-------------------------------------
    TestStart(IDS_TEST_INSERTREADER);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        TRUE,                          // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 16: Open reader and run AutoIFD
    //-------------------------------------
    TestStart(IDS_TEST_OPENREADER);
    l_bRet = l_CReader.Open(l_CDeviceName);
    TestCheck(TRUE == l_bRet,
             IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    // Run AutoIFD for one card.
    CardProviderTest(l_CReader, TRUE, 0, FALSE, FALSE);
    if (ReaderFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 17: Close reader.
    //-------------------------------------
    TestStart(IDS_TEST_CLOSEREADER);
    TestCheck(l_CReader.Close() != 0,
             IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 18: Remove reader
    //-------------------------------------
    TestStart(IDS_TEST_REMOVEREADERS);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        FALSE,                         // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 19: Display instructions and suspend/resume
    //-------------------------------------
    TestStart(IDS_INSERT_DURING_SUSPEND, ++l_dwTestCount);
    SimpleSuspendResume(in_uWaitTime);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 20: Wait for reader to be connected after resume
    //-------------------------------------
    TestStart(IDS_TEST_INSERTREADER);
    l_CDeviceName = WaitForReaderConnectDisconnect(
        TRUE,                          // in_bConnect
        0,                             // in_dwMillisBeforeAction (unused)
        MAX_RETRIES,                   // in_dwVerifyAttempts
        CLOCKS_PER_SEC,                // in_dwMillisBetweenVerifyAttempts,
        FALSE,                         // in_bUseUSBReaderSwitchBox,
        l_sDummyRSB                    // in_sRSB
    );
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 21: Open reader and run AutoIFD
    //-------------------------------------
    TestStart(IDS_TEST_OPENREADER);
    l_bRet = l_CReader.Open(l_CDeviceName);
    TestCheck(TRUE == l_bRet,
             IDS_CAN_NOT_OPEN_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

    // Run AutoIFD for one card.
    CardProviderTest(l_CReader, TRUE, 0, FALSE, FALSE);
    if (ReaderFailed())
    {
        goto cleanup;
    }

    //-------------------------------------
    // Part 22: Close reader.
    //-------------------------------------
    TestStart(IDS_TEST_CLOSEREADER);
    TestCheck(l_CReader.Close() != 0,
             IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
    TestEnd();
    if (TestFailed())
    {
        goto cleanup;
    }

cleanup:
    // Clean up utility object.
    delete g_pCSysPwr;
    g_pCSysPwr = 0;
}