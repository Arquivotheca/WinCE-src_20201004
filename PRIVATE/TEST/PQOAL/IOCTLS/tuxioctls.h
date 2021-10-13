//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * tuxIoctls.h
 */

#ifndef TUX_IOCTLS_H
#define TUX_IOCTLS_H

#include <windows.h>
#include <tux.h>

/* 
 * if you add tests add a entry to this enum and then add the enum to
 * the switch statement in the TESTPROC.  The enums are generally the
 * name of the main test function.
 */
enum eIoctlTestID {
  idIOCTL_HAL_GET_DEVICEID_Working,
  idIOCTL_HAL_GET_DEVICEID_Working_NoLpBytesReturned,
  idIOCTL_HAL_GET_DEVICEID_Working_CheckQuery,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_inBuf,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_inBufSize,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_inBoundParams,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_QueryOnLargeBuffer,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize0,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize1,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSizeOneLess,
  idIOCTL_HAL_GET_DEVICEID_Working_outSizeOneGreater,
  idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufIsNull,

  idIOCTL_HAL_GET_UUID_Working,
  idIOCTL_HAL_GET_UUID_Working_NoLpBytesReturned,
  idIOCTL_HAL_GET_UUID_ForceFailure_inBuf,
  idIOCTL_HAL_GET_UUID_ForceFailure_inBufSize,
  idIOCTL_HAL_GET_UUID_ForceFailure_inBoundParams,
  idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize0,
  idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize1,
  idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSizeOneLess,
  idIOCTL_HAL_GET_UUID_Working_outSizeOneGreater,
  idIOCTL_HAL_GET_UUID_ForceFailure_OutBufIsNull,
  idIOCTL_HAL_GET_UUID_Working_Alignment8thBit,
  idIOCTL_HAL_GET_UUID_Working_Alignment16thBit,
  idIOCTL_HAL_GET_UUID_Working_Alignment24thBit,

};

/*
 * main entry for the ioctl tests.  The test that we want to run is passed through
 * the lpFTE paramter.
 */
TESTPROCAPI ioctlTestEntry(
					     UINT uMsg, 
					     TPPARAM tpParam, 
					     LPFUNCTION_TABLE_ENTRY lpFTE);

#endif /* TUX_IOCTLS_H */
