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

    axaltoeg.cpp

Abstract:

    This is a plug-in for the smart card driver test suite.
    This plug-in is smart card dependent

    Axalto 32K e-Gate card, nominal operations

Revision History :

    Nov. 1997 - initial version

--*/


#ifndef UNDER_CE
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

#include <stdarg.h> 

#include "ifdtest.h"
#include "ifdguid.h"
#include "axaltoegrc.h"

void 
TestCardEntry(
    class CCardProvider& in_CCardProvider
    );
//
// Create a card provider object
// Note: all global varibales and all functions have to be static
//
static class CCardProvider TestCard(TestCardEntry);

//Pauses for a specified number of milliseconds. 
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
    g_uiFriendlyTest1 = 3;
    g_uiFriendlyTest2 = 0;
    g_uiFriendlyTest3 = 0;
    // Try to set INCORRECT protocol T=1
    g_uiFriendlyTest3++;
    TestStart(IDS_AXALTO_SETBADPROTOCOL );
    
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T1);

    // The test MUST fail with the incorrect protocol
    TEST_CHECK_NOT_SUPPORTED(TestLoadStringResource(IDS_AXALTO_SETPROTOCOLFAILED).GetBuffer(0), l_lResult);
    
    TestEnd();
    
    // Now set the correct protocol
    g_uiFriendlyTest3++;
    TestStart(IDS_AXALTO_SETPROTOCOLT0);
    
    l_lResult = in_CReader.SetProtocol(SCARD_PROTOCOL_T0);

    TEST_CHECK_SUCCESS(TestLoadStringResource(IDS_AXALTO_SETPROTOCOLT0).GetBuffer(0), l_lResult);
    
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
    ULONG l_lResult, l_uResultLength, l_uIndex;
    PBYTE l_pbResult;
    BYTE l_rgbBuffer[512];
    g_uiFriendlyTest1 = 3;
    g_uiFriendlyTest2 = in_CCardProvider.GetTestNo();
    g_uiFriendlyTest3 = 0;
    switch (in_CCardProvider.GetTestNo()) {
    
        case 1:
        {
                // auth to the card using the transport key
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_INITCARD  );
                
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\x2a\x00\x01\x08"
                         "\x2c\x15\xe5\x26"
                         "\xe9\x3e\x8a\x19",
                13,
                &l_pbResult,
                &l_uResultLength
                );
                
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );

                if ((l_pbResult[0] != 0x90) || (l_pbResult[1] != 0x00)) goto INITEXIT;
                
                // select the MF
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xc0\xa4\x00\x00\x02"
                         "\x3f\x00",
                7,
                &l_pbResult,
                &l_uResultLength
                );
                
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], 0, 0x61, 0x00,
                    NULL, NULL, NULL
                    );

                if ((l_pbResult[0] != 0x61) ) goto INITEXIT;
                
                // Delete the RSA public key file if it exists
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x10\x12",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x10\x12",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x6a, 0x82,
                    NULL, NULL, NULL
                    );
                    
                if ((l_pbResult[0] != 0x6a) || (l_pbResult[1] != 0x82)) goto INITEXIT;
                
                // Delete the RSA private key file if it exists
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x00\x12",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x00\x12",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x6a, 0x82,
                    NULL, NULL, NULL
                    );
                    
                if ((l_pbResult[0] != 0x6a) || (l_pbResult[1] != 0x82)) goto INITEXIT;
                
                // Delete the PIN file if it exists
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x00\x00",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x00\x00",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x6a, 0x82,
                    NULL, NULL, NULL
                    );
                
                
                if ((l_pbResult[0] != 0x6a) || (l_pbResult[1] != 0x82)) goto INITEXIT;
                
                // Delete old test file 0055 if it exists
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x00\x55",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe4\x00\x00\x02"
                         "\x00\x55",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x6a, 0x82,
                    NULL, NULL, NULL
                    );
                    
                if ((l_pbResult[0] != 0x6a) || (l_pbResult[1] != 0x82)) goto INITEXIT;


                // Create new test file 0055
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xf0\xe0\x00\x00\x10"
                         "\xff\xff"
                         "\x00\x80"
                         "\x00\x55"
                         "\x01"
                         "\xc0\x00\x00\x00"
                         "\x01"
                         "\x03"
                         "\x00\x00\x00",
                        21,
                        &l_pbResult,
                        &l_uResultLength
                        );
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );

                if ((l_pbResult[0] != 0x90) || (l_pbResult[1] != 0x00)) goto INITEXIT;

                // Select the test file in preparation for test operations
                l_lResult = in_CReader.Transmit(
                (PUCHAR) "\xc0\xa4\x00\x00\x02"
                         "\x00\x55",
                        7,
                        &l_pbResult,
                        &l_uResultLength
                        );
                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], 0, 0x61, 0x00,
                    NULL, NULL, NULL
                    );
                                       
INITEXIT:                
                TestEnd();
                break;
        } // end case 1
        
        case 2: 
        {
            //
            // Write 
            //
                ULONG l_auNumBytes[] = { 1 , 25, 75, 128 }; //, 50, 75, 100, 125 };
                
                // write pattern to buffer;
                for (l_uIndex = 0; l_uIndex < 128; l_uIndex++) {

                    l_rgbBuffer[l_uIndex + 5] = (UCHAR) (l_uIndex);
                }
                
                for (ULONG l_uTest = 0; 
                     l_uTest < sizeof(l_auNumBytes) / sizeof(l_auNumBytes[0]); 
                     l_uTest++) {

                    ULONG l_uNumBytes = l_auNumBytes[l_uTest];

                    g_uiFriendlyTest3++;
                    TestStart(IDS_AXALTO_UPDATEBINARY, l_uNumBytes);
                        
                    // Tpdu for write binary
                    memcpy(l_rgbBuffer, "\xc0\xd6\x00\x00", 4);
                        
                    // Append number of bytes (note: the buffer contains the pattern already)
                    l_rgbBuffer[4] = (UCHAR) l_uNumBytes;

                    sleep( (clock_t) 1 * (CLOCKS_PER_SEC / 4) );
                                        
                    l_lResult = in_CReader.Transmit(
                        l_rgbBuffer,
                        5 + l_uNumBytes,
                        &l_pbResult,
                        &l_uResultLength
                        );
                        
                        TestCheck(
                            l_lResult, L"==", ERROR_SUCCESS,
                            l_uResultLength, 2,
                            l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                            NULL, NULL, NULL
                            );

                        TestEnd();

                    }            
                    break;             
            }// end case 2

        case 3: 
        {
             
                // write pattern to buffer;
                for (l_uIndex = 0; l_uIndex < 128; l_uIndex++) {

                    l_rgbBuffer[l_uIndex + 5] = (UCHAR) (l_uIndex);
                }
                
                // Test read of 256 bytes
                ULONG l_uNumBytes = 128;

                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_READBINARY , l_uNumBytes);

                // tpdu for read binary
                memcpy(l_rgbBuffer, "\xc0\xb0\x00\x00\x80", 5);
                  
                l_lResult = in_CReader.Transmit(
                    l_rgbBuffer,
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

                    TestEnd();

                    break;
            } // end case 3
        
        case 4: 
        {
                // Cleanup: select and delete the test file 0055

                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_CLEAN1  );

                    l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xa4\x00\x00\x02"
                             "\x3f\x00",
                    7,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                break;
            }  // end case 4
        
        case 5: 
        {          

                // select the MF
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_SELECTMF  );
                
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xa4\x00\x00\x02"
                             "\x3f\x00",
                    7,
                    &l_pbResult,
                    &l_uResultLength
                    );
                
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_CREATEPINFILE  );

               // create the PIN file
               l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xf0\xe0\x00\x00\x10"
                             "\xff\xff"
                             "\x00\x17"
                             "\x00\x00"
                             "\x01"
                             "\x00\xf4\xff\x44"
                             "\x01\x03"
                             "\xf1\xff\x11",
                    21,
                    &l_pbResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );
                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_SETPINS  );


                // set PIN = 00000000
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xd6\x00\x00\x17"
                             "\x01\xff\xff"
                             "\x30\x30\x30\x30\x30\x30\x30\x30"
                             "\x0a\x0a"
                             "\x31\x31\x31\x31\x31\x31\x31\x31"
                             "\x0a\x0a",
                    28,
                    &l_pbResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_SELECTMF  );


                // select the MF again
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xa4\x00\x00\x02"
                             "\x3f\x00",
                    7,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_CREATEPRIVKEYFILE  );

                // Create the private key file
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xf0\xe0\x00\x00\x10"
                             "\xff\xff"
                             "\x03\x20"
                             "\x00\x12"
                             "\x01"
                             "\x00\xf1\xff\x44"
                             "\x01\x03"
                             "\x00\x10\x11",
                    21,
                    &l_pbResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_SELECTMF  );
                    
                // select the MF again
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xa4\x00\x00\x02"
                             "\x3f\x00",
                    7,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_CREATEPUBKEYFILE  );

                // Create the public key file
                // Create the private key file
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xf0\xe0\x00\x00\x10"
                             "\xff\xff"
                             "\x03\x20"
                             "\x10\x12"
                             "\x01"
                             "\x00\x01\xff\x44"
                             "\x01\x03"
                             "\x00\x10\x11",
                    21,
                    &l_pbResult,
                    &l_uResultLength
                    );

                TestCheck(
                    l_lResult, L"==", ERROR_SUCCESS,
                    l_uResultLength, 2,
                    l_pbResult[0], l_pbResult[1], 0x90, 0x00,
                    NULL, NULL, NULL
                    );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_SELECTMF  );
                                
                // Select the MF
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xa4\x00\x00\x02"
                             "\x3f\x00",
                    7,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();
                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_SELECTKEYFILE  );

                // Select the private key file
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xa4\x00\x00\x02"
                             "\x00\x12",
                    7,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_AUTHUSER);
                
                // authenticate as user
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\x20\x00\x01\x08"
                             "\x30\x30\x30\x30\x30\x30\x30\x30",
                    13,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x90, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_KG_KEYGEN  );

                // generate the RSA key pair
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xf0\x46\x00\x00\x04"
                             "\x01\x00\x01\x00",
                    9,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x90, 0x00,
                        NULL, NULL, NULL
                        );
                    
                TestEnd();

                
                break;
                //return IFDSTATUS_END;
            }  // end case 5
        
        case 6: 
        {            
                // Test read of 256 bytes
                // used to include a signature operation, but that doesn't add anything at this level
                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_HASHDATA  );
                
                // Hash a small piece of data
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\x04\x40\x00\x00\x10"
                             "\x01\x02\x03\x04"
                             "\x05\x06\x07\x08"
                             "\x09\x0a\x0b\x0c"
                             "\x0e\x0e\x0f\x10",
                    21,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
                TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 2,
                        l_pbResult[0], 0, 0x61, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                
                g_uiFriendlyTest3++;
                TestStart(IDS_AXALTO_GETHASH  );

                // get the hash data, 20 bytes
                l_lResult = in_CReader.Transmit(
                    (PUCHAR) "\xc0\xc0\x00\x00\x14",
                    5,
                    &l_pbResult,
                    &l_uResultLength
                    );
                    
               TestCheck(
                        l_lResult, L"==", ERROR_SUCCESS,
                        l_uResultLength, 22,
                        l_pbResult[20], 0, 0x90, 0x00,
                        NULL, NULL, NULL
                        );

                TestEnd();

                
                return IFDSTATUS_END;
            }  // end case 4
        
        default:
            return IFDSTATUS_FAILED;

    }    
    return IFDSTATUS_SUCCESS;
}    

void
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
    in_CCardProvider.SetCardName(L"PC/SC Compliance Test Card 3 (Axalto)");  //(L"Axalto 32K eGate");

    // Name of our card
    in_CCardProvider.SetAtr((PBYTE) "\x3b\x95\x18\x40\xff\x62\x01\x02\x01\x04", 10);
}

