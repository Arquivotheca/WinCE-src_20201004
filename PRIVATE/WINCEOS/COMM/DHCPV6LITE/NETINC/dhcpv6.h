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

    dhcpv6.h

Abstract:

    DHCPv6 Client

Author:

    Francis Duong

Revision History:



--*/

#ifndef _DHCPV6_
#define _DHCPV6_


#ifdef __cplusplus
extern "C" {
#endif


#define MAX_DHCPV6_INTERFACE_ENUM_COUNT     100
/*
typedef struct _DHCPV6_INTERFACE {
    DWORD dwInterfaceID;
    LPWSTR pszDescription;
} DHCPV6_INTERFACE, * PDHCPV6_INTERFACE;

typedef struct _DHCPV6_DNS {
    UCHAR ucDNSAddr[16];
} DHCPV6_DNS, * PDHCPV6_DNS;

typedef struct _DHCPV6_DNS_LIST {
    ULONG uNumOfEntries;
#ifdef __midl
    [size_is(uNumOfEntries)] PDHCPV6_DNS pDhcpV6DNS;
#else
    PDHCPV6_DNS pDhcpV6DNS;
#endif
} DHCPV6_DNS_LIST, * PDHCPV6_DNS_LIST;


#ifdef UNDER_CE
typedef struct _DHCPV6_IA_PREFIX {
    FILETIME    IALeaseObtained;
    DWORD       PreferedLifetime;
    DWORD       ValidLifetime;
    UCHAR       cPrefix;
    IN6_ADDR    PrefixAddr;
} DHCPV6_IA_PREFIX, *PDHCPV6_IA_PREFIX;


typedef struct _DHCPV6_PD_OPTION {
    DWORD       IAID;
    DWORD       T1;
    DWORD       T2;
    DHCPV6_IA_PREFIX    IAPrefix;
} DHCPV6_PD_OPTION, * PDHCPV6_PD_OPTION;

#endif
*/

VOID
DHCPv6FreeBuffer(
    LPVOID pvBuffer
    );

DWORD
DHCPv6AllocateBuffer(
    DWORD dwByteCount,
    LPVOID * ppvBuffer
    );

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
    );

DWORD
OpenDhcpV6InterfaceHandle(
    LPWSTR pServerName,
    DWORD dwVersion,
    PDHCPV6_INTERFACE pDhcpV6Interface,
    LPVOID pvReserved,
    PHANDLE phInterface
    );

DWORD
CloseDhcpV6InterfaceHandle(
    HANDLE hInterface
    );

DWORD
PerformDhcpV6Refresh(
    HANDLE hInterface,
    DWORD dwVersion,
    LPVOID pvReserved
    );

DWORD
GetDhcpV6DNSList(
    HANDLE hInterface,
    DWORD dwVersion,
    PDHCPV6_DNS_LIST * ppDhcpV6DNSList,
    LPVOID pvReserved
    );

DWORD
GetDhcpV6PDList(
    HANDLE hInterface,
    DWORD dwVersion,
    DHCPV6_PD_OPTION ** ppDhcpV6PDList,
    LPVOID pvReserved
    );

DWORD
GetDhcpV6DomainList(
    HANDLE hInterface,
    DWORD dwVersion,
    char ** ppDhcpV6DomainList,
    LPVOID pvReserved
    );

#ifdef __cplusplus
}
#endif


#endif // _DHCPV6_

