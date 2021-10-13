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


#define WS_SOCK_FL_CLOSED	0x01

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


