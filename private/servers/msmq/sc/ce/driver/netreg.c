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
/*++


Module Name:

    netreg.c

Abstract:

    Small client - Windows CE nonport part


--*/
#define WINCEMACRO	1
#include <windows.h>
#include <bldver.h>

#include <iphlpapi.h>

typedef void (*TFNetChangeHook) (unsigned char lananum, int flags, int unused);

HANDLE g_StopInterfaceEvent;
HANDLE g_hInterfaceThread;
TFNetChangeHook tfNetChangeHook;

DWORD WINAPI InterfaceNotificationThread(LPVOID lpvParam) {
	HANDLE haWaitHandles[2];
	haWaitHandles[0] = g_StopInterfaceEvent;

	if (NO_ERROR != NotifyAddrChange(&haWaitHandles[1], NULL))	
		return FALSE;

	while (WAIT_OBJECT_0 + 1 == WaitForMultipleObjects(2, haWaitHandles, FALSE, INFINITE)) {
		if (tfNetChangeHook)
			tfNetChangeHook (0, 0, 0);
	}

	CloseHandle(haWaitHandles[1]);

	return FALSE;
}

int scce_RegisterNET (void *ptr) {
	g_StopInterfaceEvent = NULL;
	g_hInterfaceThread = NULL;

	//
	//create the notification thread and ask iphelper for notification info
	//
	tfNetChangeHook = (TFNetChangeHook)ptr;

	//create a stop event
	if (! (g_StopInterfaceEvent = CreateEvent (NULL, FALSE, FALSE, NULL)))
		return FALSE;

	//spawn a thread to listen for notifications
	if (! (g_hInterfaceThread = CreateThread (NULL, 0, InterfaceNotificationThread, 0, 0, 0))) {
		CloseHandle (g_StopInterfaceEvent);
		g_StopInterfaceEvent = NULL;
		return FALSE;
	}

	return TRUE;
}

void scce_UnregisterNET (void) {
	//set the stop event
	if (g_StopInterfaceEvent) {
		SetEvent(g_StopInterfaceEvent);
		CloseHandle(g_StopInterfaceEvent);
		g_StopInterfaceEvent = NULL;
	}
	
	//wait for the thread to stop
	if (g_hInterfaceThread) {
		WaitForSingleObject(g_hInterfaceThread, INFINITE);
		CloseHandle(g_hInterfaceThread);
		g_hInterfaceThread = NULL;
	}
}

