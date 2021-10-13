/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     pstests.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	E:\wince\private\qaos\drivers\serial\pserial\PStests.cpp
 
Abstract:
 
	This file cantains the tests used to test the serial port.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#define __THIS_FILE__ TEXT("DtTests.cpp")

#include "PSerial.h"
//#include "GSerial.h"
#include <stdio.h>
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
 
DataSpeedTest:
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
#define TEST_TIME 5000
#define LENGTH_NUMBER 9
#define MAX_COUNT	16
#define LENGTH_OFFSET 0

typedef struct {
	volatile BOOL bTerminated;
	BOOL bError;
	HANDLE hCommPort;
	HANDLE hThread;
	DWORD  dwThreadId;
} ERROR_MONITOR_PARAM,*PERROR_MOTITOR_PARAM;

DWORD WINAPI ErrorMonitorThread(LPVOID lp)
{
	PERROR_MOTITOR_PARAM pParam= (PERROR_MOTITOR_PARAM)lp;
	pParam->bError=FALSE;
	while (pParam->bTerminated) {
		BOOL bReturn=SetCommMask(pParam->hCommPort,EV_ERR);
		if (!bReturn) {
			g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   ErrorMonitorThread Fail SetCommMask GetLastError()=%ld"),
							__THIS_FILE__, __LINE__ ,GetLastError());
			pParam->bError=TRUE;
			break;
		};
		DWORD dwEvtMask=0;
		bReturn=WaitCommEvent(pParam->hCommPort,&dwEvtMask,NULL);
		if (!bReturn) {
			g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   ErrorMonitorThread Fail WaitCommEvent GetLastError()=%ld"),
							__THIS_FILE__, __LINE__ ,GetLastError());
			pParam->bError=TRUE;
			break;
		};
		if (dwEvtMask & EV_ERR) {
			g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   ErrorMonitorThread Fail EV_ERR has been captured"),
							__THIS_FILE__, __LINE__);
			pParam->bError=TRUE;

		}
	}
	return pParam->bError;
}


TESTPROCAPI DataSpeedTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL    bRtn        = FALSE;
    DWORD   dwResult    = TPR_PASS, dwIndex=0,dwTestCount=0,dwTestLengthCount=0,dwTotal[LENGTH_NUMBER+1];
	UINT	uiStart, uiStop;
	HANDLE      hCommPort   = NULL;

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
    hCommPort = CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

	uiStart = GetTickCount();
	ERROR_MONITOR_PARAM errorParam;
	errorParam.bTerminated=FALSE;
	errorParam.bError=TRUE;
	errorParam.hCommPort=hCommPort;
	errorParam.hThread=NULL;

		bRtn = SetupDefaultPort( hCommPort );

		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );

		DCB mDCB;
		bRtn = GetCommState(hCommPort,  &mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);
		switch (lpFTE->dwUserData) {
		case 1:default:
			mDCB.BaudRate=CBR_9600;
			break;
		case 2:
			mDCB.BaudRate=CBR_19200;
			break;
		case 3:
			mDCB.BaudRate=CBR_38400;
			break;
		case 4:
			mDCB.BaudRate=CBR_57600;
			break;
		case 5:
			mDCB.BaudRate=CBR_115200;
			break;
		};
		// Enable CTS-RTS Handshaking.
		mDCB.fOutxCtsFlow = TRUE;
		mDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;

		bRtn=SetCommState(hCommPort,&mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);



		// Initial Timeout
	    COMMTIMEOUTS    cto;
		cto.ReadIntervalTimeout         =    100;
		cto.ReadTotalTimeoutMultiplier  =    10;
		cto.ReadTotalTimeoutConstant    =   TEST_TIME;      
		cto.WriteTotalTimeoutMultiplier =    10; 
		cto.WriteTotalTimeoutConstant   =   TEST_TIME; 

		bRtn = SetCommTimeouts( hCommPort, &cto );
		COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

		
		for (dwIndex=0;dwIndex<LENGTH_NUMBER+1;dwIndex++) {
			dwTotal[dwIndex]=0;
		}
		BYTE  bTestBuffer[(1<<LENGTH_NUMBER)+LENGTH_OFFSET];

		//Create Monitor Thread
		errorParam.hThread=
				CreateThread(NULL,NULL,ErrorMonitorThread,(LPVOID)&errorParam,NULL,
				&(errorParam.dwThreadId));

		if( g_fMaster ) {
			Sleep(TEST_TIME/5);// Let Slave Start First.
			uiStart = GetTickCount();
			uiStop = uiStart;
			while (dwTestCount<MAX_COUNT) {
				DWORD uiThisStart=uiStop;
				DWORD dwTestLength=(1<<dwTestLengthCount)+LENGTH_OFFSET;
				ASSERT(dwTestLength<=((1<<LENGTH_NUMBER)+LENGTH_OFFSET));
				memset(bTestBuffer,(BYTE)(dwTestLength),dwTestLength);
				//Write
				DWORD dwReturnLength=0;
				bRtn = WriteFile( hCommPort,bTestBuffer, dwTestLength, &dwReturnLength, NULL );
				if (FALSE == bRtn || dwReturnLength!=dwTestLength) {
					if (!bRtn) 
						g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail WriteFile GetLastError()=%ld"),
							__THIS_FILE__, __LINE__ ,GetLastError());

					g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail @ WriteFile dwTestCount=%ld,dwTestLength=0x%lx, dwReturnLength=0x%lx"),
							__THIS_FILE__, __LINE__ ,dwTestCount,dwTestLength,dwReturnLength);
					bRtn=FALSE;
					goto DXTCleanup;
				};
				//Readback.
				dwReturnLength=0;
				bRtn = ReadFile( hCommPort,bTestBuffer, dwTestLength, &dwReturnLength, NULL );
				if (FALSE == bRtn || dwReturnLength!=dwTestLength) {
					if (!bRtn) 
						g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail ReadFile GetLastError()=%ld"),
							__THIS_FILE__, __LINE__ ,GetLastError());

					g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail @ ReadFile dwTestCount=%ld,dwTestLength=0x%lx, dwReturnLength=0x%lx"),
							__THIS_FILE__, __LINE__ ,dwTestCount,dwTestLength,dwReturnLength);
					bRtn=FALSE;
					goto DXTCleanup;
				};
				uiStop=GetTickCount();
				dwTotal[dwTestLengthCount] += uiStop-uiThisStart; // Record Rount trip time.

				// Check Data.
				for (dwIndex=0;dwIndex<dwTestLength;dwIndex++) {
					if (bTestBuffer[dwIndex]!=(BYTE)(dwTestLength)) {
						g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail @ Data Check dwTestCount=%ld,dwIndex=%ld,dwTestLength=0x%lx, bTestBuffer[dwIndex]=0x%lx"),
							__THIS_FILE__, __LINE__ ,dwTestCount,dwIndex,dwTestLength,bTestBuffer[dwIndex]);
						bRtn=FALSE;
						goto DXTCleanup;
					}
				}

				dwTestLengthCount++;
				if (dwTestLengthCount>LENGTH_NUMBER) {
					dwTestCount++;
					dwTestLengthCount=0;
				};
			}
			// Here success and print out the result.
			g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:   DataSpeedTest Success, Total Count=%ld"),
                  __THIS_FILE__, __LINE__ ,dwTestCount);
			for (DWORD dwIndex=0;dwIndex<LENGTH_NUMBER+1;dwIndex++) {
				// Adjust if it was too fast
				if (!dwTotal[dwIndex])
					dwTotal[dwIndex]++;

				g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:   DataSpeedTest, Result for %ld bytes transfer,(%d Trips/%ld Ticks)= (Average Transfer Time) %ld Trip/Second"),
                  __THIS_FILE__, __LINE__ ,
				  (1<<dwIndex)+LENGTH_OFFSET,dwTestCount,dwTotal[dwIndex],
				  dwTestCount*1000/dwTotal[dwIndex]);
			};
		}
		else {// Slave

			Sleep(TEST_TIME/10);// Let Slave Start First.
			
			uiStart = GetTickCount();
			uiStop = uiStart;
			while (dwTestCount<MAX_COUNT) {
				DWORD dwTestLength=(1<<dwTestLengthCount)+LENGTH_OFFSET;
				ASSERT(dwTestLength<=((1<<LENGTH_NUMBER)+LENGTH_OFFSET));
				//Readback.
				DWORD dwReturnLength=0;
				bRtn = ReadFile( hCommPort,bTestBuffer, dwTestLength, &dwReturnLength, NULL );
				if (FALSE == bRtn || dwReturnLength!=dwTestLength) {
					if (!bRtn) 
						g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail ReadFile GetLastError()=%ld"),
							__THIS_FILE__, __LINE__ ,GetLastError());

					g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail @ ReadFile dwTestCount=%ld,dwTestLength=0x%lx, dwReturnLength=0x%lx"),
							__THIS_FILE__, __LINE__ ,dwTestCount,dwTestLength,dwReturnLength);
					bRtn=FALSE;
					goto DXTCleanup;
				};
				//Write
				dwReturnLength=0;
				bRtn = WriteFile( hCommPort,bTestBuffer, dwTestLength, &dwReturnLength, NULL );
				if (FALSE == bRtn || dwReturnLength!=dwTestLength) {
					if (!bRtn) 
						g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail WriteFile GetLastError()=%ld"),
							__THIS_FILE__, __LINE__ ,GetLastError());

					g_pKato->Log( LOG_FAIL, 
					      TEXT("In %s @ line %d:   DataSpeedTest Fail @ WriteFile dwTestCount=%ld,dwTestLength=0x%lx, dwReturnLength=0x%lx"),
							__THIS_FILE__, __LINE__ ,dwTestCount,dwTestLength,dwReturnLength);
					bRtn=FALSE;
					goto DXTCleanup;
				};
				uiStop=GetTickCount();
				dwTestLengthCount++;
				if (dwTestLengthCount>LENGTH_NUMBER) {
					dwTestCount++;
					dwTestLengthCount=0;
				};
			};
	//		Sleep(1000); // give the receiver a chance to catch up before we close the port
		}


    DXTCleanup:
		if (errorParam.hThread) {
			errorParam.bTerminated=TRUE;
			SetCommMask(errorParam.hCommPort,0);
			if (WaitForSingleObject(errorParam.hThread,TEST_TIME)!=WAIT_OBJECT_0) {
				ASSERT(FALSE);//.
				TerminateThread(errorParam.hThread,(DWORD)-1);
			};
		}

		if( FALSE == bRtn) dwResult = TPR_ABORT;
		if( hCommPort )  {
        /* --------------------------------------------------------------------
    	    Sync the end of a test of the test.
        -------------------------------------------------------------------- */
//        if( TPR_ABORT != dwResult )

	        bRtn = CloseHandle( hCommPort );
		    FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
		}


   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
} // end DataSpeedTest( ... ) 

#define MAX_BUFFER 0x4000
typedef struct {
	BOOL bRead;
	volatile BOOL bTerminated;
	DWORD dwNumOfIo;
	HANDLE hCommPort;
	HANDLE hThread;
	DWORD  dwThreadId;
	DWORD dwIoLegnth;
	BYTE Buffer[MAX_BUFFER];
} READWRITE_PARAM,*LPREADWRITE_PARAM;

DWORD WINAPI ReadWriteThread(LPVOID lp)
{
	LPREADWRITE_PARAM lpReadWriteParam= (LPREADWRITE_PARAM) lp;
	lpReadWriteParam->dwNumOfIo=0;
	DWORD dwCount=0;
	while (!lpReadWriteParam->bTerminated) {
		DWORD dwCurrentReturn;
		BOOL bReturn;
		if (lpReadWriteParam->bRead)
			bReturn=ReadFile( lpReadWriteParam->hCommPort,lpReadWriteParam->Buffer,
					lpReadWriteParam->dwIoLegnth,&dwCurrentReturn,NULL);
		else
			bReturn=WriteFile( lpReadWriteParam->hCommPort,lpReadWriteParam->Buffer,
					lpReadWriteParam->dwIoLegnth,&dwCurrentReturn,NULL);
		if (bReturn && dwCurrentReturn==lpReadWriteParam->dwIoLegnth)
			lpReadWriteParam->dwNumOfIo++;
		else {
			g_pKato->Log( LOG_FAIL, 
                  TEXT("In %s @ line %d:   ReadWriteThread, Fail at Read or Write bReturn=%lx or Require(%ld)!=Actual Got(%ld)"),
                 __THIS_FILE__, __LINE__, bReturn,lpReadWriteParam->dwIoLegnth,dwCurrentReturn);
			if (!bReturn)
				g_pKato->Log( LOG_FAIL, TEXT("In %s @ line %d:   ReadWriteThread, Fail at Read or Write GetLastError=%ld"),
                 __THIS_FILE__, __LINE__, GetLastError());
		}
		if (++dwCount>10) {
			Sleep(0);
			dwCount=0;
		}
	}
	return 1;
}
TESTPROCAPI DataXmitTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL    bRtn        = FALSE;
    DWORD   dwResult    = TPR_PASS;
	HANDLE      hCommPort   = NULL;
	DWORD dwLength=lpFTE->dwUserData;
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
    hCommPort = CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )


		bRtn = SetupDefaultPort( hCommPort );
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );
		DCB mDCB;
		bRtn = GetCommState(hCommPort,  &mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);
		// Enable CTS-RTS Handshaking.
		mDCB.BaudRate=CBR_38400;
		mDCB.fOutxCtsFlow = TRUE;
		mDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;

		bRtn=SetCommState(hCommPort,&mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

	    bRtn = SetupComm( hCommPort, 4096, 4096);
		COMM_ERROR( hCommPort, FALSE == bRtn, goto DXTCleanup);

		g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Xmiting Speed at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );

		// Initial Timeout
	    COMMTIMEOUTS    cto;
		cto.ReadIntervalTimeout         =    0;
		cto.ReadTotalTimeoutMultiplier  =    0;
		cto.ReadTotalTimeoutConstant    =   TEST_TIME;      
		cto.WriteTotalTimeoutMultiplier =    0; 
		cto.WriteTotalTimeoutConstant   =   TEST_TIME; 

		bRtn = SetCommTimeouts( hCommPort, &cto );
		COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

		READWRITE_PARAM readWriteParam;
		readWriteParam.bTerminated=FALSE;
		readWriteParam.dwNumOfIo=0;
		readWriteParam.hCommPort=hCommPort;

		if( g_fMaster ) {
			readWriteParam.bRead=FALSE;
			readWriteParam.dwIoLegnth=dwLength;
			readWriteParam.hThread=
				CreateThread(NULL,NULL,ReadWriteThread,(LPVOID)&readWriteParam,CREATE_SUSPENDED,
				&(readWriteParam.dwThreadId));
			ASSERT(readWriteParam.hThread);
			SetThreadPriority(readWriteParam.hThread,GetThreadPriority(GetCurrentThread())-HIGHER_PRIORITY);
			Sleep(TEST_TIME/5);
			g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Xmiting Speed (Master) at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );
			DWORD dwTotalTicks=GetTickCount();
			ResumeThread(readWriteParam.hThread);
			Sleep(4*TEST_TIME/5);
			readWriteParam.bTerminated=TRUE;
			if (WaitForSingleObject(readWriteParam.hThread,2*TEST_TIME/5)!=WAIT_OBJECT_0) {
				ASSERT(FALSE);
				TerminateThread(readWriteParam.hThread,(DWORD)-1);
			};
			dwTotalTicks=GetTickCount()-dwTotalTicks;
			CloseHandle(readWriteParam.hThread);
			g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:   DataXmitTest, Result for %ld bytes transfer,(%d Transfer/%ld Ticks)= (Average Transfer Time) %ld /per second"),
                 __THIS_FILE__, __LINE__, dwLength,
				 readWriteParam.dwNumOfIo,dwTotalTicks,
				 (readWriteParam.dwNumOfIo*1000/dwTotalTicks));
		}
		else {// Slave
			readWriteParam.bRead=TRUE;
			readWriteParam.dwIoLegnth=MAX_BUFFER;
			readWriteParam.hThread=
				CreateThread(NULL,NULL,ReadWriteThread,(LPVOID)&readWriteParam,CREATE_SUSPENDED,
				&(readWriteParam.dwThreadId));
			ASSERT(readWriteParam.hThread);
			SetThreadPriority(readWriteParam.hThread,GetThreadPriority(GetCurrentThread())-HIGHER_PRIORITY);
			Sleep(TEST_TIME/10);
			ResumeThread(readWriteParam.hThread);
			Sleep(TEST_TIME+2*TEST_TIME/5);
			readWriteParam.bTerminated=TRUE;
			if (WaitForSingleObject(readWriteParam.hThread,TEST_TIME)!=WAIT_OBJECT_0)
				TerminateThread(readWriteParam.hThread,(DWORD)-1);
		};
		bRtn=TRUE;

    DXTCleanup:

		if( FALSE == bRtn) dwResult = TPR_ABORT;
		if( hCommPort )  {
        /* --------------------------------------------------------------------
    	    Sync the end of a test of the test.
        -------------------------------------------------------------------- */
//        if( TPR_ABORT != dwResult )

	        bRtn = CloseHandle( hCommPort );
		    FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
		}


   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
} // end DataSpeedTest( ... ) 

TESTPROCAPI DataReceiveTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL    bRtn        = FALSE;
    DWORD   dwResult    = TPR_PASS;
	HANDLE      hCommPort   = NULL;
	DWORD dwLength=lpFTE->dwUserData;
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
    hCommPort = CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )


		bRtn = SetupDefaultPort( hCommPort );

		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DRTCleanup );

		DCB mDCB;
		bRtn = GetCommState(hCommPort,  &mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DRTCleanup );
		// Enable CTS-RTS Handshaking.
		mDCB.BaudRate=CBR_38400;
		mDCB.fOutxCtsFlow = TRUE;
		mDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;

		bRtn=SetCommState(hCommPort,&mDCB);
		COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DRTCleanup );

	    bRtn = SetupComm( hCommPort, 4096, 4096);
		COMM_ERROR( hCommPort, FALSE == bRtn, goto DRTCleanup );

		g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Receiving Speed at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );

		// Initial Timeout
	    COMMTIMEOUTS    cto;
		cto.ReadIntervalTimeout         =    0;
		cto.ReadTotalTimeoutMultiplier  =    0;
		cto.ReadTotalTimeoutConstant    =   TEST_TIME;      
		cto.WriteTotalTimeoutMultiplier =    0; 
		cto.WriteTotalTimeoutConstant   =   TEST_TIME; 

		bRtn = SetCommTimeouts( hCommPort, &cto );
		COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DRTCleanup );
		READWRITE_PARAM readWriteParam;
		readWriteParam.bTerminated=FALSE;
		readWriteParam.dwNumOfIo=0;
		readWriteParam.hCommPort=hCommPort;

		if( g_fMaster ) {
			readWriteParam.bRead=TRUE;
			readWriteParam.dwIoLegnth=dwLength;
			readWriteParam.hThread=
				CreateThread(NULL,NULL,ReadWriteThread,(LPVOID)&readWriteParam,CREATE_SUSPENDED,
				&(readWriteParam.dwThreadId));
			ASSERT(readWriteParam.hThread);
			SetThreadPriority(readWriteParam.hThread,GetThreadPriority(GetCurrentThread())-HIGHER_PRIORITY);
			Sleep(TEST_TIME/5);
			g_pKato->Log( LOG_DETAIL, 
                 TEXT("In %s @ line %d:  Test Receiving Speed (Master) at BufferLength= %dBytes"),
                 __THIS_FILE__, __LINE__, dwLength );
			DWORD dwTotalTicks=GetTickCount();
			ResumeThread(readWriteParam.hThread);
			Sleep(4*TEST_TIME/5);
			readWriteParam.bTerminated=TRUE;
			if (WaitForSingleObject(readWriteParam.hThread,2*TEST_TIME/5)!=WAIT_OBJECT_0) {
				ASSERT(FALSE);
				TerminateThread(readWriteParam.hThread,(DWORD)-1);
			}
			dwTotalTicks=GetTickCount()-dwTotalTicks;
			CloseHandle(readWriteParam.hThread);
			g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:   DataReceiveTest, Result for %ld bytes transfer,(%d Transfer/%ld Ticks)= (Average Transfer Time) %ld /per second"),
                 __THIS_FILE__, __LINE__, dwLength,
				 readWriteParam.dwNumOfIo,dwTotalTicks,
				 (readWriteParam.dwNumOfIo*1000/dwTotalTicks));
			// 
			Sleep(2*TEST_TIME/5); //Wait for Slave Complete
			ReadFile( readWriteParam.hCommPort,readWriteParam.Buffer,
					dwLength,&dwTotalTicks,NULL);
		}
		else {// Slave
			readWriteParam.bRead=FALSE;
			readWriteParam.dwIoLegnth=dwLength;
			readWriteParam.hThread=
				CreateThread(NULL,NULL,ReadWriteThread,(LPVOID)&readWriteParam,CREATE_SUSPENDED,
				&(readWriteParam.dwThreadId));
			ASSERT(readWriteParam.hThread);
			SetThreadPriority(readWriteParam.hThread,GetThreadPriority(GetCurrentThread())-HIGHER_PRIORITY);
			Sleep(TEST_TIME/10);
			ResumeThread(readWriteParam.hThread);
			Sleep(TEST_TIME);
			readWriteParam.bTerminated=TRUE;
			if (WaitForSingleObject(readWriteParam.hThread,TEST_TIME)!=WAIT_OBJECT_0)
				TerminateThread(readWriteParam.hThread,(DWORD)-1);
		};
		bRtn=TRUE;


    DRTCleanup :

		if( FALSE == bRtn) dwResult = TPR_ABORT;
		if( hCommPort )  {
        /* --------------------------------------------------------------------
    	    Sync the end of a test of the test.
        -------------------------------------------------------------------- */
//        if( TPR_ABORT != dwResult )

	        bRtn = CloseHandle( hCommPort );
		    FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
		}


   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
} // end DataSpeedTest( ... ) 

