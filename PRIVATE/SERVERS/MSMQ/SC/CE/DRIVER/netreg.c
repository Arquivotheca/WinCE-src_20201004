//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#if !defined (SDK_BUILD) && !defined (USE_IPHELPER_INTERFACE)
#include <afdfunc.h>

//
//	NetBIOS LAN-UP Hook Interface
//
#define NB_NET_NOTIFY		0x04	// call back fn for net notifications

int scce_RegisterNET (void *ptr) {
	return NETbios (0, NB_NET_NOTIFY, ptr, 0, NULL, 0, NULL);
}

void scce_UnregisterNET (void) {
}

#elif defined USE_IPHELPER_INTERFACE

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

#else

int scce_RegisterNET (void *ptr) {
	return TRUE;
}

void scce_UnregisterNET (void) {
}
#endif

