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
#endif

#ifdef UNDER_CE
#define HIGHER_PRIORITY	(-1)
#else
#define HIGHER_PRIORITY	1
#endif

#define TEST_TIME  50000
#define MAX_BUFFER 1024*10//10KB


/*

DataXmitTest:
Transmit 10 KBytes of Data with Different Buffer Length .
 
Arguments:
TUX standard arguments.
 
Return Value:
TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
TPR_EXECUTE: for TPM_EXECUTE
TPR_NOT_HANDLED: for all other messages.
 
Notes:

*/
TESTPROCAPI DataXmitTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	BOOL    bRtn					= FALSE;
    DWORD   dwResult				= TPR_PASS;
	CommPort *   hCommPort			= NULL;
	DWORD dwStartTicks				= 0;
	DWORD dwStopTicks				= 0;
	DWORD dwTotalTicks				= 0;
	DWORD dwIterations				= 0;
	DWORD dwTransferCount			= 0;
	DWORD dwNumOfBytesTransferred	= 0;
	DWORD dwIntegrity				= 0;
	
	LPVOID lpBuffer;

	DWORD dwLength=lpFTE->dwUserData;if( uMsg == TPM_QUERY_THREAD_COUNT )
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

	if(g_fBT)
	{
      	g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Bluetooth does not support this test - skipping."),
			      __THIS_FILE__, __LINE__ );

		return TPR_SKIP;
	}

    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */
    hCommPort = CreateCommObject(g_bCommDriver);
    hCommPort -> CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

	bRtn = SetupDefaultPort( hCommPort );
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );
	DCB mDCB;
	bRtn = GetCommState(hCommPort,  &mDCB);
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	//Enable CTS-RTS Handshaking.
	mDCB.BaudRate     = CBR_38400;
	mDCB.fOutxCtsFlow = TRUE;
	mDCB.fRtsControl  = RTS_CONTROL_HANDSHAKE;

	bRtn=SetCommState(hCommPort,&mDCB);
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Xmiting Speed at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );

	// Initial Timeout
	COMMTIMEOUTS    cto;
	cto.ReadIntervalTimeout         =   0;
	cto.ReadTotalTimeoutMultiplier  =   0;
	cto.ReadTotalTimeoutConstant    =   TEST_TIME;      
	cto.WriteTotalTimeoutMultiplier =   0; 
	cto.WriteTotalTimeoutConstant   =   TEST_TIME; 

	bRtn = SetCommTimeouts( hCommPort, &cto );
	COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

	if( g_fMaster )
	{
		//Allocate the Buffer of Length passed by the user
		lpBuffer = malloc(dwLength);
		if(lpBuffer == NULL)
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
			bRtn = TRUE;
			dwResult = TPR_SKIP;
			goto DXTCleanup;
		}

		//Set the Memory Elements to contain the Size of the Buffer 
		memset(lpBuffer,(BYTE)(dwLength),dwLength);

		//Calculate the No Of Iterations to transfer 10KB
        dwIterations = MAX_BUFFER/dwLength ;

		//Get the Start Time 
		dwStartTicks = GetTickCount();
		
		//Tranfer 10 KB
		while (dwTransferCount < dwIterations)
		{
			bRtn = WriteFile(hCommPort,lpBuffer, dwLength,&dwNumOfBytesTransferred, NULL);

			if (FALSE == bRtn  || dwNumOfBytesTransferred != dwLength) //We did not write any data
			{
				g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to send data Expected to Send %u Bytes Actually Sent %u Bytes")
					                      ,__THIS_FILE__, __LINE__,dwLength,dwNumOfBytesTransferred);
			    dwResult = TPR_FAIL;
				goto  DXTCleanup;

			}

			dwTransferCount++;
		}
		//Get the End Time 
        dwStopTicks  = GetTickCount();

		//Calculate the total Ticks
		dwTotalTicks = dwStopTicks - dwStartTicks;

		//Sleep Till the Data is Read
		Sleep(1000);

		dwNumOfBytesTransferred = 0;

		//Read Back Data Integrity Result and then Output the Transfer Time Value 
		bRtn = ReadFile(hCommPort,&dwIntegrity,sizeof(DWORD),&dwNumOfBytesTransferred, NULL);
		if (FALSE == bRtn || dwNumOfBytesTransferred != sizeof(DWORD)) //We did not Read any data
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Integrity Check Failed"),__THIS_FILE__, __LINE__);
			dwResult = TPR_FAIL;
		}

		if(dwIntegrity)
		{
		    //Calculate the Transfer Time 
			g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:DataXmitTest, Result (%d Transfer/%ld Ticks)= (Average Transfer Time) %ld /per second"),
                 __THIS_FILE__, __LINE__,dwIterations,dwTotalTicks,((dwIterations *1000)/dwTotalTicks));
     	}
		
	}
	else
	{
		//Slave
		lpBuffer = malloc(MAX_BUFFER);
		if (lpBuffer == NULL)
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
			bRtn = TRUE;
			dwResult = TPR_SKIP;
			goto DXTCleanup;
		}

		//Let Master Start First.
		Sleep(200);

    	bRtn = ReadFile(hCommPort,lpBuffer, MAX_BUFFER,&dwNumOfBytesTransferred, NULL);
		if (FALSE == bRtn || dwNumOfBytesTransferred != MAX_BUFFER) //We did not Read any data
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Read data Expected to Read %u Bytes Actually Read %u Bytes")
					                      ,__THIS_FILE__, __LINE__,MAX_BUFFER,dwNumOfBytesTransferred);
			dwResult = TPR_FAIL;
		}

		//Verify Data
		LPBYTE lpByte = (LPBYTE) lpBuffer;

		for(DWORD index = 0 ;index <MAX_BUFFER;index++)
		{
           if(*lpByte++ != (BYTE)dwLength)
		   {
			   g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Mismatch Expecting %u Got %u ") ,__THIS_FILE__, __LINE__,dwLength,*lpByte);
			   dwIntegrity = 0 ;
			   dwResult = TPR_FAIL;
			   break;
		   }
		}

		if(MAX_BUFFER == index)
			dwIntegrity = 1;

		
		dwNumOfBytesTransferred = 0;

		//Write Back the Data Integrity Result to the Master
		bRtn = WriteFile(hCommPort,&dwIntegrity,sizeof(DWORD),&dwNumOfBytesTransferred, NULL);
		if (FALSE == bRtn  || dwNumOfBytesTransferred != sizeof(DWORD)) //We did not write any data
		{
				g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Obtain Data Integrity Result"),__THIS_FILE__, __LINE__);
			    dwResult = TPR_FAIL;
				goto  DXTCleanup;
		}

	}

	bRtn=TRUE;
    DXTCleanup:
		if( FALSE == bRtn)
			dwResult = TPR_ABORT;

        /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
		-------------------------------------------------------------------- */
		dwResult = EndTestSync(hCommPort, lpFTE->dwUniqueID, dwResult );

		if( hCommPort )
		{
 	        delete hCommPort;
		    FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
		}

		return dwResult;
} 


/*

DataReceiveTest:
Receive 10 KBytes of Data with Different Buffer Length .
 
Arguments:
TUX standard arguments.
 
Return Value:
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 
Notes:

*/
TESTPROCAPI DataReceiveTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	BOOL    bRtn					= FALSE;
    DWORD   dwResult				= TPR_PASS;
	CommPort *   hCommPort			= NULL;
	DWORD dwStartTicks				= 0;
	DWORD dwStopTicks				= 0;
	DWORD dwTotalTicks				= 0;
	DWORD dwIterations				= 0;
	DWORD dwTransferCount			= 0;
	DWORD dwNumOfBytesTransferred	= 0;
	DWORD dwIntegrity				= 0;
	
	LPVOID lpBuffer;

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

	if(g_fBT)
	{
      	g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Bluetooth does not support this test - skipping."),
			      __THIS_FILE__, __LINE__ );

		return TPR_SKIP;
	}

    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */
    hCommPort = CreateCommObject(g_bCommDriver);
    hCommPort -> CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

	bRtn = SetupDefaultPort( hCommPort );
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );
	DCB mDCB;
	bRtn = GetCommState(hCommPort,  &mDCB);
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	//Enable CTS-RTS Handshaking.
	mDCB.BaudRate     = CBR_38400;
	mDCB.fOutxCtsFlow = TRUE;
	mDCB.fRtsControl  = RTS_CONTROL_HANDSHAKE;

	bRtn=SetCommState(hCommPort,&mDCB);
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Receive Speed at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );

	// Initial Timeout
	COMMTIMEOUTS    cto;
	cto.ReadIntervalTimeout         =   0;
	cto.ReadTotalTimeoutMultiplier  =   0;
	cto.ReadTotalTimeoutConstant    =   TEST_TIME;      
	cto.WriteTotalTimeoutMultiplier =   0; 
	cto.WriteTotalTimeoutConstant   =   TEST_TIME; 

	bRtn = SetCommTimeouts( hCommPort, &cto );
   COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

   	if( g_fMaster )
	{
		//Slave
		lpBuffer = malloc(MAX_BUFFER);
		if (lpBuffer == NULL)
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
			bRtn = TRUE;
			dwResult = TPR_SKIP;
			goto DXTCleanup;
		}

		//Calculate the No Of Iterations 
        dwIterations = MAX_BUFFER/dwLength ;

		//Get the Start Time 
		dwStartTicks = GetTickCount();

		bRtn = ReadFile(hCommPort,lpBuffer, MAX_BUFFER,&dwNumOfBytesTransferred, NULL);
		if (FALSE == bRtn || dwNumOfBytesTransferred != MAX_BUFFER) //We did not Read any data
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Read data Expected to Read %u Bytes Actually Read %u Bytes")
					                      ,__THIS_FILE__, __LINE__,MAX_BUFFER,dwNumOfBytesTransferred);
			dwResult = TPR_FAIL;
		}

		//Get the End Time 
        dwStopTicks  = GetTickCount();

		//Calculate the total Ticks
		dwTotalTicks = dwStopTicks - dwStartTicks;
		
		//Verify Data
		LPBYTE lpByte = (LPBYTE) lpBuffer;

		for(DWORD index = 0 ;index <MAX_BUFFER;index++)
		{
           if(*lpByte++ != (BYTE)dwLength)
		   {
			   g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Mismatch Expecting %u Got %u ") ,__THIS_FILE__, __LINE__,dwLength,*lpByte);
			   dwIntegrity = 0 ;
			   dwResult = TPR_FAIL;
			   break;
		   }
		}

		if(MAX_BUFFER == index)
			dwIntegrity = 1;

		
		dwNumOfBytesTransferred = 0;

		//Write Back the Data Integrity Result to the Master
		bRtn = WriteFile(hCommPort,&dwIntegrity,sizeof(DWORD),&dwNumOfBytesTransferred, NULL);
		if (FALSE == bRtn  || dwNumOfBytesTransferred != sizeof(DWORD)) //We did not write any data
		{
				g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Obtain Data Integrity Result"),__THIS_FILE__, __LINE__);
			    dwResult = TPR_FAIL;
				goto  DXTCleanup;
		}

		if(dwIntegrity)
		{
		    //Calculate the Transfer Time 
			g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:DataReceive Test , Result (%d Transfer/%ld Ticks)= (Average Transfer Time) %ld /per second"),
                 __THIS_FILE__, __LINE__,dwIterations,dwTotalTicks,((dwIterations *1000)/dwTotalTicks));
     	}

	}
	else
	{
		//Allocate the Buffer of Length passed by the User
		lpBuffer = malloc(dwLength);
		if(lpBuffer == NULL)
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to Allocate Buffer"),__THIS_FILE__, __LINE__);
			bRtn = TRUE;
			dwResult = TPR_SKIP;
			goto DXTCleanup;
		}

		//Set the Memory Elements to contain the Size of the Buffer 
		memset(lpBuffer,(BYTE)(dwLength),dwLength);


		//Calculate the No Of Iterations 
        dwIterations = MAX_BUFFER/dwLength ;

		//Let Master Start First.
		Sleep(300);

		//Tranfer 10 KB
		while (dwTransferCount < dwIterations)
		{
			bRtn = WriteFile(hCommPort,lpBuffer, dwLength,&dwNumOfBytesTransferred, NULL);

			if (FALSE == bRtn  || dwNumOfBytesTransferred != dwLength) //We did not write any data
			{
				g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Failed to send data Expected to Send %u Bytes Actually Sent %u Bytes")
					                      ,__THIS_FILE__, __LINE__,dwLength,dwNumOfBytesTransferred);
			    dwResult = TPR_FAIL;
				goto  DXTCleanup;

			}

			dwTransferCount++;
		}

		//Sleep for the Data to be Read
		Sleep(1000);

		dwNumOfBytesTransferred = 0;

		//Read Back Data Integrity Result and then Output the Transfer Time Value 
		bRtn = ReadFile(hCommPort,&dwIntegrity,sizeof(DWORD),&dwNumOfBytesTransferred, NULL);
		if (FALSE == bRtn || dwNumOfBytesTransferred != sizeof(DWORD)) //We did not Read any data
		{
			g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Data Integrity Check Failed"),__THIS_FILE__, __LINE__);
			dwResult = TPR_FAIL;
		}

		if(!dwIntegrity)
		{
		    //Calculate the Transfer Time 
			g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d:Data Integrity Check Failed"),__THIS_FILE__, __LINE__);
     	}


		
	}


	bRtn=TRUE;
    DXTCleanup:
		if( FALSE == bRtn)
			dwResult = TPR_ABORT;

        /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
		-------------------------------------------------------------------- */
		dwResult = EndTestSync(hCommPort, lpFTE->dwUniqueID, dwResult );

		if( hCommPort )
		{
 	        delete hCommPort;
		    FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
		}

		return dwResult;
	
	
} 

