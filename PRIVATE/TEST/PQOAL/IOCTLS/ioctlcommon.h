//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * ioctlCommon.h
 */

#include <windows.h>
#include "..\common\commonUtils.h"

/*
 * common code and declarations used by the ioctl code.
 */


/*
 * Allows us to wrap an ioctl call with a SetLastError and
 * GetLastError to track error value reporting.  Saves typing.
 *
 * First size parameters are the same as KernelIoControl.  bRet is the
 * return value from KernelIoControl and dwLastError is the
 * GetLastError return value.
 *
 * Before the call is made the system error value is set to
 * MY_ERROR_JUNK.  If you get this back then the ioctl didn't set
 * GetLastError.
 */
void wrapIoctl( 
	       DWORD dwIoControlCode, 
	       LPVOID lpInBuf, 
	       DWORD nInBufSize, 
	       LPVOID lpOutBuf, 
	       DWORD nOutBufSize, 
	       LPDWORD lpBytesReturned,
	       BOOL * bRet,
	       DWORD * dwLastError
	       );



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
errToStr (DWORD dwError, TCHAR * szStr, DWORD dwBufSize);

#define ERR_STR_LENGTH 20
#define MY_ERROR_NOT_SET MY_ERROR_JUNK

extern TCHAR g_LastErrorStr[ERR_STR_LENGTH];

/*
 * Same as the three parameter version, except that it writes its data into
 * a global buffer.
 */
TCHAR *
errToStr (DWORD dwError);

void
trashMemory (void * mem, DWORD dwSize);


/*****************************************************************************
 *
 *                           General Test Functions 
 *
 *****************************************************************************/

BOOL
itShouldBreakWhenInBufIsNull(DWORD ioctlCall, DWORD dwInBufSizeBytes, 
			     DWORD dwOutBufferSizeBytes);

BOOL
itShouldBreakWhenInBufIsNotNull(DWORD ioctlCall, DWORD dwInBufSizeBytes, 
				DWORD dwOutBufferSizeBytes);

BOOL
itShouldBreakWhenInBufSizeIsX (DWORD ioctlCall, DWORD * inBuffer, 
			       DWORD dwInBufSizeBytes, DWORD dwOutBufferSizeBytes);

BOOL
itShouldBreakWhenOutBufSizeIsTooSmall (DWORD ioctlCall, 
				       void * inBuf, DWORD inBufSize, 
				       DWORD dwCorrectSize, DWORD dwOutSize);

BOOL
itShouldBreakWhenOutBufIsNull (DWORD ioctlCall, 
			       void * inBuf, DWORD inBufSize, 
			       DWORD dwOutSize);

BOOL
ShouldSucceedWithNoLpBytesReturned(DWORD ioctlCall,
				   void * inBoundBufControl,
				   void * inBoundBufTest,
				   DWORD inBoundBufSize,
				   void * outBoundBufControl,
				   void * outBoundBufTest,
				   DWORD outBoundBufSize,
				   void (*printStruct)(void * buf, 
						       DWORD dwBufSize));
BOOL
compareTwoIoctlCallInstances(DWORD ioctlCall,
			     void * inBoundBufControl,
			     void * inBoundBufTest,
			     DWORD inBoundBufSize,
			     void * outBoundBufControl,
			     void * outBoundBufTest,
			     DWORD outBoundBufSize,
			      void (*printStruct)(void * buf, 
						       DWORD dwBufSize));

