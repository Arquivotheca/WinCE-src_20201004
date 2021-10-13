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

    adapters.c

Abstract:

    Adapter manager for DhcpV6 windows APIs.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
//#include "precomp.h"
//#include "adapters.tmh"


HKEY OpenDHCPv6Key(PDHCPV6_ADAPT pDhcpV6Adapt) {
    TCHAR   Buffer[MAX_PATH];
    HKEY    hKey;
    LONG    hRes;
    DWORD   cLen;
    TCHAR   RegLoc[] = TEXT("\\Parms\\TcpIp");

    _tcscpy (Buffer, COMM_REG_KEY);
    cLen = MAX_PATH - ((sizeof(COMM_REG_KEY)+sizeof(RegLoc)) / sizeof(TCHAR));
    _tcsnccat (Buffer, pDhcpV6Adapt->wszAdapterName, cLen);
    _tcscat (Buffer, RegLoc);

    hRes = RegOpenKeyEx (HKEY_LOCAL_MACHINE, Buffer, 0, 0, &hKey);
    if (hRes) {
        return NULL;
    }
    return hKey;
}   // OpenDHCPv6Key()


#define MAX_IPV6_STRING_ADDR_LEN   60


void DeleteRegistrySettings(PDHCPV6_ADAPT pDhcpV6Adapt, DWORD dwFlags) {
    HKEY    hKey;

    DEBUGMSG (ZONE_INIT, (TEXT("+DeleteRegistrySettings(%s):\r\n"), 
        pDhcpV6Adapt->wszAdapterName));

    // Open the Registry Key.
    hKey = OpenDHCPv6Key(pDhcpV6Adapt);
    if (hKey) {
        if (DEL_REG_PREFIX_FL & dwFlags) {
            RegDeleteValue(hKey, TEXT("PrefixAddress"));
            RegDeleteValue(hKey, TEXT("PrefixLength"));
            RegDeleteValue(hKey, TEXT("PrefixPreferredLifetime"));
            RegDeleteValue(hKey, TEXT("PrefixValidLifetime"));
            RegDeleteValue(hKey, TEXT("PrefixLeaseObtainedLow"));
            RegDeleteValue(hKey, TEXT("PrefixLeaseObtainedHigh"));
            RegDeleteValue(hKey, TEXT("PrefixT1"));
            RegDeleteValue(hKey, TEXT("PrefixT2"));
        }
        if (DEL_REG_DNS_LIST_FL & dwFlags) {
            RegDeleteValue(hKey, TEXT("DhcpV6DNS"));
        }
        if (DEL_REG_DNS_LIST_FL & dwFlags) {
            RegDeleteValue(hKey, TEXT("DhcpV6DomainList"));
        }
        RegCloseKey(hKey);
    }

}   // DeleteRegistrySettings()


void SaveRegistrySettings(PDHCPV6_ADAPT pDhcpV6Adapt) {
    HKEY    hKey;
    int     Status;
    DWORD   i, cLen, DomainNameLen = 0;
    TCHAR   Buf[MAX_PATH], *pBuf;
    SOCKADDR_IN6    Sock6;

    DEBUGMSG (ZONE_INIT, (TEXT("+SaveRegistrySettings(%s):\r\n"), 
        pDhcpV6Adapt->wszAdapterName));

    // Open the Registry Key.
    hKey = OpenDHCPv6Key(pDhcpV6Adapt);
    if (hKey) {
        // prefix
        if (pDhcpV6Adapt->pPdOption) {
            DHCPV6_IA_PREFIX    *pPrefix;

            pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;
            cLen = sizeof(Buf) / sizeof(Buf[0]);
            memset(&Sock6, 0, sizeof(Sock6));
            memcpy(&(Sock6.sin6_addr), &pPrefix->PrefixAddr, sizeof(IN6_ADDR));
            Sock6.sin6_family = AF_INET6;
            
            Status = WSAAddressToString((PSOCKADDR)&Sock6, sizeof(Sock6), 
                NULL, Buf, &cLen);

            if (SOCKET_ERROR != Status) {
                SetRegSZValue(hKey, TEXT("PrefixAddress"), Buf);
                SetRegDWORDValue(hKey, TEXT("PrefixLength"), pPrefix->cPrefix);

                SetRegDWORDValue(hKey, TEXT("PrefixPreferredLifetime"), 
                    pPrefix->PreferedLifetime);
                SetRegDWORDValue(hKey, TEXT("PrefixValidLifetime"), 
                    pPrefix->ValidLifetime);
                SetRegDWORDValue(hKey, TEXT("PrefixLeaseObtainedLow"), 
                    pPrefix->IALeaseObtained.dwLowDateTime);
                SetRegDWORDValue(hKey, TEXT("PrefixLeaseObtainedHigh"), 
                    pPrefix->IALeaseObtained.dwHighDateTime);
                SetRegDWORDValue(hKey, TEXT("PrefixT1"), 
                    pDhcpV6Adapt->pPdOption->T1);
                SetRegDWORDValue(hKey, TEXT("PrefixT2"), 
                    pDhcpV6Adapt->pPdOption->T2);
            } else {
                DEBUGMSG(ZONE_ERROR, 
                    (TEXT("SaveRegistrySettings: WSAAddressToString returned error %d\r\n"),
                    WSAGetLastError()));
            }
        }
        
        // dns servers
        if (pDhcpV6Adapt->pIpv6DNSServers) {

            if (pBuf = AllocDHCPV6Mem(MAX_IPV6_STRING_ADDR_LEN * 
                pDhcpV6Adapt->uNumOfDNSServers)) {
                
                uint    cLocation = 0;

                for (i = 0; i < pDhcpV6Adapt->uNumOfDNSServers; i++) {
                    // note cLen is passed in as chars to WSAStringToAddress
                    cLen = MAX_IPV6_STRING_ADDR_LEN;

                    memset(&Sock6, 0, sizeof(Sock6));
                    memcpy(&(Sock6.sin6_addr), 
                        &(pDhcpV6Adapt->pIpv6DNSServers[i]), sizeof(IN6_ADDR));
                    Sock6.sin6_family = AF_INET6;
                    
                    Status = WSAAddressToString((PSOCKADDR)&Sock6, 
                        sizeof(Sock6), NULL, &pBuf[cLocation], &cLen);

                    if (SOCKET_ERROR != Status) {
                        // note WSAAddressToString actually returns # of chars
                        cLocation += cLen;
                    }

                }
                pBuf[cLocation++] = TEXT('\0');
                // if RegSetValueEx call fails there is nothing to do
                RegSetValueEx (hKey, TEXT("DhcpV6DNS"), 0, REG_MULTI_SZ,
                    (LPBYTE)pBuf, cLocation * sizeof(TCHAR));  

                FreeDHCPV6Mem(pBuf);
            }


        }

        // dns domain list
        cLen = pDhcpV6Adapt->cDomainList;
        if (cLen && pDhcpV6Adapt->pDomainList) {

            if (pBuf = AllocDHCPV6Mem((cLen + 2) * sizeof(TCHAR))) {
                char    *p;
                TCHAR   *pBuf2 = pBuf;

                for (i = 0, p = pDhcpV6Adapt->pDomainList; i < cLen; ) {
                    Status = mbstowcs(pBuf2, p, cLen - i);
                    if (Status < 0) {
                        goto SkipDomainList;
                    } else if (0 == Status) {
                        break;
                    }
                    Status++;
                    i += Status;
                    p += Status;
                    pBuf2 += Status;
                }

                // make sure MULTI_SZ ends with two nulls!
                if (1 == cLen)
                    pBuf[cLen++] = '\0';
                
                if (pBuf[cLen - 1] != '\0')
                    pBuf[cLen++] = '\0';
                if (pBuf[cLen - 2] != '\0')
                    pBuf[cLen++] = '\0';
                
                RegSetValueEx(hKey, TEXT("DhcpV6DomainList"), 0, REG_MULTI_SZ,
                    (LPBYTE)pBuf, cLen);

SkipDomainList:
                FreeDHCPV6Mem(pBuf);
                
            }
        }

        RegCloseKey(hKey);
    }

}   // SaveRegistrySettings()


VOID
AddDHCPV6Adapter(
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_INIT_ADAPT_CTX pDhcpV6InitAdaptCtx = (PDHCPV6_INIT_ADAPT_CTX)pvContext1;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;
    PLIST_ENTRY     pHead = &AdapterList;
    PLIST_ENTRY     pEntry = pHead->Flink;


    AcquireExclusiveLock(gpAdapterRWLock);

    while(pEntry != pHead) {
        PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

        pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
        pEntry = pEntry->Flink;

        if (pTempDhcpV6Adapt->dwIPv6IfIndex == pDhcpV6InitAdaptCtx->dwIfIndex) {
            dwError = ERROR_ALREADY_EXISTS;
            BAIL_ON_LOCK_ERROR(dwError);
        }
    }

    dwError = InitDHCPV6AdaptEx(pDhcpV6InitAdaptCtx, &pDhcpV6Adapt);
    BAIL_ON_LOCK_ERROR(dwError);

#ifdef UNDER_CE
    wcsncpy(pDhcpV6Adapt->wszAdapterName, 
        pDhcpV6InitAdaptCtx->wszAdapterName, DHCPV6_MAX_NAME_SIZE);
    pDhcpV6Adapt->cPhysicalAddr = pDhcpV6InitAdaptCtx->cPhysicalAddr;
    memcpy(pDhcpV6Adapt->PhysicalAddr, pDhcpV6InitAdaptCtx->PhysicalAddr,
        pDhcpV6InitAdaptCtx->cPhysicalAddr);
#endif

    dwError = ReferenceDHCPV6Adapt(pDhcpV6Adapt);
    BAIL_ON_LOCK_ERROR(dwError);

    ReleaseExclusiveLock(gpAdapterRWLock);

    dwError = InitDHCPV6MessageMgr(pDhcpV6Adapt);
    BAIL_ON_WIN32_ERROR(dwError);

error:
    if (pDhcpV6InitAdaptCtx) {
        FreeDHCPV6Mem(pDhcpV6InitAdaptCtx);
    }

    return;

lock:
    ReleaseExclusiveLock(gpAdapterRWLock);

    goto error;
}


DWORD
InitDHCPV6AdaptAtStartup(
    )
{
    DWORD dwError = 0;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIP_ADAPTER_ADDRESSES pIPAdapterAddresses = NULL;
    ULONG uIPAdapterAddressesSize;
    PIP_ADAPTER_ADDRESSES pTempIPAdapterAddresses = NULL;
    PDHCPV6_INIT_ADAPT_CTX pDhcpV6InitAdaptCtx = NULL;

    Sleep(15000);

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Begin Initializing Adapters at startup with Error: %!status!", dwError));

    uIPAdapterAddressesSize = 3000; // sizeof(IP_ADAPTER_ADDRESSES);
    pIPAdapterAddresses = AllocDHCPV6Mem(uIPAdapterAddressesSize);
    if (!pIPAdapterAddresses) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pIPAdapterAddresses, 0, uIPAdapterAddressesSize);

    dwError = GetAdaptersAddresses(
                    AF_INET6,
                    0,
                    NULL,
                    pIPAdapterAddresses,
                    &uIPAdapterAddressesSize
                    );

    while (dwError == ERROR_BUFFER_OVERFLOW || dwError == ERROR_MORE_DATA) {

        if (pIPAdapterAddresses) {
            FreeDHCPV6Mem(pIPAdapterAddresses);
            pIPAdapterAddresses = NULL;
        }

        pIPAdapterAddresses = AllocDHCPV6Mem(uIPAdapterAddressesSize);
        if (!pIPAdapterAddresses) {
            dwError = ERROR_OUTOFMEMORY;
            BAIL_ON_WIN32_ERROR(dwError);
        }
        memset(pIPAdapterAddresses, 0, uIPAdapterAddressesSize);

        dwError = GetAdaptersAddresses(
                        AF_INET6,
                        0,
                        NULL,
                        pIPAdapterAddresses,
                        &uIPAdapterAddressesSize
                        );
    }

    if (dwError != ERROR_SUCCESS) {
        dwError = 0;
        goto error;
    }

    // attach adapters to global
    for(pTempIPAdapterAddresses = pIPAdapterAddresses;
        pTempIPAdapterAddresses != NULL;
        pTempIPAdapterAddresses = pTempIPAdapterAddresses->Next) {

        PIP_ADAPTER_UNICAST_ADDRESS pIPAdapterUnicastAddress = NULL;
        PSOCKET_ADDRESS pSocketAddress = NULL;
        PSOCKADDR_IN6 pSockAddrIn6Temp = NULL;
        ULONG uLength = 0;
        WCHAR wcCurrentAdapterGuid[GUID_LENGTH] = { 0 };
        GUID CurrentAdapterGuid = { 0 };
        UNICODE_STRING uncGuid = { 0 };


        dwError = ConvertInterfaceLuidToGuid(&pTempIPAdapterAddresses->Luid, &CurrentAdapterGuid);
        if (dwError) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, ("ConvertInterfaceLuidToGuid Failed with Error: %!status!", dwError));
            continue;
        }

        ntStatus = RtlStringFromGUID(&CurrentAdapterGuid, &uncGuid);
        dwError = RtlNtStatusToDosError(ntStatus);
        if (dwError) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, ("Converting String From GUID Failed for %!GUID!", &CurrentAdapterGuid));
            continue;
        }

        RtlCopyMemory(wcCurrentAdapterGuid, uncGuid.Buffer, uncGuid.Length);
        wcCurrentAdapterGuid[uncGuid.Length/sizeof(WCHAR)] = L'\0';
        RtlFreeUnicodeString(&uncGuid);
        uncGuid.Buffer = NULL;
#if 1
        if (pTempIPAdapterAddresses->Ipv6OtherStatefulConfig != 1) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_INFO,
                            ("Adapter with GUID: %S on Index: %d - does not have Ipv6OtherStatefulConfig set in Router Advertisement - skipping",
                            wcCurrentAdapterGuid, pTempIPAdapterAddresses->Ipv6IfIndex));
            continue;
        }
#endif
        if (pTempIPAdapterAddresses->FirstUnicastAddress == NULL) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, ("WARN: Adapter does NOT have an IPv6 Address - Skipping Adapter LUID: %s, GUID: %S", pTempIPAdapterAddresses->AdapterName, wcCurrentAdapterGuid));
            continue;
        }
        pIPAdapterUnicastAddress = pTempIPAdapterAddresses->FirstUnicastAddress;

#ifdef UNDER_CE

        if (IF_TYPE_OTHER == pTempIPAdapterAddresses->IfType ||
            IF_TYPE_SOFTWARE_LOOPBACK == pTempIPAdapterAddresses->IfType||
            IF_TYPE_TUNNEL == pTempIPAdapterAddresses->IfType) {
            continue;
        }

        while (pIPAdapterUnicastAddress) {                         
            pSocketAddress = &pIPAdapterUnicastAddress->Address;
            pSockAddrIn6Temp = (PSOCKADDR_IN6)pSocketAddress->lpSockaddr;
            if (IN6_IS_ADDR_LINKLOCAL(&pSockAddrIn6Temp->sin6_addr))
                break;
            pIPAdapterUnicastAddress = pIPAdapterUnicastAddress->Next;
        }
        if (! pIPAdapterUnicastAddress) {
            DEBUGMSG(ZONE_WARN, 
                (TEXT("!DhcpV6: No Linklocal addres on this adapter\r\n")));
            continue;
        }

#endif

        pSocketAddress = &pIPAdapterUnicastAddress->Address;
        pSockAddrIn6Temp = (PSOCKADDR_IN6)pSocketAddress->lpSockaddr;
        ASSERT(pSockAddrIn6Temp->sin6_family == AF_INET6);
        if (pSocketAddress->iSockaddrLength != sizeof(SOCKADDR_IN6)) {
            ASSERT(0);
            continue;
        }

        uLength = sizeof(DHCPV6_INIT_ADAPT_CTX);
        pDhcpV6InitAdaptCtx = AllocDHCPV6Mem(uLength);
        if (!pDhcpV6InitAdaptCtx) {
            dwError = ERROR_OUTOFMEMORY;
            BAIL_ON_WIN32_ERROR(dwError);
        }
        memset(pDhcpV6InitAdaptCtx, 0, uLength);

#ifdef UNDER_CE
        if (IsPDEnabledInterface(pTempIPAdapterAddresses->AdapterName, NULL)) {
            pDhcpV6InitAdaptCtx->IsPDEnabled = TRUE;
        }
#endif

        pDhcpV6InitAdaptCtx->dwIfIndex = pTempIPAdapterAddresses->Ipv6IfIndex;
        memcpy(
            pDhcpV6InitAdaptCtx->wcAdapterGuid,
            wcCurrentAdapterGuid,
            sizeof(wcCurrentAdapterGuid)
            );
        memcpy(
            &pDhcpV6InitAdaptCtx->SockAddrIn6,
            pSockAddrIn6Temp,
            sizeof(SOCKADDR_IN6)
            );

#if 0
        if (IN6_IS_ADDR_LINKLOCAL(&(pDhcpV6InitAdaptCtx->SockAddrIn6))) {
            pDhcpV6InitAdaptCtx->SockAddrIn6.sin6_scope_id = 
                pTempIPAdapterAddresses->ZoneIndices[ScopeLevelLink];
        } else if (IN6_IS_ADDR_SITELOCAL(&(pDhcpV6InitAdaptCtx->SockAddrIn6))) {
            pDhcpV6InitAdaptCtx->SockAddrIn6.sin6_scope_id = 
                pTempIPAdapterAddresses->ZoneIndices[ScopeLevelSite];
        }
#endif

#ifdef UNDER_CE
        if (MAX_ADAPTER_ADDRESS_LENGTH < 
            pTempIPAdapterAddresses->PhysicalAddressLength) {
            pDhcpV6InitAdaptCtx->cPhysicalAddr = MAX_ADAPTER_ADDRESS_LENGTH;
        } else {
            pDhcpV6InitAdaptCtx->cPhysicalAddr = 
                pTempIPAdapterAddresses->PhysicalAddressLength;
        }
        memcpy(pDhcpV6InitAdaptCtx->PhysicalAddr,
            pTempIPAdapterAddresses->PhysicalAddress,
            pDhcpV6InitAdaptCtx->cPhysicalAddr);

        mbstowcs(pDhcpV6InitAdaptCtx->wszAdapterName, 
            pTempIPAdapterAddresses->AdapterName, DHCPV6_MAX_NAME_SIZE);
        pDhcpV6InitAdaptCtx->wszAdapterName[DHCPV6_MAX_NAME_SIZE-1] = 
            TEXT('\0');

#endif

        dwError = DhcpV6EventAddEvent(
                    gpDhcpV6EventModule,
                    pTempIPAdapterAddresses->Ipv6IfIndex,
                    AddDHCPV6Adapter,
                    pDhcpV6InitAdaptCtx,
                    NULL
                    );
        BAIL_ON_WIN32_ERROR(dwError);
        pDhcpV6InitAdaptCtx = NULL;
    }

success:
    ASSERT(ghAddrChangeThread);
    if (ghAddrChangeThread) {
        ResumeThread(ghAddrChangeThread);
        CloseHandle(ghAddrChangeThread);
        ghAddrChangeThread = NULL;
    }
    if (pIPAdapterAddresses) {
        FreeDHCPV6Mem(pIPAdapterAddresses);
        pIPAdapterAddresses = NULL;
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Adapters at startup with Error: %!status!", dwError));

    return dwError;

error:

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Initializing Adapters at startup with Error: %!status!", dwError));

    if (pDhcpV6InitAdaptCtx) {
        FreeDHCPV6Mem(pDhcpV6InitAdaptCtx);
        pDhcpV6InitAdaptCtx = NULL;
    }

    goto success;
}

#ifndef UNDER_CE
DWORD
DHCPV6AdaptFindAndReferenceWithGuid(
    PWCHAR pwcAdapterGuid,
    OUT PDHCPV6_ADAPT *ppDhcpV6Adapt
    )
{
    DWORD dwError = STATUS_NOT_FOUND;
    PLIST_ENTRY     pHead = &AdapterList;
    PLIST_ENTRY     pEntry = pHead->Flink;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Finding Adapter with Guid: %S", pwcAdapterGuid));

    AcquireExclusiveLock(gpAdapterRWLock);
    while(pEntry != pHead) {
        PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

        pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
        pEntry = pEntry->Flink;

        if(_wcsnicmp(pTempDhcpV6Adapt->wcAdapterGuid, pwcAdapterGuid, wcslen(pwcAdapterGuid)) == 0) {
            dwError = ERROR_SUCCESS;
            if (ppDhcpV6Adapt) {
                ReferenceDHCPV6Adapt(pTempDhcpV6Adapt);
                *ppDhcpV6Adapt = pTempDhcpV6Adapt;
            }
            break;
        }
    }
    ReleaseExclusiveLock(gpAdapterRWLock);

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Finding Adapter with Guid: %S with Error: %!status!", pwcAdapterGuid, dwError));

    return dwError;
}


DWORD
IniInitDHCPV6AdaptWithGuid(
    PWCHAR pwcAdapterGuid
    )
{
    DWORD dwError = 0;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIP_ADAPTER_ADDRESSES pIPAdapterAddresses = NULL;
    ULONG uIPAdapterAddressesSize = 0;
    PIP_ADAPTER_ADDRESSES pTempIPAdapterAddresses = NULL;
    PDHCPV6_INIT_ADAPT_CTX pDhcpV6InitAdaptCtx = NULL;
    BOOL bAdapterFound = FALSE;


    uIPAdapterAddressesSize = sizeof(IP_ADAPTER_ADDRESSES);
    pIPAdapterAddresses = AllocDHCPV6Mem(uIPAdapterAddressesSize);
    if (!pIPAdapterAddresses) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pIPAdapterAddresses, 0, uIPAdapterAddressesSize);

    dwError = GetAdaptersAddresses(
                    AF_INET6,
                    0,
                    NULL,
                    pIPAdapterAddresses,
                    &uIPAdapterAddressesSize
                    );

    while (dwError == ERROR_BUFFER_OVERFLOW || dwError == ERROR_MORE_DATA) {

        if (pIPAdapterAddresses) {
            FreeDHCPV6Mem(pIPAdapterAddresses);
            pIPAdapterAddresses = NULL;
        }

        pIPAdapterAddresses = AllocDHCPV6Mem(uIPAdapterAddressesSize);
        if (!pIPAdapterAddresses) {
            dwError = ERROR_OUTOFMEMORY;
            BAIL_ON_WIN32_ERROR(dwError);
        }
        memset(pIPAdapterAddresses, 0, uIPAdapterAddressesSize);

        dwError = GetAdaptersAddresses(
                        AF_INET6,
                        0,
                        NULL,
                        pIPAdapterAddresses,
                        &uIPAdapterAddressesSize
                        );
    }

    if (dwError != ERROR_SUCCESS) {
        dwError = 0;
        goto error;
    }

    // attach adapters to global
    for(pTempIPAdapterAddresses = pIPAdapterAddresses;
        pTempIPAdapterAddresses != NULL && bAdapterFound == FALSE;
        pTempIPAdapterAddresses = pTempIPAdapterAddresses->Next) {

        PIP_ADAPTER_UNICAST_ADDRESS pIPAdapterUnicastAddress = NULL;
        PSOCKET_ADDRESS pSocketAddress = NULL;
        PSOCKADDR_IN6 pSockAddrIn6Temp = NULL;
        ULONG uLength = 0;
        WCHAR wcCurrentAdapterGuid[GUID_LENGTH] = { 0 };
        GUID CurrentAdapterGuid = { 0 };
        UNICODE_STRING uncGuid = { 0 };


        dwError = ConvertInterfaceLuidToGuid(&pTempIPAdapterAddresses->Luid, &CurrentAdapterGuid);
        if (dwError) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, ("ConvertInterfaceLuidToGuid Failed with Error: %!status!", dwError));
            continue;
        }

        ntStatus = RtlStringFromGUID(&CurrentAdapterGuid, &uncGuid);
        dwError = RtlNtStatusToDosError(ntStatus);
        if (dwError) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, ("Converting String From GUID Failed for %!GUID!", &CurrentAdapterGuid));
            continue;
        }

        RtlCopyMemory(wcCurrentAdapterGuid, uncGuid.Buffer, uncGuid.Length);
        wcCurrentAdapterGuid[uncGuid.Length/sizeof(WCHAR)] = L'\0';
        RtlFreeUnicodeString(&uncGuid);
        uncGuid.Buffer = NULL;

        if (_wcsnicmp(pwcAdapterGuid, wcCurrentAdapterGuid, wcslen(pwcAdapterGuid)) != 0) {
            continue;
        }

        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_INFO, ("FOUND: Adapter with GUID: %S", pwcAdapterGuid));
        bAdapterFound = TRUE;

        if (pTempIPAdapterAddresses->Ipv6OtherStatefulConfig != 1) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_INFO, ("Adapter with GUID: %S does not have Ipv6OtherStatefulConfig set in Router Advertisement - skipping", pwcAdapterGuid));
            continue;
        }

        if (pTempIPAdapterAddresses->FirstUnicastAddress == NULL) {
            DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, ("WARN: Adapter does NOT have an IPv6 Address - Skipping Adapter LUID: %s, GUID: %S", pTempIPAdapterAddresses->AdapterName, wcCurrentAdapterGuid));
            continue;
        }

        pIPAdapterUnicastAddress = pTempIPAdapterAddresses->FirstUnicastAddress;

        pSocketAddress = &pIPAdapterUnicastAddress->Address;
        pSockAddrIn6Temp = (PSOCKADDR_IN6)pSocketAddress->lpSockaddr;
        ASSERT(pSockAddrIn6Temp->sin6_family == AF_INET6);
        if (pSocketAddress->iSockaddrLength != sizeof(SOCKADDR_IN6)) {
            ASSERT(0);
            continue;
        }

        uLength = sizeof(DHCPV6_INIT_ADAPT_CTX);
        pDhcpV6InitAdaptCtx = AllocDHCPV6Mem(uLength);
        if (!pDhcpV6InitAdaptCtx) {
            dwError = ERROR_OUTOFMEMORY;
            BAIL_ON_WIN32_ERROR(dwError);
        }
        memset(pDhcpV6InitAdaptCtx, 0, uLength);

        pDhcpV6InitAdaptCtx->dwIfIndex = pTempIPAdapterAddresses->Ipv6IfIndex;
        memcpy(
            pDhcpV6InitAdaptCtx->wcAdapterGuid,
            wcCurrentAdapterGuid,
            sizeof(pDhcpV6InitAdaptCtx->wcAdapterGuid)
            );
        memcpy(
            &pDhcpV6InitAdaptCtx->SockAddrIn6,
            pSockAddrIn6Temp,
            sizeof(SOCKADDR_IN6)
            );

        dwError = DhcpV6EventAddEvent(
                    gpDhcpV6EventModule,
                    pTempIPAdapterAddresses->Ipv6IfIndex,
                    AddDHCPV6Adapter,
                    pDhcpV6InitAdaptCtx,
                    NULL
                    );
        BAIL_ON_WIN32_ERROR(dwError);
        pDhcpV6InitAdaptCtx = NULL;
    }

success:
    if (pIPAdapterAddresses) {
        FreeDHCPV6Mem(pIPAdapterAddresses);
        pIPAdapterAddresses = NULL;
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Adapters with Guid with Error: %!status!", dwError));

    return dwError;

error:

    if (pIPAdapterAddresses) {
        FreeDHCPV6Mem(pIPAdapterAddresses);
        pIPAdapterAddresses = NULL;
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Initializing Adapters at startup with Error: %!status!", dwError));

    if (pDhcpV6InitAdaptCtx) {
        FreeDHCPV6Mem(pDhcpV6InitAdaptCtx);
        pDhcpV6InitAdaptCtx = NULL;
    }

    goto success;
}
#endif  // ifndef UNDER_CE

#ifdef UNDER_CE

DWORD
DHCPV6AdaptFindAndReferenceWithName(
    PWCHAR AdaptName,
    OUT PDHCPV6_ADAPT *ppDhcpV6Adapt
    )
{
    DWORD dwError = STATUS_NOT_FOUND;
    PLIST_ENTRY     pHead = &AdapterList;
    PLIST_ENTRY     pEntry = pHead->Flink;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Finding Adapter with Guid: %S", pwcAdapterGuid));

    AcquireExclusiveLock(gpAdapterRWLock);
    while(pEntry != pHead) {
        PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

        pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
        pEntry = pEntry->Flink;

        if(0 == _wcsnicmp(pTempDhcpV6Adapt->wszAdapterName, AdaptName,
            DHCPV6_MAX_NAME_SIZE)) {
            dwError = ERROR_SUCCESS;
            if (ppDhcpV6Adapt) {
                ReferenceDHCPV6Adapt(pTempDhcpV6Adapt);
                *ppDhcpV6Adapt = pTempDhcpV6Adapt;
            }
            break;
        }
    }
    ReleaseExclusiveLock(gpAdapterRWLock);

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Finding Adapter with Guid: %S with Error: %!status!", pwcAdapterGuid, dwError));

    return dwError;
}


DWORD
IniInitDHCPV6Adapt(
    PIP_ADAPTER_ADDRESSES pIPAdapterAddr
    )
{
    DWORD dwError = 0;
    PDHCPV6_INIT_ADAPT_CTX pDhcpV6InitAdaptCtx = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pIPAdapterUnicastAddress = NULL;
    PSOCKET_ADDRESS pSocketAddress = NULL;
    PSOCKADDR_IN6 pSockAddrIn6Temp = NULL;
    ULONG uLength = 0;

    // skip the Luid/Guid stuff for CE

    if (pIPAdapterAddr->Ipv6OtherStatefulConfig != 1) {
        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_INFO, 
            ("Adapter with GUID: %S does not have Ipv6OtherStatefulConfig set in Router Advertisement - skipping", pwcAdapterGuid));
        // we may have to do something here to make sure that existing PD's
        // are passed down to this adapter
        BAIL_ON_WIN32_SUCCESS(dwError);
    }

#ifdef UNDER_CE
    if (IF_TYPE_OTHER == pIPAdapterAddr->IfType ||
        IF_TYPE_SOFTWARE_LOOPBACK == pIPAdapterAddr->IfType||
        IF_TYPE_TUNNEL == pIPAdapterAddr->IfType) {
        BAIL_ON_WIN32_SUCCESS(dwError);
    }
#endif

    if (pIPAdapterAddr->FirstUnicastAddress == NULL) {
        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_WARN, 
            ("WARN: Adapter does NOT have an IPv6 Address - Skipping Adapter LUID: %s, GUID: %S", pIPAdapterAddr->AdapterName, wcCurrentAdapterGuid));
        // no address--what can we do here!
        goto error;
    }

    pIPAdapterUnicastAddress = pIPAdapterAddr->FirstUnicastAddress;

#ifdef UNDER_CE
    while (pIPAdapterUnicastAddress) {                         
        pSocketAddress = &pIPAdapterUnicastAddress->Address;
        pSockAddrIn6Temp = (PSOCKADDR_IN6)pSocketAddress->lpSockaddr;
        if (IN6_IS_ADDR_LINKLOCAL(&pSockAddrIn6Temp->sin6_addr))
            break;
        pIPAdapterUnicastAddress = pIPAdapterUnicastAddress->Next;
    }
    if (! pIPAdapterUnicastAddress) {
        DEBUGMSG(ZONE_WARN, 
            (TEXT("!DhcpV6: IniInitDHCPV6Adapt: No Linklocal addres on this adapter\r\n")));
        goto error;
    }

#endif

    ASSERT(pSockAddrIn6Temp->sin6_family == AF_INET6);
    if (pSocketAddress->iSockaddrLength != sizeof(SOCKADDR_IN6)) {
        ASSERT(0);
        goto error;
    }

    uLength = sizeof(DHCPV6_INIT_ADAPT_CTX);
    pDhcpV6InitAdaptCtx = AllocDHCPV6Mem(uLength);
    if (!pDhcpV6InitAdaptCtx) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pDhcpV6InitAdaptCtx, 0, uLength);

    pDhcpV6InitAdaptCtx->dwIfIndex = pIPAdapterAddr->Ipv6IfIndex;

#ifdef UNDER_CE
    if (IsPDEnabledInterface(pIPAdapterAddr->AdapterName, NULL)) {
        pDhcpV6InitAdaptCtx->IsPDEnabled = TRUE;
    }
#endif

    memcpy(
        &pDhcpV6InitAdaptCtx->SockAddrIn6,
        pSockAddrIn6Temp,
        sizeof(SOCKADDR_IN6)
        );

#ifdef UNDER_CE
    if (MAX_ADAPTER_ADDRESS_LENGTH < pIPAdapterAddr->PhysicalAddressLength) {
        pDhcpV6InitAdaptCtx->cPhysicalAddr = MAX_ADAPTER_ADDRESS_LENGTH;
    } else {
        pDhcpV6InitAdaptCtx->cPhysicalAddr = 
            pIPAdapterAddr->PhysicalAddressLength;
    }
    memcpy(pDhcpV6InitAdaptCtx->PhysicalAddr, pIPAdapterAddr->PhysicalAddress,
        pDhcpV6InitAdaptCtx->cPhysicalAddr);
    
    mbstowcs(pDhcpV6InitAdaptCtx->wszAdapterName, 
        pIPAdapterAddr->AdapterName, DHCPV6_MAX_NAME_SIZE);
    pDhcpV6InitAdaptCtx->wszAdapterName[DHCPV6_MAX_NAME_SIZE-1] = 
        TEXT('\0');
#endif

    dwError = DhcpV6EventAddEvent(
                gpDhcpV6EventModule,
                pIPAdapterAddr->Ipv6IfIndex,
                AddDHCPV6Adapter,
                pDhcpV6InitAdaptCtx,
                NULL
                );
    BAIL_ON_WIN32_ERROR(dwError);
    pDhcpV6InitAdaptCtx = NULL;


success:

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Adapters with Guid with Error: %!status!", dwError));

    return dwError;

error:


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Initializing Adapters at startup with Error: %!status!", dwError));

    if (pDhcpV6InitAdaptCtx) {
        FreeDHCPV6Mem(pDhcpV6InitAdaptCtx);
        pDhcpV6InitAdaptCtx = NULL;
    }

    goto success;
}


#else

DWORD
InitDHCPV6AdaptWithGuid(
    PWCHAR pwcAdapterGuid
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Begin Initializing Adapters With Guid: %S", pwcAdapterGuid));

    dwError = DHCPV6AdaptFindAndReferenceWithGuid(pwcAdapterGuid, &pDhcpV6Adapt);
    if (dwError == ERROR_SUCCESS) {

        ASSERT(pDhcpV6Adapt != NULL);

        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("FOUND Existing Adapters With Guid: %S on Index: %d", pwcAdapterGuid, pDhcpV6Adapt->dwIPv6IfIndex));
        dwError = DHCPV6MessageMgrPerformRefresh(pDhcpV6Adapt);
        BAIL_ON_WIN32_ERROR(dwError);

    } else {

        ASSERT(pDhcpV6Adapt == NULL);

        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Adapters With Guid: %S NOT FOUND", pwcAdapterGuid));
        dwError = IniInitDHCPV6AdaptWithGuid(pwcAdapterGuid);
        BAIL_ON_WIN32_ERROR(dwError);

    }

error:

    if (pDhcpV6Adapt) {
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Adapters with Guid with Error: %!status!", dwError));

    return dwError;

}

#endif  // UNDER_CE


DWORD InitDHCPV6Socket(PDHCPV6_ADAPT pDhcpV6Adapt) {
    DWORD   dwError = 0;

    if (pDhcpV6Adapt->Socket && 
        (INVALID_SOCKET != pDhcpV6Adapt->Socket)) {
        closesocket(pDhcpV6Adapt->Socket);
        pDhcpV6Adapt->Socket = INVALID_SOCKET;
    }

    pDhcpV6Adapt->Socket = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, 
        NULL, 0, 0);
    if (INVALID_SOCKET == pDhcpV6Adapt->Socket) {
        dwError = WSAGetLastError();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    dwError = bind(pDhcpV6Adapt->Socket, (PSOCKADDR)&pDhcpV6Adapt->SockAddrIn6,
        sizeof(pDhcpV6Adapt->SockAddrIn6));
    if(dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    return dwError;

error:

    // if bind failed delete the socket
    if (INVALID_SOCKET != pDhcpV6Adapt->Socket) {
        closesocket(pDhcpV6Adapt->Socket);
        pDhcpV6Adapt->Socket =  INVALID_SOCKET;
    }
    
    return dwError;

} // InitDHCPV6Socket()


#ifdef UNDER_CE
DWORD
InitDHCPV6AdaptEx(
    PDHCPV6_INIT_ADAPT_CTX  pDhcpV6AdaptCtx,
    PDHCPV6_ADAPT *ppDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uLength = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;
    DOUBLE dbRand = 0;
    BOOL    UsePD;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Initializing Adapter on interface: %!IPV6ADDR! [%d]",
                    (CONST UCHAR*) &pDhcpV6AdaptCtx->SockAddrIn6->sin6_addr, dwIPv6IfIndex));

    uLength = sizeof(DHCPV6_ADAPT);

    pDhcpV6Adapt = AllocDHCPV6Mem(uLength);
    if (!pDhcpV6Adapt) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pDhcpV6Adapt, 0, uLength);

    dwError = InitializeRWLock(&pDhcpV6Adapt->RWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    pDhcpV6Adapt->uRefCount = 1;

    pDhcpV6Adapt->dwIPv6IfIndex = pDhcpV6AdaptCtx->dwIfIndex;

    memcpy(pDhcpV6Adapt->wcAdapterGuid, pDhcpV6AdaptCtx->wcAdapterGuid,
        sizeof(pDhcpV6Adapt->wcAdapterGuid));

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_init;

    if (pDhcpV6AdaptCtx->IsPDEnabled)
        pDhcpV6Adapt->Flags |= DHCPV6_PD_ENABLED_FL;

    memcpy(&pDhcpV6Adapt->SockAddrIn6, &pDhcpV6AdaptCtx->SockAddrIn6, 
        sizeof(SOCKADDR_IN6));
    pDhcpV6Adapt->SockAddrIn6.sin6_port = 0;
    pDhcpV6Adapt->SockAddrIn6.sin6_flowinfo = 0;
#ifndef UNDER_CE
    pDhcpV6Adapt->SockAddrIn6.sin6_scope_id = 0;
#endif

    dwError = InitDHCPV6Socket(pDhcpV6Adapt);
    if (dwError) {
        DEBUGMSG(ZONE_ERROR, 
            (TEXT("DhcpV6L: InitDHCPV6AdaptEx: InitDHCPV6Socket failed %d\r\n"),
            dwError));
        //ASSERT(0);
        BAIL_ON_WIN32_ERROR(dwError);
    }

    UsePD = gbDHCPV6PDEnabled && pDhcpV6AdaptCtx->IsPDEnabled;
    dwError = InitDhcpV6OptionMgr(&pDhcpV6Adapt->DhcpV6OptionModule, gbDHCPV6PDEnabled);
    BAIL_ON_WIN32_ERROR(dwError);

    dbRand = DhcpV6UniformRandom();

    if (UsePD) {
        SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
            DHCPV6_SOL_MAX_RT, 0);
    } else {
        SetInitialTimeout(pDhcpV6Adapt, DHCPV6_INF_TIMEOUT, 
            DHCPV6_INF_MAX_RT, 0);
    }

    InitializeListHead(&pDhcpV6Adapt->Link);
    InsertTailList(&AdapterList, &pDhcpV6Adapt->Link);

    if (ppDhcpV6Adapt) {
        *ppDhcpV6Adapt = pDhcpV6Adapt;
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Adapter on interface: %d", dwIPv6IfIndex));

    return dwError;

error:

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Initializing Adapter on interface: %d with Error: %!status!", dwIPv6IfIndex, dwError));

    if (pDhcpV6Adapt) {
        FreeDHCPV6Mem(pDhcpV6Adapt);
        pDhcpV6Adapt = NULL;
    }

    return dwError;
}
#else
DWORD
InitDHCPV6Adapt(
    DWORD dwIPv6IfIndex,
    PWCHAR pwcAdapterGuid,
    PSOCKADDR_IN6 pSockAddrIn6,
    PDHCPV6_ADAPT *ppDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uLength = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = NULL;
    DOUBLE dbRand = 0;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Initializing Adapter on interface: %!IPV6ADDR! [%d]",
                    (CONST UCHAR*) &pSockAddrIn6->sin6_addr, dwIPv6IfIndex));

    uLength = sizeof(DHCPV6_ADAPT);

    pDhcpV6Adapt = AllocDHCPV6Mem(uLength);
    if (!pDhcpV6Adapt) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pDhcpV6Adapt, 0, uLength);

    dwError = InitializeRWLock(&pDhcpV6Adapt->RWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    pDhcpV6Adapt->uRefCount = 1;

    pDhcpV6Adapt->dwIPv6IfIndex = dwIPv6IfIndex;

    memcpy(pDhcpV6Adapt->wcAdapterGuid, pwcAdapterGuid, sizeof(pDhcpV6Adapt->wcAdapterGuid));

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_init;

    pDhcpV6Adapt->Socket = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
    if (pDhcpV6Adapt->Socket == INVALID_SOCKET) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("Create Socket on interface: %d failed with Error: %!status!", dwIPv6IfIndex, dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    memcpy(&pDhcpV6Adapt->SockAddrIn6, pSockAddrIn6, sizeof(SOCKADDR_IN6));
    pDhcpV6Adapt->SockAddrIn6.sin6_port = 0;
    pDhcpV6Adapt->SockAddrIn6.sin6_flowinfo = 0;
#ifndef UNDER_CE
    pDhcpV6Adapt->SockAddrIn6.sin6_scope_id = 0;
#endif
    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_INFO,
                        ("Attempting to bind on interface: %d with Parameters - Addr: %!IPV6ADDR!",
                        dwIPv6IfIndex, (CONST UCHAR*) &pDhcpV6Adapt->SockAddrIn6.sin6_addr));
    dwError = bind(pDhcpV6Adapt->Socket, (PSOCKADDR)&pDhcpV6Adapt->SockAddrIn6, sizeof(pDhcpV6Adapt->SockAddrIn6));
    if(dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("Bind on interface: %d failed with Error: %!status!", dwIPv6IfIndex, dwError));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    dwError = InitDhcpV6OptionMgr(&pDhcpV6Adapt->DhcpV6OptionModule);
    BAIL_ON_WIN32_ERROR(dwError);

    dbRand = DhcpV6UniformRandom();

    if (gbDHCPV6PDEnabled) {
        SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
            DHCPV6_SOL_MAX_RT, 0);
    } else {
        SetInitialTimeout(pDhcpV6Adapt, DHCPV6_INF_TIMEOUT, 
            DHCPV6_INF_MAX_RT, 0);
    }

    InitializeListHead(&pDhcpV6Adapt->Link);
    InsertTailList(&AdapterList, &pDhcpV6Adapt->Link);

    if (ppDhcpV6Adapt) {
        *ppDhcpV6Adapt = pDhcpV6Adapt;
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Adapter on interface: %d", dwIPv6IfIndex));

    return dwError;

error:

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Initializing Adapter on interface: %d with Error: %!status!", dwIPv6IfIndex, dwError));

    if (pDhcpV6Adapt) {
        FreeDHCPV6Mem(pDhcpV6Adapt);
        pDhcpV6Adapt = NULL;
    }

    return dwError;
}
#endif

DWORD
IniDeInitDHCPV6Adapt(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("DeInitializing Adapter on Index: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);

    DhcpV6TimerDelete(gpDhcpV6TimerModule, pDhcpV6Adapt);

    DeleteRegistrySettings(pDhcpV6Adapt, DEL_REG_ALL);

    if (pDhcpV6Adapt->pIpv6DNSServers != NULL) {
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Releasing Previous %d DNS Entries Received", pDhcpV6Adapt->uNumOfDNSServers));

        pDhcpV6Adapt->uNumOfDNSServers = 0;
        FreeDHCPV6Mem(pDhcpV6Adapt->pIpv6DNSServers);
        pDhcpV6Adapt->pIpv6DNSServers = NULL;
    }
#ifdef UNDER_CE
    if (pDhcpV6Adapt->pPdOption)
        FreeDHCPV6Mem(pDhcpV6Adapt->pPdOption);
    if (pDhcpV6Adapt->pDomainList)
        FreeDHCPV6Mem(pDhcpV6Adapt->pDomainList);
    if (pDhcpV6Adapt->pServerID)
        FreeDHCPV6Mem(pDhcpV6Adapt->pServerID);
#endif

    DeInitDhcpV6OptionMgr(&pDhcpV6Adapt->DhcpV6OptionModule);

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

    DestroyRWLock(&pDhcpV6Adapt->RWLock);

    FreeDHCPV6Mem(pDhcpV6Adapt);

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End DeInitializing Adapter"));

    return dwError;
}


DWORD
DereferenceDHCPV6Adapt(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uRefCount = 0;

    uRefCount = DHCPV6_DECREMENT(pDhcpV6Adapt->uRefCount);
    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("DeReferencing Adapter on Index: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, uRefCount));

    if (uRefCount == 0) {
        AcquireExclusiveLock(gpAdapterPendingDeleteRWLock);
        RemoveEntryList(&pDhcpV6Adapt->Link);
        ReleaseExclusiveLock(gpAdapterPendingDeleteRWLock);

        IniDeInitDHCPV6Adapt(pDhcpV6Adapt);
    }

    return dwError;
}


DWORD __inline
IniRemoveDHCPV6AdaptFromTable(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;


    RemoveEntryList(&pDhcpV6Adapt->Link);

    AcquireExclusiveLock(gpAdapterPendingDeleteRWLock);

    InsertTailList(&AdapterPendingDeleteList, &pDhcpV6Adapt->Link);

    ReleaseExclusiveLock(gpAdapterPendingDeleteRWLock);

    return dwError;
}


DWORD
IniDeleteDHCPV6Adapt(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    PLIST_ENTRY     pHead = &AdapterList;
    PLIST_ENTRY     pEntry = pHead->Flink;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Deleting Adapter: %p", pDhcpV6Adapt));

    AcquireExclusiveLock(gpAdapterRWLock);

    while(pEntry != pHead) {
        PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

        pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);

        if(pTempDhcpV6Adapt == pDhcpV6Adapt) {
            break;
        }

        pEntry = pEntry->Flink;
    }

    if (pEntry == pHead) {
        ReleaseExclusiveLock(gpAdapterRWLock);
        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Deleting Adapter: %p Not Found in Adapter List - May be deleted already", pDhcpV6Adapt));
        return dwError;
    }

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);

    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        BAIL_ON_LOCK_SUCCESS(dwError);
    }

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Deleting Adapter: %p at Index: %d", pDhcpV6Adapt, pDhcpV6Adapt->dwIPv6IfIndex));

    IniRemoveDHCPV6AdaptFromTable(pDhcpV6Adapt);

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_deinit;

    DhcpV6TimerCancel(gpDhcpV6TimerModule, pDhcpV6Adapt);
    dwError = closesocket(pDhcpV6Adapt->Socket);
    if(dwError == SOCKET_ERROR) {
        dwError = WSAGetLastError();
        ASSERT(0);
    }

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    ReleaseExclusiveLock(gpAdapterRWLock);

    //
    // de-ref for the creation ref-counter
    //
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    return dwError;

lock_success:

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    ReleaseExclusiveLock(gpAdapterRWLock);

    return dwError;
}


DWORD
ShutdownAllDHCPV6Adapt(
    )
{
    DWORD dwError = 0;
    PLIST_ENTRY     pHead = &AdapterList;
    PLIST_ENTRY     pEntry = pHead->Flink;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Shutting Down ALL Adapters"));

    AcquireExclusiveLock(gpAdapterRWLock);
    while(pEntry != pHead) {
        PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

        pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
        pEntry = pEntry->Flink;

        ReleaseExclusiveLock(gpAdapterRWLock);
        dwError = IniDeleteDHCPV6Adapt(pTempDhcpV6Adapt);
        AcquireExclusiveLock(gpAdapterRWLock);
    }
    ReleaseExclusiveLock(gpAdapterRWLock);

    return dwError;
}


DWORD
ReferenceDHCPV6Adapt(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uRefCount = 0;


    uRefCount = DHCPV6_INCREMENT(pDhcpV6Adapt->uRefCount);

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Referencing Adapter on Index: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, uRefCount));

    return dwError;
}


//
// To Reference an Adapter, the ppDhcpV6Adapt parameter must be specified
//
DWORD
DHCPV6AdaptFindAndReference(
    DWORD dwIPv6IfIndex,
    OUT PDHCPV6_ADAPT *ppDhcpV6Adapt
    )
{
    DWORD dwError = STATUS_NOT_FOUND;
    PLIST_ENTRY     pHead = &AdapterList;
    PLIST_ENTRY     pEntry = pHead->Flink;


    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("Finding Adapter on Index: %d", dwIPv6IfIndex));

    AcquireExclusiveLock(gpAdapterRWLock);
    while(pEntry != pHead) {
        PDHCPV6_ADAPT pTempDhcpV6Adapt = NULL;

        pTempDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
        pEntry = pEntry->Flink;

        if (pTempDhcpV6Adapt->dwIPv6IfIndex == dwIPv6IfIndex) {
            dwError = ERROR_SUCCESS;
            if (ppDhcpV6Adapt) {
                ReferenceDHCPV6Adapt(pTempDhcpV6Adapt);
                *ppDhcpV6Adapt = pTempDhcpV6Adapt;
            }
            break;
        }
    }
    ReleaseExclusiveLock(gpAdapterRWLock);

    DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, ("End Finding Adapter on Index: %d with Error: %!status!", dwIPv6IfIndex, dwError));

    return dwError;
}

