//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
