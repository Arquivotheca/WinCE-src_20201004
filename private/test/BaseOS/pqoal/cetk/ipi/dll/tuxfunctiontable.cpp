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


/*
 * pull in function defs for the IPI tests
 */

#include "..\code\tuxIPI.h"


/***** Base to start the tests at *******/
#define IPI_BASE 1000
#define IPI_USER_BASE 2000
#define IPI_KERNEL_BASE 3000



FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("Tests for Inter-Processor Interrupt"), 0, 0, 0, NULL,
  _T("IPI - Usage"), 0, 1, IPI_BASE, IPIUsage, 
  _T("IPI - Suspend Thread"), 0, 1, IPI_USER_BASE, IPISuspendThread,
  _T("IPI - Terminate Thread"), 0, 1, IPI_USER_BASE+1, IPITerminateThread,
  _T("IPI - Cache Flush (User Address)"), 0, 1, IPI_USER_BASE+2, IPIUserAddrCacheFlush,
  _T("IPI - Query Performance Counter"), 0, 1, IPI_USER_BASE+3, IPIQueryPerformanceCounter,
  _T("IPI - Query Performance Counter (Multi-threaded)"), 0, 1, IPI_USER_BASE+4, IPIQueryPerformanceCounterMT,
  _T("IPI - Power off Cores #1"), 0, 1, IPI_KERNEL_BASE, IPIPowerOffOnCores1,
  _T("IPI - Power off Cores #2"), 0, 1, IPI_KERNEL_BASE+1, IPIPowerOffOnCores2,
  _T("IPI - Sending IPI to specific CPU"), 0, 1, IPI_KERNEL_BASE+2, IPISendIPISpecificCPU,
  _T("IPI - Sending IPI to every other CPU"), 0, 1, IPI_KERNEL_BASE+3, IPISendIPIAllButSelf,
    
   NULL, 0, 0, 0, NULL  // marks end of list
};


