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
#define __THIS_FILE__ TEXT("PStests.cpp")

#include "PSerial.h"
//#include "GSerial.h"
#include <stdio.h>
#include "TstModem.h"
#include "TstModem.h"

/*++
 
TempleteTest:
 
	This test is a templete for other tests it simply
	syncs with its peer and then ends the tests.
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI TempleteTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    DWORD   dwResult    = TPR_FAIL;
	UINT	uiStart, uiStop;
	HANDLE      hCommPort   = NULL;

    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */
	bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

	// With bluetooth it does not make sense to do this test
	// as the master.  Since CreateFile will fail on the client
	// if the master is not connected, this is not practical for
	// the master.
		
	if(!g_fBT || !g_fMaster)
	{
		INT i = 0;
		
		uiStart = GetTickCount();
	    hCommPort = CreateFile( g_lpszCommPort, 
	                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                                OPEN_EXISTING, 0, NULL );
		if(hCommPort != INVALID_HANDLE_VALUE)
		{
			CloseHandle( hCommPort );
			uiStop = GetTickCount();

			g_pKato->Log( LOG_DETAIL, 
		                 TEXT("In %s @ line %d:  Port Open/Close Time = %dms"),
		                 __THIS_FILE__, __LINE__, (uiStop-uiStart) );

			dwResult = TPR_PASS;
		}
	}
	else
	{
		dwResult = TPR_PASS;
	}

   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
} // end TempleteTest( ... ) 


/*++
 
NegotiateSerialProperties:
 
	This test will negotiate with the peer to figure out what serial properties
	the two sides have in common.  This allows the tests to use the capablities
	that they have only in common.
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI NegotiateSerialProperties( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
	
    BOOL        bRtn        = FALSE;
    DWORD       dwResult    = TPR_FAIL;
    HANDLE      hCommPort   = NULL;
    DWORD       dwBytes     = 0;
    COMMPROP    CommProperties;

    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */

	bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
    
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT );

	if(g_fBT && g_fMaster)
	{
		hCommPort = g_hBTPort;
	}
	else
	{
	    hCommPort = CreateFile( g_lpszCommPort, 
	                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                            OPEN_EXISTING, 0, NULL );
	}

    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , return TPR_ABORT );


    bRtn = SetupDefaultPort( hCommPort );
    COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto NSPCleanup);

    bRtn = GetCommProperties( hCommPort, &g_CommProp );
	COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT;goto NSPCleanup);
     /* -------------------------------------------------------------------
     	This is set here because it says that the struct is filled it
     	doesn't indicate that the negotiation succeeded. 
     ------------------------------------------------------------------- */
    g_fSetCommProp = TRUE;

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Comm Properties BEFORE Negotiation"),
                  __THIS_FILE__, __LINE__ );
    DumpCommProp( &g_CommProp );

    // Master sends the CommProperties first
	if( g_fMaster )
	{
        bRtn = TstModemSendBuffer( hCommPort, (LPCVOID)&g_CommProp, 
                                 sizeof( g_CommProp ), &dwBytes );
        DEFAULT_ERROR( FALSE == bRtn, goto NSPCleanup);
        DEFAULT_ERROR( sizeof( g_CommProp ) != dwBytes, dwResult = TPR_FAIL; goto NSPCleanup);
	} // end if( g_fMaster )

    bRtn = TstModemReceiveBuffer( hCommPort, (LPVOID)&CommProperties, 
                                sizeof( CommProperties ), &dwBytes );
    DEFAULT_ERROR( FALSE == bRtn, goto NSPCleanup);                                 
    DEFAULT_ERROR( sizeof( CommProperties ) != dwBytes, dwResult = TPR_FAIL; goto NSPCleanup);

    /* --------------------------------------------------------------------
    	Set properties.
    -------------------------------------------------------------------- */
    g_CommProp.dwMaxBaud           =   min( CommProperties.dwMaxBaud, 
                                                 g_CommProp.dwMaxBaud );
    g_CommProp.dwProvCapabilities  &=  CommProperties.dwProvCapabilities; 
    g_CommProp.dwSettableParams    &=  CommProperties.dwSettableParams;
    g_CommProp.dwSettableBaud      &=  CommProperties.dwSettableBaud;
    g_CommProp.wSettableData       &=  CommProperties.wSettableData;
    g_CommProp.wSettableStopParity &=  CommProperties.wSettableStopParity; 

	if( !g_fMaster )
	{
        bRtn = TstModemSendBuffer( hCommPort, (LPCVOID)&g_CommProp, 
                                 sizeof( g_CommProp ), &dwBytes );
        FUNCTION_ERROR( FALSE == bRtn, goto NSPCleanup);
        DEFAULT_ERROR( sizeof( g_CommProp ) != dwBytes, dwResult = TPR_FAIL; goto NSPCleanup);
        
	} // end if( !g_fMaster )

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Comm Properties AFTER Negotiation"),
                  __THIS_FILE__, __LINE__ );
    DumpCommProp( &g_CommProp );
    
	dwResult = TPR_PASS;

NSPCleanup:
    if( (!g_fBT || !g_fMaster) && hCommPort )
    {
        /* --------------------------------------------------------------------
    	    Sync the end of a test of the test.
        -------------------------------------------------------------------- */

        bRtn = CloseHandle( hCommPort );
        FUNCTION_ERROR( FALSE == bRtn, dwResult = WorseResult( TPR_ABORT, dwResult ));
    }

    if( FALSE == bRtn && dwResult != TPR_ABORT ) dwResult = TPR_FAIL;


	dwResult = EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
	
    return dwResult;
    
} // end NegotiateSerialProperties( ... ) 


/*++
 
TestReadDataParityAndStop:
 
	This function does a configuration test of the rates,
	data bits, parity, and stops.
	
	
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI TestReadDataParityAndStop( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
	
    /* -------------------------------------------------------------------
        Test work begins here
    ------------------------------------------------------------------- */
    BOOL            bRtn        = FALSE;
    DWORD           dwResult    = TPR_PASS;
    HANDLE          hCommPort   = NULL;
    DWORD           dwBytes     = 0;
    INT             iDataIdx;
    INT             iParityIdx;
    INT             iBaudIdx;
    INT             iIdx;
    INT             iTestIdx;
    BOOL            fSending = FALSE;
    BYTE            lpBuffer[TSTMODEM_DATASIZE];
    DCB             LocalDCB;
    COMMTIMEOUTS    LocalCTO;
    DWORD           nLowestBaud;
    
    /* --------------------------------------------------------------------
    	Initalize the slowest speed tested and open the port.
    -------------------------------------------------------------------- */
    nLowestBaud = lpFTE->dwUserData;    

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
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL: in %s @ line %d: g_CommProp NOT negotiated."),
                      __THIS_FILE__, __LINE__ );
        goto TRPSCleanup;        
    } // end if( !g_fSetCommProp )
    

    /* --------------------------------------------------------------------
    	Sync the begining of the test
    -------------------------------------------------------------------- */

    bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );

	DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_ABORT; goto TRPSCleanup);

	bRtn = SetupDefaultPort( hCommPort );
	DEFAULT_ERROR( FALSE == bRtn, goto TRPSCleanup);

    LocalDCB.DCBlength = sizeof( LocalDCB );
	bRtn = GetCommState( hCommPort, &LocalDCB );
    COMM_ERROR( hCommPort,  FALSE == bRtn, goto TRPSCleanup);

     /* -------------------------------------------------------------------
     	For each cbaud rate onfiguration option.
		(Bauds are run backwards for some reason - ??? set it up this way)
     ------------------------------------------------------------------- */
     for( iBaudIdx = NUMBAUDS-1; iBaudIdx >= 0; iBaudIdx-- )
     {
        /* ----------------------------------------------------------------
        	Skip unsupported Baud.
        ---------------------------------------------------------------- */
        if( !(g_BaudTable[iBaudIdx].dwFlag & g_CommProp.dwSettableBaud) )
            continue;

        /* ----------------------------------------------------------------
        	The lowest baud can be passed to the test.
        ---------------------------------------------------------------- */
        if( g_BaudTable[iBaudIdx].dwBaud < nLowestBaud ) break;

        /* ----------------------------------------------------------------
        	Set Data Bits Idx.
        ---------------------------------------------------------------- */
        iDataIdx = iBaudIdx % NUMDATABITS;    
        while( !(g_DataBitsTable[iDataIdx].dwFlag & g_CommProp.wSettableData) )
		{
            iDataIdx = ++iDataIdx % NUMDATABITS;
		}

        /* ----------------------------------------------------------------
        	Set Parity
        ---------------------------------------------------------------- */
        iParityIdx = iBaudIdx % STOPBITSOFFSET;
        while( !(g_ParityStopBitsTable[iParityIdx].dwFlag & g_CommProp.wSettableStopParity) )
		{
            iParityIdx = ++iParityIdx % STOPBITSOFFSET;    
		}

	
        /* ----------------------------------------------------------------
        	This is where we skip things.
        ---------------------------------------------------------------- */
        // Hack to get around space parity.
#if 0        
        iParityIdx = (iParityIdx == 4)  ? 0: iParityIdx;

		// 56000 baud does not have NT support

        if( CBR_56000 == g_BaudTable[iBaudIdx].dwBaud )
        {
            g_pKato->Log( LOG_WARNING,
                          TEXT("WARNING in %s @ line %d: Skipping %s"),
                          __THIS_FILE__, __LINE__, g_BaudTable[iBaudIdx].ptszString );
            continue;                          
        } // if( CBR_56000 = g_BaudTable[iBaudIdx].dwBaud )

#endif

        /* ----------------------------------------------------------------
            Extra time for slow baud rates.        	
        ---------------------------------------------------------------- */
        if( 1200 > LocalDCB.BaudRate ) Sleep( 1000 );

        bRtn = ClearCommError( hCommPort, &dwBytes, NULL );
        DEFAULT_ERROR( FALSE == bRtn, goto TRPSCleanup);
        if( dwBytes )
        {
			#ifdef UNDER_NT
				/* NT incorectly generates a frame error when device changes baud rate.  This does
					not prevent communications and should not abort the test
				*/
				if ( dwBytes != CE_FRAME )
				{
					LOGLINENUM();
					ShowCommError( dwBytes );
				}
			#else
				LOGLINENUM();
				ShowCommError( dwBytes );
			#endif
        }

        bRtn = PurgeComm( hCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | 
                          PURGE_RXCLEAR | PURGE_TXCLEAR );

        bRtn = ClearCommError( hCommPort, &dwBytes, NULL );
        DEFAULT_ERROR( FALSE == bRtn, goto TRPSCleanup);
        if( dwBytes )
        {
           g_pKato->Log( LOG_WARNING,
                          TEXT("WARNING in %s @ line %d: COMM ERROR HERE!"),
                          __THIS_FILE__, __LINE__ );

            LOGLINENUM();
            ShowCommError( dwBytes );
        }
        
        /* ----------------------------------------------------------------
        	Set the DCB
        ---------------------------------------------------------------- */
        LocalDCB.BaudRate       = g_BaudTable[iBaudIdx].dwBaud;

#if 0
        if( 8 != g_DataBitsTable[iDataIdx].bDCBFlag )
        {
            g_pKato->Log( LOG_WARNING,
                          TEXT("WARNING in %s @ line %d: Forcing %d bit data to 8 bit"),
                          __THIS_FILE__, __LINE__, g_DataBitsTable[iDataIdx].bDCBFlag );
            LocalDCB.ByteSize       = 8;
                          
        }
        else
        {
            LocalDCB.ByteSize       = g_DataBitsTable[iDataIdx].bDCBFlag;
        }

        if( NOPARITY != g_ParityStopBitsTable[iParityIdx].bDCBFlag )
        {
            g_pKato->Log( LOG_WARNING,
                          TEXT("WARNING in %s @ line %d: Forcing %s parity to NO parity"),
                          __THIS_FILE__, __LINE__, g_ParityStopBitsTable[iParityIdx].ptszString );
			LocalDCB.Parity = NOPARITY;

		}
		else
        {
        	LocalDCB.Parity = g_ParityStopBitsTable[iParityIdx].bDCBFlag;               
		}        	
#endif		

      	LocalDCB.StopBits = g_ParityStopBitsTable[STOPBITSOFFSET].bDCBFlag;

        LogDataFormat( &LocalDCB );

        do {    // while( COMM ERRORS )
            bRtn = SetCommState( hCommPort, &LocalDCB );
            bRtn = ClearCommError( hCommPort, &dwBytes, NULL );
            DEFAULT_ERROR( FALSE == bRtn, goto TRPSCleanup);
            if( dwBytes )   CMDLINENUM( ShowCommError( dwBytes ) );

        } while( dwBytes );

        /* ----------------------------------------------------------------
            Lets see if this clears the problem.        	
        ---------------------------------------------------------------- */
        if( 1200 > LocalDCB.BaudRate ) Sleep( 1000 );

        bRtn = ClearCommError( hCommPort, &dwBytes, NULL );
        DEFAULT_ERROR( FALSE == bRtn, goto TRPSCleanup);
        if( dwBytes )
        {
            ShowCommError( dwBytes );
        }

        bRtn = PurgeComm( hCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | 
                          PURGE_RXCLEAR | PURGE_TXCLEAR );

        bRtn = ClearCommError( hCommPort, &dwBytes, NULL );
        DEFAULT_ERROR( FALSE == bRtn, goto TRPSCleanup);
        if( dwBytes )
        {
            LOGLINENUM();
            ShowCommError( dwBytes );
        }

        /* ----------------------------------------------------------------
        	Set the timeouts.
        ---------------------------------------------------------------- */
        LocalCTO.ReadIntervalTimeout        = 600;
        LocalCTO.ReadTotalTimeoutMultiplier = max( (11000 / LocalDCB.BaudRate), 50 );
        LocalCTO.ReadTotalTimeoutConstant   = 6000;
        LocalCTO.WriteTotalTimeoutMultiplier= 0; 
        LocalCTO.WriteTotalTimeoutConstant  = 60000; // Just in case

        bRtn = SetCommTimeouts( hCommPort, &LocalCTO );
        COMM_ERROR( hCommPort, FALSE == bRtn, goto TRPSCleanup);

        /* ----------------------------------------------------------------
        	Time to transmit data.
        ---------------------------------------------------------------- */
        fSending = g_fMaster;

		// CWL -  Lets do a test.  Dont send and receive, just send if master, 
		// receive if slave
        //for( iTestIdx = 1; iTestIdx <= 2; iTestIdx++ )
		for( iTestIdx = 1; iTestIdx <= 1; iTestIdx++ )
        {
			#ifdef UNDER_NT
				Sleep( 1000 );
			#endif    

			// If I am the master, I am sending data on the first pass
            if( fSending )
            {
            	// On next pass I will receive
				fSending = FALSE;
                /* ----------------------------------------------------------------
        	        Setup Buffer.
                ---------------------------------------------------------------- */
				for( iIdx = 0; iIdx < TSTMODEM_DATASIZE; iIdx++ )
                    lpBuffer[iIdx] = 0xAA & (0xFF >> (8 - LocalDCB.ByteSize));

				bRtn = TstModemSendBuffer( hCommPort, (LPCVOID)lpBuffer, TSTMODEM_DATASIZE, &dwBytes );
				if ( bRtn == FALSE )
				{
					g_pKato->Log( LOG_FAIL, 
                               TEXT("FAIL in %s @ line %d: TstModemSendBuffer failed."),
                               __THIS_FILE__, __LINE__ );
					dwResult = TPR_FAIL;
					goto TRPSCleanup;
				}

                if( TSTMODEM_DATASIZE != dwBytes )
                {
                    g_pKato->Log( LOG_FAIL, 
                                  TEXT("FAIL in %s @ line %d: TstModemSendBuffer tried to send %d bytes but only %d bytes sent."),
                                  __THIS_FILE__, __LINE__, TSTMODEM_DATASIZE, dwBytes );
                    dwResult = TPR_FAIL;                                  
                    goto TRPSCleanup;
                } // end if( TSTMODEM_DATASIZE != dwBytes )
            } // end if( Sending )
			// If I am the slave, I am receivng data on the first pass
            else
            {
				// On next pass I will send
                fSending = TRUE;

				bRtn = TstModemReceiveBuffer( hCommPort, (LPVOID)lpBuffer, TSTMODEM_DATASIZE, &dwBytes );
				if ( bRtn == FALSE )
				{
					g_pKato->Log( LOG_FAIL, 
                               TEXT("FAIL in %s @ line %d: TstModemReceiveBuffer failed."),
                               __THIS_FILE__, __LINE__ );
					dwResult = TPR_FAIL;
					goto TRPSCleanup;
				}

                if( TSTMODEM_DATASIZE != dwBytes )
                {
                    g_pKato->Log( LOG_FAIL, 
                                  TEXT("FAIL in %s @ line %d: TstModemReceiveBuffer tried to get %d bytes but only %d bytes received."),
                                  __THIS_FILE__, __LINE__, TSTMODEM_DATASIZE, dwBytes );
                    dwResult = TPR_FAIL;                                  
                    goto TRPSCleanup;

                } // end if( TSTMODEM_DATASIZE != dwBytes )


				// Check received data
                for( iIdx = 0; iIdx < TSTMODEM_DATASIZE; iIdx++ )
                {
                    if( lpBuffer[iIdx] != (0xAA & (0xFF >> (8 - LocalDCB.ByteSize))) )
                    {
                        g_pKato->Log( LOG_FAIL, 
                                      TEXT("FAIL in %s @ line %d: Byte error buffer index %d is 0x%02X expected 0x%02X."),
                                      __THIS_FILE__, __LINE__, iIdx, lpBuffer[iIdx], (0xAA & (0xFF >> LocalDCB.ByteSize)) );
                        // Attempt to cause a quick end to the test.
                        dwResult = TPR_FAIL;
                        Sleep( 1000 );
                        TstModemCancel( hCommPort );
                        goto TRPSCleanup;
                    } // end if( lpBuffer[iIdx] != (0xAA & (0xFF >> LocalDCB.ByteSize)) )
                } // end for( iIdx = 0; iIdx < TSTMODEM_DATASIZE; iIdx++ )
            } // // end if( Sending )
        } // end for( iIdx = 1; iIdx <= 2; iIdx++ );
    } // for( iBaudIdx = 0; iBaudIdx < NUMDATABITS; iDataIdx++ )

TRPSCleanup:
	

    /* --------------------------------------------------------------------
        Clean up at the end of the test.
    -------------------------------------------------------------------- */
    if( FALSE == bRtn ) dwResult = TPR_FAIL;

	g_pKato->Log(LOG_COMMENT, TEXT("Beginning cleanup - hCommPort = %lu\n"), hCommPort);
	PurgeComm( hCommPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXCLEAR );
    ClearCommError( hCommPort, &dwBytes, NULL );
    
    bRtn = SetupDefaultPort( hCommPort );
	DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );
#ifdef UNDER_CE
	Sleep( 1500 );
#endif    

	g_pKato->Log(LOG_COMMENT, TEXT("Beginning EndTestSync - hCommPort = %lu\n"), hCommPort);
    dwResult = EndTestSync( hCommPort, lpFTE->dwUniqueID, dwResult );

	// We always close handles in EndTestSync for bluetooth
	if(!g_fBT)
	{
    	bRtn = CloseHandle( hCommPort );
	}
        

    return dwResult;
	
} // end TestRadeDataParityAndStop( ... ) 


/*++
 
TestModemSignals:
 
    Tests the GetCommModemStatus function.
    
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Uknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI TestModemSignals( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    BOOL    bRtn            = FALSE;
    DWORD   dwResult        = TPR_PASS;
    HANDLE  hCommPort       = INVALID_HANDLE_VALUE;
    DWORD   dwModemState    = 0;

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
        COMM_ERROR( hCommPort, FALSE == bRtn, goto TMSCleanup );
        
    } // end if( !g_fSetCommProp )

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TMSCleanup);
    
    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */
	bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_ABORT; goto TMSCleanup);

    /* --------------------------------------------------------------------
    	Set All of the modem signals high
    -------------------------------------------------------------------- */
    bRtn = EscapeCommFunction( hCommPort, SETRTS );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TMSCleanup);

    bRtn = EscapeCommFunction( hCommPort, SETDTR );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TMSCleanup);

    CMDLINENUM( Sleep( 10000 ) );  // 10 seconds

    /* --------------------------------------------------------------------
    	Get and test the modem states
    -------------------------------------------------------------------- */
    bRtn = GetCommModemStatus( hCommPort, &dwModemState );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TMSCleanup);

	g_pKato->Log( LOG_DETAIL,TEXT("In %s @ line %d: ModemStatus CTS:%s, DSR:%s,RING:%s,RLSD:%s"), __THIS_FILE__, __LINE__,
		(MS_CTS_ON & dwModemState)?TEXT("On"):TEXT("Off"),
		(MS_DSR_ON & dwModemState)?TEXT("On"):TEXT("Off"),
		(MS_RING_ON & dwModemState)?TEXT("On"):TEXT("Off"),
		(MS_RLSD_ON & dwModemState)?TEXT("On"):TEXT("Off"));
    // RTS-CTS
    if( (PCF_RTSCTS & g_CommProp.dwProvCapabilities)  &&
        (0 == (MS_CTS_ON & dwModemState))                   )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: RTS-CTS supported CTS not on"),
                      __THIS_FILE__, __LINE__ );
        dwResult = TPR_FAIL;

    } // end if( RTS-CTS FAILED )    

    else if( MS_CTS_ON & dwModemState )

    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("In %s @ line %d: CTS ON"), __THIS_FILE__, __LINE__);

    }  // end if( RTS-CTS FAILED ) else if( CTS ON )
    
    // DTR-DSR
    if( (PCF_DTRDSR & g_CommProp.dwProvCapabilities)  &&
        (0 == (MS_DSR_ON & dwModemState))                   )
    {
        dwResult = TPR_FAIL;
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: DTR-DSR supported DSR not ON.  ")
                      TEXT("Note, this may be due to the serial cable."),
                      __THIS_FILE__, __LINE__ );
        dwResult = TPR_FAIL;                      

    } // end if( DTR-DSR FAILED )    

    else if( MS_DSR_ON & dwModemState )

    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("In %s @ line %d: DSR ON"), __THIS_FILE__, __LINE__);

    }  // end if( DTR-DSR FAILED ) else if( DSR ON )

    if( MS_RLSD_ON & dwModemState )

    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("In %s @ line %d: RLSD(DCD) ON"), __THIS_FILE__, __LINE__);

    }  // end if( DTR-DSR FAILED ) else if( RLSD ON )

    if( MS_RING_ON & dwModemState )

    {
        g_pKato->Log( LOG_DETAIL,
                      TEXT("In %s @ line %d: RING ON"), __THIS_FILE__, __LINE__);

    }  // end if( DTR-DSR FAILED ) else if( DSR ON )
    
    TMSCleanup:
        

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    CMDLINENUM( Sleep( 10000 ) );

    /* --------------------------------------------------------------------
    	Remove the signals
    -------------------------------------------------------------------- */
    bRtn = EscapeCommFunction( hCommPort, CLRRTS );
    COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_FAIL );

    bRtn = EscapeCommFunction( hCommPort, CLRDTR );
    COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_FAIL );

    /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	dwResult = EndTestSync( hCommPort, lpFTE->dwUniqueID, dwResult );

    LOGLINENUM();

	// In bluetooth we always close the handle in EndTestSync
	if(!g_fBT)
	{
    	CloseHandle( hCommPort );
	}

    LOGLINENUM();
    

	return dwResult;
	
} // end TestModemSignals( ... ) 



/*-----------------------------------------------------------------------------
  Function		:TESTPROCAPI TestPorts( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY )
  Description	:Tests opening and closing serial port.  Measures amount of time,
					does multiple opens/closes to check for leaks, etc.
  Parameters	:
  Returns		:
  Comments		: #2291, 

	Chris,
	I added support in the MDD for THREAD_AT_OPEN.  This flag allows the PDD to 
	not spin its interrupt thread until Open().  In order to test this, 
	I modified COM1: on the CEPC to use this flag.
	
	  There are two primary issues to regress.
	1) On CEPC COM1:, test lots of open/close scenarios and make sure 
	we don't hang or leak resources.

	2) On Odo or other ports, run similar open/close load 
	tests and make sure everything still works as before and that 
	we don't leak or hang.

ONLINE HELP for CreateFile

CreateFile can create a handle to a communications resource, such as the serial port COM1:
For communications resources, the dwCreationDistribution parameter must be OPEN_EXISTING, 
and the hTemplate parameter must be NULL. Read, write, or read/write access can be specified, 
and the handle can be opened for overlapped I/O. 

------------------------------------------------------------------------------*/
TESTPROCAPI TestPorts( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    DWORD   dwResult    = TPR_FAIL;
	UINT	uiStart;
	UINT	uiStop;
	HANDLE  hCommPort   = NULL;
	int		iX;

    /* --------------------------------------------------------------------
    	Sync the begining of the test.
    -------------------------------------------------------------------- */
	bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

	if(g_fBT && g_fMaster)
	{
		// Should take about 1 second for client to do its stuff
		Sleep(1000);
	}
	else
	{
		// Test std open close.  Check for leaks by doing 10 times.
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d:  Testing CreateFile()\n"), __THIS_FILE__, __LINE__);
		for ( iX = 0; iX < 10; iX++ )
		{
			g_pKato->Log( LOG_DETAIL, TEXT("."));
			
			hCommPort = CreateFile( g_lpszCommPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
			if ( hCommPort != INVALID_HANDLE_VALUE )
			{
				CloseHandle( hCommPort );
			}
		}
		g_pKato->Log( LOG_DETAIL, TEXT("\n"));
	}
	
	#ifdef UNDER_NT
			g_pKato->Log( LOG_DETAIL, TEXT("Under NT - Sleep(8000) "));
			Sleep( 8000 );
	#endif 

	// Force a resync - keeps things lined up			
	EndTestSync( NULL, lpFTE->dwUniqueID, TPR_PASS );
	bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
	DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

	if(g_fBT && g_fMaster) 
	{
		// Sleep for about 0.5 seconds
		Sleep(500);
	}
	else
	{
		// Check open/close time.
		uiStart = GetTickCount();
	    hCommPort = CreateFile( g_lpszCommPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
		if ( hCommPort != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hCommPort );
		}
		
		uiStop = GetTickCount();
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d:  Port Open/Close Time = %dms"), __THIS_FILE__, __LINE__, (uiStop-uiStart) );

		// CreateFile variations - Query, no read/write
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Testing CreateFile(QUERY)"), __THIS_FILE__, __LINE__ );
		hCommPort = CreateFile( g_lpszCommPort, 0, 0, NULL, OPEN_EXISTING, 0, NULL );
		if ( hCommPort != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hCommPort );
		}
		else
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: CreateFile(QUERY) failed, ERROR=%d "), __THIS_FILE__, __LINE__, GetLastError() );
		}

		// read
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Testing CreateFile(GENERIC_READ)"), __THIS_FILE__, __LINE__ );
		hCommPort = CreateFile( g_lpszCommPort, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
		if ( hCommPort != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hCommPort );
		}
		else
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: CreateFile(GENERIC_READ) failed, ERROR=%d "), __THIS_FILE__, __LINE__, GetLastError() );
		}

		// write
		g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Testing CreateFile(GENERIC_WRITE)"), __THIS_FILE__, __LINE__ );
		hCommPort = CreateFile( g_lpszCommPort, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
		if ( hCommPort != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hCommPort );
		}
		else
		{
			g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: CreateFile(GENERIC_WRITE) failed, ERROR=%d "), __THIS_FILE__, __LINE__, GetLastError() );
		}
	}
	
   /* --------------------------------------------------------------------
    	Sync the end of a test of the test.
    -------------------------------------------------------------------- */
	return EndTestSync( NULL, lpFTE->dwUniqueID, TPR_PASS );
}
