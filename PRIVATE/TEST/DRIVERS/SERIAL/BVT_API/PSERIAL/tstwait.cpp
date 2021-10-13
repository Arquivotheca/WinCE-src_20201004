/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tstwait.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
   TstWait.cpp
 
Abstract:
 
   These test test the CommWaitEvent fuction
 

 
   Uknown (unknown)
 
Notes:
 
--*/
#define __THIS_FILE__ TEXT("TstWait.cpp")

#include "PSerial.h"
#include "TstModem.h"
#include "TstModem.h"


/*++
 
EscapeWaitCommEventThread:
    
 
Arguments:
 
   lpCtrl  Pointer to a COMMTHREADCTRL struct used to pass data and control
   event to the thread
 
Return Value:
 
   DWORD
 

 
   Uknown (unknown)
 
Notes:
 
--*/
/*-----------------------------------------------------------------------------
  Function     :    static DWORD WINAPI EscapeWaitCommEventThread( LPCOMMTHREADCTRL lpCtrl )
  Description  :    This thread is used to break out of WaitCommEvent loops after a
                    predetermined time.  This keeps a test fcn from locking if
                    an event is never received
  Parameters   :    lpCtrl  Pointer to a COMMTHREADCTRL struct used to pass data and control
                    event to the thread

  Returns      :
  Comments     :
------------------------------------------------------------------------------*/
static DWORD WINAPI EscapeWaitCommEventThread( LPCOMMTHREADCTRL lpCtrl )
{
    BOOL    bRtn;
    DWORD   dwRtn;

    g_pKato->Log( LOG_DETAIL,TEXT("EscapeWaitCommEventThread RUNNING"));
    dwRtn = WaitForSingleObject( lpCtrl->hEndEvent, lpCtrl->dwData );
    DEFAULT_ERROR(WAIT_FAILED == dwRtn, ExitThread( 1 ));
    while( WAIT_TIMEOUT == dwRtn )
    {
        g_pKato->Log( LOG_DETAIL, TEXT("EscapeWaitCommEventThread: Reseting CommMask"));
        bRtn = SetCommMask( lpCtrl->hPort, EV_ERR );
        g_pKato->Log( LOG_DETAIL, TEXT("EscapeWaitCommEventThread: Reset CommMask"));
                         
        dwRtn = WaitForSingleObject( lpCtrl->hEndEvent, lpCtrl->dwData );
        DEFAULT_ERROR( WAIT_FAILED == dwRtn, ExitThread( 1 ) );

    } // end while( TIMEOUT )

    g_pKato->Log( LOG_DETAIL, TEXT("EscapeWaitCommEventThread EXITING"));
    ExitThread( 0 );
    return 0; // never executed but it makes the compiler happy.
} // end static DWORD WINAPI EscapeWaitCommEventThread( LPCOMMTHREADCTRL lpCtrl )

// 
// data structure and thread routine used for the TestCommEventTXEmpty routine
// 
typedef struct _BUFFEROUTPUT {
    BOOL fKeepSending;
    HANDLE hCommPort;
} BUFFEROUTPUT, *LPBUFFEROUTPUT;

#define OUTPUT_BUFFER_SIZE  5
#define BUFFER_CHAR         0xAA

//  
//  FillTxBufferThread continually fills the TX output buffer with
//  data to send. It matters not if there is someone to receive it,
//  it simply sends data until lpBufOut->fKeepSending is set to FALSE
//  when it immediately exits.
//
//  INPUT: 
//      BUFFEROUTPUT lpBufOut:  contains a comm port handle and a 
//                              signal flag
//  
//  OUTPUT:
//      
//  REMARKS:
//      
//
static DWORD WINAPI FillTxBufferThread( LPBUFFEROUTPUT lpBufOut )
{
    DWORD dwWritten = 0;
    DWORD dwLastErr = 0;
    DWORD dwResult = 0;
    LPOVERLAPPED pOvr = NULL;

    LPBYTE pbBuffer = (LPBYTE)malloc(OUTPUT_BUFFER_SIZE);
    
#ifdef UNDER_NT    
    OVERLAPPED osWrite = {0};    
    osWrite.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );    
    DEFAULT_ERROR( NULL == osWrite.hEvent, goto Exit );    
    pOvr = &osWrite;
#endif
    
    DEFAULT_ERROR( NULL == pbBuffer, goto Exit );
    
    g_pKato->Log( LOG_DETAIL, TEXT("FillTxBufferThread RUNNING") );

    while( lpBufOut->fKeepSending )
    {
        // set the output characters to our output character
        memset( pbBuffer, BUFFER_CHAR, OUTPUT_BUFFER_SIZE );
        g_pKato->Log( LOG_DETAIL, TEXT("FillTxBufferThread -- sending data") );
        if( !WriteFile( lpBufOut->hCommPort, pbBuffer, OUTPUT_BUFFER_SIZE, &dwWritten, pOvr ) )
        {
            dwLastErr = GetLastError();
#ifdef UNDER_NT        
            // handle overlapped I/O

            if( ERROR_IO_PENDING == dwLastErr ) 
            {       
                dwResult = WaitForSingleObject( osWrite.hEvent, INFINITE );
                DEFAULT_ERROR( WAIT_OBJECT_0 != dwResult, goto Exit );    
                FUNCTION_ERROR( !GetOverlappedResult( lpBufOut->hCommPort, &osWrite, &dwWritten, FALSE ), goto Exit );
            }
            else
            {
                g_pKato->Log( LOG_DETAIL, TEXT("FillTxBufferThread -- failed to send! error %d"), dwLastErr );
            }                
#else
            g_pKato->Log( LOG_DETAIL, TEXT("FillTxBufferThread -- failed to send! error %d"), dwLastErr );
#endif
        }
        g_pKato->Log( LOG_DETAIL, TEXT("FillTxBufferThread -- finished sent %lu bytes"), dwWritten );
        CMDLINENUM( Sleep( 2000 ) );
    }
    g_pKato->Log( LOG_DETAIL, TEXT("FillTxBufferThread EXITING") );

Exit:
    if( pbBuffer )
    {
        free( pbBuffer );
    }
#ifdef UNDER_NT    
    if( osWrite.hEvent )
    {
        CloseHandle( osWrite.hEvent );
    }
#endif    
    ExitThread(0);
    return 0;
}
//
// end TestCommEventTXEmpty definitions
// 

/*++
 
TestSignalLine:
 
    This test is for WaitCommEvent on EV_CTS and EV_DSR using EscapeComm
    function for RTS and DTR.
    
Arguments:
 
   TUX standard arguments.
 
Return Value:
 
   TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
   TPR_EXECUTE: for TPM_EXECUTE
   TPR_NOT_HANDLED: for all other messages.
 

 
   Uknown (unknown)
 
Notes:
 
   
 
--*/
TESTPROCAPI TestCommEventSignals( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    SHORT           nSet            = 0;
    DWORD           dwResult        = TPR_PASS;
    DWORD           dwEvent         = 0;
    DWORD           dwMask          = 0;
    HANDLE          hCommPort       = NULL;
    INT             iIdx;
    INT             iTestIdx;
    COMMTHREADCTRL  EscapeThreadCtrl;
    HANDLE          hEscapeThread   = NULL;
    LPOVERLAPPED    lpOverLapped    = NULL;
    DWORD           dwPortFlag      = 0;
#ifdef UNDER_NT
    OVERLAPPED  OvrLpd;
#endif
    struct EVENTTEST 
    {               
        LPTSTR  szTest;
        DWORD   dwProperty;
        DWORD   dwEventMask;
        LPTSTR  szEventMask;
        DWORD   dwOnFunc;
        LPTSTR  szOnFunc;
        DWORD   dwOffFunc;
        LPTSTR  szOffFunc;
    } aEventTests[] =
        {   
         {  TEXT("CTS-RTS"),
            PCF_RTSCTS, 
            EV_CTS,
            TEXT("EV_CTS"),     
            SETRTS,
            TEXT("SETRTS"),       
            CLRRTS,
            TEXT("CLRRTS") 
         },
            
         {  TEXT("DTR-DSR"),
            PCF_DTRDSR,
            EV_DSR,
            TEXT("EV_DSR"), 
            SETDTR,
            TEXT("SETDTR"), 
            CLRDTR,
            TEXT("CLRDTR") 
         },
            
         {  TEXT("DTR-RLSD"),
            PCF_DTRDSR,
            EV_RLSD,
            TEXT("EV_RSLD"), 
            SETDTR,
            TEXT("SETDTR"), 
            CLRDTR,
            TEXT("CLRDTR") 
         }
        };
      
      #define NUMEVENTTESTS (sizeof(aEventTests)/sizeof(struct EVENTTEST))

    /* --------------------------------------------------------------------
      Sync the begining of the test
    -------------------------------------------------------------------- */
    bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
   DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT );

    // Do this before the __try statement.
    ZeroMemory( &EscapeThreadCtrl, sizeof( EscapeThreadCtrl ) );
    
 //   __try {

    /* --------------------------------------------------------------------
      Under NT The port must be opened for Overlapped IO
    -------------------------------------------------------------------- */
#ifdef UNDER_NT
    ZeroMemory( &OvrLpd, sizeof(OvrLpd) );
    OvrLpd.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    DEFAULT_ERROR( NULL == OvrLpd.hEvent, goto TCECleanup );

    lpOverLapped = &OvrLpd;
    dwPortFlag = FILE_FLAG_OVERLAPPED;
#endif

    /* --------------------------------------------------------------------
      Create and set up a default comm port.
    -------------------------------------------------------------------- */
    if(g_fBT && g_fMaster)
    {
    	hCommPort = g_hBTPort;
    }
    else
    {
	    hCommPort = CreateFile( g_lpszCommPort, 
	                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                            OPEN_EXISTING, dwPortFlag, lpOverLapped );
    }

    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , goto TCECleanup);

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TCECleanup);

    /* --------------------------------------------------------------------
        If the Com Port Properties haven't been negotiated.  Log a warning 
        and then get a new set for the local port.
    -------------------------------------------------------------------- */
    if( !g_fSetCommProp )
    {
        g_pKato->Log( LOG_WARNING, 
                      TEXT("WARNING: in %s @ line %d: g_CommProp NOT negotiated using local value."),
                      __THIS_FILE__, __LINE__ );
        bRtn =  GetCommProperties( hCommPort, &g_CommProp );
        COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_FAIL; goto TCECleanup);
        
    } // end if( !g_fSetCommProp )

    /* -------------------------------------------------------------------
      A thread is created to prevent the test from hanging.  The thread
      waits a predetermend time and then changes the ports event mask
      to zero.  This will cause a WaitCommEvent to return dwMask == 0.
    ------------------------------------------------------------------- */
    EscapeThreadCtrl.hPort = hCommPort;
    EscapeThreadCtrl.hEndEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    FUNCTION_ERROR( NULL == EscapeThreadCtrl.hEndEvent, dwResult = TPR_FAIL; goto TCECleanup);
    EscapeThreadCtrl.dwData = 25000; // 25 Seconds
      
    hEscapeThread = CreateThread( NULL, 0, 
                                  (LPTHREAD_START_ROUTINE)EscapeWaitCommEventThread,
                                  (LPVOID)&EscapeThreadCtrl, 0, &dwEvent );
    FUNCTION_ERROR( NULL == hEscapeThread, dwResult = TPR_FAIL; goto TCECleanup);
    Sleep( 100 );   // To guarantee task switch
    
    /* --------------------------------------------------------------------
        For each EventTest
    -------------------------------------------------------------------- */
    for( iTestIdx = 0; iTestIdx < NUMEVENTTESTS; iTestIdx++ )
    {
        /* ----------------------------------------------------------------
         Test is only preformed if supported in test enviroment.
        ---------------------------------------------------------------- */
      if( aEventTests[iTestIdx].dwProperty & g_CommProp.dwProvCapabilities )
      {
         g_pKato->Log( LOG_DETAIL, TEXT("TestCommEventSignals: Testing %s"), aEventTests[iTestIdx].szTest );
            
         /* ------------------------------------------------------------
                Inorder to minimize code duplication a loop is used for 
                flow control.  The master uses a set-wait-clr-wait flow
                and the slave uses a wait-set-wait-clr flow.  Inside the
                loop the master set/clr the event on odd iterations.  
                Setting is done in the first half of the test clearing
                in last part.
            ------------------------------------------------------------ */
            nSet = g_fMaster ? 1 : 0;
            for( iIdx = 1; iIdx <= 4; iIdx++ )
            {
                CMDLINENUM( bRtn = SetCommMask( hCommPort, aEventTests[iTestIdx].dwEventMask ) );
                COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECleanup);

                if( (iIdx % 2) == nSet )
                {
                    CMDLINENUM( Sleep( 5000 ) );

                    // if( 3 > iIdx )
                    switch( iIdx ) {
                    case 1:
                    case 2:
                    {
                        g_pKato->Log( LOG_DETAIL, 
                                      TEXT("In %s @ line %d: EscapeCommFunction %s"),
                                    __THIS_FILE__, __LINE__, aEventTests[iTestIdx].szOnFunc );

                        bRtn = EscapeCommFunction( hCommPort, aEventTests[iTestIdx].dwOnFunc );
                        COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECleanup);
                        break;
                    } 
                    // else
                    case 3:
                    case 4:
                    {
                        g_pKato->Log( LOG_DETAIL, 
                                      TEXT("In %s @ line %d: EscapeCommFunction %s"),
                                    __THIS_FILE__, __LINE__, aEventTests[iTestIdx].szOffFunc );
                        bRtn = EscapeCommFunction( hCommPort, aEventTests[iTestIdx].dwOffFunc );
                        COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECleanup);
                        break;
                    } // end if( 3 > iIdx ) else

                    }
                } // end if( (iIdx % 2) == nSet )

                else
                {
                    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: WaitCommEvent %s"),
                                __THIS_FILE__, __LINE__, aEventTests[iTestIdx].szEventMask );

                    bRtn = WaitCommEvent( hCommPort, &dwEvent, lpOverLapped );
                    if( FALSE == bRtn )
                    {
                        DWORD dwRtn = GetLastError();
#ifdef UNDER_NT                    
                        /* ----------------------------------------------------
                         Handle the NT special cases
                        ---------------------------------------------------- */
                        if( dwRtn == ERROR_IO_PENDING )
                        {
                            CMDLINENUM( bRtn = GetOverlappedResult( hCommPort, lpOverLapped,  &dwRtn, TRUE ));
                            
                        } // end if( dwRtn == ERROR_IO_PENDING )
                        
                        else 
                        {
                            g_pKato->Log( LOG_FAIL, 
                                          TEXT("FAIL in %s @ line %d: WaitCommEvent failed with %d"),
                                          __FILE__, __LINE__, dwRtn );
                            break;
                            
                        } // end if( dwRtn == ERROR_IO_PENDING ) else
#endif // UNDER_NT               
                        if( ERROR_INVALID_PARAMETER == dwRtn )
                        {
                            dwRtn = TRUE;
                        }
                        
                        else if( FALSE == bRtn && dwRtn )
                        {
                            g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d"),
                                      __THIS_FILE__, __LINE__, dwRtn );
                            ClearTestCommErrors( hCommPort );
                            goto TCECleanup;
                        
                        } // end if( dwRtn )


                    } 

                    // added to test the GetCommMask function 
                    bRtn = GetCommMask( hCommPort, &dwMask );
                    COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECleanup );
                    // if the event received was not the event expected
                    if( 0 == (aEventTests[iTestIdx].dwEventMask & dwEvent & dwMask) ) 
                    { 
                        break;
                    }

                    else 
                    {
                        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: %s Event successfully received"),
                                    __THIS_FILE__, __LINE__, aEventTests[iTestIdx].szEventMask );
                    }
                
                } // end if else; 

            } // end  for( iIdx = 1; iIdx <= 4; iIdx++ )

            /* ----------------------------------------------------------------
             Test failed.
            ---------------------------------------------------------------- */
            if( 0 == ( aEventTests[iTestIdx].dwEventMask & dwEvent ) )
            {
                g_pKato->Log( LOG_WARNING, 
                              TEXT("WARNING in %s @ %d:  Didn't recieve %s.  ")
                              TEXT("Note this could be a cable problem"),
                              __THIS_FILE__, __LINE__, aEventTests[iTestIdx].szEventMask );
            
            } // end if( 0== ( aEventTests[iTestIdx].dwEventMask & dwEvent ) )

        } // end if( CONTROL SUPPORTED IN TEST ENVIROMENT )
    
        else
        {
            g_pKato->Log( LOG_WARNING, 
                          TEXT("WARNING in %s @ %d:  Test Enviroment doesn't support %s control, ")
                          TEXT("0x%08X doesn't include 0x%08X"),
                          __THIS_FILE__, __LINE__, aEventTests[iTestIdx].szTest,
                          g_CommProp.dwProvCapabilities, aEventTests[iTestIdx].dwProperty );
                      
        } // end end if( CONTROL SUPPORTED IN TEST ENVIROMENT ) else

        do{ 
            bRtn = GetCommMask( hCommPort, &dwEvent );
            COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECleanup);
            Sleep( 1000 );
        } while( dwEvent & ~EV_ERR );

    } // end for( iTestIdx = 0; iTestIdx <= NUMEVENTTESTS; iTestIdx++ )
    
    TCECleanup:

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );

    if( hEscapeThread )
    {
        bRtn = SetEvent( EscapeThreadCtrl.hEndEvent );
        FUNCTION_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );
      
        CMDLINENUM( dwEvent = WaitForSingleObject( hEscapeThread, INFINITE ) );
        dwEvent = WaitForSingleObject( hEscapeThread, INFINITE );
        FUNCTION_ERROR( WAIT_FAILED == dwEvent, dwResult = TPR_FAIL );
        
        CloseHandle( hEscapeThread );

#ifdef UNDER_NT
        CloseHandle(  OvrLpd.hEvent );
#endif        
        
    } // end if( hEscapeThread )
    CloseHandle( EscapeThreadCtrl.hEndEvent );

    if(!g_fBT || !g_fMaster)
    {
    	CloseHandle( hCommPort );
    }

    /*--------------------------------------------------------------------
      Sync the end of a test of the test.
    -------------------------------------------------------------------- */
    dwResult = EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
   
    return dwResult;
   
} // end TESTPROCAPI TestCommEventSignals( ... )


/*++
 
TestCommEventBreak:
 
    This test checks WaitCommEvents for BREAK using EscapeCommFucntion.
    
Arguments:
 
   TUX standard arguments.
 
Return Value:
 
   TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
   TPR_EXECUTE: for TPM_EXECUTE
   TPR_NOT_HANDLED: for all other messages.
 

 
   Uknown (unknown)
 
Notes:
 
   
 
--*/
TESTPROCAPI TestCommEventBreak( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    BOOL            fSet            = FALSE;
    DWORD           dwResult        = TPR_PASS;
    DWORD           dwEvent         = 0;
    DWORD           dwMask          = 0;
    DWORD           dwCommError     = 0;
    HANDLE          hCommPort       = NULL;
    INT             iIdx;
    COMMTHREADCTRL  EscapeThreadCtrl;
    HANDLE          hEscapeThread   = NULL;
    LPOVERLAPPED    lpOverLapped    = NULL;
    DWORD           dwPortFlag      = 0;
#ifdef UNDER_NT
    OVERLAPPED    OvrLpd;
#endif

    /* --------------------------------------------------------------------
      Sync the begining of the test
    -------------------------------------------------------------------- */
    bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
   DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT );

    // Do this before the __try statement.
    ZeroMemory( &EscapeThreadCtrl, sizeof( EscapeThreadCtrl ) );
    
//    __try {

    /* --------------------------------------------------------------------
      Under NT The port must be opened for Overlapped IO
    -------------------------------------------------------------------- */
#ifdef UNDER_NT
    ZeroMemory( &OvrLpd, sizeof(OvrLpd) );
    OvrLpd.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    DEFAULT_ERROR( NULL == OvrLpd.hEvent, goto TCEBCleanup );

    lpOverLapped = &OvrLpd;
    dwPortFlag = FILE_FLAG_OVERLAPPED;
#endif

    /* --------------------------------------------------------------------
      Create and set up a default comm port.
    -------------------------------------------------------------------- */
    if(g_fBT && g_fMaster)
    {
    	hCommPort = g_hBTPort;
    }
    else
    {
	    hCommPort = CreateFile( g_lpszCommPort, 
	                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                            OPEN_EXISTING, dwPortFlag, lpOverLapped );
    }

    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , goto TCEBCleanup);
    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TCEBCleanup);

    /* -------------------------------------------------------------------
      A thread is created to prevent the test form hanging.  The thread
      waits a predetermend time and then changes the ports event mask
      to zero.  This will cause a WaitCommEvent to return dwMask == 0.
    ------------------------------------------------------------------- */
    EscapeThreadCtrl.hPort = hCommPort;
    EscapeThreadCtrl.hEndEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    FUNCTION_ERROR( NULL == EscapeThreadCtrl.hEndEvent, dwResult = TPR_FAIL; goto TCEBCleanup);
    EscapeThreadCtrl.dwData = 12000; // 12 Seconds
      
    hEscapeThread = CreateThread( NULL, 0, 
                                  (LPTHREAD_START_ROUTINE)EscapeWaitCommEventThread,
                                  (LPVOID)&EscapeThreadCtrl, 0, &dwEvent );
    FUNCTION_ERROR( NULL == hEscapeThread, dwResult = TPR_FAIL; goto TCEBCleanup);
    Sleep( 100 );   // To guarantee task switch
    
    /* ------------------------------------------------------------
        The first time through the master set a break.  The second
        time through the slave sets a break.  When both sides
        have detected the break then the test is over.
    ------------------------------------------------------------ */
    fSet = g_fMaster;
    for( iIdx = 1; iIdx <= 2; iIdx++ )
    {
        CMDLINENUM( bRtn = SetCommMask( hCommPort, EV_BREAK ) );
        COMM_ERROR( hCommPort, FALSE == bRtn, goto TCEBCleanup);

        if( fSet )
        {
            fSet = FALSE;
            CMDLINENUM( Sleep( 5000 ) );

            CMDLINENUM( bRtn = EscapeCommFunction( hCommPort, SETBREAK ) );
            COMM_ERROR( hCommPort, FALSE == bRtn, goto TCEBCleanup);

            CMDLINENUM( Sleep( 1000 ) );

            CMDLINENUM( bRtn = EscapeCommFunction( hCommPort, CLRBREAK ) );
            COMM_ERROR( hCommPort, FALSE == bRtn, goto TCEBCleanup);
            
        } // end if( fSet )
        
        else
        {
            fSet = TRUE;
            
            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: WaitCommEvent EV_BREAK"),
                         __THIS_FILE__, __LINE__ );

            bRtn = WaitCommEvent( hCommPort, &dwEvent, lpOverLapped );
            if( FALSE == bRtn )
            {
                DWORD dwRtn = GetLastError();
#ifdef UNDER_NT                    
                /* ----------------------------------------------------
                    Handle the NT special cases
                ---------------------------------------------------- */
                if( dwRtn == ERROR_IO_PENDING )
                {
                    bRtn = GetOverlappedResult( hCommPort, lpOverLapped,  &dwRtn, TRUE );
                    
                } // end if( dwRtn == ERROR_IO_PENDING )
                
                else
                {
                    g_pKato->Log( LOG_FAIL, 
                                  TEXT("FAIL in %s @ line %d: WaitCommEvent failed with %d"),
                                  __FILE__, __LINE__, dwRtn );
                    break;
                    
                } // end if( dwRtn == ERROR_IO_PENDING ) else
#endif // UNDER_NT
                if( ERROR_INVALID_PARAMETER == dwRtn )
                {
                    dwRtn = TRUE;
                }

                else if( FALSE == bRtn && dwRtn )
                {
                    g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d"), \
                              __THIS_FILE__, __LINE__, GetLastError() ); \
                    ClearTestCommErrors( hCommPort );
                    goto TCEBCleanup;
                
                } // end if( dwRtn )

            } // end if( FALSE == bRtn ) 
            // if the event received was not the event expected


            /* ----------------------------------------------------------------
             Clear any comm errors
            ---------------------------------------------------------------- */
            do { // while COMM ERRORS 

                dwCommError = 0;
                bRtn = ClearCommError( hCommPort, &dwCommError, NULL );
                DEFAULT_ERROR( FALSE == bRtn, goto TCEBCleanup);
                if( dwCommError )   CMDLINENUM( ShowCommError( dwCommError ) );
                CMDLINENUM( Sleep( 200 ) );

            } while( dwCommError );
            
            //if( EV_BREAK != dwEvent ) break;
            if( 0 == (dwEvent & dwMask & EV_BREAK) ) break;

            else 
            {
                g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: EV_BREAK Event successfully received"),
                            __THIS_FILE__, __LINE__ );
            }

        } // end if( fSet ) else
 
    } // end  for( iIdx = 1; iIdx <= 2; iIdx++ )

    /* ----------------------------------------------------------------
        if dwEvent != EV_BREAK, ONLY WARN do not fail.  NOT ALL 
      DEVICES SUPPORT BREAK.
    ---------------------------------------------------------------- */
    if( EV_BREAK != dwEvent )
    {
        g_pKato->Log( LOG_WARNING, TEXT("WARNING in %s @ %d:  Didn't recieve EV_BREAK"),
                      __THIS_FILE__, __LINE__ );
    
    } // end if( EV_BREAK != dwEvent )
    
TCEBCleanup:

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );

    /* --------------------------------------------------------------------
      Clean Up the thread
    -------------------------------------------------------------------- */
    if( hEscapeThread )
    {
        bRtn = SetEvent( EscapeThreadCtrl.hEndEvent );
        FUNCTION_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );

        CMDLINENUM( dwEvent = WaitForSingleObject( hEscapeThread, INFINITE ) );
        FUNCTION_ERROR( WAIT_FAILED == dwEvent, dwResult = TPR_FAIL );

        CloseHandle( hEscapeThread );
      hEscapeThread = NULL;

#ifdef UNDER_NT
        CloseHandle(  OvrLpd.hEvent );
#endif        
        
    } // end if( hEscapeThread )
    
    CloseHandle( EscapeThreadCtrl.hEndEvent );

    if(!g_fBT || !g_fMaster)
    {
   		CloseHandle( hCommPort );
    }


   /* --------------------------------------------------------------------
      Sync the end of a test of the test.
    -------------------------------------------------------------------- */
   dwResult = EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
   
   return dwResult;
   
} // end TESTPROCAPI TestCommEventBreak( ... )

/*++
 
TestCommEventTXEmpty:
 
    This test checks WaitCommEvents for EV_TXEMPTY by spawning a thread
    to send data and waiting for the EV_TXEMPTY event when the data TX
    buffer is empty. This test requires no additional computer.
    
Arguments:
 
   TUX standard arguments.
 
Return Value:
 
   TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
   TPR_EXECUTE: for TPM_EXECUTE
   TPR_NOT_HANDLED: for all other messages.
 

 
   Uknown (unknown)
 
Notes:
 
--*/
TESTPROCAPI TestCommEventTxEmpty( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    BOOL            fSet            = FALSE;
    DWORD           dwResult        = TPR_PASS;
    DWORD           dwEvent         = 0;
    DWORD           dwMask          = 0;
    DWORD           dwCommError     = 0;
    DWORD           dwLastErr       = 0;
    HANDLE          hCommPort       = NULL;
    COMMTHREADCTRL  EscapeThreadCtrl;
    HANDLE          hEscapeThread   = NULL;
    HANDLE          hTXFillThread   = NULL;
    LPOVERLAPPED    lpOverLapped    = NULL;
    DWORD           dwPortFlag      = 0;
       BUFFEROUTPUT txThreadInfo ;
     
     
#ifdef UNDER_NT
    OVERLAPPED    OvrLpd;
#endif

    /* --------------------------------------------------------------------
      Sync the begining of the test -- this test involves no comm, but
      it is sync'd with the other machine so that they start and end
      together and subsequent tests will succeed.
    -------------------------------------------------------------------- */
    bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
   DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT );
    // Do this before the __try statement.
    ZeroMemory( &EscapeThreadCtrl, sizeof( EscapeThreadCtrl ) );
    
 //   __try {

    /* --------------------------------------------------------------------
      Under NT The port must be opened for Overlapped IO
    -------------------------------------------------------------------- */
#ifdef UNDER_NT
    ZeroMemory( &OvrLpd, sizeof(OvrLpd) );
    OvrLpd.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    DEFAULT_ERROR( NULL == OvrLpd.hEvent, goto TCETECleanup );

    lpOverLapped = &OvrLpd;
    dwPortFlag = FILE_FLAG_OVERLAPPED;
#endif
    /* --------------------------------------------------------------------
      Create and set up a default comm port.
    -------------------------------------------------------------------- */
    if(g_fBT && g_fMaster)
    {
    	hCommPort = g_hBTPort;
    }
    else
	{
    	hCommPort = CreateFile( g_lpszCommPort, 
	                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                            OPEN_EXISTING, dwPortFlag, lpOverLapped );
    }

	FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , goto TCETECleanup );
    
    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TCETECleanup );

    /* -------------------------------------------------------------------
      A thread is created to prevent the test form hanging.  The thread
      waits a predetermend time and then changes the ports event mask
      to zero.  This will cause a WaitCommEvent to return dwMask == 0
      so the test will fail rather than hang indefinitely.
    ------------------------------------------------------------------- */
    EscapeThreadCtrl.hPort = hCommPort;
    EscapeThreadCtrl.hEndEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    FUNCTION_ERROR( NULL == EscapeThreadCtrl.hEndEvent, dwResult = TPR_FAIL; goto TCETECleanup );
    EscapeThreadCtrl.dwData = 12000; // 12 Seconds
      
    hEscapeThread = CreateThread( NULL, 0, 
                                  (LPTHREAD_START_ROUTINE)EscapeWaitCommEventThread,
                                  (LPVOID)&EscapeThreadCtrl, 0, &dwEvent );
    FUNCTION_ERROR( NULL == hEscapeThread, dwResult = TPR_FAIL; goto TCETECleanup );
    Sleep( 100 );   // To guarantee task switch
    
    // set mask
    CMDLINENUM( bRtn = SetCommMask( hCommPort, EV_TXEMPTY ) );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TCETECleanup );

    // for controlling the output buffer thread
  txThreadInfo.fKeepSending=  TRUE;
  txThreadInfo.hCommPort =hCommPort ;

    hTXFillThread = CreateThread( NULL, 0, 
                          (LPTHREAD_START_ROUTINE)FillTxBufferThread,
                          (LPVOID)&txThreadInfo, 0, &dwEvent );
    FUNCTION_ERROR( NULL == hEscapeThread, dwResult = TPR_FAIL; goto TCETECleanup );
        
    g_pKato->Log( LOG_DETAIL, TEXT("waiting for comm event EV_TXEMPTY") );
    if( !WaitCommEvent( hCommPort, &dwEvent, lpOverLapped ) )
    {
#ifdef UNDER_NT    
        dwLastErr = GetLastError();
        if( ERROR_IO_PENDING == dwLastErr )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("WaitCommEvent IO Pending"));           
            dwLastErr = WaitForSingleObject( lpOverLapped->hEvent, INFINITE );
            DEFAULT_ERROR( WAIT_OBJECT_0 != dwLastErr, dwResult = TPR_FAIL );    
            FUNCTION_ERROR( 
                !GetOverlappedResult( hCommPort, lpOverLapped, &dwEvent, FALSE ), 
                dwResult = TPR_FAIL );
                            
        }
        else
        {
            g_pKato->Log( LOG_DETAIL, TEXT("WaitCommEvent failed"));
            dwResult = TPR_FAIL;
        }
#else
        g_pKato->Log( LOG_DETAIL, TEXT("WaitCommEvent failed"));
        dwResult = TPR_FAIL;
#endif        
    }

    
    // signal thread to stop sending data 
    txThreadInfo.fKeepSending = FALSE;
  
    bRtn = GetCommMask( hCommPort, &dwMask );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TCETECleanup );
    DEFAULT_ERROR( !(dwEvent & dwMask & EV_TXEMPTY ), dwResult = TPR_FAIL; goto TCETECleanup );
    g_pKato->Log( LOG_DETAIL, TEXT("EV_TXEMPTY event received") );



 TCETECleanup:

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );

    if( hEscapeThread )
    {
        bRtn = SetEvent( EscapeThreadCtrl.hEndEvent );
        FUNCTION_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );
        CMDLINENUM( dwEvent = WaitForSingleObject( hEscapeThread, INFINITE ) );
        FUNCTION_ERROR( WAIT_FAILED == dwEvent, dwResult = TPR_FAIL );

        CloseHandle( hEscapeThread );
        hEscapeThread = NULL;

#ifdef UNDER_NT
        CloseHandle(  OvrLpd.hEvent );
#endif        
        
    } // end if( hEscapeThread )

    if( hTXFillThread ) 
    {
        CMDLINENUM( dwEvent = WaitForSingleObject( hTXFillThread, INFINITE ) );
        FUNCTION_ERROR( WAIT_FAILED == dwEvent, dwResult = TPR_FAIL );
        CloseHandle( hTXFillThread );
        hTXFillThread = NULL;
    }

	if(!g_fBT || !g_fMaster)
	{
    	CloseHandle( hCommPort );
	}


    
   /* --------------------------------------------------------------------
      Sync the end of a test of the test.
    -------------------------------------------------------------------- */

   Sleep( 1000 ); // give time for the other machine to complete sending
   dwResult = EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
   
   return dwResult;
 
}

/*++
 
TestCommEventChars:
 
    This test checks WaitCommEvents for EV_RXCHAR and EV_RXFLAG using 
    TransmitCommChar.
    
Arguments:
 
   TUX standard arguments.
 
Return Value:
 
   TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
   TPR_EXECUTE: for TPM_EXECUTE
   TPR_NOT_HANDLED: for all other messages. 

 
   Uknown (unknown)
 
Notes:
 
   
 
--*/
TESTPROCAPI TestCommEventChars( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    BOOL            fSet            = FALSE;
    DWORD           dwResult        = TPR_PASS;
    DWORD           dwEvent         = 0;
    DWORD           dwMask          = 0;
    DWORD           dwCommError     = 0;
    HANDLE          hCommPort       = NULL;
    INT             iIdx;
    INT             iTestIdx;
    COMMTHREADCTRL  EscapeThreadCtrl;
    HANDLE          hEscapeThread   = NULL;
    LPOVERLAPPED    lpOverLapped    = NULL;
    DWORD           dwPortFlag      = 0;
    const UCHAR     cRxFlag         = 0xAA;
    DCB             LocalDCB;
#ifdef UNDER_NT
    OVERLAPPED  OvrLpd;
#endif
    struct EVENTCHAR
    {
        LPTSTR  szTest;
        DWORD   dwEvent;
        LPTSTR  szEvent;
        CHAR    cXmit;
    }
    aEventChar[] = {
       { TEXT("WaitCommEvent for EV_RXCHAR"), EV_RXCHAR, TEXT("EV_RXCHAR"), 0x55 },
       { TEXT("WaitCommEvent for EV_RXFLAG"), EV_RXFLAG, TEXT("EV_RXFLAG"), cRxFlag }       
    };
    
    /* --------------------------------------------------------------------
      Sync the begining of the test
    -------------------------------------------------------------------- */
    bRtn = BeginTestSync( NULL, lpFTE->dwUniqueID );
   DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT );

    ZeroMemory( &EscapeThreadCtrl, sizeof( EscapeThreadCtrl ) );
    
    
    /* --------------------------------------------------------------------
      Under NT The port must be opened for Overlapped IO
    -------------------------------------------------------------------- */
#ifdef UNDER_NT
    ZeroMemory( &OvrLpd, sizeof(OvrLpd) );
    OvrLpd.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    DEFAULT_ERROR( NULL == OvrLpd.hEvent, goto TCECCleanup );

    lpOverLapped = &OvrLpd;
    dwPortFlag = FILE_FLAG_OVERLAPPED;
#endif

    /* --------------------------------------------------------------------
      Create and set up a default comm port.
    -------------------------------------------------------------------- */
    if(g_fBT && g_fMaster)
    {
    	hCommPort = g_hBTPort;
    }
    else
    {
	    hCommPort = CreateFile( g_lpszCommPort, 
	                            GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                            OPEN_EXISTING, dwPortFlag, lpOverLapped );
    }

    FUNCTION_ERROR( INVALID_HANDLE_VALUE == hCommPort , goto TCECCleanup);
    
    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, goto TCECCleanup);

    /* --------------------------------------------------------------------
      Set the RXFLAG character
    -------------------------------------------------------------------- */
    bRtn = GetCommState( hCommPort, &LocalDCB );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECCleanup);
    LocalDCB.EvtChar = cRxFlag;
    bRtn = SetCommState( hCommPort, &LocalDCB );
    COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECCleanup);

    /* -------------------------------------------------------------------
      A thread is created to prevent the test form hanging.  The thread
      waits a predetermend time and then changes the ports event mask
      to zero.  This will cause a WaitCommEvent to return dwMask == 0.
    ------------------------------------------------------------------- */
    EscapeThreadCtrl.hPort = hCommPort;
    EscapeThreadCtrl.hEndEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    FUNCTION_ERROR( NULL == EscapeThreadCtrl.hEndEvent, dwResult = TPR_FAIL; goto TCECCleanup);
    EscapeThreadCtrl.dwData = 25000; // 25 Seconds
      
    hEscapeThread = CreateThread( NULL, 0, 
                                  (LPTHREAD_START_ROUTINE)EscapeWaitCommEventThread,
                                  (LPVOID)&EscapeThreadCtrl, 0, &dwEvent );
    FUNCTION_ERROR( NULL == hEscapeThread, dwResult = TPR_FAIL; goto TCECCleanup );
    Sleep( 100 );   // To guarantee task switch
    
    /* ------------------------------------------------------------
        The first time through the master set a break.  The second
        time through the slave sets a break.  When both sides
        have detected the break then the test is over.
    ------------------------------------------------------------ */
    fSet = g_fMaster;
    for( iIdx = 0; iIdx < 4; iIdx++ )
    {
        iTestIdx = iIdx / (int)2;
        
        g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: %s Test"),
                      __THIS_FILE__, __LINE__, aEventChar[iTestIdx].szTest );
                      
        CMDLINENUM( bRtn = SetCommMask( hCommPort, aEventChar[iTestIdx].dwEvent ) );
        COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECCleanup);

        if( fSet )
        {
            fSet = FALSE;
            CMDLINENUM( Sleep( 5000 ) );

            CMDLINENUM( bRtn = TransmitCommChar( hCommPort, aEventChar[iTestIdx].cXmit ) );
            COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECCleanup);
            
        } // end if( fSet )
        
        else
        {
            fSet = TRUE;
            
            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: WaitCommEvent %s"),
                         __THIS_FILE__, __LINE__, aEventChar[iTestIdx].szEvent );

            CMDLINENUM( bRtn = WaitCommEvent( hCommPort, &dwEvent, lpOverLapped ) );
            if( FALSE == bRtn )
            {
                DWORD dwRtn = GetLastError();

#ifdef UNDER_NT                    
                /* ----------------------------------------------------
                    Handle the NT special cases
                ---------------------------------------------------- */
                if( dwRtn == ERROR_IO_PENDING )
                {
                    bRtn = GetOverlappedResult( hCommPort, lpOverLapped,  &dwRtn, TRUE );
                    
                } // end if( dwRtn == ERROR_IO_PENDING )
                
                else
                {
                    g_pKato->Log( LOG_FAIL, 
                                  TEXT("FAIL in %s @ line %d: WaitCommEvent failed with %d"),
                                  __FILE__, __LINE__, dwRtn );
                    break;
                    
                } // end if( dwRtn == ERROR_IO_PENDING ) else
#endif UNDER_NT

                if( ERROR_INVALID_PARAMETER == dwRtn )
                {
                    bRtn = TRUE;
                }
                else  if( FALSE == bRtn && dwRtn )
                {
                    g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d"), \
                              __THIS_FILE__, __LINE__, GetLastError() ); \
                    ClearTestCommErrors( hCommPort );
                    goto TCECCleanup;
                
                } // end if( dwRtn )

            } // end if( fSet ) else
            
            bRtn = GetCommMask( hCommPort, &dwMask );
            COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECCleanup);
            
            if( 0 == (aEventChar[iTestIdx].dwEvent & dwEvent & dwMask) ) break;
            else 
            {
                g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: %s Event successfully received"),
                            __THIS_FILE__, __LINE__, aEventChar[iTestIdx].szEvent );
            }

        } // end if( fSet ) else

        bRtn = GetCommMask( hCommPort, &dwMask );
        COMM_ERROR( hCommPort, FALSE == bRtn, goto TCECCleanup); 
        /* ----------------------------------------------------------------
            Test failed.
        ---------------------------------------------------------------- */
        if( 0 == (aEventChar[iTestIdx].dwEvent & dwEvent & dwMask) )
        {
            g_pKato->Log( LOG_WARNING, 
                          TEXT("WARNING in %s @ %d:  Didn't recieve %s"),
                          __THIS_FILE__, __LINE__, aEventChar[iTestIdx].szEvent );
    
        } // end if( EV_BREAK == dwEvent )
      
    } // end  for( iIdx = 1; iIdx <= 2; iIdx++ )

    
    TCECCleanup:

    if( FALSE == bRtn ) dwResult = TPR_FAIL;

    bRtn = SetupDefaultPort( hCommPort );
    DEFAULT_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );

    /* --------------------------------------------------------------------
      Clean Up the thread
    -------------------------------------------------------------------- */
    if( hEscapeThread )
    {
      
      g_pKato->Log( LOG_FAIL, TEXT("in %s @ line %d: Calling SetEvent()"), __THIS_FILE__, __LINE__);
      CMDLINENUM( bRtn = SetEvent( EscapeThreadCtrl.hEndEvent ) );
      FUNCTION_ERROR( FALSE == bRtn, dwResult = TPR_FAIL );

      g_pKato->Log( LOG_FAIL, TEXT("#############in %s @ line %d: Calling WaitForSingleObject()"), __THIS_FILE__, __LINE__);
      CMDLINENUM( dwEvent = WaitForSingleObject( hEscapeThread, INFINITE ) );
      g_pKato->Log( LOG_FAIL, TEXT("#############in %s @ line %d: Called WaitForSingleObject()"), __THIS_FILE__, __LINE__);
      FUNCTION_ERROR( WAIT_FAILED == dwEvent, dwResult = TPR_FAIL );

      g_pKato->Log( LOG_FAIL, TEXT("in %s @ line %d: Calling CloseHandle(hEscapeThread), hEscapeThread=0x%X"), __THIS_FILE__, __LINE__, hEscapeThread);
      CloseHandle( hEscapeThread );


#ifdef UNDER_NT
        CloseHandle(  OvrLpd.hEvent );
#endif        
        
    } // end if( hEscapeThread )
    
    CloseHandle( EscapeThreadCtrl.hEndEvent );

    if(!g_fBT || !g_fMaster)
    {
   		CloseHandle( hCommPort );
    }


   /* --------------------------------------------------------------------
      Sync the end of a test of the test.
    -------------------------------------------------------------------- */
   dwResult = EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
   
   return dwResult;
   
} // end TESTPROCAPI TestCommEventChar( ... )

