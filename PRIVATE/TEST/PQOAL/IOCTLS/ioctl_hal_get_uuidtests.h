//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * IOCTL_HAL_GET_UUIDtests.h
 */

#ifndef IOCTL_HAL_GET_UUID_TESTS
#define IOCTL_HAL_GET_UUID_TESTS

#include <windows.h>

/*
 * general test case.  Tests operation of the function
 */
BOOL
testIOCTL_HAL_GET_UUID_Working();

/*
 * general test case.  Tests operation of the function.  The optional
 * lpBytesReturned paramter to KernelIoControl is set to null.
 */
BOOL
testIOCTL_HAL_GET_UUID_Working_NoLpBytesReturned();

/*
 * Force a failure by setting the nInBuf to a bad ptr.
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_inBuf();

/*
 * Force a failure by setting the nInBufSize to != 0
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_inBufSize();

/*
 * Force a failure by setting the inbound parameters, just as someone
 * whom didn't understand the syntax would do.
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_inBoundParams();

/*
 * force failure by setting outBufSize to 0 
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize0();

/*
 * force failure by setting outBufSize to 1
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize1();

/*
 * force failure by setting outBufSize to one less than is needed
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSizeOneLess();

/*
 * force failure by setting outBuf to NULL.
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufIsNull();

/*
 * general test case.  Tests operation of the function (and a good set
 * of example code).
 */
BOOL
testIOCTL_HAL_GET_UUID_Working_outSizeOneGreater();

/*
 * makes sure that the function correctly writes into buffers that
 * aren't DWORD aligned.
 */
BOOL
testIOCTL_HAL_GET_UUID_Working_Alignment(DWORD dwAlignOffset);

#endif /* IOCTL_HAL_GET_UUID_TESTS */
