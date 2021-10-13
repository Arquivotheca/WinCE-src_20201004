//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#ifndef __TUXDEMO_H__
#define __TUXDEMO_H__


#include "tuxIoctls.h"

/**************************************************************************
 * 
 *                           Timer Test Function Prototypes
 *
 **************************************************************************/

/* these, when including the number of tests for each set, cannot overlap */
#define DEVICEID 500
#define UUID_BASE 600

TESTPROCAPI entry(
					     UINT uMsg, 
					     TPPARAM tpParam, 
					     LPFUNCTION_TABLE_ENTRY lpFTE);

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

   TEXT("IOCTL_HAL_GET_DEVICEID tests"          ), 0, 0,         0, NULL,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Normal Operation"                     ), 1, idIOCTL_HAL_GET_DEVICEID_Working,  DEVICEID + 1, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Normal Operation No lpBytesReturned"  ), 1, idIOCTL_HAL_GET_DEVICEID_Working_NoLpBytesReturned,  DEVICEID + 2, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Normal Operation Check Query"         ), 1, idIOCTL_HAL_GET_DEVICEID_Working_CheckQuery,  DEVICEID + 3, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Force Failure - Query on Large Buffer"), 1, idIOCTL_HAL_GET_DEVICEID_ForceFailure_QueryOnLargeBuffer,  DEVICEID + 7, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Force Failure - outBufSize = 0"       ), 1, idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize0,  DEVICEID + 8, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Force Failure - outBufSize = 1"       ), 1, idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize1,  DEVICEID + 9, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Force Failure - outBufSize One Less"  ), 1, idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSizeOneLess,  DEVICEID + 10, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Normal Operation - outBufSize One Greater"  ), 1, idIOCTL_HAL_GET_DEVICEID_Working_outSizeOneGreater,  DEVICEID + 11, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_DEVICEID - Force Failure - outBuf is NULL"  ), 1, idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufIsNull,  DEVICEID + 12, ioctlTestEntry,

   TEXT("IOCTL_HAL_GET_UUID tests"          ), 0, 0,         0, NULL,
   TEXT(   "IOCTL_HAL_GET_UUID - Normal Operation"                     ), 1, idIOCTL_HAL_GET_UUID_Working,  UUID_BASE + 1, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Normal Operation No lpBytesReturned"  ), 1, idIOCTL_HAL_GET_UUID_Working_NoLpBytesReturned,  UUID_BASE + 2, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Force Failure - outBufSize = 0"       ), 1, idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize0,  UUID_BASE + 6, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Force Failure - outBufSize = 1"       ), 1, idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize1,  UUID_BASE + 7, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Force Failure - outBufSize One Less"  ), 1, idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSizeOneLess,  UUID_BASE + 8, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Normal Operation - outBufSize One Greater"  ), 1, idIOCTL_HAL_GET_UUID_Working_outSizeOneGreater,  UUID_BASE + 9, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Force Failure - outBuf is NULL"  ), 1, idIOCTL_HAL_GET_UUID_ForceFailure_OutBufIsNull,  UUID_BASE + 10, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Normal Operation - Alignment 8th Bit"  ), 1, idIOCTL_HAL_GET_UUID_Working_Alignment8thBit,  UUID_BASE + 11, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Normal Operation - Alignment 16th Bit"  ), 1, idIOCTL_HAL_GET_UUID_Working_Alignment16thBit,  UUID_BASE + 12, ioctlTestEntry,
   TEXT(   "IOCTL_HAL_GET_UUID - Normal Operation - Alignment 24th Bit"  ), 1, idIOCTL_HAL_GET_UUID_Working_Alignment24thBit,  UUID_BASE + 13, ioctlTestEntry,

   NULL,                                      0,  0,             0, NULL  // marks end of list
};

#endif // __TUXDEMO_H__
