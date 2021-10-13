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

//
// Implements the filter that simulates functionality that previously
// was broken off into servicesd.exe.  Services are for the most part just device
// drivers, with a few extensions to functionality.
//

#include <servicesFilter.h>

// Global services.exe critical section.  This controls services.exe specific
// funcionality - namely CoFreeUnusedLibraries thread, IP change notification
// thread, and adding and removing super services.
SVSSynch *g_pServicesLock;
// Number of seconds to wait should a transient system error occur
const DWORD g_errorRetrySleep = (10*1000);
// Number of milliseconds to wait before trying to unload a service again
// if it's blocked processing a fastIoctl
static const DWORD g_waitOnFastIoctl = 100;
// g_servicesFilterRunning is initialized to TRUE during first Filter Init and
// is set to FALSE when last service has been unloaded and it's time to shutdown
BOOL g_servicesFilterRunning;
// List of all services filters being hosted by this process
ServiceFilter *g_pServiceFilterList;

// Event signaled when last service is going to unload
HANDLE g_servicesShutdownEvent = NULL;

#if defined(UNDER_CE)
#ifdef DEBUG
  DBGPARAM dpCurSettings = {
    TEXT("ServicesFilter"), {
    TEXT("Error"),TEXT("Warning"),TEXT("Init"),TEXT("Iphlpapi"),
    TEXT("Accept"),TEXT("Driver Filter"),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT("") },
    0x0007
  }; 
#endif
#endif

// Exported fcns
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DEBUGREGISTER((HMODULE)hModule);
            svsutil_Initialize();
            g_pServicesLock = new SVSSynch();
            if (! g_pServicesLock)
                return FALSE;
                
            DisableThreadLibraryCalls((HMODULE)hModule);
            break;

        case DLL_PROCESS_DETACH:
            svsutil_DeInitialize();
            break;

    }
    return TRUE;
}


BOOL InitServicesFilterIfNeeded() {
    BOOL ret = FALSE;

    if (g_servicesFilterRunning) {
        ret = TRUE;
        goto done;
    }
    DEBUGMSG(ZONE_INIT,(L"SERVICES: Initializing services filter for current process for 1st time\r\n"));
    DEBUGCHK(g_pServiceFilterList == NULL);

    // Initialize WinSock. The accept and COM cleanup threads will be started only if something needs them.
    InitWinsock();

    g_servicesShutdownEvent = CreateEvent(NULL,TRUE,FALSE,NULL); // manual reset

    if (!g_servicesShutdownEvent) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICESD: ERROR: CreateEvent failed, GLE=<0x%08x>\r\n",GetLastError()));
        goto done;
    }

    g_servicesFilterRunning = TRUE;
    ret = TRUE;
done:
    return ret;
}

extern "C" PDRIVER_FILTER ServicesFilterInit(LPCTSTR lpcFilterRegistryPath, LPCTSTR lpcDriverRegistryPath, PDRIVER_FILTER pNextFilter) {

    g_pServicesLock->Lock();

    ServiceFilter *pServiceFilter = NULL;

    if (! InitServicesFilterIfNeeded()) 
        goto done;
    
    DEBUGMSG(ZONE_INIT,(L"SERVICES: Creating new service filter for service <%s>\r\n",lpcDriverRegistryPath));

    pServiceFilter = new ServiceFilter(lpcFilterRegistryPath,lpcDriverRegistryPath,pNextFilter);
    if (! pServiceFilter) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICESD: Error, Unable to allocate memory.  Service <%s> cannot be inited\r\n",lpcDriverRegistryPath));
        goto done;
    }


done:
    g_pServicesLock->Unlock();
    return (PDRIVER_FILTER)pServiceFilter;
}

DWORD WINAPI FilterInitThread(LPVOID lpv) {
    return ((ServiceFilter*)lpv)->DoFilterInit();
}

// Make sure that the given ServiceFilter is in the list still.
ServiceFilter *VerifyServiceFilter(ServiceFilter *pServiceFilter) {
    DEBUGCHK(g_pServicesLock->IsLocked());

    ServiceFilter *pTrav = g_pServiceFilterList;
    while (pTrav) {
        if (pTrav == pServiceFilter)
            return pTrav;

        pTrav = pTrav->GetNext();
    }

    return NULL;
}

ServiceFilter::ServiceFilter(LPCTSTR lpcFilterRegistryPath, LPCTSTR lpcDriverRegistryPath, PDRIVER_FILTER pNextFilterParam) : 
      DriverFilterBase(lpcFilterRegistryPath, pNextFilterParam)
{
    m_pNext             = NULL;
    m_numFastIoctlCalls = 0;
    m_isDeIniting       = FALSE;

    HRESULT hr = StringCchCopy(m_registryKeyName,_countof(m_registryKeyName),lpcDriverRegistryPath);
    if (FAILED (hr))
    {
        DEBUGMSG(ZONE_ERROR,(L"SERVICED: ERROR: ServiceFilter Failed in StringCchCopy, GLE=<0x%08x>\r\n",hr));
        return;
    }

    GetServiceContext();

    // If the service wants notification of IP address changes, initialize the address change helper
    if (m_serviceContext & SERVICE_NET_ADDR_CHANGE_THREAD)
        InitIphlapi();

    // If the service wants background CoFreeUnusedLibraries cleanup, start the thread
    if (m_serviceContext & SERVICE_COFREE_UNUSED_THREAD)
        InitCOM();

    // Add the service to the global list.
    g_pServicesLock->Lock();
    m_pNext                 = g_pServiceFilterList;
    g_pServiceFilterList    = this;
    g_pServicesLock->Unlock();
}

// Implementation of ServiceFilter itself
BOOL ServiceFilter::FilterControl(DWORD dwOpenCont, DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
                                  PDWORD pdwActualOut,HANDLE hAsyncRef)
{
    BOOL ret;
    switch (dwCode) {
    case IOCTL_SERVICE_INTERNAL_SERVICE_ADD_PORT:
        return ServiceAddPortInternal(pBufIn);
        break;

    case IOCTL_SERVICE_INTERNAL_SERVICE_CLOSE_PORT:
        return ServiceClosePortInternal(pBufIn);
        break;

    case IOCTL_SERVICE_INTERNAL_SERVICE_UNBIND_PORTS:
        return ServiceUnbindPortsInternal();
        break;

    // default case is that the service processes this IOCTL
    }

    __try {
        ret = pNextFilter->fnControl(dwOpenCont, dwCode, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut, hAsyncRef,pNextFilter);
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
        ret = FALSE;
    }

    if (ret && (m_serviceContext & SERVICE_INIT_STOPPED) && (dwCode == IOCTL_SERVICE_STOP)) {
        // If IOCTL_SERVICE_STOP was sent to a super-service,
        // then if there are any network ports bound to the service we must stop
        // listening on them now
        StopListeningOnSuperServicePorts(FALSE);
    }

    if (ret && (m_serviceContext & SERVICE_INIT_STOPPED) && (dwCode == IOCTL_SERVICE_START)) {
        // If IOCTL_SERVICE_START was sent to a super-service, then 
        // we must rebind any network ports (the assumption being that
        // the service was previously turned off and unbound ports)
        StartListeningOnSuperServicePorts(FALSE);
    }

    return ret;
}

// Actually call the xxx_Init() on the worker thread
DWORD ServiceFilter::DoFilterInit() {
    // Comment explaining service context & never passing in this param!=NULL for services.
    __try {
        m_initReturn = pNextFilter->fnInit(m_serviceContext,NULL,pNextFilter);
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
        m_initReturn = 0;
    }

    if (m_initReturn)
        StartListeningOnSuperServicePortsIfNeeded();
    
    return m_initReturn;
}

DWORD ServiceFilter::FilterInit(DWORD /*dwContext*/, LPVOID /*lpParam*/) {
    // Run DoFilterInit on a separate worker thread. Historically it was not safe to use
    // thread local storage on the current thread, because it is a PSL thread. Now CeTlsAlloc()
    // exists as a safe alternative. If every service used it, then there would be no need
    // for a worker thread. Eliminating this thread will be a breaking change that poses an
    // unknown stability risk (if any service still depends upon this).
    DWORD err = ERROR_INTERNAL_ERROR;
    DWORD ret = 0;
    HTHREAD hThread = CreateThread(NULL, NULL, FilterInitThread, this, 0, NULL);
    if (NULL != hThread) {
        if (WAIT_OBJECT_0 == WaitForSingleObject(hThread, INFINITE)) {
            if (GetExitCodeThread(hThread, &ret)) {
                err = NO_ERROR;
            }
            else {
                err = ERROR_INTERNAL_ERROR;
                ret = 0;
            }
        }
        else {
            err = GetLastError();
        }
        CloseHandle(hThread);
    }
    else {
        err = GetLastError();
    }
    ASSERT(err == NO_ERROR);
    return ret;
}

BOOL ServiceFilter::FilterDeinit(DWORD dwContext) {
    StopListeningOnSuperServicePorts(FALSE);
    m_isDeIniting = TRUE;
    BOOL ret;

    __try {
        ret = pNextFilter->fnDeinit(dwContext,pNextFilter);
    } __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
        ret = FALSE;
    }

    return ret;
}

void ServiceFilter::FilterPowerdn(DWORD /*dwContext*/) {
    // Services.exe should never be called for power change notifications so
    // do not pass this notification onto the service.  Services that need these 
    // notifications are not services, and should be moved to udevice or the kernel.
    SetLastError(ERROR_NOT_SUPPORTED);
    ASSERT(0); // services bus enumerator should have prevented this from making it through anyway
}

void ServiceFilter::FilterPowerup(DWORD /*dwContext*/) {
    // See FilterPowerdn comments.
    SetLastError(ERROR_NOT_SUPPORTED);
    ASSERT(0); // services bus enumerator should have prevented this from making it through anyway    
}

// Services filters in some circumstances can directly generate IOCTL calls
// to services, for instance IOCTL_SERVICE_CONNECTION and IOCTL_SERVICE_NOTIFY_ADDR_CHANGE.
// Since it's coming from services and not an app, we can have a fast path call here.
BOOL ServiceFilter::InternalIOCTL(DWORD dwCode, BYTE *pInBuf, DWORD dwLenIn, BYTE *pOutBuf, DWORD dwLenOut) {
    DEBUGCHK(g_pServicesLock->IsLocked());

    BOOL  ret;
    DWORD actualOut;

    if (m_isDeIniting) {
        // If we're in process of shutting service down, then don't shoot more IOCTLs
        SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
        return FALSE;
    }

    // For IOCTLs generated from a PSL, the caller (device manager)
    // has extra ref count on us.  Since InternalIOCTL() is generated by a 
    // thread in the services filter, we need the extra add-ref here or else
    // we risk possibility of service being unloaded from under us on another thread.
    // Ideally we'd tie into the ref counting of the object done by devmgr, but
    // since (to keep BC) we're not going through devmgr for IOCTL in 1st place
    // we need our own ref count.
    m_numFastIoctlCalls++;
    g_pServicesLock->Unlock();

    __try {
        // We're passing return from xxx_Init() rather than xxx_Open()
        // to DeviceIoControl() because there may not be an xxx_Open() on the service
        // but we want to call it anyway.  We don't call xxx_Open() ourselves
        // for BC, because we never did this in past (we always passed xxx_Init() ret).
        ret = FilterControl(m_initReturn,dwCode,pInBuf,dwLenIn,
                          pOutBuf, dwLenOut, &actualOut,NULL);
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) {
        ret = FALSE;
    }

    g_pServicesLock->Lock();
    m_numFastIoctlCalls--;

    return ret;
}

// Retrieves "ServiceContext" registry value.  This is effectively used 
// as "Context" when passing into xxx_Init(), but we can't pass in "Context"
// from registry key for various Backcompat issues.
void ServiceFilter::GetServiceContext() {
    CReg reg(HKEY_LOCAL_MACHINE,m_registryKeyName);
    m_serviceContext = reg.ValueDW(L"ServiceContext");
}

ServiceFilter::~ServiceFilter() {
    ServiceFilter *pTrav, *pFollow;

    // First, make sure there's no FastIOControl occurring.  We can't 
    // actually delete ServiceFilter object itself if IOCTL is being
    // processed.
    g_pServicesLock->Lock();

    while (m_numFastIoctlCalls > 0) {
        g_pServicesLock->Unlock();
        Sleep(g_waitOnFastIoctl);
        g_pServicesLock->Lock();
    }

    // Remove ourselves from the global list
    pTrav   = g_pServiceFilterList;
    pFollow = NULL;

    while (pTrav) {
        if (pTrav == this) {
            if (pTrav == g_pServiceFilterList) {
                DEBUGCHK(pFollow == NULL);
                g_pServiceFilterList = pTrav->m_pNext;
            }
            else {
                pFollow->m_pNext = pTrav->m_pNext;
            }
            break;
        }
        pFollow = pTrav;
        pTrav   = pTrav->m_pNext;
    }

    // if g_pServiceFilterList = NULL, it implies the last service is unloaded. Do the cleanup
    
    if(NULL == g_pServiceFilterList) {

        g_servicesFilterRunning = FALSE;    // Initing g_servicesFilterRunning = FALSE here, as ServicesNotifyAddrChangeThread and ServicesAcceptThread relies on it to terminate

        if (g_servicesShutdownEvent)
            SetEvent(g_servicesShutdownEvent);

        DeInitCOM();
        DeInitIphlapi();        
        DeInitWinsock();

        if (g_servicesShutdownEvent) {
            CloseHandle(g_servicesShutdownEvent);
            g_servicesShutdownEvent = NULL;
        }
        
    }

    DEBUGCHK(pTrav); // Expect to find ourselves in the list
    g_pServicesLock->Unlock();
}



