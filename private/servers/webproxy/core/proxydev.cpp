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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "proxydev.h"
#include "proxydbg.h"

CHttpProxy* g_pHttpProxy;        // Controls the behavior of the web proxy
DWORD g_dwState;
CRITICAL_SECTION g_csService;
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

    if (SERVICE_STATE_OFF != g_dwState) {
        SetLastError(ERROR_ALREADY_INITIALIZED);
        return fRetVal;
    }

    DebugInit();
    
    ServiceInitLock;
    ServiceLock;
    
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

    g_dwState = SERVICE_STATE_ON;
    fRetVal = TRUE;

exit:
    if (FALSE == fRetVal) {
        delete g_pHttpProxy;
        g_pHttpProxy = NULL;
    }
    
    ServiceUnlock;
    return fRetVal;
}

extern "C" BOOL PRX_Deinit(DWORD dwData)
{
    BOOL fRetVal = FALSE;
    DWORD dwErr;

    if (! g_pHttpProxy) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error deinitializing Web Proxy service.\n")));
        SetLastError(ERROR_NOT_READY);
        goto exit;
    }

    ServiceLock;

    if (SERVICE_STATE_ON == g_dwState) {
        // Stop the web proxy
        dwErr = g_pHttpProxy->Stop();
        if (ERROR_SUCCESS != dwErr) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping Web Proxy service.\n")));
            g_dwState = SERVICE_STATE_ON; // If stop() failed then change state back to ON
            SetLastError(dwErr);
            goto exit;
        }

        g_dwState = SERVICE_STATE_OFF;
    }
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
    ServiceUnlock;
    ServiceDeinitLock;
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
            ServiceLock;
    
            if (SERVICE_STATE_OFF != g_dwState) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error starting Web Proxy service.\n")));
                ServiceUnlock;
                SetLastError(ERROR_ALREADY_INITIALIZED);
                return FALSE;
            }

            // Start the web proxy
            dwErr = g_pHttpProxy->Start();
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error starting Web Proxy service.\n")));
                ServiceUnlock;
                SetLastError(dwErr);
                return FALSE;
            }

            g_dwState = SERVICE_STATE_ON;
            ServiceUnlock;
            return TRUE;

        case IOCTL_SERVICE_REFRESH:
            ServiceLock;
            
            if (SERVICE_STATE_ON == g_dwState) {
                g_dwState = SERVICE_STATE_SHUTTING_DOWN;
                g_pHttpProxy->Stop();
            }

            dwErr = g_pHttpProxy->Start();
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error refreshing Web Proxy service.\n")));
                ServiceUnlock;
                SetLastError(dwErr);
                return FALSE;
            }
            
            g_dwState = SERVICE_STATE_ON;
            ServiceUnlock;
            return TRUE;
            
        case IOCTL_SERVICE_STOP:
            ServiceLock;
            
            if (SERVICE_STATE_ON != g_dwState) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping Web Proxy service.\n")));
                ServiceUnlock;
                SetLastError(ERROR_NOT_READY);
                return FALSE;
            }

            g_dwState = SERVICE_STATE_SHUTTING_DOWN;

            // Stop the web proxy
            dwErr = g_pHttpProxy->Stop();
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error stopping Web Proxy service.\n")));
                g_dwState = SERVICE_STATE_ON; // If Stop() failed then change state back to ON
                ServiceUnlock;
                SetLastError(dwErr);
                return FALSE;
            }

            g_dwState = SERVICE_STATE_OFF;
            ServiceUnlock;
            return TRUE;

        case IOCTL_SERVICE_STATUS:
            __try {
                if (!pBufOut || dwLenOut < sizeof(DWORD)) {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return FALSE;
                }
                *(DWORD *)pBufOut = g_dwState;
                if (pdwActualOut)
                    *pdwActualOut = sizeof(DWORD);
            }
            __except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            return TRUE;
        break;

        case IOCTL_SERVICE_NOTIFY_ADDR_CHANGE:

            if ((! pBufIn) || (0 == dwLenIn)) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error notifying Web Proxy of addr change: ERROR_INVALID_PARAMTER.\n")));
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;    
            }
            
            ServiceLock;
            
            if (SERVICE_STATE_ON != g_dwState) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error notifying Web Proxy of addr change: ERROR_NOT_READY.\n")));
                ServiceUnlock;
                SetLastError(ERROR_NOT_READY);
                return FALSE;
            }

            // Notify the web proxy that a relevant IP address may have changed
            dwErr = g_pHttpProxy->NotifyAddrChange();
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error notifying Web Proxy of addr change.\n")));
                ServiceUnlock;
                SetLastError(dwErr);
                return FALSE;
            }
            
            ServiceUnlock;
            return TRUE;

        case IOCTL_SERVICE_PRX_SIGNAL_FILTER:
            DWORD dwSignal;
            
            if ((! pBufIn) || (0 == dwLenIn)) {
                IFDBG(DebugOut(ZONE_ERROR, (_T("WebProxy: Error in call to IOCTL_SERVICE_PRX_SIGNAL_FILTER.\n"))));
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (SERVICE_STATE_ON != g_dwState) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Service must be ON to signal filter.\n")));
                SetLastError(ERROR_NOT_READY);
                return FALSE;
            }

            __try {
                dwSignal = (DWORD)*pBufIn;
            }
            __except (ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
                IFDBG(DebugOut(ZONE_ERROR, (_T("WebProxy: Caught exception in IOCTL_SERVICE_PRX_SIGNAL_FILTER - pBufIn is not a valid DWORD*.\n"))));
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            
            dwErr = g_pHttpProxy->SignalFilter(dwSignal);
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error signalling proxy filter: %d.\n"), dwErr));
                SetLastError(dwErr);
                return FALSE;
            }
            
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


