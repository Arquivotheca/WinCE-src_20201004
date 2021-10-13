//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "proxydev.h"
#include "proxydbg.h"

CSrvMgr 	g_SvcMgr;			// Controls service-specific behavior
CHttpProxy* g_pHttpProxy;		// Controls the behavior of the web proxy
HINSTANCE g_hInst;


extern "C" BOOL WINAPI DllMain( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	if (DLL_PROCESS_ATTACH == fdwReason) {
		DisableThreadLibraryCalls((HMODULE)hInstDll);
		DEBUGREGISTER((HINSTANCE)hInstDll);
		g_hInst = (HINSTANCE)hInstDll;
	}
	
	return TRUE;
}


// 
// The following PRX_xxxxx functions are called by services.exe to control this particular
// services dll.
//

extern "C" DWORD PRX_Init (DWORD dwData)
{
	BOOL fRetVal = FALSE;
	DWORD dwErr;

	if (SERVICE_STATE_OFF != g_SvcMgr.GetState()) {
		SetLastError(ERROR_ALREADY_INITIALIZED);
		return fRetVal;
	}

	DebugInit();

	g_SvcMgr.Lock();
	
	// Create the main web proxy object
	g_pHttpProxy = new CHttpProxy;
	if (! g_pHttpProxy) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
		SetLastError(ERROR_OUTOFMEMORY);
		goto exit;
	}

	dwErr = g_pHttpProxy->Init();
	if (ERROR_SUCCESS != dwErr) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error initializing Web Proxy service.\n")));
		SetLastError(dwErr);
		goto exit;
	}

	// Start the web proxy
	dwErr = g_pHttpProxy->Start();
	if (ERROR_SUCCESS != dwErr) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error starting Web Proxy service.\n")));
		SetLastError(dwErr);
		dwErr = g_pHttpProxy->Deinit();

		// If Deinit failed we may not have cleaned up all resources
		ASSERT(ERROR_SUCCESS == dwErr);

		goto exit;
	}

	g_SvcMgr.SetState(SERVICE_STATE_ON);
	fRetVal = TRUE;

exit:
	if (FALSE == fRetVal) {
		delete g_pHttpProxy;
		g_pHttpProxy = NULL;
	}
	
	g_SvcMgr.Unlock();
	return fRetVal;
}

extern "C" BOOL PRX_Deinit(DWORD dwData)
{
	BOOL fRetVal = FALSE;
	DWORD dwErr;

	g_SvcMgr.Lock();

	if (! g_pHttpProxy) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error deinitializing Web Proxy service.\n")));
		SetLastError(ERROR_NOT_READY);
		goto exit;
	}

	g_SvcMgr.SetState(SERVICE_STATE_SHUTTING_DOWN);

	// Stop the web proxy	
	dwErr = g_pHttpProxy->Stop();
	if (ERROR_SUCCESS != dwErr) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping Web Proxy service.\n")));
		g_SvcMgr.SetState(SERVICE_STATE_ON); // If stop() failed then change state back to ON
		SetLastError(dwErr);
		goto exit;
	}

	g_SvcMgr.SetState(SERVICE_STATE_OFF);

	dwErr = g_pHttpProxy->Deinit();
	if (ERROR_SUCCESS != dwErr) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error deinitializing Web Proxy service.\n")));
		SetLastError(dwErr);
		goto exit;
	}

	delete g_pHttpProxy;
	g_pHttpProxy = NULL;
	fRetVal = TRUE;

exit:
	g_SvcMgr.Unlock();
	DebugDeinit();
	
	return TRUE;
}

extern "C" DWORD PRX_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
	return TRUE;
}

extern "C" BOOL PRX_Close (DWORD dwData) 
{
	return TRUE;
}

extern "C" DWORD PRX_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen)
{
	return -1;
}

extern "C" DWORD PRX_Read (DWORD dwData, LPVOID pBuf, DWORD dwLen)
{
	return -1;
}

extern "C" DWORD PRX_Seek (DWORD dwData, long pos, DWORD type)
{
	return (DWORD)-1;
}

extern "C" void PRX_PowerUp(void)
{
	return;
}

extern "C" void PRX_PowerDown(void)
{
	return;
}

extern "C" BOOL PRX_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
	DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
	PDWORD pdwActualOut)
{
	DWORD dwErr;
	
	switch (dwCode) {
		case IOCTL_SERVICE_START:
			g_SvcMgr.Lock();
	
			if (SERVICE_STATE_OFF != g_SvcMgr.GetState()) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error starting Web Proxy service.\n")));
				g_SvcMgr.Unlock();
				SetLastError(ERROR_ALREADY_INITIALIZED);
				return FALSE;
			}

			// Start the web proxy
			dwErr = g_pHttpProxy->Start();
			if (ERROR_SUCCESS != dwErr) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error starting Web Proxy service.\n")));
				g_SvcMgr.Unlock();
				SetLastError(dwErr);
				return FALSE;
			}

			g_SvcMgr.SetState(SERVICE_STATE_ON);
			g_SvcMgr.Unlock();
			return TRUE;

		case IOCTL_SERVICE_REFRESH:
			g_SvcMgr.Lock();
			
			if (SERVICE_STATE_ON == g_SvcMgr.GetState()) {
				g_SvcMgr.SetState(SERVICE_STATE_SHUTTING_DOWN);
				g_pHttpProxy->Stop();
			}

			dwErr = g_pHttpProxy->Start();
			if (ERROR_SUCCESS != dwErr) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error refreshing Web Proxy service.\n")));
				g_SvcMgr.Unlock();
				SetLastError(dwErr);
				return FALSE;
			}
			
			g_SvcMgr.SetState(SERVICE_STATE_ON);
			g_SvcMgr.Unlock();
			return TRUE;
			
		case IOCTL_SERVICE_STOP:
			g_SvcMgr.Lock();
			
			if (SERVICE_STATE_ON != g_SvcMgr.GetState()) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping Web Proxy service.\n")));
				g_SvcMgr.Unlock();
				SetLastError(ERROR_NOT_READY);
				return FALSE;
			}

			g_SvcMgr.SetState(SERVICE_STATE_SHUTTING_DOWN);

			// Stop the web proxy
			dwErr = g_pHttpProxy->Stop();
			if (ERROR_SUCCESS != dwErr) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping Web Proxy service.\n")));
				g_SvcMgr.SetState(SERVICE_STATE_ON); // If Stop() failed then change state back to ON
				SetLastError(dwErr);
				return FALSE;
			}

			g_SvcMgr.SetState(SERVICE_STATE_OFF);
			g_SvcMgr.Unlock();
			return TRUE;

		case IOCTL_SERVICE_STATUS:
			if (pBufOut && dwLenOut == sizeof(DWORD))  {
				*(DWORD *)pBufOut = g_SvcMgr.GetState();
				if (pdwActualOut)
					*pdwActualOut = sizeof(DWORD);
				return TRUE;
			}
			break;

		case IOCTL_SERVICE_NOTIFY_ADDR_CHANGE:

			if ((! pBufIn) || (0 == dwLenIn)) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error notifying Web Proxy of addr change.\n")));
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;	
			}
			
			g_SvcMgr.Lock();
			
			if (SERVICE_STATE_ON != g_SvcMgr.GetState()) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error notifying Web Proxy of addr change.\n")));
				g_SvcMgr.Unlock();
				SetLastError(ERROR_NOT_READY);
				return FALSE;
			}

			// Notify the web proxy that a relevant IP address may have changed
			dwErr = g_pHttpProxy->NotifyAddrChange(pBufIn, dwLenIn);
			if (ERROR_SUCCESS != dwErr) {
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error notifying Web Proxy of addr change.\n")));
				g_SvcMgr.Unlock();
				SetLastError(dwErr);
				return FALSE;
			}
			
			g_SvcMgr.Unlock();
			return TRUE;

		case IOCTL_SERVICE_DEREGISTER_SOCKADDR:
		case IOCTL_SERVICE_DEBUG:
		case IOCTL_SERVICE_STARTED:
		case IOCTL_SERVICE_CONTROL:
			return TRUE;

		case IOCTL_SERVICE_CONNECTION:
		case IOCTL_SERVICE_REGISTER_SOCKADDR:
			return FALSE;
	}

	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
}


//
// The following methods belong to the CSvcMgr class which is used by the web proxy
// to control the state of the service.
//

CSrvMgr::CSrvMgr()
{
	m_dwState = SERVICE_STATE_OFF;
}

CSrvMgr::~CSrvMgr()
{
}

DWORD CSrvMgr::GetState(void)
{
	return m_dwState;
}

void CSrvMgr::SetState(DWORD dwState)
{
	m_dwState = dwState;
}


