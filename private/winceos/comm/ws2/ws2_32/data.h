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

