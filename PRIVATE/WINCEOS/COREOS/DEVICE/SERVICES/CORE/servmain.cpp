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
// This module is implementing the Services.exe reflector.  
// It receives application requests that are sent to it by the kernel
// and marshalls these on (if possible) to the appropriate service DLL.
//

#include <servmain.hpp>

// Global services.exe critical section.  This controls services.exe specific
// funcionality - namely CoFreeUnusedLibraries thread, IP change notification
// thread, and adding and removing super services.
// This is NOT the lock for adding and removing new services into the global
// list.  This is obtained via g_pUserDriverContainer->Lock/Unlock.
SVSSynch *g_pServicesLock;
// Number of seconds to wait should a transient system error occur
static const DWORD g_errorRetrySleep = (10*1000);
// Is services.exe running or has it received a shutdown notification?
BOOL g_servicesExeRunning;
// Event signaled when services.exe it going to shutdown
HANDLE g_servicesShutdownEvent;

// Called to initialize services.exe before any services are loaded
BOOL InitServicesExe() {
    DEBUGCHK(g_pServicesLock == FALSE);
    DEBUGCHK(g_servicesExeRunning == FALSE);

    DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Beginning init of services.exe\r\n"));

    svsutil_Initialize();

    g_pServicesLock = new SVSSynch();
    if (! g_pServicesLock) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Memory allocation failed\r\n"));
        return FALSE;
    }

    g_servicesShutdownEvent = CreateEvent(NULL,TRUE,FALSE,NULL); // manual reset
    if (!g_servicesShutdownEvent) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: CreateEvent failed, GLE=<0x%08x>\r\n",GetLastError()));
        return FALSE;
    }

    InitWinsock();
    InitIphlapi();
    InitCOM();

    g_servicesExeRunning = TRUE;

    DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Completed init of services.exe structures (not services themselves)\r\n"));

    return TRUE;
}

// Called to deinitialize services.exe after the last service has been unloaded.
void DeInitServicesExe() {
    g_servicesExeRunning = FALSE;

    DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Beginning deinit of services.exe\r\n"));

    if (g_servicesShutdownEvent)
        SetEvent(g_servicesShutdownEvent);

    DeInitCOM();
    DeInitIphlapi();
    DeInitWinsock();

    if (g_servicesShutdownEvent) {
        CloseHandle(g_servicesShutdownEvent);
        g_servicesShutdownEvent = NULL;
    }

    if (g_pServicesLock) {
        delete g_pServicesLock;
        g_pServicesLock = NULL;
    }


    svsutil_DeInitialize();

    DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Completed deinit of services.exe\r\n"));
}


// A new service is to be loaded - create a ServiceDriver for it and
// insert it into the global list.
UserDriver * CreateUserModeDriver(FNDRIVERLOAD_PARAM& fnDriverLoadParam)
{
    if (0 != (fnDriverLoadParam.dwFlags & ~SERV_DEVLOAD_FLAGS_SUPPORTED)) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: Driver flags = <0x%08x> are invalid; services.exe doesn't only supports up to <0x%08x>\r\n",
                 fnDriverLoadParam.dwFlags,SERV_DEVLOAD_FLAGS_SUPPORTED));
        return NULL;
    }
    
    // Services are always loaded via LoadLibrary, not LoadDriver,
    // regardless of whether it was explicitly set in the registry or not.
    fnDriverLoadParam.dwFlags |= DEVFLAGS_LOADLIBRARY;
    return new ServiceDriver(fnDriverLoadParam,NULL);
}

// This routine is the entry point for the device manager.  It simply calls the device
// management DLL's entry point.
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
{
    int status=-1;
    RETAILMSG(1,(TEXT("hello servicesd.exe %s \r\n"),lpCmdLine));    
    DEBUGREGISTER(NULL);

    if (! InitServicesExe())
        goto done;

    if (RegisterAFSAPI(lpCmdLine)) {
        BOOL bRet = (WaitForPrimaryThreadExit(INFINITE) ==  WAIT_OBJECT_0) ;
        ASSERT(bRet);
    }
    else
        ASSERT(FALSE);
    RETAILMSG(1,(TEXT("exiting hello servicesd.exe\r\n")));    

done:
    UnRegisterAFSAPI();
    DeInitServicesExe();

    return status;
}


typedef DWORD (WINAPI *PFN_SERVICE_THREAD)(LPVOID lpv);

// A number of operatations, in particular anything using TLS, must not
// be performed in the context of a process calling into services.exe via a PSL
// but must instead be called with services.exe as the owner.  To make this
// happen we create a thread in services.exe and block until its completed.

BOOL ServiceSpinThreadAndWait(PFN_SERVICE_THREAD pFunc, LPVOID lpv) {
    HANDLE hThread = CreateThread(NULL,0,pFunc,lpv,0,NULL);

    if (! hThread) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: CreateThread failed, GLE=<0x%08x>\r\n",GetLastError()));
        return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    }

    DWORD err;
    WaitForSingleObject(hThread,INFINITE);

    if (! GetExitCodeThread(hThread,&err)) { 
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: GetExitCodeThread failed, GLE=<0x%08x>\r\n",GetLastError()));
        DEBUGCHK(0); // Something is seriously wrong with system or services.exe itself
        err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
    }

    if (err != ERROR_SUCCESS)
        SetLastError(err);

    CloseHandle(hThread);
    return (err==ERROR_SUCCESS);
}

BOOL ServiceDriver::StoreRegistrySettings(FNINIT_PARAM& fnInitParam) {
    if (m_registryKeyName[0]) {
        DEBUGCHK(0); // should not be able to be called more than once
        return FALSE;
    }

    CReg reg(HKEY_LOCAL_MACHINE,fnInitParam.ActiveRegistry);
    if (! reg.IsOK()) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Cannot open registry key <%s>, GLE=<0x%08x>\r\n",fnInitParam.ActiveRegistry,GetLastError()));
        return FALSE;
    }

    if (!reg.ValueSZ(DEVLOAD_DEVKEY_VALNAME,m_registryKeyName,SVSUTIL_ARRLEN(m_registryKeyName))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Cannot open read \"Key\" value for service <%s>\r\n",fnInitParam.ActiveRegistry));
        return FALSE;
    }

    if (! reg.ValueSZ(DEVLOAD_DEVNAME_VALNAME,m_prefix,PREFIX_LEN)) {
        // Not a fatal error.  Means that 'services list' will not
        // be able to retrieve the prefix name for this service.
        m_prefix[0] = 0;
    }


    return TRUE;
}

const WCHAR g_acceptRegKeySuffix[]  = L"\\Accept";
const DWORD g_acceptRegKeySuffixLen = SVSUTIL_CONSTSTRLEN(g_acceptRegKeySuffix);

// Retrieves registry key name for HKLM\Services\<ThisService\Accept.
BOOL ServiceDriver::GetAcceptRegKey(WCHAR *acceptRegKey, DWORD acceptRegKeyLen) {
    DWORD registryKeyNameLen = wcslen(m_registryKeyName);
    if ((registryKeyNameLen + g_acceptRegKeySuffixLen + 1) >= acceptRegKeyLen) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Registry key for <%s>\\Accept is > <%d> characters\r\n",acceptRegKey,acceptRegKeyLen));
        return FALSE;
    }

    wcscpy(acceptRegKey,m_registryKeyName);
    wcscpy(acceptRegKey+registryKeyNameLen,g_acceptRegKeySuffix);
    return TRUE;
}

static DWORD DriverInitThread(LPVOID lpv) {
    return ((ServiceDriver*)lpv)->DriverInitThread();
}

static DWORD DriverDeinitThread(LPVOID lpv) {
    return ((ServiceDriver*)lpv)->DriverDeinitThread();
}

static DWORD InitThread(LPVOID lpv) {
    return ((ServiceDriver*)lpv)->InitThread();
}

static DWORD FreeServiceLibraryThread(LPVOID lpv) {
    ((UserDriver*)lpv)->FreeDriverLibrary();
    return 0;
}

// Since services can support certain features not available in 
// udevice.exe, after a successful xxx_Init() call we must 
// examine services's config flags to determine if it needs those
// extra functions.
// This function is called by DriverInit PSL in order to call into
// service's xxx_Init().  Many services in particular use COM API's,
// which require TLS (not PSL safe)
DWORD ServiceDriver::DriverInitThread() {
    // Debug verify this is not being called directly from a PSL thread
    DEBUGCHK((DWORD)GetOwnerProcess() == GetCurrentProcessId());

    BOOL ret;

    DetermineReflectorHandle(m_fnInitParam.ActiveRegistry);

    __try {
        // Use service context, not whatever happens to be in context (possible
        // registry key) to keep compat with existing services.
        m_dwInitData = m_fnInit(m_serviceContext,m_fnInitParam.lpvParam);
        ret = (m_dwInitData!=0);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        ret =  FALSE;
    }
    if (!ret && m_hReflector) {
        CloseHandle(m_hReflector);
        m_hReflector = NULL;
    }

    if (! ret)
        return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;

    g_pServicesLock->Lock();

    // If we ever get multiple instances of servicesd.exe to run on 
    // the device, we should check SERVICE_COFREE_UNUSED_THREAD and 
    // SERVICE_NET_ADDR_CHANGE_THREAD before spinning up these threads (at least
    // in the non-default servicesd.exe).
    // Since we only have 1 servicesd.exe instance for now, always spin the threads
    // up.

    StartCoFreeUnusedLibraryThread(); // SERVICE_COFREE_UNUSED_THREAD
    StartIPAddrChangeNotificationThread(); // SERVICE_NET_ADDR_CHANGE_THREAD
    if (m_serviceContext & SERVICE_INIT_STOPPED)
        StartListeningOnSuperServicePorts(TRUE,TRUE);
    g_pServicesLock->Unlock();

    return ERROR_SUCCESS;
}


// Need to load service DLLs on a worker thread
BOOL ServiceDriver::Init() 
{
    return ServiceSpinThreadAndWait(::InitThread,this);
}

DWORD ServiceDriver::InitThread()
{
    if (UserDriver::Init())
        return ERROR_SUCCESS;

    // UserDriver::Init() err code doesn't get propogated back so
    // it doesn't matter what we set it to.
    return ERROR_INTERNAL_ERROR;
}

BOOL ServiceDriver::DriverInit(FNINIT_PARAM& fnInitParam) {
    // Need to store information about the service we won't 
    // be able to access at a later stage.
    if (! StoreRegistrySettings(fnInitParam))
        return FALSE;

    // Check the optional flags to see if this service needs any
    // other functionality that services.exe can support.
    CReg reg(HKEY_LOCAL_MACHINE,m_registryKeyName);
    m_serviceContext = reg.ValueDW(L"ServiceContext");

    memcpy(&m_fnInitParam,&fnInitParam,sizeof(fnInitParam));
    return ServiceSpinThreadAndWait(::DriverInitThread,this);
}

// Like DriverInitThread(), this must be overridden to support
// services.exe specific functionality and must be on a non-PSL
// thread since many services will deinit COM (not PSL safe) in 
// their xxx_DeInit call.
DWORD ServiceDriver::DriverDeinitThread() {
    // The base functionality for loading the service is the same as
    // if it was a user-mode device driver in udevice.dll

    DWORD err;
    if (UserDriver::DriverDeinit())
        err = ERROR_SUCCESS;
    else
        err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;

    g_pServicesLock->Lock();

    if (m_serviceContext & SERVICE_INIT_STOPPED) {
        // Now that the service is being unloaded, any sockets
        // it may be listening on must be stopped now.
        StopListeningOnSuperServicePorts(TRUE);
    }

    g_pServicesLock->Unlock();
    return err;
}



void ServiceDriver::FreeDriverLibrary() {
    ServiceSpinThreadAndWait(::FreeServiceLibraryThread,this);
}


BOOL ServiceDriver::DriverDeinit() {
    return ServiceSpinThreadAndWait(::DriverDeinitThread,this);
}

BOOL ServiceDriver::Ioctl(FNIOCTL_PARAM& fnIoctlParam) {
    BOOL ret;
    DWORD ioctlCode = fnIoctlParam.dwIoControlCode;

    if (! IsContainFileContent(fnIoctlParam.dwContent)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // There are a few well-defined IOCTLs that are used to implement
    // services.exe APIs; currently related to adding or removing
    // super services ports.  If one of these is sent, then it 
    // is processed and the underlying service is not forwarded the IOCTL.
    switch (ioctlCode) {
    case IOCTL_SERVICE_INTERNAL_SERVICE_ADD_PORT:
        return ServiceAddPortInternal(fnIoctlParam);
        break;

    case IOCTL_SERVICE_INTERNAL_SERVICE_CLOSE_PORT:
        return ServiceClosePortInternal(fnIoctlParam);
        break;

    case IOCTL_SERVICE_INTERNAL_SERVICE_UNBIND_PORTS:
        return ServiceUnbindPortsInternal(fnIoctlParam);
        break;

    // default case is that the service processes this IOCTL
    }

    ret = UserDriver::Ioctl(fnIoctlParam);

    if (ret && (m_serviceContext & SERVICE_INIT_STOPPED) && (ioctlCode == IOCTL_SERVICE_STOP)) {
        // If IOCTL_SERVICE_STOP was sent to a super-service,
        // then if there are any network ports bound to the service we must stop
        // listening on them now
        StopListeningOnSuperServicePorts(FALSE);
    }

    if (ret && (m_serviceContext & SERVICE_INIT_STOPPED) && (ioctlCode == IOCTL_SERVICE_START)) {
        // If IOCTL_SERVICE_START was sent to a super-service, then 
        // we must rebind any network ports (the assumption being that
        // the service was previously turned off and unbound ports)
        StartListeningOnSuperServicePorts(FALSE,FALSE);
    }

    return ret;
}

BOOL ServiceDriver::DriverPowerUp() {
    // Services.exe should never be called for power change notifications so
    // do not pass this notification onto the service.  Services that need these 
    // notifications are not services, and should be moved to udevice or the kernel.
    return FALSE;
}

BOOL ServiceDriver::DriverPowerDown() {
    // See ServiceDriver::DriverPowerUp comments.
    return FALSE;
}

// Services.exe in some circumstances can directly generate IOCTL calls
// to services, for instance IOCTL_SERVICE_CONNECTION and IOCTL_SERVICE_NOTIFY_ADDR_CHANGE.
// Since it's services.exe and not an app, we can have a fast path call here.
BOOL ServiceDriver::InternalIOCTL(DWORD ioControlCode, BYTE *pInBuf, DWORD inBufLen, BYTE *pOutBuf, DWORD outBufLen) {
    DEBUGCHK(g_pServicesLock->IsLocked());

    BOOL  ret;
    DWORD actualOut;

    // For IOCTLs generated from a PSL, the caller (udevice.exe or kernel itself)
    // has extra ref count on us.  Since InternalIOCTL() is generated by a 
    // thread in services.exe space, we need the extra add-ref here.
    AddRef();

    g_pServicesLock->Unlock();

    __try {
        ret = m_fnControl(m_dwInitData,ioControlCode,pInBuf,inBufLen,
                          pOutBuf, outBufLen, &actualOut);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        ret = FALSE;
    }

    g_pServicesLock->Lock();

    DeRef();
    return ret;
}

// Makes sure that the given service is indeed a super-service
BOOL ServiceDriver::VerifyIsASuperServer() {
    if (m_serviceContext & SERVICE_INIT_STOPPED)
        return TRUE;

    DEBUGMSG(ZONE_ERROR,(L"SERVICES: Service <%s> is not a super-service and cannot have super-service APIs called on it\r\n",
                           m_registryKeyName));
    SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
    return FALSE;
}

// Print out the "friendly" prefix (e.g. "TEL0:") of this service instance
void ServiceDriver::WritePrefix(WCHAR *szPrefix) {
    // Note that m_fnDriverLoadParam.Prefix only contains the first
    // 3 letters ("TEL") but not the index and ":".  For BC, 
    // we must return these as well here.
    wcscpy(szPrefix,m_prefix);
}

//
//  Handle special internal IOCTLs to servicesd.exe itself (not to a particular
//  service), such as enumerating all running services.
//


#pragma warning( disable : 4509 )


// Implements EnumServices API call.
BOOL ServicesGetInfo(PBYTE pBufferUnMrsh, DWORD *pdwServiceEntriesUnMrsh, DWORD *pdwBufferLenUnMrsh) {
    DWORD            err = ERROR_INVALID_PARAMETER;
    ServiceDriver   *pService;
    DWORD            numBytesRequired = 0;
    DWORD            numServices      = 0;
    ServiceEnumInfo *pEnumTrav   = NULL;
    WCHAR           *pWriteTrav;
    WCHAR           *pWriteTravCaller;
    serviceDriver_t  servicesToQuery;

    if (!pdwServiceEntriesUnMrsh || !pdwBufferLenUnMrsh) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MarshalledBuffer_t serviceEntries(pdwServiceEntriesUnMrsh,sizeof(DWORD),ARG_O_PTR);
    DWORD *pdwServiceEntries = (DWORD*)serviceEntries.ptr();
    if (! pdwServiceEntries) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MarshalledBuffer_t bufferLen(pdwBufferLenUnMrsh,sizeof(DWORD),ARG_IO_PTR);
    DWORD *pdwBufferLen = (DWORD*)bufferLen.ptr();
    if (! pdwBufferLen) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MarshalledBuffer_t servEnum(pBufferUnMrsh,*pdwBufferLen,ARG_O_PTR);
    ServiceEnumInfo *pEnumInfo = (ServiceEnumInfo*)servEnum.ptr();

    if (pBufferUnMrsh && !pEnumInfo && (*pdwBufferLen != 0)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    g_pUserDriverContainer->Lock();
    pService = (ServiceDriver*)g_pUserDriverContainer->GetFirstDriver();
    
    for (; pService; pService = (ServiceDriver*)pService->GetNextObject())
    {
        numBytesRequired += (1+wcslen(pService->GetDllName()))*sizeof(WCHAR);
        numServices++;
    }
    numBytesRequired += (sizeof(ServiceEnumInfo) * numServices);

    __try {
        *pdwServiceEntries = numServices;

        if (!pBufferUnMrsh) {
            err = ERROR_INSUFFICIENT_BUFFER;
            goto doneHoldsLock;
        }
        else if (*pdwBufferLen < numBytesRequired) {
            err = ERROR_MORE_DATA;
            goto doneHoldsLock;
        }

        pService = (ServiceDriver*)g_pUserDriverContainer->GetFirstDriver();
        pEnumTrav  = pEnumInfo;
        // pWriteTravCaller is the pointer the caller will access, based off original ptr passed
        pWriteTravCaller = (WCHAR*) (pBufferUnMrsh + (numServices * sizeof(ServiceEnumInfo)));
        // pWriteTrav is the pointer in servicesd.exe space that will be written to
        pWriteTrav = (WCHAR*) ((CHAR*)pEnumInfo + (numServices * sizeof(ServiceEnumInfo)));

        for (; pService; pService = (ServiceDriver*)pService->GetNextObject())
        {
            pService->WritePrefix(pEnumTrav->szPrefix);
            pEnumTrav->hServiceHandle = NULL; // NOTE: This was old GetServiceHandle(), no longer supported

            wcscpy(pWriteTrav,pService->GetDllName());
            pEnumTrav->szDllName = pWriteTravCaller;
            pWriteTrav += (wcslen(pService->GetDllName()) + 1);
            pWriteTravCaller += (wcslen(pService->GetDllName()) + 1);
            // Increase ref count and put service into list to notify.  We
            // must do this because we must release lock to call into the service
            // themselves.
            pService->AddRef();
            servicesToQuery.push_back(pService);

            pEnumTrav++;
        }

        pEnumTrav = (ServiceEnumInfo*) pEnumInfo;
        g_pUserDriverContainer->Unlock();

        // Enumerate all services to determine their state
        serviceDriverIter_t it    = servicesToQuery.begin();
        serviceDriverIter_t itEnd = servicesToQuery.end();

        g_pServicesLock->Lock();
        for (; it != itEnd; ++it) {
            DWORD serviceState;

            if ((*it)->InternalIOCTL(IOCTL_SERVICE_STATUS,NULL,0,(PBYTE)&serviceState,sizeof(serviceState)))
                pEnumTrav->dwServiceState = serviceState;
            else
                pEnumTrav->dwServiceState = SERVICE_STATE_UNKNOWN;

            (*it)->DeRef();
            pEnumTrav++;
        }
        g_pServicesLock->Unlock();

        DEBUGCHK((CHAR*)pEnumTrav == (CHAR*)((CHAR*)pEnumInfo + (numServices * sizeof(ServiceEnumInfo))));
        err = ERROR_SUCCESS;
        goto done;
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
        err = ERROR_INVALID_PARAMETER;
    }

doneHoldsLock:
    g_pUserDriverContainer->Unlock();

done:
    *pdwBufferLen = numBytesRequired;
    
    if (err)
        SetLastError(err);
    return (err == ERROR_SUCCESS) ? TRUE : FALSE;
}

#pragma warning( default : 4509 )

// Handles servicesd.exe specific IOCTLs.
BOOL HandleServicesIOCTL(FNIOCTL_PARAM& fnIoctlParam) {
    // Note: Unlike the standard DeviceIoControl case, none of
    // the user buffers passed to this are pre-mapped by nk.exe.
    
    switch (fnIoctlParam.dwIoControlCode) {
    case IOCTL_SERVICE_INTERNAL_SERVICE_ENUM:
    {
        if (! fnIoctlParam.lpInBuf)
            break;

        ServicesExeEnumServicesIntrnl *pEnumServices = (ServicesExeEnumServicesIntrnl*)fnIoctlParam.lpInBuf;
        if (! pEnumServices)
            break;

        return ServicesGetInfo(pEnumServices->pBuffer,pEnumServices->pdwServiceEntries,pEnumServices->pdwBufferLen);
    }
    break;

    default:
    break;
    }

    // No apps should be using this IOCTL so we should never get a bogus one
    DEBUGCHK(0); 
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}


