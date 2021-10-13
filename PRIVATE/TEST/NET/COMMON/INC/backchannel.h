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
#ifndef __BACKCHANNEL_H__
#define __BACKCHANNEL_H__

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "cmnprint.h"
#include "netcmn.h"

#ifndef countof
#define countof(x)					(sizeof(x) / sizeof(x[0]))
#endif

// This is the only reserved CommandId value
#define COMMAND_STOP_SERVICE			0

#define STOP_SERVICE_POLL_FREQ_SECS		5

#ifndef MAX_COMMAND_DATA_SIZE			
#define MAX_COMMAND_DATA_SIZE			100 * 1024			// 100K
#endif

#ifndef MAX_RESULT_DATA_SIZE
#define MAX_RESULT_DATA_SIZE			100 * 1024			// 100K
#endif

typedef BOOL (*LPSTARTUP_CALLBACK)(SOCKET sock);
typedef BOOL (*LPCLEANUP_CALLBACK)(SOCKET sock);

typedef struct __COMMAND_PACKET_HEADER__ {
	DWORD dwCommand;
	DWORD dwDataSize;
} COMMAND_PACKET_HEADER;

typedef struct __RESULT_PACKET_HEADER__ {
	DWORD dwResult;
	DWORD dwDataSize;
} RESULT_PACKET_HEADER;

typedef struct __START_SERVICE_PARMS__ {
	DWORD dwTimeout;
	LPSTARTUP_CALLBACK pfnStartupFunc;
	LPCLEANUP_CALLBACK pfnCleanupFunc;
} START_SERVICE_PARMS;

typedef struct __SERVER_THREAD_DATA__ {
	HANDLE hStopService;
	int nNumSocks;
	LPSTARTUP_CALLBACK pfnStartupFunc;
	LPCLEANUP_CALLBACK pfnCleanupFunc;
	SOCKET SockServ[FD_SETSIZE];
} SERVER_THREAD_DATA;

typedef struct __WORKER_THREAD_DATA__ {
	HANDLE hStopService;
	SOCKET sock;
	LPSTARTUP_CALLBACK pfnStartupFunc;
	LPCLEANUP_CALLBACK pfnCleanupFunc;
} WORKER_THREAD_DATA;

typedef DWORD (*HANDLER_PROC)(SOCKET sock, DWORD dwCommand, BYTE *pbCommandData, DWORD dwCommandDataSize, BYTE **ppbReturnData, DWORD *pdwReturnDataSize);

typedef struct __COMMAND_HANDLER_ENTRY__ {
	LPTSTR tszHandlerName;
	DWORD dwCommand;
	HANDLER_PROC pfHandlerFunction;
} COMMAND_HANDLER_ENTRY;

extern COMMAND_HANDLER_ENTRY g_CommandHandlerArray[];
extern INT g_CommandTimeoutSecs;
extern INT g_ResultTimeoutSecs;

// Function Prototypes
SOCKET BackChannel_ConnectEx(LPSTR szServer, LPSTR szPort, int iFamily);
BOOL BackChannel_SendCommand(SOCKET sock, DWORD dwCommand, BYTE *pbCommandData, DWORD dwCommandDataSize);
BOOL BackChannel_GetResult(SOCKET sock, DWORD *pdwResult, BYTE **ppbReturnData, DWORD *pdwReturnDataSize);
BOOL BackChannel_Disconnect(SOCKET sock);
HANDLE BackChannel_StartService(
								LPSTR szPort, 
								LPSTARTUP_CALLBACK pfnStartupFunc = NULL, 
								LPCLEANUP_CALLBACK pfnCleanupFunc = NULL,
								HANDLE *phServiceThread = NULL);

inline SOCKET BackChannel_Connect(LPSTR szServer, LPSTR szPort)
{ return BackChannel_ConnectEx(szServer, szPort, PF_UNSPEC); }

#endif // __BACKCHANNEL_H__
