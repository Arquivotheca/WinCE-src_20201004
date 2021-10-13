// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 1996-2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#ifndef __TUXMAIN_H__
#define __TUXMAIN_H__
#include <tapi.h>

#define countof(a) (sizeof(a)/sizeof(*(a)))

// Test function prototypes (TestProc's)
TESTPROCAPI DefaultTest          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
struct LineInfoStr
	{	DWORD dwDeviceID;									 /* TAPI device number */
		HLINE hLine;												/* line handle */
		HCALL hCall;							 /* call handle (NULL if inactive) */
		DWORD dwStatus;												/* call status */
		DWORD dwTime; };							  /* length of call (milliseconds) */
#define MAX_OPEN_LINES   10						   /* maximum number of open lines */
#define RUN_FOREVER      0xFFFFFFFF
#define INVALID_LINE_ID  0xFFFFFFFF
#define MAX_COMMAND_LINE_ELEM	64	
#define MAX_COMMAND_LINE_LEN	256
#define NO_OPTION  0xFFFFFFFF


// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("Default Test"			 ),				0,   0,              0, NULL,
   TEXT(   "Default Test"		 ),				1,   0,			   100, DefaultTest,
   NULL,										0,   0,              0, NULL  // marks end of list
};

#endif // __TUXMAIN_H__
