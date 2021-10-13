//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * ioctlCommon.cpp
 */

#include "ioctlCommon.h"

/*
 * see header for more information
 */
void wrapIoctl( 
	       DWORD dwIoControlCode, 
	       LPVOID lpInBuf, 
	       DWORD nInBufSize, 
	       LPVOID lpOutBuf, 
	       DWORD nOutBufSize, 
	       LPDWORD lpBytesReturned,
	       BOOL * bRet,
	       DWORD * dwLastError)
{
  SetLastError (MY_ERROR_JUNK);

  *bRet = KernelIoControl (dwIoControlCode, lpInBuf, nInBufSize,
			   lpOutBuf, nOutBufSize, lpBytesReturned);

  *dwLastError = GetLastError();

}

/*
 * convert an error into a str
 *
 * dwError is the error valid.  It can be anything.
 * 
 * szStr is the place to put the converted string.  It has length
 * dwBufSize, including the null terminator.
 *
 * The return value is null if szStr is null or dwBufSize is zero, or
 * it is szStr in all other cases.
 *
 * The output is always null terminated.  It is truncated if it won't
 * fit, including the null terminator.
 */
TCHAR *
errToStr (DWORD dwError, TCHAR * szStr, DWORD dwBufLength)
{
  if ((szStr == NULL) || (dwBufLength == 0))
    {
      return (NULL);
    }

  if (dwError == MY_ERROR_NOT_SET)
    {
      PREFAST_SUPPRESS (419, "By design");
      _tcsncpy (szStr, _T("NOT SET"), dwBufLength);
    }
  else
    {
      _sntprintf (szStr, dwBufLength, _T("%u"), dwError);
    }

  /* force null term on end no matter above functions do */
  szStr[dwBufLength - 1] = 0;

  return (szStr);
}

/* global to handle output below */
TCHAR g_LastErrorStr[ERR_STR_LENGTH];

/*
 * Same as the three parameter version, except that it writes its data into
 * a global buffer.
 */
TCHAR *
errToStr (DWORD dwError)
{
  return (errToStr (dwError, g_LastErrorStr, ERR_STR_LENGTH));
}


void
trashMemory (void * mem, DWORD dwSize)
{
  PREFAST_SUPPRESS (419, "memset should handle bad args");
  memset (mem, BAD_VAL, dwSize);
}


/*****************************************************************************
 *
 *                           General Test Functions 
 *
 *****************************************************************************/

/*
 * set inBuf to null and see if the function breaks.
 *
 * ioctlCall is the ioctl call to make.  dwInBufSizeBytes is the
 * inbufSize paramter to the ioctl call.  dwOutBufferSizeBytes is the
 * size of the buffer to allocate and send into the outbuf part of the
 * call.  The size of this buffer is correctly sent in as well.
 *
 * Returns TRUE if the ioctl call correctly fails and false if it does
 * not.
 */
BOOL
itShouldBreakWhenInBufIsNull(DWORD ioctlCall, DWORD dwInBufSizeBytes, 
			     DWORD dwOutBufferSizeBytes)
{
  BOOL returnVal = FALSE;

  byte * outBuffer = NULL;

  if (dwOutBufferSizeBytes != 0)
    {
      PREFAST_SUPPRESS (419, "By design");
      outBuffer = (byte *) malloc (dwOutBufferSizeBytes);
      
      if (outBuffer == NULL)
	{
	  Error (_T("Couldn't allocate %u bytes for test."), 
		 dwOutBufferSizeBytes);
	  goto cleanAndReturn;
	}
    }

  BOOL bRet;
  DWORD dwLastError;

  /* make call */
  wrapIoctl (ioctlCall, NULL, dwInBufSizeBytes, 
	     &outBuffer, dwOutBufferSizeBytes, NULL, 
	     &bRet, &dwLastError);

  if (bRet == FALSE)
    {
      /* we wanted it to break */
      returnVal = TRUE;
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      returnVal = FALSE;
    }

 cleanAndReturn:

  if (outBuffer != NULL)
    {
      free (outBuffer);
    }

  return (returnVal);
}


/*
 * set inBuf to not null and see if the function breaks.
 *
 * ioctlCall is the ioctl call to make.  dwInBufSizeBytes is the
 * inbufSize paramter to the ioctl call.  dwOutBufferSizeBytes is the
 * size of the buffer to allocate and send into the outbuf part of the
 * call.  The size of this buffer is correctly sent in as well.
 *
 * Returns TRUE if the ioctl call correctly fails and false if it does
 * not.
 */
BOOL
itShouldBreakWhenInBufIsNotNull(DWORD ioctlCall, DWORD dwInBufSizeBytes, 
				DWORD dwOutBufferSizeBytes)
{
  BOOL returnVal = FALSE;

  byte * outBuffer = NULL;

  if (dwOutBufferSizeBytes != 0)
    {
      PREFAST_SUPPRESS (419, "By design");
      outBuffer = (byte *) malloc (dwOutBufferSizeBytes);
      
      if (outBuffer == NULL)
	{
	  Error (_T("Couldn't allocate %u bytes for test."), 
		 dwOutBufferSizeBytes);
	  goto cleanAndReturn;
	}
    }

  DWORD inboundVal;

  BOOL bRet;
  DWORD dwLastError;

  /* make call */
  wrapIoctl (ioctlCall, &inboundVal, dwInBufSizeBytes, 
	     &outBuffer, dwOutBufferSizeBytes, NULL, 
	     &bRet, &dwLastError);

  if (bRet == FALSE)
    {
      /* we wanted it to break */
      returnVal = TRUE;
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      returnVal = FALSE;
    }

 cleanAndReturn:

  if (outBuffer != NULL)
    {
      free (outBuffer);
    }

  return (returnVal);
}
  
/*
 * Use this in a case in which you want to make sure that the ioctl
 * fails when the inBufferSize is a given value.  The user is charged
 * with giving the inBuffer that will passed in.  This value can be
 * null if the user so desires.
 *
 * ioctlCall is the ioctl to call.  Inbuffer is the input buffer to
 * the ioctl.  This is passed directly into the ioctl.
 * dwInBufSizeBytes is the inbound size to the ioctl.
 * dwOutBufferSizeBytes is the output buffer size (which is allocated
 * for the buffer parameter.
 *
 * return true is the ioctl successfully broke (returned false) and
 * false if the ioctl succeeded.
 */

BOOL
itShouldBreakWhenInBufSizeIsX (DWORD ioctlCall, DWORD * inBuffer, 
			     DWORD dwInBufSizeBytes, DWORD dwOutBufferSizeBytes)
{
  BOOL returnVal = FALSE;

  /* get outbound buffer */
  byte * outBuffer = NULL;

  if (dwOutBufferSizeBytes != 0)
    {
      PREFAST_SUPPRESS (419, "By design");
      outBuffer = (byte *) malloc (dwOutBufferSizeBytes);
      
      if (outBuffer == NULL)
	{
	  Error (_T("Couldn't allocate %u bytes for outbound buffer."), 
		 dwOutBufferSizeBytes);
	  goto cleanAndReturn;
	}
    }

  BOOL bRet;
  DWORD dwLastError;

  /* make call */
  wrapIoctl (ioctlCall, inBuffer, dwInBufSizeBytes, 
	     &outBuffer, dwOutBufferSizeBytes, NULL, 
	     &bRet, &dwLastError);

  if (bRet == FALSE)
    {
      /* we wanted it to break */
      returnVal = TRUE;
    }
  else
    {
      /* test failed */
      Error (_T("%S FAILED: bRet = %u  GetLastError = %s"),
	     __FUNCTION__, bRet, errToStr (dwLastError));
      returnVal = FALSE;
    }

 cleanAndReturn:

  if (outBuffer != NULL)
    {
      free (outBuffer);
    }

  return (returnVal);
}

/*
 * make sure that the ioctl breaks when a size is passed into the
 * outBufSize that is too small.
 *
 * This function allocates dwCorrectSize of memory for the output
 * buffer.  Set this amount correctly so that you won't crash if the
 * function under test is wrong.
 *
 * return TRUE if the ioctl behaves correctly (ie. fails) and false if
 * it does not.
 */
BOOL
itShouldBreakWhenOutBufSizeIsTooSmall (DWORD ioctlCall, 
				       void * inBuf, DWORD inBufSize, 
				       DWORD dwCorrectSize, DWORD dwOutSize)
{
  BOOL returnVal = FALSE;

  if ((dwOutSize >= dwCorrectSize) || (dwCorrectSize == 0))
    {
      Error (_T("Invalid parameters"));
      goto cleanAndReturn;
    }

  /* get memory */
  PREFAST_SUPPRESS (419, "validated above");
  byte * buffer = (byte *) malloc (dwCorrectSize);

  if (buffer == NULL)
    {
      Error (_T("Couldn't malloc %u bytes."), dwCorrectSize);
      goto cleanAndReturn;
    }

  /* potentially makes easier to find errors */
  trashMemory (buffer, dwCorrectSize);

  BOOL bRet;
  DWORD dwReturnedBytes;
  DWORD dwLastError;

  /* query for the buffer */
  wrapIoctl (ioctlCall, inBuf, inBufSize,
	     buffer, dwOutSize,
	     &dwReturnedBytes, &bRet, &dwLastError);

  returnVal = TRUE;

 cleanAndReturn:

  if ((bRet == FALSE) && (returnVal == TRUE))
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
 * Send in null as outBuf to the function.  ioctl should return false
 * in this case.
 *
 * ioctlCall is the ioctl call to be made.  Inbuf and inBufSize are
 * just like the params to the ioctl.
 *
 * return true when the ioctl successfully breaks (ie. return false)
 * and false if the ioctl doesn't act correctly.
 */
BOOL
itShouldBreakWhenOutBufIsNull (DWORD ioctlCall, 
			       void * inBuf, DWORD inBufSize, 
			       DWORD dwOutSize)
{
  BOOL returnVal = FALSE;
  
  BOOL bRet;
  DWORD dwReturnedBytes;
  DWORD dwLastError;

  /* query for the buffer */
  wrapIoctl (ioctlCall, inBuf, inBufSize,
	     NULL, dwOutSize,
	     &dwReturnedBytes, &bRet, &dwLastError);

  returnVal = TRUE;

  if ((bRet == FALSE) && (returnVal == TRUE))
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


BOOL
ShouldSucceedWithNoLpBytesReturned(DWORD ioctlCall,
				   void * inBoundBufControl,
				   void * inBoundBufTest,
				   DWORD inBoundBufSize,
				   void * outBoundBufControl,
				   void * outBoundBufTest,
				   DWORD outBoundBufSize,
				   void (*printStruct)(void * buf, 
						       DWORD dwBufSize))
{

  /* assume we goofed until proven otherwise */
  BOOL returnVal = FALSE;

  BOOL bRet;
  DWORD dwReturnedBytes;
  DWORD dwLastError;

  /****** get data structure with lpBytesReturned set */

  /* make the call */
  wrapIoctl (ioctlCall, inBoundBufControl, inBoundBufSize,
	     outBoundBufControl, outBoundBufSize,
	     &dwReturnedBytes, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* worked */
    }
  else if (bRet == FALSE)
    {
      Info (_T("During init for control:"));
      Error (_T("Either ioctl is not implemented or it isn't working correctly."));
      Error (_T("Got FALSE and GetLastError of %u."), errToStr (dwLastError));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Info (_T("During init for control:"));
      Error (_T("ioctl returned %u, which is neither TRUE or FALSE"));
      goto cleanAndReturn;
    }

  /* returned bytes needs to be size of data structure */
  if (dwReturnedBytes != outBoundBufSize)
    {
      Info (_T("During init for control:"));
      Error (_T("Returned bytes != to the buffer size:  %u != %u"),
	     dwReturnedBytes, outBoundBufSize);
      goto cleanAndReturn;
    }

  /****** now get data structure without sending in lpBytesReturned */

  /* make the call */
  wrapIoctl (ioctlCall, inBoundBufTest, inBoundBufSize,
	     outBoundBufTest, outBoundBufSize,
	     NULL, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* worked */
    }
  else if (bRet == FALSE)
    {
      Info (_T("During init for test:"));
      Error (_T("Either ioctl is not implemented or it isn't working correctly."));
      Error (_T("Got FALSE and GetLastError of %u."), errToStr (dwLastError));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Info (_T("During init for test:"));
      Error (_T("ioctl returned %u, which is neither TRUE or FALSE"));
      goto cleanAndReturn;
    }

  /* compare two structures */
  if (memcmp (outBoundBufTest, outBoundBufControl, outBoundBufSize) != 0)
    {
      if (printStruct == NULL)
	{
	  Error (_T("Buffers are not equal:"));
	  Error (_T("I do not know how to print out the structure values."));
	}
      else
	{
	  Error (_T("lpBytesReturned version:"));
	  (*printStruct)(outBoundBufControl, outBoundBufSize);
	  Error (_T("no lpBytesReturned version:"));
	  (*printStruct)(outBoundBufTest, outBoundBufSize);
	}
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
 * compare two calls to a given ioctl.  returns true if the results
 * are the same and false otherwise.  The calls are made with
 * lpBytesReturned set to non-NULL.  This value is checked as well.
 *
 * return true if both calls succeed and the returned structures are
 * the same and false otherwise.
 *
 * Control is the first test (assumed to be a control) and test is the
 * second test.  printStruct is a function that will print out the data
 * structures returned from this ioctl call.  It must be able to support
 * unaligned memory accesses.
 */
BOOL
compareTwoIoctlCallInstances(DWORD ioctlCall,
			     void * inBoundBufControl,
			     void * inBoundBufTest,
			     DWORD inBoundBufSize,
			     void * outBoundBufControl,
			     void * outBoundBufTest,
			     DWORD outBoundBufSize,
			     void (*printStruct)(void * buf, 
						 DWORD dwBufSize))
{

  /* assume we goofed until proven BOOL */
  BOOL returnVal = FALSE;

  DWORD dwBytesReturnedControl;
  DWORD dwBytesReturnedTest;
  BOOL bRet;
  DWORD dwLastError;

  /****** get data structure with normal alignment */

  /* make the call */
  wrapIoctl (ioctlCall, inBoundBufControl, inBoundBufSize,
	     outBoundBufControl, outBoundBufSize,
	     &dwBytesReturnedControl, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* worked */
    }
  else if (bRet == FALSE)
    {
      Info (_T("During init for control:"));
      Error (_T("Either ioctl is not implemented or it isn't working correctly."));
      Error (_T("Got FALSE and GetLastError of %u."), errToStr (dwLastError));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Info (_T("During init for control:"));
      Error (_T("ioctl returned %u, which is neither TRUE or FALSE"));
      goto cleanAndReturn;
    }

  /****** now get data structure with unaligned access */

  /* make the call */
  wrapIoctl (ioctlCall, inBoundBufTest, inBoundBufSize,
	     outBoundBufTest, outBoundBufSize,
	     &dwBytesReturnedTest, &bRet, &dwLastError);

  if (bRet == TRUE)
    {
      /* worked */
    }
  else if (bRet == FALSE)
    {
      Info (_T("During init for test:"));
      Error (_T("Either ioctl is not implemented or it isn't working correctly."));
      Error (_T("Got FALSE and GetLastError of %u."), errToStr (dwLastError));
      goto cleanAndReturn;
    }
  else
    {
      /* something is wrong */
      Info (_T("During init for test:"));
      Error (_T("ioctl returned %u, which is neither TRUE or FALSE"));
      goto cleanAndReturn;
    }

  if (dwBytesReturnedControl != dwBytesReturnedTest)
    {
      Error (_T("The bytes returned values don't match: %u != %u"),
	     dwBytesReturnedControl, dwBytesReturnedTest);
      goto cleanAndReturn;
    }

  /* compare two structures */
  if (memcmp (outBoundBufTest, outBoundBufControl, outBoundBufSize) != 0)
    {
      Error (_T("Buffers are not equal."));
      if (printStruct == NULL)
	{
	  Error (_T("Don't know how to print out data structures."));
	}
      else
	{
	  Error (_T("Control is:"));
	  (*printStruct)(outBoundBufControl, outBoundBufSize);
	  Error (_T("Test is:"));
	  (*printStruct)(outBoundBufTest, outBoundBufSize);
	}

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

