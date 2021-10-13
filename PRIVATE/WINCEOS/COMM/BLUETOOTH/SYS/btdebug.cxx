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
//      Bluetooth DEBUG
// 
// 
// Module Name:
// 
//      btdebug.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth Debug Subsystem
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
#include <stdio.h>

#if ! defined (SDK_BUILD)
#define BT_USE_CELOG	1
#endif

#include <bt_debug.h>
#include <svsutil.hxx>
#include <bt_os.h>

static CRITICAL_SECTION gcs_debug;

static unsigned int		gMask;
static int				gOutputMode;
static int				gOutputCnt;
static FILE             *gfpOutputFile;

#define BPR		8

// Debug Zones.
#if defined (DEBUG)
  DBGPARAM dpCurSettings = {
    TEXT("BTD"), {
    TEXT("Output"),TEXT("Shell"),TEXT("HCI"),
    TEXT("L2CAP"),TEXT("RFCOMM"),TEXT("SDP"),TEXT("TDI"),
    TEXT("Packets"),TEXT("Stream"),TEXT("Dump"),TEXT("Init"),TEXT("Callback"),
    TEXT("Trace"),TEXT("Transport"),TEXT("Warn"),TEXT("Error") },
    0xc001
  };

unsigned long gulZoneMask;

#endif

void DebugOut (unsigned int cMask, WCHAR *lpszFormat, ...) {
#if defined (DEBUG)
	if (gulZoneMask != dpCurSettings.ulZoneMask) {
		EnterCriticalSection (&gcs_debug);
		if (gulZoneMask != dpCurSettings.ulZoneMask) {
			gulZoneMask = dpCurSettings.ulZoneMask;
			gMask = 0;
			if (gulZoneMask & 0x0001)
				gMask |= DEBUG_OUTPUT;
			if (gulZoneMask & 0x0002)
				gMask |= DEBUG_SHELL_ALL;
			if (gulZoneMask & 0x0004)
				gMask |= DEBUG_HCI_ALL;
			if (gulZoneMask & 0x0008)
				gMask |= DEBUG_L2CAP_ALL;
			if (gulZoneMask & 0x0010)
				gMask |= DEBUG_RFCOMM_ALL;
			if (gulZoneMask & 0x0020)
				gMask |= DEBUG_SDP_ALL;
			if (gulZoneMask & 0x0040)
				gMask |= DEBUG_TDI_ALL;
			if (gulZoneMask & 0x0080)
				gMask |= (DEBUG_HCI_PACKETS | DEBUG_L2CAP_PACKETS | DEBUG_RFCOMM_PACKETS | DEBUG_SDP_PACKETS | DEBUG_TDI_PACKETS);
			if (gulZoneMask & 0x0100)
				gMask |= DEBUG_STREAM_ALL;
			if (gulZoneMask & 0x0200)
				gMask |= DEBUG_HCI_DUMP;
			if (gulZoneMask & 0x0400)
				gMask |= (DEBUG_SHELL_INIT | DEBUG_HCI_INIT | DEBUG_L2CAP_INIT | DEBUG_RFCOMM_INIT | DEBUG_SDP_INIT | DEBUG_TDI_INIT);
			if (gulZoneMask & 0x0800)
				gMask |= (DEBUG_HCI_CALLBACK | DEBUG_L2CAP_CALLBACK | DEBUG_RFCOMM_CALLBACK | DEBUG_SDP_CALLBACK | DEBUG_TDI_CALLBACK);
			if (gulZoneMask & 0x1000)
				gMask |= (DEBUG_HCI_TRACE | DEBUG_L2CAP_TRACE | DEBUG_RFCOMM_TRACE | DEBUG_SDP_TRACE | DEBUG_TDI_TRACE);
			if (gulZoneMask & 0x2000)
				gMask |= DEBUG_HCI_TRANSPORT;
			if (gulZoneMask & 0x4000)
				gMask |= DEBUG_WARN;
			if (gulZoneMask & 0x8000)
				gMask |= DEBUG_ERROR;
		}
		LeaveCriticalSection (&gcs_debug);
	}

#endif
	if (cMask && (! (cMask & gMask)))
		return;

	EnterCriticalSection (&gcs_debug);

	static WCHAR szBigBuffer[2000];
	va_list arglist;
	va_start (arglist, lpszFormat);
	wvsprintf (szBigBuffer, lpszFormat, arglist);
	va_end (arglist);

	WCHAR *pszWhat = L"[MULTI] ";
	if (cMask & DEBUG_ERROR)
		pszWhat = L"[ERR] ";
	else if (cMask & DEBUG_WARN)
		pszWhat = L"[WARN] ";
	else if (cMask & DEBUG_OUTPUT)
		pszWhat = L"";
	else if ((cMask & DEBUG_SHELL_ALL) == cMask)
		pszWhat = L"[SHELL] ";
	else if ((cMask & DEBUG_HCI_ALL) == cMask)
		pszWhat = L"[HCI] ";
	else if ((cMask & DEBUG_L2CAP_ALL) == cMask)
		pszWhat = L"[L2CAP] ";
	else if ((cMask & DEBUG_RFCOMM_ALL) == cMask)
		pszWhat = L"[RFCOMM] ";
	else if ((cMask & DEBUG_SDP_ALL) == cMask)
		pszWhat = L"[SDP] ";
	else if ((cMask & DEBUG_TDI_ALL) == cMask)
		pszWhat = L"[TDI] ";
	else if ((cMask & DEBUG_STREAM_ALL) == cMask)
		pszWhat = L"[STREAM] ";

	if (gOutputMode & OUTPUT_MODE_REOPEN) {
		if (gfpOutputFile) {
			fclose (gfpOutputFile);
			gfpOutputFile = NULL;
		}

		gOutputMode &= ~OUTPUT_MODE_REOPEN;
	}

	if ((gOutputMode != OUTPUT_MODE_FILE) && gfpOutputFile) {
		fclose (gfpOutputFile);
		gfpOutputFile = NULL;
	}

	if ((gOutputMode == OUTPUT_MODE_FILE) && (! gfpOutputFile)) {
		WCHAR szFileName[50];
		wsprintf (szFileName, L"\\temp\\btd%d.txt", (gOutputCnt++) & 1);
		gfpOutputFile = _wfopen (szFileName, L"w");
	}

	if (gOutputMode == OUTPUT_MODE_FILE) {
		if (gfpOutputFile) {
			fwprintf (gfpOutputFile, L"%s%s", pszWhat, szBigBuffer);

			if (ftell (gfpOutputFile) > OUTPUT_FILE_MAX)
				gOutputMode |= OUTPUT_MODE_REOPEN;
		}
	} else if (gOutputMode == OUTPUT_MODE_CONSOLE) {
#if ! defined (UNDER_CE)
		HANDLE hStdout = GetStdHandle (STD_OUTPUT_HANDLE);
		if (hStdout != INVALID_HANDLE_VALUE) {
			if (cMask & DEBUG_ERROR)
				SetConsoleTextAttribute (hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
			else if (cMask & DEBUG_WARN)
				SetConsoleTextAttribute (hStdout, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
		}
#endif
		wprintf (L"%s%s", pszWhat, szBigBuffer);
#if ! defined (UNDER_CE)
		if ((hStdout != INVALID_HANDLE_VALUE) && (cMask & (DEBUG_ERROR | DEBUG_WARN)))
			SetConsoleTextAttribute (hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);	
#endif
	} else if (gOutputMode == OUTPUT_MODE_CELOG) {
#if defined (BT_USE_CELOG)
		RETAILCELOGMSG(1, (L"%s%s", pszWhat, szBigBuffer));
#endif
	} else {
		OutputDebugString (pszWhat);
		OutputDebugString (szBigBuffer);
	}

	LeaveCriticalSection (&gcs_debug);
}

void DumpBuff (unsigned int cMask, unsigned char *lpBuffer, unsigned int cBuffer) {
	if (cMask && (! (cMask & gMask)))
		return;

	EnterCriticalSection (&gcs_debug);

	WCHAR szLine[5 + 7 + 2 + 4 * BPR];

	for (int i = 0 ; i < (int)cBuffer ; i += BPR) {
		int bpr = cBuffer - i;
		if (bpr > BPR)
			bpr = BPR;

		wsprintf (szLine, L"%04x ", i);
		WCHAR *p = szLine + wcslen (szLine);

		for (int j = 0 ; j < bpr ; ++j) {
			WCHAR c = (lpBuffer[i + j] >> 4) & 0xf;
			if (c > 9) c += L'a' - 10; else c += L'0';
			*p++ = c;
			c = lpBuffer[i + j] & 0xf;
			if (c > 9) c += L'a' - 10; else c += L'0';
			*p++ = c;
			*p++ = L' ';
		}

		for ( ; j < BPR ; ++j) {
			*p++ = L' ';
			*p++ = L' ';
			*p++ = L' ';
		}

		*p++ = L' ';
		*p++ = L' ';
		*p++ = L' ';
		*p++ = L'|';
		*p++ = L' ';
		*p++ = L' ';
		*p++ = L' ';

		for (j = 0 ; j < bpr ; ++j) {
			WCHAR c = lpBuffer[i + j];
			if ((c < L' ') || (c >= 127))
				c = L'.';

			*p++ = c;
		}

		for ( ; j < BPR ; ++j) {
			*p++ = L' ';
		}

		*p++ = L'\n';
		*p++ = L'\0';

		SVSUTIL_ASSERT (p == szLine + sizeof(szLine)/sizeof(szLine[0]));

		DebugOut (cMask, L"%s", szLine);
	}

	LeaveCriticalSection (&gcs_debug);
}

void DumpBuffPfx (unsigned int cMask, WCHAR *lpszLinePrefix, unsigned char *lpBuffer, unsigned int cBuffer) {
	if (cMask && (! (cMask & gMask)))
		return;

	EnterCriticalSection (&gcs_debug);

	WCHAR szLine[5 + 7 + 2 + 4 * BPR];

	for (int i = 0 ; i < (int)cBuffer ; i += BPR) {
		int bpr = cBuffer - i;
		if (bpr > BPR)
			bpr = BPR;

		wsprintf (szLine, L"%04x ", i);
		WCHAR *p = szLine + wcslen (szLine);

		for (int j = 0 ; j < bpr ; ++j) {
			WCHAR c = (lpBuffer[i + j] >> 4) & 0xf;
			if (c > 9) c += L'a' - 10; else c += L'0';
			*p++ = c;
			c = lpBuffer[i + j] & 0xf;
			if (c > 9) c += L'a' - 10; else c += L'0';
			*p++ = c;
			*p++ = L' ';
		}

		for ( ; j < BPR ; ++j) {
			*p++ = L' ';
			*p++ = L' ';
			*p++ = L' ';
		}

		*p++ = L' ';
		*p++ = L' ';
		*p++ = L' ';
		*p++ = L'|';
		*p++ = L' ';
		*p++ = L' ';
		*p++ = L' ';

		for (j = 0 ; j < bpr ; ++j) {
			WCHAR c = lpBuffer[i + j];
			if ((c < L' ') || (c >= 127))
				c = L'.';

			*p++ = c;
		}

		for ( ; j < BPR ; ++j) {
			*p++ = L' ';
		}

		*p++ = L'\n';
		*p++ = L'\0';

		SVSUTIL_ASSERT (p == szLine + sizeof(szLine)/sizeof(szLine[0]));

		DebugOut (cMask, L"%s %s", lpszLinePrefix, szLine);
	}

	LeaveCriticalSection (&gcs_debug);
}

void DebugInitialize (void) {
	static int s_fInit = 0;

	gOutputCnt = 0;

	DeleteFile (L"\\temp\\btd0.txt");
	DeleteFile (L"\\temp\\btd1.txt");

#if defined (UNDER_CE)
#if defined (SDK_BUILD)
	gOutputMode = OUTPUT_MODE_FILE;
#else
	gOutputMode = OUTPUT_MODE_DEBUG;
#endif
#else
	gOutputMode = OUTPUT_MODE_CONSOLE;
#endif

	if (!s_fInit) {
		HKEY hk;

		gMask = 0;
		s_fInit = TRUE;
		InitializeCriticalSection (&gcs_debug);
		
		if (ERROR_SUCCESS != RegOpenKeyEx (HKEY_BASE, L"software\\Microsoft\\bluetooth\\Debug", 0, KEY_READ, &hk))
			return;

		DWORD dwSize = sizeof(gMask);
		DWORD dwType;
		RegQueryValueEx (hk, L"Mask", 0, &dwType, (LPBYTE)&gMask, &dwSize);

		dwSize = sizeof(gMask);
		RegQueryValueEx (hk, L"Console", 0, &dwType, (LPBYTE)&gOutputMode, &dwSize);

		RegCloseKey (hk);
	#if defined (DEBUG)
		dpCurSettings.ulZoneMask = 0;
		gulZoneMask = 0;
	#endif
	}
	
}

void DebugUninitialize (void) {
	gMask = 0;
	gOutputMode = OUTPUT_MODE_DEBUG;
	DeleteCriticalSection (&gcs_debug);
}

void DebugSetMask (unsigned int aMask) {
	gMask = aMask;
}

void DebugSetOutput (unsigned int fConsole) {
	if (fConsole > OUTPUT_MODE_MAX)
		gOutputMode |= fConsole;
	else
		gOutputMode = fConsole;
}

#else
void DebugOut (unsigned int cMask, WCHAR *lpszFormat, ...) {
}

void DumpBuff (unsigned int cMask, unsigned char *lpBuffer, unsigned int cBuffer) {
}

void DebugInitialize (void) {
}

void DebugUninitialize (void) {
}

void DebugSetMask (unsigned int aMask) {
}

void DebugSetOutput (unsigned int fConsole) {
}

#endif	// DEBUG || _DEBUG
