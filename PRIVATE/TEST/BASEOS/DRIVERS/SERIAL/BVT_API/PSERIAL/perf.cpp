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

     perf.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
 
Module Name:
 
	E:\wince\private\qaos\drivers\serial\pserial\perf.cpp
 
Abstract:
 
	This file cantains the tests used to test the serial port.
 
Notes:
 
--*/

#define __THIS_FILE__ TEXT("perf.cpp")

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
 
SerialPerfTest:
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 
Notes:
 
	
 
--*/
//Array of Time the test should run for mutiple of 30 seconds
#define TEST_TIME_ARRAY_SIZE  5
#define LENGTH_NUMBER 9
#define MAX_COUNT	16
#define LENGTH_OFFSET 0

#define DATA_INTEGRITY_FAIL 0
#define DATA_INTEGRITY_PASS 1



DWORD dwTestTime  ;


TESTPROCAPI SerialPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	BOOL    bRtn     = FALSE;
    DWORD   dwResult = TPR_PASS, dwIndex=0,dwTestCount=0,dwTestLengthCount=0;
	DWORD	dwBaudRate = 0;
	DWORD	dwStart, dwStop;
	double	measuredBaudRate = 0;
	CommPort* hCommPort   = NULL;
	DWORD dwNumOfBytesTransferred = 0;
	DWORD dwNumOfMilliSeconds = 0;
	DWORD dwNumOfBytes = 0;
	BYTE dataIntegrityResult = DATA_INTEGRITY_PASS;
	DWORD Index=0;
    LPVOID lpBuffer;
	dwTestTime = g_PerfTime;
	


	if( uMsg == TPM_QUERY_THREAD_COUNT )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
    	return SPR_HANDLED;
	}
	else if (uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	} // end if else if

	if(g_fBT)
	{
		
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Bluetooth does not support this test - skipping."),
			      __THIS_FILE__, __LINE__ );
		
		return TPR_SKIP;
	}
	
	if(!g_fPerf)
	{
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Performance test is not enabled by default."),
			      __THIS_FILE__, __LINE__ );

		g_pKato->Log( LOG_DETAIL, 
			      TEXT("Refer to the help menu for more information."),
			      __THIS_FILE__, __LINE__ );

		return TPR_SKIP;
	}

    
	/*--------------------------------------------------------------------
    	Sync the begining of the test.
      --------------------------------------------------------------------*/

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


	switch (lpFTE->dwUserData) 
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
	}

       	g_pKato->Log( LOG_DETAIL,
			         TEXT("In %s @ line %d:Duration of the Performance Test (Calculated for 100 /% Throughtput) = %d secs \n"),
					 __THIS_FILE__, __LINE__,dwTestTime );
       
	   
		dwNumOfBytes = dwBaudRate * dwTestTime/8; 
		
		//Enable CTS-RTS Handshaking.
		mDCB.fOutxCtsFlow = TRUE;
		mDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;

		bRtn=SetCommState(hCommPort,&mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

		//Initial Timeout
		COMMTIMEOUTS    cto;
		cto.ReadIntervalTimeout         =   50;
		cto.ReadTotalTimeoutMultiplier  =   100;  //100
		cto.ReadTotalTimeoutConstant    =   50000;//50000
		cto.WriteTotalTimeoutMultiplier =   10; 
		cto.WriteTotalTimeoutConstant   =   100; 

		bRtn = SetCommTimeouts( hCommPort, &cto );
		COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

        //In Some Cases the Data is corrupted when we do repeated Writes and repeated Reads 
		//Purge Data before every Iterations

		//Allocate Buffer
		g_pKato->Log( LOG_DETAIL,
			         TEXT("In %s @ line %d:Expected to transfer %u bytes \n"),
					 __THIS_FILE__, __LINE__,dwNumOfBytes );
       
		lpBuffer = malloc(dwNumOfBytes);

		if (lpBuffer == NULL)
		{
			g_pKato->Log( LOG_FAIL,
			         TEXT("In %s @ line %d:Failed to Allocate memory for buffer\n"), __THIS_FILE__, __LINE__);
			
			bRtn = TRUE;
			dwResult = TPR_SKIP;
			goto DXTCleanup;
		}
		
		memset(lpBuffer, 0,dwNumOfBytes); //clear the memory just in case

		if( g_fMaster )
		{

			//Instead of sleeping and wait for the slave to start, I'll use this time to initialize the array
			LPBYTE lpByte = (LPBYTE)lpBuffer;
			for (UINT i = 0; i <dwNumOfBytes; i++)
			{
				*lpByte = i%256;
				lpByte++;
			}

			g_pKato->Log( LOG_DETAIL, TEXT("COMM Port Handle is %d "),hCommPort);

			dwStart = GetTickCount();
			bRtn = WriteFile(hCommPort,lpBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);
			dwStop = GetTickCount();
			if (bRtn  == 0 || dwNumOfBytesTransferred != dwNumOfBytes) //We did not write any data
			{
					g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Failed to send data \n"),__THIS_FILE__, __LINE__);
					g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: WriteFile Failed. Last Error was: %d\n")
						                       ,__THIS_FILE__, __LINE__,GetLastError());
					
					g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Expected to transfer %u bytes \n")
						                       ,__THIS_FILE__, __LINE__,dwNumOfBytes);

					g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Actually sent %u bytes \n"),
												 __THIS_FILE__, __LINE__,dwNumOfBytesTransferred );
					dwResult = TPR_FAIL;
			}

			DWORD dwNumOfBytesReceived = 0;

			bRtn = ReadFile(hCommPort,&dataIntegrityResult, sizeof(dataIntegrityResult),&dwNumOfBytesReceived, NULL);
			if (bRtn == FALSE)
			{
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Can not get reply from slave! \n"),__THIS_FILE__, __LINE__);
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: ReadFile Failed. Last Error was: %d\n")
						                       ,__THIS_FILE__, __LINE__,GetLastError());
				dwResult = TPR_ABORT;
			}
            
			g_pKato->Log( LOG_DETAIL, TEXT("Data Intergiry result is %d "),dataIntegrityResult);

			if (dataIntegrityResult != DATA_INTEGRITY_PASS)
			{
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Data Integrity test failed \n"),__THIS_FILE__, __LINE__);
				dwResult = TPR_FAIL;
			}
		}
		
		else // Slave
		{

			//Let Master Start First.
			Sleep(dwTestTime/10);

			dwStart = GetTickCount();
			bRtn = ReadFile(hCommPort,lpBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);
			dwStop = GetTickCount();

			if (bRtn  == 0 || dwNumOfBytesTransferred != dwNumOfBytes) //We did not write any data
			{
		        g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Failed to Receive data \n"),__THIS_FILE__, __LINE__);
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: WriteFile Failed. Last Error was: %d\n")
						                       ,__THIS_FILE__, __LINE__,GetLastError());
					
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Expected to transfer %u bytes \n")
						                       ,__THIS_FILE__, __LINE__,dwNumOfBytes);

				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Actually sent %u bytes \n"),
												 __THIS_FILE__, __LINE__,dwNumOfBytesTransferred ); 
				dwResult = TPR_FAIL;
		    }
			
			//Check the data
			 
			g_pKato->Log( LOG_DETAIL, TEXT("Checking Data Received"));
			LPBYTE lpByte = (LPBYTE)lpBuffer;
			
			for (UINT i = 0; i < dwNumOfBytesTransferred; i++)
			{
				if (*lpByte != i%256)
				{
					 g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Data Mismatch! \n"),__THIS_FILE__, __LINE__);
				     g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: Expecting %u, Got %u\n")
						                       ,__THIS_FILE__, __LINE__,i%256,*lpByte);

					 dataIntegrityResult = DATA_INTEGRITY_FAIL;
					 dwResult = TPR_FAIL;
					 break;
				}
				lpByte++;
			}

			//Report result to Master
			DWORD dwNumOfBytesSent = 0;
			bRtn = WriteFile(hCommPort,&dataIntegrityResult, sizeof(dataIntegrityResult),&dwNumOfBytesSent, NULL);
			if (bRtn == FALSE )
			{
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Can not send reply to Master! \n"),__THIS_FILE__, __LINE__);
				g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: WriteFile Failed. Last Error was: %d\n")
						                       ,__THIS_FILE__, __LINE__,GetLastError());
				dwResult = TPR_ABORT;
			}
		}



		if (dwResult == TPR_PASS)  /*	Report result if the test is not aborted or failed */
		{
				
		
			g_pKato->Log(LOG_DETAIL,TEXT("Passed data integrity check"));
			dwNumOfMilliSeconds = dwStop - dwStart; 
			g_pKato->Log(LOG_DETAIL,TEXT("Expected to transfer %u bytes"),dwNumOfBytes);
			g_pKato->Log(LOG_DETAIL,TEXT("Actually transferred %u bytes"),dwNumOfBytesTransferred);
			
			measuredBaudRate = (double)dwNumOfBytesTransferred /(double)dwNumOfMilliSeconds * 1000 * 8; //in bits per sec

				
			g_pKato->Log(LOG_DETAIL,TEXT("Time elasped: %u msec"),dwNumOfMilliSeconds);
			g_pKato->Log(LOG_DETAIL,TEXT("Expected transfer rate: %u bits/sec"),dwBaudRate);

//Tests run in NT env Generate floating point error 			
#ifndef UNDER_NT			
			g_pKato->Log(LOG_DETAIL,TEXT("Measured transfer rate: %10.2f bits/sec"),measuredBaudRate);
			g_pKato->Log(LOG_DETAIL,TEXT("Driver is performing at %4.2f percent of top speed"),(double)measuredBaudRate/(double)dwBaudRate * 100 );
#else		
			g_pKato->Log(LOG_DETAIL,TEXT("Measured transfer rate: %10.2d bits/sec"),measuredBaudRate);
			g_pKato->Log(LOG_DETAIL,TEXT("Driver is performing at %4.2d percent of top speed"),(double)measuredBaudRate/(double)dwBaudRate * 100 );
		
#endif

		}

	

    DXTCleanup:
		if( FALSE == bRtn)
			dwResult = TPR_ABORT;

		if(hCommPort)
		{
			delete hCommPort ;
			hCommPort = NULL;
		}

		if (lpBuffer)
		{
			free(lpBuffer);
			lpBuffer = NULL;
		}

   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
}// end SerialPerfTest( ... ) 


