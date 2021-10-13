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

    Athena.cpp

Abstract:

    This is a plug-in for the smart card driver test suite.
    This plug-in is smart card dependent

    Athena T0 inverse convention card

Revision History :

    Nov. 1997 - initial version

--*/

#ifndef UNDER_CE
#include <stdarg.h> 
#include <stdio.h>
#include <string.h>
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
#include "athena2rc.h"

#pragma warning (disable: 4211)
#define BYTES_PER_BLOCK 64

#define CHECKRESULT(s,a,b,x,y)  {if ((a != x) || (b != y)) {printf("   %s\n",s);TEST_END(); break;}}

void 
TestCardEntry(
    class CCardProvider& in_CCardProvider
    );
//
// Create a card provider object
// Note: all global varibales and all functions have to be static
//
static class CCardProvider TestCard(TestCardEntry);

static void 
sleep( 
    clock_t wait 
    )
{
#ifndef UNDER_CE
    clock_t goal;
    goal = wait + clock();
    while( goal > clock() )
        ;
#else
    Sleep(wait);
#endif
}

static ULONG
TestCardSetProtocol(
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
    g_uiFriendlyTest1 = 2;
    g_uiFriendlyTest2 = 0;
    g_uiFriendlyTest3 = 0;

    g_uiFriendlyTest3++;
    TestStart(IDS_ATHENA2_SETPROTOCOLT0);
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);
    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_ATHENA2_SETPROTOCOLFAILED).GetBuffer(0), l_lResult);
    TestEnd();

    if (l_lResult != ERROR_SUCCESS) {

        return IFDSTATUS_FAILED;
    }

    return IFDSTATUS_SUCCESS;
}

static 
ULONG
TestCardTest(
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
    ULONG l_lResult, l_uResultLength, l_uBlock, l_uIndex;
    PUCHAR l_pchResult;
    UCHAR l_rgchBuffer[512];
    g_uiFriendlyTest1 = 2;
    g_uiFriendlyTest2 = in_CCardProvider.GetTestNo();
    g_uiFriendlyTest3 = 0;

    switch (in_CCardProvider.GetTestNo()) {
    
        case 1: {

            ULONG l_uNumBytes = 256;
            // write some data to the test file using T=0
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_COLDRESET);
            l_lResult = in_CReader.ColdResetCard();
            TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_ATHENA2_SETPROTOCOLFAILED).GetBuffer(0), l_lResult);
            TestEnd();

            ULONG l_uState;
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_CHECKREADERSTATE);
            l_lResult = in_CReader.GetState(&l_uState);
            TEST_CHECK_SUCCESS(
                TestLoadStringResource(IDS_ATHENA2_IOCTLFAILED).GetBuffer(0), 
                l_lResult
                );
            TestCheck(
                l_uState == SCARD_NEGOTIABLE,
                IDS_ATHENA2_INVALIDREADERSTATE, 
                l_uState,
                SCARD_NEGOTIABLE
                );
            TEST_END();

            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_SETPROTOCOLT0);
            l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);
            TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_ATHENA2_SETPROTOCOLFAILED).GetBuffer(0), l_lResult);
            TestEnd();

            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_ERASECARDFILES);
            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xa4\x00\x0C\x00",
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                NULL, NULL, 0
                );

            if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0x0)) goto INITDONE;
            
            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xe4\x00\x00\x00",
                5,
                &l_pchResult,
                &l_uResultLength
                );

            // Create test file 0002
            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xe0\x01\x00\x08"
                         "\x85\x06\x00\x02\x00\x01\x04\x00",
                13,
                &l_pchResult,
                &l_uResultLength
                );
                
            if (l_pchResult[0] == 0x90)
            {
                // success on creating the file
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, 0
                    );
                    
            }
            else
            {
                // file already exists
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x6a, 0x89,
                    NULL, NULL, 0
                    );
            }
INITDONE:            
            TEST_END();
            
            // select a file
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_SELECTFILEEFPTS);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xa4\x00\x00\x02"
                         "\x00\x02",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            // allow any response of the form 61 xx (ref impl returned 61 09 - Athena returns 61 18)
            
            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], 0, 0x61, 0,
                NULL, NULL, 0
                );

            TEST_END();         

            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_WRITEBINARY, l_uNumBytes);

            for (l_uBlock = 0; l_uBlock < l_uNumBytes; l_uBlock += BYTES_PER_BLOCK) {
                 
                // apdu for write binary
                memcpy(l_rgchBuffer, "\x00\xd6\x00", 3);

                // offset within the file we want to write to
                l_rgchBuffer[3] = (UCHAR) l_uBlock;

                // Append number of bytes 
                l_rgchBuffer[4] = (UCHAR) BYTES_PER_BLOCK;

                // append pattern to buffer;
                for (l_uIndex = 0; l_uIndex < BYTES_PER_BLOCK; l_uIndex++) {

                    l_rgchBuffer[5 + l_uIndex] = (UCHAR) (l_uBlock + l_uIndex);
                }

                l_lResult = in_CReader.Transmit(
                    l_rgchBuffer,
                    5 + BYTES_PER_BLOCK,
                    &l_pchResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], l_pchResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );

                if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0)) break;
                
            }

            TEST_END();

            //
            // read the data back
            //

            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_READBINARY, l_uNumBytes);

            for (l_uBlock = 0; l_uBlock < l_uNumBytes; l_uBlock += BYTES_PER_BLOCK) {
                 
                // apdu for read binary 
                memcpy(l_rgchBuffer, "\x00\xb0\x00", 3);

                // offset within the file we want to read from
                l_rgchBuffer[3] = (UCHAR) l_uBlock;

                // Append number of bytes (note: the buffer contains the pattern already)
                l_rgchBuffer[4] = (UCHAR) BYTES_PER_BLOCK;

                l_lResult = in_CReader.Transmit(
                    l_rgchBuffer,
                    5,
                    &l_pchResult,
                    &l_uResultLength
                    );

                // append pattern to buffer;
                for (l_uIndex = 0; l_uIndex < BYTES_PER_BLOCK; l_uIndex++) {

                    l_rgchBuffer[l_uIndex] = (UCHAR) (l_uBlock + l_uIndex);
                }

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, (l_uNumBytes / 4) + 2,
                    l_pchResult[BYTES_PER_BLOCK], l_pchResult[BYTES_PER_BLOCK + 1], 0x90, 0x00,
                    l_pchResult, l_rgchBuffer, BYTES_PER_BLOCK
                    );
                    
                if ((l_pchResult[0] != 0x90) || (l_pchResult[1] != 0)) break;
                
            }
            
            TEST_END();

            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_WRITE255);

            // apdu for read binary 
            memcpy(l_rgchBuffer, "\x00\xD6\x00\x00\xFF", 5);
            
            // append pattern to buffer;
            for (l_uIndex = 0; l_uIndex < 255; l_uIndex++) {

                l_rgchBuffer[l_uIndex + 5] = (UCHAR) (l_uBlock + l_uIndex);
            }

            l_lResult = in_CReader.Transmit(
                l_rgchBuffer,
                5 + 255,
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

            //return IFDSTATUS_END;                   // PUT AT END OF LAST TEST
        }
        __fallthrough;

        case 2: 
        {
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_BUFFERBOUNDARY);

            //
            // Check if the reader correctly determines that
            // our receive buffer is too small
            //
            in_CReader.SetReplyBufferSize(9);
            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\x84\x00\x00\x08",
                5,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult == ERROR_INSUFFICIENT_BUFFER,
                IDS_ATHENA2_SHOULD_FAIL_SMALL_BUFFER,
                l_lResult, 
                ERROR_INSUFFICIENT_BUFFER
                );

            TEST_END();
            in_CReader.SetReplyBufferSize(2048);

            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_SELECT77);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xa4\x00\x00\x08\x00",
                6,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_INVALID_PARAMETER,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, 0
                );
                
            TEST_END();
                          
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_SELECTNOFILE);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x00\xa4\x00\x00\x02\x00\x77",
                7,
                &l_pchResult,
                &l_uResultLength
                );

            TestCheck(
                l_lResult, L"==", ERROR_SUCCESS,
                l_uResultLength, 2,
                l_pchResult[0], l_pchResult[1], 0x6a, 0x82,     // these cards return 6A 82 instead of 94 04
                NULL, NULL, 0
                );
                
            TEST_END();
                          
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_COMMANDTIMEOUT);

            l_lResult = in_CReader.Transmit(
                (PUCHAR) "\x80\x00\x01\x00\x01\x11",
                6,
                &l_pchResult,
                &l_uResultLength
                );
            
            TestCheck(
                l_lResult, L"==", ERROR_SEM_TIMEOUT,
                0, 0,
                0, 0, 0, 0,
                NULL, NULL, 0
                );
                
            TEST_END();

            // clean card state for next test group
            g_uiFriendlyTest3++;
            TestStart(IDS_ATHENA2_SETPROTOCOLT0);
            l_lResult = in_CReader.ColdResetCard();
            l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);
            TestEnd();
        }
        __fallthrough;

        case 3: 
        {            // Test restart of work waiting time
            ULONG l_auDelay[] = { 1, 2, 5, 30 };

            for (ULONG l_uTest = 0; 
                 l_uTest < sizeof(l_auDelay) / sizeof(l_auDelay[0]); 
                 l_uTest++) 
            {

                ULONG l_uNumBytes = l_auDelay[l_uTest];
                g_uiFriendlyTest3++;
                TestStart(IDS_ATHENA2_RESTARTWORKWAITTIME, l_uNumBytes);

                // tpdu for echo binary
                memcpy(l_rgchBuffer, "\x80\x00\x03\x00\x02", 5);

                // Append number of bytes
                l_rgchBuffer[5] = (UCHAR)l_uNumBytes;
                l_rgchBuffer[6] = 1;
            
                sleep( (clock_t) 1 * (CLOCKS_PER_SEC / 2) );

                l_lResult = in_CReader.Transmit(
                    l_rgchBuffer,
                    7,
                    &l_pchResult,
                    &l_uResultLength
                );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pchResult[0], 0, 0x61, 0x00,
                    NULL, NULL, 0
                    );

                TEST_END();
            }
            return IFDSTATUS_END;                   // PUT AT END OF LAST TEST
        }

        default:
            return IFDSTATUS_FAILED;

    }    
}    

static void
TestCardEntry(
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
    in_CCardProvider.SetProtocol(TestCardSetProtocol);

    // Card test callback
    in_CCardProvider.SetCardTest(TestCardTest);

    // Name of our card
    in_CCardProvider.SetCardName(L"PC/SC Compliance Test Card 2 (Athena Inverse Convention)"); //(L"Athena T0 Inverse Convention Test Card");

    // ATR of our card - gm050913 - ATR changed by the supplier
    in_CCardProvider.SetAtr((PBYTE) "\x3f\x96\x18\x80\x01\x80\x51\x00\x61\x10\x30\x9f", 12);
}


