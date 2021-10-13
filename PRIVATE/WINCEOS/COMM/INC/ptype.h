//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
