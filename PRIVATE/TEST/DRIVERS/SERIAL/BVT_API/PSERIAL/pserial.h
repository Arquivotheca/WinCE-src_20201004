/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     pserial.h  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	Pserial.h
 
Abstract:
 
	This file contains the headers for the tux tests.
 

 
	Uknown (unknown)
 
Notes:
 
--*/
#ifndef _TUXTEST_H
#define _TUXTEST_H
#include <windows.h>
#ifdef UNDER_NT
#include <stdio.h>
#endif
#include <tchar.h>
#include <tux.h>

#include <katoex.h>
//#include <netqalog.h>


__inline int _CCprintf(const TCHAR *str, ...)
{
    TCHAR buffer[2048];
    va_list ArgList;

    va_start(ArgList, str);
    wvsprintf(buffer, str, ArgList);
#ifdef UNDER_NT
	_tprintf(TEXT("%s"), buffer);
#else
    RETAILMSG(1, (TEXT("%s"), buffer));
#endif
    va_end(ArgList);
    return (0);
}


/* ------------------------------------------------------------------------
    Global Values
------------------------------------------------------------------------ */

// These constants define the number of times to attempt syncronization at the
// beginning and end of a test
#define BEGINSYNCS	40
#define ENDSYNCS	40


#ifdef PEG
#define DEFCOMMPORT    TEXT("COM1:")
#elif UNDER_CE
#define DEFCOMMPORT    TEXT("COM1:")
#else
#define DEFCOMMPORT    TEXT("COM1")
#endif

/* ------------------------------------------------------------------------
	Global Variables
------------------------------------------------------------------------ */

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
extern CKato *g_pKato;
//extern CNetQaLog *g_pKato;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
extern SPS_SHELL_INFO g_spsShellInfo;

// Test role
extern BOOL         g_fMaster;
extern BOOL			g_fDump;
extern TCHAR        g_lpszCommPort[];
extern COMMPROP     g_CommProp;
extern BOOL         g_fSetCommProp;
extern BOOL			g_fBT;
extern HANDLE		g_hBTPort;

#define MAX_BT_CONN_ATTEMPTS	5
#define BT_CONN_ATTEMPT_TIMEOUT	1000


/* ------------------------------------------------------------------------
        Declarations for TUX Test Functions.
------------------------------------------------------------------------ */
TESTPROCAPI TempleteTest                ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NegotiateSerialProperties   ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestReadDataParityAndStop   ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventSignals        ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventBreak          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventChars          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventTxEmpty		( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestModemSignals            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestReadTimeouts            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestWriteTimeouts           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestPurgeCommRxTx           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestXonXoffIdle		        ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestXonXoffReliable			( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestPorts					( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DataSpeedTest				( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DataXmitTest                ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DataReceiveTest             ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

/* ------------------------------------------------------------------------
	PSerial.cpp's functions
------------------------------------------------------------------------ */
BOOL SetupDefaultPort( HANDLE hCommPort );
BOOL BeginTestSync( HANDLE hCommPort, DWORD dwUniqueID );
DWORD EndTestSync( HANDLE hCommPort, DWORD dwUniqueID, DWORD dwMyResult );
DWORD WorseResult( DWORD dwResult1, DWORD dwResult2 );
BOOL DumpCommProp( const COMMPROP *pcp );
VOID LogDataFormat( DCB *pDCB );
BOOL ClearTestCommErrors( HANDLE hCommPort );
void ShowCommError( DWORD dwErrors );

/* ------------------------------------------------------------------------
    BTReg.cpp's functions
------------------------------------------------------------------------ */

BOOL RegisterBTDevice(BOOL fMaster, WCHAR* wszBTAddr, int port, int channel);
void UnregisterBTDevice();

/* ------------------------------------------------------------------------
	Defines used to provide better logging support
------------------------------------------------------------------------ */
#define LOG_WARNING     3

/* ------------------------------------------------------------------------
	Standard Error Macros
------------------------------------------------------------------------ */
#define FUNCTION_ERROR( T, C ); \
    if( T ) { \
        g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d, %s "), \
                      __THIS_FILE__, __LINE__, GetLastError(), TEXT( #C ) ); \
            C; \
        }

#define COMM_ERROR( P, T, C ); \
    if( T ) { \
        g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d"), \
                      __THIS_FILE__, __LINE__, GetLastError() ); \
        if( FALSE == ClearTestCommErrors( P ) ) C; \
    }


#define DEFAULT_ERROR( TST, ACT );  \
    if( TST ) { \
        g_pKato->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d: CONDITION( %s ) ACTION( %s )" ), \
                      __THIS_FILE__, __LINE__, TEXT( #TST ), TEXT( #ACT ) ); \
        ACT; \
    }

#define LOGLINENUM() g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d:" ), __THIS_FILE__, __LINE__ );


#define CMDLINENUM( CMD ) \
    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: %s" ), \
                  __THIS_FILE__, __LINE__, TEXT( #CMD ) );  \
    CMD;                  

/* ------------------------------------------------------------------------
	Handy Macros
------------------------------------------------------------------------ */
#ifdef UNDER_CE
#ifndef ZeroMemory
#define ZeroMemory(x,y) memset(x,0,y)
#endif
#endif

// TEST MACRO
#define TESTME( ) \
    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: I AM A MACRO!" ),  __THIS_FILE__, __LINE__ )

#define FCNMACROTEST( CMD ) _CCprintf(L"%s", TEXT(#CMD))

#endif _TUXTEST_H


