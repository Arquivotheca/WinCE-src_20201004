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

     ammi.cpp

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
#include "ammirc.h"

void 
AMMITestCardEntry(
    class CCardProvider& in_CCardProvider
    );
//
// Create a card provider object
// Note: all global varibales and all functions have to be static
//
static class CCardProvider AMMITestCard(AMMITestCardEntry);

static ULONG
AMMITestCardSetProtocol(
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

    TestStart(IDS_AMMI_SET_INCORRECT_PROTOCOL);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);

    // The test MUST fail with the incorrect protocol
    TEST_CHECK_NOT_SUPPORTED(TestLoadStringResource(IDS_AMMI_SET_FAILED).GetBuffer(0), l_lResult);
    TestEnd();

    // Now set the correct protocol
    TestStart(IDS_AMMI_SET_PROTOCOL_T0);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);
    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_AMMI_SET_FAILED).GetBuffer(0), l_lResult);
    TestEnd();

    if (l_lResult != ERROR_SUCCESS) {

        return IFDSTATUS_FAILED;
    }

    return IFDSTATUS_SUCCESS;
}

static 
ULONG
AMMITestCardTest(
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
    ULONG l_lResult, l_uResultLength, l_uIndex;
    PUCHAR l_pbResult;
    UCHAR l_rgbBuffer[512];
    
    // Generate a 'test' pattern which will be written to the card
    for (l_uIndex = 0; l_uIndex < 256; l_uIndex++) {

        l_rgbBuffer[l_uIndex + 5] = (UCHAR) l_uIndex;                 
    }

    switch (in_CCardProvider.GetTestNo()) {
    
        case 1: {

            // select a file
            TestStart(IDS_AMMI_SELECT_FILE_EFPTSDATACHECK);

            l_lResult = in_CReader.Transmit(
                (PBYTE) "\x00\xa4\x00\x00\x02\x00\x10",
                7,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pbResult[0], l_pbResult[1], 0x61, 0x15,
                NULL, NULL, NULL
                );

            TEST_END();         

            // Test read of 256 bytes
            ULONG l_uNumBytes = 256;
            TestStart(IDS_AMMI_READ_BINARY, l_uNumBytes);

            l_lResult = in_CReader.Transmit(
                (PBYTE) "\x00\xb0\x00\x00\x00",
                5,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, l_uNumBytes + 2,
                l_pbResult[l_uNumBytes], l_pbResult[l_uNumBytes + 1], 0x90, 0x00,
                l_pbResult, l_rgbBuffer + 5, l_uNumBytes
                );

            TEST_END();
            break;             
        }

        case 2: {

            // select a file
            TestStart(IDS_AMMI_SELECT_FILE_EFPTSDATACHECK);

            l_lResult = in_CReader.Transmit(
                (PBYTE) "\x00\xa4\x00\x00\x02\x00\x10",
                7,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pbResult[0], l_pbResult[1], 0x61, 0x15,
                NULL, NULL, NULL
                );

            TEST_END();         

            // Test write of 255 bytes
            ULONG l_uNumBytes = 255;
            TestStart(IDS_AMMI_WRITE_BINARY, l_uNumBytes);

            // set the number of bytes we want to write to the card
            memcpy(l_rgbBuffer, "\x00\xd6\x00\x00", 4);

            l_rgbBuffer[4] = (BYTE) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgbBuffer,
                l_uNumBytes + 5,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                NULL, NULL, 0
                );

            TEST_END();
            break;             
        }

        case 3: {

            //
            // Read the result file from the smart card.
            // The card stores results of each test in 
            // a special file
            //
             
            TestStart(IDS_AMMI_SELECT_FILE_EFRESULT);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xa4\x00\x00\x02\xa0\x00",
                7,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pbResult[0], l_pbResult[1], 0x61, 0x15,
                NULL, NULL, NULL
                );

            TEST_END();         

            // Read
            TestStart(IDS_AMMI_READ_FILE_EFRESULT);

            // apdu for read binary
            memcpy(l_rgbBuffer, "\x00\xb0\x00\x00", 4);

            // Append number of bytes we want to read
            l_rgbBuffer[4] = (BYTE) sizeof(T0_RESULT_FILE_HEADER);

            // read in the header of the result file
            l_lResult = in_CReader.Transmit(
                l_rgbBuffer,
                5,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, sizeof(T0_RESULT_FILE_HEADER) + 2,
                l_pbResult[sizeof(T0_RESULT_FILE_HEADER)], 
                l_pbResult[sizeof(T0_RESULT_FILE_HEADER) + 1], 
                0x90, 0x00,
                NULL, NULL, NULL
                );

            // get the card reset count
            PT0_RESULT_FILE_HEADER l_pCResultFileHeader;
            l_pCResultFileHeader = (PT0_RESULT_FILE_HEADER) l_pbResult;
            BYTE l_bCardResetCount = l_pCResultFileHeader->CardResetCount;

            // set the offset from where we want to read
            l_rgbBuffer[3] = (BYTE) l_pCResultFileHeader->Offset;
            // Append number of bytes
            l_rgbBuffer[4] = (BYTE) sizeof(T0_RESULT_FILE);

            // read in the result data of the result file
            l_lResult = in_CReader.Transmit(
                l_rgbBuffer,
                5,
                &l_pbResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, sizeof(T0_RESULT_FILE) + 2,
                l_pbResult[sizeof(T0_RESULT_FILE)], 
                l_pbResult[sizeof(T0_RESULT_FILE) + 1], 
                0x90, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();

            // Now check the result file. 
            PT0_RESULT_FILE l_pCResultFile = (PT0_RESULT_FILE) l_pbResult;

            // check if the card received a proper PTS
            TestStart(IDS_AMMI_PTS);
            TestCheck(
                l_pCResultFile->PTS.ResetCount == l_bCardResetCount,
                IDS_AMMI_NO_TEST
                );
            TestCheck(
                (l_pCResultFile->PTS.Result & 0x01) != 1, 
                IDS_AMMI_NO_PTS1
                );
            TEST_END();

            TestStart(IDS_AMMI_PTS_DATA_CHECK);
            TestCheck(
                l_pCResultFile->PTSDataCheck.ResetCount == l_bCardResetCount,
                IDS_AMMI_NO_TEST
                );
            TestCheck(
                (l_pCResultFile->PTSDataCheck.Result) == 0, 
                IDS_AMMI_INCORRECT_DATA
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
AMMITestCardEntry(
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
    in_CCardProvider.SetProtocol(AMMITestCardSetProtocol);

    // Card test callback
    in_CCardProvider.SetCardTest(AMMITestCardTest);

    // Name of our card
    in_CCardProvider.SetCardName(L"AMMI");

    // Name of our card
    in_CCardProvider.SetAtr(
        (PBYTE) "\x3b\x7e\x13\x00\x00\x80\x53\xff\xff\xff\x62\x00\xff\x71\xbf\x83\x03\x90\x00", 
        19
        );
}

