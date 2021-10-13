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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef _POWER_TEST
  //headers
	#include <windows.h>
  #include <diskio.h>
  #include <atapi.h>
  #include <tchar.h>
  #include <clparse.h>
  #include <fatfspowerutil.h>
  #include <devutils.h>
  #include <storemgr.h>
  #include <storehlp.h>
  //defines
  #define Debug NKDbgPrintfW
  
	#define _POWER_TEST
	#define TIMER_LENGTH 600000
	#define WRITE_LARGE 1
	#define READ_LARGE 2
	static bool ProcessCmdLine(LPCTSTR szCmdLine);
	static void Usage();
	BOOL TestReadWritePower(void);
	
	typedef enum
	 {
	   READ=0,
	   WRITE=1,
	   READWRITE=2,
	   UNKNOWN
	 }TEST_TYPE;
	 
	 typedef struct
	 {
	     TEST_TYPE testType;
	     LPTSTR str;
	 }TEST_TYPE_LOOKUP;
#endif