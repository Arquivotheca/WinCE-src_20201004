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
/*****************************************************************************/
/**								Microsoft Windows							**/
/*****************************************************************************/

/*
	init.c

  DESCRIPTION:
	initialization routines for DhcpV6Lite

*/

#include "dhcpv6p.h"


DWORD
DHCPv6AllocateBuffer(
    DWORD dwByteCount,
    LPVOID * ppvBuffer
    )
{
    DWORD dwError = 0;

    if (ppvBuffer == NULL) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_WIN32_ERROR(dwError);
    }

    *ppvBuffer = NULL;
    *ppvBuffer = RemoteLocalAlloc(LPTR, dwByteCount);

    if (*ppvBuffer == NULL) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }

error:

    return (dwError);
}


VOID
DHCPv6FreeBuffer(
    LPVOID pvBuffer
    )
{
    if (pvBuffer) {
        RemoteLocalFree(pvBuffer);
    }
}

HANDLE  ghAddressChange;

DWORD HandleAddressChanges(SOCKET hAddrChangeSock) {
{
    DWORD                   dwError = 0;
    DWORD                   cError = 0;
    PIP_ADAPTER_ADDRESSES   pIPAdapterAddresses = NULL;
    PIP_ADAPTER_ADDRESSES   pCurIPAddr;
    PIP_ADAPTER_ADDRESSES   pNewIPAddrs, *ppNewIPAddrs;
    ULONG                   uIPAdapterAddressesSize;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    BOOL                    bFoundAdapter;
    WCHAR                   wszAdapterName[DHCPV6_MAX_NAME_SIZE];
    
    while (1) {

        //WaitForSingleObject(ghAddressChange, INFINITE);

        dwError = WSAIoctl(hAddrChangeSock, SIO_ADDRESS_LIST_CHANGE, 
                    NULL, 0, NULL, 0, NULL, NULL, NULL);

        if (SOCKET_ERROR == dwError) {
            dwError = WSAGetLastError();

            DEBUGMSG(ZONE_WARN | ZONE_MEDIA_SENSE, 
                (TEXT("DhcpV6: WsaIoctl returned error %d (%d)\r\n"),
                dwError, cError + 1));
            
            if (cError++ > 10) {
                goto ExitError;
            }
            Sleep(1000);
            continue;
        } else {
            DEBUGMSG(ZONE_MEDIA_SENSE, 
                (TEXT("DhcpV6: WsaIoctl returned %d\r\n"), dwError));
        }
        cError = 0;

        DhcpV6Trace(DHCPV6_ADAPT, DHCPV6_LOG_LEVEL_TRACE, 
            ("Begin Initializing Adapters at startup with Error: %!status!", dwError));

        uIPAdapterAddressesSize = 3000;     // sizeof(IP_ADAPTER_ADDRESSES);
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

        pNewIPAddrs = pIPAdapterAddresses;

        pHead = &AdapterList;
        pEntry = pHead->Flink;
        
        AcquireExclusiveLock(gpAdapterRWLock);
        while(pEntry != pHead) {
            PDHCPV6_ADAPT pDhcpV6Adapt = NULL;

            pDhcpV6Adapt = CONTAINING_RECORD(pEntry, DHCPV6_ADAPT, Link);
            
            pEntry = pEntry->Flink;

            bFoundAdapter = FALSE;
            ppNewIPAddrs = &pNewIPAddrs;

            while (pCurIPAddr = *ppNewIPAddrs) {
                size_t  cConvert;
                
                cConvert = mbstowcs(wszAdapterName, pCurIPAddr->AdapterName,
                    DHCPV6_MAX_NAME_SIZE);
                if (DHCPV6_MAX_NAME_SIZE == cConvert)
                    wszAdapterName[DHCPV6_MAX_NAME_SIZE - 1] = TEXT('\0');

                if((-1 != cConvert) && (cConvert) && 
                    (0 == _wcsnicmp(pDhcpV6Adapt->wszAdapterName, 
                    wszAdapterName, DHCPV6_MAX_NAME_SIZE))) {

                    // we found adapter!
                    bFoundAdapter = TRUE;
                    *ppNewIPAddrs = pCurIPAddr->Next;

                    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);

                    if (IfOperStatusUp != pCurIPAddr->OperStatus){
                        if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
                            // nothing to do

                        } else {
                            pDhcpV6Adapt->Flags |= DHCPV6_MEDIA_DISC_FL;
                        }
                    } else {
                        if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
                            PIP_ADAPTER_UNICAST_ADDRESS pUcastAddr;
                            PSOCKET_ADDRESS pSockAddr = NULL;
                            PSOCKADDR_IN6 pSockAddrIn6 = NULL;

                            pUcastAddr = pCurIPAddr->FirstUnicastAddress;

                            while (pUcastAddr) {                         
                                pSockAddr = &pUcastAddr->Address;
                                pSockAddrIn6 = (PSOCKADDR_IN6)pSockAddr->lpSockaddr;
                                if (IN6_IS_ADDR_LINKLOCAL(&pSockAddrIn6->sin6_addr))
                                    break;
                                pUcastAddr = pUcastAddr->Next;
                            }
                            if (! pUcastAddr) {
                                DEBUGMSG(ZONE_WARN, 
                                    (TEXT("!DhcpV6: No Linklocal addres on this adapter\r\n")));
                                goto MediaConnExit;
                            }
                                
                            ASSERT(pSockAddrIn6->sin6_family == AF_INET6);
                            if (pSockAddr->iSockaddrLength != sizeof(SOCKADDR_IN6)) {
                                ASSERT(0);
                                goto MediaConnExit;
                            }
                            memcpy(&pDhcpV6Adapt->SockAddrIn6, pSockAddrIn6, 
                                sizeof(SOCKADDR_IN6));
                            pDhcpV6Adapt->SockAddrIn6.sin6_port = 0;
                            pDhcpV6Adapt->SockAddrIn6.sin6_flowinfo = 0;
                            
                            dwError = InitDHCPV6Socket(pDhcpV6Adapt);
                            if (dwError) {
                                DEBUGMSG(ZONE_ERROR, 
                                    (TEXT("DhcpV6L: HandleAddressChanges: InitDHCPV6Socket failed %d\r\n"),
                                    dwError));

                                //ASSERT(0);
                                goto MediaConnExit;
                            }

                            // reference adapter
                            ReferenceDHCPV6Adapt(pDhcpV6Adapt);
                            pDhcpV6Adapt->Flags &= ~DHCPV6_MEDIA_DISC_FL;

                            if ((DHCPV6_PD_ENABLED_FL & pDhcpV6Adapt->Flags) && 
                                gbDHCPV6PDEnabled) {

                                if (pDhcpV6Adapt->pPdOption) {

                                    // set timeouts for confirm message
                                    SetInitialTimeout(pDhcpV6Adapt, DHCPV6_CNF_TIMEOUT,
                                        DHCPV6_CNF_MAX_RT, 0);

                                    // goto rebind state.
                                    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_rebindconfirm;
                                
                                    // we use tickcounts here instead of the filetime
                                    // a little less hassle
                                    pDhcpV6Adapt->StartRebindConfirm = GetTickCount();

                                    dwError = DhcpV6TimerCancel(
                                        gpDhcpV6TimerModule, pDhcpV6Adapt);
                                    ASSERT(! dwError);

                                    // set timer/event to do work
                                    dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                                        pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrTCallback,
                                        pDhcpV6Adapt, NULL);
                                    
                                    ASSERT(! dwError);
                                } else {
                                    // go to solicit
                                    SetInitialTimeout(pDhcpV6Adapt, 
                                        DHCPV6_SOL_TIMEOUT, DHCPV6_SOL_MAX_RT, 0);

                                    dwError = DhcpV6TimerCancel(
                                        gpDhcpV6TimerModule, pDhcpV6Adapt);
                                    ASSERT(! dwError);

                                    dwError = DhcpV6EventAddEvent(
                                        gpDhcpV6EventModule,
                                        pDhcpV6Adapt->dwIPv6IfIndex, 
                                        DHCPV6MessageMgrSolicitCallback,
                                        pDhcpV6Adapt, NULL);

                                    ASSERT(! dwError);
                                }

                            } else {

                                // set timeouts for information request msg
                                SetInitialTimeout(pDhcpV6Adapt, DHCPV6_INF_TIMEOUT,
                                    DHCPV6_INF_MAX_RT, 0);

                                pDhcpV6Adapt->DhcpV6State = dhcpv6_state_init;

                                dwError = DhcpV6EventAddEvent(
                                    gpDhcpV6EventModule, 
                                    pDhcpV6Adapt->dwIPv6IfIndex,
                                    DHCPV6MessageMgrInfoReqCallback, 
                                    pDhcpV6Adapt, NULL);

                                ASSERT(! dwError);

                            }
MediaConnExit:
                            ;
                        } else {
                            // nothing to do we were up and we're still up!
                        }
                    }

                    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
                    break;
                } else {
                    ppNewIPAddrs = &(pCurIPAddr->Next);
                }

            }


            if (! bFoundAdapter) {
                AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
                // adapter has been released
                if (dhcpv6_state_deinit != pDhcpV6Adapt->DhcpV6State) {
                    // get rid of the adapter!
                    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_deinit;
                    IniRemoveDHCPV6AdaptFromTable(pDhcpV6Adapt);

                    DhcpV6TimerCancel(gpDhcpV6TimerModule, pDhcpV6Adapt);
                    dwError = closesocket(pDhcpV6Adapt->Socket);
                    if(dwError == SOCKET_ERROR) {
                        dwError = WSAGetLastError();
                        ASSERT(0);
                    }
                    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

                    // de-ref for the creation ref-counter
                    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

                } else {
                    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
                }
            }
            
        }
        ReleaseExclusiveLock(gpAdapterRWLock);

        // now go thru list of adapters which were not already there
        for ( ; pNewIPAddrs ; pNewIPAddrs = pNewIPAddrs->Next) {

            dwError = IniInitDHCPV6Adapt(pNewIPAddrs);
            if (dwError) {
                DEBUGMSG(ZONE_WARN, 
                    (TEXT("DhcpV6: IniInitDHCPV6AdaptWithName failed\r\n")));
            }
        }

    error:

        if (pIPAdapterAddresses) {
            FreeDHCPV6Mem(pIPAdapterAddresses);
        }


    }

ExitError:
    DEBUGMSG(ZONE_ERROR, 
        (TEXT("DhcpV6: HandleAddressChange: exiting with error #%d\r\n"),
        dwError));
    
    closesocket(hAddrChangeSock);
    return dwError;
    
}


}   // HandleAddressChanges()


DWORD DhcpV6InitTdiNotifications() {
    SOCKET      hAddrChangeSock;
    DWORD       dwError = 0;
    DWORD       ThreadId;
        

    hAddrChangeSock = socket(AF_INET6, SOCK_STREAM, 0);

    if (INVALID_SOCKET == hAddrChangeSock) {
        dwError = WSAGetLastError();
        goto error;
    }

    ghAddrChangeThread = CreateThread(NULL, 0, 
        (LPTHREAD_START_ROUTINE)HandleAddressChanges, (void *)hAddrChangeSock,
        CREATE_SUSPENDED, &ThreadId);
    
    if (! ghAddrChangeThread) {
        dwError = ERROR_NOT_ENOUGH_MEMORY;
        closesocket(hAddrChangeSock);
    }

error:

    return dwError;

}


