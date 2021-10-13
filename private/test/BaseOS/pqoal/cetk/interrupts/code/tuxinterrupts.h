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


#ifndef __TUX_INTERRUPTS_H__
#define __TUX_INTERRUPTS_H__

#include <windows.h>
#include <tux.h>


#include <nkintr.h>

#define OAL_INTR_DYNAMIC            (1 << 0)
#define OAL_INTR_STATIC             (1 << 1)
#define OAL_INTR_FORCE_STATIC       (1 << 2)
#define OAL_INTR_TRANSLATE          (1 << 3)
#define SPECIAL_OAL_INTR_STANDARD   (1 << 16)

#define MUST_FAIL (1 << 30)
#define IN_RANGE (1 << 31)

TESTPROCAPI
PrintUsage(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE);

/***************************************************************************
 *                                                                         *
 *                       General IOCTL Param Checks                        *
 *                                                                         *
 ***************************************************************************/

/* see file paramsToIoctls.cpp for these tests */

/* the test cases.  Set these in the dwUserData field of the function table */
enum tGeneralIoctlTestCase {
  /* IOCTL_HAL_REQUEST_SYSINTR */
  TESTCASE_BAD_IN_SIZE_NEW_MODEL,
  TESTCASE_BAD_INPUT_INCOMPLETE_ARGS_NEW_MODEL,
  TESTCASE_BAD_IN_SIZE_OLD_MODEL,
  TESTCASE_BAD_FLAGS,
  TESTCASE_BAD_OUT_SIZE_OLD_MODEL,
  TESTCASE_BAD_OUT_SIZE_NEW_MODEL,
  TESTCASE_GOOD_OUT_SIZE_OLD_MODEL,
  TESTCASE_GOOD_OUT_SIZE_NEW_MODEL,
  TESTCASE_BYTES_RETURNED_OLD_MODEL,
  TESTCASE_BYTES_RETURNED_NEW_MODEL,

  /* IOCTL_HAL_RELEASE_SYSINTR */
  IOCTL_HAL_RELEASE_SYSINTR_TESTCASE_BAD_IN_SIZE,

  /* IOCTL_HAL_REQUEST_IRQ */
  IOCTL_HAL_REQUEST_IRQ_TESTCASE_BAD_IN_SIZE,
  IOCTL_HAL_REQUEST_IRQ_TESTCASE_BAD_OUT_SIZE
};

/* test proc */
TESTPROCAPI
runGeneralIoctlTest(
            UINT uMsg,
            TPPARAM tpParam,
            LPFUNCTION_TABLE_ENTRY lpFTE);


/***************************************************************************
 *                                                                         *
 *                       IOCTL_HAL_REQUEST_SYSINTR                         *
 *                       IOCTL_HAL_RELEASE_SYSINTR                         *
 *                            Specific Tests                               *
 *                                                                         *
 ***************************************************************************/

/* see file requestReleaseSysintr.cpp for these tests */

TESTPROCAPI  freeWhatShouldNotBeFreed(
                      UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI  stressUnallocatedSysInters(
                    UINT uMsg, 
                    TPPARAM tpParam, 
                    LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI  testFindMaxIRQPerSysIntr(
                      UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI  testFindNumFreeSysIntrs(
                      UINT uMsg, 
                      TPPARAM tpParam, 
                      LPFUNCTION_TABLE_ENTRY lpFTE);


/********  Allocation Tests **********************************************/

TESTPROCAPI  testFindMaxSuccessfullyAllocatedIrq(
                         UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI  allocateAll_SingleIRQ(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI  allocateRand_MultIRQ(
                 UINT uMsg, 
                 TPPARAM tpParam, 
                 LPFUNCTION_TABLE_ENTRY lpFTE);

/***************************************************************************
 *                                                                         *
 *                         IOCTL_HAL_REQUEST_IRQ                           *
 *                                                                         *
 ***************************************************************************/

TESTPROCAPI  exerciseRequestIRQUserParams(
                                      UINT uMsg,
                                      TPPARAM tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI  exerciseRequestIRQRandom(
                                      UINT uMsg,
                                      TPPARAM tpParam,
                                      LPFUNCTION_TABLE_ENTRY lpFTE);


#endif
