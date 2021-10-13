/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     tuxmain.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	tuxmain.cpp
 
Abstract:
 
	This is file contains the the function needed to set up the
	TUX test enviroment.
 

 
	Uknown (unknown)
 
Notes:
 
--*/

#define __THIS_FILE__   TEXT("TUXMAIN.CPP")

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
//#include "netqalog.h"
#include <kato.h>

#include "PSerial.h"
#include "GSerial.h"
#include "TstModem.h"


#define BVT_BASE       10
#define ERROR_BASE     100
#define STRESS_BASE    1000
#define OTHER_BASE     10000

static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("BVT"                              ), 0,       0,            0,	NULL,
    TEXT("TempleteTest"                     ), 1,       0,  BVT_BASE+ 0,	TempleteTest,
    TEXT("NegotiateSerialProperties"        ), 1,       0,  BVT_BASE+ 1,	NegotiateSerialProperties,
    TEXT("TestReadDataParityAndStop"        ), 1,    2400,  BVT_BASE+ 2,	TestReadDataParityAndStop,    
    TEXT("TestCommEventSignals"             ), 1,       0,  BVT_BASE+ 3,	TestCommEventSignals,
	TEXT("TestCommEventBreak"               ), 1,       0,  BVT_BASE+ 4,	TestCommEventBreak,
    TEXT("TestCommEventChars"               ), 1,       0,  BVT_BASE+ 5,	TestCommEventChars,
    TEXT("TestModemSignals"                 ), 1,       0,  BVT_BASE+ 6,	TestModemSignals,    
    TEXT("TestReadTimeouts"                 ), 1,       0,  BVT_BASE+ 7,	TestReadTimeouts,
    TEXT("TestWriteTimeouts"                ), 1,       0,  BVT_BASE+ 8,	TestWriteTimeouts,
    TEXT("TestPurgeCommRxTx"                ), 1,       0,  BVT_BASE+ 9,	TestPurgeCommRxTx,
	TEXT("TestXonXoffIdle"					), 1,       0,  BVT_BASE+ 10,	TestXonXoffIdle,
	TEXT("TestXonXoffReliable"				), 1,		0,  BVT_BASE+ 11,	TestXonXoffReliable,
	TEXT("Test Opening & Closing Ports"		), 1,		0,  BVT_BASE+ 12,	TestPorts,
    TEXT("TestCommEventTxEmpty"             ), 1,       0,  BVT_BASE+ 13,   TestCommEventTxEmpty,
	TEXT("Round Trip Test"                  ), 0,       0,             0,   NULL,
	TEXT("Test Transfer(Round Trip) Time for 9600"), 1,     1,  BVT_BASE+ 20,   DataSpeedTest,
	TEXT("Test Transfer(Round Trip) Time for 19200"), 1,    2,  BVT_BASE+ 21,   DataSpeedTest,
	TEXT("Test Transfer(Round Trip) Time for 38400"), 1,    3,  BVT_BASE+ 22,   DataSpeedTest,
	TEXT("Test Transfer(Round Trip) Time for 57600"), 1,    4,  BVT_BASE+ 23,   DataSpeedTest,
//	TEXT("Test Transfer(Round Trip) Time for 115200"), 1,    5,  BVT_BASE+ 24,   DataSpeedTest,

	TEXT("Xmit Speed Test"                  ),0,         0,            0,   NULL,
	TEXT("Xmit Speed Test For Buffer 1"     ),1,          1,     BVT_BASE+ 31,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 2"     ),1,          2,     BVT_BASE+ 32,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 8"     ),1,          8,     BVT_BASE+ 33,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 32"     ),1,         32,    BVT_BASE+ 34,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 64"     ),1,         64,    BVT_BASE+ 35,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 128"    ),1,         128,   BVT_BASE+ 36,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 512"    ),1,         512,   BVT_BASE+ 37,   DataXmitTest,
	TEXT("Xmit Speed Test For Buffer 1024"   ),1,         1024,  BVT_BASE+ 38,   DataXmitTest,
	TEXT("Receive Speed Test"                  ),0,         0,            0,   NULL,
	TEXT("Receive Speed Test For Buffer 1"   ),1,          1,     BVT_BASE+ 41,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 2"   ),1,          2,     BVT_BASE+ 42,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 8"   ),1,          8,     BVT_BASE+ 43,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 32"   ),1,         32,    BVT_BASE+ 44,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 64"   ),1,         64,    BVT_BASE+ 45,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 128"  ),1,         128,   BVT_BASE+ 46,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 512"  ),1,         512,   BVT_BASE+ 47,   DataReceiveTest,
	TEXT("Receive Speed Test For Buffer 1024" ),1,         1024,  BVT_BASE+ 48,   DataReceiveTest,

    NULL,                                      0,   0,  0, NULL  // marks end of list
};


// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;
//CNetQaLog *g_pKato = NULL;



// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO g_spsShellInfo;

/* ------------------------------------------------------------------------
	Global values
------------------------------------------------------------------------ */
BOOL        g_fMaster = TRUE;
BOOL		g_fDump  = FALSE;
BOOL        g_fInteractive = FALSE;
TCHAR       g_lpszCommPort[8];
INT			g_iBTChannel;
TCHAR		g_wszBTAddr[13];
COMMPROP    g_CommProp;
BOOL        g_fSetCommProp = FALSE;
BOOL        g_fAbortTesting = FALSE;
BOOL		g_fBT = FALSE;
HANDLE		g_hBTPort;

/* ------------------------------------------------------------------------
	Some constances
------------------------------------------------------------------------ */
static const  TCHAR CmdFlag         = (TCHAR)'-';
static const  TCHAR CmdPort         = (TCHAR)'p';
static const  TCHAR CmdSlave        = (TCHAR)'s';
static const  TCHAR CmdMaster       = (TCHAR)'m';
static const  TCHAR CmdBT			= (TCHAR)'b';
static const  TCHAR CmdDump         =  _T('d');
static const  TCHAR CmdSpace        = (TCHAR)' ';


/* ------------------------------------------------------------------------
    Module functions	
------------------------------------------------------------------------ */
static BOOL SetTestOptions( LPTSTR lpszCmdLine );


extern "C"
BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   return TRUE;
   
} // end BOOL WINAPI DllMain


/*++
 
ShellProc:
 
	This function provides the interface with the tux shell
 
Arguments:
 
	Standard Tux Shell Arguments
 
Return Value:
 
	Standard Tux Shell return codes
 


    Unknown (unknown) 
 
Notes:

    Addapted 1/23/97 by Uknown (unknown)
 
--*/
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{
    LPTSTR  lpszCmdLine = NULL;
    BOOL    fRtn        = FALSE;
    
    switch (uMsg) 
    {

    case SPM_LOAD_DLL:

        #ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
        #endif
       
        /* ----------------------------------------------------------------
            Get a kato object to do system logging	
        ---------------------------------------------------------------- */
        g_pKato = (CKato*)KatoGetDefaultObject();
		

        if( NULL == g_pKato )
        {
            OutputDebugString( TEXT("FATIAL ERROR: Couldn't get Kato Logging Object") );
            return SPR_FAIL;

        } // end if( NULL == g_pKato )

        KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

        g_pKato->Log( LOG_DETAIL, 
                      TEXT("**** Peer Serial Test built on %hs @ %hs ****"), 
                      __DATE__, __TIME__ );

    	/* --------------------------------------------------------------------
    		Set up the default enviroment.
	    -------------------------------------------------------------------- */
    	_tcscpy( g_lpszCommPort, DEFCOMMPORT );
    	g_fMaster = TRUE;
		ZeroMemory( &g_CommProp, sizeof(COMMPROP) );
		g_fSetCommProp = FALSE;
		g_fAbortTesting = FALSE;

        return SPR_HANDLED;

    case SPM_UNLOAD_DLL:
#ifdef UNDER_CE
    	UnregisterBTDevice();
#endif
        return SPR_HANDLED;

    case SPM_SHELL_INFO:
        g_spsShellInfo = *(LPSPS_SHELL_INFO)spParam;

        g_pKato->Log( LOG_DETAIL, TEXT("Get command line") );
        
        if( g_spsShellInfo.szDllCmdLine && g_spsShellInfo.szDllCmdLine[0] )
        {
            lpszCmdLine = new TCHAR[ (_tcslen(g_spsShellInfo.szDllCmdLine)+1) ];
            if( NULL == lpszCmdLine )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Couldn't set test options"),
                              __THIS_FILE__, __LINE__ );
                return SPR_FAIL;

            } // if( NULL == lpszCmdLine )

            _tcscpy( lpszCmdLine, g_spsShellInfo.szDllCmdLine );

            fRtn = SetTestOptions( lpszCmdLine );
            delete[] lpszCmdLine;
            if( !fRtn )
            {
                g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Couldn't set test options"),
                              __THIS_FILE__, __LINE__ );
                return SPR_FAIL;
            
            } // end if( NULL == g_pKato );

#ifdef UNDER_CE
            if(g_fBT && !RegisterBTDevice(g_fMaster, g_wszBTAddr, (g_lpszCommPort[3] - _T('0')), g_iBTChannel))
            {
				g_pKato->Log( LOG_FAIL, 
                              TEXT("FAIL in %s @ line %d: Couldn't register Bluetooth device"),
                              __THIS_FILE__, __LINE__ );
                return SPR_FAIL;
            }
#endif UNDER_CE

        }
        else
        {
            g_pKato->Log( LOG_WARNING, 
                          TEXT("WARNING in %s @ line %d:  No command line, running default test."),
                          __THIS_FILE__, __LINE__ );
        }
        
		g_pKato->Log( LOG_DETAIL, TEXT("Returning from SPM_SHELL_INFO") );
        return SPR_HANDLED;

    case SPM_REGISTER:

        // Initalize the return value to null.
//        ((LPSPS_REGISTER)spParam)->lpFunctionTable = NULL;
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
//        ((LPSPS_REGISTER)spParam)->lpFunctionTable = InitializeTestEnvironment();
       
        return SPR_HANDLED;

      case SPM_START_SCRIPT:
         return SPR_HANDLED;

      case SPM_STOP_SCRIPT:
         return SPR_HANDLED;

      case SPM_BEGIN_GROUP:
         //g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: PSERIAL.DLL"));

         return SPR_HANDLED;

      case SPM_END_GROUP:
         //g_pKato->EndLevel(TEXT("END GROUP: PSERIAL.DLL"));
         
         return SPR_HANDLED;

      case SPM_BEGIN_TEST:

         /* ---------------------------------------------------------------
         	if testing aborted skip all tests.
         --------------------------------------------------------------- */
         g_pKato->Log(LOG_DETAIL,TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
								((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpDescription,
                                ((LPSPS_BEGIN_TEST)spParam)->dwThreadCount,
                                ((LPSPS_BEGIN_TEST)spParam)->dwRandomSeed);

         if( g_fAbortTesting ) return SPR_SKIP;
                                
                                         
         return SPR_HANDLED;

        case SPM_END_TEST:

            g_pKato->Log(LOG_DETAIL, TEXT("END TEST: \"%s\" result == %d"),
                                   ((LPSPS_END_TEST)spParam)->lpFTE->lpDescription,
                                   ((LPSPS_END_TEST)spParam)->dwResult );

            if( TPR_ABORT == ((LPSPS_END_TEST)spParam)->dwResult )
            {
                g_pKato->Log( LOG_DETAIL, TEXT("Aborting on %d == %d"),
                              TPR_ABORT, ((LPSPS_END_TEST)spParam)->dwResult );
                g_fAbortTesting = TRUE;

            }

         return SPR_HANDLED;

      case SPM_EXCEPTION:
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
   }

   return SPR_NOT_HANDLED;

} // end ShellProc( ... )

/*++
 
SetTestOptions:
 
    This function takes a string with the command line arguments
    and sets up the test enviroment.  If the parameter is null
    then the user is prompted for a command line.
 
Arguments:

    lpszCmdLine     The command line string
 
Return Value:
 
	TRUE if success
 

 
	Uknown (unknown)
 
Notes:
 
--*/
static BOOL  SetTestOptions( LPTSTR lpszCmdLine )
{
    BOOL    fRtn        = FALSE;
    DWORD   dwLineSize  = 0;
    INT     iCmdIdx     = 0;
    INT     iStrEnd     = 0;
    TCHAR*	pTmp		= NULL;

    DEFAULT_ERROR( 0 == _tcslen(lpszCmdLine),  return TRUE );

    /* --------------------------------------------------------------------
    	Parse command line.
    -------------------------------------------------------------------- */
    iCmdIdx = 0;
    while( '\0' != lpszCmdLine[iCmdIdx] )
    {
        // remove junk
        while( CmdFlag != lpszCmdLine[iCmdIdx] && '\0' != lpszCmdLine[iCmdIdx] ) iCmdIdx++;
        if( '\0' == lpszCmdLine[iCmdIdx] ) continue;
        iCmdIdx++;
        
        switch( lpszCmdLine[iCmdIdx] )
        {
        case CmdPort:
        
            iCmdIdx++;
            while( CmdSpace == lpszCmdLine[iCmdIdx] ) iCmdIdx++;       
            if( '\0' == lpszCmdLine[iCmdIdx] ) continue;

            // Must use strlen because WinCE uses longer names.
            _tcsncpy( g_lpszCommPort, &(lpszCmdLine[iCmdIdx]), _tcslen(g_lpszCommPort) );
            iCmdIdx += _tcslen(g_lpszCommPort);

            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Option set port == %s"),
                          __THIS_FILE__, __LINE__, g_lpszCommPort );
            
        break;
        
        case CmdSlave:

            g_fMaster = FALSE;
            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Option set SLAVE"),
                          __THIS_FILE__, __LINE__ );

        break;
        
        case CmdMaster:

            g_fMaster = TRUE;
            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Option set MASTER"),
                          __THIS_FILE__, __LINE__ );
            
        break;
		case CmdDump:
			g_fDump=TRUE;
            g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Option set DUMP"),
                          __THIS_FILE__, __LINE__ );
			break;

#ifdef UNDER_CE
		case CmdBT:
			iCmdIdx++;

			g_fBT = TRUE;

			// Skip spaces
			while( CmdSpace == lpszCmdLine[iCmdIdx] ) iCmdIdx++;       
            if( _T('\0') == lpszCmdLine[iCmdIdx] ) continue;

			// Get the channel
			g_iBTChannel = 0;
			do
			{
				g_iBTChannel += lpszCmdLine[iCmdIdx] - _T('0');
				g_iBTChannel *= 10;
				iCmdIdx++;
			}
			while( CmdSpace != lpszCmdLine[iCmdIdx] && _T('\0') != lpszCmdLine[iCmdIdx]);
			g_iBTChannel /= 10;

			// Skip spaces
            while( CmdSpace == lpszCmdLine[iCmdIdx] ) iCmdIdx++;       
            if( _T('\0') == lpszCmdLine[iCmdIdx] ) continue;

			// BT address is optional so we may encounter a cmd flag
			if( CmdFlag == lpszCmdLine[iCmdIdx]) continue;

			// Get the address
			pTmp = g_wszBTAddr;
			do
			{
				*pTmp = lpszCmdLine[iCmdIdx];
				iCmdIdx++;
				pTmp++;
			}
			while( CmdSpace != lpszCmdLine[iCmdIdx] && _T('\0') != lpszCmdLine[iCmdIdx]);
			pTmp = _T('\0');
#endif // UNDER_CE
			
        } // end switch( lpszCmdLine[iCmdIdx] )

    } // end while( '\0' != lpszCmdLine[iCmdIdx] )

    return TRUE;
            
} // end static BOOL  SetTestOptions( LPTSTR lpszCmdLine )

