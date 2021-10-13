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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
 * tuxFunctionTable.cpp
 */

/*
 * pull in the function defs for the cache tests
 */
#include "..\code\tuxInterrupts.h"

/* easily add end of line characters */
#define EOL _T("\r\n")

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

  _T("Usage"), 0, 0, 1, PrintUsage,

  _T("IOCTL_HAL_REQUEST_SYSINTR / IOCTL_HAL_RELEASE_SYSINTR" EOL), 0, 0, 0, NULL,

  _T("Kernel Mode and User Mode") EOL, 1, 0, 0, NULL,

  _T("Incorrect Parameter Checks") EOL, 2, 0, 0, NULL,

  _T("IOCTL_HAL_REQUEST_SYSINTR") EOL, 3, 0, 0, NULL,

  _T("Bad Input New Model - Incomplete Args"), 4, TESTCASE_BAD_INPUT_INCOMPLETE_ARGS_NEW_MODEL, 500, runGeneralIoctlTest,
  _T("Bad Input New Model - Bad Input Sizes"), 4, TESTCASE_BAD_IN_SIZE_NEW_MODEL, 510, runGeneralIoctlTest,
  _T("Bad Input-Size Old Model"), 4, TESTCASE_BAD_IN_SIZE_OLD_MODEL, 520, runGeneralIoctlTest,
  _T("Bad Flags"), 4, TESTCASE_BAD_FLAGS, 530, runGeneralIoctlTest,
  _T("Bad Out-Size New Model"), 4, TESTCASE_BAD_OUT_SIZE_NEW_MODEL, 540, runGeneralIoctlTest,
  _T("Bad Out-Size Old Model"), 4, TESTCASE_BAD_OUT_SIZE_OLD_MODEL, 550, runGeneralIoctlTest,

  _T("IOCTL_HAL_RELEASE_SYSINTR") EOL, 3, 0, 0, NULL,

  _T("Bad Input Size"), 3, IOCTL_HAL_RELEASE_SYSINTR_TESTCASE_BAD_IN_SIZE, 600, runGeneralIoctlTest,

  _T("SYSINTR focused testing") EOL, 2, 0, 0, NULL,

  _T("Free SYSINTRs that Shouldn't be Freed"),  3, 0, 1000, freeWhatShouldNotBeFreed,

  _T("Kernel Mode") EOL, 1, 0, 0, NULL,

  _T("Parameter Checks") EOL, 2, 0, 0, NULL,

  _T("Good Out-Size New Model"), 4, TESTCASE_GOOD_OUT_SIZE_NEW_MODEL, 1110, runGeneralIoctlTest,
  _T("Good Out-Size Old Model"), 4, TESTCASE_GOOD_OUT_SIZE_OLD_MODEL, 1120, runGeneralIoctlTest,
  _T("lpBytesReturned New Model"), 4, TESTCASE_BYTES_RETURNED_NEW_MODEL, 1130, runGeneralIoctlTest,
  _T("lpBytesReturned Old Model"), 4, TESTCASE_BYTES_RETURNED_OLD_MODEL, 1140, runGeneralIoctlTest,

  _T("SYSINTR focused testing") EOL, 2, 0, 0, NULL,

  _T("Stress Unallocated SYSINTRs"),  3, 0, 1200, stressUnallocatedSysInters,
  _T("Find Number of Free SYSINTRS"),  3, 0, 1300, testFindNumFreeSysIntrs,
  _T("Find Max IRQ per SYSINTR"),  3, 0, 1400, testFindMaxIRQPerSysIntr,

  _T("Single IRQ Allocations") EOL, 2, 0, 0, NULL,

  _T("Single IRQ Standard (Old) Alloc"),  3, SPECIAL_OAL_INTR_STANDARD, 2000, allocateAll_SingleIRQ,
  _T("Single IRQ static Alloc"),  3, OAL_INTR_STATIC, 2100,  allocateAll_SingleIRQ,
  _T("Single IRQ dynamic Alloc"),  3, OAL_INTR_DYNAMIC, 2200,  allocateAll_SingleIRQ,
  /* we include this for completeness.  Not really needed. */
  _T("Find Max Successfully Allocated IRQ" EOL),  3, OAL_INTR_DYNAMIC, 2300, testFindMaxSuccessfullyAllocatedIrq,

  _T("Single IRQ Standard (Old) Alloc - In Range"),  3, SPECIAL_OAL_INTR_STANDARD | IN_RANGE, 3000, allocateAll_SingleIRQ,
  _T("Single IRQ Static Alloc - In Range"),  3, OAL_INTR_STATIC | IN_RANGE, 3100,  allocateAll_SingleIRQ,
  _T("Single IRQ Dynamic Alloc - In Range"),  3, OAL_INTR_DYNAMIC | IN_RANGE, 3200,  allocateAll_SingleIRQ,

  _T("Find Max Successfully Allocated IRQ - In Range"),  3, OAL_INTR_DYNAMIC | IN_RANGE, 3300, testFindMaxSuccessfullyAllocatedIrq,

  _T("Multiple IRQ Allocations" EOL), 2, 0, 0, NULL,

  _T("Mult IRQ Static Alloc "),  3, OAL_INTR_STATIC, 4000, allocateRand_MultIRQ,
  _T("Mult IRQ Dynamic Alloc"),  3, OAL_INTR_DYNAMIC, 4100, allocateRand_MultIRQ,
  _T("Mult IRQ Force-Static Alloc") EOL,  3, OAL_INTR_FORCE_STATIC, 4200, allocateRand_MultIRQ,

  _T("Mult IRQ Static Alloc - In Range"),  3, OAL_INTR_STATIC | IN_RANGE, 5000, allocateRand_MultIRQ,
  _T("Mult IRQ Dynamic Alloc - In Range"),  3, OAL_INTR_DYNAMIC | IN_RANGE, 5100, allocateRand_MultIRQ,
  _T("Mult IRQ Force-Static Alloc - In Range"),  3, OAL_INTR_FORCE_STATIC | IN_RANGE, 5200, allocateRand_MultIRQ,

  _T("User Mode") EOL, 1, 0, 0, NULL,

  _T("Single IRQ Allocations") EOL, 2, 0, 0, NULL,

  _T("Single IRQ Standard (Old) Alloc"),  3, SPECIAL_OAL_INTR_STANDARD | MUST_FAIL, 12000, allocateAll_SingleIRQ,
  _T("Single IRQ static Alloc"),  3, OAL_INTR_STATIC | MUST_FAIL, 12100,  allocateAll_SingleIRQ,
  _T("Single IRQ dynamic Alloc") EOL,  3, OAL_INTR_DYNAMIC | MUST_FAIL, 12200,  allocateAll_SingleIRQ,

  _T("Multiple IRQ Allocations" EOL), 2, 0, 0, NULL,

  _T("Mult IRQ Static Alloc "),  3, OAL_INTR_STATIC | MUST_FAIL, 14000, allocateRand_MultIRQ,
  _T("Mult IRQ Dynamic Alloc"),  3, OAL_INTR_DYNAMIC | MUST_FAIL, 14100, allocateRand_MultIRQ,
  _T("Mult IRQ Force-Static Alloc") EOL,  3, OAL_INTR_FORCE_STATIC | MUST_FAIL, 14200, allocateRand_MultIRQ,

  _T("IOCTL_HAL_REQUEST_IRQ" EOL), 0, 0, 0, NULL,

  _T("Incorrect Parameter Checks") EOL, 1, 0, 0, NULL,

  _T("Bad Input Size"), 2, IOCTL_HAL_REQUEST_IRQ_TESTCASE_BAD_IN_SIZE, 20010, runGeneralIoctlTest,
  _T("Bad Output Size"), 2, IOCTL_HAL_REQUEST_IRQ_TESTCASE_BAD_OUT_SIZE, 20020, runGeneralIoctlTest,

  _T("Kernel Mode") EOL, 1, 0, 0, NULL,

  _T("Random Request"),  2, 0, 20100, exerciseRequestIRQRandom,
  _T("User Specified Params"),  2, 0, 20200, exerciseRequestIRQUserParams,

  _T("User Mode" EOL), 1, 0, 0, NULL,

  _T("Random Request"),  2, MUST_FAIL, 30100, exerciseRequestIRQRandom,
  _T("User Specified Params"),  2, MUST_FAIL, 30200, exerciseRequestIRQUserParams,

   NULL, 0, 0, 0, NULL  // marks end of list
};


