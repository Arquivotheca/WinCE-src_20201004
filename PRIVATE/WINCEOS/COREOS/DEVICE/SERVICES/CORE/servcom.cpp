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
// Implements COM (Component Object Model) specific functionality for services.exe
// 

#include <servMain.hpp>

// Sleep 15 minutes between calls (by default)
#define CO_FREE_UNUSED_SLEEP_MS   (1000*60*15)
static const WCHAR g_freeUnusedLibReg[] = L"CoFreeUnusedLibrariesThreadPeriod";

// Time, in ms, that we wait between calling CoFreeUnusedLibraries() should
// services.exe be configured to do so
static DWORD g_freeUnusedLibPeriod;


// Function pointer type definitions for relevant COM APIs.  We must dynamically
// load COM because servuces.exe does not have a COM dependency.
typedef DWORD (WINAPI *PFN_COFREEUNUSEDLIBRARIES)(void);
typedef DWORD (WINAPI *PFN_COINITIALIZEEX)(LPVOID lpvReserved, DWORD dwCoInit);
typedef DWORD (WINAPI *PFN_COUNINITIALIZE)(void);


// Has COM been initialized?
static BOOL    g_comInited;
// Thread handle for the worker thread spun up
static HANDLE  g_hCoFreeWorkerThread;
// Library handle for ole32.dll
static HMODULE g_hOle32;
// Function pointers for relevant COM APIs
static PFN_COFREEUNUSEDLIBRARIES g_pfnCoFreeUnusedLibraries;
static PFN_COINITIALIZEEX        g_pfnCoInitializeEx;
static PFN_COUNINITIALIZE        g_pfnCoUninitialize;

// Initializes COM related functionality
void InitCOM() {
    DEBUGCHK(g_comInited == FALSE);
    DEBUGCHK(g_hCoFreeWorkerThread == NULL);
    DEBUGCHK(g_hOle32 == NULL);

    g_hOle32 = LoadLibrary(L"\\windows\\ole32.dll");
	if (g_hOle32 == NULL) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES: Can not start CoFreeUnusedLibrariesThread, cannot load ole32.dll, GLE=0x%08x\r\n",GetLastError()));
		return;
	}

	g_pfnCoFreeUnusedLibraries = (PFN_COFREEUNUSEDLIBRARIES) GetProcAddress(g_hOle32,L"CoFreeUnusedLibraries");
	g_pfnCoInitializeEx = (PFN_COINITIALIZEEX) GetProcAddress(g_hOle32,L"CoInitializeEx");
    g_pfnCoUninitialize = (PFN_COUNINITIALIZE) GetProcAddress(g_hOle32,L"CoUninitialize");

    CReg reg(HKEY_LOCAL_MACHINE,L"Services");
    g_freeUnusedLibPeriod = reg.ValueDW(g_freeUnusedLibReg,CO_FREE_UNUSED_SLEEP_MS);
    if (g_freeUnusedLibPeriod == 0)
        g_freeUnusedLibPeriod = CO_FREE_UNUSED_SLEEP_MS;

    if (! (g_pfnCoFreeUnusedLibraries && g_pfnCoInitializeEx && g_pfnCoUninitialize)) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: Can not start CoFreeUnusedLibrariesThread, ole32.dll does not have required exports, GLE=0x%08x\r\n",GetLastError()));
        // This should never happen - even the smallest componentization of ole32.dll
        // should always have these functions
        DEBUGCHK(0);
        FreeLibrary(g_hOle32);
        g_hOle32 = NULL;
    }
    else {
        g_comInited = TRUE;
    }
    
}

// DeInitializes COM related functionality
void DeInitCOM() {
    if (! g_comInited)
        return;

    // DeInitServicesExe has already signaled g_servicesShutdownEvent.
    if (g_hCoFreeWorkerThread) {
        DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Waiting for CoFreeUnusedLibraries thread to complete\r\n"));
        WaitForSingleObject(g_hCoFreeWorkerThread,INFINITE);
        CloseHandle(g_hCoFreeWorkerThread);
        g_hCoFreeWorkerThread = NULL;
    }

    DEBUGCHK(g_hOle32);
    FreeLibrary(g_hOle32);
}

DWORD WINAPI ServicesCoFreeUnusedLibrariesThread(LPVOID lpv) {
	if (FAILED(g_pfnCoInitializeEx(NULL,COINIT_MULTITHREADED))) {
		DEBUGMSG(ZONE_ERROR,(L"SERVICES: CoInitializeEx failed, cannot start ServicesCoFreeUnusedLibrariesThread, GLE=0x%08x\r\n",GetLastError()));
		return 0;
	}

	while (1) {
		DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Sleeping <%d> seconds before next call to CoFreeUnusedLibraries()\r\n",g_freeUnusedLibPeriod / 1000));
		DWORD err = WaitForSingleObject(g_servicesShutdownEvent,g_freeUnusedLibPeriod);
        if (err == WAIT_OBJECT_0) {
            // Indicates that services.exe is shutting down.
            DEBUGCHK(g_servicesExeRunning==FALSE);
            break;
        }

		DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Making periodic call to CoFreeUnusedLibraries()\r\n"));
		g_pfnCoFreeUnusedLibraries();
	}

    g_pfnCoUninitialize();
	return 0;
}

// If needed and possible, spin up a thread to periodically call
// CoFreeUnusedLibraries.  This saves each service from having to spin 
// up their own thread to make this call
void StartCoFreeUnusedLibraryThread() {
    DEBUGCHK(g_pServicesLock->IsLocked());

    if (! g_comInited) {
        // COM was not successfully initialized
        return;
    }

    if (g_hCoFreeWorkerThread) {
        // The worker thread is already running
        return;
    }

    DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Spinning up thread to periodically call CoFreeUnusedLibraries\r\n"));

    g_hCoFreeWorkerThread = CreateThread(NULL, 0, ServicesCoFreeUnusedLibrariesThread, NULL, 0, NULL);
    if (g_hCoFreeWorkerThread == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: CreateThread for ServicesCoFreeUnusedLibrariesThread failed, GLE=<0x%08x>\r\n",GetLastError()));
    }
}

