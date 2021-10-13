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

     ibm.cpp

Abstract:
Functions:
Notes:
--*/


#ifndef UNDER_CE
#include <afx.h>
#include <afxtempl.h>
#else
#include "afxutil.h"
#endif

#include <winioctl.h>
#include <winsmcrd.h>

#include "ifdtest.h"
#include "ibmrc.h"

void MyCardEntry(class CCardProvider& in_CCardProvider);

//
// Create a card provider object
// Note: all global varibales and all functions have to be static
//
static class CCardProvider MyCard(MyCardEntry);
                                                  
                                          
static ULONG
MyCardSetProtocol(
    class CCardProvider& in_CCardProvider,
    class CReader& in_CReader
    )
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
{
    ULONG l_lResult;

    TestStart(IDS_IBM_SET_PROTOCOL_T0);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);

    // The test MUST fail with the incorrect protocol
    TEST_CHECK_NOT_SUPPORTED(TestLoadStringResource(IDS_IBM_SET_PROTOCOL_FAILED).GetBuffer(0), l_lResult);
    TestEnd();

    // Now set the correct protocol
    TestStart(IDS_IBM_SET_PROTOCOL_T1);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);
    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_IBM_SET_PROTOCOL_FAILED).GetBuffer(0), l_lResult);
    TestEnd();

    if (l_lResult != ERROR_SUCCESS) {

        return IFDSTATUS_FAILED;
    }

    return IFDSTATUS_SUCCESS;
}

static ULONG
MyCardTest(
    class CCardProvider& in_CCardProvider,
    class CReader& in_CReader
    )
/*++

Routine Description:
        
    This serves as the test function for a particular smart card

Arguments:

    in_CReader - ref. to class that provides all information for the test

Return Value:

    IFDSTATUS value

--*/
{
    ULONG l_lResult;
    ULONG l_uResultLength, l_uExpectedLength, l_uIndex;
    PUCHAR l_pchResult;
    UCHAR l_rgchBuffer[512], l_rgchBuffer2[512];

    switch (in_CCardProvider.GetTestNo()) {

        case 1: {
            //
            // select a file 0007 and write data pattern 0 to N-1 to the card. 
            // Then read the data back and verify correctness. 
            // Check IFSC and IFSD above card limits
            //

            //
            // Select a file
            //
            TestStart(IDS_IBM_SELECT_FILE_0007);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x02\x00\x07",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 16,
                l_pchResult[14], l_pchResult[15], 0x90, 0x00,
                l_pchResult, 
                (PUCHAR) "\x63\x0c\x03\xe8\x00\x07\x00\x00\x00\xff\xff\x11\x01\x00\x90\x00", 
                l_uResultLength
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
            ULONG l_auNumBytes[] = { 1 , 25, 50, 75, 100, 125, 128, 150, 175, 200, 225, 250, 254 };
    
            DWORD l_TimeStart;
            l_TimeStart = GetTickCount();

            for (ULONG l_uTest = 0; l_uTest < sizeof(l_auNumBytes) / sizeof(l_auNumBytes[0]); l_uTest++) {

                ULONG l_uNumBytes = l_auNumBytes[l_uTest];
                 
                //
                // Write 
                //
                TestStart(IDS_IBM_WRITE_BINARY, l_uNumBytes);
                            
                //
                // Tpdu for write binary
                //
                memcpy(l_rgchBuffer, "\xa4\xd6\x00\x00", 4);

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

                if (l_uNumBytes <= 128) {

                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                        NULL, NULL, NULL
                        );

                } else {
                     
                    TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pchResult[0], l_pchResult[1], 0x67, 0x00,
                        NULL, NULL, NULL
                        );
                }

                TEST_END();

                //
                // Read
                //
                TestStart(IDS_IBM_READ_BINARY, l_uNumBytes);

                //
                // tpdu for read binary
                //
                memcpy(l_rgchBuffer, "\xa4\xB0\x00\x00", 4);

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
                    IDS_IBM_READER_PERF_GOOD, 
                    l_TimeElapsedSec
                    );
                 
            } else if (l_TimeElapsedSec < 30) {

                LogMessage(
                    LOG_CONSOLE,
                    IDS_IBM_READER_PERF_AVG, 
                    l_TimeElapsedSec
                    );
                 
            } else {

                LogMessage(
                    LOG_CONSOLE,
                    IDS_IBM_READER_PERF_BAD, 
                    l_TimeElapsedSec
                    );
            }
            break;
        }

        case 2: {
            //
            // Select a file 0007 and write alternately pattern 55 and AA 
            // to the card. 
            // Read the data back and verify correctness after each write. 
            //
            //
            // Select a file
            //
            TestStart(IDS_IBM_SELECT_FILE_0007);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x02\x00\x07",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 16,
                l_pchResult[14], l_pchResult[15], 0x90, 0x00,
                l_pchResult, 
                (PUCHAR) "\x63\x0c\x03\xe8\x00\x07\x00\x00\x00\xff\xff\x11\x01\x00\x90\x00", 
                l_uResultLength
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
            ULONG l_uNumBytes = 128; 

            for (ULONG l_uTest = 0; l_uTest < 2; l_uTest++) {

                 
                //
                // Write 
                //
                TestStart(IDS_IBM_WRITE_BINARY_PATTERN55, l_uNumBytes);
                            
                //
                // Tpdu for write binary
                //
                memcpy(l_rgchBuffer, "\xa4\xd6\x00\x00", 4);

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
                    NULL, NULL, NULL
                    );

                TEST_END();

                //
                // Read
                //
                TestStart(IDS_IBM_READ_BINARY_PATTERN55, l_uNumBytes);

                //
                // tpdu for read binary
                //
                memcpy(l_rgchBuffer, "\xa4\xB0\x00\x00", 4);

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
                TestStart(IDS_IBM_WRITE_BINARY_PATTERNAA, l_uNumBytes);
                            
                //
                // Tpdu for write binary
                //
                memcpy(l_rgchBuffer2, "\xa4\xd6\x00\x00", 4);

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
                    l_pchResult[0], l_pchResult[1], 
                    0x90, 0x00,
                    NULL, NULL, NULL
                    );

                TEST_END();

                //
                // Read
                //
                TestStart(IDS_IBM_READ_BINARY_PATTERNAA, l_uNumBytes);

                //
                // tpdu for read binary
                //
                memcpy(l_rgchBuffer2, "\xa4\xB0\x00\x00", 4);

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
                    l_pchResult, l_rgchBuffer2 + 5, min(l_uExpectedLength,125)
                    );

                TEST_END();
            }

            break;             
        }

        case 3: {
            
            // select a file 0007 and write alternately pattern 00 and FF 
            // to the card. 
            // Read the data back and verify correctness after each write. 


            //
            // Select a file
            //
            TestStart(IDS_IBM_SELECT_FILE_0007);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x02\x00\x07",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 16,
                l_pchResult[14], l_pchResult[15], 0x90, 0x00,
                l_pchResult, 
                (PUCHAR) "\x63\x0c\x03\xe8\x00\x07\x00\x00\x00\xff\xff\x11\x01\x00\x90\x00", 
                l_uResultLength
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
            ULONG l_uNumBytes = 128; 

            for (ULONG l_uTest = 0; l_uTest < 2; l_uTest++) {

                 
                //
                // Write 
                //
                TestStart(IDS_IBM_WRITE_BINARY_PATTERN00, l_uNumBytes);
                            
                //
                // Tpdu for write binary
                //
                memcpy(l_rgchBuffer, "\xa4\xd6\x00\x00", 4);

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
                    NULL, NULL, NULL
                    );

                TEST_END();

                //
                // Read
                //
                TestStart(IDS_IBM_READ_BINARY_PATTERN00, l_uNumBytes);

                //
                // tpdu for read binary
                //
                memcpy(l_rgchBuffer, "\xa4\xB0\x00\x00", 4);

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
                    l_pchResult, 
                    l_rgchBuffer + 5, 
                    l_uExpectedLength
                    );

                TEST_END();

                //
                // Write 
                //
                TestStart(IDS_IBM_WRITE_BINARY_PATTERNFF, l_uNumBytes);
                            
                //
                // Tpdu for write binary
                //
                memcpy(l_rgchBuffer2, "\xa4\xd6\x00\x00", 4);

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
                    l_pchResult[0], l_pchResult[1], 
                    0x90, 0x00,
                    NULL, NULL, NULL
                    );

                TEST_END();

                //
                // Read
                //
                TestStart(IDS_IBM_READ_BINARY_PATTERNFF, l_uNumBytes);

                //
                // tpdu for read binary
                //
                memcpy(l_rgchBuffer2, "\xa4\xB0\x00\x00", 4);

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
                    l_pchResult, 
                    l_rgchBuffer2 + 5, 
                    min(l_uExpectedLength,125)
                    );

                TEST_END();
            }
            break;             
        }

        case 4: {
            //
            // Select Command for Nonexisting File
            //

            TestStart(IDS_IBM_SELECT_NONEXISTING_FILE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x02\x77\x77",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x94, 0x04,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 5: {
            //
            // Select Command without Fileid
            //
            TestStart(IDS_IBM_SELECT_COMMAND_NO_FILEID);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00",
                4,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x67, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 6: {
            //
            // Select Command with path too short
            //
            
           TestStart(IDS_IBM_SELECT_COMMAND_SHORT_PATH);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x01\x77",
                6,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x67, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 7: {
            //
            // Select Command with wrong Lc
            //
            
           TestStart(IDS_IBM_SELECT_COMMAND_WRONG_LC);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x08\x00",
                6,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x67, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 8: {
            //
            // Select Command too short
            //

           TestStart(IDS_IBM_SELECT_COMMAND_SHORT);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00",
                3,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x6f, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 9: {
            //
            // Select Command with invalid P2
            //

           TestStart(IDS_IBM_SELECT_COMMAND_INVALID_P2);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x02\x02\x00\x07",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x6b, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 10: {
            //
            // Select command without fileid but with Le
            //
            
            TestStart(IDS_IBM_SELECT_COMMAND_NO_FILEID_YES_LE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x00",
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x67, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;
        }

        case 11: {
            //
            // Use Change Speed command to simulate unresponsive card
            //

            //
            // Select a file
            //
            TestStart(IDS_IBM_SELECT_FILE_0007);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x02\x00\x07",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 16,
                l_pchResult[14], l_pchResult[15], 0x90, 0x00,
                l_pchResult, 
                (PUCHAR) "\x63\x0c\x03\xe8\x00\x07\x00\x00\x00\xff\xff\x11\x01\x00\x90\x00", 
                l_uResultLength
                );

            TEST_END();

            //
            // Perform change speed command to simulate unresponsive card 
            //
            TestStart(IDS_IBM_CHANGE_SPEED);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xb6\x42\x00\x40",
                4,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 
                0x90, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();

            //
            // Select a file to verify bad return code
            //
            TestStart(IDS_IBM_SELECT00007_NO_VALID_RESPONSE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xa4\xa4\x00\x00\x02\x00\x07",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"!=", ERROR_SUCCESS,
                NULL, NULL,
                NULL, NULL,
                NULL, NULL,
                NULL, NULL, NULL
                );

            TEST_END();
            return IFDSTATUS_END;
        }

        default:
            return IFDSTATUS_FAILED;        
    }
    
    return IFDSTATUS_SUCCESS;
}    

static void
MyCardEntry(
    class CCardProvider& in_CCardProvider
    )
/*++

Routine Description:
    
    This function registers all callbacks from the test suite
    
Arguments:

    CCardProvider - ref. to card provider class

Return Value:

    -

--*/
{
    // Set protocol callback
    in_CCardProvider.SetProtocol(MyCardSetProtocol);

    // Card test callback
    in_CCardProvider.SetCardTest(MyCardTest);

    // Name of our card
    in_CCardProvider.SetCardName(L"IBM");

    in_CCardProvider.SetAtr((PBYTE) "\x3b\xef\x00\xff\x81\x31\x86\x45\x49\x42\x4d\x20\x4d\x46\x43\x34\x30\x30\x30\x30\x38\x33\x31\x43", 24);
}

