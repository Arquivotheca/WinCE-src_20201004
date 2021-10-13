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

    bull.cpp

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
#include "winsmcrd.h"

#include "ifdtest.h"
#include "bullrc.h"

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

    TestStart(IDS_BULL_SET_PROTOCOL_T1);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);

    // The test MUST fail with the incorrect protocol
    TEST_CHECK_NOT_SUPPORTED(TestLoadStringResource(IDS_BULL_SET_PROTOCOL_FAILED).GetBuffer(0), l_lResult);
    TestEnd();

    // Now set the correct protocol
    TestStart(IDS_BULL_SET_PROTOCOL_T0);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);
    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_BULL_SET_PROTOCOL_FAILED).GetBuffer(0), l_lResult);
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

    ULONG l_auNumBytes[] = { 1 , 25, 50, 75, 100, 125, 150, 175, 200, 225, 254 };
    
     
    ULONG l_uNumBytes = l_auNumBytes[10];
    
    ULONG l_lResult;
    ULONG l_uResultLength, l_uIndex;
    PUCHAR l_pchResult;
    UCHAR l_rgchBuffer[512];
    ULONG l_uTest;
    UCHAR Buf_Tempo[9];
    ULONG l_Tempo;
    ULONG Adresse;

    switch (in_CCardProvider.GetTestNo()) {

        case 1:
            TestStart(IDS_BULL_BUFFER_BOUNDARY);

            //
            // Check if the reader correctly determines that
            // our receive buffer is too small
            //
            in_CReader.SetReplyBufferSize(9);
            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\x84\x00\x00\x08",
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult == ERROR_INSUFFICIENT_BUFFER,
                IDS_BULL_SHOULD_FAIL_SMALL_BUFFER                
                );

            TestEnd();

            in_CReader.SetReplyBufferSize(2048);
            break;

        case 2:            
            TestStart(IDS_BULL_3BYTE_APDU);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xC4\x00",
                3,                                        
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_INVALID_PARAMETER,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, NULL
                );

            TEST_END();
            break;

        case 3:            
            // Get Challenge
            TestStart(IDS_BULL_GET_CHALLENGE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xC4\x00\x00\x08",
                5,                                        
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 10,
                l_pchResult[8], l_pchResult[9], 0x90, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();

            //
            // Submit Alternate Identification Code (AID)
            //
            TestStart(IDS_BULL_VERIFY_PIN);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\x38\x00\x00\x0A\x01\x02\x03\x04\x05\x06\x07\x08\x09\0x0A",
                15,
                &l_pchResult,
                &l_uResultLength
                ); 
            
            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 0x90, 0x08,
                NULL, NULL, NULL
                );

            TEST_END();
            break;

        case 4:
            // Translate of 4 byte APDU (Search for next blank word)
            TestStart(IDS_BULL_SEARCH_BLANK_WORD);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xA0\x00\x00",        // Search for next blank word
                4,
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

            // Read result of Search for next blank word
            TestStart(IDS_BULL_GET_RESPONSE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xC0\x00\x00\x08",        // Read Result command 
                5,                                        
                &l_pchResult,
                &l_uResultLength
                );
        
            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 10,
                l_pchResult[8], l_pchResult[9], 0x90, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();
            break;

        case 5:    
            // Select Working File 2F01
            TestStart(IDS_BULL_LC_BYTE_INCORRECT);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xA4\x00\x00\x02\x2F",
                6,
                &l_pchResult,
                &l_uResultLength
                ); 

            TestCheck(
                l_lResult, L"==", ERROR_INVALID_PARAMETER,
                0, 0, 
                0, 0, 0, 0,
                NULL, NULL, NULL
                );

            TEST_END();
            break;

        case 6:    
            //
            // Select Working File 2F01
            //
            TestStart(IDS_BULL_SELECT_FILE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xA4\x00\x00\x02\x2F\x01",
                7,
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
            
            // Erase memory with restart of work waiting time
            TestStart(IDS_BULL_ERASE_BINARY);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\x0E\x00\x00\x02\x00\x78",
                7,
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
            
            // Generate a 'test' pattern which will be written to the card
            for (l_uIndex = 0; l_uIndex < 256; l_uIndex++) {

                l_rgchBuffer[l_uIndex] = (UCHAR) l_uIndex;                 
            }
            
            // Tpdu for write binary. TB100L can write only 4 byte 4 byte
            memcpy(Buf_Tempo, "\xBC\xD0", 2);    // writting order
            Buf_Tempo[4] = 0x4;                    //write 4 bytes in the card
            
            // This is the amount of bytes we write to the card 
            l_uTest = 0;
            Adresse = 0;

            while (l_uTest < 256) {
                
                for(l_Tempo=5 ; l_Tempo < 9; l_Tempo++){

                     Buf_Tempo[l_Tempo] = l_rgchBuffer[l_uTest++];
                     
                 }
                    
                 Buf_Tempo[2] = 00;                // Writting address
                 Buf_Tempo[3] = (UCHAR) Adresse++;
             
                //
                // Write 
                //
                   TestStart(IDS_BULL_WRITE_BINARY,Adresse);
                            
                //
                // Append number of bytes (note: the buffer contains the pattern already)
                //
                l_lResult = in_CReader.Transmit(
                    Buf_Tempo,
                    9,
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
            }
                
            // Read 256 bytes 
            TestStart(IDS_BULL_READ_BINARY_256_BYTES);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xB0\x00\x00\x00",
                5,
                &l_pchResult,
                &l_uResultLength
                );
            
            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, l_uNumBytes + 4,
                l_pchResult[256], l_pchResult[257], 0x90, 0x00,
                l_pchResult, l_rgchBuffer , l_uNumBytes + 2
                );

            TEST_END();
            break;

        case 7:
            //
            // Command with Slave Mode
            // Data bytes transferred subsequently (INS')
            //
    
            TestStart(IDS_BULL_GENERATE_TEMP_KEY);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\x80\x00\x00\x02\x12\x00",
                7,
                &l_pchResult,
                &l_uResultLength
                ); 
            
            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 0x90, 0x08,
                NULL, NULL, NULL
                );

            TEST_END();
            break;

        case 8:
            // Select Master File 3F00
            TestStart(IDS_BULL_SELECT_FILE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\xA4\x00\x00\x02\x3F\x00",
                7,
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
            
            // Erase memory on an invalid file => mute card
            TestStart(IDS_BULL_ERASE_BINARY);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xBC\x0E\x00\x00\x02\x00\x78",
                7,
                &l_pchResult,
                &l_uResultLength
                ); 
            
            TestCheck(
                l_lResult, L"==", ERROR_SEM_TIMEOUT,
                NULL, NULL, 
                NULL, NULL, 
                NULL, NULL, 
                NULL, NULL, NULL
                );

            TEST_END();
            return IFDSTATUS_END;
    
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
    in_CCardProvider.SetCardName(L"Bull");

    // Maximum number of tests
    in_CCardProvider.SetAtr((PBYTE) "\x3f\x67\x25\x00\x21\x20\x00\x0F\x68\x90\x00", 11);
}

