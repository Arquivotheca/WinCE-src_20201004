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

    example.cpp

Abstract:

    This is a plug-in for the smart card driver test suite.
    This plug-in is smart card dependent

Author:

    Klaus U. Schutz

Revision History :

    Nov. 1997 - initial version

--*/
#ifndef UNDER_CE
#include <stdarg.h> 
#include <stdio.h>
#include <string.h>

#include <afx.h>
#include <afxtempl.h>
#else
#include "afxutil.h"
#endif

#include <winioctl.h>
#include <winsmcrd.h>

#include "ifdtest.h"
#include "ifdguid.h"
#include "snirc.h"

#pragma warning (disable: 4211)
void MyCardEntry(class CCardProvider& in_CCardProvider);

//
// Create a card provider object
// Note: all global varibales and all functions have to be static
//
static class CCardProvider MyCard(MyCardEntry);

//
// This structure represents the result file 
// that is stored in the smart card
//
typedef struct _RESULT_FILE {
     
    // Offset to first test result
    UCHAR Offset;

    // Number of times the card has been reset
    UCHAR CardResetCount;

    // Version number of this card
    UCHAR CardMajorVersion;
    UCHAR CardMinorVersion;

    // RFU
    UCHAR Reserved[6];

    //
    // The following structures store the results
    // of the tests. Each result comes with the 
    // reset count when the test was performed.
    // This is used to make sure that we read not
    // the result from an old test, maybe even 
    // performed with another reader/driver.
    //
    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } Wtx;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } ResyncRead;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } ResyncWrite;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } Seqnum;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } IfscRequest;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } IfsdRequest;

    struct {

        UCHAR Result;
        UCHAR ResetCount;     

    } Timeout;

} RESULT_FILE, *PRESULT_FILE;



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
    g_uiFriendlyTest1 = 5;
    g_uiFriendlyTest2 = 0;
    g_uiFriendlyTest3 = 0;

    g_uiFriendlyTest3++;
    TestStart(IDS_SNI_SET_PROTOCOL_T0);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);

    // The test MUST fail with the incorrect protocol
    TEST_CHECK_NOT_SUPPORTED(TestLoadStringResource(IDS_SNI_SET_PROTOCOL_FAILED).GetBuffer(0), l_lResult);
    TestEnd();

    // Now set the correct protocol
    g_uiFriendlyTest3++;
    TestStart(IDS_SNI_SET_PROTOCOL_T1);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);
    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_SNI_SET_PROTOCOL_FAILED).GetBuffer(0), l_lResult);
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
    ULONG l_lResult, l_uResultLength, l_uIndex;
    PUCHAR l_pchResult;
    UCHAR l_rgchBuffer[512];
    CHAR l_chFileId;
    g_uiFriendlyTest1 = 5;
    g_uiFriendlyTest2 = in_CCardProvider.GetTestNo();
    g_uiFriendlyTest3 = 0;

    if (in_CCardProvider.GetTestNo() > 1 && in_CCardProvider.GetTestNo() < 7) {

        //
        // Select the appropriate file for the test
        // Each test is tied to a particular file
        //
        l_chFileId = (CHAR) in_CCardProvider.GetTestNo() - 1;

        //
        // APDU for select file
        //
        LONG l_FileDesc[] = {
            IDS_SNI_SELECT_FILE_EFWTX,
            IDS_SNI_SELECT_FILE_FRESYNC,
            IDS_SNI_SELECT_FILE_EFSEQNUM,
            IDS_SNI_SELECT_FILE_EFIFS,
            IDS_SNI_SELECT_FILE_EFTIMEOUT
        };

        memcpy(l_rgchBuffer, "\x00\xa4\x08\x04\x04\x3e\x00\x00\x00", 9);

        //
        // add file number to select
        //
        l_rgchBuffer[8] = l_chFileId;

        //
        // select a file
        //
        g_uiFriendlyTest3++;
        TestStart(l_FileDesc[l_chFileId - 1]);

        l_lResult = in_CReader.Transmit(
            (PUCHAR) l_rgchBuffer,
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

        //
        // Generate a 'test' pattern which will be written to the card
        //
        for (l_uIndex = 0; l_uIndex < 256; l_uIndex++) {

            l_rgchBuffer[5 + l_uIndex] = (UCHAR) l_uIndex;                 
        }
    }


    switch (in_CCardProvider.GetTestNo()) {

        case 1:
            //
            // First test
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_BUFFER_BOUNDARY);

            //
            // Check if the reader correctly determines that
            // our receive buffer is too small
            //
            in_CReader.SetReplyBufferSize(9);
            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x08\x84\x00\x00\x08",
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult == ERROR_INSUFFICIENT_BUFFER,
                IDS_SNI_SHOULD_FAIL_SMALL_BUFFER,
                l_lResult, 
                ERROR_INSUFFICIENT_BUFFER
                );

            TestEnd();

            in_CReader.SetReplyBufferSize(2048);
            break;

        case 2: {

            //
            // Wtx test file id 00 01
            // This test checks if the reader/driver correctly handles WTX requests
            //
            ULONG l_auNumBytes[] = { 1 , 2, 5, 30 };

            for (ULONG l_uTest = 0; 
                 l_uTest < sizeof(l_auNumBytes) / sizeof(l_auNumBytes[0]); 
                 l_uTest++) {

                ULONG l_uNumBytes = l_auNumBytes[l_uTest];

                //
                // Now read from this file
                // The number of bytes we read corresponds to 
                // the waiting time extension this command produces
                //
                g_uiFriendlyTest3++;
                TestStart(IDS_SNI_READ_BINARY, l_uNumBytes);

                //
                // apdu for read binary
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

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, l_uNumBytes + 2,
                    l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                    0x90, 0x00,
                    l_pchResult, l_rgchBuffer + 5, l_uNumBytes
                    );

                TEST_END();
            }
            break;
        }

        case 3: {
             
            ULONG l_uNumBytes = 255;

            // resync. on write file id 00 02
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_WRITE_BINARY, l_uNumBytes);
                    
            // Tpdu for write binary
            memcpy(l_rgchBuffer, "\x00\xd6\x00\x00", 4);

            // Append number of bytes (note: the buffer contains the pattern already)
            l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5 + l_uNumBytes,
                &l_pchResult,
                &l_uResultLength
                );
            TestCheck(
                l_lResult, L"==", ERROR_IO_DEVICE,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, 0
                );
            TEST_END();

            // resync. on read file id 00 02
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_READ_BINARY, l_uNumBytes);

            // tpdu for read binary
            memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

            // Append number of bytes
            l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_IO_DEVICE,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, 0
                );
            TEST_END();
            break;
        }

        case 4: {
             
            // wrong block seq. no file id 00 03
            ULONG l_uNumBytes = 255;
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_READ_BINARY, l_uNumBytes);
                    
            // Tpdu for read binary
            memcpy(l_rgchBuffer, "\x00\xb0\x00\x00", 4);

            // Append number of bytes (note: the buffer contains the pattern already)
            l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5,
                &l_pchResult,
                &l_uResultLength
                );
            TestCheck(
                l_lResult, L"==", ERROR_IO_DEVICE,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, 0
                );
            TEST_END();
            break;
        }

        case 5: { 

            // ifsc request file id 00 04
            ULONG l_uNumBytes = 255;
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_WRITE_BINARY, l_uNumBytes);
                    
            // Tpdu for write binary
            memcpy(l_rgchBuffer, "\x00\xd6\x00\x00", 4);

            // Append number of bytes (note: the buffer contains the pattern already)
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
#ifdef junk
            l_uNumBytes = 255;
            g_uiFriendlyTest3++;
            TestStart(L"READ  BINARY %3d byte(s)", l_uNumBytes);

            // tpdu for read binary
            memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

            // Append number of bytes
            l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, l_uNumBytes + 2,
                l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                0x90, 0x00,
                l_pchResult, l_rgchBuffer + 5, l_uNumBytes
                );

            TEST_END();
#endif
            break;
        }

        case 6: {

            // forced timeout file id 00 05
            ULONG l_uNumBytes = 254;
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_READ_BINARY, l_uNumBytes);

            // tpdu for read binary
            memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

            // Append number of bytes
            l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_IO_DEVICE,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, 0
                );

            TEST_END();
            break;             
        }

        case 7:{

            //
            // Read the result file from the smart card.
            // The card stores results of each test in 
            // a special file
            //
            ULONG l_uNumBytes = sizeof(RESULT_FILE);
            PRESULT_FILE pCResultFile;
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_SELECT_FILE_EFRESULT);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xa4\x08\x04\x04\x3e\x00\xA0\x00",
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

            // Read
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_READ_BINARY, l_uNumBytes);

            // apdu for read binary
            memcpy(l_rgchBuffer, "\x00\xB0\x00\x00", 4);

            // Append number of bytes
            l_rgchBuffer[4] = (UCHAR) l_uNumBytes;

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, l_uNumBytes + 2,
                l_pchResult[l_uNumBytes], l_pchResult[l_uNumBytes + 1], 
                0x90, 0x00,
                NULL, NULL, NULL
                );

            TEST_END();

            pCResultFile = (PRESULT_FILE) l_pchResult;

            //
            // Now check the result file. 
            //

            //
            // Check wtx result
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_WTX_RESULT);
            TestCheck(
                pCResultFile->Wtx.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                (pCResultFile->Wtx.Result & 0x01) == 0, 
                IDS_SNI_NO_WTX_REPLY
                );
            TestCheck(
                (pCResultFile->Wtx.Result & 0x02) == 0, 
                IDS_SNI_WRONG_WTX_REPLY
                );
            TestCheck(
                pCResultFile->Wtx.Result == 0, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->Wtx.Result
                );
            TestEnd();

            //
            // Check resync. read result
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_RESYNC_READ_RESULT);
            TestCheck(
                pCResultFile->ResyncRead.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                (pCResultFile->ResyncRead.Result & 0x01) == 0, 
                IDS_SNI_NO_RESYNC_REQ
                );
            TestCheck(
                pCResultFile->ResyncRead.Result == 0, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->ResyncRead.Result
                );
            TestEnd();

            //
            // Check resync. write result
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_RESYNC_WRITE_RESULT);
            TestCheck(
                pCResultFile->ResyncWrite.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                (pCResultFile->ResyncWrite.Result & 0x01) == 0, 
                IDS_SNI_NO_RESYNC_REQ
                );
            TestCheck(
                (pCResultFile->ResyncWrite.Result & 0x02) == 0, 
                IDS_SNI_BAD_DATA
                );
            TestCheck(
                pCResultFile->ResyncWrite.Result == 0, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->ResyncWrite.Result
                );
            TestEnd();

            //
            // Sequence number result
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_SEQNUM_RESULT);
            TestCheck(
                pCResultFile->ResyncRead.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                (pCResultFile->Seqnum.Result & 0x01) == 0, 
                IDS_SNI_NO_RESYNC_REQ
                );
            TestCheck(
                (pCResultFile->Seqnum.Result & 0x02) == 0, 
                IDS_SNI_BAD_DATA
                );
            TestCheck(
                pCResultFile->Seqnum.Result == 0, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->Seqnum.Result
                );
            TestEnd();

            //
            // IFSC Request
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_IFSC_REQUEST);
            TestCheck(
                pCResultFile->IfscRequest.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                (pCResultFile->IfscRequest.Result & 0x01) == 0, 
                IDS_SNI_NO_IFSC_REPLY
                );
            TestCheck(
                (pCResultFile->IfscRequest.Result & 0x02) == 0, 
                IDS_SNI_BAD_DATA
                );
            TestCheck(
                (pCResultFile->IfscRequest.Result & 0x04) == 0, 
                IDS_SNI_BAD_SIZE_BEFORE_IFSC_REQ,
                pCResultFile->IfscRequest.Result
                );
            TestCheck(
                (pCResultFile->IfscRequest.Result & 0x08) == 0, 
                IDS_SNI_BAD_SIZE_AFTER_IFSC_REQ,
                pCResultFile->IfscRequest.Result
                );
            TestCheck(
                pCResultFile->IfscRequest.Result == 0x00, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->IfscRequest.Result
                );
            TestEnd();

            //
            // IFSD Request
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_IFSD_REQUEST);
            TestCheck(
                pCResultFile->IfsdRequest.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                (pCResultFile->IfsdRequest.Result & 0x01) == 0, 
                IDS_SNI_NO_IFSC_REQ
                );
            TestCheck(
                pCResultFile->IfsdRequest.Result == 0x00, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->IfsdRequest.Result
                );
            TestEnd();

            //
            // Timeout
            //
            g_uiFriendlyTest3++;
            TestStart(IDS_SNI_FORCED_TIMEOUT_RESULT);
            TestCheck(
                pCResultFile->Timeout.ResetCount == pCResultFile->CardResetCount,
                IDS_SNI_NO_TEST
                );
            TestCheck(
                pCResultFile->Timeout.Result == 0, 
                IDS_SNI_FAILED_ERROR,
                pCResultFile->Timeout.Result
                );
            TestEnd();
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
    in_CCardProvider.SetCardName(L"Infineon Technologies PC/SC Compliance Test Card");

    // Set ATR of our card
    in_CCardProvider.SetAtr(
        (PBYTE) "\x3b\xef\x00\x00\x81\x31\x20\x49\x00\x5c\x50\x43\x54\x10\x27\xf8\xd2\x76\x00\x00\x38\x33\x00\x4d", 
        24
        );
}

