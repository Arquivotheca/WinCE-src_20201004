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
////////////////////////////////////////////////////////////////////////////////
//
//  Remote Network Logger 
//
//
////////////////////////////////////////////////////////////////////////////////

#include <winsock2.h>
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

#include "ILogger.h"

NetworkLogger::NetworkLogger()
{
	m_Socket = INVALID_SOCKET;
}

NetworkLogger::~NetworkLogger()
{
	Reset();
}

HRESULT NetworkLogger::Init(TCHAR* szServer, unsigned short port)
{
	HRESULT hr = S_OK;
	
	// Save the remote server and port
	_tcsncpy(m_szServer, szServer, sizeof(m_szServer)/sizeof(m_szServer[0]));
	m_Port = port;
	
	// Init Winsock
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD( 2, 0 );		 
	err = WSAStartup(wVersionRequested, &wsaData );
	if ( err != 0 ) {
		return E_FAIL;
	}

	// Create the socket descriptor
	m_Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_Socket == INVALID_SOCKET)
	{
		return E_FAIL;
	}

	int wsaret = 0;

	// Create the address
	sockaddr_in sin;
	int sinsize = sizeof(sin);
	wsaret = WSAStringToAddress(m_szServer, AF_INET, NULL, (LPSOCKADDR)&sin, &sinsize);
	if (wsaret != 0)
	{
		return E_FAIL;
	}

	// Set the port
	sin.sin_port = m_Port;

	// Connect to the server
	wsaret = connect(m_Socket, (LPSOCKADDR)&sin, sizeof(sin));
	if (wsaret != 0)
	{
		return E_FAIL;
	}

	return hr;
}

void NetworkLogger::Reset()
{
	// Close the socket
	if (m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}

	// Cleanup Winsock
	WSACleanup();
}

HRESULT NetworkLogger::Log(BYTE* pData, int datalen)
{
	if (!pData || (datalen <= 0))
		return E_INVALIDARG;

	int wsaret = send(m_Socket, (const char*)pData, datalen, 0);
	if (wsaret == SOCKET_ERROR)
	{
		return E_FAIL;
	}

	return S_OK;
}