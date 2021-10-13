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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#pragma once

#define MODULE_NAME     _T("SERDRVBVT")

#include <WINDOWS.H>
#include <TCHAR.H>
#include <winreg.h>
#include <pm.h>
#include <KATOEX.H>
#include <TUX.H>

// --------------------------------------------------------------------
// Windows CE specific code

#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

#endif // UNDER_CE

// NT ASSERT macro
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#define HFILE FILE*
#endif



#define REG_HKEY_ACTIVE     TEXT("Drivers\\Active")
#define USBFN_SERIAL_CLASS  TEXT("Drivers\\USB\\FunctionDrivers\\Serial_Class")
#define IR_COMM_PORT_CLASS  TEXT("Drivers\\BuiltIn\\IrCOMM")
#define IR_RAW_PORT_CLASS   TEXT("Comm\\Irsir1\\Params")

//COMx is 4 Byte field 
#define COM_PORT_SIZE 5

#define COM0 TEXT("COM0:")
#define COM1 TEXT("COM1:")
#define COM2 TEXT("COM2:")
#define COM3 TEXT("COM3:")
#define COM4 TEXT("COM4:")
#define COM5 TEXT("COM5:")
#define COM6 TEXT("COM6:")
#define COM7 TEXT("COM7:")
#define COM8 TEXT("COM8:")
#define COM9 TEXT("COM9:")



//Number Base 
#define BVT_BASE       10
#define ERROR_BASE     100
#define STRESS_BASE    1000
#define OTHER_BASE     10000

/*------------------------------------------------------------------------
    Some constances
------------------------------------------------------------------------ */
static const  TCHAR CmdFlag         = (TCHAR)'-';
static const  TCHAR CmdPort         = (TCHAR)'p';
static const  TCHAR CmdUsb          = (TCHAR)'u';
static const  TCHAR CmdIR           = (TCHAR)'i';
static const  TCHAR CmdSpace        = (TCHAR)' ';


//
// global macros
//
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

// kato logging macros
#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define SUCCESS(x)  g_pKato->Log( LOG_PASS, \
                        TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, TEXT(x) )

// same as ERRFAIL, but doesn't log it as a failure
#define ERR(x)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

void LOG(LPCWSTR szFmt, ...);

// externs
extern CKato            *g_pKato;
extern INT                     COMx;
extern INT                irCOMx;
extern INT               COMMask[10];



