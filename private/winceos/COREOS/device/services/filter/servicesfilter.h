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

#ifndef SERVICE_FILTER_H
#define SERVICE_FILTER_H

#include <winsock2.h>
#include <strsafe.h>
#include <service.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <devload.h>
#include <svsutil.hxx>
#include <creg.hxx>
#include <list.hxx>
#include <marshal.hpp>
#include <mservice.h>
#include <drfilter.h>
#include <errorrep.h>

// Implements an actual service filter that does services.exe specific "stuff".
class ServiceFilter : public DriverFilterBase {
protected:
    // Name of the registry key associated with this service
    WCHAR  m_registryKeyName[MAX_PATH];
    // ServiceContext flags associated with this service
    DWORD  m_serviceContext;
    // Value returned from xxx_Init()
    DWORD m_initReturn;
    // Next entry in linked list of ServiceFilters
    ServiceFilter *m_pNext;
    // Number of calls into InternalIOCTL.  Unfortunately for BC we have to 
    // support concept of "fast" IOCTL where we don't have a handle to service
    // via xxx_Open() but instead send it an IOCTL directly;  super-services related
    // IPHlpapi addr changes, and IOCTL_SERVICE_STATUS all use this.  No
    // new fast IOCTLs should ever be added.
    BOOL  m_numFastIoctlCalls;
    // Have we entered the DeInit phase?  If so no more fast IOCTLs are allowed
    BOOL  m_isDeIniting;

    // Super-service port related    
    BOOL BindSocketToService(SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol);
    BOOL CloseSocketFromService(SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol);
    void StartListeningOnSuperServicePorts(BOOL signalStarted);
    BOOL StopListeningOnSuperServicePorts(BOOL callerHoldsLock);
    BOOL VerifyIsASuperServer();
    void NotifyServiceOfSocketDeregistration(SOCKADDR *pSockAddr, DWORD sockAddrLen);
    BOOL ServiceAddPortInternal(BYTE const*const pBufIn);
    BOOL ServiceClosePortInternal(BYTE const*const pBufIn);
    BOOL ServiceUnbindPortsInternal();
    BOOL GetAcceptRegKey(__out_ecount(acceptRegKeyLen) WCHAR *acceptRegKey, DWORD acceptRegKeyLen);
    void GetServiceContext();

public:
    // Override DeviceIoControl handling
    BOOL FilterControl(DWORD dwOpenCont,DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut,HANDLE hAsyncRef);

    DWORD DoFilterInit();

    void StartListeningOnSuperServicePortsIfNeeded() { 
        if (m_serviceContext & SERVICE_INIT_STOPPED)
            StartListeningOnSuperServicePorts(TRUE);
    }

    BOOL  FilterDeinit(DWORD dwContext);
    DWORD FilterInit(DWORD dwContext,LPVOID lpParam);

    void FilterPowerdn(DWORD dwConext);
    void FilterPowerup(DWORD dwConext);
    ServiceFilter(LPCTSTR lpcFilterRegistryPath, LPCTSTR lpcDriverRegistryPath, PDRIVER_FILTER pNextFilterParam);

    ServiceFilter *GetNext() { return m_pNext; }

    ~ServiceFilter();

    BOOL InternalIOCTL(DWORD dwCode, BYTE *pInBuf, DWORD dwLenIn, PBYTE pBufOut=NULL, DWORD dwLenOut=0);
};

extern ServiceFilter *g_pServiceFilterList;

ServiceFilter *VerifyServiceFilter(ServiceFilter *pServiceFilter);

#include "ServAcpt.hpp"
#include "ServAddr.hpp"
#include "ServCom.hpp"

extern SVSSynch *g_pServicesLock;
extern const DWORD g_errorRetrySleep;
extern BOOL g_servicesFilterRunning;
extern HANDLE g_servicesShutdownEvent;

// DEBUGZONES
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARNING    DEBUGZONE(1)
#define ZONE_INIT       DEBUGZONE(2)
#define ZONE_COM        DEBUGZONE(3)
#define ZONE_IPHLAPI    DEBUGZONE(4)
#define ZONE_ACCEPT     DEBUGZONE(5)
#define ZONE_FILTER     DEBUGZONE(6)

#endif // SERVICE_FILTER_H

