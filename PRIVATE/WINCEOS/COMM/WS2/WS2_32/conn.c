//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

comm.c

communications related winsock2 functions


FILE HISTORY:
	OmarM     22-Sep-2000

*/


#include "winsock2p.h"


int WSAAPI listen (
	SOCKET s,
	int backlog) {

	int			Err, Status;
	WsSocket	*pSock;

	if (! (Err = RefSocketHandle(s, &pSock))) {

		Status = pSock->pProvider->ProcTable.lpWSPListen(pSock->hWSPSock, 
			backlog, &Err);

		if (Status && ! Err) {
			ASSERT(0);
			Err = WSASYSCALLFAILURE;
		}

		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	}
	
	return Status;

}	// listen()


int WSAAPI connect (
	SOCKET s,
	const struct sockaddr FAR*  name,
	int namelen) {

	return WSAConnect(s, name, namelen, NULL, NULL, NULL, NULL);

}	// connect()


int  WSAConnect (
	SOCKET s,
	const struct sockaddr FAR * name,
	int namelen,
	LPWSABUF lpCallerData,
	LPWSABUF lpCalleeData,
	LPQOS lpSQOS,
	LPQOS lpGQOS) {

	int			Err, Status;
	WsSocket	*pSock;

	if (! (Err = RefSocketHandle(s, &pSock))) {

		if (! (Err = CheckSockaddr(name, namelen))) {

			Status = pSock->pProvider->ProcTable.lpWSPConnect(
				pSock->hWSPSock, name, namelen, lpCallerData, lpCalleeData, 
				lpSQOS, lpGQOS, &Err);

			if (Status && ! Err) {
				ASSERT(0);
				Err = WSASYSCALLFAILURE;
			}
		}
		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	}
	
	return Status;

}	// WSAConnect()


int WSAAPI shutdown (
	SOCKET s,
	int how) {

	int			Err, Status;
	WsSocket	*pSock;

	if ((SD_BOTH != how) && (SD_RECEIVE != how) && (SD_SEND != how)) {
		Err = WSAEINVAL;
	} else if (! (Err = RefSocketHandle(s, &pSock))) {

		// check validity of the how parameter in stack below
		// some cases of it may be dependent on type of socket

		Status = pSock->pProvider->ProcTable.lpWSPShutdown(pSock->hWSPSock, 
			how, &Err);

		if (Status && ! Err) {
			ASSERT(0);
			Err = WSASYSCALLFAILURE;
		}

		DerefSocket(pSock);
	}

	if (Err) {
		SetLastError(Err);
		Status = SOCKET_ERROR;
	}
	
	return Status;

}	// shutdown()



