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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#define HINSTANCE_ERROR	((HINSTANCE)32)		// from windows.h
#define HFILE_ERROR		((HFILE)-1)			// from windows.h

#define HFILE			int
#define time_t			int

#define PLUID			__int64

#define WSOCK_DEVICE_ID	0x003e				// from netvxd.h


FARPROC WSASetBlockingHook(FARPROC lpBlockFunc);	// from wsamisc.c
int WSACancelBlockingCall( VOID );


#include "cclib.h"	// the C library functions...
