/**
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tstFlow.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1997  Microsoft Corporation
 
Module Name:
 
	TstFlow.cpp
 
Abstract:
 
	Software Flow Control Testing.
	
	Test #1 - TextXonXoffIdle
		After receiving an XOFF, does the serial driver properly
		sleep while waiting for	an XON?

	Test #2 - TestXonXoffReliable
		Determines 2 things:
			1. How much "slack" data is sent by the sender after it
				receives an XOFF.

			2. Is any data lost between the XOFF and XON.
 

  
	Chris Levin
 
Notes:
 
--*/
#define __THIS_FILE__ TEXT("tstFlow.cpp")

#include "PSerial.h"
#include "TstModem.h"
#include "TstModem.h"


/* -----------------------------------------------------------

	Oct 6 1997.  OemIdle and OemIdleTime? are not supported
				on the cepc platform.  

				I use a low priority thread to determine 
				how much time is spent in an idle state.
				See the IdleThread fcn for details
-------------------------------------------------------------*/

// Used by IdleThread to hold maximum delta time while thread is running
static	DWORD	dwIdleDelta;

// IdleThread stays alive as long as this is TRUE
static	BOOL	bKeepRunning;

// Handle and ID for IdleThread
static	HANDLE	hIdleThread;
static	DWORD	dwThreadId;


#define SLEEPTIME			5000	// milliseconds
#define SENDXOFFATCOUNT		5000	// Receiver will send an XOFF after receiving this many bytes
#define SENDBYTES			10000	// Number of bytes transmitter will send
#define MAXXOFFSLACK		100		// Max # bytes that can be sent after an xoff is sent

#define XON		0x11
#define XOFF	0x13


/*-----------------------------------------------------------------------------
  Function		:DWORD IdleThread(LPVOID *lp)
  Description	:Keeps track of the meximum time that passes between switches
					to this thread on a Sleep(0)
  Parameters	:
  Returns		:Stores the maximum time in the 
					file global variable DWORD dwIdleDelta
  
Comments		:This thread is terminated by setting the file global variable
					bKeepRunning to FALSE
------------------------------------------------------------------------------*/
DWORD IdleThread(LPVOID *lp)
{
	DWORD dwStart = 0, dwEnd = 0, dwDelta = 0, dwTemp = 0;
	
	dwIdleDelta = 0;

	bKeepRunning = TRUE;
	g_pKato->Log(LOG_COMMENT, TEXT("Entering IdleThread()\n"));

	do
	{
		dwStart = GetTickCount();
		if ( dwStart == 0 )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("####In %s @ line %d: GetTickCount() returned 0, trying again\n"),
			  __THIS_FILE__, __LINE__);
			dwStart = GetTickCount();
		}
		
		Sleep(0);
		dwEnd = GetTickCount();

		if ( dwEnd < dwStart )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("####In %s @ line %d: GetTickCount() error end time (%lu) < start time (%lu)"),
			   __THIS_FILE__, __LINE__, dwEnd, dwStart);
		}

		// Compute time other threads were running
		dwTemp = dwEnd - dwStart;
		if ( dwTemp > dwDelta )
		{
			dwDelta = dwTemp;
		}
	} while ( bKeepRunning );

	dwIdleDelta = dwDelta;

	//g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: IdleThread() exiting"),
	//__THIS_FILE__, __LINE__);
	
	ExitThread( 0 );
	g_pKato->Log(LOG_COMMENT, TEXT("Exiting IdleThread()\n"));
	return 0;
}



/*-----------------------------------------------------------------------------
  Function		: TESTPROCAPI CreateIdleThread()
  Description	: Creates the idle timer thread.
  Parameters	:
  Returns		: TPR_FAIL, TPR_PASS
  Comments		: The thread is started in a suspended state.  Use 
					ResumeThread(hIdleThread) to start it.
------------------------------------------------------------------------------*/
TESTPROCAPI CreateIdleThread()
{
    DWORD   dwResult    = TPR_FAIL;

	// Create idle thread
	hIdleThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IdleThread, NULL, CREATE_SUSPENDED, &dwThreadId);
	if ( hIdleThread == NULL )
	{
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Could not create IdleThread()"),
		 __THIS_FILE__, __LINE__);
		return TPR_FAIL;
	}
	g_pKato->Log(LOG_COMMENT, TEXT("IdleThread created - handle = %lu"), hIdleThread);

	// change priority of idle thread
/*	if ( !SetThreadPriority(hIdleThread, THREAD_PRIORITY_IDLE) )
	{
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Could not change IdleThread() priority"),
		 __THIS_FILE__, __LINE__);
		return TPR_FAIL;
	}
*/
	return TPR_PASS;
}


/*-----------------------------------------------------------------------------
  Function		:TESTPROCAPI TestXonXoffIdle( UINT uMsg, TPPARAM tpParam, 
						LPFUNCTION_TABLE_ENTRY lpFTE )
  Description	:This is a serial test that determines how well the serial
					drivers "sleep" while waiting for an XON after receiving
					an XOFF.  If the driver properly suspends itself, the 
					delta idle times computed by IdleThread will be low.  
					If the values are very high or 0 ( 0 = thread never ran )
					then there is a probem.
					
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/
TESTPROCAPI TestXonXoffIdle( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    // Return values
	BOOL			bRtn					= FALSE;
    DWORD			dwResult				= TPR_PASS;

	// Comm port setup vars
    HANDLE			hCommPort				= INVALID_HANDLE_VALUE;
	DCB				dNewDcb;
	COMMTIMEOUTS	cNewTime;

	// Vars used for running time timing
	DWORD			dwStartTickCount;	
	DWORD			dwStopTickCount;
	signed int		dwDelta[]				= { 0,0,0 };
	signed int		dwDeltaChk;
		
	// Generic counter
	int				iX, iY;

	// Used for xmit/rcv of serial data
	TCHAR			cChar					= TEXT('A');
	DWORD			dwBytesWritten;
	DWORD			dwBytesRead;
	TCHAR			*lpBuffer				= NULL;

	// Bluetooth does not support this test
    if(g_fBT)
    {
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Bluetooth does not support this test - skipping."),
			      __THIS_FILE__, __LINE__ );
    	return TPR_SKIP;
    }

    /* --------------------------------------------------------------------
		Open the comm port for this test.
    -------------------------------------------------------------------- */
    hCommPort = CreateFile( g_lpszCommPort, 
                        GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        OPEN_EXISTING, 0, NULL );
    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , return TPR_ABORT );    	
    
		if( !g_fSetCommProp )
		{
			g_pKato->Log( LOG_WARNING, TEXT("WARNING: in %s @ line %d: g_CommProp NOT negotiated using local value."),
					__THIS_FILE__, __LINE__ );
			bRtn =  GetCommProperties( hCommPort, &g_CommProp );
			if ( bRtn == FALSE )
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: GetCommProperties Failed"),
				__THIS_FILE__, __LINE__);
				dwResult = TPR_FAIL;
				goto TXXCleanup;
			}
		} // end if( !g_fSetCommProp )

		bRtn = SetupDefaultPort( hCommPort );
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: SetupDefaultPort Failed"),
			__THIS_FILE__, __LINE__);
			dwResult = TPR_FAIL;
			goto TXXCleanup;
		}

		/* --------------------------------------------------------------------
    		Sync the begining of the test.
		-------------------------------------------------------------------- */
		bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
		DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_ABORT; goto TXXCleanup);

		//
		//
		// TEST 1 - Compute IdleTime Delta for Sleep(SLEEPTIME)
		//
		//

		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Getting baseline delta for Sleep()"),
				__THIS_FILE__, __LINE__);

		// Start idle thread
		if ( CreateIdleThread() == TPR_FAIL )
		{
			goto TXXCleanup;
		}
		ResumeThread(hIdleThread);	

		dwStartTickCount = GetTickCount();
		g_pKato->Log(LOG_COMMENT, TEXT("Going to sleep at %lu"), dwStartTickCount);
		Sleep(SLEEPTIME);
		g_pKato->Log(LOG_COMMENT, TEXT("Waking up..."));
		dwStopTickCount = GetTickCount();
		g_pKato->Log(LOG_COMMENT, TEXT("Woke up at %lu"), dwStopTickCount);

  		// terminate idle thread
		bKeepRunning = FALSE;
		WaitForSingleObject(hIdleThread, 10000);

		// Display results
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sleep(%d) lasted for %d ms"),
				__THIS_FILE__, __LINE__, SLEEPTIME, dwStopTickCount-dwStartTickCount);

		if ( dwIdleDelta != 0 )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: IdleThreadDelta() =  %d ms"),
					__THIS_FILE__, __LINE__, dwIdleDelta);
		}
		else
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: IdleThreadDelta() =  INFINITE"),
					__THIS_FILE__, __LINE__);
		}

		// Store basic sleep idle time in [0]
		dwDelta[0] = dwIdleDelta;


		//
		//
		// TEST 2 - Xmit/Rcv data including and XOFF/XON and compute IdleTime Delta.
		//
		//
	
		// Activate XON/XOFF flow control
		dNewDcb.DCBlength = sizeof(DCB);
		bRtn = GetCommState(hCommPort, &dNewDcb);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: GetCommState() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXCleanup;
		}
		dNewDcb.fOutxCtsFlow	= FALSE;				// turn of CTS flow control
		dNewDcb.fOutxDsrFlow	= FALSE;				// turn of DSR flow control
		dNewDcb.fDsrSensitivity	= FALSE;				// ignore DSR signal
		dNewDcb.fRtsControl		= RTS_CONTROL_DISABLE;	// disable and ignore RTS
		dNewDcb.XonChar			= XON;
		dNewDcb.XoffChar		= XOFF;
		dNewDcb.fOutX			= TRUE;					// use XON/XOFF during xmit
		dNewDcb.fInX			= TRUE;					// use XON/XOFF during rcv
		// RTS-CTS Handshake is nothing to do with Xon/Xoff. One is hardware, Another is software.

		bRtn = SetCommState(hCommPort, &dNewDcb);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: SetCommState() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXCleanup;
		}

		// Set some reasonable timeouts
		bRtn = GetCommTimeouts(hCommPort, &cNewTime);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: GetCommTimeouts() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXCleanup;
		}

		/* Timeouts should be SLEEPTIME + 1000ms so that no one timesout during a sleep */
		cNewTime.ReadIntervalTimeout			= 0;
		cNewTime.ReadTotalTimeoutMultiplier		= 0;
		cNewTime.ReadTotalTimeoutConstant		= SLEEPTIME + 1000;
		cNewTime.WriteTotalTimeoutMultiplier	= 0;
		cNewTime.WriteTotalTimeoutConstant		= SLEEPTIME + 3000;

		bRtn = SetCommTimeouts(hCommPort, &cNewTime);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: SetCommTimeouts() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXCleanup;
		}

		// Run test 2 times - Once without XOFF once with XOFF
		for ( iY = 0; iY < 2; iY++ )
		{
			if ( iY == 1 )
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Getting XON/XOFF delta for Comms..." ),
					__THIS_FILE__, __LINE__);
			}
			else
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Getting baseline delta for Comms..." ),
					__THIS_FILE__, __LINE__);
			}

			// Start idle thread
			if ( CreateIdleThread() == TPR_FAIL )
			{
				goto TXXCleanup;
			}
			ResumeThread(hIdleThread);	
			dwStartTickCount = GetTickCount();

			// This is the TRANSMIT section
			if (g_fMaster)
			{
				Sleep(SLEEPTIME/5); // Master send out data will after receive started.
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending data..." ),
					__THIS_FILE__, __LINE__);
				
				for ( iX = 0; iX < SENDBYTES; iX++)
				{
					DWORD dwStartTicks=GetTickCount();
					if ( (!WriteFile(hCommPort, &cChar, 1, &dwBytesWritten ,NULL)) || ( dwBytesWritten != 1 ) )
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: WriteFile() failed Error=%d.  Wanted to write 1 byte, wrote %d bytes, spend %ld ticks" ),
							__THIS_FILE__, __LINE__, GetLastError(), dwBytesWritten,GetTickCount()-dwStartTicks);
						bRtn = FALSE;
						goto TXXCleanup;
					} // if

					if ( !( iX % 1000 ) && iX )
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sent %d bytes so far   %c" ),
							__THIS_FILE__, __LINE__, iX, cChar);
						cChar++;
						// Give CE machine a chance to catch up
						#if ( !UNDER_CE )
							Sleep(1000);
						#endif
					} // if
				} // for
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sent %d bytes total" ),
				__THIS_FILE__, __LINE__, iX);
			}
			else
			{
				// This is the RECEIVE secion
				lpBuffer = new TCHAR[1];
				*lpBuffer = _T(' ');
				if ( lpBuffer == NULL )
				{	
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Can not allocate receive buffer" ),
							__THIS_FILE__, __LINE__);
					bRtn = FALSE;
					goto TXXCleanup;
				}
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Waiting for data..." ),
					__THIS_FILE__, __LINE__);
		
				// Begin read
				for ( iX = 0; iX < SENDBYTES; iX++ )
				{
					if ( !ReadFile(hCommPort, (LPVOID)lpBuffer, 1, &dwBytesRead, NULL) || ( dwBytesRead != 1 ) )
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: ReadFile() failed Error=%d.  Wanted to read 1 byte, read %d bytes" ), __THIS_FILE__, __LINE__, GetLastError(), dwBytesRead);
						bRtn = FALSE;
						goto TXXCleanup;
					} // if
					
					if ( !( iX % 1000 ) && iX)
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Received %d bytes so far	%c", ),
								__THIS_FILE__, __LINE__, iX, lpBuffer[0]);
					} // if
					
					// send XOFF at SENDXOFFATCOUNT bytes if this is the 2nd pass
					if ( iX == SENDXOFFATCOUNT ) 
					{
						// Only do XON/XOFF testing during second pass
						if ( iY == 1 )
						{
							// Send XOFF
							g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XOFF" ),
									__THIS_FILE__, __LINE__);
							bRtn = TransmitCommChar(hCommPort,XOFF); 
							if ( bRtn == FALSE )
							{
								g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XOFF failed" ),
										__THIS_FILE__, __LINE__);
							}
						}
			
						// Goto sleep
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sleeping for %dms" ),
								__THIS_FILE__, __LINE__, SLEEPTIME);
						Sleep(SLEEPTIME);

						if ( iY == 1 )
						{
							// Send XON
							g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XON" ),
									__THIS_FILE__, __LINE__);
							bRtn = TransmitCommChar(hCommPort,XON); 
							if ( bRtn == FALSE )
							{
								g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XON failed" ),
										__THIS_FILE__, __LINE__);
							}
						}
					} // if ( iX == 5000 && iY == 1)
				} // for
		
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Received %d of %d bytes." ),
						__THIS_FILE__, __LINE__, iX, SENDBYTES);
			} // if (g_fMaster)
	
			//
			//
			// Get timings, stop IdleThread and display results
			//
			//
			dwStopTickCount = GetTickCount();

			// terminate idle thread
			bKeepRunning = FALSE;
			WaitForSingleObject(hIdleThread, 10000);

			// Display results
			if ( iY == 1 )
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: XON/XOFF Communications lasted for %d ms"),
				__THIS_FILE__, __LINE__, dwStopTickCount-dwStartTickCount);
			}
			else
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: BASELINE Communications lasted for %d ms"),
				__THIS_FILE__, __LINE__, dwStopTickCount-dwStartTickCount);
			}

			if ( dwIdleDelta != 0 )
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: IdleThreadDelta() =  %d ms"),
						__THIS_FILE__, __LINE__, dwIdleDelta);
			}
			else
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: IdleThreadDelta() =  INFINITE"),
						__THIS_FILE__, __LINE__);
			}
			dwDelta[iY+1] = dwIdleDelta;
		} // for ( iY = 0; iY < 2; iY++ )

		/* I now have 3 delta values
				dwDelta[0] = Baseline sleep delta
				dwDelta[1] = Serial comms without pausing for an XOFF/XON sequence
				dwDelta[2] = Serial comms with a wait on XOFF/XON sequence
		*/
		/* This is an inital failure test.  If the delta of the XOFF/XON test > 2 times no XOFF/XON
			consider this an error condition
		*/

		dwDeltaChk = dwDelta[2] - dwDelta[1];
		if ( dwDeltaChk < 0 )
		{
			dwDeltaChk = dwDeltaChk * -1;
		}
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Final Delta = %d"),
				__THIS_FILE__, __LINE__, dwDeltaChk);

		if ( dwDeltaChk > 1000 )
		{
			dwResult = TPR_FAIL;
		}

TXXCleanup:
		{
			// Give CE machine a chance to catch up
			// terminate idle thread
			bKeepRunning = FALSE;
			WaitForSingleObject(hIdleThread, 10000);
			#if ( !UNDER_CE )
				Sleep(2000);
			#endif

			if ( lpBuffer != NULL )
			{
				delete[] lpBuffer;
			}

			/* 3/12/98 - I added this code, because although the idle numbers are in the pass range,
			the test occasionaly fails and it may be due to some type of comm error.  I am trying to
			catch it with this code
			*/
			if( (FALSE == bRtn) && ( dwResult == TPR_PASS ) )
			{
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Test passed but bRtn==FALSE"),
					__THIS_FILE__, __LINE__);
				//dwResult = TPR_FAIL;
				dwResult = TPR_PASS;
			}
			else if ( FALSE == bRtn )
			{
				dwResult = TPR_FAIL;
			}

			/* --------------------------------------------------------------------
				Sync the end of a test of the test.
			-------------------------------------------------------------------- */
			dwResult = EndTestSync( hCommPort, lpFTE->dwUniqueID, dwResult );

			if(!g_fBT)
			{
				CloseHandle( hCommPort );
			}
    	} 
	return dwResult;
} // TestXonXoffIdle( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )



/*-----------------------------------------------------------------------------
  Function		: TESTPROCAPI TestXonXoffReliable( UINT uMsg, 
								TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
  Description	:Tests the reliability and latency of XON/XOFF flow control
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/
TESTPROCAPI TestXonXoffReliable( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    // Return values
	BOOL			bRtn					= FALSE;
    DWORD			dwResult				= TPR_PASS;

	// Comm port setup vars
    HANDLE			hCommPort				= INVALID_HANDLE_VALUE;
	DCB				dNewDcb;
	COMMTIMEOUTS	cNewTime;

	// Generic counter
	int				iX;

	// Used for xmit/rcv of serial data
	TCHAR			cChar					= TEXT('A');
	DWORD			dwBytesWritten;
	DWORD			dwBytesRead;
	TCHAR			*lpBuffer				= NULL;

	 // Bluetooth doesn't support this test.
    if(g_fBT)
    {
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Bluetooth does not support this test - skipping."),
			      __THIS_FILE__, __LINE__ );
    	return TPR_SKIP;
    }

    /* --------------------------------------------------------------------
	Open the comm port for this test.
    -------------------------------------------------------------------- */
    hCommPort = CreateFile( g_lpszCommPort, 
                        GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        OPEN_EXISTING, 0, NULL );
    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , return TPR_ABORT );    	

//	{
		if( !g_fSetCommProp )
		{
			g_pKato->Log( LOG_WARNING, TEXT("WARNING: in %s @ line %d: g_CommProp NOT negotiated using local value."),
					__THIS_FILE__, __LINE__ );
			bRtn =  GetCommProperties( hCommPort, &g_CommProp );
			COMM_ERROR( hCommPort, FALSE == bRtn, goto TXXRCleanup);
		} // end if( !g_fSetCommProp )

		bRtn = SetupDefaultPort( hCommPort );
		DEFAULT_ERROR( FALSE == bRtn, goto TXXRCleanup);
    

		/* --------------------------------------------------------------------
    		Sync the begining of the test.
		-------------------------------------------------------------------- */
		bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
		DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_ABORT; goto TXXRCleanup);


		/*---------------------------------------------------------
			Activate XON/XOFF flow control
		----------------------------------------------------------*/
		dNewDcb.DCBlength = sizeof(DCB);
		bRtn = GetCommState(hCommPort, &dNewDcb);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: GetCommState() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXRCleanup;
		}
		dNewDcb.fOutxCtsFlow	= FALSE;				// turn of CTS flow control
		dNewDcb.fOutxDsrFlow	= FALSE;				// turn of DSR flow control
		dNewDcb.fDsrSensitivity	= FALSE;				// ignore DSR signal
		dNewDcb.fRtsControl		= RTS_CONTROL_DISABLE;	// disable and ignore RTS
		dNewDcb.XonChar			= XON;
		dNewDcb.XoffChar		= XOFF;
		dNewDcb.fOutX			= TRUE;					// use XON/XOFF during xmit
		dNewDcb.fInX			= TRUE;					// use XON/XOFF during rcv
		// RTS-CTS Handshake is nothing to do with Xon/Xoff. One is hardware, Another is software.

		bRtn = SetCommState(hCommPort, &dNewDcb);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: SetCommState() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXRCleanup;
		}

		/*---------------------------------------------------------
			Set some reasonable timeouts
		----------------------------------------------------------*/
		bRtn = GetCommTimeouts(hCommPort, &cNewTime);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: GetCommTimeouts() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXRCleanup;
		}
		// Setup buffer.
		bRtn = SetupComm( hCommPort, 2048, 2048);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: GetCommTimeouts() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXRCleanup;
		}
		/* Timeouts should be SLEEPTIME + 1000ms so that no one timesout during a sleep */
		cNewTime.ReadIntervalTimeout			= 0;
		cNewTime.ReadTotalTimeoutMultiplier		= 0;
		cNewTime.ReadTotalTimeoutConstant		= SLEEPTIME + 1000;
		cNewTime.WriteTotalTimeoutMultiplier	= 0;
		cNewTime.WriteTotalTimeoutConstant		= SLEEPTIME + 1000;

		bRtn = SetCommTimeouts(hCommPort, &cNewTime);
		if ( bRtn == FALSE )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: SetCommTimeouts() failed"),
					__THIS_FILE__, __LINE__);
			goto TXXRCleanup;
		}

		/*---------------------------------------------------------
			Actual Test Starts Here
		----------------------------------------------------------*/

		/*---------------------------------------------------------
			TEST 1 - See if XON/XOFF superceeds any WRITE
				timeouts set on the xmit side.  This code will 
				send an XOFF then sleep for 2x the WriteTimeout
				time.  WriteFile should timeout.  NOTE: it returns
				true, so you have to check actual bytes written
		----------------------------------------------------------*/
		// This is the TRANSMIT section
		if (g_fMaster)
		{
			Sleep(SLEEPTIME/5);// Master send out data will after receive started.

			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending data..." ),
				__THIS_FILE__, __LINE__);
		
			for ( iX = 0; iX < SENDBYTES; iX++)
			{
				DWORD dwStartTicks=GetTickCount();
				if ( (!WriteFile(hCommPort, &cChar, 1, &dwBytesWritten ,NULL)) || ( dwBytesWritten != 1 ) )
				{
					// At 5000 bytes, receiver sends and XOFF, allow some latitude for a WriteFile failure
					if ( iX >= SENDXOFFATCOUNT && iX < (SENDXOFFATCOUNT + 4096) )
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: WriteFile() Timed Out @ %d Bytes. Max Allowed = %d bytes, " ),
								__THIS_FILE__, __LINE__, iX, SENDXOFFATCOUNT + 4096 );
						DWORD dwErr;
						dwErr = CE_IOE;
						ClearCommError(hCommPort, &dwErr, NULL);
						Sleep(SLEEPTIME / 2); // Let receiver wakeup again.
						iX--; // Retransmit the timed out byte
						continue;
					}
					else
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: WriteFile() failed @ %d Bytes,spend %ld ticks" ),
							__THIS_FILE__, __LINE__, iX,GetTickCount()-dwStartTicks );
						bRtn = FALSE;
					}
					goto TXXRCleanup;
				} // if

				if ( !( iX % 1000 ) && iX )
				{
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sent %d bytes so far   %c" ),
						__THIS_FILE__, __LINE__, iX, cChar);
					cChar++;
				} // if
			} // for
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sent %d of %d bytes." ),
				__THIS_FILE__, __LINE__, iX, SENDBYTES);
		}
		else
		{
			// This is the RECEIVE secion
			lpBuffer = new TCHAR[1];
			if ( lpBuffer == NULL )
			{	
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Can not allocate receive buffer" ),
						__THIS_FILE__, __LINE__);
				bRtn = FALSE;
				goto TXXRCleanup;
			}
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Waiting for data..." ),
				__THIS_FILE__, __LINE__);
		
			// Begin read
			for ( iX = 0; iX < SENDBYTES; iX++ )
			{
				bRtn = ReadFile(hCommPort, (LPVOID)lpBuffer, 1, &dwBytesRead, NULL);
				if ( (bRtn == FALSE) || (dwBytesRead == 0) )
				{
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: ReadFile() failed @ %ld Bytes"),
							__THIS_FILE__, __LINE__,iX);
					bRtn=FALSE;
					goto TXXRCleanup;
				} // if
				
				if ( !( iX % 1000 ) && iX)
				{
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Received %d bytes so far	%c", ),
							__THIS_FILE__, __LINE__, iX, lpBuffer[0]);
				} // if
				
				// send XOFF halfway through reception
				if ( iX == SENDXOFFATCOUNT ) 
				{
					// Send XOFF
					bRtn = TransmitCommChar(hCommPort,XOFF); 
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XOFF" ),
							__THIS_FILE__, __LINE__);
					if ( bRtn == FALSE )
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XOFF failed" ),
								__THIS_FILE__, __LINE__);
					}

		
					// Goto sleep for longer than a read or write timeout
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sleeping for %dms" ),
							__THIS_FILE__, __LINE__, SLEEPTIME*2);
					Sleep(SLEEPTIME*2);

					// Send XON
					bRtn = TransmitCommChar(hCommPort,XON); 
					g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XON" ),
							__THIS_FILE__, __LINE__);
					
					if ( bRtn == FALSE )
					{
						g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sending XON failed" ),
								__THIS_FILE__, __LINE__);
					}

				} // if ( iX == 5000 )
			} // for ( iX = 0; iX < 10000; iX++ )
		
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Received %d of %d bytes." ),
					__THIS_FILE__, __LINE__, iX, SENDBYTES);
		} // if (g_fMaster)

	TXXRCleanup:
		{
			// Give CE machine a chance to catch up
			#if ( !UNDER_CE )
				Sleep(2000);
			#endif

			if ( lpBuffer != NULL )
			{
				delete[] lpBuffer;
			}
			if( FALSE == bRtn ) dwResult = TPR_FAIL;

			/* --------------------------------------------------------------------
				Sync the end of a test of the test.
			-------------------------------------------------------------------- */
			#if ( !UNDER_CE )
				Sleep(2000);
			#endif
			dwResult = EndTestSync( hCommPort, lpFTE->dwUniqueID, dwResult );

			CloseHandle( hCommPort );
    
	}

	return dwResult;
}
