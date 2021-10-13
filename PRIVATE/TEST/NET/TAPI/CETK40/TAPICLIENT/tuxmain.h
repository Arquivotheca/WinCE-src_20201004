// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 1996-2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#ifndef __TUXDEMO_H__
#define __TUXDEMO_H__
#include <tapi.h>

#define countof(a) (sizeof(a)/sizeof(*(a)))

#define _FILENAM  "TapiClient"
#define _FILEVER  " V2.0 "
#define _TESTDESCR	"- TAPI \"lineMakeCall\" Test Program"
#define DEFAULT_THREAD_COUNT	0
#define MAX_COMMAND_LINE_ELEM	64	
#define MAX_COMMAND_LINE_LEN	256
#define NO_OPTION  0xFFFFFFFF

//------------------------------------------------------------------------
//	Global constants and variables:
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

extern DWORD dwNumCalls;
extern DWORD dwDeviceID;
extern DWORD dwBearerMode;
extern DWORD dwMediaMode;
extern DWORD dwBaudrate;
extern DWORD dwHangupTimeout;
extern DWORD dwDeallocateDelay;			  /* timeout before/after "lineDrop" reply */
extern BOOL bEditDevConfig;
extern BOOL bListAllDevices;
extern TCHAR szPhoneNumber[120];					/* buffer for telephone number */

#ifdef UNDER_CE
	 /* CE Unimodem dials the call before sending the LINE_REPLY to "lineMakeCall" */
extern DWORD dwDialingTimeout;
extern DWORD dwConnectionDelay;
#else
					 /* NT Unimodem sends LINE_REPLY first and then dials the call */
extern DWORD dwDialingTimeout;
extern DWORD dwConnectionDelay;
#endif


// Test function prototypes (TestProc's)
TESTPROCAPI DefaultTest         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ListDevices         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

static DWORD modes[][2] = { {LINEBEARERMODE_VOICE, LINEMEDIAMODE_DATAMODEM},
							{LINEBEARERMODE_DATA, LINEMEDIAMODE_DATAMODEM},
							};

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("Tapi Tests"                ),			0,				  0,	0,			NULL,
   TEXT(   "List Devices Test"		),			1,				  0,	100,			ListDevices,
   TEXT(   "VOICE-DATAMODEM Test"   ),			1,	(DWORD)modes[0],	200,			DefaultTest,
   TEXT(   "DATA-DATAMODEM Test"    ),			1,	(DWORD)modes[1],	201,			DefaultTest,
   NULL,										0,				  0,	0,			NULL  // marks end of list
};

#endif // __TUXDEMO_H__
