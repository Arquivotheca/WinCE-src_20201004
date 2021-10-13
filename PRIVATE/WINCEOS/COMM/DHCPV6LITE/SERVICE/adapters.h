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

    adapters.h

Abstract:

    Adapter manager for DhcpV6 windows APIs.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

//
// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} + \n
//
#define GUID_LENGTH     39
#define DHCPV6_MAX_NAME_SIZE    256

typedef enum _DHCPV6_STATE {
    dhcpv6_state_init = 1,
    dhcpv6_state_solicit = 2,
    dhcpv6_state_srequest = 3,
    dhcpv6_state_request = 4,
    dhcpv6_state_rebindconfirm = 5,
    dhcpv6_state_configured = 6,
    dhcpv6_state_T1 = 7,
    dhcpv6_state_T2 = 8,
    dhcpv6_state_deinit = 9,
} DHCPV6_STATE, * PDHCPV6_STATE;

#define DHCPV6_MEDIA_DISC_FL    0x0001
#define DHCPV6_PD_ENABLED_FL    0x0010

typedef struct _DHCPV6_ADAPT {

    // Chain adapters. Access to this is protected by the global lock.
    LIST_ENTRY Link;

    // Global Adapter lock
    DHCPV6_RW_LOCK RWLock;

    ULONG uRefCount;

    DWORD dwIPv6IfIndex;

    WCHAR wcAdapterGuid[GUID_LENGTH];

    DHCPV6_STATE DhcpV6State;

    //
    // IPv6 Socket Address for this adapter
    //
    SOCKADDR_IN6 SockAddrIn6;

    //
    // Send Socket
    //
    SOCKET Socket;

    //
    // Options
    //
    DHCPV6_OPTION_MODULE DhcpV6OptionModule;

    //
    // Last Transaction ID
    //
    UCHAR ucTransactionID[3];

    //
    // Timer Events
    //
    BOOL bTimerCreated;
    BOOL bEventTimerQueued;
    BOOL bEventTimerCancelled;
    HANDLE hEventTimer;

    //
    // Timer Variables
    //
    ULONG uRetransmissionTimeout;
    ULONG uMaxRetransmissionTimeout;
    ULONG uMaxReXmits;
    ULONG uReXmits;

    DWORD StartRebindConfirm;

    //
    // Retrieved Configuration
    //
    ULONG uNumOfDNSServers;
    PIN6_ADDR pIpv6DNSServers;
#ifdef UNDER_CE
    DWORD               Flags;
    CHAR                *pDomainList;
    DWORD               cDomainList;
    DHCPV6_PD_OPTION    *pPdOption;

    BYTE                *pServerID;
    USHORT              cServerID;

    BYTE PhysicalAddr[MAX_ADAPTER_ADDRESS_LENGTH];
    DWORD cPhysicalAddr;
    WCHAR wszAdapterName[DHCPV6_MAX_NAME_SIZE];
#endif

} DHCPV6_ADAPT, * PDHCPV6_ADAPT;

typedef struct _DHCPV6_INIT_ADAPT_CTX {
    WCHAR wcAdapterGuid[GUID_LENGTH];
    DWORD dwIfIndex;
    SOCKADDR_IN6 SockAddrIn6;
#ifdef UNDER_CE
    BOOL    IsPDEnabled;
    BYTE    PhysicalAddr[MAX_ADAPTER_ADDRESS_LENGTH];
    DWORD   cPhysicalAddr;
    WCHAR   wszAdapterName[DHCPV6_MAX_NAME_SIZE];
#endif
} DHCPV6_INIT_ADAPT_CTX, * PDHCPV6_INIT_ADAPT_CTX;

#define DEL_REG_PREFIX_FL       0x0001
#define DEL_REG_DNS_LIST_FL     0x0002
#define DEL_REG_DOMAIN_LIST_FL  0x0004
#define DEL_REG_ALL             0xffff

void DeleteRegistrySettings(PDHCPV6_ADAPT pDhcpV6Adapt, DWORD dwFlags);
void SaveRegistrySettings(PDHCPV6_ADAPT pDhcpV6Adapt);

DWORD
InitDHCPV6AdaptAtStartup(
    );

#ifdef UNDER_CE
DWORD InitDHCPV6AdaptEx(PDHCPV6_INIT_ADAPT_CTX  pDhcpV6AdaptCtx,
    PDHCPV6_ADAPT *ppDhcpV6Adapt);
#else
DWORD
InitDHCPV6Adapt(
    DWORD dwIPv6IfIndex,
    PWCHAR pwcAdapterGuid,
    PSOCKADDR_IN6 pSockAddrIn6,
    PDHCPV6_ADAPT *ppDhcpV6Adapt
    );
#endif

DWORD
DereferenceDHCPV6Adapt(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
ReferenceDHCPV6Adapt(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
ShutdownAllDHCPV6Adapt(
    );

DWORD
DHCPV6AdaptFindAndReference(
    DWORD dwIPv6IfIndex,
    OUT PDHCPV6_ADAPT *ppDhcpV6Adapt
    );

#ifdef UNDER_CE
DWORD IniInitDHCPV6Adapt(PIP_ADAPTER_ADDRESSES pIPAdapterAddr);
DWORD IniRemoveDHCPV6AdaptFromTable(PDHCPV6_ADAPT pDhcpV6Adapt);
DWORD InitDHCPV6Socket(PDHCPV6_ADAPT pDhcpV6Adapt);
#else

DWORD
InitDHCPV6AdaptWithGuid(
    PWCHAR pwcAdapterGuid
    );
#endif

