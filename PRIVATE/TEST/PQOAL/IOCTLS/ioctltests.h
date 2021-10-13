//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * ioctlTests.h
 */

#ifndef IOCTL_TESTS_H
#define IOCTL_TESTS_H

/*
 * Test functions for ioctls
 */

#include <windows.h>

#include "..\common\commonUtils.h"

/*
 * general test case.  Tests operation of the function (and a good set
 * of example code).
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working();

/*
 * confirm that no specifying lpBytesReturned doesn't change the
 * behavior.  We compare to the output returned when this parameter is
 * not null, so we assume that the test above is working.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working_NoLpBytesReturned();

/*
 * confirm that no specifying lpBytesReturned doesn't change the
 * behavior for a query.  We compare to the output returned when this
 * paramter is not null, so we assume that the test above is working.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working_CheckQuery();


/*
 * Force a failure by setting the nInBuf to a bad ptr.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_inBuf();

/*
 * Force a failure by setting the nInBufSize to != 0
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_inBufSize();

/*
 * Force a failure by setting the inbound parameters, just as someone
 * whom didn't understand the syntax would do.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_inBoundParams();

/* 
 * run a query, but send in a large enough buffer to make sure that we still
 * get query operation.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_QueryOnLargeBuffer();

/*
 * force failure by setting outBufSize to 0 
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize0();


/*
 * force failure by setting outBufSize to 0 
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize1();

/*
 * force failure by setting outBufSize to one less than is needed
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSizeOneLess();

/*
 * set the outsize to one greater than what is needed.  ioctl should
 * return data correctly.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working_outSizeOneGreater();

#endif /* IOCTL_TESTS_H */
