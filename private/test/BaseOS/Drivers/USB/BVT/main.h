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
////////////////////////////////////////////////////////////////////////////////
//
// Module: main.h
//         Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_H__
#define __MAIN_H__

//******************************************************************************
//***** Included files
//****************************************************************************** 

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>


//******************************************************************************
//***** Debug Zones
//******************************************************************************
#ifdef  DEBUG
#define DBG_ALWAYS              0x0000
#define DBG_INIT                0x0001
#define DBG_DEINIT              0x0002
#define DBG_OPEN                0x0004
#define DBG_CLOSE               0x0008
#define DBG_BANDWIDTH           0x0010
#define DBG_HUB                 0x0020
#define DBG_DPC                 0x0040
#define DBG_PCI                 0x0080
#define DBG_TEST                0x0100
#define DBG_IOCTL               0x0200
#define DBG_DEVICE_INFO         0x0400
#define DBG_CRITSECT            0x0800
#define DBG_ALLOC               0x1000
#define DBG_FUNCTION            0x2000
#define DBG_WARNING             0x4000
#define DBG_ERROR               0x8000

#endif //debug
//******************************************************************************
//***** Suggested log verbosities
//****************************************************************************** 

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_WARNING            3
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14


//******************************************************************************
//***** Kato logging, global handle and short access functions
//******************************************************************************

#ifdef  __cplusplus
extern "C" {
#endif

extern  HKATO   g_hKato;
BOOL    WINAPI  LoadGlobalKato      ( void );
BOOL    WINAPI  CheckKatoInit        ( void );
void    WINAPI  Log                 ( LPTSTR szFormat, ... );
void    WINAPI  LWarn               ( LPTSTR szFormat, ... );
void    WINAPI  LCondVerbose        ( BOOL fVerbose, LPTSTR szFormat, ...);
void    WINAPI  LogVerboseV         (BOOL fVerbose, LPTSTR szFormat, va_list pArgs) ;
void    WINAPI  Fail                ( LPTSTR szFormat, ... );
void    WINAPI  ClearFailWarnCount  (void);
void    WINAPI  ClearFailCount  (void);

DWORD    WINAPI    GetFailCount        (void);
DWORD    WINAPI    GetWarnCount        (void);

#ifdef  __cplusplus
}
#endif //cplusplus
void    WINAPI      Log(LPTSTR szFormat, ...);

// kato logging macros
#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define ABORT(x)     g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x)  g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s;" ), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as WARN, but also logs GetLastError value
#define ERRWARN(x)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

// log's an error, but doesn't log a failure
#define ERR(x)      g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL,(x) )

#define COMMENT(x)  g_pKato->Log( LOG_COMMENT, TEXT(x) )

#define SKIP(x)     g_pKato->Log( LOG_SKIP, TEXT(x) )

#define UNIMPL(x)   g_pKato->Log( LOG_NOT_IMPLEMENTED, TEXT(x) )

//******************************************************************************
//***** String Definitions
//******************************************************************************

#define IDS_TEST_USBD_LOAD                        "USBD Load.\r\n"
#define IDS_TEST_USBD_UNLOAD                    "USBD UnLoad.\r\n"
#define IDS_TEST_OPEN_REGISTRY                    "Opening Registry.\r\n"
#define IDS_TEST_QUERY_REGISTRY                    "Querying Registry.\r\n"
#define IDS_TEST_CLOSE_REGISTRY                    "Closing Registry.\r\n"

#define IDS_TEST_READ_REGISTRY_PATH                "Reading Registry Path.\r\n"    
#define IDS_TEST_READ_REGISTRY_PATH_STR            "Reading Registry Path %s.\r\n"    
#define IDS_TEST_REGISTER_CLIENT_SETTINGS        "Registering Client Settings for %s.\r\n"
#define IDS_TEST_UNREGISTER_CLIENT_SETTINGS        "Unregistering Client Settings for %s.\r\n"
#define IDS_TEST_REGISTER_CLIENT                "Registering Client %s.\r\n"
#define IDS_TEST_UNREGISTER_CLIENT                "Unregistering Client %s.\r\n"

#define IDS_TEST_GET_USBD_VERSION                "Get USBD Version.\r\n"
#define IDS_TEST_GET_USBD_VERSION_STR            "Get USBD Version.Major version: %d, Minor Version: %d.\r\n"
#define IDS_TEST_VERIFY_REGISTRY                "Verify Registry Values.\r\n"

#define IDS_READ_REGISTRY_VALUE                    "Read \"%s\" string from registry \"%s\""
#define IDS_MATCH_REGISTRY_VALUE                "Match \"%s\" string with \"%s\""
#define IDS_SUSPEND_RESUME_UNSUPPORTED            "Suspend Resume Not supported.\r\n"
#define IDS_SUSPEND_RESUME                        "Successfully Completed Suspend Resume.\r\n"
#define IDS_SKIP_SUSPEND_RESUME_VERSION            "Skipping test, due to problem in suspend resume or getting version information.\r\n"
#define IDS_SHOULD_PASS_REGISTERED_CLIENT        "Should have passed above check since client is registered.\r\n"
#define IDS_SKIP_SUSPEND_RESUME_REGISTERED_CLIENT    "Skipping test, due to problem in suspend resume or registering client.\r\n"
#define IDS_SHOULD_FAIL_UNREGISTERED_CLIENT        "Should have failed above check since client is unregistered.\r\n"
#define IDS_SKIP_SUSPEND_RESUME_UNREGISTERED_CLIENT    "Skipping test, due to problem in suspend resume or unregistering client.\r\n"

#endif // __MAIN_H__
