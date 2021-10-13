// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#ifndef __TUXMAIN_H__
#define __TUXMAIN_H__

#define MODULE_NAME     _T("SERDRVBVT")

#include <WINDOWS.H>
#include <TCHAR.H>
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

void LOG(LPWSTR szFmt, ...);

// externs
extern CKato            *g_pKato;

#endif // __TUXMAIN_H__
