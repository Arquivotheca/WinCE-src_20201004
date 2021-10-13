//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++
 
 
Module Name:
 
    Pserial.h
 
Abstract:
 
    This file contains the headers for the tux tests.

 
--*/
#ifndef _TUXTEST_H
#define _TUXTEST_H
#include <windows.h>
#ifdef UNDER_NT
#include <stdio.h>
#endif
#include <tchar.h>
#include <tux.h>
#include "comm.h"
#include <katoex.h>


__inline int _CCprintf(const TCHAR *str, ...)
{
    static TCHAR buffer[2048];
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
#define BEGINSYNCS    40
#define ENDSYNCS    40

//Integrity Checks
#define DATA_INTEGRITY_FAIL   0x02
#define DATA_INTEGRITY_PASS  0x04



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

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
extern SPS_SHELL_INFO g_spsShellInfo;

// Test role
extern BOOL         g_fMaster;
extern BOOL            g_fDump;
extern TCHAR        g_lpszCommPort[];
extern BOOL         g_bCommDriver;
extern COMMPROP     g_CommProp;
extern BOOL         g_fSetCommProp;
extern BOOL            g_fBT;
extern CommPort *   g_hBTPort;
extern BOOL         g_fPerf;
extern BOOL         g_fStress;
extern DWORD        g_StressCount;
extern DWORD        g_PerfData;   
extern BOOL       g_fDisableFlow;

#define MAX_BT_CONN_ATTEMPTS    5
#define BT_CONN_ATTEMPT_TIMEOUT    1000


/* ------------------------------------------------------------------------
        Declarations for TUX Test Functions.
------------------------------------------------------------------------ */
TESTPROCAPI TempleteTest                ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NegotiateSerialProperties   ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestReadDataParityAndStop   ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventSignals        ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventBreak          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventChars          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestCommEventTxEmpty        ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestModemSignals            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestReadTimeouts            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestWriteTimeouts           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestPurgeCommRxTx           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestXonXoffIdle                ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestXonXoffReliable            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TestPorts                    ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DataXmitTest                ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DataReceiveTest             ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SerialStressTest            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SerialPerfTest              ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

/* ------------------------------------------------------------------------
    PSerial.cpp's functions
------------------------------------------------------------------------ */
BOOL SetupDefaultPort( CommPort * hCommPort );
BOOL BeginTestSync( CommPort * hCommPort, DWORD dwUniqueID );
DWORD EndTestSync( CommPort *hCommPort, DWORD dwUniqueID, DWORD dwMyResult );
DWORD WorseResult( DWORD dwResult1, DWORD dwResult2 );
BOOL DumpCommProp( const COMMPROP *pcp );
BOOL SelectiveConfig( CommPort * hCommPort);
VOID LogDataFormat( DCB *pDCB );
BOOL ClearTestCommErrors( CommPort * hCommPort );
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

#ifdef UNDER_CE
/* ------------------------------------------------------------------------
    Performance Items
------------------------------------------------------------------------ */
//-- PerfScenario function pointers --
enum PERFSCENARIO_STATUS {PERFSCEN_UNINITIALIZED, PERFSCEN_INITIALIZED, PERFSCEN_ABORT};
typedef HRESULT (*PFN_PERFSCEN_OPENSESSION)(LPCTSTR lpszSessionName, BOOL fStartRecordingNow);
typedef HRESULT (*PFN_PERFSCEN_CLOSESESSION)(LPCTSTR lpszSessionName);
typedef HRESULT (*PFN_PERFSCEN_ADDAUXDATA)(LPCTSTR lpszLabel, LPCTSTR lpszValue);
typedef HRESULT (*PFN_PERFSCEN_FLUSHMETRICS)(BOOL fCloseAllSessions, GUID* scenarioGuid, LPCTSTR lpszScenarioNamespace, LPCTSTR lpszScenarioName, LPCTSTR lpszLogFileName, LPCTSTR lpszTTrackerFileName, GUID* instanceGuid);
#endif 