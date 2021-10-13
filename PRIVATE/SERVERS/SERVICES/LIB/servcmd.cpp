//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    servutil.c
 *
 * Purpose: WinCE service manager
 *          Handles services.exe command line options.
 *
 */

#include <windows.h>
#include <serv.h>
#include <service.h>
#include <svsutil.hxx>
#include <servrc.h>
#include <cardserv.h>
#include <devload.h>

#if defined (UNDER_CE)
typedef int (*wprintf_t)(const wchar_t *, ...);
typedef FILE *(*getstdfilex_t) (int);
typedef FILE * (*wfopen_t)(const wchar_t *, const wchar_t *);
typedef int (*fclose_t)(FILE *);
typedef int (*fseek_t)(FILE *, long, int);
typedef long (*ftell_t)(FILE *);
typedef wchar_t * (*fgetws_t)(wchar_t *, int, FILE *);

wprintf_t ptr_wprintf;
getstdfilex_t ptr_getstdfilex;
wfopen_t ptr_wfopen;
fclose_t ptr_fclose;
fseek_t ptr_fseek;
ftell_t ptr_ftell;
fgetws_t ptr_fgetws;

#define wprintf ptr_wprintf
#define _getstdfilex ptr_getstdfilex
#define _wfopen ptr_wfopen
#define fclose ptr_fclose
#define fseek ptr_fseek
#define ftell ptr_ftell
#define fgetws ptr_fgetws

#endif // UNDER_CE


#define SERVICE_BUFFER_SIZE   512

BOOL   g_fSilent;
HANDLE g_hOutputFile = INVALID_HANDLE_VALUE;
BOOL   g_fDebugOut;

BOOL InitializeOutput(void) {
	HINSTANCE hCoreDLL = LoadLibrary(L"coredll.dll");
	if (!hCoreDLL)
		return FALSE;
		
	ptr_wfopen = (wfopen_t)GetProcAddress (hCoreDLL, L"_wfopen");
	ptr_fclose = (fclose_t)GetProcAddress (hCoreDLL, L"fclose");
	ptr_ftell = (ftell_t)GetProcAddress (hCoreDLL, L"ftell");
	ptr_fseek = (fseek_t)GetProcAddress (hCoreDLL, L"fseek");
	ptr_fgetws = (fgetws_t)GetProcAddress (hCoreDLL, L"fgetws");
	ptr_wprintf = (wprintf_t)GetProcAddress (hCoreDLL, L"wprintf");
	ptr_getstdfilex = (getstdfilex_t)GetProcAddress (hCoreDLL, L"_getstdfilex");

	if (! (ptr_wfopen && ptr_fclose && ptr_ftell && ptr_fseek && ptr_fgetws && ptr_wprintf && ptr_getstdfilex)) {
		ptr_wfopen = NULL;
		ptr_fclose = NULL;
		ptr_ftell = NULL;
		ptr_fseek = NULL;
		ptr_fgetws = NULL;
		ptr_wprintf = NULL;
		ptr_getstdfilex = NULL;

		g_fSilent = TRUE;
		FreeLibrary (hCoreDLL);
	}
	return TRUE;
}

int OutputPrintf (WCHAR *szFormat, ...) {
	WCHAR szLongBuf[SERVICE_BUFFER_SIZE];

	if (g_fSilent)
		return 0;

	va_list lArgs;
	va_start (lArgs, szFormat);

	int iRet = wvsprintf (szLongBuf, szFormat, lArgs);

	if (g_fDebugOut) {
		OutputDebugString (szLongBuf);
	}
	else if ((g_hOutputFile == INVALID_HANDLE_VALUE)) {
		wprintf (L"%s", szLongBuf);
	} 
	else {
		char szMB[SERVICE_BUFFER_SIZE];
		DWORD cBytes = WideCharToMultiByte (CP_ACP, 0, szLongBuf, -1, szMB, sizeof(szMB), NULL, NULL) - 1;

		if (cBytes > 0) {
			SetFilePointer (g_hOutputFile, 0, NULL, FILE_END);
			DWORD dwWritten = 0;
			WriteFile (g_hOutputFile, szMB, cBytes, &dwWritten, NULL);
		}
	}

	va_end(lArgs);
	return iRet;
}

int ResourcePrintf (int iFormat, ...) {
	WCHAR szBuf[SERVICE_BUFFER_SIZE];
	WCHAR szLongBuf[SERVICE_BUFFER_SIZE];

	if (g_fSilent)
		return 0;

	if (! LoadString (GetModuleHandle(NULL), iFormat, szBuf, SVSUTIL_ARRLEN(szBuf)))
		OutputPrintf (L"Can't find the resource %d!", iFormat);

	va_list lArgs;
	va_start (lArgs, iFormat);

	int iRet = wvsprintf (szLongBuf, szBuf, lArgs);
	OutputPrintf (L"%s\n", szLongBuf);

	va_end(lArgs);

	return iRet;
}

int Help(void) {
	if (!g_fSilent) {
		ResourcePrintf(IDS_HELP1);
		ResourcePrintf(IDS_HELP2);
		ResourcePrintf(IDS_HELP3);
		ResourcePrintf(IDS_HELP4);
		ResourcePrintf(IDS_HELP5);
		ResourcePrintf(IDS_HELP6);
		ResourcePrintf(IDS_HELP7);
		ResourcePrintf(IDS_HELP8);
		ResourcePrintf(IDS_HELP50);
		ResourcePrintf(IDS_HELP51);
	}
	return 0;
}


typedef struct {
	DWORD dwServiceState;
	DWORD dwResourceCode;
} SERVICE_STATE_NAME_ELEMENT;

const SERVICE_STATE_NAME_ELEMENT g_ServiceMap[] = {
	{ SERVICE_STATE_OFF,           IDS_STATE_OFF},
	{ SERVICE_STATE_ON,            IDS_STATE_RUNNING},
	{ SERVICE_STATE_STARTING_UP,   IDS_STATE_STARTING_UP},
	{ SERVICE_STATE_SHUTTING_DOWN, IDS_STATE_SHUTTING_DOWN},
	{ SERVICE_STATE_UNLOADING,     IDS_STATE_UNLOADING},
	{ SERVICE_STATE_UNINITIALIZED, IDS_STATE_UNINITIALIZED},
	{ SERVICE_STATE_UNKNOWN,       IDS_STATE_UNKNOWN}
};

DWORD GetReadableServiceState(DWORD dwServiceState) {
	DWORD i;

	for (i = 0; i < SVSUTIL_ARRLEN(g_ServiceMap); i++) {
		if (dwServiceState == g_ServiceMap[i].dwServiceState)
			return g_ServiceMap[i].dwResourceCode;
	}

	DEBUGCHK(0);
	return IDS_STATE_UNKNOWN;
}

int ListServices(void) {
	PBYTE pBuffer    = NULL;
	PBYTE pNewBuffer = NULL;
	DWORD dwEntries  = 0;
	DWORD dwBufLen   = 0;
	DWORD i;
	ServiceEnumInfo *pEnum;

	while (!EnumServices(pBuffer,&dwEntries,&dwBufLen)) {
		DWORD dwGLE = GetLastError();
		if ((dwGLE != ERROR_INSUFFICIENT_BUFFER) && (dwGLE != ERROR_MORE_DATA)) {
			ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
			goto done;
		}

		if (!pBuffer) {
			if (NULL == (pBuffer = (PBYTE) LocalAlloc(LMEM_MOVEABLE,dwBufLen))) {
				ResourcePrintf(IDS_NO_MEMORY);
				goto done;
			}
		}
		else {
			if (NULL == (pNewBuffer = (PBYTE) LocalReAlloc(pBuffer,dwBufLen,LMEM_MOVEABLE))) {
				ResourcePrintf(IDS_NO_MEMORY);
				goto done;
			}
			pBuffer = pNewBuffer;
		}
	}
	pEnum = (ServiceEnumInfo *) pBuffer;

	for (i = 0; i < dwEntries; i++)  {
		OutputPrintf(L"%s\t0x%08x\t%s\t",pEnum[i].szPrefix, pEnum[i].hServiceHandle,pEnum[i].szDllName);
		ResourcePrintf(GetReadableServiceState(pEnum[i].dwServiceState));
	}

done:
	if (pBuffer)
		LocalFree(pBuffer);
	return 0;
}

int UnloadService(WCHAR *szServiceName) {
	HANDLE h = GetServiceHandle(szServiceName,NULL,0);

	if (h == INVALID_HANDLE_VALUE) {
		ResourcePrintf(IDS_INVALID_SERVICE,szServiceName);
		return 0;
	}

	if (! DeregisterService(h)) {
		ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
		return 0;
	}

	return 0;
}

int SendIOControl(DWORD dwIOControl,WCHAR *szServiceName,void *pInput,DWORD dwInputSize) {
	HANDLE h = GetServiceHandle(szServiceName,NULL,0);

	if (h == INVALID_HANDLE_VALUE) {
		ResourcePrintf(IDS_INVALID_SERVICE,szServiceName);
		return 0;
	}

	if (! ServiceIoControl(h, dwIOControl, pInput, dwInputSize,NULL,0, NULL,NULL)) {
		ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
		return 0;
	}

	return 0;
}

int SetAutomaticLoadState(WCHAR *szServiceName, BOOL fLoadAtInitTime) {
	WCHAR szRegPath[MAX_PATH+1];
	HKEY  hKey;
	DWORD dwType;
	DWORD dwLen = sizeof(DWORD);
	DWORD dwValue;

	if ( (wcslen(szServiceName) + SERVICE_BUILTIN_KEY_STR_LEN) > SVSUTIL_ARRLEN(szRegPath)) {
		ResourcePrintf(IDS_OPERATION_FAILED,ERROR_BUFFER_OVERFLOW);
		return 0;
	}

	wcscpy(szRegPath,SERVICE_BUILTIN_KEY);
	szRegPath[SERVICE_BUILTIN_KEY_STR_LEN] = TEXT('\\');
	wcscpy(szRegPath+SERVICE_BUILTIN_KEY_STR_LEN+1,szServiceName);

	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE,szRegPath,0,KEY_ALL_ACCESS,&hKey)) {
		ResourcePrintf(IDS_OPERATION_FAILED,ERROR_DEV_NOT_EXIST);
		return 0;
	}

	if (ERROR_SUCCESS != RegQueryValueEx(hKey,DEVLOAD_FLAGS_VALNAME,NULL,&dwType,(LPBYTE)&dwValue,&dwLen) ||
	    (dwType != REG_DWORD) || (dwLen != sizeof(DWORD))) {
		dwValue = 0;
	}

	if (fLoadAtInitTime)
		dwValue &= ~(DEVFLAGS_NOLOAD);
	else
		dwValue |= (DEVFLAGS_NOLOAD);

	RegSetValueEx(hKey,DEVLOAD_FLAGS_VALNAME,0,REG_DWORD,(LPBYTE)&dwValue,sizeof(DWORD));
	RegCloseKey(hKey);

	return 0;
}

int GetDWORD (DWORD *pdw, WCHAR *p) {
    DWORD dwBase = 10;

    *pdw = 0;

    if ((p[0] == '0') && (p[1] == 'x')) {
        dwBase = 16;
        p += 2;
    }

    while (*p) {
        DWORD c;

        if ((*p >= '0') && (*p <= '9'))
            c = *p - '0';
        else if ((dwBase == 16) && (*p >= 'A') && (*p <= 'F'))
            c = *p - 'A' + 0xa;
        else if ((dwBase == 16) && (*p >= 'a') && (*p <= 'f'))
            c = *p - 'a' + 0xa;
        else
            return FALSE;

        *pdw = *pdw * dwBase + c;
	++p;
    }

    return TRUE;
}

int RunServiceCMD(int argc, WCHAR **argv) {
	DWORD dwIOControl = 0;
        DWORD dwInputSize = 0;

        union {
            DWORD dwInput;
        } Input;

        void *pInput = NULL;
	int iSvc = 1;

	if (wcsicmp (argv[0], L"help") == 0)
		return Help();
	else if ((wcsicmp (argv[0], L"list") == 0) && (argc == 1))
		return ListServices();
	else if ((wcsicmp (argv[0], L"load") == 0) && (argc == 2)) {
		HANDLE h = ActivateService(argv[1],0);
		if (!h)
			ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
		return 0;
	}
	else if ((argc == 3) && (wcsicmp (argv[0], L"load") == 0) && (wcsicmp (argv[1], L"standalone") == 0)) {
		// Stand alone loading of a service.
		InitServicesDataStructures();
		HANDLE h;

		if (0 == (h = StartOneService(argv[2],0,TRUE))) {
			ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
			return 0;
		}

		// We will wait forever or until service calls ServiceCallbackShutdown().
		WaitForSingleObject(g_hCleanEvt,INFINITE);
		DEBUGMSG(ZONE_INIT,(L"SERVICES.EXE: pfnServiceShutdown has been called, initiating services.exe shutdown\r\n"));
		SERV_DeregisterService(h);
		DeInitServicesDataStructures();
		return 0;
	}
	else if ((wcsicmp (argv[0], L"unload") == 0) && (argc == 2))
		return UnloadService(argv[1]);
	else if ((wcsicmp (argv[0], L"register") == 0) && (argc == 2))
		return SetAutomaticLoadState(argv[1],TRUE);
	else if ((wcsicmp (argv[0], L"unregister") == 0) && (argc == 2))
		return SetAutomaticLoadState(argv[1],FALSE);

	if ((wcsicmp (argv[0], L"start") == 0) && (argc == 2))
		dwIOControl = IOCTL_SERVICE_START;
	else if ((wcsicmp (argv[0], L"stop") == 0) && (argc == 2))
		dwIOControl = IOCTL_SERVICE_STOP;
	else if ((wcsicmp (argv[0], L"refresh") == 0) && (argc == 2))
		dwIOControl = IOCTL_SERVICE_REFRESH;
	else if ((wcsicmp (argv[0], L"debug") == 0) && (argc == 3)) {
		dwIOControl = IOCTL_SERVICE_DEBUG;
		iSvc = 2;

		if (GetDWORD(&Input.dwInput, argv[1])) {
			pInput = &Input;
			dwInputSize = sizeof(Input.dwInput);
		} else {
			pInput = argv[1];
			dwInputSize = sizeof(WCHAR) * (wcslen (argv[1]) + 1);
		}
	}

	if (dwIOControl)
		return SendIOControl(dwIOControl,argv[iSvc], pInput, dwInputSize);

	return ResourcePrintf(IDS_USAGE);
}

int HandleServicesCommandLine(int argc, WCHAR **argv) {
	int   iShift = 0;

	if (! InitializeOutput())
		return 0;

	if (argc < 2)
		return ResourcePrintf(IDS_USAGE);

	if (wcsicmp (argv[1], L"-s") == 0) {
		if (argc < 3)
			return 0;

		g_fSilent = TRUE;
		iShift  = 1;
	}
	else if (wcsicmp (argv[1], L"-f") == 0) {
		if (argc < 4)
			return 0;

		iShift = 2;

		g_hOutputFile = CreateFile (argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (g_hOutputFile == INVALID_HANDLE_VALUE)
			return 0;
	}
	else if (wcsicmp (argv[1], L"-d") == 0) {
		if (argc < 3)
			return 0;

		g_fDebugOut = TRUE;
		iShift = 1;
	}

	RunServiceCMD(argc - 1 - iShift,&argv[1 + iShift]);

	if (g_hOutputFile != INVALID_HANDLE_VALUE)
		CloseHandle(g_hOutputFile);
	return 0;
}
