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

//
// ServCmd.cpp - handles services.exe command line options to make 
// managing services easier.  
//
// NOTE this process MUST run at lowest possible security privelege on systems that
// enable trust.  A malicious application can call CreateProcess("services.exe","command...",).
// When the service received the call, it would check the trust of services.exe
// itself and NOT the process that initiated the Create Process.
//
// Actual services themselves are now hosted in servicesd.exe.  Services.exe is
// just the command line wrapper to avoid the trust issues, keeping name same
// to avoid BC issues.
//

#include <windows.h>
#include <service.h>
#include <svsutil.hxx>
#include <servrc.h>
#include <cardserv.h>
#include <devload.h>
#include <creg.hxx>
#include <strsafe.h>

// Base registry key of services.exe
#define     SERVICE_BUILTIN_KEY           L"Services"
#define     SERVICE_BUILTIN_KEY_STR_LEN   SVSUTIL_CONSTSTRLEN(SERVICE_BUILTIN_KEY)
// Do we allow services command line parsing?
#define     SERVICE_ALLOW_CMD_LINE        L"AllowCmdLine"

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
BOOL   g_fDebugOut;

BOOL InitializeOutput(void) {
    HINSTANCE hCoreDLL = LoadLibrary(L"\\windows\\coredll.dll");
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

void OutputPrintf (WCHAR *szFormat, ...) {
    WCHAR szLongBuf[SERVICE_BUFFER_SIZE];

    if (g_fSilent)
        return;

    va_list lArgs;
    va_start (lArgs, szFormat);

    if (FAILED(StringCchVPrintfW(szLongBuf, SERVICE_BUFFER_SIZE, szFormat, lArgs)))
        return;

    if (g_fDebugOut)
        OutputDebugString (szLongBuf);
    else 
        wprintf (L"%s", szLongBuf);

    va_end(lArgs);
}

void ResourcePrintf (int iFormat, ...) {
    WCHAR szBuf[SERVICE_BUFFER_SIZE];
    WCHAR szLongBuf[SERVICE_BUFFER_SIZE];

    if (g_fSilent)
        return;

    if (! LoadString (GetModuleHandle(NULL), iFormat, szBuf, SVSUTIL_ARRLEN(szBuf)))
        OutputPrintf (L"Can't find the resource %d!", iFormat);

    va_list lArgs;
    va_start (lArgs, iFormat);

    if (FAILED(StringCchVPrintfW(szLongBuf, SERVICE_BUFFER_SIZE, szBuf, lArgs)))
        return;

    OutputPrintf (L"%s\n", szLongBuf);

    va_end(lArgs);
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

    ASSERT(0);
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
    PREFAST_ASSERT(pBuffer != NULL);
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

// To unload a service, need its service handle (not file handle)
int UnloadService(WCHAR *szServiceName) {
    HANDLE h = CreateFile(szServiceName,GENERIC_READ|GENERIC_WRITE,0,
                          NULL,OPEN_EXISTING,0,NULL);

    if (h == INVALID_HANDLE_VALUE) {
        ResourcePrintf(IDS_INVALID_SERVICE,szServiceName);
        return 0;
    }

    DEVMGR_DEVICE_INFORMATION devInfo;
    devInfo.dwSize = sizeof(devInfo);

    if (! GetDeviceInformationByFileHandle(h,&devInfo)) {
        ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
        CloseHandle(h);
        return 0;
    }

    // Must close handle before unloading so there's no open references.
    CloseHandle(h);

    if (! DeregisterService(devInfo.hDevice)) {
        ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
        return 0;
    }

    return 0;
}

int SendIOControl(DWORD dwIOControl,WCHAR *szServiceName,void *pInput,DWORD dwInputSize, void *pOutput, DWORD dwOutputSize, DWORD *pdwActualOutputSize) {
    HANDLE h = CreateFile(szServiceName,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);

    if (h == INVALID_HANDLE_VALUE) {
        ResourcePrintf(IDS_INVALID_SERVICE,szServiceName);
        return 0;
    }

    if (! DeviceIoControl(h, dwIOControl, pInput, dwInputSize,pOutput,dwOutputSize,pdwActualOutputSize,NULL)) {
        ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
    }
    CloseHandle(h);

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

    DWORD dwRet = RegSetValueEx(hKey,DEVLOAD_FLAGS_VALNAME,0,REG_DWORD,(LPBYTE)&dwValue,sizeof(DWORD));

    if (dwRet != ERROR_SUCCESS) {
        ResourcePrintf(IDS_OPERATION_FAILED,dwRet);
    }
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
    WCHAR szOutput[MAX_PATH*4];
    DWORD cbActualOutput;

    szOutput[0] = 0;

    DWORD dwInput;
    void *pInput = NULL;
    int iSvc = 1;

    if (wcsicmp (argv[0], L"help") == 0) {
        return Help();
    }
    else if ((wcsicmp (argv[0], L"list") == 0) && (argc == 1))
        return ListServices();
    else if ((wcsicmp (argv[0], L"load") == 0) && (argc == 2)) {
        HANDLE h = ActivateService(argv[1],0);
        if (!h)
            ResourcePrintf(IDS_OPERATION_FAILED,GetLastError());
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

        if (GetDWORD(&dwInput, argv[1])) {
            dwInputSize = sizeof(dwInput);
        } else {
            pInput = argv[1];
            dwInputSize = sizeof(WCHAR) * (wcslen (argv[1]) + 1);
        }
    }

    if (dwIOControl)
        return SendIOControl(dwIOControl,argv[iSvc], pInput, dwInputSize,szOutput,sizeof(szOutput),&cbActualOutput);

    ResourcePrintf(IDS_USAGE);
    return 3;
}


// 
// It is possible for an untrusted application to call 
// CreateProcess("Services.exe","<command line argument>",...).  When services.exe
// calls into the service itself, the service will have as its caller trusted
// services.exe, not the original untrusted app that called CreateProcess.  This allows
// an untrusted application to do whatever possibly can be done via the services
// command line, which includes a lot of management operations.  Not good.
//
// The only mitigation for this threat is to disable services.exe command line
// parsing via the registry.  Any app that wants to take management
// action on a service has to call CreateFile("Service...",...) and then 
// DeviceIOControl(hService,...).  The service can then determine what action to 
// take based on the application's trust level.
//

DWORD IsCmdLineAllowed(void) {
    CReg reg(HKEY_LOCAL_MACHINE,SERVICE_BUILTIN_KEY);
    return reg.ValueDW(SERVICE_ALLOW_CMD_LINE,1);
}

int wmain(int argc, WCHAR **argv) {
    int   iShift = 0;

    if (! InitializeOutput())
        return 0;

    if (0 == IsCmdLineAllowed()) {
        ResourcePrintf(IDS_OPERATION_FAILED,ERROR_ACCESS_DENIED);
        return 0;
    }

    if (argc < 2) {
        ResourcePrintf(IDS_USAGE);
        return 0;
    }

    if (wcsicmp (argv[1], L"-s") == 0) {
        if (argc < 3)
            return 0;

        g_fSilent = TRUE;
        iShift  = 1;
    }
    else if (wcsicmp (argv[1], L"-d") == 0) {
        if (argc < 3)
            return 0;

        g_fDebugOut = TRUE;
        iShift = 1;
    }

    RunServiceCMD(argc - 1 - iShift,&argv[1 + iShift]);
    return 0;
}
