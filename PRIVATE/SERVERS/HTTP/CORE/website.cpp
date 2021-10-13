//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: Website.CPP
Abstract:    Maps IP addresses to a particular website
--*/


#include "httpd.h"


// 
// iphlpapi helper routines
//


//
// Init/deinitialization of web server
//
void CleanupGlobalMemory(DWORD dwWebsites) {
	EnterCriticalSection(&g_CritSect);

#if defined (DEBUG) || defined (_DEBUG)
	DEBUGCHK(g_fState != SERVICE_STATE_ON);
	
	if (!g_pWebsites)
		DEBUGCHK(g_cWebsites == 0);

	DEBUGCHK(dwWebsites <= g_cWebsites);
	if (g_pWebsites)
		DEBUGCHK(g_pVars);

	if (dwWebsites == 0)
		DEBUGCHK(g_cWebsites == 0);
#endif

	// We must cleanup any non-default websites first.
	// We use dwWebsites and not g_cWebsites as the index counter in the event 
	// that something went wrong on startup and not all the values are initialized.

	for (DWORD i = 0; i < dwWebsites; i++)
		g_pWebsites[i].DeInitGlobals();

	MyFree(g_pWebsites);
	g_cWebsites = 0;

	if (g_pVars) {
		delete g_pVars;
		g_pVars = NULL;
	}
	LeaveCriticalSection(&g_CritSect);
}


BOOL InitializeGlobals(BOOL fServicesModel) {
	EnterCriticalSection(&g_CritSect);
	BOOL fRet = FALSE;
	CReg webSites;
	DWORD dwWebsites;

	DEBUGCHK (g_fState == SERVICE_STATE_STARTING_UP && !g_pVars && !g_pRequestList);
	g_pRequestList = NULL;

	svsutil_ResetInterfaceMapper();

	g_pVars = new CGlobalVariables(NULL);

	if (NULL == g_pVars || !g_pVars->m_fAcceptConnections) {
		g_fState = SERVICE_STATE_SHUTTING_DOWN;
		CleanupGlobalMemory(0);

		g_fState = SERVICE_STATE_OFF;
		goto done;
	}

	// Filters may make reference to logging global var, so make call to filters 
	// outside constructor.  Filters are global and are not setup per-website, so this
	// is the only time we call this function.
	g_pVars->m_fFilters = InitFilters();  

	webSites.Open(HKEY_LOCAL_MACHINE,RK_WEBSITES);

	if (webSites.IsOK() && (0 != (dwWebsites = webSites.NumSubkeys()))) {
		WCHAR szSiteName[MAX_PATH];
		if (NULL == (g_pWebsites = MyRgAllocNZ(CGlobalVariables,dwWebsites))) {
			CleanupGlobalMemory(0);

			g_fState = SERVICE_STATE_OFF;
			goto done;
		}

		for (DWORD i = 0; i < dwWebsites; i++) {
			webSites.EnumKey(szSiteName,SVSUTIL_ARRLEN(szSiteName));
			CReg subReg(webSites,szSiteName);

			if (! subReg.IsOK())
				break;

			g_pWebsites[i].InitGlobals(&subReg);

			if (! g_pWebsites[i].m_fAcceptConnections)
				break;
			g_cWebsites++;
		}

		if (g_cWebsites != i) {
			CleanupGlobalMemory(i);
			g_fState = SERVICE_STATE_OFF;
			goto done;
		}
	}

#if 0
	// This was needed in old device.exe model.  Now we have HTTPD automatically
	// registered by device.exe/services.exe on startup w/appropriate registry keys.
	if (!g_fRegistered && (SERVICE_CALLER_PROCESS_DEVICE_EXE == g_CallerExe)) {
		if (NULL == RegisterDevice(HTTPD_DEV_PREFIX,HTTPD_DEV_INDEX,L"httpd.dll",1))
			DEBUGMSG(ZONE_INIT,(L"HTTPD:  Register device failed, GLE = 0x%08x\r\n",GetLastError()));
		g_fRegistered = TRUE;
	}
#endif // 0

	if (fServicesModel)
		g_fState = SERVICE_STATE_ON;

	fRet = TRUE;
done:
	LeaveCriticalSection(&g_CritSect);
	return fRet;
}

// Called on incoming connection, maps to appropriate website.
CGlobalVariables* MapWebsite(SOCKET sock, PSTR szHostHeader) {
	CGlobalVariables       *pWebsite = NULL;

	PCSTR szAdapterName;
	DWORD i;

	// Need to hold critical section because HandleNotifyAddrChange() can come in 
	// at any time with an IP address notification change.
	EnterCriticalSection(&g_CritSect);
	svsutil_LockInterfaceMapper();

	szAdapterName = svsutil_GetAdapterNameOfConnection(sock);

	if (g_pVars->IsMapped(szAdapterName,szHostHeader)) {
		pWebsite = g_pVars;
		goto done;
	}

	for (i = 0; i < g_cWebsites; i++) {
		if (g_pWebsites[i].IsMapped(szAdapterName,szHostHeader)) {
			pWebsite = &g_pWebsites[i];
			goto done;
		}
	}
	DEBUGCHK(pWebsite == NULL);

done:
	svsutil_UnLockInterfaceMapper();
	LeaveCriticalSection(&g_CritSect);

	if (!pWebsite)
		return g_pVars->m_fUseDefaultSite ? g_pVars : NULL;

	return pWebsite;
}


