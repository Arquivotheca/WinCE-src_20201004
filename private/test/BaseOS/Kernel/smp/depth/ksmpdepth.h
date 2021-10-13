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
TESTPROCAPI SMP_DEPTH_LOAD_BALANCE        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

TESTPROCAPI SMP_DEPTH_SYNC_1              (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_SYNC_STRESS         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

TESTPROCAPI SMP_DEPTH_EXCEPTION_1         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_EXCEPTION_2         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


TESTPROCAPI SMP_DEPTH_CACHE_FLUSH         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_CACHE_FLUSH_SIMPLE         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


TESTPROCAPI SMP_DEPTH_KDATA_1         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_KDATA_2         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_KDATA_3         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);



TESTPROCAPI SMP_DEPTH_MATRIX_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_MATRIX_2      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);



TESTPROCAPI SMP_DEPTH_AFFINITY_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_AFFINITY_2      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


TESTPROCAPI SMP_DEPTH_STRESS_THREAD_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_STRESS_PROCESS_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

TESTPROCAPI SMP_DEPTH_CS_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_CS_2      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


TESTPROCAPI SMP_DEPTH_SCHL_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_SCHL_2      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_SCHL_3      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

TESTPROCAPI SMP_DEPTH_SYNC_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

TESTPROCAPI SMP_DEPTH_BC_1      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);


TESTPROCAPI SMP_DEPTH_POWER_1    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_POWER_2    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SMP_DEPTH_POWER_3    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);



#define CS1_DIFFERENT_PROC_FLAG          (1<<1)
#define CS1_NUM_OF_NORMAL_PRI_THD_SHIFT_MASK   (16)


#define CS1_SET_PARAM(fDifferentProc, dwNumOfNorPriThds) \
    (fDifferentProc) ?     \
    ((dwNumOfNorPriThds << CS1_NUM_OF_NORMAL_PRI_THD_SHIFT_MASK) | CS1_DIFFERENT_PROC_FLAG): \
    (dwNumOfNorPriThds << CS1_NUM_OF_NORMAL_PRI_THD_SHIFT_MASK)

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every
// test case within the entire team to be uniquely identified.

#define BASE                      2000

void LOG(LPWSTR szFmt, ...);

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("Kernel SMP Test"                              ), 0,   0,      0, NULL,
   TEXT( "Load Balancing"        ), 1,   0, BASE,   SMP_DEPTH_LOAD_BALANCE,
   TEXT( "Sync Object - Signaling across processor"        ), 1,   0, BASE+1,   SMP_DEPTH_SYNC_1,
   TEXT( "Sync Object - Stress"        ), 1,   0, BASE+2,   SMP_DEPTH_SYNC_STRESS,
   
   //CacheRangeFlush()
   TEXT( "Multi-thread CacheRangeFlush"        ), 1,   0, BASE+3,   SMP_DEPTH_CACHE_FLUSH,
   TEXT( "CacheRangeFlush - Simple"        ), 1,   0, BASE+4,   SMP_DEPTH_CACHE_FLUSH_SIMPLE,

   //Affinity
   TEXT( "Affinity test 1"        ), 1,   0, BASE+5,   SMP_DEPTH_AFFINITY_1,
   TEXT( "Affinity test 2"        ), 1,   0, BASE+6,   SMP_DEPTH_AFFINITY_2,

      //Exception
   TEXT( "Exception => Change Affinity"        ), 1,   0, BASE+10,   SMP_DEPTH_EXCEPTION_1,
   TEXT( "Exception => High priority spinning thread"        ), 1,   0, BASE+11,   SMP_DEPTH_EXCEPTION_2,

   //Kdata
   TEXT( "Process affinity and KData"        ), 1,   0, BASE+20,   SMP_DEPTH_KDATA_1,
   TEXT( "Number of processor and KData"        ), 1,   0, BASE+21,   SMP_DEPTH_KDATA_2,
   TEXT( "Thread affinity and KData"        ), 1,   0, BASE+22,   SMP_DEPTH_KDATA_3,

   TEXT( "Scheduler Test #1"        ), 1,   0, BASE+30,   SMP_DEPTH_SCHL_1,
   TEXT( "Scheduler Test #2"        ), 1,   0, BASE+31,   SMP_DEPTH_SCHL_2,
   TEXT( "Scheduler Test #3"        ), 1,   0, BASE+32,   SMP_DEPTH_SCHL_3,


   TEXT( "Creating Max number of threads"        ), 1,   0, BASE+50,   SMP_DEPTH_STRESS_THREAD_1,
   TEXT( "Creating Max number of processes"        ), 1,   0, BASE+51,   SMP_DEPTH_STRESS_PROCESS_1,

   TEXT( "Critical Section Test #1 - Priority donation"        ), 1,   CS1_SET_PARAM(FALSE,0), BASE+60,   SMP_DEPTH_CS_1,
   TEXT( "Critical Section Test #1 - Priority donation between CPUs"        ), 1,   CS1_SET_PARAM(TRUE,0), BASE+61,   SMP_DEPTH_CS_1,
   TEXT( "Critical Section Test #1 - Priority donation with multiple Normal Pri threads"), 1,   CS1_SET_PARAM(FALSE, 4), BASE+62,   SMP_DEPTH_CS_1,
   TEXT( "Critical Section Test #2"        ), 1,   0, BASE+63,   SMP_DEPTH_CS_2,


   TEXT( "Matrix Multiply"        ),                1,   0, BASE+100,   SMP_DEPTH_MATRIX_1,
   TEXT( "Matrix Multiply - Load Balance"        ), 1,   0, BASE+101,   SMP_DEPTH_MATRIX_2,
   
   TEXT( "Power Test #1"        ),                1,   0, BASE+110,   SMP_DEPTH_POWER_1,
   TEXT( "Power Test #2"        ),                1,   0, BASE+111,   SMP_DEPTH_POWER_2,
   TEXT( "Power Test #3"        ),                1,   0, BASE+112,   SMP_DEPTH_POWER_3,

   //Backward Compatibility
   TEXT( "Backward Compatibility #1 - CE 5 process"        ),                1,   0, BASE+200,   SMP_DEPTH_BC_1,
   TEXT( "Backward Compatibility #1 - CE 6 process"        ),                1,   1, BASE+201,   SMP_DEPTH_BC_1,
   TEXT( "Backward Compatibility #1 - CE 7 process"        ),                1,   2, BASE+202,   SMP_DEPTH_BC_1,
   TEXT( "Backward Compatibility #2 - CE 5 process"        ),                1,   3, BASE+203,   SMP_DEPTH_BC_1,
   TEXT( "Backward Compatibility #2 - CE 6 process"        ),                1,   4, BASE+204,   SMP_DEPTH_BC_1,

   NULL,                                   0,   0,  0, NULL  // marks end of list
};



#ifndef CHECK
    #define CHECK(s) if (!s) break;
#endif
#endif // __KTBVT_H__
