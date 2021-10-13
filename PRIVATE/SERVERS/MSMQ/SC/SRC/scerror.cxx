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

    scerror.cxx

Abstract:

    Small client error processing


--*/

#include <sc.hxx>

//
//	Private routines
//
static int scerror_GetFileNameAndSize (WCHAR *szFileName, int ccFileName, DWORD *pdwSize) {
	SVSUTIL_ASSERT (ccFileName > 0);
	SVSUTIL_ASSERT (szFileName);

	int fError = FALSE;

	for ( ; ; ) {
		HKEY hKey;
		LONG hr = RegOpenKeyEx (HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_READ | KEY_WRITE, &hKey);

		if (hr != ERROR_SUCCESS) {
			fError = TRUE;
			break;
		}

		DWORD dwType = REG_DWORD;
		DWORD dwValue = 0;
		DWORD dwSize = sizeof(dwValue);

		if ((RegQueryValueEx (hKey, L"LogSize", NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) &&
			(dwType == REG_DWORD) && (dwSize == sizeof(dwValue)))
			*pdwSize = dwValue * 1024;

		dwSize = sizeof(WCHAR) * ccFileName;
		dwType = REG_SZ;
		szFileName[0] = L'\0';

		hr = RegQueryValueEx (hKey, L"BaseDir", NULL, &dwType, (LPBYTE)szFileName, &dwSize);

		RegCloseKey (hKey);

		if ((hr != ERROR_SUCCESS) || (dwType != REG_SZ) || (szFileName[0] == L'\0'))
			fError = TRUE;

		break;
	}

	if (fError)
		GetTempPath (ccFileName, szFileName);

	szFileName[ccFileName-1] = L'\0';
	WCHAR *p = szFileName + wcslen(szFileName);

	SVSUTIL_ASSERT (p != szFileName);

	if (*(p - 1) == L'\\')
		--p;

	if ((int)(p - szFileName + SVSUTIL_CONSTSTRLEN(MSMQ_SC_LOGFILE) + 2) > ccFileName)
		return FALSE;

	*p++ = L'\\';

	memcpy (p, MSMQ_SC_LOGFILE, sizeof (MSMQ_SC_LOGFILE));

	return TRUE;
}

static HANDLE scerror_GetFileError (void) {
	WCHAR szFileName[_MAX_PATH];
	DWORD dwSize = MSMQ_SC_LOGSIZE;

	if (! scerror_GetFileNameAndSize (szFileName, _MAX_PATH, &dwSize))
		return INVALID_HANDLE_VALUE;

	if (dwSize != 0) {
		WIN32_FIND_DATA fd;
		HANDLE hSearch = FindFirstFile (szFileName, &fd);
		if (hSearch != INVALID_HANDLE_VALUE) {
			CloseHandle (hSearch);

			if (fd.nFileSizeLow > (dwSize / 2)) {
				WCHAR szFileName2[_MAX_PATH];
				wcscpy (szFileName2, szFileName);
				wcscat (szFileName2, L".old");

				DeleteFile (szFileName2);

				if (! MoveFile (szFileName, szFileName2))
					DeleteFile (szFileName);
			}
		}
	}

	return CreateFile (szFileName, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
						NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static HANDLE scerror_GetFileInform (void) {
	return scerror_GetFileError ();
}

int hFile_vfwprintf (HANDLE hFile, WCHAR *lpszFormat, va_list args) {
	WCHAR szBigLine[MSMQ_SC_BIGBUFFER];

	int iRes = wvsprintf (szBigLine, lpszFormat, args);

	char szMB[MSMQ_SC_BIGBUFFER];
	DWORD cBytes = WideCharToMultiByte (CP_ACP, 0, szBigLine, -1, szMB, sizeof(szMB), NULL, NULL) - 1;

	if (cBytes <= 0)	// Empty line
		return 0;

	SetFilePointer (hFile, 0, NULL, FILE_END);
	DWORD dwWritten = 0;

	if ((! WriteFile (hFile, szMB, cBytes, &dwWritten, NULL)) ||
		(dwWritten != cBytes))
		return 0;

	return iRes;
}

int hFile_fwprintf (HANDLE hFile, WCHAR *lpszFormat, ...) {
	va_list args;

	va_start (args, lpszFormat);
	int iRes = hFile_vfwprintf (hFile, lpszFormat, args);
	va_end (args);

	return iRes;
}

void scerror_timestamp (HANDLE hFile) {
	SYSTEMTIME st;
	GetLocalTime (&st);
	hFile_fwprintf (hFile, L"[%02d:%02d:%02d %d/%d/%d] ", st.wHour, st.wMinute, st.wSecond, st.wMonth, st.wDay, st.wYear);
}

//
//	Main code section
//
void scerror_Complain (WCHAR *lpszFormat, ...) {
	HANDLE hFile = scerror_GetFileError ();

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	scerror_timestamp (hFile);

	va_list args;
	va_start (args, lpszFormat);
	hFile_vfwprintf (hFile, lpszFormat, args);
	hFile_fwprintf (hFile, L"\n");

	va_end (args);

	CloseHandle (hFile);
}

void scerror_Complain (int iFormat, ...) {
	HANDLE hFile = scerror_GetFileError ();

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	scerror_timestamp (hFile);

	WCHAR szFormat[MSMQ_SC_BIGBUFFER];

	LoadString (
#if defined (MSMQ_ANCIENT_OS)
		NULL,
#else
		GetModuleHandle(NULL),
#endif
		iFormat, szFormat, MSMQ_SC_BIGBUFFER);

	va_list args;
	va_start (args, iFormat);
	hFile_vfwprintf (hFile, szFormat, args);
	hFile_fwprintf (hFile, L"\n");

	va_end (args);

	CloseHandle (hFile);
}

void scerror_Inform (WCHAR *lpszFormat, ...) {
	HANDLE hFile = scerror_GetFileError ();

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	scerror_timestamp (hFile);

	va_list args;
	va_start (args, lpszFormat);
	hFile_vfwprintf (hFile, lpszFormat, args);
	hFile_fwprintf (hFile, L"\n");

	va_end (args);

	CloseHandle (hFile);
}

void scerror_Inform (int iFormat, ...) {
	HANDLE hFile = scerror_GetFileError ();

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	scerror_timestamp (hFile);

	WCHAR szFormat[MSMQ_SC_BIGBUFFER];

	LoadString (
#if defined (MSMQ_ANCIENT_OS)
		NULL,
#else
		GetModuleHandle(NULL),
#endif
		iFormat, szFormat, MSMQ_SC_BIGBUFFER);

	va_list args;
	va_start (args, iFormat);
	hFile_vfwprintf (hFile, szFormat, args);
	hFile_fwprintf (hFile, L"\n");

	va_end (args);

	CloseHandle (hFile);
}

int scerror_AssertOut (void *pvParam, WCHAR *lpszFormat, ...) {
	HANDLE hFile = scerror_GetFileError ();

	if (hFile == INVALID_HANDLE_VALUE)
		return 0;

	scerror_timestamp (hFile);

	va_list args;
	va_start (args, lpszFormat);
	int iRes = hFile_vfwprintf (hFile, lpszFormat, args);
	hFile_fwprintf (hFile, L"\n");

	va_end (args);

	CloseHandle (hFile);

	return iRes;
}

#if defined (SC_VERBOSE)
unsigned int g_bCurrentMask    = 0;
unsigned int g_bOutputChannels = 0;

#if defined (winCE)
typedef _CRTIMP int (*wprintf_t) (const wchar_t *, ...);
static wprintf_t ptr_wprintf;
#endif

void scerror_DebugInitialize (void) {
#if defined (winCE)
	if (! ptr_wprintf) {
		HINSTANCE hLib = LoadLibrary (L"coredll.dll");
		if (hLib) {
			ptr_wprintf = (wprintf_t)GetProcAddress (hLib, L"wprintf");
			if (! ptr_wprintf)
				FreeLibrary (hLib);
		}
	}
#endif

	g_bCurrentMask    = 0;
	g_bOutputChannels = 0;

	HKEY hKey;
	LONG hr = RegOpenKeyEx (HKEY_LOCAL_MACHINE, MSMQ_SC_REGISTRY_KEY, 0, KEY_READ | KEY_WRITE, &hKey);

	if (hr != ERROR_SUCCESS)
		return;

	DWORD dwType = 0;
	DWORD dwSize = sizeof(g_bOutputChannels);

	RegQueryValueEx (hKey, L"DebugOutputChannels", NULL, &dwType, (LPBYTE)&g_bOutputChannels, &dwSize);
	RegQueryValueEx (hKey, L"DebugOutputMask", NULL, &dwType, (LPBYTE)&g_bCurrentMask, &dwSize);

	RegCloseKey (hKey);
}

void scerror_DebugOutPrintArgs(WCHAR *lpszFormat, va_list args) {
	if (! g_bOutputChannels)
		return;

	WCHAR szBigBuffer[MSMQ_SC_BIGBUFFER];
	wvsprintf (szBigBuffer, lpszFormat, args);

#if defined (winCE)
	if ((g_bOutputChannels & VERBOSE_OUTPUT_CONSOLE) && ptr_wprintf)
		ptr_wprintf (L"%s", szBigBuffer);
#else
	if (g_bOutputChannels & VERBOSE_OUTPUT_CONSOLE)
		wprintf (L"%s", szBigBuffer);
#endif

	if (g_bOutputChannels & VERBOSE_OUTPUT_LOGFILE) {
		HANDLE hFile = scerror_GetFileError ();

		if (hFile != INVALID_HANDLE_VALUE) {
			hFile_fwprintf (hFile, L"%s", szBigBuffer);
			CloseHandle (hFile);
		}
	}

	if (g_bOutputChannels & VERBOSE_OUTPUT_DEBUGMSG) {
		OutputDebugString (L"MSMQ : ");
		OutputDebugString (szBigBuffer);
		OutputDebugString (L"\n");
	}
}

void scerror_DebugOutPrint(WCHAR *lpszFormat, ...) {
	va_list args;
	va_start (args, lpszFormat);
	scerror_DebugOutPrintArgs(lpszFormat,args);
	va_end (args);
}

int scerror_DebugOut (unsigned int fMask, WCHAR *lpszFormat, ...) {
	if (! (fMask & g_bCurrentMask))
		return 0;

	va_list args;
	va_start (args, lpszFormat);
	scerror_DebugOutPrintArgs(lpszFormat,args);
	va_end (args);
	return 1;
}

#else

int scerror_DebugOut (unsigned int fMask, WCHAR *lpszFormat, ...) {
	return 0;
}

void scerror_DebugInitialize (void) {
}

#endif		// SC_VERBOSE
