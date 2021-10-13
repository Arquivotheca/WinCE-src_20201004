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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <stressutils.h>

#define WINSOCK_VERSION_REQ		MAKEWORD(2,2)

#define MODULE_NAME				_T("wsx_conn")  // Module name used in logging and for
												// global named objects

#define STATUS_FREQUENCY		1000		// print out our status every time this number
											// of iterations has been performed

#define DEFAULT_WAITTIME		10000		// Default time to wait (in ms) between binds.
											// This is used to lessen the chance that
											// multiple, concurrent tests will
											// interfere with one another and to prevent
											// the test from cycling thru the ports too
											// quickly (WSAEADDRINUSE).

#define DEFAULT_FAMILY			PF_UNSPEC	// Set this to PF_INET for v4 only,
											// PF_INET6 for v6 only
#define DATA_XFER_SIZE			1024		// size of data (in bytes) to echo
											// off the server
#define DEFAULT_THREADS			4			// Default numbers of threads to run
#define DEFAULT_SERVER			_T("localhost")
#define DEFAULT_PORT			_T("12678")	// Defines the default port to 'connect' to

#define MAX_ADDRS				100			// Maximum number of addresses to choose
											// from for connection purposes

#define TIMEOUT_MSECS			1000		// Time (in milliseconds) to wait to send or
											// receive data before giving up

#define THREAD_WAIT_INTERVAL	(60 * 1000)


#define SZ_AI_FAMILY	((pAI->ai_family == PF_INET) ? _T("PF_INET") : \
						((pAI->ai_family == PF_INET6) ? _T("PF_INET6") : \
						_T("UNKNOWN")))
#define SZ_AI_TYPE		((nSockType == SOCK_STREAM) ? _T("SOCK_STREAM") : \
						((nSockType == SOCK_DGRAM) ? _T("SOCK_DGRAM") : \
						_T("UNKNOWN")))