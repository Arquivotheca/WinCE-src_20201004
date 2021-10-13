//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      Bluetooth Console
// 
// 
// Module Name:
// 
//      btconsole.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth Console
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>

#include <svsutil.hxx>

#include <bt_debug.h>

#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_hcip.h>

#if defined (BTH_CONSOLE)

static HANDLE hConsole = NULL;
static CRITICAL_SECTION cs;

static WCHAR *skipwhite (WCHAR *p) {
	while ((*p != '\0') && iswspace (*p))
		++p;

	return *p == '\0' ? NULL : p;
}

static DWORD WINAPI bth_ConsoleThread (LPVOID pParm) {
	for ( ; ; ) {
		WCHAR szBuffer[500];
		if (! fgetws (szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), stdin))
			return 0;

		WCHAR *p = wcschr (szBuffer, L'\n');
		if (p)
			*p = L'\0';

		int iRes = ERROR_SUCCESS;

		if (wcsicmp (szBuffer, L"exit") == 0)
			break;
		else if (wcsicmp (szBuffer, L"break") == 0) {
			DebugBreak ();
			continue;
		} else if (wcsnicmp (szBuffer, L"debug 0x", 8) == 0) {
			p = szBuffer + 8;
			unsigned int mask = 0;
			for (int i = 0 ; i < 8 ; ++i, ++p) {
				int c = *p;
				if (c >= 'a')
					c -= 'a' - 'A';

				if ((c < '0') || (c > 'F') || ((c > '9') && (c < 'A')))
					break;

				if (c >= 'A')
					c -= 'A' - 10;
				else
					c -= '0';

				mask = (mask << 4) | c;
			}

			if (i == 8) {
				DebugSetMask (mask); 
				DebugOut (DEBUG_OUTPUT, L"Set mask to 0x%08x\n", mask);
			} else
				DebugOut (DEBUG_OUTPUT, L"Command format is : debug 0xXXXXXXXX\n");
			continue;
		} else if (wcsicmp (szBuffer, L"stop card") == 0) {
			HCI_StopHardware ();
			continue;
		} else if (wcsicmp (szBuffer, L"start card") == 0) {
			HCI_StartHardware ();
			continue;
		} else if (wcsnicmp (szBuffer, L"tdi", 3) == 0) {
			if (p = skipwhite (szBuffer + 3))
				iRes = tdiR_ProcessConsoleCommand (p);
			else
				iRes = tdiR_ProcessConsoleCommand (L"help");
		} else if (wcsnicmp (szBuffer, L"portemu", 7) == 0) {
			if (p = skipwhite (szBuffer + 7))
				iRes = portemu_ProcessConsoleCommand (p);
			else
				iRes = portemu_ProcessConsoleCommand (L"help");
		} else if (wcsnicmp (szBuffer, L"rfcomm", 6) == 0) {
			if (p = skipwhite (szBuffer + 6))
				iRes = rfcomm_ProcessConsoleCommand (p);
			else
				iRes = rfcomm_ProcessConsoleCommand (L"help");
		} else if (wcsnicmp( szBuffer, L"sdp", 3) == 0) {
			if (p = skipwhite (szBuffer + 3))
				iRes = sdp_ProcessConsoleCommand (p);
			else
				iRes = sdp_ProcessConsoleCommand (L"help");
		} else if (wcsnicmp(szBuffer, L"l2cap", 5) == 0) {
			if (p = skipwhite (szBuffer + 5))
				iRes = l2cap_ProcessConsoleCommand (p);
			else
				iRes = l2cap_ProcessConsoleCommand (L"help");
		} else if (wcsnicmp(szBuffer, L"hci", 3) == 0) {
			if (p = skipwhite (szBuffer + 3))
				iRes = hci_ProcessConsoleCommand (p);
			else
				iRes = hci_ProcessConsoleCommand (L"help");
		} else if (wcsnicmp (szBuffer, L"help", 4) == 0) {
			p = skipwhite (szBuffer + 4);
			if (p) {
				if (wcsicmp (p, L"rfcomm") == 0)
					iRes = rfcomm_ProcessConsoleCommand (L"help");
				else if (wcsicmp (p, L"portemu") == 0)
					iRes = portemu_ProcessConsoleCommand (L"help");
				else if (wcsicmp (p, L"tdi") == 0)
					iRes = tdiR_ProcessConsoleCommand (L"help");
				else if (wcsicmp (p, L"sdp") == 0)
					iRes = sdp_ProcessConsoleCommand (L"help");
				else if (wcsicmp (p, L"sdp") == 0)
					iRes = sdp_ProcessConsoleCommand (L"help");
				else if (wcsicmp (p, L"l2cap") == 0)
					iRes = l2cap_ProcessConsoleCommand (L"help");
				else if (wcsicmp (p, L"hci") == 0)
					iRes = hci_ProcessConsoleCommand (L"help");
				else {
					tdiR_ProcessConsoleCommand (L"help");
					portemu_ProcessConsoleCommand (L"help");
					rfcomm_ProcessConsoleCommand (L"help");
					sdp_ProcessConsoleCommand (L"help");
					l2cap_ProcessConsoleCommand (L"help");
					hci_ProcessConsoleCommand (L"help");
				}
			} else {
				tdiR_ProcessConsoleCommand (L"help");
				portemu_ProcessConsoleCommand (L"help");
				rfcomm_ProcessConsoleCommand (L"help");
				sdp_ProcessConsoleCommand (L"help");
				l2cap_ProcessConsoleCommand (L"help");
				hci_ProcessConsoleCommand (L"help");
			}
		}

		if (iRes != ERROR_SUCCESS)
			DebugOut (DEBUG_OUTPUT, L"Command not understood\n");
	}
	return 0;
}

int bth_StartConsole (void) {
	if (hConsole)
		return ERROR_ALREADY_INITIALIZED;

	InitializeCriticalSection (&cs);

	hConsole = CreateThread (NULL, 0, bth_ConsoleThread, NULL, 0, NULL);
	if (! hConsole)
		return GetLastError ();

	return ERROR_SUCCESS;
}

int bth_EndConsole (void) {
	HANDLE h = (HANDLE)InterlockedExchange ((LONG *)&hConsole, (LONG)NULL);

	if (! h)
		return ERROR_SERVICE_NOT_ACTIVE;

	EnterCriticalSection (&cs);
	TerminateThread (h, 0);
	CloseHandle (h);
	LeaveCriticalSection (&cs);
	DeleteCriticalSection (&cs);

	return ERROR_SUCCESS;
}

#else

int bth_StartConsole (void) { return ERROR_SUCCESS; }

int bth_EndConsole (void) { return ERROR_SUCCESS; }

#endif