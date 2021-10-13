/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     pserial.cpp  

Abstract:
Functions:
Notes:
--*/
#define __THIS_FILE__   TEXT("PSerial.cpp")
#include "GSerial.h"
#include "PSerial.h"
void DumpData(PBYTE pData,DWORD dwLen,BOOL bRead)
{
	if (g_fDump) {
		TCHAR sBuffer[0x100];
		g_pKato->Log( LOG_DETAIL,
			TEXT("Dump %lx bytes for %s"),dwLen,bRead?TEXT("READ"):TEXT("WRITE"));
		DWORD curLen=min(dwLen,25);

		while (curLen) {
			LPTSTR pBuffer=sBuffer;
			for (DWORD dwIndex=0;dwIndex<curLen;dwIndex++) {
				TCHAR tch=_T('0')+(((*pData)>>4) & 0xf);
				if (tch>_T('9'))
					tch=tch - _T('0') -10 + _T('A');
				*pBuffer=tch;
				pBuffer++;
				tch=_T('0')+((*pData) & 0xf);
				if (tch>_T('9'))
					tch=tch - _T('0') -10 + _T('A');
				*pBuffer=tch;
				pBuffer++;
				*pBuffer=_T(' ');
				pBuffer++;
				pData++;
			}
			*pBuffer=0;
			g_pKato->Log( LOG_DETAIL,TEXT("%s"),sBuffer);
			dwLen -=dwLen;
			curLen=min(dwLen,25);
		}
	}
}
BOOL TestSyncReadPacket( HANDLE hFile,
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
		if (fRtn && dwBytesRead ) 
			DumpData(&(lpByteBuffer[*lpNumberOfBytesRead]),dwBytesRead,TRUE);
					  
		*lpNumberOfBytesRead += dwBytesRead;
		if( FALSE == fRtn || 
			nNumberOfBytesToRead == *lpNumberOfBytesRead ) return fRtn;

		dwBytesLeft -= dwBytesRead;
		
	} while( dwBytesRead );

	return fRtn;
	
}  // end BOOL TestSyncReadPacket( ... )

/*++
 
BeginTestSync:
 
	This function synchronizes the begining of test UniqueID 
	on the passed com port.
 
Arguments:
 
	hCommPort:  This is the comm port to sync the test on.  If this 
	            value is NULL this function will open the systems
	            test environments default comm port and perform the 
	            test synchronization and then close the port.

    dwTestID:  This is the Tux Test UniqueID this is used as a sanity
               check to attempt to insure the same test is running.
               If the test developer doesn't give a test a uniqe ID
               then this check will fail.
 
Return Value:
 
	TRUE for success else FALSE
 

 
	Uknown (unknown)
 
Notes:

    All synchronization happens at the default comm state settings of
    9600-8n1.  But this test will preserve the comm state of the passed
    in port.  Also the timeouts are changed and preserved durring 
    synchronization.
 
--*/
BOOL BeginTestSync( HANDLE hCommPort, DWORD dwUniqueID )
{
    BOOL            fOpenedPort = FALSE;
    BOOL            bRtn = FALSE;
    BOOL            bResult = FALSE;
    BOOL            fOldTimeouts = FALSE;
    DCB             oldDcb;
    COMMTIMEOUTS    oldCto, newCto;
    DWORD           dwBytes;
    INT             iIdx;
    BYTE            bHeader = 0x00;
    struct _SYNCPACKET
    {
        BYTE    bHeader;
        DWORD   dwUniqueID;
        DWORD   dwCompUniqueID;
    }   Packet;

    /* --------------------------------------------------------------------
    	Initalize some values.
    -------------------------------------------------------------------- */
    oldDcb.DCBlength = 0;

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Begin Test Synchronization for 0x%08X"),
                  __THIS_FILE__, __LINE__, dwUniqueID );

	// Bluetooth servers use a global port handle that is deleted in the EndTestSync 
	// call.  All others use the local handle.

	if(hCommPort)
	{
        // If the test sent the comm port make sure it is clear.
        bRtn = PurgeComm( hCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR );
	}
    else if(g_fBT && g_fMaster)
    {
		g_hBTPort = CreateFile( g_lpszCommPort, 
                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, 0, NULL );
		hCommPort = g_hBTPort;
		FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , return FALSE );
		 FUNCTION_ERROR(!SetCommMask(hCommPort, EV_TXEMPTY),NULL);
   
    }
    else
    {
    	INT i = 0;
    	
        fOpenedPort = TRUE;
        hCommPort = CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );

        // If client could not connect it could be because master is behind, let's
	    // time out and try a bunch of times
		while(hCommPort == INVALID_HANDLE_VALUE && i < MAX_BT_CONN_ATTEMPTS)
		{
			Sleep(BT_CONN_ATTEMPT_TIMEOUT);

			hCommPort = CreateFile( g_lpszCommPort, 
	                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                                OPEN_EXISTING, 0, NULL );
			i++;
		}
		
        FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , return FALSE );
        FUNCTION_ERROR(!SetCommMask(hCommPort, EV_TXEMPTY),NULL);
       } // end if( NULL == hCommPort )

    /* --------------------------------------------------------------------
    	Begin Protected calles
    -------------------------------------------------------------------- */

    /* --------------------------------------------------------------------
    	Initalize COMM Port state
    -------------------------------------------------------------------- */
    oldDcb.DCBlength = sizeof( oldDcb );
    bRtn = GetCommState( hCommPort, &oldDcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, oldDcb.DCBlength = 0; goto BTSCleanup);//change __Leave to break and set error=1

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR(FALSE == bRtn, goto BTSCleanup);

    bRtn = GetCommTimeouts( hCommPort, &oldCto );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto BTSCleanup);
    fOldTimeouts = TRUE;

    // Set CTO
    newCto.ReadIntervalTimeout          =  	300; 
    newCto.ReadTotalTimeoutMultiplier   =  100; 
    newCto.ReadTotalTimeoutConstant     =  5000;
    newCto.WriteTotalTimeoutMultiplier  =    0;
    newCto.WriteTotalTimeoutConstant    =   500; 
    bRtn = SetCommTimeouts( hCommPort, &newCto );
    COMM_ERROR( hCommPort,FALSE == bRtn, goto BTSCleanup);
//#ifdef UNDER_NT
//    Sleep( 1000 );
//#endif    
    ClearTestCommErrors( hCommPort );

    /* --------------------------------------------------------------------
    	Port is all setup lets synchronize
    -------------------------------------------------------------------- */
    if( g_fMaster )
    {
        iIdx = 0;
        bHeader = SOH;
    }
    else
    {
        iIdx = 1;
        bHeader = SOH;
    }

    // Total exchanges is BEGINSYNCS.
    while( iIdx < BEGINSYNCS )
    {
        // Master Writes first.
        if( 0 == iIdx % 2 )
        {
            Packet.bHeader          = bHeader;
            Packet.dwUniqueID       = dwUniqueID;
            Packet.dwCompUniqueID   = ~dwUniqueID;
        
            bRtn = WriteFile( hCommPort, (LPCVOID)&Packet, 
                              sizeof(Packet), &dwBytes, NULL );
			DumpData((PBYTE)&Packet,dwBytes,FALSE);
            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Wrote %d B of %d B"),
                          __THIS_FILE__, __LINE__, dwBytes, sizeof(Packet) );
            COMM_ERROR( hCommPort,FALSE == bRtn, goto BTSCleanup);
            // Writes should allways succeed (except with bluetooth since we are not 
            // always connected and writes can pending in the queue)
            if(!g_fBT)
            {
	            DEFAULT_ERROR(sizeof(Packet) != dwBytes, goto BTSCleanup);
            }

            
            switch( bHeader )
            {
            case SOH:
                g_pKato->Log( LOG_DETAIL, 
                              TEXT("In %s @ line %d: %s sent SOH"),
                              __THIS_FILE__, __LINE__, (g_fMaster ? TEXT("MASTER") : TEXT("SLAVE")) );
                bHeader = ACK;
                break;
                
            case ACK:
                g_pKato->Log( LOG_DETAIL, 
                              TEXT("In %s @ line %d: %s sent ACK"),
                              __THIS_FILE__, __LINE__, (g_fMaster ? TEXT("MASTER") : TEXT("SLAVE")) );
                if( g_fMaster )
                {
                    bResult = TRUE;
                    goto BTSCleanup;
                }
                else
                {
                    break;
                }

            case NCK:
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sent NCK"),
                              __THIS_FILE__, __LINE__, bHeader );
                bResult = FALSE;
                goto BTSCleanup;
                
            default:
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sent Unknow header, 0x%02X."),
                              __THIS_FILE__, __LINE__, bHeader );
                bResult = FALSE;
                goto BTSCleanup;

            } // end switch( bHeader );                
            
        } // if( TURN TO WRITE )

        else
        {
//            bRtn = TestSyncReadPacket( hCommPort, (LPVOID)&Packet, 
//                                       sizeof(Packet), &dwBytes );
            bRtn = ReadFile( hCommPort, (LPVOID)&Packet, 
                              sizeof(Packet), &dwBytes, NULL );
			if (bRtn)
				DumpData((PBYTE)&Packet,dwBytes,TRUE);
			else
	      		g_pKato->Log( LOG_DETAIL,
            					  TEXT("In %s @ line %d: ReadFile Fails at GetLastError=%d"),
            					  __THIS_FILE__, __LINE__, GetLastError() );


            // Test for retry failures
            if( bRtn==FALSE ||
				sizeof(Packet) != dwBytes                       ||
               (Packet.bHeader != bHeader && Packet.bHeader != NCK) ||
                Packet.dwUniqueID != (~Packet.dwCompUniqueID)        ||
				Packet.dwUniqueID != dwUniqueID )
            {
                if( sizeof(Packet) == dwBytes )
                {
                    g_pKato->Log( LOG_WARNING,
                                  TEXT("WARNING in %s @ line %d:  ")
                                  TEXT("Corrupt packet 0x%02X expect 0x%02X, ID error 0x%08X"),
                                  __THIS_FILE__, __LINE__, Packet.bHeader, bHeader,
                                  (Packet.dwUniqueID & Packet.dwCompUniqueID ) );
                                  
                } // end if( sizeof(Packet) == dwBytes )

                else
                {
                    g_pKato->Log( LOG_DETAIL,
                                  TEXT("In %s @ line %d:  Begin Sync packet read timeout, read %d bytes"),
                                  __THIS_FILE__, __LINE__, dwBytes );

                } // end if( sizeof(Packet) == dwBytes )
               
                bHeader = SOH; // if error ; resync
                if( g_fMaster )
                {
                    iIdx++;
                }
                else
                {
                    iIdx += 2;
                }
                    
                continue;
                
            } // end if ( RETRY FAILURE )

            // Check for NCK, wrong test error.
            if( Packet.bHeader == NCK )
            {
                // Other End Dosen't aggree with UnqueID
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sync for test 0x%08X Peer expected 0x%08X."),
                              __THIS_FILE__, __LINE__, dwUniqueID, Packet.dwUniqueID );
                bResult = FALSE;
                goto BTSCleanup;

            } // if( Packet.bHeader == NCK )

            // Check for SOH wrong test
            if( Packet.bHeader == SOH  && Packet.dwUniqueID != dwUniqueID )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sync for test 0x%08X Peer sent 0x%08X ."),
                              __THIS_FILE__, __LINE__, dwUniqueID, Packet.dwUniqueID );
                bHeader = NCK;
                                             
                bResult = FALSE;
                iIdx++;
                continue;
                
            } // end if( WRONG TEST )

            // Recieved ACK
            if( FALSE == g_fMaster && ACK == bHeader ) 
            {
            	g_pKato->Log( LOG_FAIL, 
                              TEXT("In %s @ line %d: Slave received ACK."),
                              __THIS_FILE__, __LINE__);
                bResult = TRUE;
                goto BTSCleanup;
                
            } // end if( ACK == bHeader )
                
            bHeader = ACK;
                
        } // if( TURN TO WRITE ) else

        iIdx++;

    } // end while( NOT TIMED OUT )

    // Check for timeout
    if( BEGINSYNCS <= iIdx )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: Test Synchronization timed out."),
                      __THIS_FILE__, __LINE__ );
        bResult = FALSE;
        goto BTSCleanup;

    } // end if( TIMED OUT )
    
    /* --------------------------------------------------------------------
        Clean Up    	
    -------------------------------------------------------------------- */
 //   } 



    BTSCleanup:

    DWORD OldCmEvtMask=0,CmEvtMask=0;
    g_pKato->Log( LOG_DETAIL, 
                      TEXT("%s @ line %d,Wait 1 second before close."),
                      __THIS_FILE__, __LINE__ );
//	Sleep(1000); 
// Wait for hardware send data out, Becuase it may followed by set property.
    
    FUNCTION_ERROR(!WaitCommEvent(hCommPort, &CmEvtMask,NULL), NULL);
   
    if( oldDcb.DCBlength )
    {
        bRtn = SetCommState( hCommPort, &oldDcb );
        COMM_ERROR( hCommPort,FALSE == bRtn , bResult = FALSE);

    } // end if( oldDcb.DCBlength )

    if( bRtn && fOldTimeouts )
    {
        bRtn = SetCommTimeouts( hCommPort, &oldCto );
        COMM_ERROR( hCommPort,FALSE == bRtn , bResult = FALSE);        
        
    } // if( bRtn && fOldTimeouts )
    
    if( fOpenedPort && hCommPort && !(g_fBT && g_fMaster))
    {
        bRtn = CloseHandle( hCommPort );
        DEFAULT_ERROR( FALSE == bRtn, bRtn = FALSE );
        hCommPort = NULL;
    } // if( fOpendPort && hCommPort )
    
    else if(!(g_fBT && g_fMaster))
    {
        // If the test sent the comm port make sure it is clear.
        bRtn = PurgeComm( hCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | 
                          PURGE_TXCLEAR | PURGE_RXCLEAR );
        COMM_ERROR( hCommPort, FALSE == bRtn, bResult = FALSE);
    } // // end if( NULL == hCommPort )


    if( bRtn && bResult )
    {
        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s @ line %d:  Completed, Begin Test Synchronization for 0x%08X"),
                      __THIS_FILE__, __LINE__, dwUniqueID );

    } // end if( bRtn && bResult )

    else 
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: Begin Test Synchronization for 0x%08X,") 
                      TEXT("bRtn = %s, bResult = %s"),
                      __THIS_FILE__, __LINE__, dwUniqueID,
                      bRtn ? TEXT("TRUE") : TEXT("FALSE"), 
                      bResult ? TEXT("TRUE") : TEXT("FALSE"));

    } // // end if( bRtn && bResult ) else


    return (bRtn && bResult);

} // end BeginTestSyncComm( const HANDLE hCommPort, const DWORD dwUniqueID )


/*++
 
EndTestSync:
 
	This function synchronizes the end of a tests UniqueID 
	on the passed com port.
 
Arguments:
 
	hCommPort:  This is the comm port to sync the test on.  If this 
	            value is NULL this function will open the systems
	            test environments default comm port and perform the 
	            test synchronization and then close the port.

    dwTestID:  This is the Tux Test UniqueID this is used as a sanity
               check to attempt to insure the same test is running.
               If the test developer doesn't give a test a uniqe ID
               then this check will fail.

    dwMyResult:  This result of the test the called the synchronization
                 function.
 
Return Value:
 
	TRUE for success else FALSE
 

 
	Uknown (unknown)
 
Notes:

    All synchronization happens at the default comm state settings of
    9600-8n1.  But this test will preserve the comm state of the passed
    in port.  Also the timeouts are changed and preserved durring 
    synchronization.
 
--*/
DWORD EndTestSync( HANDLE hCommPort, DWORD dwUniqueID, DWORD dwMyResult )
{
    BOOL            fOpenedPort = FALSE;
    BOOL            bRtn = FALSE;
    DWORD           dwResult = TPR_ABORT;
    BOOL            fOldTimeouts = FALSE;
    DCB             oldDcb;
    COMMTIMEOUTS    oldCto, newCto;
    DWORD           dwBytes;
    INT             iIdx;
    BYTE            bHeader = 0x00;
   DWORD CmEvtMask=0;
   
    struct _SYNCPACKET
    {
        BYTE    bHeader;
        DWORD   dwUniqueID;
        DWORD   dwCompUniqueID;
        DWORD   dwResult;
        DWORD   dwCompResult;
    }   Packet;
	
    /* --------------------------------------------------------------------
    	Initalize some values.
    -------------------------------------------------------------------- */
    oldDcb.DCBlength = 0;

    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  End Test Synchronization for 0x%08X, result == %d"),
                  __THIS_FILE__, __LINE__, dwUniqueID, dwMyResult );

	Sleep(4000);  // Give a second (or 4) for buffered data out.

	// With bluetooth master we don't have to create file because the 
	// global port handle exists
	if(g_fBT && g_fMaster)
	{
		hCommPort = g_hBTPort;
	}
	
	//Sleep(1000);  // Give a second for buffered data out.
//	Sleep(1000); 
// Wait for hardware send data out, Becuase it may followed by set property.
/*    FUNCTION_ERROR(!GetCommMask(hCommPort, &OldCmEvtMask),NULL);
    FUNCTION_ERROR(!SetCommMask(hCommPort, EV_TXEMPTY),NULL);
    FUNCTION_ERROR(!WaitCommEvent(hCommPort, &CmEvtMask,NULL), NULL);
   FUNCTION_ERROR(!SetCommMask(hCommPort, OldCmEvtMask),NULL)
    */


    
    else if( NULL == hCommPort)
    {
        fOpenedPort = TRUE;
        bRtn = FALSE;
        hCommPort = CreateFile( g_lpszCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL );
        COMM_ERROR( hCommPort, INVALID_HANDLE_VALUE == hCommPort , return FALSE );
         FUNCTION_ERROR(!SetCommMask(hCommPort, EV_TXEMPTY),NULL);
   
                               
    } // end if( NULL == hCommPort )

    else
    {
        // If the test sent the comm port make sure it is clear.
        bRtn = PurgeComm( hCommPort, 
                          PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR );
        COMM_ERROR( hCommPort, INVALID_HANDLE_VALUE == hCommPort , return FALSE );                          
    } // // end if( NULL == hCommPort ) else

    /* --------------------------------------------------------------------
    	Begin Protected calles
    -------------------------------------------------------------------- */

    /* --------------------------------------------------------------------
    	Initalize COMM Port state
    -------------------------------------------------------------------- */
    oldDcb.DCBlength = sizeof( oldDcb );
    bRtn = GetCommState( hCommPort, &oldDcb );
    COMM_ERROR( hCommPort, FALSE == bRtn , oldDcb.DCBlength = 0;goto ETSCleanup);

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR(FALSE == bRtn, goto ETSCleanup);

    bRtn = GetCommTimeouts( hCommPort, &oldCto );
    COMM_ERROR( hCommPort,FALSE == bRtn, goto ETSCleanup);
    fOldTimeouts = TRUE;

    // Set CTO
    newCto.ReadIntervalTimeout          =    300; 
    newCto.ReadTotalTimeoutMultiplier   =  100; 
    newCto.ReadTotalTimeoutConstant     =  5000;     
    newCto.WriteTotalTimeoutMultiplier  =  0; 
    newCto.WriteTotalTimeoutConstant    =  5000; 
    bRtn = SetCommTimeouts( hCommPort, &newCto );
    COMM_ERROR( hCommPort,FALSE == bRtn, goto ETSCleanup);

    /* --------------------------------------------------------------------
    	Port is all setup lets synchronize
    -------------------------------------------------------------------- */
    if( g_fMaster )
    {
        iIdx = 0;
        bHeader = SOH;
    }
    else
    {
        iIdx = 1;
        bHeader = SOH;
    }
    // Total exchanges is ENDSYNCS.
    while( iIdx < ENDSYNCS )
    {
        // Master Writes first.
        if( 0 == iIdx % 2 )
        {
            Packet.bHeader          = bHeader;
            Packet.dwUniqueID       = dwUniqueID;
            Packet.dwCompUniqueID   = ~dwUniqueID;
            Packet.dwResult         = dwMyResult;
            Packet.dwCompResult     = ~dwMyResult;
            bRtn = WriteFile( hCommPort, (LPCVOID)&Packet, 
                              sizeof(Packet), &dwBytes, NULL );
            COMM_ERROR( hCommPort,FALSE == bRtn, goto ETSCleanup);
			DumpData((PBYTE)&Packet,dwBytes,FALSE);
            // Writes should allways succeed expect with bluetooth since we are not always
            // connected and write may be pending
			if(!g_fBT)
			{
            	DEFAULT_ERROR(sizeof(Packet) != dwBytes, goto ETSCleanup);
			}
            
            switch( bHeader )
            {
            case SOH:
                g_pKato->Log( LOG_DETAIL, 
                              TEXT("In %s @ line %d: %s sent SOH"),
                              __THIS_FILE__, __LINE__, (g_fMaster ? TEXT("MASTER") : TEXT("SLAVE")) );
                bHeader = ACK;
                break;
                
            case ACK:
                g_pKato->Log( LOG_DETAIL, 
                              TEXT("In %s @ line %d: %s sent SOH"),
                              __THIS_FILE__, __LINE__, (g_fMaster ? TEXT("MASTER") : TEXT("SLAVE")) );

                dwResult = WorseResult( Packet.dwResult, dwMyResult );                

                if( g_fMaster )
                {
                    goto ETSCleanup;
                    
                } // end if( g_fMaster )
                
                break;

            case NCK:
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: %s sent NCK."),
                              __THIS_FILE__, __LINE__, 
                              (g_fMaster ? TEXT("MASTER") : TEXT("SLAVE")) );
                dwResult = TPR_ABORT;
                goto ETSCleanup;
                
            default:
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sent Unknow header, 0x%02X."),
                              __THIS_FILE__, __LINE__, bHeader );
                goto ETSCleanup;

            } // end switch( bHeader );                
            
        } // if( TURN TO WRITE )

        else
        {
//            bRtn = TestSyncReadPacket( hCommPort, (LPVOID)&Packet, 
//                             		   sizeof(Packet), &dwBytes );
            bRtn = ReadFile( hCommPort, (LPVOID)&Packet, 
                              sizeof(Packet), &dwBytes, NULL );
			if (bRtn)
				DumpData((PBYTE)&Packet,dwBytes,TRUE);
			else
	      		g_pKato->Log( LOG_DETAIL,
            					  TEXT("In %s @ line %d: ReadFile Fails at GetLastError=%d"),
            					  __THIS_FILE__, __LINE__, GetLastError() );


            // Test for retry failures
            
            if( sizeof(Packet) != dwBytes                           ||
               (Packet.bHeader != bHeader && Packet.bHeader != NCK) ||
                Packet.dwUniqueID != (~Packet.dwCompUniqueID)       ||
                Packet.dwResult != (~Packet.dwCompResult)              )
            {

            	if( dwBytes )
            	{
            		g_pKato->Log( LOG_DETAIL,
            					  TEXT("In %s @ line %d: Read %d bytes of %d bytes"),
            					  __THIS_FILE__, __LINE__, dwBytes, sizeof(Packet) );
				}            					  

                /* --------------------------------------------------------
                	On time out wait: 2 Sec
                -------------------------------------------------------- */
                Sleep( 2000 );
                
                bHeader = SOH;
                if( g_fMaster )
                {
                    iIdx++;
                }
                else
                {
                    iIdx += 2;
                }
                
                continue;
                
            } // end if ( RETRY FAILURE )

            // Check for NCK, wrong test error.
            if( Packet.bHeader == NCK )
            {
                // Other End Dosen't aggree with UnqueID
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sync for test 0x%08X Peer expected 0x%08X."),
                              __THIS_FILE__, __LINE__, dwUniqueID, Packet.dwUniqueID );
                dwResult = TPR_ABORT;
                goto ETSCleanup;

            } // if( Packet.bHeader == NCK )

            // Check for SOH wrong test
            if( Packet.bHeader == SOH  && Packet.dwUniqueID != dwUniqueID )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Sync for test 0x%08X Peer sent 0x%08X ."),
                              __THIS_FILE__, __LINE__, dwUniqueID, Packet.dwUniqueID );
                bHeader = NCK;
                                             
                dwResult = TPR_ABORT;
                iIdx++;
                continue;
                
            } // end if( WRONG TEST )

            dwResult = WorseResult( Packet.dwResult, dwMyResult );
            
            // Send ACK
            if( FALSE == g_fMaster && ACK == bHeader ) 
            {
                g_pKato->Log( LOG_DETAIL, 
                              TEXT("In %s @ line %d: %s recieved ACK"),
                              __THIS_FILE__, __LINE__, (g_fMaster ? TEXT("MASTER") : TEXT("SLAVE")) );
                goto ETSCleanup;
                
            } // end if( ACK == bHeader )
                
            bHeader = ACK;
                
        } // if( TURN TO WRITE ) else

        iIdx++;

    } // end while( NOT TIMED OUT )

    // Check for timeout
    if( ENDSYNCS <= iIdx )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: Test Synchronization timedout."),
                      __THIS_FILE__, __LINE__ );
        dwResult = TPR_ABORT;
        goto ETSCleanup;

    } // end if( TIMED OUT )

    /* --------------------------------------------------------------------
        Clean Up    	
    -------------------------------------------------------------------- */
    //}
    ETSCleanup:

	//Sleep(500); // Wait for hardware finishing transfer.
    FUNCTION_ERROR(!WaitCommEvent(hCommPort, &CmEvtMask,NULL), NULL);
    

	if(g_fBT)
	{
		// Always use the result passed for bluetooth.
		dwResult = WorseResult(TPR_PASS, dwMyResult);

		// With bluetooth wait a little bit longer to make sure sync finishes
		// together.
		Sleep(3000);
		
		if(g_fMaster)
		{
			CloseHandle(g_hBTPort);
			g_hBTPort = NULL;
		}
		else
		{
			CloseHandle(hCommPort);
	        hCommPort = NULL;
		}
	}
	else
	{
	    if( oldDcb.DCBlength )
	    {
	        bRtn = SetCommState( hCommPort, &oldDcb );
	        COMM_ERROR( hCommPort,FALSE == bRtn , dwResult = TPR_ABORT);

	    } // end if( oldDcb.DCBlength )

	    if( bRtn && fOldTimeouts )
	    {
	        bRtn = SetCommTimeouts( hCommPort, &oldCto );
	        COMM_ERROR( hCommPort,FALSE == bRtn , dwResult = TPR_ABORT);        
	        
	    } // if( bRtn && fOldTimeouts )
	    
	    if( fOpenedPort && hCommPort )
	    {
	        bRtn = CloseHandle( hCommPort );
	        hCommPort = NULL;
	    } // if( fOpendPort && hCommPort )

	    else
	    {
	        // If the test sent the comm port make sure it is clear.
	        bRtn = PurgeComm( hCommPort, 
	                          PURGE_TXABORT | PURGE_RXABORT | 
	                          PURGE_TXCLEAR | PURGE_RXCLEAR );
	        COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT);
	    } // // end if( NULL == hCommPort )



	    if( FALSE == bRtn ) dwResult = TPR_ABORT;
    }
    
    g_pKato->Log( LOG_DETAIL, 
                  TEXT("In %s @ line %d:  Completed End Test Synchronization for 0x%08X result = %d"),
                  __THIS_FILE__, __LINE__, dwUniqueID, dwResult );
    
    return dwResult;

} // end End TestSyncComm( const HANDLE hCommPort, const DWORD dwUniqueID )



/*++
 
WorseResult:
 
	This runction returns the worst of the two TUX based results.
 
Arguments:
 
    dwResult1 and  dwResult2:  The results to compair.
 
Return Value:
 
	The worst of the two passed in results.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
DWORD WorseResult( DWORD dwResult1, DWORD dwResult2 )
{

    if( dwResult1 == dwResult2 ) return dwResult1;

    if( dwResult1 == TPR_ABORT || dwResult2 == TPR_ABORT )
        return TPR_ABORT;

    if( dwResult1 == TPR_FAIL || dwResult2 == TPR_FAIL )
        return TPR_FAIL;
    
    if( dwResult1 == TPR_SKIP || dwResult2 == TPR_SKIP )
        return TPR_SKIP;

    return TPR_FAIL;
    
} // end DWORD WorseResult( Packet.dwResult, dwMyResult )

/*++
 
SetupDefaultPort:
 
	This function sets the passed com port to 9600-8n1	without flow control.
 
Arguments:
 
    HANDLE hCommPort:  The Comm Port to set up.	
 
Return Value:
 
	
 

 
	Uknown (unknown)
 
Notes:
 
--*/
BOOL SetupDefaultPort( HANDLE hCommPort )
{
    DCB             dcb;
    COMMTIMEOUTS    cto;
    BOOL            bRtn;

//    bRtn = PurgeComm( hCommPort, 
//                      PURGE_TXABORT | PURGE_RXABORT | 
//                      PURGE_RXCLEAR | PURGE_TXCLEAR );
//    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );
                      
    bRtn = SetupComm( hCommPort, 2048, 2048);
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );
    
    ZeroMemory( &dcb, sizeof( dcb ) );

    dcb.DCBlength = sizeof( DCB );
    dcb.BaudRate = 9600;
    dcb.fBinary = TRUE;
    dcb.fParity = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE; //typically this should be DTR_CONTROL_ENABLE
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;                                             // XON/XOFF out flow control 
    dcb.fInX = FALSE;                                               // XON/XOFF in flow control 
    dcb.fErrorChar = FALSE;                                      // enable error replacement 
    dcb.fNull= FALSE;                                                // enable null stripping 
    dcb.fRtsControl = RTS_CONTROL_DISABLE;          // RTS flow control 
    dcb.fAbortOnError = TRUE;                                  // abort reads/writes on error 
    dcb.fDummy2 = 0;                                              // reserved 
    dcb.wReserved = 0;                                            // not currently used 
    dcb.XonLim = 100;                                              // transmit XON threshold 
    dcb.XoffLim = 100;                                              // transmit XOFF threshold 
    dcb.ByteSize = 8;                                                // number of bits/byte, 4-8 
    dcb.Parity = NOPARITY;                                       // 0-4=no,odd,even,mark,space 
    dcb.StopBits = ONESTOPBIT;                               // 0,1,2 = 1, 1.5, 2 
    dcb.XonChar = 'A';                                              // Tx and Rx XON character 
    dcb.XoffChar = 'B';                                              // Tx and Rx XOFF character 
    dcb.ErrorChar = '*';                                             // error replacement character 
    dcb.EofChar = 127;                                              // end of input character 
    dcb.EvtChar = 126;                                              // received event character 
    dcb.wReserved1 = 0;                                           // reserved; do not use 

    bRtn = SetCommState( hCommPort, &dcb );
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

    cto.ReadIntervalTimeout         =    100;
    cto.ReadTotalTimeoutMultiplier  =    100;
    cto.ReadTotalTimeoutConstant    =   1000;      
    cto.WriteTotalTimeoutMultiplier =      0; 
    cto.WriteTotalTimeoutConstant   =  60000; 

    bRtn = SetCommTimeouts( hCommPort, &cto );
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

    bRtn = EscapeCommFunction( hCommPort, CLRRTS );
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

    bRtn = EscapeCommFunction( hCommPort, CLRDTR );
    COMM_ERROR( hCommPort, FALSE == bRtn, return FALSE );

    return TRUE;

} // end BOOL SetupDefaultPort( HANDLE hCommPort )


BOOL DumpCommProp( const COMMPROP *pcp )
{
    INT     iIdx;
    TCHAR   szBuffer[1024];
    LPTSTR  lpszBuf = szBuffer;

    DEFAULT_ERROR(  NULL == pcp, return FALSE );

    // BAUDS
    lpszBuf += wsprintf( lpszBuf, TEXT("  ** Bauds: ") );
    for( iIdx = 0; iIdx < NUMBAUDS; iIdx++ ) 
    {
        if( pcp->dwSettableBaud & g_BaudTable[iIdx].dwFlag ) 
        {
            lpszBuf += wsprintf( lpszBuf, TEXT("%s "), g_BaudTable[iIdx].ptszString );
        }
    }
    g_pKato->Log( LOG_DETAIL, szBuffer );

    // Parities
    lpszBuf = szBuffer;
    lpszBuf += wsprintf( lpszBuf, TEXT("  ** Parities & Stop bits: ") );
    for( iIdx = 0; iIdx < NUMPARITIESANDSTOPS; iIdx++ ) 
    {
        if( pcp->wSettableStopParity & g_ParityStopBitsTable[iIdx].dwFlag ) 
        {
            lpszBuf += wsprintf( lpszBuf, TEXT("%s "), g_ParityStopBitsTable[iIdx].ptszString );
        }
    }
    g_pKato->Log( LOG_DETAIL, szBuffer );

    // Data Bits
    lpszBuf = szBuffer;
    lpszBuf += wsprintf( lpszBuf, TEXT("  ** Data Bits: ") );
    for( iIdx = 0; iIdx < NUMDATABITS; iIdx++ ) 
    {
        if( pcp->wSettableData & g_DataBitsTable[iIdx].dwFlag ) 
        {
            lpszBuf += wsprintf( lpszBuf, TEXT("%1hd, "), g_DataBitsTable[iIdx].siBits );
        }
    }
    g_pKato->Log( LOG_DETAIL, szBuffer );
    
    return TRUE;
    
} // end DumpCommProp

/*++
 
PrintDataFormat:
 
	This function prints the basic data format in the following way:
	    [baudrate]-[data bits][parity][stop bits]
 
Arguments:
 
    pDCB    pointer to a DCB	
 
Return Value:
 
	NONE
 

 
	Uknown (unknown)
 
Notes:
 
--*/
VOID LogDataFormat( DCB *pDCB )
{
    TCHAR szBuffer[120];
    TCHAR cTmp;
    INT   iBufIdx;
    INT   iIdx;

    iBufIdx = wsprintf( szBuffer, TEXT("  ** COMM port set to:  ") );

    for( iIdx = 0; iIdx < NUMBAUDS; iIdx++ )
        if( g_BaudTable[iIdx].dwBaud == pDCB->BaudRate )
            break;

    if( iIdx < NUMBAUDS )
        iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("%s-"), 
                             g_BaudTable[iIdx].ptszString );
    else
        iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("%d-"), 
                             pDCB->BaudRate );
    
    iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("%1d"), pDCB->ByteSize );

    switch( pDCB->Parity )
    {
    
    case 0: cTmp = 'n'; break;
    case 1: cTmp = 'o'; break;
    case 2: cTmp = 'e'; break;
    case 3: cTmp = 'm'; break;
    case 4: cTmp = 's'; break;
    default: cTmp = '-'; break;

    } // end switch( pDCB->Parity )
    
    iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("%1c"), cTmp );

    switch( pDCB->StopBits )
    {
    
    case 0: 
        iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("1"));
        break;
    case 1: 
        iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("1.5"));
        break;
    case 2:
        iBufIdx += wsprintf( &(szBuffer[iBufIdx]), TEXT("2"));
        break;

    } // end switch( pDCB->Parity )

    wsprintf( &(szBuffer[iBufIdx]), TEXT("\n"));

    g_pKato->Log( LOG_DETAIL, szBuffer );

} // end VOID SerLink::PrintDataFormat( DCB *pDCB )


BOOL ClearTestCommErrors( HANDLE hCommPort )
{
    DWORD   dwCommError = 0;
    BOOL    bRtn = FALSE;

    bRtn = ClearCommError( hCommPort, &dwCommError, NULL );
    FUNCTION_ERROR( FALSE == bRtn, return FALSE );

    if( dwCommError ) ShowCommError( dwCommError );
    
    return FALSE;

} // end BOOL ClearTestCommErrors( HANDLE hCommPort )


void ShowCommError( DWORD dwErrors )
{
    int     i;
    TCHAR   szBuffer[14 + NUMCOMMERRORS * 12];
    int     iBufIdx = 0;
    
    for( i = 0; i < NUMCOMMERRORS; i++ ) {
        if( dwErrors & CommErrs[i].dwErrNum ) 
        {
            iBufIdx += wsprintf( &(szBuffer[iBufIdx]),
                                 TEXT("%s  "), 
                                 CommErrs[i].ptszString );
                                 
        } // end if( dwErrors & CommErrs[i].dwErrNum )
        
    } // end for( i = 0; i < NUMCOMMERRORS; i++ )

    if( iBufIdx )
        g_pKato->Log( LOG_DETAIL, TEXT("  ** COMM ERRORS:%s\n"), szBuffer );
    
} // end void SerLink::ShowCommError( DWORD dwErrors )


