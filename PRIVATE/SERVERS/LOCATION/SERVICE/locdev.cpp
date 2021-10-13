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

// Abstract: Services.exe interface layer for Location Framework Service

#include <locCore.hpp>

// Called when services.exe first loads the location framework DLL
extern "C" DWORD LOC_Init (DWORD dwData) {
    DEBUGMSG(ZONE_SERVICE | ZONE_INIT,(L"LOC: LOC_Init(0x%08x)\r\n",dwData));

    DWORD err;

    g_pLocLock->Lock();
    if (g_serviceState != SERVICE_STATE_UNINITIALIZED) {
        // This indicates LOC_Init has been called already.  Don't allow multiple
        // calls into this function
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: LOC_Init() called when service state != UNINITIALIZED.\r\n"));
        err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
        goto done;
    }

    g_serviceState = SERVICE_STATE_STARTING_UP;

    if (0 == g_pThreadPool->ScheduleEvent(StartLocServiceThread, (LPVOID)TRUE)) {
        DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Could not spin worker thread to initialize service\r\n"));
        err = ERROR_OUTOFMEMORY; // Best guess as to failure
        goto done;
    }
    DEBUGMSG(ZONE_INIT | ZONE_THREAD,(L"LOC: Spinning thread to load location framework on service initialization\r\n"));

    err = ERROR_SUCCESS;
done:
    g_pLocLock->Unlock();
    if (err != ERROR_SUCCESS) {
        SetLastError(err);
        return 0;
    }

    return 1;
}

extern "C" BOOL LOC_Deinit(DWORD dwData) {
    DEBUGMSG(ZONE_SERVICE | ZONE_INIT,(L"LOC: LOC_Deinit(0x%08x)\r\n",dwData));
    return LocationSpinThreadAndWait(StopLocServiceThread,(LPVOID)TRUE);
}

extern "C" DWORD LOC_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode) {
    DEBUGMSG(ZONE_SERVICE,(L"LOC: LOC_Open(0x%08x,0x%08x,0x%08x)\r\n",dwData,dwAccess,dwShareMode));
    g_pLocLock->Lock();
    DWORD err = (DWORD)g_pHandleMgr->AllocServiceHandle();
    g_pLocLock->Unlock();
    return err;
}

extern "C" BOOL LOC_Close (DWORD dwData)  {
    DEBUGMSG(ZONE_SERVICE,(L"LOC: LOC_Close(0x%08x)\r\n",dwData));
    g_pLocLock->Lock();
    BOOL ret = (ERROR_SUCCESS == g_pHandleMgr->FreeServiceHandle((HANDLE)dwData));
    g_pLocLock->Unlock();
    return ret;
}

extern "C" BOOL LOC_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
                          DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
                          PDWORD pdwActualOut)  {
    DEBUGMSG(ZONE_FUNCTION, (L"LOC: LOC_IOControl(0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x)\r\n",
                            dwData,dwCode,pBufIn,dwLenIn,pBufOut,dwLenOut,pdwActualOut));

    BOOL ret = FALSE;

    // Control codes relating to changing service state must NOT have lock
    // held at this stage
    switch (dwCode) {
    case IOCTL_SERVICE_START:
        // Bring location service into started state.  Do start/stop/refresh
        // operations on a worker thread to avoid PSL problems.
        return LocationSpinThreadAndWait(StartLocServiceThread,(LPVOID)FALSE);
    break;

    case IOCTL_SERVICE_STOP:
        // Stop the location service
        return LocationSpinThreadAndWait(StopLocServiceThread,(LPVOID)FALSE);
    break;

    case IOCTL_SERVICE_REFRESH:
        // Stop and then start the location service
        return LocationSpinThreadAndWait(RefreshLocServiceThread,(LPVOID)FALSE);
    break;

    default:
        ;
    }

    
    g_pLocLock->Lock(); // Remaining IOCTL codes do expect the lock.
    switch (dwCode) {
    case IOCTL_SERVICE_STATUS:
        ret = GetServiceStatus(pBufOut,dwLenOut,pdwActualOut);
    break;

    case IOCTL_LF_INVOKE:
    {
        serviceHandle_t *pServiceHandle = g_pHandleMgr->FindServiceHandle((HANDLE)dwData);

        if (NULL == pServiceHandle) {
            // Caller sets last error to appropriate error code.
            break;
        }

        if (g_serviceState != SERVICE_STATE_ON) {
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: Attempt to use Location API but service state is not on, in state <%d>\r\n",g_serviceState));
            SetLastError(ERROR_SERVICE_NOT_ACTIVE);
            ret = FALSE;
            break;
        }

        ce::psl_stub<> stub(pBufIn, dwLenIn);

        switch (stub.function()) {
        case IOCTL_LF_OPEN:
            ret = stub.call(LocationOpenIntrnl,pServiceHandle);
        break;

        case IOCTL_LF_CLOSE:
            ret = stub.call(LocationCloseIntrnl,pServiceHandle);
        break;

        case IOCTL_LF_REGISTER_FOR_REPORT:
            ret = stub.call(LocationRegisterForReportIntrnl,pServiceHandle);
        break;
            
        case IOCTL_LF_UN_REGISTER_FOR_REPORT:
            ret = stub.call(LocationUnRegisterForReportIntrnl,pServiceHandle);
        break;
        
        case IOCTL_LF_GET_REPORT:
            ret = stub.call(LocationGetReportIntrnl,pServiceHandle);
        break;
        
        case IOCTL_LF_GET_SERVICE_STATE:
            ret = stub.call(LocationGetServiceStateIntrnl,pServiceHandle);
        break;
        
        case IOCTL_LF_GET_PLUGIN_INFO_FOR_REPORT:
            ret = stub.call(LocationGetPluginInfoForReportIntrnl,pServiceHandle);
        break;
        
        case IOCTL_LF_GET_PROVIDERS_INFO:
            ret = stub.call(LocationGetProvidersInfoIntrnl,pServiceHandle);
        break;
        
        case IOCTL_LF_GET_RESOLVERS_INFO:
            ret = stub.call(LocationGetResolversInfoIntrnl,pServiceHandle);
        break;
        
        case IOCTL_LF_PLUGIN_OPEN:
            ret = stub.call(LocationPluginOpenPSL,pServiceHandle);
        break;

        case IOCTL_LF_PLUGIN_IOCTL:
            ret = stub.call(LocationPluginIOCTLPSL,pServiceHandle);
        break;

        case IOCTL_LF_PLUGIN_CLOSE:
            ret = stub.call(LocationPluginClosePSL,pServiceHandle);
        break;

        default:
            DEBUGMSG(ZONE_ERROR,(L"LOC: ERROR: IOCTL_LF_INVOKE received unsupported function code\r\n"));
            DEBUGCHK(0); // Totally bogus argument - services.exe shouldn't generate anything in this range, so complain.
            SetLastError(ERROR_INVALID_PARAMETER);
        break;

        }
    }
    break;

    }

    g_pLocLock->Unlock();
    return ret;
}

#ifndef LOC_WHITEBOX_TESTING
// Whitebox tests are implemented as DLLs.  Cannot have multiply defined DllMain.

extern "C" BOOL WINAPI DllEntry(HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved) { 
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls((HMODULE)hInstDll);
            DEBUGREGISTER((HINSTANCE)hInstDll);
            return InitLocService();
            break;

        case DLL_PROCESS_DETACH:
            DeInitLocService();
            break;
    }
    return TRUE;
}

#endif


