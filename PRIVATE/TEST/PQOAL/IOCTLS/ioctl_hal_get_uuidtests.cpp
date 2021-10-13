//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * ioctl_hal_get_uuidtests.cpp
 */

#include "IOCTL_HAL_GET_UUIDtests.h"

#include "..\common\commonUtils.h"
#include "ioctlCommon.h"

/*************************************************************************
 * 
 *                Functions and Defs local to this file
 *
 *************************************************************************/

/*
 * length of UUID.  Setting to zero or MAX_DWORD - sizeof(DWORD) will
 * break this test code.  These values aren't correct anyhow.
 */
#define UUID_LENGTH_B 16

/*
 * print out the data structure 
 */
void
dumpUUIDErr(byte UNALIGNED * buffer)
{
  Error (_T("0x%x 0x%x 0x%x 0x%x"), 
	 (DWORD) *(buffer + 0 * sizeof(DWORD)),
	 (DWORD) *(buffer + 1 * sizeof(DWORD)),
	 (DWORD) *(buffer + 2 * sizeof(DWORD)),
	 (DWORD) *(buffer + 3 * sizeof(DWORD)));
}

void
dumpUUIDInfo(byte * buffer)
{
  Info (_T("0x%x 0x%x 0x%x 0x%x"), 
	(DWORD) *(buffer + 0 * sizeof(DWORD)),
	(DWORD) *(buffer + 1 * sizeof(DWORD)),
	(DWORD) *(buffer + 2 * sizeof(DWORD)),
	(DWORD) *(buffer + 3 * sizeof(DWORD)));
}


/**************************************************************************
 *
 *                             Normal Behavior
 *
 **************************************************************************/

/*
 * general test case.  Tests operation of the function.
 */
BOOL
testIOCTL_HAL_GET_UUID_Working()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  byte buffer[UUID_LENGTH_B];

  BOOL bRet;

  DWORD dwReturnedBytes;

  DWORD dwLastError;

  /* query for the buffer */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, buffer, sizeof (buffer), 
	     &dwReturnedBytes, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      Info (_T("Successfully retrieved UUID.  It is:"));
      dumpUUIDInfo(buffer);
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s."),
	     bRet, errToStr(dwLastError));
      goto cleanAndReturn;
    }

  /* check to make sure that bytes returned is correct */
  if (UUID_LENGTH_B != dwReturnedBytes)
    {
      Error (_T("Bytes returned from KernelIoControl != length of UUID."));
      Error (_T("%u != %u"), UUID_LENGTH_B, dwReturnedBytes);
      goto cleanAndReturn;
    }

  returnVal = TRUE;

 cleanAndReturn:
  
  if (returnVal != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED"), __FUNCTION__);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
    }
  
  return (returnVal);

}


/*
 * general test case.  Tests operation of the function.  The optional
 * lpBytesReturned paramter to KernelIoControl is set to null.
 */
BOOL
testIOCTL_HAL_GET_UUID_Working_NoLpBytesReturned()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  byte buffer1[UUID_LENGTH_B];
  byte buffer2[UUID_LENGTH_B];
  BOOL bRet;

  DWORD dwReturnedBytes;

  DWORD dwLastError;

  /* get the version with bytes returned */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, buffer1, sizeof (buffer1), 
	     &dwReturnedBytes, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* it worked */
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s"),
	     bRet, errToStr(dwLastError));
      goto cleanAndReturn;
    }

  /* check to make sure that bytes returned is correct */
  if (UUID_LENGTH_B != dwReturnedBytes)
    {
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Bytes returned from KernelIoControl != length of UUID."));
      Error (_T("%u != %u"), UUID_LENGTH_B, dwReturnedBytes);
      goto cleanAndReturn;
    }

  /* get the version without bytes returned */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, 
	     buffer2, sizeof (buffer2), NULL, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* it worked */
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("In test part of %S"), __FUNCTION__);
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("In test part of %S"), __FUNCTION__);
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s"),
	     bRet, errToStr(dwLastError));
      goto cleanAndReturn;
    }

  /* compare two structures */
  if (memcmp (buffer1, buffer2, sizeof (buffer1)) != 0)
    {
      Error (_T("Buffers are not equal.  lpBytesReturned version:"));
      dumpUUIDErr (buffer1);
      Error (_T("no lpBytesReturned version:"));
      dumpUUIDErr (buffer2);
      goto cleanAndReturn;
    }
      
  returnVal = TRUE;

 cleanAndReturn:
  
  if (returnVal != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED"), __FUNCTION__);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
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
testIOCTL_HAL_GET_UUID_ForceFailure_inBuf()
{
  byte buffer[UUID_LENGTH_B];

  BOOL bRet;

  /* use something that, if they dereference, will crash */
  DWORD * badPtr = (DWORD *) 1;

  DWORD dwLastError;

  /* make call */
  wrapIoctl (IOCTL_HAL_GET_UUID, badPtr, 0, 
	     buffer, sizeof (buffer), NULL, &bRet, &dwLastError);
  
  if ((bRet == FALSE) && (dwLastError == ERROR_INVALID_PARAMETER))
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      return (FALSE);
    }
}

/*
 * Force a failure by setting the nInBufSize to != 0
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_inBufSize()
{
  byte buffer[UUID_LENGTH_B];

  BOOL bRet;

  DWORD dwLastError;

  /* make call with wrong inBufSize */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 1,
	     buffer, sizeof (buffer), NULL, &bRet, &dwLastError);
  
  if ((bRet == FALSE) && (dwLastError == ERROR_INVALID_PARAMETER))
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      return (FALSE);
    }
}


/*
 * Force a failure by setting the inbound parameters, just as someone
 * whom didn't understand the syntax would do.
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_inBoundParams()
{
  byte buffer[UUID_LENGTH_B], inBound[UUID_LENGTH_B];

  BOOL bRet;

  DWORD dwLastError;

  wrapIoctl (IOCTL_HAL_GET_UUID, inBound, sizeof (inBound),
	     buffer, sizeof (buffer), NULL, &bRet, &dwLastError);
  
  if ((bRet == FALSE) && (dwLastError == ERROR_INVALID_PARAMETER))
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      return (FALSE);
    }
}

/**************************************************************************
 *
 *                             OutBuf checks
 *
 **************************************************************************/

/*
 * force failure by setting outBufSize to 0 
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize0()
{
  /* 
   * we need a junk byte as input
   */
  byte buffer[UUID_LENGTH_B];

  BOOL bRet;

  DWORD junk;

  DWORD dwLastError;

  /* make call */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0,
	     buffer, 0, &junk, &bRet, &dwLastError);
  
  if ((bRet == FALSE) && (dwLastError == ERROR_INSUFFICIENT_BUFFER))
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      return (FALSE);
    }

}

/*
 * force failure by setting outBufSize to 1
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSize1()
{
  /* 
   * we need a junk buffer as input
   */
  byte buffer[UUID_LENGTH_B];

  BOOL bRet;

  DWORD junk;

  DWORD dwLastError;

  /* make call */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0,
	     buffer, 1, &junk, &bRet, &dwLastError);
  
  if ((bRet == FALSE) && (dwLastError == ERROR_INSUFFICIENT_BUFFER))
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      return (FALSE);
    }

}


/*
 * force failure by setting outBufSize to one less than is needed
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufSizeOneLess()
{
  /* assumption check */
  PREFAST_SUPPRESS (326, "Intended");
  if (UUID_LENGTH_B == 0)
    {
      IntErr (_T("UUID_LENGTH_B not set correctly"));
      return (FALSE);
    }

  /* make this one less */
  byte buffer[UUID_LENGTH_B - 1];

  BOOL bRet;

  DWORD junk;

  DWORD dwLastError;

  /* make call */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0,
	     buffer, sizeof (buffer), &junk, &bRet, &dwLastError);
  
  if ((bRet == FALSE) && (dwLastError == ERROR_INSUFFICIENT_BUFFER))
    {
      Info (_T("%S succeeded"), __FUNCTION__);
      return (TRUE);
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      return (FALSE);
    }

}


/*
 * general test case.  Tests operation of the function (and a good set
 * of example code).
 */
BOOL
testIOCTL_HAL_GET_UUID_Working_outSizeOneGreater()
{
  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;
  
  byte buffer1[UUID_LENGTH_B];
  byte buffer2[UUID_LENGTH_B + 1];
  BOOL bRet;

  DWORD dwLastError;

  /* get the version with normal size */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, 
	     buffer1, sizeof (buffer1), NULL, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* it worked */
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s"),
	     bRet, errToStr (dwLastError));
      goto cleanAndReturn;
    }

  DWORD checkSize;

  /* get the version with one bigger */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, 
	     buffer2, sizeof (buffer2), &checkSize, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* it worked */
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("In test part of %S"), __FUNCTION__);
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("In test part of %S"), __FUNCTION__);
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s"),
	     bRet, errToStr (dwLastError));
      goto cleanAndReturn;
    }

  if (checkSize != UUID_LENGTH_B)
    {
      Error (_T("Structure size returned by ioctl not correct.  %u != %u"),
	     checkSize, UUID_LENGTH_B);
      goto cleanAndReturn;
    }

  /* compare two structures */
  if (memcmp (buffer1, buffer2, UUID_LENGTH_B) != 0)
    {
      Error (_T("Buffers are not equal.  normal version:"));
      dumpUUIDErr (buffer1);
      Error (_T("size is one greater version:"));
      dumpUUIDErr (buffer2);
      goto cleanAndReturn;
    }
      
  returnVal = TRUE;

 cleanAndReturn:
  
  if (returnVal != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED"), __FUNCTION__);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
    }
  
  return (returnVal);

}

/*
 * force failure by setting outBuf to NULL.
 */
BOOL
testIOCTL_HAL_GET_UUID_ForceFailure_OutBufIsNull()
{
  return (itShouldBreakWhenOutBufIsNull(IOCTL_HAL_GET_UUID, NULL, 0,
					UUID_LENGTH_B));
}

/**************************************************************************
 *
 *                             Alignment checks
 *
 **************************************************************************/

/*
 * makes sure that the function correctly writes into buffers that
 * aren't DWORD aligned.
 */
BOOL
testIOCTL_HAL_GET_UUID_Working_Alignment(DWORD dwAlignOffset)
{
  if (dwAlignOffset >= sizeof (DWORD))
    {
      IntErr (_T("(dwAlignOffset > 3)"));
      return (FALSE);
    }

  /* assumption */
  PREFAST_SUPPRESS (326, "Intended");
  if (UUID_LENGTH_B + sizeof (DWORD) <= MAX(UUID_LENGTH_B, sizeof (DWORD)))
    {
      IntErr (_T("Overflow UUID_LENGTH_B + sizeof (DWORD)"));
      return (FALSE);
    }

  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;
  
  byte buffer1[UUID_LENGTH_B];

  /* 
   * We want to hit the alignments within a DWORD, so add a DWORD's
   * worth of bytes to the end
   */
  byte buffer2[UUID_LENGTH_B + sizeof (DWORD)];
  BOOL bRet;

  byte * misAlignedBuffer = buffer2 + dwAlignOffset;

  DWORD dwLastError;

  /* get the version with normal alignment */
  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, 
	     buffer1, UUID_LENGTH_B, NULL, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* it worked */
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("In init part of %S"), __FUNCTION__);
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s"),
	     bRet, errToStr (dwLastError));
      goto cleanAndReturn;
    }

  wrapIoctl (IOCTL_HAL_GET_UUID, NULL, 0, 
	     misAlignedBuffer, UUID_LENGTH_B, NULL, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* it worked */
    }
  else if ((dwLastError == ERROR_NOT_SUPPORTED) && (bRet == FALSE))
    {
      Error (_T("In test part of %S"), __FUNCTION__);
      Error (_T("Appears that IOCTL_HAL_GET_UUID is not supported.  Please"));
      Error (_T("disable this test if this is the case."));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Error (_T("In test part of %S"), __FUNCTION__);
      Error (_T("Something went wrong when calling IOCTL_HAL_GET_UUID."));
      Error (_T("We received and return value of %u and a GetLastError of %s"),
	     bRet, errToStr (dwLastError));
      goto cleanAndReturn;
    }

  /* compare two structures */
  if (memcmp (buffer1, misAlignedBuffer, UUID_LENGTH_B) != 0)
    {
      Error (_T("Buffers are not equal.  normal version:"));
      dumpUUIDErr (buffer1);
      Error (_T("size is one greater version:"));
      dumpUUIDErr (misAlignedBuffer);
      goto cleanAndReturn;
    }
      
  returnVal = TRUE;

 cleanAndReturn:
  
  if (returnVal != TRUE)
    {
      /* test failed */
      Error (_T("%S FAILED"), __FUNCTION__);
    }
  else
    {
      Info (_T("%S succeeded"), __FUNCTION__);
    }
  
  return (returnVal);

}


