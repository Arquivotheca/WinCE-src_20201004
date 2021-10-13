//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    natglob.h

Abstract:

    Contains global variable declarations and global constants for the IPNAT component



    David Kanz (davidka) 11-June-1999

Revision History:

--*/

#ifndef _NATGLOB_H_
#define _NATGLOB_H_

#include <rtinfo.h>             // for RTR_INFO_BLOCK_HEADER
#include <ipinfoid.h>           // for IP_GENERAL_INFO_BASE

#ifdef __cplusplus
extern "C" {
#endif

#ifdef UNDER_CE

// Afd function tables
extern PFNVOID *v_apSocketFns;
extern PFNVOID *v_apAfdFns;

// As per NT specs, we give IP address for 10 minute lease times.
#define DHCP_DEFAULT_LEASE_TIME                  10             

extern const TCHAR ICSRegServersKey[];
extern const TCHAR ICSRegAddressKey[];


// FILETIME (100-ns intervals) to minutes (10 x 1000 x 1000 x 60)
#define FILETIME_TO_MINUTES ((__int64)600000000L)
extern ULONG            v_DhcpLeaseTime;
extern unsigned __int64 v_DhcpStoreAddressTime;


// 
// Registry settings for connection sharing
//
#define ICSRegKey                  TEXT("COMM\\ConnectionSharing")
#define ICSRegApplicationsKey      TEXT("COMM\\ConnectionSharing\\Applications")
#define ICSRegServersKey           TEXT("COMM\\ConnectionSharing\\Servers")
#define ICSRegAddressKey           TEXT("COMM\\ConnectionSharing\\Addresses")
#define ICSRegPublicInterface      TEXT("PublicInterface")  // REG_SZ
#define ICSRegPrivateInterface     TEXT("PrivateInterface") // REG_MULTI_SZ
#define ICSRegEnableNAT            TEXT("EnableAddressTranslation") // DWORD
#define ICSRegEnableDhcp           TEXT("EnableDhcpAllocator")      // DWORD
#define ICSRegEnableDns            TEXT("EnableDnsProxy")           // DWORD
#define ICSRegEnableFirewall       TEXT("EnablePacketFiltering")  // DWORD
#define ICSRegDhcpLeaseTime        TEXT("DhcpLeaseTime")
#define ICSRegInternalExposedHost  TEXT("InternalExposedHost")


#define ICSLogRegKey                TEXT("Comm\\IPNat")
#define ICSLogDLLValue              TEXT("FirewallLoggerDLL")
#define ICSLogDropFunction          TEXT("LogDroppedPacket")
#define ICSLogCreationFunction      TEXT("LogConnectionCreation")
#define ICSLogDeletionFunction      TEXT("LogConnectionDeletion")
#define ICSLogInitFunction          TEXT("LogInit")

#define ICSRegProtoTCP              TEXT("TCP")
#define ICSRegProtoUDP              TEXT("UDP")
#define ICS_MAX_PROTO_STRING        4

#endif // UNDER_CE


//  Functions called by NAT init/deinit routines.
BOOLEAN
DhcpInitializeModule(
    VOID
    );

ULONG
DhcpRmAddInterface(
#ifdef UNDER_CE
    WCHAR *IFName,
#endif    
    ULONG Index,
    IN ULONG IPAddr,
    IN ULONG IPMask
    );

ULONG
APIENTRY
DnsRmAddPublicInterface(
    ULONG Index,
    IN ULONG IPAddr
    );

ULONG
DnsRmAddPublicInterface(
    ULONG Index,
    IN ULONG IPAddr
    );


ULONG
DhcpRmDeleteInterface(
    ULONG Index
    );


BOOLEAN
DnsInitializeModule(
    VOID
    );

ULONG
APIENTRY
DnsRmAddPrivateInterface(
    ULONG Index,
    IN ULONG IPAddr,
	IN ULONG IPMask
    );

ULONG
APIENTRY
DnsRmDeleteInterface(
    ULONG Index
    );





#ifdef __cplusplus
}
#endif

#endif // _NATGLOB_H_
