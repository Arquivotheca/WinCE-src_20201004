//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * tuxIoctls.cpp
 */


/* provides the TESTPROC and enum declaration */
#include "tuxIoctls.h"

/* needed for ACCESSTIMEOUT definition below (and maybe others) */
#include "winuser.h"

/* provides actual test functions */
#include "IOCTL_HAL_GET_DEVICEIDtests.h"
#include "IOCTL_HAL_GET_UUIDtests.h"

/*
 * main entry for the ioctl tests.  The test that we want to run is passed through
 * the lpFTE paramter.
 */
TESTPROCAPI ioctlTestEntry(
			   UINT uMsg, 
			   TPPARAM tpParam, 
			   LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT returnVal = TPR_PASS;

  /* only supporting executing the test */
  if (uMsg != TPM_EXECUTE)
    {
      returnVal =  TPR_NOT_HANDLED;
      goto cleanAndExit;
    }

  /* the return value from the test */
  BOOL rval = FALSE;

  /* the argument from the function table has the test to run */
  switch (lpFTE->dwUserData)
    {
      /******  IOCTL_HAL_GET_DEVICEID  ****************************************/
    case idIOCTL_HAL_GET_DEVICEID_Working:
      rval = testIOCTL_HAL_GET_DEVICEID_Working();
      break;
    case idIOCTL_HAL_GET_DEVICEID_Working_NoLpBytesReturned:
      rval = testIOCTL_HAL_GET_DEVICEID_Working_NoLpBytesReturned();
      break;
    case idIOCTL_HAL_GET_DEVICEID_Working_CheckQuery:
      rval = testIOCTL_HAL_GET_DEVICEID_Working_CheckQuery();
      break;
    case idIOCTL_HAL_GET_DEVICEID_ForceFailure_QueryOnLargeBuffer:
      rval = testIOCTL_HAL_GET_DEVICEID_ForceFailure_QueryOnLargeBuffer();
      break;
    case idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize0:
      rval = testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize0();
      break;
    case idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize1:
      rval = testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize1();
      break;
    case idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSizeOneLess:
      rval = testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSizeOneLess();
      break;
    case idIOCTL_HAL_GET_DEVICEID_Working_outSizeOneGreater:
      rval = testIOCTL_HAL_GET_DEVICEID_Working_outSizeOneGreater();
      break;
    case idIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufIsNull:
      rval = testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufIsNull();
      break;


      /******  IOCTL_HAL_GET_UUID  **********************************************/
    case idIOCTL_HAL_GET_UUID_Working:
      rval = testIOCTL_HAL_GET_UUID_Working();
      break;
    case idIOCTL_HAL_GET_UUID_Working_NoLpBytesReturned:
      rval = testIOCTL_HAL_GET_UUID_Working_NoLpBytesReturned();
      break;
    case idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize0:
      rval = testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize0();
      break;
    case idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize1:
      rval = testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize1();
      break;
    case idIOCTL_HAL_GET_UUID_ForceFailure_OutBufSizeOneLess:
      rval = testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSizeOneLess();
      break;
    case idIOCTL_HAL_GET_UUID_Working_outSizeOneGreater:
      rval = testIOCTL_HAL_GET_UUID_Working_outSizeOneGreater();
      break;
    case idIOCTL_HAL_GET_UUID_ForceFailure_OutBufIsNull:
      rval = testIOCTL_HAL_GET_UUID_ForceFailure_OutBufIsNull();
      break;
    case idIOCTL_HAL_GET_UUID_Working_Alignment8thBit:
      rval = testIOCTL_HAL_GET_UUID_Working_Alignment(1);
      break;
    case idIOCTL_HAL_GET_UUID_Working_Alignment16thBit:
      rval = testIOCTL_HAL_GET_UUID_Working_Alignment(2);
      break;
    case idIOCTL_HAL_GET_UUID_Working_Alignment24thBit:
      rval = testIOCTL_HAL_GET_UUID_Working_Alignment(3);
      break;

      /******  DEFAULT  ***************************************************/

    default:
      Error (_T("Unknown test specified."));
      rval = FALSE;
      break;
    }

 cleanAndExit:

  if ((rval == TRUE) && (returnVal == TPR_PASS))
    {
      return (TPR_PASS);
    }
  else
    {
      return (TPR_FAIL);
    }
}
