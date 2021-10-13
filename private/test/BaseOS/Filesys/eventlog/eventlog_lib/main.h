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
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  FSACCESS TUX DLL
//
//  Module: main.h
//          Header for all files in the project.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_H__
#define __MAIN_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <clparse.h>

////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

////////////////////////////////////////////////////////////////////////////////

// kato logging macros
#define FAIL(x)     if(g_pKato != NULL){g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) );}

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x, y)  if(g_pKato!=NULL){g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), y );}

#define ABORT(x)     if(g_pKato!=NULL){g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) );}

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x)  if(g_pKato!=NULL){g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() );}

#define WARN(x)     if(g_pKato!=NULL){g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) );}

// same as WARN, but also logs GetLastError value
#define ERRWARN(x)  if(g_pKato!=NULL){g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() );}

// log's an error, but doesn't log a failure
#define ERR(x)      if(g_pKato!=NULL){g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) );}

#define DETAIL(x)   if(g_pKato!=NULL){g_pKato->Log( LOG_DETAIL, TEXT(x) );}

#define COMMENT(x)  if(g_pKato!=NULL){g_pKato->Log( LOG_COMMENT, TEXT(x) );}

#define SKIP(x)     if(g_pKato!=NULL){g_pKato->Log( LOG_SKIP, TEXT(x) );}

#define UNIMPL(x)   if(g_pKato!=NULL){g_pKato->Log( LOG_NOT_IMPLEMENTED, TEXT(x) );}

#define PASS(x)   if(g_pKato!=NULL){g_pKato->Log( LOG_PASS, TEXT(x) );}

#define VERIFY_RETURN                   if(bRetVal != TRUE){FAIL("A function failed, failing whole test");goto done;}
#define RETURN_TPR                      if(bRetVal != TRUE){return TPR_FAIL;}return TPR_PASS;
#define VERIFY_RETURN_NOBAIL(x)         if(!x){FAIL("Cleanup function failed");bRetVal = FALSE;}

TESTPROCAPI GetTestResult(void);

void LOG(LPWSTR szFmt, ...);

//////////////////////////////////////////////////////////////

#define OUTPUT_TEST_RESULT(x)   if(x == FALSE){g_pKato->Log( LOG_FAIL, "TEST_RESULT: FAIL: One or more parts of this test failed, check log for details");return TPR_FAIL;}g_pKato->Log(LOG_PASS, TEXT("TEST_RESULT: PASS")); return TPR_PASS;

#endif // __MAIN_H__
