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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:
     dttests.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Abstract:
          This Module Contains Tests that verify the Transmit /Recieve Capability of the Serial port
Author:
        shivss

Notes:
--*/

#define __THIS_FILE__ TEXT("DtTests.cpp")

#include "PSerial.h"
#include "GSerial.h"
#include <stdio.h>
#include "Comm.h"
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#include<math.h>
#endif

#ifdef UNDER_CE
#define HIGHER_PRIORITY    (-1)
#else
#define HIGHER_PRIORITY    1
#endif

#define TEST_TIME  5000 //5Sec 
#define MAX_BUFFER 1024*1024*10//70 MB ( For 5 Seconds with MAX 100Mbps)

BOOL g_bComplete                     = FALSE;
CommPort *   g_hCommPort           = NULL;






/* Tx Thread */

DWORD TxThread( LPVOID lpParam)
{
    DWORD dwResult = TPR_PASS; 
    DWORD dwLength = 0 ;
    BOOL bRtn = FALSE;
    LPVOID lpBuffer;
    DWORD dwStartTicks                = 0;
    DWORD dwStopTicks                = 0;
    DWORD dwTotalTicks                = 0;
    DWORD dwNumOfBytesTransferred    = 0;
    DWORD dwIntegrity                = 0;
    DWORD dwIterations                = 0;

             
    if(lpParam)
        dwLength  = *(DWORD*)lpParam;
                       
          
    //Allocate the Buffer of Length passed by the user

    lpBuffer = malloc(dwLength);
    if(lpBuffer == NULL)
    {
        g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
        bRtn = TRUE;
        dwResult = TPR_SKIP;
        goto TxExit;
    }

    //Set the Memory Elements to  the Size of the Buffer 
    memset(lpBuffer,(BYTE)(dwLength),dwLength);

    g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:Length of the transfer %d "),__THIS_FILE__, __LINE__,dwLength);
    g_pKato->Log( LOG_DETAIL,TEXT("TxThread:Start Write"));
      
    //Get the Start Time 
    dwStartTicks = GetTickCount();
  
    //Tranfer For 5 Seconds
    while (!g_bComplete)
    {
        bRtn = WriteFile(g_hCommPort,lpBuffer, dwLength,&dwNumOfBytesTransferred, NULL);
        if (FALSE == bRtn  || dwNumOfBytesTransferred != dwLength) //We did not write any data
        {
            // Ignore write failures
            // Test will still fail if the verification the blob integrity check fails
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to send data Expected to Send %u Bytes Actually Sent %u Bytes:Error = %d")
                                      ,__THIS_FILE__, __LINE__,dwLength,dwNumOfBytesTransferred,GetLastError() );
            
        }
        dwNumOfBytesTransferred = 0;
        dwIterations++;

    }

    //Get the End Time 
    dwStopTicks  = GetTickCount();

    g_pKato->Log( LOG_DETAIL,TEXT("TxThread:Number of Iteration = %d"),dwIterations);
    g_pKato->Log( LOG_DETAIL,TEXT("TxThread:End Write"));

    //Calculate the total Ticks
    dwTotalTicks = dwStopTicks - dwStartTicks;

    //Sleep Till the Data is Read
    Sleep(TEST_TIME*3/2);



    dwNumOfBytesTransferred = 0;

    g_pKato->Log( LOG_DETAIL,TEXT("TxThread:Start Integrity Check"));

    //Read Back Data Integrity Result and then Output the Transfer Time Value 
    bRtn = ReadFile(g_hCommPort,&dwIntegrity,sizeof(DWORD),&dwNumOfBytesTransferred, NULL);
    if (FALSE == bRtn || dwNumOfBytesTransferred != sizeof(DWORD) ||!(dwIntegrity &DATA_INTEGRITY_PASS)) //We did not Read any data
    {
        g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Integrity Check Failed"),__THIS_FILE__, __LINE__);
        dwResult = TPR_FAIL;
    }

    if((dwIntegrity&DATA_INTEGRITY_PASS)&& (dwTotalTicks!= 0 )&& (dwIterations !=0))
    {
        //Calculate the Transfer Time 
        g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:DataXmitTest, Result (%d Transfer/%ld Ticks)= (Average TX Transfer Time) %ld /per second"),
             __THIS_FILE__, __LINE__,dwIterations,dwTotalTicks,((dwIterations *1000)/dwTotalTicks));
    }

    g_pKato->Log( LOG_DETAIL,TEXT("TxThread:End Integrity Check"));

TxExit:
    if(lpBuffer)
        free(lpBuffer);
    

    return dwResult;
}


/* Tx Thread */
DWORD RxThread( LPVOID lpParam)
{
    DWORD dwResult = TPR_PASS;
    LPVOID lpBuffer;
    BOOL bRtn = FALSE;
    DWORD dwNumOfBytesTransferred    = 0;
    DWORD index ;
    DWORD dwIntegrity = DATA_INTEGRITY_FAIL;
    DWORD dwStartTicks = 0;
    DWORD dwStopTicks  = 0;
    DWORD dwTotalTicks = 0;
    DWORD dwIterations = 0 ;
    BOOL bRead = FALSE;
    DWORD dwLength = 0 ;

    if(lpParam)
        dwLength = *(DWORD*)lpParam;
    

    //Slave
    lpBuffer = malloc(MAX_BUFFER);
    if (lpBuffer == NULL)
    {
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
            bRtn = TRUE;
            dwResult = TPR_SKIP;
            goto RxExit;
    }

    g_pKato->Log( LOG_DETAIL,TEXT("RxThread:Start Read"));

    while(!g_bComplete)
    {
        if(!bRead)
        {

            dwStartTicks = GetTickCount();
            bRtn = ReadFile(g_hCommPort,lpBuffer, MAX_BUFFER,&dwNumOfBytesTransferred, NULL);
            if (FALSE == bRtn||dwNumOfBytesTransferred==0 ) //We did not Read any data
            {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Read data  Error = %d")
                                      ,__THIS_FILE__, __LINE__,GetLastError());
                dwResult = TPR_FAIL;
            }

            dwStopTicks  = GetTickCount();
            bRead = TRUE;
        }
    }
        
    g_pKato->Log( LOG_DETAIL,TEXT("RxThread:End Read"));
    g_pKato->Log( LOG_DETAIL,TEXT("RxThread: Read %d Bytes"),dwNumOfBytesTransferred);

    //Calculate the total Ticks
    dwTotalTicks = dwStopTicks - dwStartTicks;
    dwIterations = dwNumOfBytesTransferred/dwLength;

    if(dwResult == TPR_PASS)
    {
        //Verify Data
        LPBYTE lpByte = (LPBYTE) lpBuffer;
        for( index = 0; index <dwNumOfBytesTransferred; index++)
        {
            if(*lpByte != (BYTE)dwLength)
            {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Mismatch Expecting %u Got %u ") ,__THIS_FILE__, __LINE__,dwLength,*lpByte);
                ASSERT(0);
                dwResult = TPR_FAIL;
                break;
            }
            lpByte++;
        }

        if(dwNumOfBytesTransferred == index)
        {
                   dwIntegrity = DATA_INTEGRITY_PASS;
            g_pKato->Log( LOG_DETAIL,TEXT("RxThread:Data  Integrity Check PASSED!!"));

        }
        else
            g_pKato->Log( LOG_DETAIL,TEXT("RxThread:Data  Integrity Check FAILED!!"));    
    }

    dwNumOfBytesTransferred = 0;
           
    //Write Back the Data Integrity Result to the Master
    bRtn = WriteFile(g_hCommPort,&dwIntegrity,sizeof(DWORD),&dwNumOfBytesTransferred, NULL);
    if (FALSE == bRtn  || dwNumOfBytesTransferred != sizeof(DWORD)) //We did not write any data
    {
        g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Obtain Data Integrity Result"),__THIS_FILE__, __LINE__);
        dwResult = TPR_FAIL;
        goto  RxExit;
    }

    if(dwIntegrity & DATA_INTEGRITY_PASS)
    {
        //Calculate the Transfer Time 
        g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:DataXmitTest, Result (%d Transfer/%ld Ticks)= (Average RX Transfer Time) %ld /per second"),
                     __THIS_FILE__, __LINE__,dwIterations,dwTotalTicks,((dwIterations *1000)/dwTotalTicks));
    }

 RxExit:
    if(lpBuffer)
        free(lpBuffer);

    return dwResult;
}

/*

DataXmitTest:
Transmit  Data with Different Buffer Length .
 
Arguments:
TUX standard arguments.
 
Return Value:
TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
TPR_EXECUTE: for TPM_EXECUTE
TPR_NOT_HANDLED: for all other messages.
 
Notes:

*/
TESTPROCAPI DataXmitTest(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE)
{
    BOOL    bRtn = FALSE;
    DWORD   dwResult = TPR_PASS;
    HANDLE hThread = NULL;

    DWORD dwLength=lpFTE->dwUserData;
    if( uMsg == TPM_QUERY_THREAD_COUNT )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    } 
    else if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    } // end if else if

    if (dwLength<=0 || dwLength>=MAX_BUFFER)
        dwLength=MAX_BUFFER;

    /* --------------------------------------------------------------------
        Sync the begining of the test.
    -------------------------------------------------------------------- */
    g_hCommPort = CreateCommObject(g_bCommDriver);
    FUNCTION_ERROR( g_hCommPort == NULL, return FALSE );
    g_hCommPort -> CreateFile( g_lpszCommPort, 
                               GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL );

    bRtn = BeginTestSync( g_hCommPort, lpFTE->dwUniqueID );
    DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

    bRtn = SetupDefaultPort( g_hCommPort );
    COMM_ERROR( g_hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );
    DCB mDCB;
    bRtn = GetCommState(g_hCommPort,  &mDCB);
    COMM_ERROR( g_hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    //Enable CTS-RTS Handshaking.
    mDCB.BaudRate     = CBR_38400;
    if(!g_fDisableFlow)
    {
        mDCB.fOutxCtsFlow = TRUE;
        mDCB.fRtsControl  = RTS_CONTROL_HANDSHAKE;
    }
    bRtn=SetCommState(g_hCommPort,&mDCB);
    COMM_ERROR( g_hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Xmiting Speed at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );

    // Initial Timeout
    COMMTIMEOUTS    cto;
    cto.ReadIntervalTimeout         =TEST_TIME;
    cto.ReadTotalTimeoutMultiplier  =  0;
    cto.ReadTotalTimeoutConstant    =   2*TEST_TIME;      
    cto.WriteTotalTimeoutMultiplier =   0; 
    cto.WriteTotalTimeoutConstant   =   TEST_TIME; 

    bRtn = SetCommTimeouts( g_hCommPort, &cto );
    COMM_ERROR( g_hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

    if(g_hCommPort)
    {
          // If the test sent the comm port make sure it is clear.
         if(!PurgeComm( g_hCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ))
             {
                     g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:PurgeComm Failed "),__THIS_FILE__, __LINE__);
            dwResult = TPR_SKIP;

             }
                          
    }

    if( g_fMaster )
    {
        g_bComplete = FALSE;
        hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&TxThread ,(LPVOID)&dwLength,0,NULL);
        if(hThread == NULL)
        {
            goto DXTCleanup;
        }

        //Sleep for the Test Time 
        Sleep(TEST_TIME);
        g_bComplete = TRUE;
    }
    else
    {
        //Let the master start
        Sleep(TEST_TIME/5);

        g_bComplete = FALSE;
        hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&RxThread ,(LPVOID)&dwLength,0,NULL);
        if(hThread == NULL)
        {
            goto DXTCleanup;
        }

        Sleep(TEST_TIME*2);
        g_bComplete = TRUE;
    }

    if (hThread)
    {
        WaitForSingleObject(hThread, INFINITE);
    }

    GetExitCodeThread(hThread,&dwResult);

    bRtn=TRUE;
    DXTCleanup:
    if( FALSE == bRtn)
        dwResult = TPR_ABORT;

    /* --------------------------------------------------------------------
    Sync the end of a test of the test.
    -------------------------------------------------------------------- */
    dwResult = EndTestSync(g_hCommPort, lpFTE->dwUniqueID, dwResult );
    if( g_hCommPort )
    {
        delete g_hCommPort;
        FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
    }

    return dwResult;
} 


/*

DataReceiveTest:
Receive  Data with Different Buffer Length .
 
Arguments:
TUX standard arguments.
 
Return Value:
    TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
    TPR_EXECUTE: for TPM_EXECUTE
    TPR_NOT_HANDLED: for all other messages.
 
Notes:

*/
TESTPROCAPI DataReceiveTest( UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BOOL   bRtn     = FALSE;
    DWORD  dwResult = TPR_PASS;
    HANDLE hThread  = NULL;

    DWORD dwLength=lpFTE->dwUserData;
    if( uMsg == TPM_QUERY_THREAD_COUNT )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    } 
    else if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    } // end if else if

    if (dwLength<=0 || dwLength>=MAX_BUFFER)
        dwLength=MAX_BUFFER;

    /* --------------------------------------------------------------------
        Sync the begining of the test.
    -------------------------------------------------------------------- */
    g_hCommPort = CreateCommObject(g_bCommDriver);
    FUNCTION_ERROR( g_hCommPort == NULL, return FALSE );
    g_hCommPort -> CreateFile( g_lpszCommPort, 
                               GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL );

    bRtn = BeginTestSync( g_hCommPort, lpFTE->dwUniqueID );
    DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

    bRtn = SetupDefaultPort( g_hCommPort );
    COMM_ERROR( g_hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );
    DCB mDCB;
    bRtn = GetCommState(g_hCommPort,  &mDCB);
    COMM_ERROR( g_hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    //Enable CTS-RTS Handshaking.
    mDCB.BaudRate     = CBR_38400;
    if(!g_fDisableFlow)
    {
        mDCB.fOutxCtsFlow = TRUE;
        mDCB.fRtsControl  = RTS_CONTROL_HANDSHAKE;
    }
    bRtn=SetCommState(g_hCommPort,&mDCB);
    COMM_ERROR( g_hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    g_pKato->Log(LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Xmiting Speed at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );

    // Initial Timeout
    COMMTIMEOUTS    cto;
    cto.ReadIntervalTimeout         = TEST_TIME;
    cto.ReadTotalTimeoutMultiplier  = 0;
    cto.ReadTotalTimeoutConstant    = 2*TEST_TIME;      
    cto.WriteTotalTimeoutMultiplier = 0; 
    cto.WriteTotalTimeoutConstant   = TEST_TIME; 

    bRtn = SetCommTimeouts( g_hCommPort, &cto );
    COMM_ERROR( g_hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

    if(g_hCommPort)
    {
        // If the test sent the comm port make sure it is clear.
        if(!PurgeComm( g_hCommPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ))
        {
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:PurgeComm Failed "),__THIS_FILE__, __LINE__);
            dwResult = TPR_SKIP;
        }
    }

    if(g_fMaster )
    {
        g_bComplete = FALSE;
        hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&RxThread ,(LPVOID)&dwLength,0,NULL);
        if(hThread == NULL)
        {
            goto DXTCleanup;
        }

        Sleep(TEST_TIME*2);
        g_bComplete = TRUE;
    }
    else
    {
        //Let the master start
        Sleep(TEST_TIME/5);

        g_bComplete = FALSE;
        hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&TxThread ,(LPVOID)&dwLength,0,NULL);
        if(hThread == NULL)
        {
            goto DXTCleanup;
        }

        //Sleep for the Test Time 
        Sleep(TEST_TIME);
        g_bComplete = TRUE;
    }

    //Wait for the thread to exit
    if (hThread)
    {
        WaitForSingleObject(hThread, INFINITE);
    }
    
    GetExitCodeThread(hThread,&dwResult);

    bRtn=TRUE;
    DXTCleanup:
    if( FALSE == bRtn)
        dwResult = TPR_ABORT;

    /* --------------------------------------------------------------------
    Sync the end of a test of the test.
    -------------------------------------------------------------------- */
    dwResult = EndTestSync(g_hCommPort, lpFTE->dwUniqueID, dwResult );

    if( g_hCommPort )
    {
        delete g_hCommPort;
        FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
    }

    return dwResult;
} 
