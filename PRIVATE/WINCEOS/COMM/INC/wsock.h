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
    wsock.h

    WSOCK.386 VxD service definitions.


*/


#ifndef _WSOCK_H_
#define _WSOCK_H_

//
//  Version numbers.
//

#define WSOCK_Ver_Major         1
#define WSOCK_Ver_Minor         0


//
// Private SIO used to implement WSARecvMsg
//
#define SIO_RECV_MSG  _WSAIORW(IOC_VENDOR, 0x4153)

//
//  The current provider interface version number.  Increment
//  this constant after any change that effects the provider
//  interface.
//

#define WSOCK_INTERFACE_VERSION 0x80000001

//
//  All FD_* events.
//

#define FD_ALL  (FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE)

// moved the following from DLL
#define READ_EVENTS     (FD_READ | FD_CLOSE | FD_ACCEPT)
#define WRITE_EVENTS    (FD_WRITE | FD_CONNECT)
#define EXCEPT_EVENTS   (FD_OOB | FD_FAILED_CONNECT)
//
//  This "special" FD_ event is used in select so that we may
//  synthesize proper exceptfds for failed connection attempts.
//

#define FD_FAILED_CONNECT   0x0100


//
//  Infinite wait time for send/recv timeout.
//

#define SOCK_IO_TIME            (DWORD)-1L

//
//  A list of socket/event mask pairs.  A pointer to an array
//  of these structures is passed to WsCreateMultipleNotify to
//  create multiple notification objects.
//


typedef struct _SOCK_LIST {
	DWORD			hSocket;			// handle to socket passed in from dll layer
    struct _SOCK_INFO	*Socket;             // the target socket
    DWORD   	    EventMask;          // events the client is interested in
    DWORD	      	Context;            // user-defined context value (handle?)
    struct _SOCK_INFO	*pSock;			// ptr to real socket
				// no need to confuse people Context is just a SOCKET
} SOCK_LIST, *LPSOCK_LIST, *PSOCK_LIST;

// added for select funtionality
typedef struct _PEGNOTIFY {
	LIST_ENTRY			PerSocketList;
	LIST_ENTRY			GlobalList;		// we may need this later for WSAClose
	struct _SOCK_INFO	*pSocket;
	DWORD				EventMask;
	HANDLE				hEvent;


} PEGNOTIFY, * PPEGNOTIFY;



#ifndef SOCKHAND_DEFINED
DECLARE_HANDLE(SOCKHAND);
typedef SOCKHAND *PSOCKHAND;
#define SOCKHAND_DEFINED
#endif


// Include the AFD functions.
#include "afdfunc.h"

//
// Socket Handle API calls
//
#define WINSOCK_CALL(type, api, args)	(*(type (*) args)IMPLICIT_CALL(HT_SOCKET, api))


#define 	AFDCloseSocket WINSOCK_CALL (DWORD, 0, 			\
					(SOCKHAND     Socket))

#define 	AFDAccept WINSOCK_CALL (DWORD, 2, 				\
					 (SOCKHAND   ListeningSocket, 			\
					  PSOCKHAND  ConnectedSocket,			\
					  LPSOCKADDR Address,					\
					  LPDWORD	 AddrLen,					\
					  LPCONDITIONPROC pfnCondition,			\
					  DWORD		dwCallbackData,				\
					  CRITICAL_SECTION	*pDllCS))

#define 	AFDBind WINSOCK_CALL (DWORD, 3, \
					(SOCKHAND     Socket, \
					 LPSOCKADDR   Address, \
					 DWORD        AddressLength, \
					 CRITICAL_SECTION	*pDllCS))

#define 	AFDConnect WINSOCK_CALL (DWORD, 4, \
					(SOCKHAND     Socket, \
					 LPSOCKADDR   Address, \
					 DWORD        AddressLength, \
					 CRITICAL_SECTION	*pDllCS))
							 
#define 	AFDIoctl WINSOCK_CALL (DWORD, 5, \
					(SOCKHAND		Socket,		\
					 DWORD			Code,		\
					 VOID			*pInBuf,	\
					 DWORD			cInBuf,		\
					 VOID			*pOutBuf,	\
					 DWORD			cOutBuf,	\
					 LPDWORD		pcRet,		\
					 WSAOVERLAPPED	*pOv,		\
					 LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn,	\
					 LPWSATHREADID pThreadId,						\
					 CRITICAL_SECTION	*pDllCS))

#define 	AFDListen WINSOCK_CALL (DWORD, 6, \
					(SOCKHAND      Socket, \
					 DWORD         Backlog,	\
					 CRITICAL_SECTION	*pDllCS))

#define 	AFDRecvImpl WINSOCK_CALL (DWORD, 7, \
					(SOCKHAND		Socket,		\
					 WSABUF			*pWsaBufs,	\
					 DWORD			cWsaBufs,	\
					 DWORD			*pcRcvd,	\
					 WSABUF         *pControl,  \
					 DWORD			*pFlags,	\
					 LPSOCKADDR		pAddr,		\
					 LPDWORD		pcAddr,		\
					 WSAOVERLAPPED	*pOv,		\
					 LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn,	\
					 LPWSATHREADID pThreadId,		\
					 CRITICAL_SECTION	*pDllCS))

#define 	AFDSend WINSOCK_CALL (DWORD, 8, \
					(SOCKHAND		Socket,		\
					 WSABUF			*pWsaBufs,	\
					 DWORD			cWsaBufs,	\
					 DWORD			*pcSent,	\
					 DWORD			Flags,	\
					 LPSOCKADDR		pAddr,		\
					 DWORD			cAddr,		\
					 WSAOVERLAPPED	*pOv,		\
					 LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompRtn,	\
					 LPWSATHREADID pThreadId,		\
					 CRITICAL_SECTION	*pDllCS))
							 
#define 	AFDShutdown WINSOCK_CALL (DWORD, 9,				\
					(SOCKHAND      Socket,					\
					 DWORD         How,						\
					 CRITICAL_SECTION	*pDllCS))
					 
#define 	AFDGetsockname WINSOCK_CALL (DWORD, 10,			\
					(SOCKHAND      Socket,					\
					 LPSOCKADDR    Address,					\
					 LPDWORD       AddressLength,			\
					 CRITICAL_SECTION	*pDllCS))

#define 	AFDGetpeername WINSOCK_CALL (DWORD, 11,			\
					(SOCKHAND      Socket,					\
					 LPSOCKADDR    Address,					\
					 LPDWORD       AddressLength,			\
					 CRITICAL_SECTION	*pDllCS))

#define		AFDGetSockOpt WINSOCK_CALL (DWORD, 12,			\
					(SOCKHAND     Socket,					\
					 DWORD		  Level,					\
					 DWORD		  OptionName,				\
					 LPVOID		  Buffer,					\
					 LPDWORD	  BufferLength,				\
					 CRITICAL_SECTION	*pDllCS))

#define		AFDSetSockOpt WINSOCK_CALL (DWORD, 13,			\
					(SOCKHAND     Socket,					\
					 DWORD		  Level,					\
					 DWORD		  OptionName,				\
					 LPVOID		  Buffer,					\
					 DWORD		  BufferLength,				\
					 CRITICAL_SECTION	*pDllCS))
					 
#define		AFDWakeup WINSOCK_CALL (DWORD, 14,		\
					(SOCKHAND		Socket,			\
					DWORD			Event,			\
					DWORD			Status,			\
					CRITICAL_SECTION	*pDllCS))

#define		AFDGetOverlappedResult WINSOCK_CALL (DWORD, 15,	\
					(SOCKHAND		Socket,					\
					 WSAOVERLAPPED	*pOv,					\
					 DWORD			*pcTransfer,			\
					 DWORD			fWait,					\
					 DWORD			*pFlags,				\
					 DWORD			*pErr,					\
					 CRITICAL_SECTION	*pDllCS))

#define		AFDEventSelect WINSOCK_CALL (DWORD, 16,			\
					(SOCKHAND		Socket,					\
					 WSAEVENT		hEvent,					\
					 long			NetworkEvents,			\
					 DWORD			*pErr,					\
					 CRITICAL_SECTION	*pDllCS))
		
#define		AFDEnumNetworkEvents WINSOCK_CALL (DWORD, 17,		\
					(SOCKHAND			Socket,					\
					 WSAEVENT			hEvent,					\
					 LPWSANETWORKEVENTS	pNetworkEvents,			\
					 DWORD				*pErr,					\
					 CRITICAL_SECTION	*pDllCS))
					 

#endif  // _WSOCK_H_

