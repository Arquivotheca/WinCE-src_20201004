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

#ifndef __DBTST_H__
#define __DBTST_H__
#include <tux.h>
#include <katoex.h>
CKato                    *g_pKato = NULL;

// kato logging macros
#define FAIL(x)            g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)        g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define WARN(x)            g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )


#define DETAIL(x, ...)   g_pKato->Log( LOG_DETAIL, TEXT(x),__VA_ARGS__ )

#define CHECKTRUE(s) if (!(s)) { \
                        ERRFAIL("CHECKTRUE"); \
                        goto EXIT; \
                     }
#define CHECKFALSE(s) if (s) {    \
                        ERRFAIL("CHECKFALSE"); \
                        goto EXIT; \
                      }


// Test function prototypes (TestProc's)
TESTPROCAPI TestCreateDeleteOpenCloseDB(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestFindDB(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TestRWSeekDelRecordsFromEmptyDB(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every
// test case within the entire team to be uniquely identified.

#define BASE 0x00000000

// Our function table that we pass to Tux
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("Database BVT"                           ), 0,   0,              0, NULL,
    TEXT( "Create/Delete/Open/CloseDB"         ), 1,   0,      BASE+      1, TestCreateDeleteOpenCloseDB,
    TEXT( "FindDB"                             ), 1,   0,      BASE+      2, TestFindDB,
    TEXT( "RWSeekDelRecordsFromEmptyDB"    ), 1,   0,      BASE+      3, TestRWSeekDelRecordsFromEmptyDB,
    NULL                                 , 0,    0,               0, NULL  // marks end of list
};

#define    DB_DBASE_TYPE    555

#endif // __DBTST_H__
