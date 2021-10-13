//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _COMMON_H_
#define _COMMON_H_
#include "TestMain.h"

//debug output
#ifdef DEBUG
#define NKDMSG	NKDbgPrintfW
#else
#define NKDMSG
#endif

#define NKMSG	NKDbgPrintfW

#define 	TST_NO_ERROR                  0
#define 	TST_SEV1                        	2
#define 	TST_SEV2                        	3
#define 	TST_SEV3                        	4
#define 	TST_THREAD_ERROR        	5

#define 	UNDEFINED_VALUE		0xFF
#define	TEST_WAIT_TIME			1000  // 1 sec.

//client data definition
typedef struct  {
	LPTSTR 				lpClient;
	int    				nClient;
	DWORD				ThreadNo;
    	CARD_CLIENT_HANDLE 	hClient;
	HANDLE  				hEvent;
	PVOID   				pWin;
    	UINT32  				uGranularity;
	UINT    				uStreamLen;
	BYTE				rgByteStream[64];
    	BOOL    				fCardReqExclDone;
     	UINT32  				uEventGot;
     	UINT32				fEventGot;
    	UINT8   				fCardInserted;
    	UINT8				fCardRemoved;
   	UINT32  				uCardReqStatus;
    	UINT32  				uReqReturn;
     	UINT32  				uCardExpStatus;
} CLIENT_CONTEXT, *PCLIENT_CONTEXT;

//function prototype
STATUS CallBackFn_Client(
    CARD_EVENT EventCode,
    CARD_SOCKET_HANDLE hSocket,
    PCARD_EVENT_PARMS pParms
    );
LPTSTR  FindEventName( CARD_EVENT EventCode);
LPTSTR  FindStatusName(STATUS StatusCode);
void updateError (UINT n);

//tuple test related definitions, structures and function prototypes

#define CARD_IO  1            // Required for I/O cards
#define CARD_MEMORY 2         // Required for memory cards
#define CARD_IO_RECOMMENDED 4 // Recomended for I/O cards, note required bits are clear.

#define MAX_MY_STREAM        	5

typedef struct 
{
    TCHAR *text ; 
    unsigned int index ; 
    int cardTypesSupportingTuple ;
} CISStrings ;

typedef struct 
{
	UINT   uStreamLen;
	CHAR  * rgByteCmd;
} STREAMTYPE;


//Globals that will be used through out the whole test module
extern UINT EventMasks[MAX_EVENT_MASK];
extern EVENT_NAME_TBL v_EventNames[];
extern UINT8   rgWinSpeed[MAX_WIN_SPEED];
extern UINT16   rgWinAttr[MAX_WIN_ATTR];
extern STREAMTYPE   rgStreamType[MAX_MY_STREAM];
extern CLIENT_CONTEXT client_data;
extern TCHAR *RtnCodes [];
extern CISStrings TupleCodes [];
extern UINT 	LastError;
extern USHORT	g_Vcc;
extern int  nThreadsDone   ;
extern BOOL	gfSomeoneGotExclusive;

#endif
