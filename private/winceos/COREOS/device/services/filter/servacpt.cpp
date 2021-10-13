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
// Implements super-service accept thread to notify a service when a new
// network connection has arrived.
// 

#include <servicesFilter.h>

// Was winsock functionality successfully initialized?
static BOOL g_winsockInited;
// Handle to thread spun up to process super-services accept
static HANDLE g_acceptThread;
// Name of registry value that contains SOCKADDR information for a super-service
static const WCHAR g_sockAddrRegValue[] = L"SockAddr";
static const WCHAR g_protocolRegValue[] = L"Protocol";
// Maximum number of super-services sockets services.exe supports simultaneously
static const DWORD g_servicesMaxSockets = (FD_SETSIZE-1);

// Used to signal main worker thread when a socket needs to be added or removed
// from the current accept() call
static SOCKET g_sockNotify;
static BOOL   g_fSockNotifyOpen;

static DWORD WINAPI ServicesAcceptThread(LPVOID lpv);
static void NotifyAcceptThreadOfChange(void);

// Contains SOCKET information and corresponding information about 
// its owner super-service
class ServiceSocket {
private:
    // The instance of the service filter that corresponds with this socket
    ServiceFilter        *m_pService;
    // The incoming connection socket itself
    SOCKET                m_socket;
    // IP port that this socket has bound too
    SOCKADDR_STORAGE      m_sockAddr;
    // size of sockAddr data
    DWORD                 m_sockAddrLen;
    // Protocol this socket is associated with
    DWORD                 m_protocol;
public:
    ServiceSocket(ServiceFilter *pService, SOCKET sockNew, SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol);
    ~ServiceSocket();

    SOCKET GetSocket(void)              { return m_socket; }
    ServiceFilter*GetOwnerService(void) { return m_pService; }
    BOOL   IsServiceSocket(ServiceFilter *pService, SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol);
};

typedef ce::smart_ptr<ServiceSocket>    ServiceSocketSmartPtr;
typedef ce::list<ServiceSocketSmartPtr> ServiceSocketList;
// List of all ServiceSocket on the system
static ServiceSocketList *g_pServiceSocketList;
// Number of items in g_pServiceSocketList
static DWORD g_numServiceSockets;


ServiceSocket::ServiceSocket(ServiceFilter *pService, SOCKET sockNew, SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol) {
    DEBUGCHK(sockAddrLen <= sizeof(SOCKADDR_STORAGE));

    m_pService    = pService;
    m_socket      = sockNew;
    m_sockAddrLen = sockAddrLen;
    m_protocol    = protocol;
    memcpy(&m_sockAddr,pSockAddr,sockAddrLen);

    g_numServiceSockets++;
    DEBUGCHK((int)g_numServiceSockets <= g_servicesMaxSockets);
}

ServiceSocket::~ServiceSocket() {
    DEBUGCHK(m_socket != INVALID_SOCKET);
    closesocket(m_socket);
    g_numServiceSockets--;
    DEBUGCHK((int)g_numServiceSockets >= 0);
}

// Returns whether passed in SOCKET Addr information corresponds
// with the current socket.
BOOL ServiceSocket::IsServiceSocket(ServiceFilter *pService, SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol) {
    return ((pService == m_pService) &&
            (sockAddrLen == m_sockAddrLen) &&
            (protocol == m_protocol) &&
            (0 == memcmp(&m_sockAddr,pSockAddr,sockAddrLen)));
}

void InitWinsock() {
    DEBUGCHK(g_servicesFilterRunning == FALSE);
    DEBUGCHK(g_pServicesLock->IsLocked());
    DEBUGCHK(g_winsockInited == FALSE);
    DEBUGCHK(g_numServiceSockets == 0);
    DEBUGCHK(g_acceptThread==NULL);

    WSADATA wsadata;
    DWORD err = WSAStartup(MAKEWORD(2,2), &wsadata);

    if (err != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: WSAStartup failed, error=<0x%08x>\r\n",err));
        return;
    }

    g_pServiceSocketList = new ServiceSocketList();
    if (! g_pServiceSocketList) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Error on memory allocation\r\n"));
        return;
    }

    g_winsockInited = TRUE;
}

void DeInitWinsock() {
    if (g_winsockInited == FALSE)
        return;

    g_pServicesLock->Lock();
    NotifyAcceptThreadOfChange();
    g_pServicesLock->Unlock();

    if (g_acceptThread) {
        DEBUGMSG(ZONE_INIT | ZONE_ACCEPT,(L"SERVICES: Waiting for super-services accept thread to complete\r\n"));
        WaitForSingleObject(g_acceptThread,INFINITE);
        CloseHandle(g_acceptThread);
        g_acceptThread = NULL;
    }

    if (g_pServiceSocketList)
        delete g_pServiceSocketList;

    WSACleanup();
}

// Services.exe super-services only support V4 & V6
static BOOL IsSockAddrValid(SOCKADDR *pSockAddr, DWORD sockAddrLen) {
    if ((sockAddrLen < sizeof(SOCKADDR_IN)) ||
        ((pSockAddr->sa_family == AF_INET) && (sockAddrLen != sizeof(SOCKADDR_IN))) ||
        ((pSockAddr->sa_family == AF_INET6) && (sockAddrLen != sizeof(SOCKADDR_IN6))) ||
        ((pSockAddr->sa_family != AF_INET6) && (pSockAddr->sa_family != AF_INET)))
    {
        DEBUGMSG(ZONE_ERROR,(L"SERVICS: ERROR: Super-services accept either received non IPv4/IPv6 addresses or wrong length.\r\n"));
        return FALSE;
    }
    return TRUE;
}

// Sets up socket to receive notification of super-service state change event
static void CreateNotifySocket(void) {
    DEBUGCHK(g_fSockNotifyOpen == FALSE);
    DEBUGCHK(g_pServicesLock->IsLocked());
    g_sockNotify = socket(AF_INET,SOCK_STREAM,0);

    if (g_sockNotify == INVALID_SOCKET) {
        // for the day when IPv6 can be on a device without IPv4
        g_sockNotify = socket(AF_INET6,SOCK_STREAM,0);
    }

    // If we can't create notification socket we're in trouble.
    DEBUGCHK(g_sockNotify != INVALID_SOCKET);
    g_fSockNotifyOpen = TRUE;
}

// Creates ServicesAcceptThread if its not running.
static BOOL CreateAcceptThreadIfNeeded(void)  {
    DEBUGCHK(g_pServicesLock->IsLocked());
    if (g_acceptThread) // thread is already running
        return TRUE;

    DEBUGCHK((g_numServiceSockets == 1) && (g_fSockNotifyOpen == FALSE));

    g_acceptThread = CreateThread(NULL, 0, ServicesAcceptThread, NULL, 0, NULL);
    if (!g_acceptThread) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: CreateThread(ServicesAcceptThread) failed, GLE=<0x%08x>\r\n",GetLastError()));
        return FALSE;
    }
   
    DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Spun up thread <0x%08x> to listen for incoming network connections\r\n",g_acceptThread));
    return TRUE;
}

// When a super-service socket has been added or removed, notify the
// main accept thread so that it keeps in sync.
static void NotifyAcceptThreadOfChange(void) {
    DEBUGCHK(g_pServicesLock->IsLocked());
    if (g_fSockNotifyOpen) {
        DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Signaling to accept thread to change sockets listened on\r\n"));
        closesocket(g_sockNotify);
        g_fSockNotifyOpen = FALSE;
    }
}

// Read all of the SOCKETs associated with the registered 
// super-services and put them in the local fd_set array.
static void FillSelectArray(fd_set *pSockSet) {
    FD_ZERO(pSockSet);
    g_pServicesLock->Lock();

    if (!g_fSockNotifyOpen)
        CreateNotifySocket();

    if (g_sockNotify != INVALID_SOCKET) {
#pragma warning(suppress:4127) // conditional expression is constant
        FD_SET(g_sockNotify,pSockSet);
    }

    ServiceSocketList::iterator it    = g_pServiceSocketList->begin();
    ServiceSocketList::iterator itEnd = g_pServiceSocketList->end();

    for (; it != itEnd; ++it) {
#pragma warning(suppress:4127) // conditional expression is constant
        FD_SET((*it)->GetSocket(),pSockSet);
    }
    g_pServicesLock->Unlock();
}

// There has been some unexpected error in select().  Wait here
// (and DEBUGCHK 1st time we get in this state)
static void PauseOnSelectFailure() {
#if defined (DEBUG)
    static BOOL breakOnSelectFailure = TRUE;
#endif

    g_pServicesLock->Lock();
    if (!g_fSockNotifyOpen) {
        // In ServicesAcceptThread() there is a slight window where after 
        // FillSelectArray() has been called and the lock released, another 
        // thread can close the notify socket *before* ServicesAcceptThread()
        // can call select().  This means we're calling select() with a bogus
        // socket and hence we'll get into this failure case, but since
        // this isn't truly an error at winsock layer we don't ASSERT().
        g_pServicesLock->Unlock();
        return;
    }
    g_pServicesLock->Unlock();

    // This can only happen if winsock fails an internal CreateEvent().
    DEBUGMSG(ZONE_ERROR,(L"services.exe: ERROR select() returned 0 or SOCKET_ERROR, GLE=0x%08x.  Sleeping for %d ms and trying to call select again\r\n",
                          GetLastError(),g_errorRetrySleep));

#if defined (DEBUG)
    if (breakOnSelectFailure) {
        // Possible condition won't go away, in which case DEBUGCHK every 10 seconds is obnoxious.
        breakOnSelectFailure = FALSE;
        DEBUGCHK(0);
    }
#endif

    // Sleep and retry in case this is a temporary condition.
    Sleep(g_errorRetrySleep);
}

// When select() has returned and indicated that there are clients
// waiting for a new connection, this function calls appropriate service(s)
static void NotifyOnNewConnection(fd_set *pSockSet, DWORD numReadySockets) {
    DWORD i;

    // For each socket that has been set in the fd_set, determine the service
    // that owns it and then call it.
    for (i = 0; i < numReadySockets; i++) {
        g_pServicesLock->Lock();
        if (!g_fSockNotifyOpen)  {
            // If we get a notification message play it safe and reread the list.
            DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Received notification socket for super-services.\r\n"));
            g_pServicesLock->Unlock();
            break;
        }

        ServiceSocketList::iterator it    = g_pServiceSocketList->begin();
        ServiceSocketList::iterator itEnd = g_pServiceSocketList->end();

        // For each ServiceSocket element, determine if this socket belongs to it.
        for (; it != itEnd; ++it) {
            if (pSockSet->fd_array[i] == (*it)->GetSocket()) {
                int addrLen = 0;
                SOCKET sockAccept = accept(pSockSet->fd_array[i],0,&addrLen);
                if (sockAccept == INVALID_SOCKET) {
                    DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: accept() call for super-services failed, GLE=<0x%08x>\r\n",WSAGetLastError()));
                }
                else {
                    ServiceFilter *pService = (*it)->GetOwnerService();
                    DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: ServicesAcceptThread calling service <0x%08x> with connection socket\r\n",pService));
                    pService->InternalIOCTL(IOCTL_SERVICE_CONNECTION,(PBYTE)(&sockAccept), sizeof(SOCKET));
                }
                break;
            }
        }
        // Should have found a match...
        DEBUGCHK(it != itEnd);

        // In case creating notification socket failed at some point, this 
        // will cause CreateNotifySocket() to try again next time it's called.
        if (g_sockNotify == INVALID_SOCKET)
            g_fSockNotifyOpen = FALSE;

        g_pServicesLock->Unlock();
    }
}

// Thread spun up in order to accept incoming network connections 
// and pass them to appropriate services as they arrive.
#pragma warning(push)
#pragma warning(disable:4702) // unreachable code
static DWORD WINAPI ServicesAcceptThread(LPVOID /*lpv*/)  {
    
    fd_set     sockSet;
    int        numReadySockets;

    for (;;) {
        DEBUGCHK(! g_pServicesLock->IsLocked());
        FillSelectArray(&sockSet);

        DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: About to call select for super-services\r\n"));
        numReadySockets = select(0,&sockSet,NULL,NULL,NULL);
        DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Woke up from select() call for super-services, numReadySockets=%d\r\n",numReadySockets));

        if (!g_servicesFilterRunning) {
            DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Stopping super-services accept thread on services.exe shutdown\r\n"));
            return 0;
        }

        if (numReadySockets == 0 || numReadySockets == SOCKET_ERROR) {
            PauseOnSelectFailure();
            continue;
        }

        NotifyOnNewConnection(&sockSet,numReadySockets);
    } // while (1)

    DEBUGCHK(0); 
    return 0;
}
#pragma warning(pop)


// Creates a new socket of address family pSockAddr and associates it 
// with this services.exe DLL.  Spins up a worker accept thread if required.
BOOL ServiceFilter::BindSocketToService(SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol) {
    DEBUGCHK(g_pServicesLock->IsLocked());
    DEBUGCHK(pSockAddr && IsSockAddrValid(pSockAddr,sockAddrLen));

    SOCKET sockNew = INVALID_SOCKET;
    DWORD  err;
    BOOL   ioctlRet;

    ServiceSocketSmartPtr serviceSocket = NULL;

    DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Begining to create a new accept socket for service <%s>\r\n",m_registryKeyName));

    if (g_numServiceSockets >= g_servicesMaxSockets) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Too many super-services sockets registered.\r\n"));
        err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
        goto done;
    }

    //
    //  Allocate the socket, bind and listen to it
    //
    if (INVALID_SOCKET == (sockNew = socket(pSockAddr->sa_family,SOCK_STREAM,protocol))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Unable to create socket, GLE=0x%08x\r\n",WSAGetLastError()));
        err = WSAGetLastError();
        goto done;
    }

    if (ERROR_SUCCESS != (bind(sockNew,pSockAddr,sockAddrLen))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Unable to bind to socket, GLE=0x%08x\r\n",WSAGetLastError()));
        err = WSAGetLastError();
        goto done;
    }

    if (ERROR_SUCCESS != (listen(sockNew,SOMAXCONN))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Unable to listen to socket, GLE=0x%08x\r\n",WSAGetLastError()));
        err = WSAGetLastError();
        goto done;
    }

    serviceSocket = new ServiceSocket(this,sockNew,pSockAddr,sockAddrLen, protocol);
    if (!serviceSocket.valid()) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Error on memory allocation\r\n"));
        err = ERROR_OUTOFMEMORY;
        goto done;
    }

    // Alert the service that a new socket is being bound to it.
    ioctlRet = InternalIOCTL(IOCTL_SERVICE_REGISTER_SOCKADDR,(PBYTE)pSockAddr,sockAddrLen,NULL,0);

    if (ioctlRet != TRUE) {
        DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: ERROR: IOControl(IOCTL_SERVICE_REGISTER_SOCKADDR) returns FALSE, not adding socket to accept list\r\n"));
        err = ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
        goto done;
    }

    // Spin up the thread to actually do the listening, if needed
    if (! CreateAcceptThreadIfNeeded()) {
        err = ERROR_OUTOFMEMORY;
        goto done;
    }

    // Finally, put into the global linked list.
    if (! g_pServiceSocketList->push_back(serviceSocket)) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Error on memory allocation\r\n"));
        err = ERROR_OUTOFMEMORY;
        goto done;
    }
    // Note: All calls past inserting into list must succeed or it must
    // be OK if they fail.  We don't cleanup after the serviceSocket
    // only when insertion into list gave it an extra ref count
    err = ERROR_SUCCESS;

    NotifyAcceptThreadOfChange();
    DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Successfully created a new accept socket for service <%s>\r\n",m_registryKeyName));
done:
    if (err != ERROR_SUCCESS) {
        if (!serviceSocket.valid() && (sockNew != INVALID_SOCKET)) {
            // pServiceSocket destructor auto-frees SOCKET, so only
            // need this if serviceSocket was never allocated
            closesocket(sockNew);
        }

        SetLastError(err);
        return FALSE;
    }

    return TRUE;
}

// Removes one particular socket that is bound to a given service.
BOOL ServiceFilter::CloseSocketFromService(SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol) { 
    DEBUGCHK(g_pServicesLock->IsLocked());

    ServiceSocketList::iterator it    = g_pServiceSocketList->begin();
    ServiceSocketList::iterator itEnd = g_pServiceSocketList->end();

    // Find each ServiceSocket that this ServiceFilter owns and 
    // remove it from the list.

    for (; it != itEnd; ) {
        if ((*it)->IsServiceSocket(this,pSockAddr, sockAddrLen, protocol)) {
            g_pServiceSocketList->erase(it++);
            NotifyServiceOfSocketDeregistration(pSockAddr,sockAddrLen);
            return TRUE;
        }
        else {
            ++it;
        }
    }

    DEBUGMSG(ZONE_ERROR,(L"SERVICES: ServiceClosePort fails because specified socket is not listen for by this service\r\n"));
    SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
    return FALSE;
}

// Adds this service to super-services list on service startup.  To do this,
// open up HKLM\Services\<ServiceName>\Accept registry key and enumerate
// through each subkey, which contains a port to associate with this service.
void ServiceFilter::StartListeningOnSuperServicePorts(BOOL signalStarted) {
    CReg reg;

    DEBUGCHK(! g_pServicesLock->IsLocked());
    g_pServicesLock->Lock();

    if (! g_winsockInited)
        goto done;

    WCHAR acceptKey[MAX_PATH];
    if (! GetAcceptRegKey(acceptKey,SVSUTIL_ARRLEN(acceptKey)))
        goto done;

    if (! reg.Open(HKEY_LOCAL_MACHINE,acceptKey)) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Cannot open super-services registry key <%s>\r\n",acceptKey));
        goto done;
    }

    WCHAR portKey[MAX_PATH];

    // Alert the service that we are about to start registering super-services.
    // A service can indicate it doesn't want any sockets registered at this
    // stage by returning FALSE.
    BOOL ioctlRet = InternalIOCTL(IOCTL_SERVICE_REGISTER_SOCKADDR,NULL,0,NULL,0);
    if (!ioctlRet) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Service <%s> returned FALSE for IOCTL_SERVICE_REGISTER_SOCKADDR on checking if it support super-services.  GLE=<0x%08x>\r\n",
                             acceptKey,GetLastError()));
        goto done;
    }

    while (reg.EnumKey(portKey, SVSUTIL_ARRLEN(portKey))) {
        CReg subreg(reg,portKey);
        SOCKADDR_STORAGE sockAddr;
        DWORD protocol;
        DWORD sockAddrLen;

        sockAddrLen = subreg.ValueBinary(g_sockAddrRegValue,(LPBYTE)&sockAddr,sizeof(sockAddr));
        if (0 == sockAddrLen) {
            DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Super service <%s\\%s> does not have <%s> registry set\r\n",
                                  acceptKey,portKey,g_sockAddrRegValue));
            continue;
        }

        if (! IsSockAddrValid((SOCKADDR*)&sockAddr,sockAddrLen))
            continue;

        protocol = subreg.ValueDW(g_protocolRegValue,0);

        BindSocketToService((SOCKADDR*)&sockAddr,sockAddrLen,protocol);
    }

    if (signalStarted) {
        // Send this IOCTL to the service to indicate we have completed
        // registering SOCKADDRs for it on startup.  We only do this 
        // when loading a service, not on processing an IOCTL_SERVICE_START.
        InternalIOCTL(IOCTL_SERVICE_STARTED,NULL,0);
    }

done:
    g_pServicesLock->Unlock();
}

// Removes service from super-services list on service shutdown/stop
BOOL ServiceFilter::StopListeningOnSuperServicePorts(BOOL callerHoldsLock) {
    if (callerHoldsLock) {
        DEBUGCHK(g_pServicesLock->IsLocked());
    }
    else {
        // Calling function doesn't own the services.exe lock, so grab it now.
        DEBUGCHK(! g_pServicesLock->IsLocked());
        g_pServicesLock->Lock();
    }

    ServiceSocketList::iterator it;
    ServiceSocketList::iterator itEnd;

    if (! g_winsockInited)
        goto done;

    BOOL foundService = FALSE;

    DEBUGMSG(ZONE_ACCEPT,(L"SERVICES: Removing accept super-service socket for service <%s>\r\n",m_registryKeyName));

    it    = g_pServiceSocketList->begin();
    itEnd = g_pServiceSocketList->end();

    // Find each ServiceSocket that this ServiceFilter owns and 
    // remove it from the list.

    for (; it != itEnd; ) {
        if ((*it)->GetOwnerService() == this) {
            // This service does own the socket in question
            g_pServiceSocketList->erase(it++);
            foundService = TRUE;
        }
        else {
            ++it;
        }
    }

    // Alert service of the socket being deregistered only if 
    // we have removed at least one.
    if (foundService) {
        NotifyServiceOfSocketDeregistration(NULL,0);
    }

done:
    if (! callerHoldsLock) {
        // Release lock since caller won't release it.
        g_pServicesLock->Unlock();
    }
    
    return TRUE;
}

void ServiceFilter::NotifyServiceOfSocketDeregistration(SOCKADDR *pSockAddr, DWORD sockAddrLen)  {
    DEBUGCHK(g_pServicesLock->IsLocked());
    NotifyAcceptThreadOfChange();
        
    InternalIOCTL(IOCTL_SERVICE_DEREGISTER_SOCKADDR,(BYTE*)pSockAddr,sockAddrLen);
}

// Processes ServiceAddPort API.  pBufIn corresponds to the pBufIn data
// passed in DeviceIoControl() called by xxx_ServiceAddPort in CoreDLL.

BOOL ServiceFilter::ServiceAddPortInternal(BYTE const*const pBufIn) {

    if (! VerifyIsASuperServer())
        return FALSE;

    ServicesExeAddPort addPort;

    if (! CeSafeCopyMemory(&addPort,pBufIn,sizeof(addPort))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ServiceAddPort fails because pointer is invalid\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MarshalledBuffer_t sockAddr(addPort.pSockAddr,addPort.cbSockAddr,ARG_I_PTR);
    SOCKADDR *pSockAddr = (SOCKADDR *)sockAddr.ptr();

    if (!pSockAddr  || (!IsSockAddrValid(pSockAddr,addPort.cbSockAddr))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ServiceAddPort fails because socket is invalid\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    g_pServicesLock->Lock();
    BOOL ret = BindSocketToService(pSockAddr,addPort.cbSockAddr,addPort.iProtocol);
    g_pServicesLock->Unlock();

    return ret;
}

// Processes ServiceClosePort API.  pBufIn corresponds to the pBufIn data
// passed in DeviceIoControl() called by xxx_ServiceClosePort in CoreDLL.


BOOL ServiceFilter::ServiceClosePortInternal(BYTE const*const pBufIn) {

    if (! VerifyIsASuperServer())
        return FALSE;

    ServicesExeClosePort closePort;

    if (! CeSafeCopyMemory(&closePort,pBufIn,sizeof(closePort))) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ServiceClosePort fails because pointer is invalid\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    MarshalledBuffer_t sockAddr(closePort.pSockAddr,closePort.cbSockAddr,ARG_I_PTR);
    SOCKADDR *pSockAddr = (SOCKADDR *)sockAddr.ptr();

    if (! pSockAddr) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ServiceClosePort fails because pointer is invalid\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    g_pServicesLock->Lock();
    BOOL ret = CloseSocketFromService(pSockAddr,closePort.cbSockAddr,closePort.iProtocol);
    g_pServicesLock->Unlock();

    return ret;

}

// Processes ServiceUnbindPorts API
BOOL ServiceFilter::ServiceUnbindPortsInternal() {
    if (! VerifyIsASuperServer())
        return FALSE;

    return StopListeningOnSuperServicePorts(FALSE);
}

const WCHAR g_acceptRegKeySuffix[]  = L"\\Accept";
const DWORD g_acceptRegKeySuffixLen = SVSUTIL_CONSTSTRLEN(g_acceptRegKeySuffix);

BOOL ServiceFilter::GetAcceptRegKey(__out_ecount(acceptRegKeyLen) WCHAR *acceptRegKey, DWORD acceptRegKeyLen) {
    WCHAR *stringEnd;
    size_t stringLenRemaining;

    if (FAILED(StringCchCopyEx(acceptRegKey,acceptRegKeyLen,m_registryKeyName,&stringEnd,&stringLenRemaining,0)) ||
        FAILED(StringCchCopy(stringEnd,stringLenRemaining,g_acceptRegKeySuffix)))
    {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Registry key for <%s>\\Accept is too large to copy into buffer passed in\r\n",acceptRegKey));
        return FALSE;
    }

    return TRUE;
}

// Makes sure that the given service is indeed a super-service
BOOL ServiceFilter::VerifyIsASuperServer() {

    if (m_serviceContext & SERVICE_INIT_STOPPED)
        return TRUE;

    DEBUGMSG(ZONE_ERROR,(L"SERVICES: Service <%s> is not a super-service and cannot have super-service APIs called on it\r\n",
                           m_registryKeyName));
    SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
    return FALSE;
}

