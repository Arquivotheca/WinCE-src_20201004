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

    apis.c

Abstract:

    Interface manager for DhcpV6 windows APIs.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
//#include "precomp.h"
//#include "apis.tmh"


DWORD
IniEnumDhcpV6InterfacesByTemplate(
    PDHCPV6_INTERFACE pTemplateDhcpV6Interface,
    DWORD dwPreferredNumEntries,
    PDHCPV6_INTERFACE * ppDhcpV6Interfaces,
    LPDWORD pdwNumInterfaces,
    LPDWORD pdwTotalNumInterfaces,
    LPDWORD pdwResumeHandle
    )
{
    DWORD dwError = ERROR_NOT_SUPPORTED;

    return (dwError);
}


DWORD
IniEnumDhcpV6Interfaces(
    DWORD dwPreferredNumEntries,
    PDHCPV6_INTERFACE * ppDhcpV6Interfaces,
    LPDWORD pdwNumInterfaces,
    LPDWORD pdwTotalNumInterfaces,
    LPDWORD pdwResumeHandle
    )
{
    DWORD dwError = 0;
    DWORD dwResumeHandle = 0;
    DWORD dwNumToEnum = 0;
    DWORD dwTotalNumInterfaces = 0;
    DWORD i = 0;
    DWORD dwNumInterfaces = 0;
    PDHCPV6_INTERFACE pDhcpV6Interfaces = NULL;
    PDHCPV6_INTERFACE pTempDhcpV6Interface = NULL;
    PLIST_ENTRY pHead = &AdapterList;
    PLIST_ENTRY pEntry = pHead->Flink;
    LPWSTR      pszAdapterName;


    dwResumeHandle = *pdwResumeHandle;

    if (!dwPreferredNumEntries ||
        (dwPreferredNumEntries > MAX_DHCPV6_INTERFACE_ENUM_COUNT)) {
        dwNumToEnum = MAX_DHCPV6_INTERFACE_ENUM_COUNT;
    }
    else {
        dwNumToEnum = dwPreferredNumEntries;
    }

    AcquireSharedLock(gpAdapterRWLock);
    dwTotalNumInterfaces = 0;
    while(pEntry != pHead) {
        dwTotalNumInterfaces++;
        pEntry = pEntry->Flink;
    }

    if (dwTotalNumInterfaces <= dwResumeHandle) {
        dwError = ERROR_NO_DATA;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    pHead = &AdapterList;
    pEntry = pHead->Flink;
    for (i = 0; i < dwResumeHandle; i++) {
        pEntry = pEntry->Flink;
    }

    dwNumInterfaces = dwTotalNumInterfaces - dwResumeHandle;

    if (dwNumInterfaces > dwNumToEnum) {
        dwNumInterfaces = dwNumToEnum;
    }

    __try {
        dwError = DHCPv6AllocateBuffer(
                      sizeof(DHCPV6_INTERFACE)*dwNumInterfaces,
                      &pDhcpV6Interfaces
                      );
        BAIL_ON_LOCK_ERROR(dwError);

        pTempDhcpV6Interface = pDhcpV6Interfaces;

        for (i = 0; i < dwNumInterfaces; i++) {
            PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

            pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
            pEntry = pEntry->Flink;

            pTempDhcpV6Interface->dwInterfaceID = pTempDhcpV6Adapt->dwIPv6IfIndex;

#ifdef UNDER_CE
            if (! (DHCPv6AllocateBuffer(
                (wcslen(pTempDhcpV6Adapt->wszAdapterName) + 1) * sizeof(WCHAR),
                &pszAdapterName))) {
                wcscpy(pszAdapterName, pTempDhcpV6Adapt->wszAdapterName);
                pTempDhcpV6Interface->pszDescription = pszAdapterName;
            }
#endif

            pTempDhcpV6Interface++;
        }
        
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    ReleaseSharedLock(gpAdapterRWLock);

    *ppDhcpV6Interfaces = pDhcpV6Interfaces;
    *pdwResumeHandle = dwResumeHandle + dwNumInterfaces;
    *pdwNumInterfaces = dwNumInterfaces;
    *pdwTotalNumInterfaces = dwTotalNumInterfaces;

cleanup:

    return (dwError);

error:

    if (pDhcpV6Interfaces) {
        DHCPv6FreeBuffer(pDhcpV6Interfaces);
    }

    *ppDhcpV6Interfaces = NULL;
    *pdwResumeHandle = dwResumeHandle;
    *pdwNumInterfaces = 0;
    *pdwTotalNumInterfaces = 0;

    goto cleanup;

lock:
    ReleaseSharedLock(gpAdapterRWLock);

    goto error;
}


DWORD
EnumDhcpV6Interfaces(
    LPWSTR pServerName,
    DWORD dwVersion,
    PDHCPV6_INTERFACE pTemplateDhcpV6Interface,
    DWORD dwPreferredNumEntries,
    PDHCPV6_INTERFACE * ppDhcpV6Interfaces,
    LPDWORD pdwNumInterfaces,
    LPDWORD pdwTotalNumInterfaces,
    LPDWORD pdwResumeHandle,
    LPVOID pvReserved
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Enumerating Interfaces"));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    if (pTemplateDhcpV6Interface) {
        dwError = IniEnumDhcpV6InterfacesByTemplate(
                      pTemplateDhcpV6Interface,
                      dwPreferredNumEntries,
                      ppDhcpV6Interfaces,
                      pdwNumInterfaces,
                      pdwTotalNumInterfaces,
                      pdwResumeHandle
                      );
    }
    else {
        dwError = IniEnumDhcpV6Interfaces(
                      dwPreferredNumEntries,
                      ppDhcpV6Interfaces,
                      pdwNumInterfaces,
                      pdwTotalNumInterfaces,
                      pdwResumeHandle
                      );
    }
    BAIL_ON_WIN32_ERROR(dwError);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Enumerating Interfaces"));

    return (dwError);

error:

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Enumerating Interfaces with Error: %!status!", dwError));

    return (dwError);
}


DWORD
CreateIniInterfaceHandle(
    PINI_INTERFACE_HANDLE * ppIniInterfaceHandle
    )
{
    DWORD dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;


    dwError = AllocateDHCPV6Memory(
                  sizeof(INI_INTERFACE_HANDLE),
                  &pIniInterfaceHandle
                  );
    BAIL_ON_WIN32_ERROR(dwError);

    memset(pIniInterfaceHandle, 0, sizeof(INI_INTERFACE_HANDLE));

    *ppIniInterfaceHandle = pIniInterfaceHandle;
    return (dwError);

error:

    *ppIniInterfaceHandle = NULL;
    return (dwError);
}


DWORD
IniOpenDhcpV6InterfaceHandle(
    PDHCPV6_INTERFACE pDhcpV6Interface,
    PINI_INTERFACE_HANDLE pIniInterfaceHandle
    )
{
    DWORD dwError = 0;


    pIniInterfaceHandle->dwInterfaceID = pDhcpV6Interface->dwInterfaceID;

    dwError = DHCPV6AdaptFindAndReference(
                pIniInterfaceHandle->dwInterfaceID,
                NULL                                    // Find if the adapter exist only
                );
    BAIL_ON_WIN32_ERROR(dwError);

error:

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Handle for Interface: %d with Error: %!status!", pDhcpV6Interface->dwInterfaceID, dwError));

    return (dwError);
}


VOID
FreeIniInterfaceHandle(
    PINI_INTERFACE_HANDLE pIniInterfaceHandle
    )
{
    if (pIniInterfaceHandle) {
        FreeDHCPV6Memory(pIniInterfaceHandle);
    }
    return;
}

DWORD
OpenDhcpV6InterfaceHandle(
    LPWSTR pServerName,
    DWORD dwVersion,
    PDHCPV6_INTERFACE pDhcpV6Interface,
    LPVOID pvReserved,
    PHANDLE phInterface
    )
{
    DWORD dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Opening Interfaces"));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    dwError = CreateIniInterfaceHandle(
                  &pIniInterfaceHandle
                  );
    BAIL_ON_WIN32_ERROR(dwError);

    AcquireExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    dwError = IniOpenDhcpV6InterfaceHandle(pDhcpV6Interface, pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    pIniInterfaceHandle->pNext = gpIniInterfaceHandle;
    gpIniInterfaceHandle = pIniInterfaceHandle;

    *phInterface = (HANDLE) pIniInterfaceHandle;

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Opening Interfaces - Handle: %p", *phInterface));

    return (dwError);

lock:

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

error:

    if (pIniInterfaceHandle) {
        FreeIniInterfaceHandle(pIniInterfaceHandle);
    }

    *phInterface = NULL;

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Opening Interfaces with Error: %!status!", dwError));

    return (dwError);
}


DWORD
GetIniInterfaceHandle(
    HANDLE hInterface,
    PINI_INTERFACE_HANDLE * ppIniInterfaceHandle
    )
{
    DWORD dwError = ERROR_INVALID_HANDLE;
    PINI_INTERFACE_HANDLE * ppTemp = NULL;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = (PINI_INTERFACE_HANDLE) hInterface;


    *ppIniInterfaceHandle = NULL;

    ppTemp = &gpIniInterfaceHandle;

    while (*ppTemp) {

        if (*ppTemp == pIniInterfaceHandle) {
            break;
        }
        ppTemp = &((*ppTemp)->pNext);
    }

    if (*ppTemp) {
        *ppIniInterfaceHandle = *ppTemp;
        dwError = ERROR_SUCCESS;
    }

    return (dwError);
}


DWORD
IniCloseDhcpV6InterfaceHandle(
    PINI_INTERFACE_HANDLE pIniInterfaceHandle
    )
{
    DWORD dwError = 0;

    return (dwError);
}


VOID
RemoveIniInterfaceHandle(
    PINI_INTERFACE_HANDLE pIniInterfaceHandle
    )
{
    PINI_INTERFACE_HANDLE * ppTemp = NULL;


    ppTemp = &gpIniInterfaceHandle;

    while (*ppTemp) {

        if (*ppTemp == pIniInterfaceHandle) {
            break;
        }
        ppTemp = &((*ppTemp)->pNext);
    }

    if (*ppTemp) {
        *ppTemp = pIniInterfaceHandle->pNext;
    }

    return;
}


VOID
DestroyIniInterfaceHandleList(
    PINI_INTERFACE_HANDLE pIniInterfaceHandleList
    )
{
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;
    PINI_INTERFACE_HANDLE pTemp = NULL;

    pIniInterfaceHandle = pIniInterfaceHandleList;

    while (pIniInterfaceHandle) {

        pTemp = pIniInterfaceHandle;
        pIniInterfaceHandle = pIniInterfaceHandle->pNext;

        (VOID) IniCloseDhcpV6InterfaceHandle(pTemp);
        FreeIniInterfaceHandle(pTemp);

    }
}


DWORD
CloseDhcpV6InterfaceHandle(
    HANDLE hInterface
    )
{
    DWORD dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Closing Interfaces: Handle: %p", hInterface));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    AcquireExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    dwError = GetIniInterfaceHandle(hInterface, &pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = IniCloseDhcpV6InterfaceHandle(pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    RemoveIniInterfaceHandle(
        pIniInterfaceHandle
        );

    FreeIniInterfaceHandle(pIniInterfaceHandle);

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Closing Interfaces"));

    return (dwError);

lock:

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

error:

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Closing Interfaces"));

    return (dwError);
}


DWORD
PerformDhcpV6Refresh(
    HANDLE hInterface,
    DWORD dwVersion,
    LPVOID pvReserved
    )
{
    DWORD dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Refreshing Interface: Handle: %p", hInterface));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    AcquireExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    dwError = GetIniInterfaceHandle(hInterface, &pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = DHCPV6AdaptFindAndReference(
                    pIniInterfaceHandle->dwInterfaceID,
                    &pDhcpV6Adapt
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = DHCPV6MessageMgrPerformRefresh(
                    pDhcpV6Adapt
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Refreshing Interface"));

    return (dwError);

lock:

    if (pDhcpV6Adapt) {
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

error:

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Refreshing Interface: Handle: %p with Error: %!status!", hInterface, dwError));

    return (dwError);
}


DWORD
GetDhcpV6DNSList(
    HANDLE hInterface,
    DWORD dwVersion,
    PDHCPV6_DNS_LIST * ppDhcpV6DNSList,
    LPVOID pvReserved
    )
{
    DWORD               dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;
    PDHCPV6_ADAPT       pDhcpV6Adapt = NULL;
    PDHCPV6_DNS_LIST    pDhcpV6DNSList = NULL;
    PDHCPV6_DNS         pDhcpV6DNS;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Fetching DNS Information: Handle: %p", hInterface));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    AcquireExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    dwError = GetIniInterfaceHandle(hInterface, &pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = DHCPV6AdaptFindAndReference(
                    pIniInterfaceHandle->dwInterfaceID,
                    &pDhcpV6Adapt
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    AcquireSharedLock(&pDhcpV6Adapt->RWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_INFO, ("Fetching DNS Information for Interface: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    if (pDhcpV6Adapt->uNumOfDNSServers == 0) {
        dwError = ERROR_NOT_FOUND;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    __try {
        dwError = DHCPv6AllocateBuffer(
                      sizeof(DHCPV6_DNS_LIST),
                      &pDhcpV6DNSList
                      );
        BAIL_ON_LOCK_ERROR(dwError);

        dwError = DHCPv6AllocateBuffer(
                  sizeof(DHCPV6_DNS) * (pDhcpV6Adapt->uNumOfDNSServers),
                  &pDhcpV6DNS
                  );
        BAIL_ON_LOCK_ERROR(dwError);

        pDhcpV6DNSList->uNumOfEntries = pDhcpV6Adapt->uNumOfDNSServers;
        memcpy(
            pDhcpV6DNS,
            pDhcpV6Adapt->pIpv6DNSServers,
            sizeof(DHCPV6_DNS) * (pDhcpV6Adapt->uNumOfDNSServers)
            );
        pDhcpV6DNSList->pDhcpV6DNS = pDhcpV6DNS;
    }

    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    *ppDhcpV6DNSList = pDhcpV6DNSList;

    ReleaseSharedLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Fetching DNS Information"));

    return (dwError);

lock:

    if (pDhcpV6Adapt) {
        ReleaseSharedLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

error:

    if (pDhcpV6DNSList) {
        if (pDhcpV6DNSList->pDhcpV6DNS) {
            DHCPv6FreeBuffer(pDhcpV6DNSList->pDhcpV6DNS);
        }
        DHCPv6FreeBuffer(pDhcpV6DNSList);
    }

    *ppDhcpV6DNSList = NULL;

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Fetching DNS Information: Handle: %p with Error: %!status!", hInterface, dwError));

    return (dwError);
}


DWORD
GetDhcpV6PDList(
    HANDLE hInterface,
    DWORD dwVersion,
    DHCPV6_PD_OPTION ** ppDhcpV6PDList,
    LPVOID pvReserved
    )
{
    DWORD dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;
    DHCPV6_PD_OPTION *pDhcpV6PDList = NULL;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Fetching DNS Information: Handle: %p", hInterface));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    AcquireExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    dwError = GetIniInterfaceHandle(hInterface, &pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = DHCPV6AdaptFindAndReference(
                    pIniInterfaceHandle->dwInterfaceID,
                    &pDhcpV6Adapt
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    AcquireSharedLock(&pDhcpV6Adapt->RWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_INFO, ("Fetching DNS Information for Interface: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    if (! pDhcpV6Adapt->pPdOption) {
        dwError = ERROR_NOT_FOUND;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    __try {
        dwError = DHCPv6AllocateBuffer(
                  sizeof(DHCPV6_PD_OPTION),
                  &pDhcpV6PDList);
        
        BAIL_ON_LOCK_ERROR(dwError);

        memcpy(pDhcpV6PDList, pDhcpV6Adapt->pPdOption, 
            sizeof(DHCPV6_PD_OPTION));

    }

    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);
    }
    
    *ppDhcpV6PDList = pDhcpV6PDList;

    ReleaseSharedLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Fetching DNS Information"));

    return (dwError);

lock:

    if (pDhcpV6Adapt) {
        ReleaseSharedLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

error:

    if (pDhcpV6PDList) {
        DHCPv6FreeBuffer(pDhcpV6PDList);
    }

    *ppDhcpV6PDList = NULL;

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Fetching DNS Information: Handle: %p with Error: %!status!", hInterface, dwError));

    return (dwError);
}


DWORD
GetDhcpV6DomainList(
    HANDLE hInterface,
    DWORD dwVersion,
    char ** ppDhcpV6DomainList,
    LPVOID pvReserved
    )
{
    DWORD dwError = 0;
    PINI_INTERFACE_HANDLE pIniInterfaceHandle = NULL;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;
    char * pDhcpV6DomainList = NULL;
    DWORD cLen, cAdd;


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Fetching DNS Information: Handle: %p", hInterface));

    ENTER_DHCPV6_SECTION();
    dwError = ValidateSecurity(
                  DHCPV6_OBJECT_SERVER,
                  SERVER_ACCESS_ADMINISTER,
                  NULL,
                  NULL
                  );
    LEAVE_DHCPV6_SECTION();
    BAIL_ON_WIN32_ERROR(dwError);

    AcquireExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    dwError = GetIniInterfaceHandle(hInterface, &pIniInterfaceHandle);
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = DHCPV6AdaptFindAndReference(
                    pIniInterfaceHandle->dwInterfaceID,
                    &pDhcpV6Adapt
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    AcquireSharedLock(&pDhcpV6Adapt->RWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_INFO, ("Fetching DNS Information for Interface: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    cLen = pDhcpV6Adapt->cDomainList;
    if ((0 == cLen) || (! pDhcpV6Adapt->pDomainList)) {
        dwError = ERROR_NOT_FOUND;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    if (pDhcpV6Adapt->pDomainList[cLen-1] != '\0')
        cAdd = 2;
    else if ((1 == cLen) || (pDhcpV6Adapt->pDomainList[cLen-2] != '\0'))
        cAdd = 1;
    else
        cAdd = 0;

    __try {
        dwError = DHCPv6AllocateBuffer(cLen + cAdd, &pDhcpV6DomainList);
        
        BAIL_ON_LOCK_ERROR(dwError);

        memcpy(pDhcpV6DomainList, pDhcpV6Adapt->pDomainList, cLen);

        while (cAdd) {
            pDhcpV6DomainList[cLen + --cAdd] = '\0';
        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    *ppDhcpV6DomainList = pDhcpV6DomainList;

    ReleaseSharedLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("End Fetching DNS Information"));

    return (dwError);

lock:

    if (pDhcpV6Adapt) {
        ReleaseSharedLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    ReleaseExclusiveLock(gpDhcpV6IniInterfaceTblRWLock);

error:

    if (pDhcpV6DomainList) {
        DHCPv6FreeBuffer(pDhcpV6DomainList);
    }

    *ppDhcpV6DomainList = NULL;

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Fetching DNS Information: Handle: %p with Error: %!status!", hInterface, dwError));

    return (dwError);
}


