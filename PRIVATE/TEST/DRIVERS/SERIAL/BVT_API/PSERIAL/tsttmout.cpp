/**
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tsttmout.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	TstTmOut.cpp
 
Abstract:
 
	These test test the Read and Write Timeouts.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#define __THIS_FILE__ TEXT("TstTmOut.cpp")

#include "PSerial.h"
#include "TstModem.h"
#include "TstModem.h"

/*++
 
TestReadTimeout:
 
    Tests the duration of read time outs in serveral configurations.
    
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI TestReadTimeouts( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL            bRtn            = FALSE;
    DWORD           dwResult        = TPR_PASS;
    HANDLE          hCommPort       = INVALID_HANDLE_VALUE;
    DWORD           dwBytes         = 0;
    DWORD           nStartTick      = 0;
    DWORD           nEndTick        = 0;
    DWORD           nTotalTicks     = 0;
    DWORD           nDeltaTicks     = 0;
    DWORD           nExpectedTicks  = 0;
    INT             iIdx;
    COMMTIMEOUTS    cto;
    BOOL            fSending        = FALSE;
    const int       nBuffSize       = 100;
    BYTE            aBuffer[nBuffSize];
	// 
    const int       nTimeoutTests   = 1;
    
	struct TESTREADTIMEOUTS {
	DWORD   ReadMultiplier;
	DWORD   ReadConstant;
    } aTestReadTimeouts[nTimeoutTests] = {
// { 50,   0   },
// { 40, 1000  },  
	{  0, 5000  } };
    
    /* --------------------------------------------------------------------
	Open the comm port for this test.
    -------------------------------------------------------------------- */
    if(g_fBT && g_fMaster)
    {
    	g_hBTPort = CreateFile( g_lpszCommPort, 
			                GENERIC_READ | GENERIC_WRITE, 0, NULL,
			                OPEN_EXISTING, 0, NULL );
		hCommPort = g_hBTPort;
    }
    else
    {
    	INT i = 0;
    	
	    hCommPort = CreateFile( g_lpszCommPort, 
                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, 0, NULL );

   // If client could not connect it could be because master has not done
	    // CreateFile yet, let's time out and try a bunch of times
		while(hCommPort == INVALID_HANDLE_VALUE && i < MAX_BT_CONN_ATTEMPTS)
		{
			Sleep(BT_CONN_ATTEMPT_TIMEOUT);

			hCommPort = CreateFile( g_lpszCommPort, 
	                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                                OPEN_EXISTING, 0, NULL );
			i++;
		}
    }

    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , return TPR_ABORT );    	


    if( !g_fSetCommProp )
    {
	g_pKato->Log( LOG_WARNING, 
		      TEXT("WARNING: in %s @ line %d: g_CommProp NOT negotiated using local value."),
		      __THIS_FILE__, __LINE__ );
	bRtn =  GetCommProperties( hCommPort, &g_CommProp );
	COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup );
	
    } // end if( !g_fSetCommProp )

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TRTCleanup);
    
    /* --------------------------------------------------------------------
	Sync the begining of the test.
    -------------------------------------------------------------------- */
	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_ABORT; goto TRTCleanup);

    /* --------------------------------------------------------------------
	Test the immediate return read.
    -------------------------------------------------------------------- */
    cto.ReadIntervalTimeout         = MAXDWORD; 
    cto.ReadTotalTimeoutMultiplier  = 0; 
    cto.ReadTotalTimeoutConstant    = 0; 
    cto.WriteTotalTimeoutMultiplier = 0; 
    cto.WriteTotalTimeoutConstant   = 0; 
    bRtn = SetCommTimeouts( hCommPort, &cto );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);
   
    nStartTick = GetTickCount();
    bRtn = ReadFile( hCommPort, aBuffer, nBuffSize, &dwBytes, NULL );
    nEndTick = GetTickCount();
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);

    nTotalTicks = nEndTick - nStartTick;

    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Took %d ms to pole for %d bytes."),
		 __THIS_FILE__, __LINE__, nTotalTicks, dwBytes );

    /* --------------------------------------------------------------------
	Test inter-char interval timing.
    -------------------------------------------------------------------- */
	if( PCF_INTTIMEOUTS & g_CommProp.dwProvCapabilities )
	{
	cto.ReadIntervalTimeout         = 100;
	cto.ReadTotalTimeoutConstant    = 10000;
	bRtn = SetCommTimeouts( hCommPort, &cto );
	COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);

	g_pKato->Log( LOG_DETAIL, 
		      TEXT("In %s @ line %d:  Testing interdigit timeout of %d ms"),
		      __THIS_FILE__, __LINE__, cto.ReadIntervalTimeout );
    
	fSending = g_fMaster;
	for( iIdx = 1; iIdx <= 2; iIdx++ )
	{
	    if( fSending )
	    {
			// 8-sep-97 - On a slow CE machine, it may take some time for it to get into
			// ReadFile, so wait 2 seconds before sending the first byte
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Sender Waiting 2000ms before sending."), __THIS_FILE__, __LINE__ );
			Sleep( 2000 );
			fSending = FALSE;


//              CMDLINENUM( bRtn = TransmitCommChar( hCommPort, 'X' ) );
		aBuffer[0] = 'X';
		CMDLINENUM( bRtn = WriteFile( hCommPort, aBuffer, 1, &dwBytes, NULL) );
		DEFAULT_ERROR( 1 != dwBytes, goto TRTCleanup);
		COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);
		
		Sleep( 3 * cto.ReadIntervalTimeout );
		
//              CMDLINENUM( bRtn = TransmitCommChar( hCommPort, 'y' ) );
		aBuffer[0] = 'y';
		CMDLINENUM( bRtn = WriteFile( hCommPort, aBuffer, 1, &dwBytes, NULL ) );
		DEFAULT_ERROR( 1 != dwBytes, goto TRTCleanup);
		COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);

	    } // end if( fSending )            

	    else
	    {
//		Sleep(50);									// work around Thumb optimizer 
		fSending = TRUE;
				g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Receiver Starting Timeout Test."),
					__THIS_FILE__, __LINE__ );
		nStartTick = GetTickCount();
		bRtn = ReadFile( hCommPort, aBuffer, 2, &dwBytes, NULL );
		COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);
				nEndTick = GetTickCount();

			CMDLINENUM( nTotalTicks = nEndTick - nStartTick );

			if( 2 == dwBytes ) 
			{
			    g_pKato->Log( LOG_FAIL, 
					  TEXT("FAIL in %s @ line %d: Read didn't time out.  ")
					  TEXT("Read 2, \"%c%c\", bytes in %d ms."),
					  __THIS_FILE__, __LINE__, aBuffer[0], aBuffer[1],
					  nEndTick - nStartTick );
			    dwResult = TPR_FAIL;

			} // end if( 2 == dwBytes )

			else if( 0 == dwBytes )
			{
			    LOGLINENUM();
			    g_pKato->Log( LOG_FAIL, 
					  TEXT("FAIL in %s @ line %d: Didn't recieve first character, timed out in %d ms"),
					  __THIS_FILE__, __LINE__, nTotalTicks );
			    dwResult = TPR_FAIL;

			} // end if( 2 == dwBytes ) else if( nTotalTicks > SLEEPTIME )

			else
			{
			    g_pKato->Log( LOG_DETAIL, 
					  TEXT("In %s @ line %d: Read timed out in %d ms"),
					  __THIS_FILE__, __LINE__, nTotalTicks );

			} // end if( 2 == dwBytes ) else if( nTotalTicks > SLEEPTIME ) else
	    
	    } //  if( fSending ) else

	} // end for( iIdx = 1; iIdx <= 2; iIdx++ )

    }  // end if( PCF_INTTIMEOUTS & g_CommProp.dwProvCapabilities )

	else 
	{
	g_pKato->Log( LOG_WARNING, 
		      TEXT("FAIL in %s @ line %d: Enviroment doesn't support interval timeouts"),
		      __THIS_FILE__, __LINE__ );
		      
	} // end if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities ) else

    /* --------------------------------------------------------------------
	Test total timeouts
    -------------------------------------------------------------------- */
    //bRtn = PurgeComm( hCommPort, 
    //                  PURGE_TXABORT | PURGE_RXABORT | 
    //                  PURGE_RXCLEAR | PURGE_TXCLEAR );

    COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);
    
	if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities )
	{
	cto.ReadIntervalTimeout         = 0; 

	for( iIdx = 0; iIdx < nTimeoutTests; iIdx++ )
	{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Pass %d Testing Total Timeouts."), __THIS_FILE__, __LINE__, iIdx );

	    // Set comm timeouts
	    cto.ReadTotalTimeoutMultiplier  = aTestReadTimeouts[iIdx].ReadMultiplier; 
	    cto.ReadTotalTimeoutConstant    = aTestReadTimeouts[iIdx].ReadConstant; 
	    bRtn = SetCommTimeouts( hCommPort, &cto );
	    COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup);

	    g_pKato->Log( LOG_DETAIL, 
			  TEXT("In %s @ line %d: Read timeout of ")
			  TEXT("%d bytes * %dms + %dms"),
			  __THIS_FILE__, __LINE__, nBuffSize, 
			  aTestReadTimeouts[iIdx].ReadMultiplier, 
			  aTestReadTimeouts[iIdx].ReadConstant );
			  
	    // Perform the read
	    nStartTick = GetTickCount();
	    bRtn = ReadFile( hCommPort, aBuffer, nBuffSize, &dwBytes, NULL );
	    nEndTick = GetTickCount();
	    COMM_ERROR( hCommPort, FALSE == bRtn, goto TRTCleanup );

	    g_pKato->Log( LOG_DETAIL, 
			  TEXT("In %s @ line %d: ReadFile Return value(%ld) return Bytes(%ld) "),
			  __THIS_FILE__, __LINE__, 
			  bRtn,dwBytes);
	    // Calculate results
	    nExpectedTicks = aTestReadTimeouts[iIdx].ReadMultiplier * nBuffSize +
			     aTestReadTimeouts[iIdx].ReadConstant;
	    nDeltaTicks = nExpectedTicks / 10;                             
	    nTotalTicks = nEndTick - nStartTick;
	    
	    /* ------------------------------------------------------------
		Check Results.
	    ------------------------------------------------------------ */
	    if( (nTotalTicks < (nExpectedTicks - nDeltaTicks)) ||
		(nTotalTicks > (nExpectedTicks + nDeltaTicks))   )
	    {
		g_pKato->Log( LOG_FAIL, 
			      TEXT("FAIL in %s @ line %d: Read Timeout, ")
			      TEXT("Took %dms to timeout expected %dms +/- %d ms"),
			      __THIS_FILE__, __LINE__, nTotalTicks, 
			      nExpectedTicks, nDeltaTicks );
		dwResult = TPR_FAIL;

	    } // if Timeout out of range.

	    else
	    {
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Read Timed out in %dms,  ")
			      TEXT("Valid range %dms +/- %d ms"),
			      __THIS_FILE__, __LINE__, nTotalTicks, 
			      nExpectedTicks, nDeltaTicks );
			      
	    }  // if ( Timeout out of range ) else


	}  // end for( iIdx = 1; iIdx <= nTimeoutTests; iIdx++ )

	} // end if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities )

	else 
	{
	g_pKato->Log( LOG_WARNING, 
		      TEXT("WARNING in %s @ line %d: Enviroment doesn't support total timeouts"),
		      __THIS_FILE__ );
		      
	} // end if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities ) else
  
    TRTCleanup:

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    /* --------------------------------------------------------------------
	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	dwResult = EndTestSync( hCommPort, lpFTE->dwUniqueID, dwResult );

	// Bluetooth handles are closed in EndTestSync
	if(!g_fBT)
	{
    	CloseHandle( hCommPort );
	}
    

	return dwResult;
	
} // end TestReadTimeouts( ... ) 


/*++
 
TestWriteTimeout:
 
    Tests the duration of write time outs in serveral configurations.
    
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI TestWriteTimeouts( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL            bRtn            = FALSE;
    DWORD           dwResult        = TPR_PASS;
    HANDLE          hCommPort       = INVALID_HANDLE_VALUE;
    DWORD           dwBytes         = 0;
    DWORD           nStartTick      = 0;
    DWORD           nEndTick        = 0;
    DWORD           nTotalTicks     = 0;
    DWORD           nDeltaTicks     = 0;
    DWORD           nExpectedTicks  = 0;
    INT             iIdx;
    COMMTIMEOUTS    cto;
    DCB             LocalDcb;
    BOOL            fSending        = FALSE;
    const int       nBuffSize       = 100;
    BYTE            aBuffer[nBuffSize];
    const int       nTimeoutTests   = 3;
    struct TESTWRITETIMEOUTS {
	DWORD   WriteMultiplier;
	DWORD   WriteConstant;
    } aTestWriteTimeouts[nTimeoutTests] = {
	{ 50,   0   },
	{ 40, 1000  },  
	{  0, 5000  } };

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

    __try {

    if( !g_fSetCommProp )
    {
	g_pKato->Log( LOG_WARNING, 
		      TEXT("WARNING: in %s @ line %d: g_CommProp NOT negotiated using local value."),
		      __THIS_FILE__, __LINE__ );
	bRtn =  GetCommProperties( hCommPort, &g_CommProp );
	COMM_ERROR( hCommPort, FALSE == bRtn, __leave );
	
    } // end if( !g_fSetCommProp )

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, __leave );
    
    /* --------------------------------------------------------------------
	Sync the begining of the test.
    -------------------------------------------------------------------- */
	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_ABORT; __leave );

    /* --------------------------------------------------------------------
	Test total timeouts
    -------------------------------------------------------------------- */
    GetCommTimeouts( hCommPort, &cto );

    /* -------------------------------------------------------------------
	Set Comm Properties for CTS-RTS.
    ------------------------------------------------------------------- */
    bRtn = GetCommState( hCommPort, &LocalDcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, __leave );

    LocalDcb.fOutxCtsFlow = TRUE;

    bRtn = SetCommState( hCommPort, &LocalDcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, __leave );

    bRtn = EscapeCommFunction( hCommPort, CLRRTS );
    COMM_ERROR( hCommPort, FALSE == bRtn, __leave );

    Sleep( 2000 );
    
	if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities )
	{
	
	for( iIdx = 0; iIdx < nTimeoutTests; iIdx++ )
	{
	    // Set comm timeouts
	    cto.WriteTotalTimeoutMultiplier  = 
			aTestWriteTimeouts[iIdx].WriteMultiplier; 
	    cto.WriteTotalTimeoutConstant    = 
			aTestWriteTimeouts[iIdx].WriteConstant; 
	    bRtn = SetCommTimeouts( hCommPort, &cto );
	    COMM_ERROR( hCommPort, FALSE == bRtn, __leave );

	    g_pKato->Log( LOG_DETAIL, 
			  TEXT("In %s @ line %d: Write timeout of ")
			  TEXT("%d bytes * %dms + %dms"),
			  __THIS_FILE__, __LINE__, nBuffSize, 
			  aTestWriteTimeouts[iIdx].WriteMultiplier, 
			  aTestWriteTimeouts[iIdx].WriteConstant );
			  
	    // Perform the Write
	    nStartTick = GetTickCount();
	    bRtn = WriteFile( hCommPort, aBuffer, nBuffSize, &dwBytes, NULL );
	    nEndTick = GetTickCount();
	    COMM_ERROR( hCommPort, FALSE == bRtn, __leave );

	    // Calculate results
	    nExpectedTicks = aTestWriteTimeouts[iIdx].WriteMultiplier * nBuffSize +
			     aTestWriteTimeouts[iIdx].WriteConstant;
	    nDeltaTicks = nExpectedTicks / 10;                             
	    nTotalTicks = nEndTick - nStartTick;
	    
	    /* ------------------------------------------------------------
		Check Results.
	    ------------------------------------------------------------ */
	    if( nBuffSize == dwBytes )
	    {
		g_pKato->Log( LOG_FAIL, 
			      TEXT("FAIL in %s @ line %d: Write Succeeded CTS ignored!  ")
			      TEXT("Took %dms to write %d bytes of %d bytes"),
			      __THIS_FILE__, __LINE__, nTotalTicks, 
			      dwBytes, nBuffSize );
		dwResult = TPR_FAIL;

	    }
	    
	    else if( (nTotalTicks < (nExpectedTicks - nDeltaTicks)) ||
		     (nTotalTicks > (nExpectedTicks + nDeltaTicks))   )
	    {
		g_pKato->Log( LOG_FAIL, 
			      TEXT("FAIL in %s @ line %d: Write Timeout %d bytes of %d bytes, ")
			      TEXT("Took %dms to timeout expected %dms +/- %d ms"),
			      __THIS_FILE__, __LINE__, dwBytes, nBuffSize,
			      nTotalTicks, nExpectedTicks, nDeltaTicks );
		dwResult = TPR_FAIL;

	    } // if ( Timeout out of range ) else

	    else
	    {
		g_pKato->Log( LOG_DETAIL, 
			      TEXT("In %s @ line %d: Write Timed out in %dms,  ")
			      TEXT("Valid range %dms +/- %d ms"),
			      __THIS_FILE__, __LINE__, nTotalTicks, 
			      nExpectedTicks, nDeltaTicks );
			      
	    } // if ( Timeout out of range ) else

	}  // end for( iIdx = 1; iIdx <= nTimeoutTests; iIdx++ )

	} // end if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities )

	else 
	{
	g_pKato->Log( LOG_WARNING, 
		      TEXT("FAIL in %s @ line %d: Enviroment doesn't support total timeouts"),
		      __THIS_FILE__ );
		      
	} // end if( PCF_TOTALTIMEOUTS & g_CommProp.dwProvCapabilities ) else

    bRtn = PurgeComm( hCommPort, 
		      PURGE_TXABORT | PURGE_RXABORT | 
		      PURGE_RXCLEAR | PURGE_TXCLEAR );
    COMM_ERROR( hCommPort, FALSE == bRtn, __leave );
   
    } __finally {

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    /* --------------------------------------------------------------------
	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	dwResult = EndTestSync( hCommPort, lpFTE->dwUniqueID, dwResult );

	// Bluetooth handles are closed in EndTestSync
	if(!g_fBT)
	{
    	CloseHandle( hCommPort );
	}
    
	} 
	return dwResult;
	
} // end TestWriteTimeouts( ... ) 

