//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * ioctlTests.cpp
 */

#include "ioctlTests.h"

/* local only to this file */
#include "layoutMap.h"

BOOL
verifyDEVICE_IDstruct (DEVICE_ID * devId);

void
dumpDEVICE_IDErr (DEVICE_ID * devId)
{  
  Error (_T("dwSize = %u  dwPresetIDOffset = %u  dwPresetIDBytes = %u ")
	 _T("dwPlatformIDOffset = %u  dwPlatformIDBytes = %u"),
	 devId->dwSize, devId->dwPresetIDOffset, devId->dwPresetIDBytes,
	 devId->dwPlatformIDOffset, devId->dwPlatformIDBytes);
}

BOOL
verifyDEVICE_IDstruct (DEVICE_ID * devId)
{
  if (devId == NULL)
    {
      IntErr (_T("devId == NULL"));
      return (BAD_VAL);
    }

  BOOL returnVal = TRUE;

  /* map to store the layout of the data in the DEVICE_ID structure */
  sLayoutMap valueMap[3];

  /* always will use at least 1 entry, for the structure information */
  DWORD dwLayoutMapUsed = 1;

  valueMap[0].dwLowerBound = 0;
  valueMap[0].dwUpperBound = sizeof (DEVICE_ID);

  /* handle the preset part, if it exists */
  if (devId->dwPresetIDBytes == 0)
    {
      Info (_T("No preset identifier set in DEVICE_ID structure."));
    }
  else
    {
      valueMap[dwLayoutMapUsed].dwLowerBound = devId->dwPresetIDOffset;
      valueMap[dwLayoutMapUsed].dwUpperBound = 
	devId->dwPresetIDOffset + devId->dwPresetIDBytes;
      
      if (valueMap[dwLayoutMapUsed].dwUpperBound < 
	  MAX(devId->dwPresetIDOffset, devId->dwPresetIDBytes))
	{
	  /* overflowed - which doesn't make sense */
	  Error (_T("dwPresetIDOffset + dwPresetIDBytes overflowed."));
	  Error (_T("dwPresetIDOffset = %u and dwPresetIDBytes = %u."),
		 devId->dwPresetIDOffset, devId->dwPresetIDBytes);
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}
		 
      dwLayoutMapUsed++;
    }

  /* handle the platform part, if it exists */
  if (devId->dwPlatformIDBytes == 0)
    {
      Info (_T("No platform identifier set in DEVICE_ID structure."));
    }
  else
    {
      valueMap[dwLayoutMapUsed].dwLowerBound = devId->dwPlatformIDOffset;
      valueMap[dwLayoutMapUsed].dwUpperBound = 
	devId->dwPlatformIDOffset + devId->dwPlatformIDBytes;

      if (valueMap[dwLayoutMapUsed].dwUpperBound < 
	  MAX(devId->dwPlatformIDOffset, devId->dwPlatformIDBytes))
	{
	  /* overflowed - which doesn't make sense */
	  Error (_T("dwPlatformIDOffset + dwPlatformIDBytes overflowed."));
	  Error (_T("dwPlatformIDOffset = %u and dwPlatformIDBytes = %u."),
		 devId->dwPlatformIDOffset, devId->dwPlatformIDBytes);
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}

      dwLayoutMapUsed++;
    }

  if (sortAndCheckLayoutMap (valueMap, dwLayoutMapUsed) == FALSE)
    {
      Error (_T("The DEVICE_ID structure has overlapping offsets and lengths."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  /* make sure that the size is at least large enough for the offsets + sizes */
  if (devId->dwSize < valueMap[dwLayoutMapUsed - 1].dwUpperBound)
    {
      Error (_T("The dwSize part of DEVICE_ID isn't big enough to contain"));
      Error (_T("all of the data referenced in the rest of the structure."));
      Error (_T("dwSize is %u and it should be at least %u."), devId->dwSize, 
	     valueMap[dwLayoutMapUsed - 1].dwUpperBound);
      returnVal = FALSE;
      goto cleanAndReturn;
    }

 cleanAndReturn:
  if (returnVal == FALSE)
    {
      dumpDEVICE_IDErr (devId);
    }

  return (returnVal);
}

/**************************************************************************
 *
 *                             Normal Behavior
 *
 **************************************************************************/

/*
 * general test case.  Tests operation of the function (and a good set
 * of example code).
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  /* pointers that have memory malloced at some point during this function */
  DEVICE_ID * devId = NULL;		/* the device id */
  
  DWORD devIdSize = 0;

  /* 
   * The correct size of the devId, which is determined early in this
   * function 
   */
  DWORD dwCorrectDevIdSizeBytes;

  /* 
   * we need a junk DEVICE_ID to set the dwSize to null to query for the
   * size 
   */
  DEVICE_ID queryId;

  /* set the size part to zero for the query function */
  queryId.dwSize = 0;

  BOOL bRet;

  /* figure out how large the devId needs to be for a standard, correct call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  &queryId, sizeof (queryId), &dwCorrectDevIdSizeBytes);

  DWORD getLastErrorVal;

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong.  We called"));
      Error (_T("KernelIoControl (IOCTL_HAL_GET_BUFFER, NULL, 0, NULL, 0, ")
	     _T("&dwCorrectDevIdSizeBytes)"));
      Error (_T("and didn't get FALSE and ERROR_INSUFFICIENT_DEVID list"));
      Error (_T("expected.  Instead we got a return value of %u and a ")
	     _T("GetLastError"), bRet);
      Error (_T("of %u."), getLastErrorVal);
      goto cleanAndReturn;
    }

  if (queryId.dwSize != dwCorrectDevIdSizeBytes)
    {
      Error (_T("The size returned in the DEVICE_ID structure (%u bytes) "),
	     queryId.dwSize);
      Error (_T("differs from that returned by the ioctl call (%u bytes)."),
	     dwCorrectDevIdSizeBytes);
      goto cleanAndReturn;
    }

  Info (_T("Just queried KernelIoControl and determined that the correct"));
  Info (_T("structure size is %u bytes."), dwCorrectDevIdSizeBytes);

  /* allocate memory for the devId */
  devId = (DEVICE_ID *) malloc (dwCorrectDevIdSizeBytes);

  if (devId == NULL)
    {
      Error (_T("Couldn't allocate %u bytes for the devId."),
	     dwCorrectDevIdSizeBytes);
      
      goto cleanAndReturn;
    }

  /* want to check that the call sets the right size on output */
  DWORD checkSize;
  
  /* try to get the DEVICE_ID */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  devId, dwCorrectDevIdSizeBytes, &checkSize);

  if (bRet != TRUE)
    {
      /* something went wrong... */
      Error (_T("Couldn't get the device id.  It might not be supported."));
      Error (_T("GetLastError returned %u."), GetLastError ());
      goto cleanAndReturn;
    }

  if (checkSize != dwCorrectDevIdSizeBytes)
    {
      Error (_T("We calculated that the DEVICE_ID structure should be %u bytes"),
	     dwCorrectDevIdSizeBytes);
      Error (_T("and the ioctl is now reporting %u bytes."), checkSize);
      goto cleanAndReturn;
    }

  /* check the size */
  if (devId->dwSize != dwCorrectDevIdSizeBytes)
    {
      Error (_T("The dwSize value of the DEVICE_ID structure (%u byes) does not"),
	     devId->dwSize);
      Error (_T("match the value returned by the ioctl (%u bytes)."),
	     dwCorrectDevIdSizeBytes);
      goto cleanAndReturn;
    }

  /* verify that the id values make sense */
  if (verifyDEVICE_IDstruct (devId) != TRUE)
    {
      Error (_T("Something is wrong with the DEVICE_ID structure"));
      Error (_T("dwSize = %u  dwPresetIDOffset = %u  dwPresetIDBytes = %u ")
	     _T("dwPlatformIDOffset = %u  dwPlatformIDBytes = %u"),
	     devId->dwSize, devId->dwPresetIDOffset, devId->dwPresetIDBytes,
	     devId->dwPlatformIDOffset, devId->dwPlatformIDBytes);
      
      goto cleanAndReturn;
    }
  
  /* print out the values */
  Info (_T("The DEVICE_ID structure values are:"));
  Info (_T("  dwSize = %u  dwPresetIDOffset = %u  dwPresetIDBytes = %u "),
	devId->dwSize, devId->dwPresetIDOffset, devId->dwPresetIDBytes);
  Info (_T("  dwPlatformIDOffset = %u  dwPlatformIDBytes = %u"),
	devId->dwPlatformIDOffset, devId->dwPlatformIDBytes);

  /* print out the preset id if set */
  if (devId->dwPresetIDOffset != 0)
    {
      DWORD dwUnicodeLengthWithNullTerm;

      /* code below relies on this assertion */
      if ((sizeof (WCHAR) != 2) || (sizeof (BYTE) != 1))
	{
	  IntErr (_T("((sizeof (WCHAR) != 2) || (sizeof (BYTE) != 1))"));
	  goto cleanAndReturn;
	}

      if (devId->dwPresetIDBytes > (MAX_DWORD - 1))
	{
	  Error (_T("Overflow: devId->dwPresetIDBytes is too large."));
	  goto cleanAndReturn;
	}

      /* if odd number of bytes then add one to make even */
      if (devId->dwPresetIDBytes % 2 == 1)
	{
	  dwUnicodeLengthWithNullTerm = 
	    (devId->dwPresetIDBytes + 1) / 2 + 1;
	}
      else
	{
	  dwUnicodeLengthWithNullTerm = 
	    devId->dwPresetIDBytes / 2 + 1;
	}

      /* 
       * values are not guaranteed to be null terminated.  Get a buffer
       * one more so that we force them to be.
       */
      WCHAR * szPreset = 
	(WCHAR *) malloc(dwUnicodeLengthWithNullTerm * sizeof (WCHAR));

      if (szPreset == NULL)
	{
	  Error (_T("Couldn't allocate memory for szPreset"));
	  goto cleanAndReturn;
	}
      
      /* zero buffer first */
      memset (szPreset, 0, dwUnicodeLengthWithNullTerm * sizeof (WCHAR));

      /* copy from the buffer into our larger buffer */
      memcpy (szPreset, ((BYTE *) devId) + devId->dwPresetIDOffset,
	      devId->dwPresetIDBytes);

      /* force null terminate */
      szPreset[dwUnicodeLengthWithNullTerm - 1] = _T('\0');

      Info (_T("Device preset id (unicode) is \"%s\""), szPreset);

      free (szPreset);
    }
  else
    {
      Info (_T("Device preset id is not set"));
    }

  /* print out the platform id if set */
  if (devId->dwPlatformIDOffset != 0)
    {
      DWORD dwUnicodeLengthWithNullTerm;

      /* code below relies on this assertion */
      if ((sizeof (WCHAR) != 2) || (sizeof (BYTE) != 1))
	{
	  IntErr (_T("((sizeof (WCHAR) != 2) || (sizeof (BYTE) != 1))"));
	  goto cleanAndReturn;
	}

      if (devId->dwPresetIDBytes > (MAX_DWORD - 1))
	{
	  Error (_T("Overflow: devId->dwPresetIDBytes is too large."));
	  goto cleanAndReturn;
	}

      /* if odd number of bytes then add one to make even */
      if (devId->dwPlatformIDBytes % 2 == 1)
	{
	  dwUnicodeLengthWithNullTerm = 
	    (devId->dwPlatformIDBytes + 1) / 2 + 1;
	}
      else
	{
	  dwUnicodeLengthWithNullTerm = 
	    devId->dwPlatformIDBytes / 2 + 1;
	}

      /* 
       * values are not guaranteed to be null terminated.  Get a buffer
       * one more so that we force them to be.
       */
      WCHAR * szPlatform = 
	(WCHAR *) malloc(dwUnicodeLengthWithNullTerm * sizeof (WCHAR));

      if (szPlatform == NULL)
	{
	  Error (_T("Couldn't allocate memory for szPlatform"));
	  goto cleanAndReturn;
	}
      
      /* zero buffer first */
      memset (szPlatform, 0, dwUnicodeLengthWithNullTerm * sizeof (WCHAR));

      /* copy from the buffer into our larger buffer */
      memcpy (szPlatform, ((BYTE *) devId) + devId->dwPlatformIDOffset,
	      devId->dwPlatformIDBytes);

      /* force null terminate */
      szPlatform[dwUnicodeLengthWithNullTerm - 1] = _T('\0');

      Info (_T("Device platform id (unicode) is \"%s\""), szPlatform);

      free (szPlatform);
    }
  else
    {
      Info (_T("Device preset id is not set"));
    }

  /* only if we got here did we succeed */
  returnVal = TRUE;

 cleanAndReturn:
  
  free (devId);

  return (returnVal);
}

/*
 * confirm that no specifying lpBytesReturned doesn't change the
 * behavior.  We compare to the output returned when this parameter is
 * not null, so we assume that the test above is working.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working_NoLpBytesReturned()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  /* return val from KernelIoControl */
  BOOL bRet;

  /* pointers that have memory malloced at some point during this function */
  DEVICE_ID * devId1 = NULL;		/* the device id */
  DEVICE_ID * devId2 = NULL;

  /* 
   * The correct size of the devId, which is determined early in this
   * function 
   */
  DWORD dwCorrectDevIdSizeBytes;

  /* 
   * we need a DEVICE_ID to set the dwSize to null to query for the
   * size
   */
  DEVICE_ID queryId;

  /* set the size part to zero for the query function */
  queryId.dwSize = 0;

  /* figure out how large the devId needs to be for a standard, correct call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  &queryId, sizeof (queryId), NULL);

  DWORD getLastErrorVal;

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong with query call."));
      Error (_T("We got a return val of %u and getLastError of %u."),
	     bRet, getLastErrorVal);

      goto cleanAndReturn;
    }

  /* save the size */
  dwCorrectDevIdSizeBytes = queryId.dwSize;

  Info (_T("Just queried KernelIoControl and determined that the correct"));
  Info (_T("structure size is %u bytes."), dwCorrectDevIdSizeBytes);

  /* allocate memory for the devId */
  devId1 = (DEVICE_ID *) malloc (dwCorrectDevIdSizeBytes);
  devId2 = (DEVICE_ID *) malloc (dwCorrectDevIdSizeBytes);

  if ((devId1 == NULL) || (devId2 == NULL))
    {
      Error (_T("Couldn't allocate memory for device ids"));
      
      goto cleanAndReturn;
    }

  /* try to get the DEVICE_ID without ending pointer */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  devId1, dwCorrectDevIdSizeBytes, NULL);

  if (bRet != TRUE)
    {
      /* something went wrong... */
      Error (_T("Couldn't get the device id for lpBytesReturned = NULL."));
      Error (_T("GetLastError returned %u."), GetLastError ());
      goto cleanAndReturn;
    }

  /* check to make sure that outbound size is correct */
  DWORD checkSize;

  /* try to get the DEVICE_ID with ending pointer */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  devId2, dwCorrectDevIdSizeBytes, &checkSize);

  if (bRet != TRUE)
    {
      /* something went wrong... */
      Error (_T("Couldn't get the device id for lpBytesReturned != NULL."));
      Error (_T("GetLastError returned %u."), GetLastError ());
      goto cleanAndReturn;
    }

  /* check what we can */
  if (checkSize != dwCorrectDevIdSizeBytes)
    {
      Error (_T("We calculated that the DEVICE_ID structure should be %u bytes"),
	     dwCorrectDevIdSizeBytes);
      Error (_T("and the ioctl is now reporting %u bytes."), checkSize);
      goto cleanAndReturn;
    }

  /* compare the two structs */
  if (memcmp (devId1, devId2, dwCorrectDevIdSizeBytes) != 0)
    {
      Error (_T("The structures returned from the ioctl differ, depending on "));
      Error (_T("whether lpBytesReturned is set or not."));
      goto cleanAndReturn;
    }

  /* only if we got here did we succeed */
  returnVal = TRUE;

 cleanAndReturn:
  
  free (devId1);
  free (devId2);

  return (returnVal);
}


/*
 * confirm that no specifying lpBytesReturned doesn't change the
 * behavior for a query.  We compare to the output returned when this
 * paramter is not null, so we assume that the test above is working.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working_CheckQuery()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  /* return val from KernelIoControl */
  BOOL bRet;

  DEVICE_ID devId1;
  DEVICE_ID devId2;

  /* set the size part to zero for the query function */
  devId1.dwSize = 0;
  devId2.dwSize = 0;

  /* figure out how large the devId needs to be for a standard, correct call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  &devId1, sizeof (devId1), NULL);

  DWORD getLastErrorVal;

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong with query call."));
      Error (_T("We got a return val of %u and getLastError of %u."),
	     bRet, getLastErrorVal);
      goto cleanAndReturn;
    }

  DWORD checkSize;

  /* figure out how large the devId needs to be for a standard, correct call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  &devId2, sizeof (devId2), &checkSize);

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong with query call."));
      Error (_T("We got a return val of %u and getLastError of %u."),
	     bRet, getLastErrorVal);
      goto cleanAndReturn;
    }

  /* check what we can */
  if (devId1.dwSize != devId2.dwSize)
    {
      Error (_T("devId1.dwSize != devId2.dwSize   %u != %u"),
	     devId1.dwSize, devId2.dwSize);
      goto cleanAndReturn;
    }

  if (devId1.dwSize != checkSize)
    {
      Error (_T("devId1.dwSize != checkSize   %u != %u"),
	     devId1.dwSize, checkSize);
      goto cleanAndReturn;
    }

  returnVal = TRUE;

 cleanAndReturn:

  if (returnVal != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

  return (returnVal);
}

/**************************************************************************
 *
 *                             Incorrect Inbound Args
 *
 **************************************************************************/

/*
 * Force a failure by setting the nInBuf to a bad ptr.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_inBuf()
{
  /* 
   * we need a junk DEVICE_ID as input
   */
  DEVICE_ID queryId;

  BOOL bRet;

  /* use something that, if they dereference, will crash */
  DWORD * badPtr = (DWORD *) 1;

  /* make call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, badPtr, 0, 
			  &queryId, sizeof (queryId), NULL);
  
  if (bRet != FALSE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

}

/*
 * Force a failure by setting the nInBufSize to != 0
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_inBufSize()
{
  /* 
   * we need a junk DEVICE_ID as input
   */
  DEVICE_ID queryId;

  BOOL bRet;

  /* make call with wrong inBufSize */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 1,
			  &queryId, sizeof (queryId), NULL);
  
  if (bRet != FALSE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

}


/*
 * Force a failure by setting the inbound parameters, just as someone
 * whom didn't understand the syntax would do.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_inBoundParams()
{
  DEVICE_ID queryId, inBound;

  BOOL bRet;

  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, &inBound, sizeof (inBound),
			  &queryId, sizeof (queryId), NULL);
  
  if (bRet != FALSE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
}

/**************************************************************************
 *
 *                             Test Query Operations
 *
 **************************************************************************/

/* 
 * run a query, but send in a large enough buffer to make sure that we still
 * get query operation.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_QueryOnLargeBuffer()
{
  /* this should be much larger than any DEVICE_ID */
  BYTE buffer[1024];

  DEVICE_ID * queryId = (DEVICE_ID *) &buffer;

  /* tell it to query */
  queryId->dwSize = 0;

  BOOL bRet;

  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0,
			  &queryId, sizeof (queryId), NULL);
  
  if (bRet != FALSE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
}

/**************************************************************************
 *
 *                             OutBufSize checks
 *
 **************************************************************************/

/*
 * force failure by setting outBufSize to 0 
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize0()
{
  /* 
   * we need a junk DEVICE_ID as input
   */
  DEVICE_ID queryId;

  BOOL bRet;

  DWORD junk;

  /* make call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0,
			  &queryId, 0, &junk);
  
  if (bRet != FALSE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

}

/*
 * force failure by setting outBufSize to 0 
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSize1()
{
  /* 
   * we need a junk DEVICE_ID as input
   */
  DEVICE_ID queryId;

  BOOL bRet;

  DWORD junk;

  /* make call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 1,
			  &queryId, 0, &junk);
  
  if (bRet != FALSE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

}


/*
 * force failure by setting outBufSize to one less than is needed
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_ForceFailure_OutBufSizeOneLess()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  /* return val from KernelIoControl */
  BOOL bRet;

  /* pointers that have memory malloced at some point during this function */
  DEVICE_ID * devId1 = NULL;		/* the device id */

  /* 
   * The correct size of the devId, which is determined early in this
   * function 
   */
  DWORD dwCorrectDevIdSizeBytes;

  /* 
   * we need a DEVICE_ID to set the dwSize to null to query for the
   * size
   */
  DEVICE_ID queryId;

  /* set the size part to zero for the query function */
  queryId.dwSize = 0;

  /* figure out how large the devId needs to be for a standard, correct call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  &queryId, sizeof (queryId), NULL);

  DWORD getLastErrorVal;

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong with query call."));
      Error (_T("We got a return val of %u and getLastError of %u."),
	     bRet, getLastErrorVal);
      goto cleanAndReturn;
    }

  /* save the size */
  dwCorrectDevIdSizeBytes = queryId.dwSize;

  if (dwCorrectDevIdSizeBytes < sizeof (DEVICE_ID))
    {
      Error (_T("Size needed for device id must be larger than sizeof(DEVICE_ID)"));
      Error (_T("%u < %u"), dwCorrectDevIdSizeBytes, sizeof (DEVICE_ID));
      goto cleanAndReturn;
    }

  Info (_T("Just queried KernelIoControl and determined that the correct"));
  Info (_T("structure size is %u bytes."), dwCorrectDevIdSizeBytes);

  /* allocate memory for the devId */
  devId1 = (DEVICE_ID *) malloc (dwCorrectDevIdSizeBytes);

  if (devId1 == NULL)
    {
      Error (_T("Couldn't allocate memory for device id."));
      
      goto cleanAndReturn;
    }

  /* try to get the DEVICE_ID without ending pointer */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  devId1, dwCorrectDevIdSizeBytes - 1, NULL);

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong with query call."));
      Error (_T("We got a return val of %u and getLastError of %u."),
	     bRet, getLastErrorVal);
      goto cleanAndReturn;
    }

  /* only if we got here did we succeed */
  returnVal = TRUE;

 cleanAndReturn:
  
  free (devId1);

  if (returnVal != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

  return (returnVal);
}


/*
 * set the outsize to one greater than what is needed.  ioctl should
 * return data correctly.
 */
BOOL
testIOCTL_HAL_GET_DEVICEID_Working_outSizeOneGreater()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  /* return val from KernelIoControl */
  BOOL bRet;

  /* pointers that have memory malloced at some point during this function */
  DEVICE_ID * devId1 = NULL;		/* the device id */
  DEVICE_ID * devId2 = NULL;

  /* 
   * The correct size of the devId, which is determined early in this
   * function 
   */
  DWORD dwCorrectDevIdSizeBytes;

  /* 
   * we need a DEVICE_ID to set the dwSize to null to query for the
   * size
   */
  DEVICE_ID queryId;

  /* set the size part to zero for the query function */
  queryId.dwSize = 0;

  /* figure out how large the devId needs to be for a standard, correct call */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  &queryId, sizeof (queryId), NULL);

  DWORD getLastErrorVal;

  /* we want to get an ERROR_INSUFFICIENT_BUFFER and FALSE */
  if ((bRet == FALSE) &&
      ((getLastErrorVal = GetLastError()) == ERROR_INSUFFICIENT_BUFFER))
    {
      /* what we want */
    }
  else
    {
      /* something went wrong */
      Error (_T("Something went wrong with query call."));
      Error (_T("We got a return val of %u and getLastError of %u."),
	     bRet, getLastErrorVal);
      goto cleanAndReturn;
    }

  /* save the size */
  dwCorrectDevIdSizeBytes = queryId.dwSize;

  Info (_T("Just queried KernelIoControl and determined that the correct"));
  Info (_T("structure size is %u bytes."), dwCorrectDevIdSizeBytes);

  /* allocate memory for the devId */
  devId1 = (DEVICE_ID *) malloc (dwCorrectDevIdSizeBytes);
  devId2 = (DEVICE_ID *) malloc (dwCorrectDevIdSizeBytes + 1);

  if ((devId1 == NULL) || (devId2 == NULL))
    {
      Error (_T("Couldn't allocate memory for device ids"));
      
      goto cleanAndReturn;
    }

  /* try to get the DEVICE_ID without ending pointer */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  devId1, dwCorrectDevIdSizeBytes, NULL);

  if (bRet != TRUE)
    {
      /* something went wrong... */
      Error (_T("Couldn't get the device id for lpBytesReturned = NULL."));
      Error (_T("GetLastError returned %u."), GetLastError ());
      goto cleanAndReturn;
    }

  /* check to make sure that outbound size is correct */
  DWORD checkSize;

  /* try to get the DEVICE_ID with ending pointer */
  bRet = KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, 
			  devId2, dwCorrectDevIdSizeBytes + 1, &checkSize);

  if (bRet != TRUE)
    {
      /* something went wrong... */
      Error (_T("Couldn't get the device id for lpBytesReturned != NULL."));
      Error (_T("GetLastError returned %u."), GetLastError ());
      goto cleanAndReturn;
    }

  /* returned bytes should still be the same */
  if (checkSize != dwCorrectDevIdSizeBytes)
    {
      Error (_T("We calculated that the DEVICE_ID structure should be %u bytes"),
	     dwCorrectDevIdSizeBytes);
      Error (_T("and the ioctl is now reporting %u bytes."), checkSize);
      goto cleanAndReturn;
    }

  /* compare the two structs */
  if (memcmp (devId1, devId2, dwCorrectDevIdSizeBytes) != 0)
    {
      Error (_T("The structures returned differs, depending on whether"));
      Error (_T("lpBytesReturned is set or not."));
      goto cleanAndReturn;
    }

  /* only if we got here did we succeed */
  returnVal = TRUE;

 cleanAndReturn:
  
  free (devId1);
  free (devId2);

  if (bRet != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u"), __FUNCTION__, bRet);
      return (FALSE);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }

  return (returnVal);
}




