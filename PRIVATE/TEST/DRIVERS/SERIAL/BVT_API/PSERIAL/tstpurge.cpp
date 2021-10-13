/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tstpurge.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	TstPurge.cpp
 
Abstract:
 
	This file contains the test for PurgeComm
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#define __THIS_FILE__ TEXT("TstPurge.cpp")

#include "PSerial.h"
#include "TstModem.h"
#include "TstModem.h"

/*++
 
ReadWriteTestThread:
 
	This thread simply times how long it takes to write its buffer.
 
Arguments:
 
	HANDLE  hCommPort
 
Return Value:
 
	DWORD of time, if error 0x80000000;
 

 
	Uknown (unknown)
 
Notes:
 
--*/
static DWORD WINAPI ReadWriteTestThread( LPCOMMTHREADCTRL lpCtrl )
{
    const int   nBuffSize =  400;
    BYTE        aBuffer[nBuffSize];
    DWORD       dwStart;
    DWORD       dwTime;
    DWORD       dwMoved;
    BOOL        bRtn;
#ifdef UNDER_NT    
    DWORD       dwRtn;
#endif    

    dwMoved = 0;

    dwStart = GetTickCount();
	memset(aBuffer,0x55,nBuffSize);
    // Write when hEvent is not zero
    if( lpCtrl->hEndEvent )
    {
        bRtn = WriteFile( lpCtrl->hPort, aBuffer, nBuffSize, 
                          &dwMoved, (LPOVERLAPPED)lpCtrl->dwData );
                          
    } // end if( FLAGED TO WRITE )
    
    else 
    {
        bRtn = ReadFile( lpCtrl->hPort, aBuffer, nBuffSize, 
                          &dwMoved, (LPOVERLAPPED)lpCtrl->dwData );
        
    } // end if( FLAGED TO WRITE ) else
    
#ifdef UNDER_NT
    if( !bRtn &&  (ERROR_IO_PENDING == (dwRtn = GetLastError())) )
    {
        bRtn = GetOverlappedResult( lpCtrl->hPort, (LPOVERLAPPED)lpCtrl->dwData,
                                    &dwMoved, TRUE );
        if( !bRtn && (ERROR_OPERATION_ABORTED != (dwRtn = GetLastError())) )
        {
            g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d"),
                          __THIS_FILE__, __LINE__, dwRtn ); 
            ClearTestCommErrors( lpCtrl->hPort );
            ExitThread( 0x80000000 );
                        
        } // end if( NOT OPERATION ABORTED )
        
    } // end if( IO_PENDING )
    
    else
    {
        g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d"),
                          __THIS_FILE__, __LINE__, dwMoved ); 
        ClearTestCommErrors( lpCtrl->hPort );        
        ExitThread( 0x80000000 );
            
    } // end if( IO_PENDING ) else

#else   
    COMM_ERROR( lpCtrl->hPort, FALSE == bRtn, ExitThread( 0x80000000 ) );
#endif UNDER_NT    
    dwTime = GetTickCount() - dwStart;

    if( nBuffSize == dwMoved )
    {
        g_pKato->Log( LOG_WARNING, TEXT("Warning in %s @ line %d: IO Operation completed, %d bytes of %d bytes"),
                      __THIS_FILE__, __LINE__, dwMoved, nBuffSize ); 
        
    }

    ExitThread( dwTime );
    return dwTime;

} // end static DWORD WINAPI ReadWriteTestThread( HANDLE hCommPort )


/*++
 
TestPurgeCommRxTx:
 
    Tests if PurgeComm will stop a write oprations.
    
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI TestPurgeCommRxTx( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL            bRtn                = FALSE;
    DWORD           dwResult            = TPR_PASS;
    DWORD           nIOTime          = 0;
    HANDLE          hIOThread        = NULL;
    HANDLE          hCommPort           = INVALID_HANDLE_VALUE;
    const DWORD     nWaitTime           = 3000;
    const DWORD     nDeltaTime          = 200;
    COMMTIMEOUTS    LocalCto;
    DCB             LocalDcb;
    INT             iIdx                = 0; 
    const DWORD     nTests              = 2;
    DWORD           aPurgeFlag[nTests] = { PURGE_RXABORT |PURGE_RXCLEAR, PURGE_TXABORT|PURGE_TXCLEAR };    
    COMMTHREADCTRL  IOThreadCtrl;    
    LPOVERLAPPED    lpOverLapped    = NULL;
    DWORD           dwPortFlag      = 0;
#ifdef UNDER_NT
    OVERLAPPED  OvrLpd;
#endif

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
	bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT );



	
    /* --------------------------------------------------------------------
    	Under NT The port must be opened for Overlapped IO
    -------------------------------------------------------------------- */
#ifdef UNDER_NT
    ZeroMemory( &OvrLpd, sizeof(OvrLpd) );
    OvrLpd.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    DEFAULT_ERROR( NULL == OvrLpd.hEvent, goto TPCRTCleanup );

    lpOverLapped = &OvrLpd;
    dwPortFlag = FILE_FLAG_OVERLAPPED;
#endif

    /* --------------------------------------------------------------------
    	Open the comm port for this test.
    -------------------------------------------------------------------- */
    hCommPort = CreateFile( g_lpszCommPort, 
                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, dwPortFlag, lpOverLapped );
    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , goto TPCRTCleanup);

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TPCRTCleanup );
    

    /* --------------------------------------------------------------------
    	Test total timeouts
    -------------------------------------------------------------------- */
    GetCommTimeouts( hCommPort, &LocalCto );
    LocalCto.WriteTotalTimeoutMultiplier  = 0; 
    LocalCto.WriteTotalTimeoutConstant    = nWaitTime * 2; 
    bRtn = SetCommTimeouts( hCommPort, &LocalCto );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TPCRTCleanup );

    /* -------------------------------------------------------------------
        Set Comm Properties for CTS-RTS.
    ------------------------------------------------------------------- */
    bRtn = GetCommState( hCommPort, &LocalDcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TPCRTCleanup );

    LocalDcb.fOutxCtsFlow = TRUE;

    bRtn = SetCommState( hCommPort, &LocalDcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TPCRTCleanup );

    bRtn = EscapeCommFunction( hCommPort, CLRRTS );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TPCRTCleanup );

    /* --------------------------------------------------------------------
    	Do Purge Twice once for RX and once for RX
    -------------------------------------------------------------------- */
    for( iIdx = 0; iIdx < nTests; iIdx++ )
    {
        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s @ line %d:  Creating Read Write thread to %s."),
                      __THIS_FILE__, __LINE__, (iIdx ? TEXT("WRITE"): TEXT("READ")) );

        /* --------------------------------------------------------------------
        	Initalize control struct and creat thread.
        -------------------------------------------------------------------- */
        IOThreadCtrl.hPort = hCommPort;
        IOThreadCtrl.hEndEvent = (HANDLE)iIdx;
        IOThreadCtrl.dwData = (DWORD)lpOverLapped;
        
        hIOThread = CreateThread( NULL, 0, 
                                     (LPTHREAD_START_ROUTINE)ReadWriteTestThread,
                                     (LPVOID)&IOThreadCtrl, 0, &nIOTime );
        FUNCTION_ERROR( NULL == hIOThread, dwResult = TPR_FAIL; goto TPCRTCleanup );
                                  
        Sleep( nWaitTime );

        bRtn = PurgeComm( hCommPort, aPurgeFlag[iIdx] );
        COMM_ERROR( hCommPort, FALSE == bRtn, goto TPCRTCleanup );

        /* --------------------------------------------------------------------
        	Wait for the thread to end.
        -------------------------------------------------------------------- */
#ifndef PEG
       nIOTime = WaitForSingleObject( hIOThread, INFINITE );
       FUNCTION_ERROR( WAIT_FAILED == nIOTime, dwResult = TPR_FAIL );
       bRtn = GetExitCodeThread( hIOThread, &nIOTime );
#else
        do {
            bRtn = GetExitCodeThread( hIOThread, &nIOTime );
            FUNCTION_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );
            Sleep(200);
        } while( STILL_ACTIVE == nIOTime );
#endif    

        /* ------------------------------------------------------------
            Check Results.
        ------------------------------------------------------------ */
        DEFAULT_ERROR( 0x80000000 == nIOTime, goto TPCRTCleanup  );
        
        if( /*(nIOTime < (nWaitTime - nDeltaTime)) || It may finished because of buffering the data */
            (nIOTime > (nWaitTime + nDeltaTime))   )
        {
            g_pKato->Log( LOG_FAIL, 
                          TEXT("FAIL in %s @ line %d:  PurgeComm, ")
                          TEXT("Took %dms to end IO expected %dms +/- %d ms"),
                          __THIS_FILE__, __LINE__, nIOTime, 
                          nWaitTime, nDeltaTime );
            dwResult = TPR_FAIL;

        } // if ( Timeout out of range ) else

        else
        {
            g_pKato->Log( LOG_DETAIL, 
                          TEXT("In %s @ line %d: Purged IO in %dms,  ")
                          TEXT("Valid range %dms +/- %d ms"),
                          __THIS_FILE__, __LINE__, nIOTime, 
                          nWaitTime, nDeltaTime );
                          
        } // if ( Timeout out of range ) else

    } // end for( iIdx = 0; iIdx < nTest; iIdx++ );

TPCRTCleanup: 
    if( FALSE == bRtn ) dwResult = TPR_FAIL;

#ifdef UNDER_NT
        CloseHandle(  OvrLpd.hEvent );
#endif        

    CloseHandle( hIOThread );

    CloseHandle( hCommPort );

    /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	dwResult = EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );


	return dwResult;
	
} // end TestPurgeCommRxTx( ... ) 

