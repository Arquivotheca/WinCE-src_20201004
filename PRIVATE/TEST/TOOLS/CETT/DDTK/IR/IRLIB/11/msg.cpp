//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock.h>
#include <oscfg.h>

#undef AF_IRDA
#include <af_irda.h>

#include <irsup.h>
#include "msg.h"

//
// Function to receive a specified number of bytes through a socket.
//
static int
Receive(
	SOCKET	s,
	char	*pBuffer,
	int		bytesToReceive)
{
	int bytesReceived = 0;
	int length;

	while (bytesReceived < bytesToReceive)
	{
		length = recv(s, pBuffer, bytesToReceive - bytesReceived, 0);
		if (length == 0 || length == SOCKET_ERROR)
		{
			// Socket has been closed, gracefully or otherwise

			if (length == SOCKET_ERROR)
				OutStr(TEXT("recv got error, WSAError=%d\n"), WSAGetLastError());

			return length;
		}
		
		ASSERT(length > 0);
		bytesReceived += length;
		pBuffer += length;
	}
	return bytesReceived;
}

extern int
MessageReceive(
	Connection	*pConn,
	char		*pMessage,
	int			 maxLength)
{
	int		cRecv;
	USHORT	msgLength;

	// Get the length of the message
	cRecv = Receive(pConn->s, (char *)&msgLength, sizeof(msgLength));
	if (cRecv == 0 || cRecv == SOCKET_ERROR)
		return cRecv;

	msgLength = ntohs(msgLength);
	if (msgLength > maxLength)
	{
		OutStr(TEXT("msgLength %d exceeds max (%d) allowed!\n"), msgLength, maxLength);
		return -1;
	}

	// Get the body of the message
	cRecv = Receive(pConn->s, pMessage, msgLength);

	return cRecv;
}

static int
Send(
	SOCKET	s,
	char	*pBuffer,
	int		bytesToSend)
{
	int cSent;

	cSent = send(s, pBuffer, bytesToSend, 0);

	if (cSent != bytesToSend)
	{
		OutStr(TEXT("Sent %d bytes, expected to send %d, WSAError=%d\n"), cSent, bytesToSend, WSAGetLastError());
	}

	return cSent;
}

extern int
MessageSend(
	Connection	*pConn,
	char		*pMessage,
	int			 length)
{
	USHORT	msgLength = length;
	int		cSent;

	ASSERT(length <= 65535);

	msgLength = htons(msgLength);

	// First send the length of the message to follow

	cSent = Send(pConn->s, (char *)&msgLength, sizeof(msgLength));

	if (cSent == 0 || cSent == SOCKET_ERROR)
		return cSent;

	// Now send the body of the message

	cSent = Send(pConn->s, pMessage, length);

	return cSent;
}

#define TCP_SERV_PORT               2001

//
// Establish a connection to the IR Test Server
//
extern BOOL
ConnectionOpen(
	int			 type,			// AF_INET or AF_IRDA
	char		*szServerName,
	Connection	*pConn)
{
	SOCKADDR_IRDA	 addressIRDA;
	SOCKADDR_IN		 addressINET;
	struct sockaddr *pSockAddr = NULL;
	int				 namelen = 0;

	pConn->s = socket(type, SOCK_STREAM, 0);
	if (pConn->s != INVALID_SOCKET)
	{
		if (type == AF_IRDA)
		{
			if (IRFindNewestDevice(&addressIRDA.irdaDeviceID[0], 1) == 0)
			{
				// Found server

				addressIRDA.irdaAddressFamily = AF_IRDA;
				strncpy(&addressIRDA.irdaServiceName[0], szServerName, sizeof(addressIRDA.irdaServiceName));

				if (connect(
						pConn->s, 
						(struct sockaddr *)&addressIRDA, 
						sizeof(addressIRDA))
					!= SOCKET_ERROR)
				{
					return TRUE;
				}
			}
		}
		else if (type == AF_INET)
		{
			struct hostent	*pHostInfo;

			memset( &addressINET, 0, sizeof(addressINET) );
			addressINET.sin_family = AF_INET;
			addressINET.sin_port = htons(TCP_SERV_PORT);
			addressINET.sin_addr.s_addr = inet_addr(szServerName);

			// If the ServerName was not a dot notation IP address, then try
			// to find a DNS to resolve it.

			if (addressINET.sin_addr.s_addr == INADDR_NONE)
			{
				pHostInfo = gethostbyname( szServerName );
				if (pHostInfo)
					memcpy( &addressINET.sin_addr, pHostInfo->h_addr, pHostInfo->h_length );
				else
					addressINET.sin_addr.s_addr = INADDR_NONE;
			}

			if (addressINET.sin_addr.s_addr != INADDR_NONE)
			{
				if (connect(pConn->s, (struct sockaddr *)&addressINET, sizeof(addressINET))
					!= SOCKET_ERROR)
				{
					return TRUE;
				}
			}

		}
		else
			ASSERT(0);

		closesocket(pConn->s);
	}
	OutStr(TEXT("SKIPPED: Unable to establish connection to %hs\n"), szServerName);
	return FALSE;
}


extern void
ConnectionClose(
	Connection *pConn)
{
	closesocket(pConn->s);
}

//
// Create an endpoint for communication.
//

extern BOOL
ConnectionCreateEndpoint(
	int			 type,			// AF_INET or AF_IRDA
	char		*strServerName,
	SOCKET		*pSock)
{
	SOCKADDR_IRDA	 addressIRDA;
	SOCKADDR_IN		 addressINET;
	struct sockaddr *pSockAddr = NULL;
	int				 namelen = 0;

	// Create a socket to wait for a connection from the test engine
	*pSock = socket(type, SOCK_STREAM, 0);
	if (*pSock == INVALID_SOCKET)
	{
		OutStr(TEXT("Socket failed, Error=%d af=%d type=%d\n"), WSAGetLastError(), AF_IRDA, SOCK_STREAM);
		return FALSE;
	}

	if (type == AF_IRDA)
	{
		addressIRDA.irdaAddressFamily = AF_IRDA;
		strncpy(&addressIRDA.irdaServiceName[0], strServerName, sizeof(addressIRDA.irdaServiceName));

		pSockAddr = (struct sockaddr *)&addressIRDA;
		namelen = sizeof(addressIRDA);
	}
	else if (type == AF_INET)
	{
		addressINET.sin_family = AF_INET;
		addressINET.sin_port = htons(TCP_SERV_PORT);
		addressINET.sin_addr.s_addr = INADDR_ANY;

		pSockAddr = (struct sockaddr *)&addressINET;
		namelen = sizeof(addressINET);
	} 
	else
		ASSERT(0);

	if (bind(*pSock, pSockAddr, namelen) == SOCKET_ERROR)
	{
		OutStr(TEXT("Bind failed, Error=%d\n"), WSAGetLastError());
		return FALSE;
	}
	if (listen(*pSock, 1))
	{
		OutStr(TEXT("Listen failed, Error=%d\n"), WSAGetLastError());
		return FALSE;
	}
	return TRUE;

}

extern BOOL
ConnectionAccept(
	SOCKET *pSock,
	Connection *pConn)
{
	pConn->s = accept(*pSock, 0, 0);
	if (pConn->s == INVALID_SOCKET)
		return FALSE;

	return TRUE;	
}