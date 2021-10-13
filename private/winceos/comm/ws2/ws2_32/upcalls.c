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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

upcalls.c

the upcalls to be provided for Winsock Service Providers (WSPs)


FILE HISTORY:
	OmarM     22-Sep-2000

*/


#include "winsock2p.h"
#include <cxport.h>

// upcalls dealing with socket handles are in socket.c
//		WPUQuerySocketHandleContext, WPUGetProviderPath, WPUFDIsSet
//		WPUCreateSocketHandle, int WPUCloseSocketHandle

void SetErrorCode(int Code, LPINT pErr) {

	__try {
		*pErr = Code;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		SetLastError(WSAEINVAL);
	}

}	// SetErrorCode()


WSAEVENT WPUCreateEvent(LPINT lpErrno) {
	WSAEVENT	hEvent;

	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (WSA_INVALID_EVENT == hEvent) {
		SetErrorCode(GetLastError(), lpErrno);
	}
	
	return hEvent;

}	// WPUCreateEvent()


BOOL WPUCloseEvent (
	WSAEVENT hEvent,
	LPINT lpErrno) {

	if (CloseHandle(hEvent)) {
		return TRUE;
	} else {
		SetErrorCode(GetLastError(), lpErrno);
		return FALSE;
	}

}	// WPUCloseEvent()


SOCKET WPUModifyIFSHandle(
	DWORD dwCatalogEntryId,
	SOCKET ProposedHandle,
	LPINT lpErrno) {

	// since we don't support IFS we shouldn't need to change anything
	if (INVALID_SOCKET == ProposedHandle)
		SetErrorCode(WSAEINVAL, lpErrno);
	
	return (SOCKET)ProposedHandle;

}	// WPUModifyIFSHandle()


BOOL WPUPostMessage(
	HWND hWnd,
	UINT Msg,
	WPARAM wParam,
	LPARAM lParam) {

	// we don't support window messages for Winsock in CE
	return 0;

}	// WPUPostMessage()



int WPUQueryBlockingCallback(
	DWORD dwCatalogEntryId,
	LPBLOCKINGCALLBACK FAR * lplpfnCallback,
	PDWORD_PTR lpdwContext,
	LPINT lpErrno) {

	int	Ret;

	// as a Win32 provider we're allowed to return a null callback fn
	// in which case the service provider can use the native Win32
	// synch objects to implement blocking - Winsock 2 spec summary
	__try {
		*lplpfnCallback = NULL;
		*lpdwContext = 0;
		Ret = 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		SetErrorCode(WSAEFAULT, lpErrno);
		Ret = SOCKET_ERROR;
	}
	
	return Ret;

}	// WPUQueryBlockingCallback()


int WPUQueueApc(
	LPWSATHREADID lpThreadId,
	LPWSAUSERAPC lpfnUserApc,
	DWORD_PTR dwContext,
	LPINT lpErrno) {

	SetErrorCode(WSAEOPNOTSUPP, lpErrno);
	return SOCKET_ERROR;

}	// WPUQueueApc()


BOOL WPUResetEvent(
	WSAEVENT hEvent,
	LPINT lpErrno) {

	if (ResetEvent(hEvent)) {
		return TRUE;
	} else {
		SetErrorCode(GetLastError(), lpErrno);
		return FALSE;
	}

}	// WPUResetEvent()


BOOL WPUSetEvent(
	WSAEVENT hEvent,
	LPINT lpErrno) {

	if (SetEvent(hEvent)) {
		return TRUE;
	} else {
		SetErrorCode(GetLastError(), lpErrno);
		return FALSE;
	}

}	// WPUSetEvent()


int WPUOpenCurrentThread(
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	SetErrorCode(WSAEOPNOTSUPP, lpErrno);
	return SOCKET_ERROR;

}	// WPUOpenCurrentThread()


int WPUCloseThread(
	LPWSATHREADID lpThreadId,
	LPINT lpErrno) {

	SetErrorCode(WSAEOPNOTSUPP, lpErrno);
	return SOCKET_ERROR;

}	// WPUCloseThread()


 // the following WSA calls are similar to the upcalls so put them here

WSAEVENT WSAAPI WSACreateEvent() {

	// CreateEvent already sets last error if there is an error
	return CreateEvent(NULL, TRUE, FALSE, NULL);

}	// WSACreateEvent()


BOOL WSAAPI WSACloseEvent (
	WSAEVENT hEvent) {

	// CloseHandle already sets last error if there is an error
	return CloseHandle(hEvent);

}	// WSACloseEvent()


BOOL WSAAPI WSAResetEvent(
	WSAEVENT hEvent) {

	return ResetEvent(hEvent);

}	// WSAResetEvent()


BOOL WSAAPI WSASetEvent(
	WSAEVENT hEvent) {

	return SetEvent(hEvent);

}	// WSASetEvent()

DWORD WSAAPI WSAWaitForMultipleEvents(
	IN DWORD			cEvents,
	IN const WSAEVENT	*lphEvents,
	IN BOOL				fWaitAll,
	IN DWORD			dwTimeout,
	IN BOOL				fAlertable) {

	ASSERT(!fAlertable);
	return WaitForMultipleObjects(cEvents, lphEvents, fWaitAll, dwTimeout);

}	// WSAWaitForMultipleEvents()


int WSAAPI WSAGetLastError() {

	return GetLastError();
	
}	// WSAGetLastError()


void WSAAPI WSASetLastError(
	IN int iError) {

	SetLastError(iError);
	
}	// WSASetLastError()



#define WINCEAFDMACRO 1
#include <psyscall.h>
#define  IMPLICIT_CALL PRIV_IMPLICIT_CALL
#include <mafdfunc.h>

DWORD
SOCKAPI
WSAControl(
    DWORD   Protocol,
    DWORD   Action,
    LPVOID  InputBuffer,
    LPDWORD InputBufferLength,
    LPVOID  OutputBuffer,
    LPDWORD OutputBufferLength
    )
{
	DWORD         Result;

	// Should we check if the API is registered?

	Result = AFDControl(Protocol,
						Action,
						InputBuffer,
						InputBufferLength ? *InputBufferLength : 0,
						OutputBuffer,
						OutputBufferLength ? *OutputBufferLength : 0,
						OutputBufferLength );
    return Result;
}

