/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tstmodem.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	TstModem.cpp
 
Abstract:
 
	This file contains a very basic buffer transfer and
	support functions.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#define __THIS_FILE__   TEXT("TstModem.cpp")

#include "TstModem.h"
#include "gserial.h"
#include "pserial.h"

void DumpData(PBYTE pData,DWORD dwLen,BOOL bRead);
/* ------------------------------------------------------------------------
	Structures
------------------------------------------------------------------------ */
// Recieve Packet
typedef struct _TSTMODEM_RPACKET 
{
    BYTE        nPacket;
    BYTE        nCompPacket;
    BYTE        abData[TSTMODEM_DATASIZE];
    BYTE        nChecksum;
} TSTMODEM_RPACKET, *LPTSTMODEM_RPACKET;

// Send Packet
typedef struct _TSTMODEM_SPACKET 
{
    BYTE        bHeader;
    BYTE        nPacket;
    BYTE        nCompPacket;
    BYTE        abData[TSTMODEM_DATASIZE];
    BYTE        nChecksum;
} TSTMODEM_SPACKET, *LPTSTMODEM_SPACKET;

BOOL TstModemReadPacket( HANDLE hFile,
						 LPVOID lpBuffer,
  						 DWORD nNumberOfBytesToRead,
						 LPDWORD lpNumberOfBytesRead )
{
	BOOL 	fRtn;
	DWORD	dwBytesLeft = nNumberOfBytesToRead;
	DWORD	dwBytesRead = 0;
	LPBYTE	lpByteBuffer = (LPBYTE)lpBuffer;
	
	*lpNumberOfBytesRead = 0;

	do 
	{
		fRtn = ReadFile( hFile, 
						 &(lpByteBuffer[*lpNumberOfBytesRead]),
						 dwBytesLeft,
						 &dwBytesRead,
						 NULL );
		g_pKato->Log( LOG_DETAIL, 
					  TEXT("In %s @ line %d: Read %d bytes of %d bytes"),
					  __THIS_FILE__, __LINE__, dwBytesRead, dwBytesLeft );
					  
		*lpNumberOfBytesRead += dwBytesRead;
		if( FALSE == fRtn || 
			nNumberOfBytesToRead == *lpNumberOfBytesRead ) return fRtn;

		dwBytesLeft -= dwBytesRead;
		
	} while( dwBytesRead );

	return fRtn;
	
}  // end BOOL TestModemReadPacket( ... )



/*++
 
TstModemPingThread:
 
	This thread will write an ENQ character to the passed in Comm Port
	every 1000 ms until the passed in Event is signaled.
 
Arguments:
 
	
 
Return Value:
 
	
 

 
	Uknown (unknown)
 
Notes:
 
--*/
static DWORD WINAPI TstModemPingThread( LPCOMMTHREADCTRL lpCtrl )
{
    DWORD           dwRtn;
    BOOL            bRtn;
    BYTE            bByte;
    DWORD           dwBytes;
    DWORD           dwWaitTimeout;

   g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Started TstModemPingThread"),
                  __THIS_FILE__, __LINE__);

    do {  // while( WAIT_TIMEOUT == dwRtn )

        bByte = ENQ;
        bRtn = WriteFile( lpCtrl->hPort, (LPCVOID)&bByte, 1, &dwBytes, NULL );
        COMM_ERROR( lpCtrl->hPort, (FALSE==bRtn), ExitThread(GetLastError()));

        bRtn = TstModemReadPacket( lpCtrl->hPort, (LPVOID)&bByte, 1, &dwBytes );
        COMM_ERROR( lpCtrl->hPort, (FALSE==bRtn), ExitThread(GetLastError()));

        if( 1 == dwBytes && NCK != bByte )
        {
            g_pKato->Log( LOG_DETAIL, 
                          TEXT("In %s @ line %d:  TstModemPingThread Received 0x%02X"),
                          __THIS_FILE__, __LINE__, bByte);

        }

        if( 1 == dwBytes )
            dwWaitTimeout = 1000;
        else
            dwWaitTimeout = 100;

        dwRtn = WaitForSingleObject( lpCtrl->hEndEvent, dwWaitTimeout );
    
    } while( WAIT_TIMEOUT == dwRtn )

    DEFAULT_ERROR((WAIT_FAILED == dwRtn), ExitThread( 0x80000000 ) );

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Exiting TstModemPingThread"),
                  __THIS_FILE__, __LINE__);
    
    ExitThread(0);
    // Never executed, just makes the compiler happy.
    return 0;

} // end DWORD WINAPI TstModemPingThread( LPCOMMTHREADCTRL lpCtrl )


/*++
 
TstModemPing:
 
	This function allows a function to ping an TstModem reciever and so
	it won't time out and terminate.  If the hStopEvnet member is set
	to null the event is created.
 
Arguments:
 
    LPCOMMTHREADCTRL  lpCtrl:  pointer to the struct that contains the
	                           port to ping and the event that signals
	                           the end. 
Return Value:
 
	HANDLE to the TstModemPingThread.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
HANDLE TstModemPing( LPCOMMTHREADCTRL lpCtrl )
{

    HANDLE  hThread;
    DWORD   dwThreadId;

    if( NULL == lpCtrl->hEndEvent )
    {
        lpCtrl->hEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
        FUNCTION_ERROR((NULL == lpCtrl->hEndEvent), return NULL )
    }
    
    hThread = CreateThread( NULL, 0, 
                            (LPTHREAD_START_ROUTINE)TstModemPingThread, 
                            (LPVOID)lpCtrl, 0, &dwThreadId );
    FUNCTION_ERROR((NULL == hThread), return NULL )                            

    return hThread;
    
} // end HANDLE TstModemPing( LPCOMMTHREADCTRL lpCtrl );


/*++
 
TstModemSendBuffer:
 
	This function transmitts a buffers worth of data using 
 
Arguments:
 
	
 
Return Value:
 
	
 

 
	Uknown (unknown)
 
Notes:

    TstModem will send data without reguard to the number
    of data bits. But TstModem doesn't encode the data
    so the call must guarantee that all data bits contain
    modulo 0x0001 << data bits data.
 
--*/
BOOL TstModemSendBuffer( HANDLE hCommPort, 
                         LPCVOID lpBuffer, 
                         const DWORD nSendBytes, 
                         LPDWORD lpnSent )
{
    BOOL            bRtn;
    DWORD           dwBytes;
    TSTMODEM_SPACKET  Packet;
    BYTE            bCtrlPacket;
    DCB             Dcb;
    USHORT          nPacket;
    USHORT          nChecksum;
    USHORT          nModulus;
    INT             iIdx;
   

    /* --------------------------------------------------------------------
        Caluculate modulus
    -------------------------------------------------------------------- */
    Dcb.DCBlength = sizeof( Dcb );
    bRtn = GetCommState( hCommPort, &Dcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

    nModulus    = 0x0001 << Dcb.ByteSize;

    /* --------------------------------------------------------------------
    	Sync begining of transmition.
    -------------------------------------------------------------------- */
    g_pKato->Log( LOG_DETAIL, 
                   TEXT("In %s @ line %d:  TstModemSend Waiting for Sync"),
                  __THIS_FILE__, __LINE__, nSendBytes, Dcb.ByteSize, nModulus );

    for( iIdx = 1; iIdx <= TSTMODEM_RETRY; iIdx++ )
    {
        bCtrlPacket = ENQ;

        g_pKato->Log( LOG_DETAIL, 
                       TEXT("In %s @ line %d:  TstModemSend Waiting for Sync send 0x%02X"),
                      __THIS_FILE__, __LINE__, bCtrlPacket );
    
        bRtn = WriteFile( hCommPort, (LPCVOID)&bCtrlPacket, 1, &dwBytes, NULL );
        COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );
        DEFAULT_ERROR( 1 != dwBytes, return FALSE);

        bCtrlPacket = 0;
        bRtn = TstModemReadPacket( hCommPort, (LPVOID)&bCtrlPacket, 1, &dwBytes );
        COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

        if( dwBytes )
        {
            g_pKato->Log( LOG_DETAIL, 
                           TEXT("In %s @ line %d:  TstModemSend Waiting for Sync read 0x%02X"),
                          __THIS_FILE__, __LINE__, bCtrlPacket );
                          
        } // end if( dwBytes )
        
//        else
//        {
//            g_pKato->Log( LOG_WARNING, 
//                           TEXT("WARNING in %s @ line %d:  TstModemSend Waiting for Sync read timedout"),
//                           __THIS_FILE__, __LINE__ );

//        } // end if( dwBytes ) else

        if( NCK == bCtrlPacket ) break;

    } // end for( iIdx = 1; iIdx <= TSTMODEM_RETRY )

    if( iIdx > TSTMODEM_RETRY )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: TstModemSend begin Send Sync timedout, %d."),
                      __THIS_FILE__, __LINE__, iIdx );
        return FALSE;
        
    } //  if( iIdx > TSTMODEM_RETRY )
    
    /* -------------------------------------------------------------------
    	Time to send data
    ------------------------------------------------------------------- */
     g_pKato->Log( LOG_DETAIL, 
                   TEXT("In %s @ line %d:  Starting TstModem SEND of %d bytes, %d data bits, Modulus %d"),
                  __THIS_FILE__, __LINE__, nSendBytes, Dcb.ByteSize, nModulus );

    *lpnSent = 0;
    nPacket = 0;
    Packet.bHeader = SOH;

    while( nSendBytes > *lpnSent )
    {
        /* ----------------------------------------------------------------
        	Build Packet
        ---------------------------------------------------------------- */
        Packet.nPacket = ++nPacket % nModulus;
        Packet.nCompPacket = ~Packet.nPacket;

        // Fill the data.
        memcpy( Packet.abData, lpBuffer, min(TSTMODEM_DATASIZE, (nSendBytes - *lpnSent)) );

        if( (nSendBytes - *lpnSent) < TSTMODEM_DATASIZE )
            ZeroMemory( &(Packet.abData[nSendBytes - *lpnSent]), TSTMODEM_DATASIZE - (nSendBytes - *lpnSent) );
        
        // Zero Remaining memory
        for( iIdx = min(TSTMODEM_DATASIZE, (nSendBytes - *lpnSent)); 
             iIdx < TSTMODEM_DATASIZE; iIdx++ )
            Packet.abData[iIdx] = 0x00;

        // Get Checksum
        nChecksum = 0;
        for( iIdx = 0; iIdx < TSTMODEM_DATASIZE; iIdx++ ) 
            nChecksum += Packet.abData[iIdx];
        Packet.nChecksum = nChecksum % nModulus;

        /* ----------------------------------------------------------------
        	Packet complete, send data.
        ---------------------------------------------------------------- */
        bCtrlPacket = NCK;
        iIdx = 0;
        while( ACK != bCtrlPacket )
        {
            // Send the packet
            bRtn = WriteFile( hCommPort, (LPCVOID)&Packet, sizeof(Packet), &dwBytes, NULL );
            COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );
       		DumpData((PBYTE)&Packet,dwBytes,FALSE);

            if( sizeof(Packet) > dwBytes )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: TstModem packet underflow."),
                              __THIS_FILE__, __LINE__ );
               return FALSE;
               
            } // end if( sizeof(Packet) > dwBytes );

            // Get the ACK
            bRtn = TstModemReadPacket( hCommPort, (LPVOID)&bCtrlPacket, 1, &dwBytes );
            COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );
         
            if( 0 == dwBytes ) 
                bCtrlPacket = NCK;
            else
                g_pKato->Log( LOG_DETAIL, 
                              TEXT("In %s @ line %d: Send Buffer Read 0x%02X"),
                              __THIS_FILE__, __LINE__, bCtrlPacket );

            if( ++iIdx > TSTMODEM_RETRY )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: TstModem Send retry timeout."),
                              __THIS_FILE__, __LINE__ );
                return FALSE;

            } // end if( ++iIdx >= TSTMODEM_RETRY )

            
            
        } // end while( NCK == bCtrlPacket )

        // Update send count
        *lpnSent += min(TSTMODEM_DATASIZE, (nSendBytes - *lpnSent));

    } // end while( nSendBytes > *lpnSent )


    /* --------------------------------------------------------------------
    	Send End of Transmition
    -------------------------------------------------------------------- */
    bCtrlPacket = EOT;
    bRtn = WriteFile( hCommPort, (LPCVOID)&bCtrlPacket, 1, &dwBytes, NULL );
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );
    DEFAULT_ERROR( 1 != dwBytes, return FALSE);
    
    for( iIdx = 1; iIdx <= TSTMODEM_RETRY; iIdx++ )
    {
        bCtrlPacket = 0x00;
        bRtn = TstModemReadPacket( hCommPort, (LPVOID)&bCtrlPacket, 1, &dwBytes );
        COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

        if( 1 != dwBytes ) continue;
        if( ACK == bCtrlPacket ) break;

    } // end for( iIdx = 1; iIdx <= TSTMODEM_RETRY )

    if( iIdx > TSTMODEM_RETRY )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: TstModemSend EOT Timedout, %d."),
                      __THIS_FILE__, __LINE__, iIdx );
        return FALSE;
        
    } //  if( iIdx > TSTMODEM_RETRY )
    
    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Completed TstModem SEND of %d bytes"),
                  __THIS_FILE__, __LINE__, *lpnSent );

    return TRUE;

} // end BOOL TstModemSendBuffer( ... )


/*++
 
TstModemRecieveBuffer:
 
	This function does an TstModem recieve for nRecBytes and copyes them to
	lpBuffer.  Also this fuction will wait for 
 
Arguments:
 
	
 
Return Value:
 
	
 

 
	Uknown (unknown)
 
Notes:

    TstModem will recieve data without reguard to the number
    of data bits. But TstModem doesn't encode the data
    so the call must guarantee that all data bits contain
    modulo 0x0001 << data bits data.

--*/

BOOL TstModemReceiveBuffer( HANDLE hCommPort, 
                          LPVOID lpBuffer, 
                          const DWORD nRecBytes, 
                          LPDWORD lpnReceived )
{
    BOOL            bRtn;
    DWORD           dwBytes;
    TSTMODEM_RPACKET  Packet;
    BYTE            bCtrlPacket;
    BYTE            bCompMask;
    DCB             Dcb;
    USHORT          nPacket;
    USHORT          nChecksum;
    USHORT          nModulus;
    INT             iIdx;
    INT             nRetry;

    *lpnReceived = 0;
    bCtrlPacket = 0x00;

    Dcb.DCBlength = sizeof( Dcb );
    bRtn = GetCommState( hCommPort, &Dcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, return bRtn );

    nModulus = 0x0001 << Dcb.ByteSize;
    bCompMask = 0xFF >> (8 - Dcb.ByteSize);

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  ")
                  TEXT("Waiting for TstModem RECEIVE of %d bytes, %d data bits, Modulus %d, CompMask 0x%02X"),
                  __THIS_FILE__, __LINE__, nRecBytes, Dcb.ByteSize, nModulus, bCompMask );

    /* --------------------------------------------------------------------
    	Wait for data
    -------------------------------------------------------------------- */
    
    nRetry = 0;
    while( SOH != bCtrlPacket )
    {
        bCtrlPacket = 0x00;
        bRtn = TstModemReadPacket( hCommPort, (LPVOID)&bCtrlPacket, 1, &dwBytes );
        COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );

        if( ENQ == bCtrlPacket )
        {
            bCtrlPacket = NCK;
            bRtn = WriteFile( hCommPort, (LPCVOID)&bCtrlPacket, 1, &dwBytes, NULL );
            COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );
            DEFAULT_ERROR( 1 != dwBytes, return FALSE );
            
        } // end if( ENQ == bCtrlPacket )

        else if( CAN == bCtrlPacket )
        {
            // If canceled return
            g_pKato->Log( LOG_DETAIL, 
                          TEXT("In %s @ line %d:  TstModem recieve Canceled"),
                          __THIS_FILE__, __LINE__);
            return TRUE;
            
        } // end else if( CAN == bCtrlPacket )

        else if( 1 == dwBytes && SOH != bCtrlPacket )
        {
            g_pKato->Log( LOG_WARNING, 
                          TEXT("WARNING in %s @ line %d:  TstModem Recieve wait unexpected read 0x%02X"),
                          __THIS_FILE__, __LINE__, bCtrlPacket );
            nRetry++;
                
        } 
		else if( 1 != dwBytes )
        {
            g_pKato->Log( LOG_WARNING, 
                          TEXT("WARNING in %s @ line %d:  TstModem Read Timed Out dwBytes=%d,bCtrlPacket=0x%02X"),
                          __THIS_FILE__, __LINE__, dwBytes,bCtrlPacket );
            nRetry++;
            
        } // end else
        
        
        if( 60 < nRetry  )
        {
            g_pKato->Log( LOG_FAIL, 
                          TEXT("FAIL in %s @ line %d: TstModem Wait for Receive timed out."),
                          __THIS_FILE__, __LINE__ );
            return FALSE;
        }
        
    } // end while( SOH != bCtrlPacket )

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Starting TstModem RECEIVE of %d bytes"),
                  __THIS_FILE__, __LINE__, nRecBytes );

    /* --------------------------------------------------------------------
    	Recieving Packets.
    -------------------------------------------------------------------- */
    nPacket = 1;
    *lpnReceived = 0;
    nRetry = 1;
    
    do  // while( bCtrlPacket == SOH )
    {
        // Read Packet
//        bRtn = TstModemReadPacket( hCommPort, (LPVOID)&Packet, sizeof(Packet), &dwBytes );
		dwBytes=0;
		bRtn = ReadFile( hCommPort, (LPVOID)&Packet,sizeof(Packet),&dwBytes,NULL);
   		DumpData((PBYTE)&Packet,dwBytes,TRUE);
        COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );

        /* ----------------------------------------------------------------
        	Process read packet
        ---------------------------------------------------------------- */
        // Calculate Checketsum
        nChecksum = 0;
        for( iIdx = 0; iIdx < TSTMODEM_DATASIZE; iIdx++ ) 
            nChecksum += Packet.abData[iIdx];

        nChecksum %= nModulus;

        // Verify Good Packet.
        if( ( sizeof(Packet) == dwBytes )                           &&
            ( Packet.nPacket == (bCompMask & ~Packet.nCompPacket) ) &&
            ( Packet.nPacket == nPacket )                           &&
            ( Packet.nChecksum == nChecksum )                       )
        {
            // Reset nRetry
            nRetry = 1;
            nPacket = ++nPacket % nModulus;
            memcpy( &(((LPBYTE)lpBuffer)[*lpnReceived]), 
                    Packet.abData, 
                    min(TSTMODEM_DATASIZE, nRecBytes-*lpnReceived ) );
            *lpnReceived += min(TSTMODEM_DATASIZE, nRecBytes-*lpnReceived);

            bCtrlPacket = ACK;
            
        } // if( GOOD PACKET )

        else
        {
            if( sizeof(Packet) != dwBytes )
            {
                g_pKato->Log( LOG_WARNING, 
                              TEXT("WARNING in %s @ line %d: TstModem packet read %d B of %d B."),
                              __THIS_FILE__, __LINE__, dwBytes, sizeof(Packet) );

            } // if( sizeof(Packet) != dwBytes )

            else if( Packet.nPacket != (bCompMask & ~Packet.nCompPacket) )
            {
                g_pKato->Log( LOG_WARNING, 
                              TEXT("WARNING in %s @ line %d:  ")
                              TEXT("TstModem packet read corrupt sequnce, 0x%02X != ~0x%02X."),
                              __THIS_FILE__, __LINE__, Packet.nPacket, Packet.nCompPacket );
                              
            } // else if ( CURRUPT PACKET SEQUENCE )

            else if( Packet.nPacket != nPacket )
            {
                g_pKato->Log( LOG_WARNING, 
                              TEXT("WARNING in %s @ line %d:  ")
                              TEXT("TstModem packet read out of sequence expected %d got %d "),
                              __THIS_FILE__, __LINE__, nPacket, Packet.nPacket );
                              
            } // end if( Packet.nPacket != nPacket )

            else if( Packet.nChecksum != nChecksum )
            {
                g_pKato->Log( LOG_WARNING, 
                              TEXT("WARNING in %s @ line %d:  ")
                              TEXT("TstModem packet checksum failure expected %d got %d "),
                              __THIS_FILE__, __LINE__, nChecksum, Packet.nChecksum );
                              
            } // end if( Packet.nChecksum != nChecksum )
            
            if( nRetry++ > TSTMODEM_RETRY ) 
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: TstModem Read packet maximum retries."),
                              __THIS_FILE__, __LINE__ );
                return FALSE;

            } // end if( ++nRetry > TSTMODEM_RETRY )

            bCtrlPacket = NCK;
            
        } // end if( GOOD PACKET ) else;

        /* ----------------------------------------------------------------
        	Write Reply
        ---------------------------------------------------------------- */
        bRtn = WriteFile( hCommPort, (LPCVOID)&bCtrlPacket, 1, &dwBytes, NULL );
        COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );
        // Writes Shouldn't fail
        DEFAULT_ERROR( dwBytes < 1, return FALSE );

        /* ----------------------------------------------------------------
        	Read Next Header
        ---------------------------------------------------------------- */
        bRtn = TstModemReadPacket( hCommPort, (LPVOID)&bCtrlPacket, 1, &dwBytes );
        COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );
        if( dwBytes < 1 )
        {
            g_pKato->Log( LOG_FAIL, 
                          TEXT("FAIL in %s @ line %d: TstModem packet underflow."),
                          __THIS_FILE__, __LINE__ );
            return FALSE;

        } // end if( dwBytes < 1 )
        
    } while( bCtrlPacket == SOH );

    if( EOT == bCtrlPacket )
    {
        CMDLINENUM( Sleep( 500 ) );
        CMDLINENUM( bCtrlPacket = ACK );
        bRtn = WriteFile( hCommPort, (LPCVOID)&bCtrlPacket, 1, &dwBytes, NULL );
        COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );
        DEFAULT_ERROR( 1 != dwBytes, return FALSE);
        CMDLINENUM( Sleep( 500 ) );
  
    } // end if( EOT == bCtrlPacket )

    else
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: TstModem unexpected packet header, 0x%02X."),
                      __THIS_FILE__, __LINE__, bCtrlPacket );
        return FALSE;
        
    } // end if( EOT == bCtrlPacket ) else

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Completed TstModem RECEIVE of %d bytes"),
                  __THIS_FILE__, __LINE__, *lpnReceived );

    return TRUE;

} // end BOOL TstModemRecieveBuffer( ... )

/*++
 
TstModemCancel:
 
	This functions sends a cancel signal to a wait
	TstModemReciveBuffer function.
 
Arguments:
 
	HANDLE  hCommPort: Port to write to.
 
Return Value:
 
	TRUE for succes else FALSE
 

 
	Uknown (unknown)
 
Notes:
 
--*/
BOOL TstModemCancel( HANDLE hCommPort )
{
    BOOL bRtn;

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Canceling TstModem transfer"),
                  __THIS_FILE__, __LINE__ );

    bRtn = TransmitCommChar( hCommPort, CAN );
    COMM_ERROR( hCommPort,  FALSE == bRtn, return FALSE );

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Canceled TstModem transfer"),
                  __THIS_FILE__, __LINE__ );

    return TRUE;

} // end BOOL TstModemCancel( HANDLE hCommPort )
