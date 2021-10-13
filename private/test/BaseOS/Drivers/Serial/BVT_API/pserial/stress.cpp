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

     stress.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
 
Module Name:
 
    E:\wince\private\qaos\drivers\serial\pserial\stress.cpp
 
Abstract:
 
    This file cantains the tests used to test the serial port.
 
Notes:
 
--*/


#define __THIS_FILE__ TEXT("stress.cpp")

#include "PSerial.h"
#include "GSerial.h"
#include <stdio.h>
#include "Comm.h"
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#endif

#ifdef UNDER_CE
#define HIGHER_PRIORITY    (-1)
#else
#define HIGHER_PRIORITY    1
#endif



#define TEST_TIME 30*1000        //30 in second
#define MAX_BUFFER 1024
#define STRESS_ACK  1000
#define MAX_ERROR_COUNT 3


BOOL g_bStressThreadTerminate = FALSE;
CommPort *   g_hStressCommPort           = NULL;


DWORD TxStressThread( LPVOID lpParam)
{
    DWORD dwResult = TPR_PASS; 
    BOOL bRtn = FALSE;
    BOOL bAck = FALSE;
    LPVOID lpBuffer;
    DWORD dwNumOfBytesTransferred = 0;
    DWORD dwIterations = 0;
    DWORD dwData = 0x00;
    DWORD dwAck = STRESS_ACK ; 
    DWORD dwRxAck = 0 ;
    DWORD dwLength = 0;
    DWORD dwErrorCount = 0;
   
    if(lpParam)
        dwLength = * (const DWORD*)lpParam;

    DWORD dwStressCount = g_StressCount;
    //Allocate the Buffer of Length passed by the user

    lpBuffer = malloc(dwLength);
    if(lpBuffer == NULL)
    {
        g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
        bRtn = TRUE;
        dwResult = TPR_SKIP;
        goto TxExit;
    }

    
    g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:Length of the transfer %d "),__THIS_FILE__, __LINE__,dwLength);


    //Transferring Data 
    g_pKato->Log( LOG_DETAIL,TEXT("TxStressThread:Start Write"));

    while(dwIterations < dwStressCount)
    {
        //Set the Memory Elements 
        memset(lpBuffer,(BYTE)dwData,dwLength);
                  
        //Write Data to the Slave
        g_pKato->Log( LOG_DETAIL,TEXT("TxStress Thread: Write Data Iteration = %d"),dwIterations+1);
        bRtn = WriteFile(g_hStressCommPort,lpBuffer, dwLength,&dwNumOfBytesTransferred, NULL);
        if (FALSE == bRtn  || dwNumOfBytesTransferred != dwLength) //We did not write any data
        {
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to send data Expected to Send %u Bytes Actually Sent %u Bytes:Error = %d")
                          ,__THIS_FILE__, __LINE__,dwLength,dwNumOfBytesTransferred,GetLastError() );
            dwResult = TPR_FAIL;
        }

        dwErrorCount =0;
        bAck = FALSE;

        //Allow 3 Retries
        while(dwErrorCount <3 )
        {
            //Wait For ACK 
            bRtn = ReadFile(g_hStressCommPort,&dwRxAck, sizeof(dwRxAck),&dwNumOfBytesTransferred, NULL);
            if (bRtn!=TRUE) //ACK failed
            {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:ACK Failed to receive Retry !!"),__THIS_FILE__, __LINE__);
                              dwErrorCount++;
                dwRxAck = 0;               
                continue;
            }
            else if(dwRxAck!=dwAck)
            {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:ACK Failed to receive:Expected %d Received %d Error = %d ")
                                            ,__THIS_FILE__, __LINE__,dwAck,dwRxAck,GetLastError());
                dwResult = FALSE;
                break;
            }

            if(dwNumOfBytesTransferred !=sizeof(dwAck))
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:ACK Failed:Expected %d Bytes  Received %d Bytes !!")
                              ,__THIS_FILE__, __LINE__,sizeof(dwAck),dwNumOfBytesTransferred);
            else
                bAck = TRUE;
            break;    
        }
        if(bAck)
            g_pKato->Log( LOG_DETAIL,TEXT("TxStressThread:Received ACK = %d "),dwRxAck );
        else
        {
            dwResult = TPR_FAIL;
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:ACK Failed Multiple retries "),__THIS_FILE__, __LINE__);
        }

           dwRxAck = 0 ;
           dwIterations++;
           dwAck ++ ;//Increment the ACK
           dwData++;//Increment the Memory Element
        }
                
         g_pKato->Log( LOG_DETAIL,TEXT("TxStressThread:End Write"));

TxExit:
    if(lpBuffer)
        free(lpBuffer);
    
    g_bStressThreadTerminate = TRUE;

    return dwResult;
}


/* Rx Thread */
DWORD RxStressThread( LPVOID lpParam)
{
    DWORD dwResult = TPR_PASS;
    LPVOID lpBuffer;
    LPBYTE lpByte = 0;
    BOOL bRtn = FALSE;
    DWORD dwNumOfBytesTransferred    = 0;
    DWORD index ;
    DWORD dwIterations = 0 ;
    DWORD dwStressCount = g_StressCount;
    DWORD dwData = 0x00;
    DWORD dwAck = STRESS_ACK;
    DWORD dwLength = 0;

    if(lpParam)
        dwLength = *(const DWORD*)lpParam;    
    

    //Slave
    lpBuffer = malloc(dwLength);
    if (lpBuffer == NULL)
    {
        g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
        bRtn = TRUE;
        dwResult = TPR_SKIP;
        goto RxExit;
    }

    memset(lpBuffer,0,dwLength);
            
    g_pKato->Log( LOG_DETAIL,TEXT("RxThread:Start Read"));

    while(dwIterations < dwStressCount)
    {

        //Read Data From the Master             
        g_pKato->Log( LOG_DETAIL,TEXT("RxStressThread:Read Data Iteration = %d"),dwIterations+1);

        bRtn = ReadFile(g_hStressCommPort,lpBuffer, dwLength,&dwNumOfBytesTransferred, NULL);
        if (bRtn!=TRUE||dwNumOfBytesTransferred == 0  ) //We did not Read any data
        {
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Read data Expected to Read %u Bytes Actually Read %u Bytes Error = %d")
                                  ,__THIS_FILE__, __LINE__,dwLength,dwNumOfBytesTransferred,GetLastError());
            dwResult = TPR_FAIL;
        }

         //Verify the Data        
        if(dwResult == TPR_PASS)
        {
            //Verify Data
            if(lpBuffer)
                lpByte = (LPBYTE) lpBuffer;

            ASSERT(dwLength>=dwNumOfBytesTransferred);

            for( index = 0 ;index <dwNumOfBytesTransferred;index++)
            {
                if(*lpByte++ != (BYTE)dwData)
                {
                    g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Mismatch Expecting %u Got %u ") ,__THIS_FILE__, __LINE__,dwData,*lpByte);
                    dwResult = TPR_FAIL;
                }
            }
        }

        //Send ACK 
        bRtn = WriteFile(g_hStressCommPort,&dwAck, sizeof(dwAck),&dwNumOfBytesTransferred, NULL);
        if (bRtn!=TRUE||dwNumOfBytesTransferred !=sizeof(dwAck) ) //ACK failed
        {
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to send ACK :Error = %d "),__THIS_FILE__, __LINE__,GetLastError());
            dwResult = TPR_FAIL;
        }
        else
            g_pKato->Log( LOG_DETAIL,TEXT("RxStressThread:Sent ACK "));

        //Increment the ACK             
        dwAck++; 
            
        memset(lpBuffer,0,dwLength);
        dwNumOfBytesTransferred=0;      
        dwIterations ++;    
        dwData++;
    }
                   
 RxExit:
    if(lpBuffer)
        free(lpBuffer);

    g_bStressThreadTerminate = TRUE;

    return dwResult;
}

/*++
 
SerialStressTest:
 
Arguments:
 
    TUX standard arguments.
 
Return Value:
 
    TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
    TPR_EXECUTE: for TPM_EXECUTE
    TPR_NOT_HANDLED: for all other messages.
 
Author:
 
    Uknown (unknown)
 
Notes:
 
    
 
--*/

TESTPROCAPI SerialStressTest( UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BOOL bRtn = FALSE;
    DWORD dwResult = TPR_PASS;
    HANDLE hThread = NULL;
    DWORD dwLength = 0 ;
    DWORD dwUserData = 0;

    if( uMsg == TPM_QUERY_THREAD_COUNT )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    } 
    else if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    } // end if else if

    if(lpFTE->dwUserData)
        dwUserData=lpFTE->dwUserData;
    
    
    /* --------------------------------------------------------------------
        Sync the begining of the test.
    -------------------------------------------------------------------- */
    g_hStressCommPort = CreateCommObject(g_bCommDriver);
    FUNCTION_ERROR( g_hStressCommPort == NULL, return FALSE );
    g_hStressCommPort -> CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

    bRtn = BeginTestSync( g_hStressCommPort, lpFTE->dwUniqueID );
    DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

    bRtn = SetupDefaultPort( g_hStressCommPort );
    COMM_ERROR( g_hStressCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );
    DCB mDCB;
    bRtn = GetCommState(g_hStressCommPort,  &mDCB);
    COMM_ERROR( g_hStressCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    switch (dwUserData) 
    {
        case 2:
            mDCB.BaudRate=CBR_19200;
            dwLength = MAX_BUFFER*2;
            break;
        case 3:
            mDCB.BaudRate=CBR_38400;
            dwLength = MAX_BUFFER*8;
            break;
        case 4:
            mDCB.BaudRate=CBR_57600;
            dwLength = MAX_BUFFER*16;
            break;
        case 5:
            mDCB.BaudRate=CBR_115200;
            dwLength = MAX_BUFFER*32;
            break;
        case 6:
            mDCB.BaudRate=CBR_128000;
            dwLength = MAX_BUFFER*32;
            break;

        case 1:     //interntionally full through
        default:
            mDCB.BaudRate=CBR_9600;
            dwLength = MAX_BUFFER;
            break;
    } 
 
    //Enable CTS-RTS Handshaking.
    if(!g_fDisableFlow)
    {
        mDCB.fOutxCtsFlow = TRUE;
        mDCB.fRtsControl  = RTS_CONTROL_HANDSHAKE;
    }
    bRtn=SetCommState(g_hStressCommPort,&mDCB);
    COMM_ERROR( g_hStressCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d:  Test Xmiting Speed at BufferLength= %d Bytes"), __THIS_FILE__, __LINE__, dwLength );

    // Initial Timeout
    COMMTIMEOUTS    cto;
    cto.ReadIntervalTimeout         =TEST_TIME;
    cto.ReadTotalTimeoutMultiplier  =   10;
    cto.ReadTotalTimeoutConstant    = dwUserData  *TEST_TIME;      
    cto.WriteTotalTimeoutMultiplier =   10; 
    cto.WriteTotalTimeoutConstant   =  dwUserData* TEST_TIME; 

    bRtn = SetCommTimeouts( g_hStressCommPort, &cto );
    COMM_ERROR( g_hStressCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

    if(g_hStressCommPort)
    {
        // If the test sent the comm port make sure it is clear.
        if(!PurgeComm( g_hStressCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ))
        {
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:PurgeComm Failed "),__THIS_FILE__, __LINE__);
            dwResult = TPR_SKIP;
        }
    }
    
    g_bStressThreadTerminate = FALSE;
    if( g_fMaster )
    {
        hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&TxStressThread ,(LPVOID)&dwLength,0,NULL);
        if(hThread == NULL)
        {
            goto DXTCleanup;
        }
    }
    else
    {
        //Let the master start
        Sleep(TEST_TIME/5);
        hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)&RxStressThread ,(LPVOID)&dwLength,0,NULL);
        if(hThread == NULL)
        {
            goto DXTCleanup;
        }
    }

    do
    {
        Sleep(30);
    } while(!g_bStressThreadTerminate);
    
    //Wait for the Thread to Exit 
    GetExitCodeThread(hThread,&dwResult);

    bRtn=TRUE;
    DXTCleanup:
    if( FALSE == bRtn)
        dwResult = TPR_ABORT;

    /* --------------------------------------------------------------------
    Sync the end of a test of the test.
    -------------------------------------------------------------------- */
    dwResult = EndTestSync(g_hStressCommPort, lpFTE->dwUniqueID, dwResult );

    if( g_hStressCommPort )
    {
        delete g_hStressCommPort;
        FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
    }

    return dwResult;
} 
