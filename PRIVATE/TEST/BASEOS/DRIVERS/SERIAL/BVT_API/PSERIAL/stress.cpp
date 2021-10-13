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
#define HIGHER_PRIORITY	(-1)
#else
#define HIGHER_PRIORITY	1
#endif



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
#define TEST_TIME 30        // in second
#define LENGTH_NUMBER 9
#define MAX_COUNT	16
#define LENGTH_OFFSET 0

DWORD dwStressIterations;


TESTPROCAPI SerialStressTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	if( uMsg == TPM_QUERY_THREAD_COUNT )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
		return SPR_HANDLED;
	}
	else if (uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	} // end if else if

    BOOL    bRtn     = FALSE;
    DWORD   dwResult = TPR_PASS, dwIndex=0,dwTestCount=0,dwTestLengthCount=0;
	DWORD	dwBaudRate = 0; //bits per sec
	CommPort* hCommPort   = NULL;
	DWORD dwNumOfBytesTransferred = 0;
	DWORD dwNumOfMilliSeconds = 0;
	DWORD dwNumOfBytes = 0;
	DWORD dwCount = 0;

    //Set the Stress Count
	dwStressIterations= g_StressCount;


	if(g_fBT)
	{
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Bluetooth does not support this test - skipping."),
			      __THIS_FILE__, __LINE__ );
		return TPR_SKIP;
	}
	
	if(!g_fStress)
	{
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Stress test is not enabled by default."),
			      __THIS_FILE__, __LINE__ );
	    g_pKato->Log( LOG_DETAIL, 
			      TEXT("Refer to the help menu for more information."),
			      __THIS_FILE__, __LINE__ );
		return TPR_SKIP;
	}

    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */
    hCommPort = CreateCommObject(g_bCommDriver);
    hCommPort->CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )
	
	bRtn = SetupDefaultPort( hCommPort );
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );

	DCB mDCB;
    mDCB.DCBlength=sizeof(DCB);
	bRtn = GetCommState(hCommPort,&mDCB);
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	switch(lpFTE->dwUserData) 
	{
		case 1:
		default:
			mDCB.BaudRate=CBR_9600;
			dwBaudRate = 9600;
			break;
		case 2:
			mDCB.BaudRate=CBR_19200;
			dwBaudRate = 19200;
			break;
		case 3:
			mDCB.BaudRate=CBR_38400;
			dwBaudRate = 38400;
			break;
		case 4:
			mDCB.BaudRate=CBR_57600;
			dwBaudRate = 57600;
			break;
		case 5:
			mDCB.BaudRate=CBR_115200;
			dwBaudRate = 115200;
			break;
	};

	//BaudRate is bit/sec and we want the number of bytes to transfer
	dwNumOfBytes = dwBaudRate * TEST_TIME /8; 

	// Enable CTS-RTS Handshaking.
	mDCB.fOutxCtsFlow = TRUE;
	mDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;

	bRtn=SetCommState(hCommPort,&mDCB);

	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	// Initial Timeout
	COMMTIMEOUTS    cto;
	cto.ReadIntervalTimeout         =   100;
	cto.ReadTotalTimeoutMultiplier  =   10;
	cto.ReadTotalTimeoutConstant    =   500; //large read timeout value
	cto.WriteTotalTimeoutMultiplier =   10; 
	cto.WriteTotalTimeoutConstant   =   100; 

	bRtn = SetCommTimeouts( hCommPort, &cto );
	COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

	//Allocate Buffer
	g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Expected to transfer %u bytes"), __THIS_FILE__, __LINE__ ,dwNumOfBytes);


	LPBYTE lpSrcBuffer = (LPBYTE) malloc(dwNumOfBytes);
	LPBYTE lpDstBuffer = (LPBYTE) malloc(dwNumOfBytes);
	
	if (lpSrcBuffer == NULL || lpDstBuffer == NULL)
	{
		g_pKato->Log( LOG_FAIL, 
			      TEXT("In %s @ line %d: Failed to Allocate memory for buffer"), __THIS_FILE__, __LINE__);
		dwResult = TPR_ABORT;
		goto DXTCleanup;
	}
	
    memset(lpSrcBuffer, 0,dwNumOfBytes); //clear the memory just in case
    memset(lpDstBuffer, 0,dwNumOfBytes); //clear the memory just in case
    
    LPBYTE lpTmpPtr = lpSrcBuffer;
    
    //Initialize the source buffer
    for (UINT i = 0; i <dwNumOfBytes; i++)
	{
	    *lpTmpPtr = i%256;
		lpTmpPtr++;
    }

    memcpy((void*) lpDstBuffer, (void*) lpSrcBuffer, dwNumOfBytes);


	g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Total Iterations for Stress is %d"), __THIS_FILE__, __LINE__ ,dwStressIterations);
		    
    while (TPR_PASS== dwResult && dwCount < dwStressIterations)
    {
       
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Begining test loop: %d"), __THIS_FILE__, __LINE__ ,dwCount+1);
		
	    if( g_fMaster )
	    {
		    bRtn = WriteFile(hCommPort,lpDstBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);

			//We did not write any data
    	    if (bRtn  == 0 || dwNumOfBytesTransferred != dwNumOfBytes) 
		    {
			   	g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to send data"), __THIS_FILE__, __LINE__ );
                g_pKato->Log( LOG_FAIL, 
			      TEXT("In %s @ line %d: ERROR: WriteFile Failed. Last Error was: %d"), __THIS_FILE__, __LINE__ ,GetLastError());
		        dwResult = TPR_FAIL;
			}

            //Get data back from slave
		    DWORD dwNumOfBytesReceived = 0;

		    bRtn = ReadFile(hCommPort,lpDstBuffer, dwNumOfBytes,&dwNumOfBytesReceived, NULL);
		    if (bRtn == 0 || dwNumOfBytes != dwNumOfBytesReceived)
		    {
			    g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Can not get reply from slave"), __THIS_FILE__, __LINE__ );
                g_pKato->Log( LOG_FAIL, 
			      TEXT("In %s @ line %d: ERROR: ReadFile Failed. Last Error was: %d"), __THIS_FILE__, __LINE__ ,GetLastError());
     		    dwResult = TPR_FAIL;
		    }
            
			g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Checking data received"), __THIS_FILE__, __LINE__);
		   
		    if (0 != memcmp( (void*)lpSrcBuffer, (void*)lpDstBuffer, dwNumOfBytes))
		    {
		         
				 g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:ERROR: Data integrity check failed at Master"), __THIS_FILE__, __LINE__ );
				 //We're not sure whether the Slave knows the test failed or not, we'll just send the invalid data back
				 //So the slave won't wait forever
		         WriteFile(hCommPort,lpDstBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);
		         dwResult = TPR_FAIL;
				 
			}
        }

	else // Slave
	{
		bRtn = ReadFile(hCommPort,lpDstBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);
		if (bRtn  == 0 || dwNumOfBytesTransferred != dwNumOfBytes) //We did not write any data
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to receive data"), __THIS_FILE__, __LINE__ );
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d: ERROR: ReadFile Failed. Last Error was: %d"), __THIS_FILE__, __LINE__ ,GetLastError());
			dwResult = TPR_FAIL;
		}
		    
		//Check the data
		g_pKato->Log( LOG_DETAIL, 
			TEXT("In %s @ line %d: Checking data received : Loop %d"), __THIS_FILE__, __LINE__,dwCount+1);
		
		if (0 != memcmp( (void*)lpSrcBuffer, (void*)lpDstBuffer, dwNumOfBytes))
		{
		   g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data integrity check failed at slave, sending invalid data back to master"), __THIS_FILE__, __LINE__ );
		   dwResult = TPR_FAIL;
	    }
		    
		DWORD dwNumOfBytesSent = 0;
		
		//Note:  We send the lpDstBuffer back to Master, so if there is any error in there, Master will fail too
		bRtn = WriteFile(hCommPort,lpDstBuffer, dwNumOfBytes,&dwNumOfBytesSent, NULL);
		//if we failed the data integrity test, don't bother checking for return value
		if (bRtn == false || dwNumOfBytes != dwNumOfBytesSent)
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Can not send reply to Master!"), __THIS_FILE__, __LINE__ );
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:WriteFile Failed. Last Error was: %d"), __THIS_FILE__, __LINE__ ,GetLastError());
			dwResult = TPR_FAIL;

		}
	    
	}
	    
		if (dwResult == TPR_PASS)
			g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d: PASSED: test loop: %d "), __THIS_FILE__, __LINE__,dwCount+1);
		else
			g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d: FAILED: test loop: %d "), __THIS_FILE__, __LINE__,dwCount+1);

        dwCount++;
    }
    
    
    DXTCleanup:
		if( FALSE == bRtn)
			dwResult = TPR_ABORT;

		if(hCommPort)
		{
			delete hCommPort ;
			hCommPort = NULL;
		}

		if (lpSrcBuffer)
		{
			free(lpSrcBuffer);
			lpSrcBuffer = NULL;
		}
		
		if (lpDstBuffer)
		{
			free(lpDstBuffer);
			lpDstBuffer = NULL;
		}

   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
} // end SerialPerfTest( ... ) 
