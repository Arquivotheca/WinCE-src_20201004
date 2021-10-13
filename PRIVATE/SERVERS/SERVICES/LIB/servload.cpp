//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    servload.c
 *
 * Purpose: WinCE services loading management.
 *
 */
#include <windows.h>
#include <types.h>
#include <tchar.h>
#include "serv.h"
#include <cardserv.h>
#include <service.h>
#include <devload.h>
#include <svsutil.hxx>
#include <creg.hxx>

#ifdef DEBUG

DBGPARAM dpCurSettings = {
    TEXT("services"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Undefined"),TEXT("Initialization"),
    TEXT("Undefined"),TEXT("Accept"),TEXT("Active Services"),TEXT("Timers"),
    TEXT("Undefined"),TEXT("Dying Devs"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
    0x0001
};
#endif  // DEBUG

extern BOOL g_fUseCELog;

//
// Services Callbacks
//
// Shutsdown a service when it's running from SERVICE_INIT_STANDALONE.
void ServiceCallbackShutdown(void) {
    SetEvent(g_hCleanEvt);
}


// Called on ActivateService.  Loads up a service based on information in RegPath
// and finds and index 	
HANDLE
StartOneService(
    LPCWSTR   RegPathInit,
    DWORD     LoadOrder,
    BOOL      fStandaloneProcess
    )
{
    HKEY DevKey;
    DWORD Context;
    HANDLE Handle;
    DWORD Index;
    DWORD Flags;
    DWORD status;
    DWORD ValLen;
    DWORD ValType;
    WCHAR DevDll[DEVDLL_LEN];
    WCHAR Prefix[DEVPREFIX_LEN];
    const WCHAR* RegPath;
    WCHAR  RegPathBuf[MAX_PATH+1];
    BOOL  fHasFullServicePath;

    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("SERVICES!StartOneService starting %s.\r\n"), RegPathInit));

    if (NULL == wcsstr(RegPathInit,L"\\")) {
	    // If the service doesn't have a registry path in it (which will be the case
	    // when ActivateService() is called programatically) then build it up.

        if (wcslen(RegPathInit) + SERVICE_BUILTIN_KEY_STR_LEN > MAX_PATH+1) {
            DEBUGMSG(ZONE_ERROR,(L"SERVICES!ActivateService registry string is too long.\r\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
        }

        wcscpy(RegPathBuf,SERVICE_BUILTIN_KEY);
        RegPathBuf[SERVICE_BUILTIN_KEY_STR_LEN] = TEXT('\\');
        wcscpy(RegPathBuf+SERVICE_BUILTIN_KEY_STR_LEN+1,RegPathInit);
        RegPath = RegPathBuf;
        fHasFullServicePath = FALSE;
    }
    else {
        // It's being called on services.exe startup with full registry key passed 
        // into us from regenum.dll.
        RegPath = RegPathInit;
        fHasFullServicePath = TRUE;
    }

    //
    // Get the required (dll, prefix, index) and optional (flags and context) values.
    //
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,RegPath,0,0,&DevKey);
    if (status) {
        DEBUGMSG(ZONE_ERROR,(TEXT("SERVICES!StartOneService RegOpenKeyEx(%s) returned %d.\r\n"),RegPath, status));
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    ValLen = sizeof(DevDll);
    status = RegQueryValueEx(DevKey,DEVLOAD_DLLNAME_VALNAME,NULL,
                             &ValType,(PUCHAR)DevDll,&ValLen);

    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(TEXT("SERVICES!StartOneService RegQueryValueEx(%s\\DllName) returned %d\r\n"),RegPath, status));
        RegCloseKey(DevKey);
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    ValLen = sizeof(Prefix);
    status = RegQueryValueEx(DevKey,DEVLOAD_PREFIX_VALNAME,NULL,
                             &ValType,(PUCHAR)Prefix,&ValLen);

    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(TEXT("SERVICES!StartOneService RegQueryValueEx(%s\\Prefix) returned %d\r\n"),RegPath, status));
        RegCloseKey(DevKey);
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    ValLen = sizeof(Index);
    status = RegQueryValueEx(DevKey,DEVLOAD_INDEX_VALNAME,NULL,
                             &ValType,(PUCHAR)&Index,&ValLen);

    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(TEXT("SERVICES!StartOneService RegQueryValueEx(%s\\Index) returned %d\r\n"),RegPath, status));
        RegCloseKey(DevKey);
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    ValLen = sizeof(Context);
    status = RegQueryValueEx(DevKey,DEVLOAD_CONTEXT_VALNAME,NULL,
                             &ValType,(PUCHAR)&Context,&ValLen);

    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,(TEXT("SERVICES!StartOneService RegQueryValueEx(%s\\Context) returned %d\r\n"),RegPath, status));
        Context = 0;
    }

    ValLen = sizeof(Flags);
    status = RegQueryValueEx(DevKey,DEVLOAD_FLAGS_VALNAME,NULL,
                             &ValType,(PUCHAR)&Flags,&ValLen);

    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,(TEXT("SERVICES!StartOneService RegQueryValueEx(%s\\Flags) returned %d\r\n"),RegPath, status));
        Flags = DEVFLAGS_NONE;  // default is no flags set
    }
    if (Flags & DEVFLAGS_NOLOAD) {
        DEBUGMSG(ZONE_ACTIVE,(TEXT("SERVICES!StartOneService not loading %s with DEVFLAGS_NOLOAD\n"),RegPath));
        // Really success, but we cannot distinguish success at
        // deliberately not loading from a failure to load.
        RegCloseKey(DevKey);
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return 0;
    }
    RegCloseKey(DevKey);

    // Spin a new services.exe (assuming we haven't been spun as such already).
    if ((Context & SERVICE_INIT_STANDALONE) && !fStandaloneProcess) {
        WCHAR szArgs[MAX_PATH+20];

        if (fHasFullServicePath)
            wsprintf(szArgs,L"load standalone %s",RegPathInit + SVSUTIL_CONSTSTRLEN(L"Services\\"));
        else
            wsprintf(szArgs,L"load standalone %s",RegPathInit);
    	
        if (! CreateProcess(SERVICE_SERVICES_EXE_PROCNAME,szArgs,NULL,NULL,FALSE,0,
                            NULL,NULL,NULL,NULL)) {
            DEBUGMSG(ZONE_ERROR,(L"SERVICES.EXE: CreateProcess(\"services.exe\",\"load standone\" failed,GLE=0x%08x\r\n",GetLastError()));
            return 0;
        }
        return (HANDLE)1;
    }

	Handle = RegisterServiceEx(Prefix,Index,DevDll,Context,Flags,RegPath);

    if (Handle && (Context & SERVICE_INIT_STOPPED)) {
        if (! StartSuperService(RegPath,(HANDLE)Handle,TRUE)) {
            SERV_DeregisterService(Handle);
            Handle = 0;
        }
    }

    if (Handle && (Context & SERVICE_INIT_STANDALONE)) {
        EnterCriticalSection(&g_ServCS);
        if (!IsValidService((fsdev_t *)Handle)) {
            LeaveCriticalSection(&g_ServCS);
            return 0;
        }

        ServicesExeCallbackFunctions serviceCallback;
        memset(&serviceCallback,0,sizeof(serviceCallback));

        serviceCallback.pfnServiceShutdown  = ServiceCallbackShutdown;

        InternalServiceIOControl((fsdev_t *)Handle,IOCTL_SERVICE_CALLBACK_FUNCTIONS,(BYTE*)&serviceCallback,sizeof(serviceCallback),0,0);
        LeaveCriticalSection(&g_ServCS);
    }

    return Handle;
}

//
// Function to load services on services.exe start up.
//
BOOL
InitServices(VOID)
{
    HMODULE hDevDll = 0;
    PFN_SERVICE_ENTRY DevEntryFn;
    BOOL fRet = FALSE;
    HANDLE h = 0;

#if ! defined (SHIP_BUILD)
    HKEY hKey;

    // Use CE Log?
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, SERVICE_BUILTIN_KEY, 0, KEY_ALL_ACCESS, &hKey)) {
        DWORD ValType;
        DWORD ValLen = sizeof(DWORD);
        
        if (ERROR_SUCCESS != RegQueryValueEx(hKey,L"UseCELog",NULL,
                             &ValType,(PUCHAR)&g_fUseCELog,&ValLen) ||
                             ValType != REG_DWORD ||
                             ValLen  != sizeof(DWORD)) {
            g_fUseCELog = FALSE;
        }
        
        RegCloseKey(hKey);
    }
#endif

    hDevDll = LoadDriver(L"regenum.dll");
    
    if (hDevDll == NULL) {
        RETAILMSG(1,(L"SERVICES!InitServices LoadDriver(\"regenum.dll\") failed %d\r\n",GetLastError()));
        goto done;
    }
    DevEntryFn = (PFN_SERVICE_ENTRY)GetProcAddress(hDevDll, L"Enum");
    
    if (DevEntryFn == NULL) {
        RETAILMSG(1,(L"SERVICES!InitServices GetProcAddr(regenum.dll, enumerate) failed %d\r\n",GetLastError()));
        goto done;
    }

    h = (DevEntryFn)(SERVICE_BUILTIN_KEY,ActivateService,NULL);
    if (h) {
        // regenum allocates a table to hold all these tables which we don't use.
        LocalFree((PBYTE)h);
    }

    fRet = TRUE;
done:
    if (hDevDll)
        FreeLibrary(hDevDll);

    return fRet;
}

typedef DWORD (WINAPI *PFN_COFREEUNUSEDLIBRARIES)(void);
typedef DWORD (WINAPI *PFN_COINITIALIZEEX)(LPVOID lpvReserved, DWORD dwCoInit);

PFN_COFREEUNUSEDLIBRARIES g_pfnCoFreeUnusedLibraries;
PFN_COINITIALIZEEX        g_pfnCoInitializeEx;

// Sleep 2 minutes between calls.
#define CO_FREE_UNUSED_SLEEP_MS   (1000*60*2)  

DWORD WINAPI ServicesCoFreeUnusedLibrariesThread(LPVOID lpv) {

	if (FAILED(g_pfnCoInitializeEx(NULL,COINIT_MULTITHREADED))) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES: CoInitializeEx failed, cannot start ServicesCoFreeUnusedLibrariesThread, GLE=0x%08x\r\n",GetLastError()));
		return 0;
	}

	while (1) {
		DEBUGMSG(ZONE_TIMERS,(L"SERVICES: Sleeping %d seconds before next call to CoFreeUnusedLibraries()\r\n",CO_FREE_UNUSED_SLEEP_MS / 1000));
		Sleep(CO_FREE_UNUSED_SLEEP_MS);

		DEBUGMSG(ZONE_TIMERS,(L"SERVICES: Making periodic call to CoFreeUnusedLibraries()\r\n"));
		g_pfnCoFreeUnusedLibraries();
	}
	DEBUGCHK(0);
	return 0;
}

// Determines if we're to run CoFreeUnusedLibraries thread in the background and starts it if we are.
void StartCoFreeUnusedLibrariesThread() {
	CReg reg(HKEY_LOCAL_MACHINE,SERVICE_BUILTIN_KEY);

	if (0 == reg.ValueDW(SERVICE_COFREEUNUSED_VALUE,TRUE)) {
		DEBUGMSG(ZONE_INIT,(L"SERVICES: Will not start CoFreeUnusedLibrariesThread because it was over-ridden in the registry\r\n"));
		return;
	}

	HINSTANCE hOle32 = LoadLibrary(L"ole32.dll");
	if (hOle32 == NULL) {
		DEBUGMSG(ZONE_INIT,(L"SERVICES: Will not start CoFreeUnusedLibrariesThread, cannot load ole32.dll, GLE=0x%08x\r\n",GetLastError()));
		return;
	}

	if ((NULL == (g_pfnCoFreeUnusedLibraries = (PFN_COFREEUNUSEDLIBRARIES) GetProcAddress(hOle32,L"CoFreeUnusedLibraries"))) ||
	    (NULL == (g_pfnCoInitializeEx = (PFN_COINITIALIZEEX) GetProcAddress(hOle32,L"CoInitializeEx")))) {
		DEBUGCHK(0); // This fcn should always be part of ole32.dll, even in mini-configurations.
		DEBUGMSG(ZONE_ERROR,(L"SERVICES: Will not start CoFreeUnusedLibrariesThread, ole32.dll doesn't export required fcns, GLE=0x%08x\r\n",GetLastError()));
		return;
	}

	DEBUGMSG(ZONE_INIT,(L"SERVICES: Starting ServicesCoFreeUnusedLibrariesThread\r\n"));

	if (NULL == CreateThread(NULL, 0, ServicesCoFreeUnusedLibrariesThread, NULL, 0, NULL))
		DEBUGMSG(ZONE_ERROR,(L"SERVICES: Will not start CoFreeUnusedLibrariesThread, cannot create thread, GLE=0x%08x.\r\n",GetLastError()));

	// alright never to free ole32.dll and not close thread handle, because services.exe never terminates.
	return;
}


BOOL ServerLoadInit(void)
{
	DEBUGREGISTER(NULL);
    DEBUGMSG(ZONE_INIT, (TEXT("SERVICES!ServerLoadInit\r\n")));

    if (! SuperServicesInit())
    	return FALSE;

    if (! InitServices())
    	return FALSE;

    StartCoFreeUnusedLibrariesThread();
    return TRUE;
}
