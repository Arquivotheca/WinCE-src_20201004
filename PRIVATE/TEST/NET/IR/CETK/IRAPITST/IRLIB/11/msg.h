//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
