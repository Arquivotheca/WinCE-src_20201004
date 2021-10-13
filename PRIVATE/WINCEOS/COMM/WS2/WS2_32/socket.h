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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

socket.h

FILE HISTORY:
	OmarM     22-Sep-2000

*/



int Started();


typedef struct WsProvider {
	struct WsProvider *pNext;
	int				cRefs;
	HINSTANCE		hLibrary;
	GUID			Id;
	WSPPROC_TABLE	ProcTable;
	WCHAR			sDllPath[MAX_PATH];
} WsProvider;


#define WS_SOCK_FL_CLOSED	    0x01

// We're currently storing 
// both socket handles, and LSP socket
// handles in the same data structure.
// This flag indicates our socket was created with WS_SOCK_FL_LSP_HANDLE
#define WS_SOCK_FL_LSP_HANDLE	0x02

typedef struct WsSocket {
	struct WsSocket	*pNext;
	WsProvider		*pProvider;
	int				cRefs;
	uint			Flags;
	SOCKET			hSock;
	SOCKET			hWSPSock;
	WSAPROTOCOL_INFO	ProtInfo;
} WsSocket;


int GetProvider(int af, int type, int proto, 
		LPWSAPROTOCOL_INFOW pOrigProtInfo, LPWSAPROTOCOL_INFOW pProtInfo, 
		WsProvider **ppProv);

int RefSocketHandle(SOCKET hSock, WsSocket **ppSock);

int DerefSocketHandle(SOCKET hSock);
int DerefSocket(WsSocket *pSock);

int CheckSockaddr(const struct sockaddr *pAddr, int cAddr);
void CloseAllSockets();


