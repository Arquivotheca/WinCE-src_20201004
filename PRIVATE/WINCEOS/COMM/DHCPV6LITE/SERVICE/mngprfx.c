//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// mngprfx.c

#include "mngprfx.h"
#include <winsock2.h>
#include <ipexport.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <winreg.h>

#define MAX_DOWNSTREAM_IFS          8
#ifdef DEBUG
// Only change the period for sending unsolicited RAs if running
// debug version of DHCPv6 to faciliate simpler debugging.
#define DEBUG_PREFIX_ADVERT_MIN_MS  4500
#define DEBUG_PREFIX_ADVERT_MAX_MS  5000
#endif

#define MAX_REG_STR                     128
#define DHCPV6_REGKEY_BASE              TEXT("\\COMM\\DHCPv6L")
#define DHCPV6_REGVAL_DOWNSTREAM_IFS    TEXT("DownstreamInterfaces")
#define DHCPV6_REGVAL_ACCEPT_PD         TEXT("AcceptPrefixDelegation")

HANDLE g_hIPv6Stack = INVALID_HANDLE_VALUE;

//
// Zero-out bits past the given PrefixLen.
//
void TrimPrefix(IPv6Addr* pPrefix, UINT PrefixLen) {
    UINT i;

    if (PrefixLen >= 128) return; // Nothing to do

    if (PrefixLen % 8 != 0) {
        UCHAR mask = 0;
        for (i = 1; i <= PrefixLen % 8; i += 1)
            mask |= 1<<(8-i);
        pPrefix->s6_bytes[PrefixLen/8] &= mask;
    }    
    for (i = (PrefixLen + 7) / 8; i < 16; i += 1)
        pPrefix->s6_bytes[i] = 0;
}

//
// Create a network prefix based on InPrefix and SubnetPrefix using method
// suggested in subnetting example provided in RFC 3633, section 1.2.
//
BOOL CreateSubnetPrefix(
    IPv6Addr* pInPrefix, UINT InPrefixLen, USHORT SubnetPrefix,
    IPv6Addr* pOutPrefix, UINT* pOutPrefixLen
) {
    ASSERT(pInPrefix != NULL && pOutPrefix != NULL && pOutPrefixLen != NULL);

    // We could handle spliting longer prefixes, but there is no need to do this.
    // RFC 3633 (section 1.2), suggests subnetting prefixes based on
    // delegated prefix of only 48 bits.
    //
    if (InPrefixLen > 48) return FALSE;

    memcpy(pOutPrefix, pInPrefix, sizeof(IPv6Addr));
    TrimPrefix(pOutPrefix, InPrefixLen);
    *pOutPrefixLen = InPrefixLen + (InPrefixLen % 16 ? 16 - (InPrefixLen % 16) : 0) + 16;
    pOutPrefix->s6_words[*pOutPrefixLen/16 - 1] = ntohs(SubnetPrefix);

    return TRUE;
}

IPv6Addr* GetIpFromSockAddr(SOCKET_ADDRESS* pSockAddr) {
    return &((PSOCKADDR_IN6)(pSockAddr->lpSockaddr))->sin6_addr;
}

//
// Reconfigures given interface for routing.
//
BOOL UpdateInterface(
    UINT IfIdx,
    BOOL fAdvertises,
    BOOL fForwards
) {
    IPV6_INFO_INTERFACE If;
    UINT BytesReturned;  

    IPV6_INIT_INFO_INTERFACE(&If);
    
    If.This.Index = IfIdx;
    If.Advertises = fAdvertises;
    If.Forwards = fForwards;

#if DEBUG
    If.MinRtrAdvInterval = DEBUG_PREFIX_ADVERT_MIN_MS;
    If.MaxRtrAdvInterval = DEBUG_PREFIX_ADVERT_MAX_MS;    
#endif
    // On RETAIL, use defaults (or otherwise configured current values)
    // for prefix advertisement period.

    return DeviceIoControl(g_hIPv6Stack,
                           IOCTL_IPV6_UPDATE_INTERFACE,
                           &If, sizeof If,
                           NULL, 0, &BytesReturned, NULL);
}

//
// Updates route table entry with given setttings.
//
BOOL UpdateRoute(
    UINT IfIdx,
    IPv6Addr* pIfAddr,
    IPv6Addr* pPrefix,
    UINT PrefixLen,
    UINT ValidLifetime,
    UINT PreferredLifetime,
    UINT Preference,
    BOOL bPublish
) {
    IPV6_INFO_ROUTE_TABLE Route;
    UINT BytesReturned;

    ASSERT(pIfAddr != NULL);
    ASSERT(pPrefix != NULL);
    
    Route.SitePrefixLength = 0;
    Route.ValidLifetime = ValidLifetime;
    Route.PreferredLifetime = PreferredLifetime;
    Route.Preference = Preference;
    Route.Type = RTE_TYPE_AUTOCONF;

    Route.This.Neighbor.IF.Index = IfIdx;
    Route.This.Neighbor.Address = *pIfAddr;

    Route.This.Prefix = *pPrefix;
    Route.This.PrefixLength = PrefixLen;

    Route.Publish = bPublish;
    Route.Immortal = FALSE; // Aging route
    
    return DeviceIoControl(g_hIPv6Stack,
                           IOCTL_IPV6_UPDATE_ROUTE_TABLE,
                           &Route, sizeof Route,
                           NULL, 0, &BytesReturned, NULL);
}

//
// Publish existing routes for the given interface (if not already published).
//
BOOL PublishExistingRoutes(
    UINT IfIdx,
    IPv6Addr* pIfAddr,
    IPv6Addr* pPrefix,
    UINT PrefixLen
) {
    IPV6_QUERY_ROUTE_TABLE Query, NextQuery;
    IPV6_INFO_ROUTE_TABLE Route;
    UINT BytesReturned;
    BOOL fRoutePublished = FALSE;

    NextQuery.Neighbor.IF.Index = 0;
    for (;;) {
        Query = NextQuery;

        if (! DeviceIoControl(g_hIPv6Stack,
                              IOCTL_IPV6_QUERY_ROUTE_TABLE,
                              &Query, sizeof Query,
                              &Route, sizeof Route,
                              &BytesReturned, NULL)) {
            return FALSE;
        }

        NextQuery = Route.Next;

        if (Query.Neighbor.IF.Index == IfIdx) {
            Route.This = Query;

            if (Route.Type != RTE_TYPE_SYSTEM && // Ignore loopback.
                IN6_ADDR_EQUAL(&Route.This.Prefix, pPrefix) &&
                Route.This.PrefixLength == PrefixLen) {

                if (Route.Publish) {
                    // Route has been already published; good enough.
                    fRoutePublished = TRUE;
                } else if (pIfAddr == NULL) {
                    fRoutePublished = 
                        UpdateRoute(IfIdx, &Route.This.Neighbor.Address,
                                    pPrefix, PrefixLen,
                                    Route.ValidLifetime, Route.PreferredLifetime,
                                    Route.Preference, TRUE);
                    if (! fRoutePublished) break; // Error!
                    // Address not specified, other routes are possible.
                } else if (IN6_ADDR_EQUAL(&Route.This.Neighbor.Address, pIfAddr)) {
                    fRoutePublished = 
                        UpdateRoute(IfIdx, pIfAddr,
                                    pPrefix, PrefixLen,
                                    Route.ValidLifetime, Route.PreferredLifetime,
                                    Route.Preference, TRUE);
                    // No more than default route with same address is expected
                    // to be on one interface.
                    break;
                }
            }
        }
        
        if (NextQuery.Neighbor.IF.Index == 0)
            break;
    }
    return fRoutePublished;
}

//
// Ungraciously stolen from tcpipv6\tcpip6\inc\ip6def.h
//
__inline int
IsLinkLocal(const IPv6Addr *Addr) {
    return ((Addr->s6_bytes[0] == 0xfe) &&
            ((Addr->s6_bytes[1] & 0xc0) == 0x80));
}

__inline int
IsSiteLocal(const IPv6Addr *Addr) {
    return ((Addr->s6_bytes[0] == 0xfe) &&
            ((Addr->s6_bytes[1] & 0xc0) == 0xc0));
}

//
// Set or refresh interface address, prefix and route settings and begin
// routing on the interface.
//
BOOL SetOrRefreshPrefix(
    PIP_ADAPTER_ADDRESSES pUpIf,   // Upstream interface
    PIP_ADAPTER_ADDRESSES pDownIf, // Downstream interface
    IPv6Addr* pPrefix,      // Prefix to be assigned
    UINT PrefixLen,         //   Length
    UINT ValidLifetime,     //   Valid lifetime; Note: can use INFINITE_LIFETIME
    UINT PreferredLifetime  //   Preferred lifetime; Note: can use INFINITE_LIFETIME
) {
    BOOL fRet = FALSE; // Be pessimistic
    IPv6Addr NullAddr;

    ASSERT(pUpIf != NULL);
    ASSERT(pDownIf != NULL);
    ASSERT(pPrefix != NULL);

    // Setup null addr that will be used to configure default routes, etc.
    memset(&NullAddr, 0, sizeof(NullAddr));

    //
    // Begin forwarding and advertising on downstream interface and
    // forwarding on the upstream interface
    //
    if (! UpdateInterface(pDownIf->Ipv6IfIndex, TRUE, TRUE ) ) goto cleanup;
    if (! UpdateInterface(pUpIf->Ipv6IfIndex, FALSE, TRUE) ) goto cleanup;
    
    //
    // Configure specific route on downstream interface.
    //
    if (! UpdateRoute(pDownIf->Ipv6IfIndex, &NullAddr,
                      pPrefix, PrefixLen,
                      ValidLifetime, PreferredLifetime,
                      ROUTE_PREF_HIGHEST, TRUE) )
        goto cleanup;

    //
    // Notes:
    // - No need for a default route for downstream interface.
    // - No need to assign global address on downstream interface - it
    //   will be assigned automatically when route is created.

    //
    // Find a default route for public interface and set it to published (if it is
    // not published already). Need to publish default route on upstream interface
    // so that router lifetime advertised in RAs is > 0. (This route will not be
    // advertised on public network.)
    //
    fRet = PublishExistingRoutes(pUpIf->Ipv6IfIndex, NULL,
                                 &NullAddr, 0);

cleanup:
    ASSERT(fRet);
    return fRet;
}

//
// Consult registry settings to determine whether:
// UpIfName   upstream interface can accept prefix delegations, and (optionally)
// DownIfName interface belongs to the set of downstream interfaces for
//            UpIfName. Can be NULL.
//
// If DownIfName is NULL, then function will only check whether UpIfName
// can accept PD.
//
BOOL IsPDEnabledInterface(PCHAR UpIfName, PCHAR DownIfName) {
    BOOL fRet = FALSE; // Be pessimistic
    HKEY hKey = INVALID_HANDLE_VALUE;
    TCHAR szRegValue[MAX_REG_STR];
    TCHAR szDownIfName[MAX_REG_STR];
    DWORD dwType;
    DWORD dwVal;
    DWORD dwValSize;
    DWORD i;

    // At least name of upstream interface needs to be provided
    if (UpIfName == NULL) goto cleanup;

    // Convert names of interfaces
    _sntprintf(szDownIfName, MAX_REG_STR, TEXT("%hs"), DownIfName != NULL ? DownIfName : "\0");
    szDownIfName[MAX_REG_STR - 1] = TEXT('\0');

    // Open the key containing settings of the up-stream interface
    _sntprintf(szRegValue, MAX_REG_STR, TEXT("%s\\%hs"), DHCPV6_REGKEY_BASE, UpIfName);
    szRegValue[MAX_REG_STR - 1] = TEXT('\0');
    if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegValue, 0,
                                      0, &hKey)) {
        hKey = INVALID_HANDLE_VALUE;
        goto cleanup;
    }

    // Check whether the upstream interface should participate in PD
    dwValSize = sizeof(dwVal);
    dwType = REG_DWORD;
    if (ERROR_SUCCESS != RegQueryValueEx(hKey, DHCPV6_REGVAL_ACCEPT_PD, NULL,
                                         &dwType, (LPBYTE)&dwVal, &dwValSize)
        || dwType != REG_DWORD)
        goto cleanup;
    
    if (dwVal == 0) {
        // This interface is not supposed to participate in PD
        goto cleanup;
    }
    if (dwVal == 1 && szDownIfName[0] == TEXT('\0')) {
        // This interface is supposed to participate in PD; no need to check
        // downstream interface because it is not provided
        fRet = TRUE;
        goto cleanup;
    }

    // Check list of downstream interfaces
    dwValSize = MAX_REG_STR * sizeof(TCHAR);
    dwType = REG_MULTI_SZ;
    if (ERROR_SUCCESS != RegQueryValueEx(hKey, DHCPV6_REGVAL_DOWNSTREAM_IFS, NULL,
                                         &dwType, (LPBYTE)szRegValue, &dwValSize)
        || dwType != REG_MULTI_SZ)
        goto cleanup;
    
    // Try to find the downstream interface in the list
    for (i = 0; i < MAX_REG_STR;) {
        if (! _tcscmp(&szRegValue[i], szDownIfName))
            // Found it!
            break;

        i += _tcslen(&szRegValue[i]) + 1;
        if (szRegValue[i] == TEXT('\0'))
            // End of list; didn't find the interface
            goto cleanup;
    }
    fRet = TRUE;

cleanup:
    if (hKey != INVALID_HANDLE_VALUE) RegCloseKey(hKey);
    return fRet;
}

//
// Install new (or refresh) prefix on the system. This function will assign (refresh)
// given prefix on all applicable interfaces.
//
BOOL DHCPv6ManagePrefix(
    UINT OrigIfIdx,         // Interface on which prefix was received
    IPv6Addr Prefix,        // The prefix
    UINT PrefixLen,         //   Length
    UINT ValidLifetime,     //   Valid lifetime; Note: can use INFINITE_LIFETIME
    UINT PreferredLifetime  //   Preferred lifetime; Note: can use INFINITE_LIFETIME
) {
    BOOL fRet = FALSE; // Be pessimistic
    PIP_ADAPTER_ADDRESSES pAADownStreamIfArray[MAX_DOWNSTREAM_IFS];
    PIP_ADAPTER_ADDRESSES pAdapterAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pAAIncomingIf = NULL;
    PIP_ADAPTER_ADDRESSES pAA;
    USHORT DownStreamIfCount;
    ULONG ulFlags;
    ULONG ulBufferLength;
    USHORT usAttempts;
    UINT i;

    ASSERT(g_hIPv6Stack != INVALID_HANDLE_VALUE);

    // The passed-in prefix may actually be longer than required
    TrimPrefix(&Prefix, PrefixLen);

    // Get a list of all interfaces in the system
    ulFlags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
              | GAA_FLAG_INCLUDE_PREFIX;
    pAdapterAddresses = NULL;
    ulBufferLength = 0;
    usAttempts = 0;
    for (;;) {
        DWORD dwError;
        
        dwError = GetAdaptersAddresses(AF_UNSPEC, ulFlags, NULL,
                                       pAdapterAddresses, &ulBufferLength);
        if (dwError == ERROR_SUCCESS)
            break;
        else if (dwError == ERROR_BUFFER_OVERFLOW
                 && ++usAttempts < 4) {
            if (pAdapterAddresses != NULL)
                free(pAdapterAddresses);
            pAdapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(ulBufferLength);
        } else
            goto cleanup;
    }
    
    // Find the interface on which we received the Prefix delegation
    for (pAA = pAdapterAddresses, pAAIncomingIf = NULL;
         pAA != NULL; pAA = pAA->Next) {

        if (pAA->Ipv6IfIndex == OrigIfIdx) {
            pAAIncomingIf = pAA;
            break;
        }
    }
    if (pAAIncomingIf == NULL)
        // We couldn't find the incoming interface; just give up
        goto cleanup;

    // Consult with registry to determine whether the interface on which
    // we recieved PD is a valid upstream interface
    if (! IsPDEnabledInterface(pAAIncomingIf->AdapterName, NULL))
        goto cleanup;

    // Find interfaces that are eligible for prefix assignment
    DownStreamIfCount = 0;
    for (pAA = pAdapterAddresses;
         pAA != NULL && DownStreamIfCount < MAX_DOWNSTREAM_IFS;
         pAA = pAA->Next) {

        // Accept only Ethernet interfaces
        if (pAA->IfType != IF_TYPE_ETHERNET_CSMACD) continue;
        
        // Cannot be the same interface on which we recieved the prefix
        if (pAA->Ipv6IfIndex == OrigIfIdx) continue;

        // Consult with registry to determine whether this is one of the
        // valid downstream interfaces
        if (! IsPDEnabledInterface(pAAIncomingIf->AdapterName, pAA->AdapterName))
            continue;
        
        // This interface is eligible for prefix assignment, remember it
        pAADownStreamIfArray[DownStreamIfCount++] = pAA;

        DEBUGMSG(1,
            (TEXT("DHCPv6ManagePrefix: IF# [%d] is eligible for prefix assignment\r\n"),
            pAA->Ipv6IfIndex));
    }

    // Given number of eligible interfaces for prefix assignment, if:
    // a) no interfaces, then ignore the prefix
    // b) one or more interface, then subnet prefix between interfaces
    //    on interface index number.
    //
    // Note: We could handle (b) to not to subnet prefix if we have
    //       only one downstream interface, but handling dynamic interface
    //       creation would be more complicated.
    // 
    if (DownStreamIfCount == 0) goto cleanup;
    
    for (i = 0; i < DownStreamIfCount; i+=1) {
        PIP_ADAPTER_PREFIX pAP;
        IPv6Addr NewPrefix;
        UINT NewPrefixLen;
        
        pAA = pAADownStreamIfArray[i];

        // Check whether the interface already has the prefix
        for (pAP = pAA->FirstPrefix; pAP != NULL; pAP = pAP->Next) {
            IPv6Addr ThisPrefix;

            if (pAP->PrefixLength == 0 || pAP->PrefixLength == 128) continue;

            ThisPrefix = *GetIpFromSockAddr(&pAP->Address);
            
            TrimPrefix(&ThisPrefix, PrefixLen);
            if (IN6_ADDR_EQUAL(&ThisPrefix, &Prefix)) {

                NewPrefix = *GetIpFromSockAddr(&pAP->Address);
                NewPrefixLen = pAP->PrefixLength;

                break;
            }
        }

        if (pAP == NULL) {
            // The Prefix has never been assigned to this interface; generate one.
            if (! CreateSubnetPrefix(&Prefix, PrefixLen, (USHORT)pAA->Ipv6IfIndex,
                                     &NewPrefix, &NewPrefixLen))
                goto cleanup;
        }

        DEBUGMSG(1,
            (TEXT("DHCPv6ManagePrefix: %s prefix for IF# [%d]\r\n"),
            pAP == NULL ? TEXT("Creating") : TEXT("Refreshing"),
            pAA->Ipv6IfIndex));

        if (! SetOrRefreshPrefix(pAAIncomingIf,
                                 pAA,
                                 &NewPrefix, 
                                 NewPrefixLen,
                                 ValidLifetime,     
                                 PreferredLifetime)) {
            goto cleanup;
        }
    }
    fRet = TRUE;

cleanup:
    if (pAdapterAddresses != NULL)
        free(pAdapterAddresses);
   
    return fRet;
}

//
// Intended to be called periodically to clean-up expired routes.
//
BOOL DHCPv6ManagePrefixPeriodicCleanup() {
    // Cleanup routes
    IPV6_QUERY_ROUTE_TABLE Query, NextQuery;
    IPV6_INFO_ROUTE_TABLE Route;
    UINT BytesReturned;

    ASSERT(g_hIPv6Stack != INVALID_HANDLE_VALUE);

    NextQuery.Neighbor.IF.Index = 0;
    for (;;) {
        Query = NextQuery;

        if (! DeviceIoControl(g_hIPv6Stack,
                              IOCTL_IPV6_QUERY_ROUTE_TABLE,
                              &Query, sizeof Query,
                              &Route, sizeof Route,
                              &BytesReturned, NULL)) {
            return FALSE;
        }

        NextQuery = Route.Next;

        if (Query.Neighbor.IF.Index != 0) {
            Route.This = Query;

            if (Route.Type == RTE_TYPE_AUTOCONF &&
                Route.ValidLifetime == 0 &&
                Route.Publish) {

                // This route has been configured by DHCPv6 earlier, but is
                // no longer valid. We'll stop publishing it so that it can
                // be cleaned-up by the stack.
                //
                UpdateRoute(Route.This.Neighbor.IF.Index,
                            &Route.This.Neighbor.Address,
                            &Route.This.Prefix, Route.This.PrefixLength,
                            Route.ValidLifetime, Route.PreferredLifetime,
                            Route.Preference, FALSE);
            }
        }
        
        if (NextQuery.Neighbor.IF.Index == 0)
            break;
    }
    return TRUE;
}

BOOL DHCPv6ManagePrefixInit() {
    ASSERT(g_hIPv6Stack == INVALID_HANDLE_VALUE);

    if (g_hIPv6Stack == INVALID_HANDLE_VALUE) {
        // Get handle to IPv6 stack for DeviceIoControl calls
        g_hIPv6Stack = CreateFileW(L"IP60:",
                                   GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL,   // security attributes
                                   OPEN_EXISTING,
                                   0,      // flags & attributes
                                   NULL);  // template file
        if (g_hIPv6Stack == INVALID_HANDLE_VALUE) {
            ASSERT(FALSE);
            return FALSE;
        }
    }

    return TRUE;
}

void DHCPv6ManagePrefixDeinit() {
    if (g_hIPv6Stack != INVALID_HANDLE_VALUE) CloseHandle(g_hIPv6Stack);
}

