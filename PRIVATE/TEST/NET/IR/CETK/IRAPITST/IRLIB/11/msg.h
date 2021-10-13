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
typedef struct
{
	SOCKET s;
} Connection;

extern int
MessageReceive(
	Connection	*pConn,
	char				*pMessage,
	int					 maxLength);

extern int
MessageSend(
	Connection	*pConn,
	char				*pMessage,
	int					 length);

extern BOOL
ConnectionOpen(
	int	  af_type,
	char *strServerName,
	Connection *pConn);

extern void
ConnectionClose(
	Connection *pConn);

extern BOOL
ConnectionCreateEndpoint(
	int     af_type,
	char *strServerName,
	SOCKET *pSock);

extern BOOL
ConnectionAccept(
	SOCKET *pSock,
	Connection *pConn);
