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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	Common.h

Abstract:

    Header file for common data structure and functions

--*/

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
#define	TEST_MAX_CARDS		32

#define  IO_START_ADDRESS		0x4000

//client data definition
typedef struct  {
	UINT				uClientID;
	CARD_CLIENT_HANDLE	hClient;
	HANDLE  				hEvent;
     	UINT32  				uEventGot;
     	UINT32  				fEventGot;
   	UINT32  				uCardReqStatus;
	UINT32				uReqReturn;
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
BOOL NormalRequestConfig(CARD_CLIENT_HANDLE hClient, UINT8 uSock, UINT8 uFunc);
BOOL RetrieveRegisterValues(PCARD_CONFIG_INFO pConfInfo, CARD_SOCKET_HANDLE hSocket);


//tuple test related definitions, structures and function prototypes

#define CARD_IO  1            // Required for I/O cards
#define CARD_MEMORY 2         // Required for memory cards
#define CARD_IO_RECOMMENDED 4 // Recomended for I/O cards, note required bits are clear.

#define NUM_EXT_REGISTERS 		4
#define NUM_REGISTERS			5

#define TEST_IDLE_TIME	5000   //  5 sec
#define TOTAL_VALID_TUPLECODES		31

typedef struct 
{
    TCHAR *text ; 
    unsigned int index ; 
    int cardTypesSupportingTuple ;
} CISStrings ;

//Globals that will be used through out the whole test module
extern UINT EventMasks[MAX_EVENT_MASK];
extern EVENT_NAME_TBL v_EventNames[];
extern UINT8   rgWinSpeed[MAX_WIN_SPEED];
extern UINT16   rgWinAttr[MAX_WIN_ATTR];
extern CLIENT_CONTEXT client_data;
extern TCHAR *RtnCodes [];
extern CISStrings TupleCodes [];
extern UINT 	LastError;
extern USHORT	g_Vcc;
extern int  nThreadsDone   ;
extern BOOL	gfSomeoneGotExclusive;
extern SOCKET_DESCRIPTOR	g_SocketDescs[MAX_SOCKETS];
extern CARD_DESCRIPTOR		g_CardDescs[MAX_SOCKETS*2];
extern DWORD		g_dwTotalSockets;
extern DWORD		g_dwTotalCards;

#endif
