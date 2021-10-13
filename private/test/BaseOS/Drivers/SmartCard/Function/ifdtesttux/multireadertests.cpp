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
#include "ifdtest.h"


//***************************************************************************
// Purpose:    Input structure for AutoIFDThread.
//             These arguments reflect the arguments of the CardProviderTest
//             function.
//***************************************************************************
typedef struct 
{
    CReader * m_pCReader;
    BOOL      m_bTestOneCard;
    ULONG     m_ulTestNo;
    BOOL      m_bReqCardRemoval;
} S_AUTOIFDTHREAD_INPUT;

//***************************************************************************
// Purpose:    Used to create a linked list of reader context structures
//             for multiple reader testing.
//***************************************************************************
typedef struct _S_READERCONTEXT
{
    S_AUTOIFDTHREAD_INPUT *   m_psAutoIFDIn;
    HANDLE                    m_hThread;
    struct _S_READERCONTEXT * m_pNext;
} S_READERCONTEXT;

//***************************************************************************
// Purpose:    Thread that runs AutoIFD
//
// Arguments:
//   LPVOID in_pV - S_AUTOIFDTHREAD_INPUT struct containing input parameters.
// 
// Returns:
//   Result of USBReaderSwitchBox::Configure.
//***************************************************************************
DWORD WINAPI
AutoIFDThread(__in LPVOID in_pV)
{
    S_AUTOIFDTHREAD_INPUT * in_pS = (S_AUTOIFDTHREAD_INPUT *) in_pV;

    CardProviderTest(*in_pS->m_pCReader,
                     in_pS->m_bTestOneCard,
                     in_pS->m_ulTestNo,
                     in_pS->m_bReqCardRemoval,
                     FALSE);

    return ERROR_SUCCESS;
}

//***************************************************************************
// See header file for description
//***************************************************************************
void MultiReaderAutoIFDTest(BOOL in_bSimultaneousTest)
{
    LogMessage(LOG_CONSOLE, L"=============================");
    LogMessage(LOG_CONSOLE, IDS_TEST_MULTIREADER_AUTOIFD);
    LogMessage(LOG_CONSOLE, L"=============================");

    BOOL l_bRet = TRUE;
    CReaderList l_CReaderList;
    const DWORD l_dwNumReaders = l_CReaderList.GetNumReaders();

    if (!in_bSimultaneousTest)
    {
        CReader l_CReader;

        for (BYTE l_dwIndex = 0; l_dwIndex < l_dwNumReaders; l_dwIndex++)
        {
            //-------------------------------------
            // Part 1: Open reader.
            //-------------------------------------
            LogMessage(LOG_CONSOLE, L"\n");
            TestStart(IDS_TEST_OPENREADER);
            l_bRet = l_CReader.Open(l_CReaderList.GetDeviceName(l_dwIndex));
            TestCheck(TRUE == l_bRet,
                      IDS_CAN_NOT_OPEN_SMART_CARD_READER);
            TestEnd();
            if (TestFailed())
            {
                break;
            }

            // Print some informative information about the reader.
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

            //-------------------------------------
            // Part 2: Run AutoIFD
            //-------------------------------------
            CardProviderTest(l_CReader, TRUE, 0, FALSE, FALSE);
            if (ReaderFailed())
            {
                break;
            }

            //-------------------------------------
            // Part 3: Close reader.
            //-------------------------------------
            TestStart(IDS_TEST_CLOSEREADER);
            TestCheck(l_CReader.Close() != 0,
                     IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
            TestEnd();
            if (TestFailed())
            {
                break;
            }
        } //  for (BYTE l_dwIndex = 0; l_dwIndex < l_dwNumReaders; l_dwIndex++)
    } // if (!in_bSimultaneousTest)
    else
    {
        // NOTE: This simultaneous test does not take care of mutual exclusion
        // issues in the CCardProvider class, and is not intended for anything
        // but a rough check.
        S_READERCONTEXT * l_psRCListHead = 0;  // Points to head of list.
        S_READERCONTEXT * l_psTempRC     = 0;  // Temp pointer.
        
        BOOL              l_bError       = FALSE;
        BYTE              l_dwIndex      = 0; // Counter
        
        LogMessage(LOG_CONSOLE, IDS_TEST_SIMULTANEOUSREADERS);

        //-------------------------------------
        // Part 1: Create linked list of reader structures
        //-------------------------------------
        for (l_dwIndex = 0; l_dwIndex < l_dwNumReaders; l_dwIndex++)
        {
            // Reader HW is not assigned to the reader objects until later.
            CReader * pNewReader = new CReader();
            if (NULL == pNewReader)
            {
                l_bError = TRUE;
            }

            S_AUTOIFDTHREAD_INPUT * pNewThreadInp = (S_AUTOIFDTHREAD_INPUT *) malloc(sizeof(S_AUTOIFDTHREAD_INPUT));
            if (NULL == pNewThreadInp)
            {
                l_bError = TRUE;
            }
            else 
            {
                pNewThreadInp->m_pCReader        = pNewReader;
                pNewThreadInp->m_bTestOneCard    = TRUE;
                pNewThreadInp->m_ulTestNo        = 0;
                pNewThreadInp->m_bReqCardRemoval = FALSE;
            }

            S_READERCONTEXT * pNewRC = (S_READERCONTEXT *) malloc(sizeof(S_READERCONTEXT));
            if (NULL == pNewRC)
            {
                l_bError = TRUE;
            }
            else 
            {
                pNewRC->m_psAutoIFDIn = pNewThreadInp;
                pNewRC->m_hThread     = CreateThread(NULL, 
                                                     0, 
                                                     (LPTHREAD_START_ROUTINE) AutoIFDThread,
                                                     pNewRC->m_psAutoIFDIn,
                                                     CREATE_SUSPENDED,
                                                     NULL);
                if (NULL == pNewRC->m_hThread)
                {
                    l_bError = TRUE;
                }
                pNewRC->m_pNext = l_psRCListHead;
                l_psRCListHead = pNewRC;
            }


            // Cleanup if necessary.
            // Note that if we enter the following block l_psRCListHead has
            // not been modified on this iteration yet.
            if (l_bError)
            {
                if (pNewRC)
                {
                    if (0 != pNewRC->m_hThread)
                    {
                        l_bRet = CloseHandle(pNewRC->m_hThread);
                        TestCheck(TRUE == l_bRet, 
                                  IDS_CANNOT_CLOSE_THREAD_HANDLE,
                                  GetLastError);
                    }

                    free(pNewRC);
                    pNewRC = 0;
                }

                if (pNewThreadInp)
                {
                    free(pNewThreadInp);
                    pNewThreadInp = 0;
                }
                if (pNewReader)
                {
                    delete pNewReader;
                    pNewReader = 0;
                }

                TestCheck(FALSE,
                          IDS_COULD_NOT_ALLOCATE_TEST_STRUCTURES);

                break;
            } // if (l_bError)
        } // for (l_dwIndex = 0; l_dwIndex < l_dwNumReaders; l_dwIndex++)

        //-------------------------------------
        // Part 2: Do the test
        //-------------------------------------
        if (!l_bError)
        {
            //-------------------------------------
            // Part 2a: Open readers.
            //-------------------------------------
            l_psTempRC = l_psRCListHead;  // Temp pointer.
            for (l_dwIndex = 0; l_dwIndex < l_dwNumReaders; l_dwIndex++)
            {
                TestStart(IDS_TEST_OPENREADER);
                l_bRet = l_psTempRC->m_psAutoIFDIn->m_pCReader->Open(l_CReaderList.GetDeviceName(l_dwIndex));
                TestCheck(TRUE == l_bRet,
                          IDS_CAN_NOT_OPEN_SMART_CARD_READER);
                TestEnd();
                if (TestFailed())
                {
                    goto cleanup;
                }

                // Increment through the list.
                l_psTempRC = l_psTempRC->m_pNext;
            } // for (l_dwIndex = 0; l_dwIndex < l_dwNumReaders; l_dwIndex++)

            //-------------------------------------
            // Part 2b: Resume threads.
            //-------------------------------------
            l_dwIndex =0 ;
            l_psTempRC = l_psRCListHead;  // Temp pointer.
            while (l_psTempRC)
            {
                TestStart(IDS_TEST_RESUMETHREAD,
                          l_psTempRC->m_psAutoIFDIn->m_pCReader->GetDeviceName());
                TestCheck(ResumeThread(l_psTempRC->m_hThread)==1,
                          IDS_COULD_NOT_RESUMETHREAD, GetLastError());
                TestEnd();
                if (TestFailed())
                {
                    goto cleanup;
                }

                l_psTempRC = l_psTempRC->m_pNext;
            } // while (l_psTempRC)

            //-------------------------------------
            // Part 2c: Wait for threads to complete.
            //-------------------------------------
            DWORD l_dwReturn;
            l_psTempRC = l_psRCListHead;  // Temp pointer.
            while (l_psTempRC)
            {
                TestCheck(
                    WaitForSingleObject(l_psTempRC->m_hThread, MAX_THREAD_TIMEOUT)==WAIT_OBJECT_0 &&
                        GetExitCodeThread(l_psTempRC->m_hThread, &l_dwReturn) &&
                        ERROR_SUCCESS==l_dwReturn,
                    IDS_SWITCHBOX_THREAD_FAILED);
             
                l_psTempRC = l_psTempRC->m_pNext;
            } // while (l_psTempRC)
            
            //-------------------------------------
            // Part 2d: Close readers.
            //-------------------------------------
            l_psTempRC = l_psRCListHead;  // Temp pointer.
            while (l_psTempRC)
            {
                TestStart(IDS_TEST_CLOSEREADER);
                TestCheck(l_psTempRC->m_psAutoIFDIn->m_pCReader->Close() != 0,
                         IDS_CAN_NOT_CLOSE_SMART_CARD_READER);
                TestEnd();

                l_psTempRC = l_psTempRC->m_pNext;
            } // while (l_psTempRC)
        } // if (!l_bError)


        //-------------------------------------
        // Part 3: Cleanup list of structures.
        //-------------------------------------
cleanup:
        l_psTempRC = 0;  // Temp pointer.
        while (l_psRCListHead)
        {
            l_psTempRC = l_psRCListHead;

            if (l_psTempRC->m_psAutoIFDIn)
            {
                if (l_psTempRC->m_psAutoIFDIn->m_pCReader)
                {
                    delete l_psTempRC->m_psAutoIFDIn->m_pCReader;
                    l_psTempRC->m_psAutoIFDIn->m_pCReader = 0;
                }

                free(l_psTempRC->m_psAutoIFDIn);
                l_psTempRC->m_psAutoIFDIn = 0;
            }

            if (0 != l_psTempRC->m_hThread)
            {
                l_bRet = CloseHandle(l_psTempRC->m_hThread);
                TestCheck(TRUE == l_bRet, 
                          IDS_CANNOT_CLOSE_THREAD_HANDLE,
                          GetLastError);
            }

            // Update the list head.
            l_psRCListHead = l_psTempRC->m_pNext;
            free(l_psTempRC);
            l_psTempRC = 0;
        }
    }
}