//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
#ifndef _TESTMAIN_HEADER
#define _TESTMAIN_HEADER

//#pragma warning(disable: 4244) //suppress unnessecary warning messages when using warning level 4

//included header files
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <ceddk.h> 
#include <hw16550.h>
#include <types.h>
//extern "C"{
#include <cardserv.h>
//}
#include <tuple.h>
#include <string.h>
#include <memory.h>
#include <nkintr.h>
#include <sockserv.h>
#include <pcmfuncs.h>
#include "ddlxioct.h"

////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

////////////////////////////////////////////////////////////////////////////////


//settings
#define MAX_NAME_LENGTH   					64
#define MAX_EVENTS         					5
#define MAX_SOCKETS						2
#define EX_STATE_UNOWNED    				0
#define EX_STATE_REQUESTING 				1
#define EX_STATE_ALMOST     					2
#define EX_STATE_OWNED      					3
#define EX_STATE_RELEASING  				4

#define	MAX_STRINGLEN						255

//event mask-related settings
#define MAX_EVENT_MASK 					11
#define BASE_EVENT_MASK 					0x02FF  
#define CARD_DETECTED_MASK				0x80

#define LAST_EVENT_CODE ((CARD_EVENT) -1)

typedef struct _EVENT_NAME_TBL {
    CARD_EVENT EventCode;
    LPTSTR    pEventName;
} EVENT_NAME_TBL, *PEVENT_NAME_TBL;



//window mapping settings.
#define MAX_WIN_SPEED  			35
#define MAX_WIN_ATTR  			30

//Error messages
#define MAX_BAD_SOCKET 					3
#define TST_INIT_RATS_FAILED              		0
#define TST_CREATE_LOG_EVENT_FAILED       	1
#define TST_CREATE_LOG_THREAD_FAILED      	2
#define TST_IN_THREAD_FAILED		         	3
#define TST_CONFIG_FAILED  					4
#define TST_CONFIG_MASSIVE_FAILED         	5

//Test groups
#define TSTGROUP_CONFIG					1
#define TSTGROUP_WINDOW					2
#define TSTGROUP_INTERRUPT					3
#define TSTGROUP_MASK						4
#define TSTGROUP_EXECLUSIVE				5
#define TSTGROUP_TUPLE						6

//Test function entries
TESTPROCAPI TestDispatchEntry(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
VOID ProcessCmdLine(LPCTSTR	szCmdLine);

BOOL TestSetup(USHORT uSocket, USHORT uFunc);
BOOL  UpdateRegistry(VOID);
BOOL ResetRegistry(VOID);

#ifdef DEBUG

#define DBG_ERROR      1
#define DBG_WARNING    2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_VERBOSE   16
#define DBG_IOCTL     32
// Debug zone defs
#define 	ZONE_ERROR			DEBUGZONE(0)
#define 	ZONE_WARNING		DEBUGZONE(1)
#define	ZONE_FUNCTION		DEBUGZONE(2)
#define 	ZONE_INIT			DEBUGZONE(4)
#define 	ZONE_IOCTL			DEBUGZONE(8)
#define	ZONE_VERBOSE		DEBUGZONE(16)
#endif


//Globals that will be used through out the whold test dll
extern DDLXKato_Talk* g_pKato;
extern SPS_SHELL_INFO *g_pShellInfo;
extern CRITICAL_SECTION g_csProcess;
extern HINSTANCE g_hInst;
extern   USHORT	uSocket;
extern   USHORT 	uFunction;
extern  DWORD	dwTotalThread;
extern  BOOL		bRegUpdated;
extern  BOOL		bCardNotInSlot;
extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif

