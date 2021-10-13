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
/*++

Module Name:

    sicrypt.cpp

Abstract:

    This is a plug-in for the smart card driver test suite.
    This plug-in is smart card dependent

    Infineon SiCrypt card

Revision History :

    Jan 2005 - initial version

--*/
#ifndef UNDER_CE
#include <stdio.h>
#include <string.h>
#include <stdarg.h> 
#include <time.h>

#include <afx.h>
#include <afxtempl.h>
#else
#include "afxutil.h"
#endif

#include <winioctl.h>
#include <winsmcrd.h>

#include "ifdtest.h"
#include "ifdguid.h"
#include "sicryptrc.h"


void MyCardEntry(class CCardProvider& in_CCardProvider);

//
// Create a card provider object
// Note: all global varibales and all functions have to be static
//
static class CCardProvider MyCard(MyCardEntry);
                                                  
                                          
/*++

Routine Description:
    
    This function will be called after the card has been correctly 
    identified. We should here set the protocol that we need
    for further transmissions

Arguments:

    in_CCardProvider - ref. to our card provider object
    in_CReader - ref. to the reader object

Return Value:

    IFDSTATUS_FAILED - we were unable to set the protocol correctly
    IFDSTATUS_SUCCESS - protocol set correctly
--*/

static ULONG
MyCardSetProtocol(
    class CCardProvider& in_CCardProvider,
    class CReader& in_CReader
    )
{
    ULONG l_lResult;
    g_uiFriendlyTest1 = 4;
    g_uiFriendlyTest2 = 0;
    g_uiFriendlyTest3 = 0;

    g_uiFriendlyTest3++;
    TestStart(IDS_SICRYPT_SETINCORRECTPROTOCOLT0);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);

    // The test MUST fail with the incorrect protocol
    TEST_CHECK_NOT_SUPPORTED(TestLoadStringResource(IDS_SICRYPT_SETPROTOCOLFAILED).GetBuffer(0), l_lResult);
    TEST_END();

    // Now set the correct protocol
    g_uiFriendlyTest3++;
    TestStart(IDS_SICRYPT_SETPROTOCOLT1);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);
    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_SICRYPT_SETPROTOCOLT1).GetBuffer(0), l_lResult);
    TEST_END();

    if (l_lResult != ERROR_SUCCESS) {

        return IFDSTATUS_FAILED;
    }

    return IFDSTATUS_SUCCESS;
}

/*++

Routine Description:
        
    This serves as the test function for a particular smart card

Arguments:

    in_CReader - ref. to class that provides all information for the test

Return Value:

    IFDSTATUS value

--*/
static ULONG
MyCardTest(
    class CCardProvider& in_CCardProvider,
    class CReader& in_CReader
    )
{
    CString        l_CRcsID;            // String to read from the rc file.
    ULONG l_lResult;
    ULONG l_uResultLength, l_uExpectedLength, l_uIndex;
    PUCHAR l_pchResult;
    UCHAR l_rgchBuffer[512], l_rgchBuffer2[512];
    ULONG l_uNumBytes = 0; 
    g_uiFriendlyTest1 = 4;
    g_uiFriendlyTest2 = in_CCardProvider.GetTestNo();
    g_uiFriendlyTest3 = 0;

    switch (in_CCardProvider.GetTestNo()) {

    case 1: {

                // Initialize the card for the test - instead of organizing these operations as individual tests,
                // I have lumped them together into the card initization task, and I use TestCheck after 
                // individual operations to abort the test if initialization fails.  The failing test line number is
                // set to the TestCheck line and the test ends in failure.
                //
                // The alternative approach would be to set the line to the TestEnd, but set the text to the
                // description of the failing suboperation.
                //
                // These cleanup operations should only be necessary if the test failed and exited with the card
                // in an "uncleaned" state.  This is why these operations are not handled as detailed operations.
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_INITCARD);

                // select card root

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x00)) goto INITFAILED;
                 
                // authenticate for file creation and deletion using the admin PIN "12345678"
                
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\x20\x00\x02\x08\x31\x32\x33\x34\x35\x36\x37\x38",
                    13,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 
                    0x90, 0x00,
                    NULL, NULL, 0
                    );

                if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x00)) goto INITFAILED;

                // 
                // clean up possible leftovers from failed runs
                //
                // Select and delete 5555.5656.5757 if it exists
                //

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x06\x55\x55\x56\x56\x57\x57",
                    11,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                if (l_pchResult[0] == 0x90)
                {
                    // delete currently selected
                
                    l_lResult = in_CReader.Transmit(
                        (PUCHAR) "\x90\xe4\x00\x00",
                        4,
                        &l_pchResult,
                        &l_uResultLength
                        );
                        
                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                       
                    if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x00)) goto INITFAILED;
                }

                // Delete directory 5555.5656 if it exists

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x04\x55\x55\x56\x56",
                    9,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                if (l_pchResult[0] == 0x90)
                {
                    // delete currently selected
                
                    l_lResult = in_CReader.Transmit(
                        (PUCHAR) "\x90\xe4\x00\x00",
                        4,
                        &l_pchResult,
                        &l_uResultLength
                        );
                        
                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                       
                    if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x00)) goto INITFAILED;
                }

                // Delete directory 5555 if it exists

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x02\x55\x55",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                if (l_pchResult[0] == 0x90)
                {
                    l_lResult = in_CReader.Transmit(
                        (PUCHAR) "\x90\xe4\x00\x00",
                        4,
                        &l_pchResult,
                        &l_uResultLength
                        );
                        
                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                       
                    if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x00)) goto INITFAILED;
                }

                // Delete file 0007 if it exists

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x02\x00\x07",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                if (l_pchResult[0] == 0x90)
                {
                    l_lResult = in_CReader.Transmit(
                        (PUCHAR) "\x90\xe4\x00\x00",
                        4,
                        &l_pchResult,
                        &l_uResultLength
                        );
                        
                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                       
                    if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x00)) goto INITFAILED;
                }

                // 
                // cleanup finished, test begins in earnest
                // still auth'd using admin key - create test file 0007 that will be used for read/write tests
                //

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x90\xe0\x00\x00\x20"
                             "\x41\x41\x00\x07\x00\x00\x01\x86\x0c\xe4"
                             "\x01\x00\xd6\x01\x00\xda\x01\x00\xb0\x01"
                             "\x00\xd1\x02\x00\x00\xd2\x01\x02\xd0\x02"
                             "\x30\x37",
                    37,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );
                    
    INITFAILED:
        
                TEST_END();

                break;
                
            }
        case 2: {
                //
                // select a file 0007 and write data pattern 0 to N-1 to the card. 
                // Then read the data back and verify correctness. 
                // Check IFSC and IFSD above card limits
                //

                //
                // Select a file
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTFILE7);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x02\x00\x07",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                );
                    
                TEST_END();

                if (TestFailed()) {
                    
                    return IFDSTATUS_FAILED;
                }

                //
                // Do a couple of writes and reads up to maximum size
                // Check behaviour above IFSC and IFSD Limits
                //

                //
                // Generate a 'test' pattern which will be written to the card
                //
                for (l_uIndex = 0; l_uIndex < 254; l_uIndex++) {

                    l_rgchBuffer[5 + l_uIndex] = (UCHAR) l_uIndex;                 
                }

                //
                // This is the amount of bytes we write to the card in each loop
                //
                ULONG l_auNumBytes[] = { 1 , 25, 50, 75, 100, 125, 128}; //, 150, 175, 200, 225, 250, 254 };
        
                DWORD l_TimeStart = GetTickCount();

                for (ULONG l_uTest = 0; l_uTest < sizeof(l_auNumBytes) / sizeof(l_auNumBytes[0]); l_uTest++) {

                    l_uNumBytes = l_auNumBytes[l_uTest];
                     
                    //
                    // Write 
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_WRITEBINARY, l_uNumBytes);
                                
                    //
                    // Tpdu for write binary
                    //
                    memcpy(l_rgchBuffer, "\x00\xd6\x00\x00", 4);

                    //
                    // Append number of bytes (note: the buffer contains the pattern already)
                    //
                    l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                    l_lResult = in_CReader.Transmit(
                        l_rgchBuffer,
                        5 + l_uNumBytes,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    if (l_uNumBytes <= 128) 
                    {
                        TestCheck(
                            l_lResult, L"==", ERROR_SUCCESS,
                            l_uResultLength, 2,
                            l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                            NULL, NULL, 0
                        );
                    }  
                    else
                    {
                        TestCheck(
                            l_lResult, L"==", ERROR_SUCCESS,
                            l_uResultLength, 2,
                            l_pchResult[0], l_pchResult[1], 0x67, 0x00,
                            NULL, NULL, 0
                            );
                    }


                    TEST_END();

                    //
                    // Read
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_READBINARY, l_uNumBytes);

                    //
                    // tpdu for read binary
                    //
                    memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

                    //
                    // Append number of bytes
                    //
                    l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                    l_lResult = in_CReader.Transmit(
                        l_rgchBuffer,
                        5,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    //
                    // check if the right number of bytes has been returned
                    //
                    l_uExpectedLength = min(128, l_uNumBytes);

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, l_uExpectedLength + 2,
                        l_pchResult[l_uExpectedLength], 
                        l_pchResult[l_uExpectedLength + 1], 
                        0x90, 0x00,
                        l_pchResult, l_rgchBuffer + 5, l_uExpectedLength
                        );

                    TEST_END();
                }

                DWORD l_TimeEnd= GetTickCount();
                DWORD l_TimeElapsedSec = (l_TimeEnd - l_TimeStart)/CLOCKS_PER_SEC;

                if (l_TimeElapsedSec < 10) {

                    LogMessage(
                        LOG_CONSOLE,
                        IDS_SICRYPT_PERFISGOOD, 
                        l_TimeElapsedSec
                        );
                     
                } else if (l_TimeElapsedSec < 30) {

                    LogMessage(
                        LOG_CONSOLE,
                        IDS_SICRYPT_PERFISOK, 
                        l_TimeElapsedSec
                        );
                     
                } else {

                    LogMessage(
                        LOG_CONSOLE,
                        IDS_SICRYPT_PERFISBAD, 
                        l_TimeElapsedSec
                        );
                }
                break;
            }

        #if 1
        case 3: {
                //
                // Select a file 0007 and write alternately pattern 55 and AA 
                // to the card. 
                // Read the data back and verify correctness after each write. 
                //
                //
                // Select a file
                //
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTFILE7);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00\x02\x00\x07",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );
                    
                TEST_END();
                //
                // Do a couple of writes and reads alternately 
                // with patterns 55h and AAh 
                //

                //
                // Generate a 'test' pattern which will be written to the card
                //
                for (l_uIndex = 0; l_uIndex < 254; l_uIndex++) {

                    l_rgchBuffer[5 + l_uIndex] = (UCHAR)  0x55;    
                    l_rgchBuffer2[5 + l_uIndex] = (UCHAR) 0xAA;  
                }

                //
                // This is the amount of bytes we write to the card in each loop
                //
                l_uNumBytes = 128; 

                for (ULONG l_uTest = 0; l_uTest < 2; l_uTest++) {

                     
                    //
                    // Write 
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_WRITE55, l_uNumBytes);
                                
                    //
                    // Tpdu for write binary
                    //
                    memcpy(l_rgchBuffer, "\x00\xd6\x00\x00", 4);

                    //
                    // Append number of bytes (note: the buffer contains the pattern already)
                    //
                    l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                    l_lResult = in_CReader.Transmit(
                        l_rgchBuffer,
                        5 + l_uNumBytes,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                        
                    TEST_END();

                    //
                    // Read
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_READ55, l_uNumBytes);

                    //
                    // tpdu for read binary
                    //
                    memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

                    //
                    // Append number of bytes
                    //
                    l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                    l_lResult = in_CReader.Transmit(
                        l_rgchBuffer,
                        5,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    l_uExpectedLength = min(128, l_uNumBytes);

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, l_uExpectedLength + 2,
                        l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                        0x90, 0x00,
                        l_pchResult, l_rgchBuffer + 5, l_uExpectedLength
                        );
                        
                    TEST_END();

                    //
                    // Write 
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_WRITEAA, l_uNumBytes);
                                
                    //
                    // Tpdu for write binary
                    //
                    memcpy(l_rgchBuffer2, "\x00\xd6\x00\x00", 4);

                    //
                    // Append number of bytes (note: the buffer contains the pattern already)
                    //
                    l_rgchBuffer2[4] = (UCHAR) l_uNumBytes;

                    l_lResult = in_CReader.Transmit(
                        l_rgchBuffer2,
                        5 + l_uNumBytes,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                        
                    TEST_END();

                    //
                    // Read
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_READAA, l_uNumBytes);

                    //
                    // tpdu for read binary
                    //
                    memcpy(l_rgchBuffer2, "\x00\xB0\x00\x00", 4);

                    //
                    // Append number of bytes
                    //
                    l_rgchBuffer2[4] = (UCHAR) l_uNumBytes;

                    l_lResult = in_CReader.Transmit(
                        l_rgchBuffer2,
                        5,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    l_uExpectedLength = min(128, l_uNumBytes);

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, l_uExpectedLength + 2,
                        l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                        0x90, 0x00,
                        l_pchResult, l_rgchBuffer2 + 5, l_uExpectedLength
                        );

                    TEST_END();

                }
                break;             
            }
#endif

        case 4: {
            
                    // select a file 0007 and write alternately pattern 00 and FF 
                    // to the card. 
                    // Read the data back and verify correctness after each write. 


                    //
                    // Select a file
                    //
                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_SELECTFILE7);

                    l_lResult = in_CReader.Transmit(
                        (PUCHAR) "\x00\xa4\x00\x00\x02\x00\x07",
                        7,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, 0
                        );
                        
                    TEST_END();

                    //
                    // Do a couple of writes and reads alternately 
                    // with patterns 00h and FFh 
                    //

                    //
                    // Generate a 'test' pattern which will be written to the card
                    //
                    for (l_uIndex = 0; l_uIndex < 254; l_uIndex++) {

                        l_rgchBuffer[5 + l_uIndex] = (UCHAR)  0x00;    
                        l_rgchBuffer2[5 + l_uIndex] = (UCHAR) 0xFF;  
                    }

                    //
                    // This is the amount of bytes we write to the card in each loop
                    //
                    l_uNumBytes = 128; 

                    for (ULONG l_uTest = 0; l_uTest < 2; l_uTest++) {

                         
                        //
                        // Write 
                        //
                        g_uiFriendlyTest3++;
                        TestStart(IDS_SICRYPT_WRITE00, l_uNumBytes);
                                    
                        //
                        // Tpdu for write binary
                        //
                        memcpy(l_rgchBuffer, "\x00\xd6\x00\x00", 4);

                        //
                        // Append number of bytes (note: the buffer contains the pattern already)
                        //
                        l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                        l_lResult = in_CReader.Transmit(
                            l_rgchBuffer,
                            5 + l_uNumBytes,
                            &l_pchResult,
                            &l_uResultLength
                            );

                        TestCheck(
                            l_lResult, L"==", ERROR_SUCCESS,
                            l_uResultLength, 2,
                            l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                            NULL, NULL, 0
                            );
                            
                        TEST_END();

                        //
                        // Read
                        //
                        g_uiFriendlyTest3++;
                        TestStart(IDS_SICRYPT_READ00, l_uNumBytes);

                        //
                        // tpdu for read binary
                        //
                        memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

                        //
                        // Append number of bytes
                        //
                        l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                        l_lResult = in_CReader.Transmit(
                            l_rgchBuffer,
                            5,
                            &l_pchResult,
                            &l_uResultLength
                            );

                        l_uExpectedLength = min(128, l_uNumBytes);

                        TestCheck(
                            l_lResult, L"==", ERROR_SUCCESS,
                            l_uResultLength, l_uExpectedLength + 2,
                            l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                            0x90, 0x00,
                            l_pchResult, l_rgchBuffer + 5, l_uExpectedLength
                            );
                            
                        TEST_END();

                        //
                        // Write 
                        //
                        g_uiFriendlyTest3++;
                        TestStart(IDS_SICRYPT_WRITEFF, l_uNumBytes);
                                    
                        //
                        // Tpdu for write binary
                        //
                        memcpy(l_rgchBuffer, "\x00\xd6\x00\x00", 4);

                        //
                        // Append number of bytes (note: the buffer contains the pattern already)
                        //
                        l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                        l_lResult = in_CReader.Transmit(
                            l_rgchBuffer,
                            5 + l_uNumBytes,
                            &l_pchResult,
                            &l_uResultLength
                            );

                        TestCheck(
                            l_lResult, L"==", ERROR_SUCCESS,
                            l_uResultLength, 2,
                            l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                            NULL, NULL, 0
                        );
                            
                        TEST_END();

                        //
                        // Read
                        //
                        g_uiFriendlyTest3++;
                        TestStart(IDS_SICRYPT_READFF, l_uNumBytes);

                        //
                        // tpdu for read binary
                        //
                        memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

                        //
                        // Append number of bytes
                        //
                        l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

                        l_lResult = in_CReader.Transmit(
                            l_rgchBuffer,
                            5,
                            &l_pchResult,
                            &l_uResultLength
                            );

                        l_uExpectedLength = min(128, l_uNumBytes);

                        TestCheck(
                                    l_lResult, L"==", ERROR_SUCCESS,
                                    l_uResultLength, l_uExpectedLength + 2,
                                    l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                                    0x90, 0x00,
                                    l_pchResult, l_rgchBuffer + 5, l_uExpectedLength
                                );
                                    
                        TEST_END();

                    }
                    break;             
            }
        case 5: {
            //
            // Select Command for Nonexisting File
            //

                    g_uiFriendlyTest3++;
                    TestStart(IDS_SICRYPT_SELECTNOFILE);

                    l_lResult = in_CReader.Transmit(
                        (PUCHAR) "\x00\xa4\x00\x00\x02\x77\x77",
                        7,
                        &l_pchResult,
                        &l_uResultLength
                        );

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x94, 0x04,
                        NULL, NULL, 0
                        );
                        
                    TEST_END();
                
                break;
                
        }

            case 6: {
                //
                // Select Command without Fileid - selects the MF
                //
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTMF);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                );
                    
                TEST_END();
                
                break;
                
            }
            case 7: {
                //
                // Select Command with path too short
                //
                
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECT77);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00\x01\x77",
                    6,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x94, 0x04,
                    NULL, NULL, 0
                );
                    
                TEST_END();
                break;
        }
            case 8: {
                //
                // Select Command with wrong Lc
                //
                
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTWRONGLC);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00\x08\x00",
                    6,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x94, 0x04,
                    NULL, NULL, 0
                );
                    
                TEST_END();
                break;
        }

            case 9: {
                //
                // Select Command too short - command fragment
                //

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTTOOSHORT);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00",
                    3,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x67, 0x00,
                    NULL, NULL, 0
                );
                    
                TEST_END();
                break;
        }
        
            case 10: {
                //
                // More elaborate file selects and erase 
                //

               //TestStart("Select command variants");

                // Select the MF by a 4 byte command
                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTMF);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );
                      
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
                
                // authenticate for file creation and deletion with the admin PIN "12345678"

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_ADMINAUTH);
                
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\x20\x00\x02\x08\x31\x32\x33\x34\x35\x36\x37\x38",
                    13,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
                
                // add a subdir 5555 from the root

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_CREATE5555);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR)"\x90\xe0\x00\x00\x29"
                            "\x38\x22\x55\x55\x01\x28\x00\x86"
                            "\x15\xe0\x05\x11\xfe\x33\xfe\x10"
                            "\xe4\x05\x11\xfe\x33\xfe\x10\xda"
                            "\x05\x11\xfe\x33\xfe\x10\xd0\x05"
                            "\x61\x62\x63\x64\x31\xd3\x02\x00"
                            "\x01",
                    46,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
                
                // select into that subdir 5555

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECT5555);
                
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00\x02\x55\x55",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // add a new subdir 5656 of 5555

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_CREATE5656);
                    
                l_lResult = in_CReader.Transmit(
                    (PUCHAR)"\x90\xe0\x00\x00\x29"
                            "\x38\x22\x56\x56\x01\x28\x00\x86"
                            "\x15\xe0\x05\x11\xfe\x33\xfe\x10"
                            "\xe4\x05\x11\xfe\x33\xfe\x10\xda"
                            "\x05\x11\xfe\x33\xfe\x10\xd0\x05"
                            "\x61\x62\x63\x64\x32\xd3\x02\x00"
                            "\x01",
                    46,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
                
                // select into that subdir 5555.5656

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECT5656);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x00\x00\x02\x56\x56",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // create a file 5555.5656.5757

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_CREATE5757);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR)"\x90\xe0\x00\x00\x26"
                            "\x41\x41\x57\x57\x00\x00\x01\x86"
                            "\x12\xe4\x03\x11\xfe\x33\xd6\x03"
                            "\x11\xfe\x33\xda\x03\x11\xfe\x33"
                            "\xb0\x01\x00\xd1\x02\x00\x00\xd2"
                            "\x01\x02\xd0\x02\x57\x57",
                    43,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
                
                // select that file using a full path file select command

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTFILEBYPATH);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x06\x55\x55\x56\x56\x57\x57",
                    11,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
                
                // write 8 bytes to the selected file

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_WRITE8);
                
                 l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xd6\x00\x00\x08\x01\x02\x03\x04\x05\x06\x07\x08",
                    13,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // read 8 bytes data back from the file and compare with the data written

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_VERIFY8);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xb0\x00\x00\x08",
                    5,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 10,
                    l_pchResult[8], l_pchResult[9], 0x90, 0x00,
                    l_pchResult, 
                    (PUCHAR) "\x01\x02\x03\x04\x05\x06\x07\x08\x90\x00", 
                    l_uResultLength
                    );

                TEST_END();

                // erase the file by a 4 byte command (erase selected)

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_DELETESELECTED);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x90\xe4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // select that file using a full path select command
                // verify it is erased (expect file not found)

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_VERIFYNOTFOUND);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x06\x55\x55\x56\x56\x57\x57",
                    11,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x94, 0x04,
                    NULL, NULL, 0
                    );

                TEST_END();

                // select into that subdir 5555.5656 by path from root

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTDIRECTORY);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x04\x55\x55\x56\x56",
                    9,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // delete directory 5656 by 4 byte command (delete selected)

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_DELETESELECTED);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x90\xe4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // Select into 5555

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_SELECTDIRECTORY);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x02\x55\x55",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // delete directory 5555 by delete selected

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_DELETESELECTED);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x90\xe4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // select test file 0007

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_CLEANUPSELECT7);

                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x00\xa4\x08\x00\x02\x00\x07",
                    7,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                // delete selected file 0007 by 4 byte command

                g_uiFriendlyTest3++;
                TestStart(IDS_SICRYPT_DELETESELECTED);
                
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x90\xe4\x00\x00",
                    4,
                    &l_pchResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                    l_lResult,  L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();

                return IFDSTATUS_END;
                break;
        }
        default:
            return IFDSTATUS_FAILED;        
    }
    
    return IFDSTATUS_SUCCESS;
    }    


/*++

Routine Description:
    
    This function registers all callbacks from the test suite
    
Arguments:

    CCardProvider - ref. to card provider class

Return Value:

    -

--*/
void
MyCardEntry(
    class CCardProvider& in_CCardProvider
    )
{
    // Set protocol callback
    in_CCardProvider.SetProtocol(MyCardSetProtocol);

    // Card test callback
    in_CCardProvider.SetCardTest(MyCardTest);

    // Name of our card
    in_CCardProvider.SetCardName(L"PC/SC Compliance Test Card 4 (Infineon)"); //(L"SiCrypt");

    in_CCardProvider.SetAtr((PBYTE) "\x3b\xdf\x18\x00\x81\x31\xfe\x67\x00\x5c\x49\x43\x4d\xd4\x91\x47\xd2\x76\x00\x00\x38\x33\x00\x58", 24);
}
    
