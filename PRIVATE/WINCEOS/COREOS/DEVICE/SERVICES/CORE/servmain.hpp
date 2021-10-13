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
/*++


Module Name:

    udevice.cpp

Abstract:

    Service Reflector implementation.

Revision History:

--*/

#pragma once

#include <windows.h>
#include <service.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <devload.h>
#include <svsutil.hxx>
#include <creg.hxx>
#include <list.hxx>
#include <marshal.hpp>
#include <mservice.h>

#include "reflector.h"
#include "udevice.hpp"
#include <devzones.h>
#include <errorrep.h>

// Space required to store service prefix ("XYZ#:") and trailing null
#define PREFIX_LEN           6

// Process a request from an application to a particular service in 
// the services.exe process space.

// Most of the interprocess marshalling code is the same as in udevice.exe,
// though services.exe has a few networking specific features that need
// special handling.
class ServiceDriver : public UserDriver {
protected:
    // Name of the registry key associated with this service
    WCHAR  m_registryKeyName[MAX_PATH];
    // ServiceContext flags associated with this service
    DWORD  m_serviceContext;
    // Init params passed in during Init call
    FNINIT_PARAM m_fnInitParam;
    // Prefix of the service (e.g. "HTP0:").
    WCHAR  m_prefix[PREFIX_LEN];

    BOOL StoreRegistrySettings(FNINIT_PARAM& fnInitParam);

    void StartListeningOnSuperServicePorts(BOOL callerHoldsLock, BOOL signalStarted);
    BOOL StopListeningOnSuperServicePorts(BOOL callerHoldsLock);

    BOOL BindSocketToService(SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol);
    BOOL CloseSocketFromService(SOCKADDR *pSockAddr, DWORD sockAddrLen, DWORD protocol);
    BOOL GetAcceptRegKey(WCHAR *acceptRegKey, DWORD acceptRegKeyLen);

    // Super-service port related
    BOOL ServiceAddPortInternal(FNIOCTL_PARAM& fnIoctlParam);
    BOOL ServiceClosePortInternal(FNIOCTL_PARAM& fnIoctlParam);
    BOOL ServiceUnbindPortsInternal(FNIOCTL_PARAM& fnIoctlParam);
    BOOL VerifyIsASuperServer();
    void NotifyServiceOfSocketDeregistration(SOCKADDR *pSockAddr, DWORD sockAddrLen);

public:
    ServiceDriver(FNDRIVERLOAD_PARAM& fnDriverLoad_Param,UserDriver * pNext) : 
        UserDriver(fnDriverLoad_Param,pNext)
    {
        m_registryKeyName[0] = 0;
    }

    ~ServiceDriver() {
        ;
    }

    BOOL InternalIOCTL(DWORD ioControlCode, BYTE *pInBuf, DWORD inBufLen, PBYTE pBufOut=NULL, DWORD dwLenOut=0);

    DWORD GetContext() { return m_serviceContext; }
    const WCHAR *GetDllName() { return m_fnDriverLoadParam.DriverName; }
    void WritePrefix(WCHAR *szPrefix);

    DWORD DriverInitThread();
    DWORD DriverDeinitThread();
    DWORD ServiceDriver::InitThread();

    // Overrides of udevice.exe processing of these functions.
    virtual BOOL Init();
    virtual BOOL DriverInit(FNINIT_PARAM& fnInitParam);
    virtual BOOL DriverDeinit();
    virtual BOOL Ioctl(FNIOCTL_PARAM& fnIoctlParam);
    virtual BOOL DriverPowerUp();
    virtual BOOL DriverPowerDown();
    virtual void FreeDriverLibrary();

};

typedef ce::list<ServiceDriver *> serviceDriver_t;
typedef serviceDriver_t::iterator serviceDriverIter_t;

#include "ServAcpt.hpp"
#include "ServAddr.hpp"
#include "ServCom.hpp"

extern SVSSynch *g_pServicesLock;
extern const DWORD g_errorRetrySleep;
extern BOOL   g_servicesExeRunning;
extern HANDLE g_servicesShutdownEvent;

// These are the only devload flags that services in services.exe
// are capable of processing.
#define SERV_DEVLOAD_FLAGS_SUPPORTED  (DEVFLAGS_UNLOAD | DEVFLAGS_LOADLIBRARY | DEVFLAGS_NOLOAD | DEVFLAGS_TRUSTEDCALLERONLY | \
                                       DEVFLAGS_LOAD_AS_USERPROC | DEVFLAGS_NOUNLOAD)



