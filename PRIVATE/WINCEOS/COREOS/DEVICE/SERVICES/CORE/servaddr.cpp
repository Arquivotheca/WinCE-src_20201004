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
// Implements indications for when an IP address of the local host 
// has changed or becomes available or unavailable
// 

#include <servMain.hpp>

static BOOL    g_iphlpapiInited;
static HMODULE g_iphlpapiLib;
static HANDLE  g_iphlpapiThread;

// iphlpapi function type definition to relevant function
typedef DWORD (WINAPI * PFN_GETADAPTERSADDRESSES)(ULONG Family, DWORD Flags, PVOID Reserved, PIP_ADAPTER_ADDRESSES pAdapterAddresses, PULONG pOutBufLen);
// Function to retrieve local IP addresses
PFN_GETADAPTERSADDRESSES  g_pfnGetAdaptersAddresses;

// Address families we support notifications (ipv4 & ipv6)
static const int g_addrFamilies[] = {AF_INET, AF_INET6};
// Number of families we support
static const DWORD g_numAddrFamilies = SVSUTIL_ARRLEN(g_addrFamilies);


// Class that interfaces with iphlapi and performs appropriate logic
class NotifyAddressChange {
private:
    // Time (in ms) to wait between last network addr change and
    // notifying services.
    static const DWORD m_bounceWait = 5000;
    // Buffer to store the GetAdaptersAddresses returned buffer
    BYTE  *m_pAddrBuf;
    // Sizeof m_pAddrBuf
    DWORD  m_sizeAddrBuf;
    // Amount of time to wait for new event
    DWORD  m_timeout;
    // Handles for notification changes (+1 to hold g_servicesShutdownEvent)
    HANDLE m_events[g_numAddrFamilies+1];
    // Sockets for notification changens
    SOCKET m_sockets[g_numAddrFamilies];
    // Number of events registered
    DWORD  m_numEvents;
    // Overlapped information
    WSAOVERLAPPED   m_ov[g_numAddrFamilies];

#ifdef DEBUG
    // Whether to DebugBreak on strange error state
    BOOL m_fBreakOnWaitFailure;
#endif

    void ProcessWaitFailure(DWORD err);
    BOOL ReadLocalAddress();
    void NotifyServicesOfAddrChange();

public:
    NotifyAddressChange();
    ~NotifyAddressChange();
    void MonitorAndNotifyAddressChanges();

};

NotifyAddressChange::NotifyAddressChange() {
    m_pAddrBuf = NULL;
    m_sizeAddrBuf = 0;
#ifdef DEBUG
    m_fBreakOnWaitFailure = TRUE;
#endif
    m_timeout = INFINITE;
    m_numEvents = 0;
    memset(m_events,0,sizeof(m_events));
    memset(m_ov,0,sizeof(m_ov));

    DWORD i;

    // set up notification events for each address family
    for (i = 0; i < g_numAddrFamilies; i++) {
        m_sockets[m_numEvents] = socket(g_addrFamilies[i],SOCK_STREAM,0);

        if (m_sockets[m_numEvents] != INVALID_SOCKET) {
            // create event to be used for notifications for this address family
            m_ov[m_numEvents].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

            // register notification
            if (ERROR_SUCCESS != WSAIoctl(m_sockets[m_numEvents], SIO_ADDRESS_LIST_CHANGE, NULL, 0, NULL, 0, NULL, &m_ov[m_numEvents], NULL) &&
                ERROR_IO_PENDING != GetLastError())
            {
                closesocket(m_sockets[m_numEvents]);
                CloseHandle(m_ov[m_numEvents].hEvent);
            }
            else
            {
                m_events[m_numEvents] = m_ov[m_numEvents].hEvent;
                m_numEvents++;
            }
        }
    }

    // Finally, add event to be signaled on shutdown
    m_events[m_numEvents] = g_servicesShutdownEvent;
    m_numEvents++;
}

NotifyAddressChange::~NotifyAddressChange() {
    DWORD i;
    
    for (i = 0; i < m_numEvents; i++)
        CloseHandle(m_events[i]);

    for (i = 0; i < m_numEvents-1; i++)
        closesocket(m_sockets[i]);

    if (m_pAddrBuf)
        delete m_pAddrBuf;
}


// Called when WaitForMultipleObjects fails.  This usually indicates
// some strange system problem.  Have extra DEBUG processing because
// this has caught system bugs in stress runs in the past.
void NotifyAddressChange::ProcessWaitFailure(DWORD err) {
    RETAILMSG(1,(L"SERVICES.EXE: WaitForMultipleObjects in ServicesNotifyAddrChangeThread failed, error=<0x%08x>\r\n",err));
    // something strange is going on, because this event should always be valid.
    // In case it's a transient condition just wait for a bit.
#if defined (DEBUG)
    // only break in DBG builds, and only 1st time we hit this to avoid constant interruptions to debuggee.
    if (m_fBreakOnWaitFailure) {
        DEBUGCHK(0);
        m_fBreakOnWaitFailure = FALSE;
    }
#endif
    Sleep(g_errorRetrySleep);
}

// Calls GetAdaptersAddresses in order to determine local IP addresses
BOOL NotifyAddressChange::ReadLocalAddress() {
    // Call GetAdaptersAddresses(), realloc buffer if needed.
    DWORD bufSizeNeeded = m_sizeAddrBuf;

    DWORD error = g_pfnGetAdaptersAddresses(AF_UNSPEC,0,NULL,(PIP_ADAPTER_ADDRESSES)m_pAddrBuf,&bufSizeNeeded);
    if (error == ERROR_BUFFER_OVERFLOW) {
        // The current buffer was not big enough for the current addresses.
        // Allocate appropriate sized buffer and call again.
        DEBUGCHK(bufSizeNeeded > m_sizeAddrBuf);

        if (m_pAddrBuf) {
            LocalFree(m_pAddrBuf);
            m_pAddrBuf = NULL;
            m_sizeAddrBuf = 0;
        }

        m_pAddrBuf = (BYTE*)LocalAlloc(0,bufSizeNeeded);

        if (!m_pAddrBuf) {
            RETAILMSG(1,(L"SERVICS.EXE: Could not alloc %d bytes for GetAdaptersAddresses.  No notifications to services.  Sleeping in case this is transient condition.\r\n",g_errorRetrySleep));
            DEBUGCHK(m_sizeAddrBuf == 0);
            Sleep(g_errorRetrySleep);
            return FALSE;
        }

        m_sizeAddrBuf = bufSizeNeeded;
        error = g_pfnGetAdaptersAddresses(AF_UNSPEC,0,NULL,(PIP_ADAPTER_ADDRESSES)m_pAddrBuf,&bufSizeNeeded);
    }

    if (error != ERROR_SUCCESS) {
        RETAILMSG(1,(L"SERVICS.EXE: GetAdaptersAddresses failed, GLE=0x%08x.  Sleeping in case this is transient condition.\r\n",GetLastError()));
        Sleep(g_errorRetrySleep);
        return FALSE;
    }

    return TRUE;
}

void NotifyAddressChange::MonitorAndNotifyAddressChanges() {
    // main loop to receive network table notifications and dispatch them to services.
    while (1) {
        DWORD err = WaitForMultipleObjects(m_numEvents, m_events, FALSE, m_timeout);

        if (! g_servicesExeRunning) {
            // Services.exe is shutting down
            return;
        }

        if (err == WAIT_FAILED) {
            ProcessWaitFailure(err);
            continue;
        }
        else if (err != WAIT_TIMEOUT) {
            // give time for the network to settle down.  Wait for m_bounceWait milliseconds 
            // before notifying services of this event.  This is done
            // in order to help prevent bounce - i.e. many net adapters 
            // coming up and down in a quick interval.

            // reenable notification
            if ((err - WAIT_OBJECT_0) < m_numEvents) {
                DEBUGCHK(err - WAIT_OBJECT_0 < SVSUTIL_ARRLEN(m_sockets));
                DEBUGCHK(err - WAIT_OBJECT_0 < SVSUTIL_ARRLEN(m_ov));

                WSAIoctl(m_sockets[err - WAIT_OBJECT_0], SIO_ADDRESS_LIST_CHANGE, NULL, 0, NULL, 0, NULL, &m_ov[err - WAIT_OBJECT_0], NULL);

                m_timeout = m_bounceWait;
                continue;
            }

            // Otherwise we got a totally unexpected return from 
            // WaitForMultipleObjects().  Complain.
            ProcessWaitFailure(err);
            continue;
        }

        // We have timed out after having got the network notification change
        // m_bounceWait milliseconds ago.
        m_timeout = INFINITE;

        if (ReadLocalAddress())
            NotifyServicesOfAddrChange();
    }

    // no fall-through (yet - eventually when shutdown is added we will...)
    DEBUGCHK(0);
}

// Calls any service that indicates that it wants IOCTLs when net addr changes
void NotifyAddressChange::NotifyServicesOfAddrChange() {
    serviceDriver_t servicesToNotify;

    g_pUserDriverContainer->Lock();
    ServiceDriver *pService = (ServiceDriver*)g_pUserDriverContainer->GetFirstDriver();

    // Find services that have requested addr notification changes.
    while (pService) {
        // SERVICE_NET_ADDR_CHANGE_THREAD
        // Increase ref count and put service into list to notify.
        // Build up this list rather than calling into IOCTL directly
        // so that we don't have to release critical section in this pass
        pService->AddRef();
        servicesToNotify.push_back(pService);

        pService = (ServiceDriver*)pService->GetNextObject();
    }
    g_pUserDriverContainer->Unlock();

    // Enumerate those services that need the notification
    serviceDriverIter_t it    = servicesToNotify.begin();
    serviceDriverIter_t itEnd = servicesToNotify.end();

    g_pServicesLock->Lock();
    for (; it != itEnd; ++it) {
        (*it)->InternalIOCTL(IOCTL_SERVICE_NOTIFY_ADDR_CHANGE, m_pAddrBuf, m_sizeAddrBuf);
        (*it)->DeRef();
    }
    g_pServicesLock->Unlock();
}

void InitIphlapi() {
    DEBUGCHK(g_iphlpapiInited == FALSE);
    DEBUGCHK(g_iphlpapiLib == NULL);
    DEBUGCHK(g_iphlpapiThread == NULL);

    g_iphlpapiLib = LoadLibrary(L"\\windows\\iphlpapi.dll");
    if (g_iphlpapiLib == FALSE) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: LoadLibrary(iphlapi.dll), GLE=<0x%08x>\r\n",GetLastError()));
        return;
    }

    g_pfnGetAdaptersAddresses = (PFN_GETADAPTERSADDRESSES) GetProcAddress(g_iphlpapiLib,L"GetAdaptersAddresses");

    if (!g_pfnGetAdaptersAddresses) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: Could not find export GetAdaptersAddresses in iphlapi.dll.  GLE=<0x%08x>\r\n",GetLastError()));
        DEBUGCHK(0); // iphlpapi is corrupt.
        FreeLibrary(g_iphlpapiLib);
        g_iphlpapiLib = NULL;
    }
    else {
        g_iphlpapiInited = TRUE;
    }
}

void DeInitIphlapi() {
    if (! g_iphlpapiInited)
        return;

    DEBUGCHK(g_iphlpapiLib);
    DEBUGCHK(g_pfnGetAdaptersAddresses);

    if (g_iphlpapiThread) {
        DEBUGMSG(ZONE_SERVICES,(L"SERVICES: Waiting for ServicesNotifyAddrChangeThread to complete\r\n"));
        WaitForSingleObject(g_iphlpapiThread,INFINITE);
        CloseHandle(g_iphlpapiThread);
        g_iphlpapiThread = NULL;
    }
    
    FreeLibrary(g_iphlpapiLib);
    g_iphlpapiLib = NULL;
}

// Worker thread to listen for local-host network changes
DWORD WINAPI ServicesNotifyAddrChangeThread(LPVOID lpv) {
    NotifyAddressChange notify;
    notify.MonitorAndNotifyAddressChanges();

    return 0;
}

// Starts up the thread to listen for IP address notifications,
// if possible
void StartIPAddrChangeNotificationThread()
{
    DEBUGCHK(g_pServicesLock->IsLocked());

    if (! g_iphlpapiInited) {
        // No iphlapi.dll on the system
        return;
    }

    if (g_iphlpapiThread) {
        // The worker thread has already been started
        return;
    }

    g_iphlpapiThread = CreateThread(NULL, 0, ServicesNotifyAddrChangeThread, NULL, 0, NULL);
    if (g_iphlpapiThread == NULL) {
        DEBUGMSG(ZONE_ERROR,(L"SERVICES: ERROR: CreateThread for ServicesNotifyAddrChangeThread failed, GLE=<0x%08x>\r\n",GetLastError()));
    }
}


