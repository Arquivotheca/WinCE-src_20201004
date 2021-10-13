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