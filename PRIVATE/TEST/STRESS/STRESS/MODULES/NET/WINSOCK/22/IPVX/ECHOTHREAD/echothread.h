//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __ECHOTHREAD_H_
#define __ECHOTHREAD_H_

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "stressutils.h"

#define ALL_SOCK_TYPES			-1
#define NUM_SOCKTYPES			2

typedef struct ECHOTHREADPARMS {
	int nFamily;
	int nSockType;
	int nMaxDataXferSize;
	BOOL fBlackholeMode;
	LPSTR szPortASCII;
} EchoThreadParms;

DWORD WINAPI ServerThreadProc (LPVOID pv);

// External global variable to indicate server thread should exit
extern HANDLE				g_hTimesUp;
extern HANDLE				g_hEchoThreadRdy;

#endif // __ECHOTHREAD_H_