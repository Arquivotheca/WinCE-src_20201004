//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**								Microsoft Windows							**/
/*****************************************************************************/

/*
	DhcpV6L.h

  DESCRIPTION:


*/

#ifndef _DHCPVL_H_
#define _DHCPVL_H_


#define FSCTL_DHCP6_BASE     FILE_DEVICE_NETWORK

#define _DHCP6_CTL_CODE(function, method, access) \
            CTL_CODE(FSCTL_DHCP6_BASE, function, method, access)


#define IOCTL_DHCPV6L_ENUMERATE_INTERFACES  \
            _DHCP6_CTL_CODE(10, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DHCPV6L_OPEN_HANDLE  \
            _DHCP6_CTL_CODE(11, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DHCPV6L_CLOSE_HANDLE  \
            _DHCP6_CTL_CODE(12, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DHCPV6L_REFRESH  \
            _DHCP6_CTL_CODE(13, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_DHCPV6L_GET_DNS_LIST  \
            _DHCP6_CTL_CODE(15, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DHCPV6L_GET_PD_LIST  \
            _DHCP6_CTL_CODE(16, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DHCPV6L_GET_DOMAIN_LIST  \
            _DHCP6_CTL_CODE(17, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DHCPV6L_FREE_MEMORY  \
            _DHCP6_CTL_CODE(18, METHOD_BUFFERED, FILE_ANY_ACCESS)


typedef struct _DHCPV6_INTERFACE {
    DWORD dwInterfaceID;
    LPWSTR pszDescription;
} DHCPV6_INTERFACE, * PDHCPV6_INTERFACE;

typedef struct _DHCPV6_IOCTL_ENUM_PARAMS {
    DWORD   Status;
//  LPWSTR  pServerName; //unused
    DWORD   dwVersion;
    DWORD   CallType;
//  PDHCPV6_INTERFACE pTemplateDhcpV6Interface; // unused
    DWORD   dwPreferredNumEntries;
    DHCPV6_INTERFACE * pDhcpV6Interfaces;
    DWORD dwNumInterfaces;
    DWORD dwTotalNumInterfaces;
    DWORD dwResumeHandle;
    LPVOID  pvReserved;
} DHCPV6_IOCTL_ENUM_PARAMS;

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

typedef struct _DHCPV6_IOCTL_INTERFACE_PARAMS {
    DWORD   Status;
//  LPWSTR pServerName; //unused
    DWORD   dwVersion;
    DWORD   CallType;
    DHCPV6_INTERFACE   DhcpV6Interface;
    HANDLE  hInterface;
    union {
        DHCPV6_DNS_LIST * pDhcpV6DNSList;
        DHCPV6_PD_OPTION *pPrefixD;
        char             *pDomainList;
    };
    LPVOID  pvReserved;
} DHCPV6_IOCTL_INTERFACE_PARAMS;

typedef struct _DHCPV6_IOCTL_HDR {
    DWORD   Status;
//  LPWSTR pServerName; //unused
    DWORD   dwVersion;
    DWORD   CallType;
} DHCPV6_IOCTL_HDR;


#endif  // _DHCPVL_H_


