//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __KTBVT_H__
#define __KTBVT_H__


// Test function prototypes (TestProc's)
TESTPROCAPI SMP_BVT_THREAD_1          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_BVT_PROCESS_1         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every
// test case within the entire team to be uniquely identified.

#define BASE                      1000

void LOG(LPWSTR szFmt, ...);

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("Kernel SMP Test"                              ), 0,   0, 0,      NULL,
   TEXT( "Thread Scheduling BVT"                       ), 1,   0, BASE,   SMP_BVT_THREAD_1,
   TEXT( "Process Scheduling BVT"                      ), 1,   0, BASE+1, SMP_BVT_PROCESS_1,
   NULL,                                                  0,   0, 0,      NULL  // marks end of list
};



#ifndef CHECK
    #define CHECK(s) if (!s) break;
#endif
#endif // __KTBVT_H__
