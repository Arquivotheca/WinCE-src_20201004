//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/**********************************************************************/
/**                        Microsoft Windows                         **/
/**********************************************************************/

/*
    data.h

    This file contains global variable declarations for the WinSock DLLs.


*/


#ifndef _DATA_H_
#define _DATA_H_


//
//  Socket handles.
//

extern	int			free_sockets;
extern	int			next_handle;
extern	LPSOCK_INFO	socket_handles[NUM_SOCKETS];
extern  LPSSLSOCK_CONTEXT v_rgSslSockCtxt[NUM_SOCKETS];
extern  DWORD		SockTlsSlot;
extern CRITICAL_SECTION	v_DllCS;

//
//  Open "handle" to WINSOCK.386.
//

//extern	DWORD		WinsockVxdHandle;


//
//  Debug-dependent globals.
//

#if defined(DEBUG)
//extern	DWORD		DllDebugFlags;
#endif  // DEBUG


#endif  // _DATA_H_

