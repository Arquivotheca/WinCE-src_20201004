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

//
//  PPP Server implementation module
//


//  Include Files
#include <winsock2.h>
#include <ws2ipdef.h>

#include "windows.h"
#include "cclib.h"
#include "types.h"
#include "netui.h"

#include "cxport.h"
#include "crypt.h"
#include "memory.h"
#include "wincrypt.h"

#include "ntlmssp.h"
#include "ntlmi.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"


//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "auth.h"
#include "lcp.h"
#include "ipcp.h"
#include "ncp.h"
#include "mac.h"
#include "raserror.h"

#include "util.h"
#include "ip_intf.h"
#include "pppserver.h"

#include "iphlpapi.h"
#include "dhcp.h"

#include <fipsrandom.h>

DWORD   g_dwTotalLineCount = 0;

#define PPP_SERVER_DEFAULT_STARTUP_DELAY_SECONDS 15

#define PPP_REGKEY_PARMS    TEXT("Comm\\ppp\\Server\\Parms")
#define PPP_REGKEY_USER     TEXT("Comm\\ppp\\Server\\User")
#define PPP_REGKEY_LINE     TEXT("Comm\\ppp\\Server\\Line")
#define PPP_REGKEY_LINE_LEN 21

#define RAS_USERS_GROUP_NAME L"RasUsers"
#define MAXDFLT 5

PPPServerLineConfiguration_t *
PPPServerFindLineWithSession(
    pppSession_t *pSession);

DWORD
PPPServerLineDisable(
    IN  PPPServerLineConfiguration_t *pLine);

DWORD
PPPServerLineEnable(
    IN  PPPServerLineConfiguration_t *pLine);

typedef DWORD (*pfnGetNetworkParams) (PFIXED_INFO pFixedInfo, PULONG pOutBufLen);

typedef DWORD (*pfnGetAdaptersInfo) (PIP_ADAPTER_INFO pAdapterInfo, PULONG pOutBufLen);

typedef DWORD (*pfnGetIpAddrTable)(
  PMIB_IPADDRTABLE pIpAddrTable,  // buffer for mapping table 
  PULONG pdwSize,                 // size of buffer 
  BOOL bOrder                     // sort the table 
);

typedef DWORD (*pfnGetIpForwardTable)(
  PMIB_IPFORWARDTABLE pIpForwardTable,  // buffer for mapping table 
  PULONG pdwSize,                 // size of buffer 
  BOOL bOrder                     // sort the table 
);

typedef DWORD (*pfnGetIfEntry)(
  PMIB_IFROW pRow);


typedef DWORD (*pfnSendARP)(
  IPAddr DestIP,     // destination IP address
  IPAddr SrcIP,      // IP address of sender (optional)
  PULONG pMacAddr,   // returned physical address
  PULONG PhyAddrLen  // length of returned physical addr.
);

BOOL (*g_pfnCESetDHCPNTEByName)(PWSTR);

typedef DWORD (* PFN_GETIFTABLE)(
    PMIB_IFTABLE pIfTable,
    PULONG       pdwSize,
    BOOL         bOrder
    );

typedef DWORD (* PFN_GETADAPTERSINFO)(
    PIP_ADAPTER_INFO pAdapterInfo,
    PULONG pOutBufLen
    );

typedef DWORD (* PFN_CREATEPROXYARPENTRY)(
    DWORD   dwIpAddressToProxy,
    DWORD   dwSubnetMask,
    DWORD   dwIfIndex
    );

typedef DWORD (* PFN_DELETEPROXYARPENTRY)(
    DWORD   dwIpAddressToProxy,
    DWORD   dwSubnetMask,
    DWORD   dwIfIndex
    );

typedef DWORD (* PFN_GETADAPTERINDEX)(
    LPWSTR       AdapterName,
    PULONG       IfIndex
    );

typedef DWORD (* PFN_CREATEIPFORWARDENTRY)(
    PMIB_IPFORWARDROW   pRoute
    );

typedef DWORD (* PFN_DELETEIPFORWARDENTRY)(
    PMIB_IPFORWARDROW   pRoute
    );

typedef DWORD (* PFN_NOTIFYADDRCHANGE)(
    );

typedef DWORD (*pfnGetInterfaceInfo)(
    PIP_INTERFACE_INFO pAdapterInfo, 
    PULONG pOutBufLen
    );

typedef DWORD (*pfnGetIpInterfaceEntry)(
    PMIB_IPINTERFACE_ROW    row
    );

typedef DWORD (*pfnSetIpInterfaceEntry)(
    PMIB_IPINTERFACE_ROW    row 
    );

typedef DWORD (*pfnInitializeIpInterfaceEntry)(
    PMIB_IPINTERFACE_ROW    row 
    );

HINSTANCE  g_hTcpIpMod = NULL;
HINSTANCE  g_hIpHlpApiMod = NULL;

PFN_NOTIFYADDRCHANGE     g_pfnNotifyAddrChange;
PFN_CREATEIPFORWARDENTRY g_pfnCreateIpForwardEntry;
PFN_CREATEPROXYARPENTRY  g_pfnCreateProxyArpEntry;
PFN_DELETEPROXYARPENTRY  g_pfnDeleteProxyArpEntry;
PFN_DELETEIPFORWARDENTRY g_pfnDeleteIpForwardEntry;
pfnGetAdaptersInfo       g_pfnGetAdaptersInfo;
PFN_GETADAPTERINDEX      g_pfnGetAdapterIndex;
pfnGetIfEntry            g_pfnGetIfEntry;
pfnGetIpAddrTable        g_pfnGetIpAddrTable;
pfnGetIpForwardTable     g_pfnGetIpForwardTable;
pfnGetNetworkParams      g_pfnGetNetworkParams;
pfnSendARP               g_pfnSendARP;
pfnGetInterfaceInfo      g_pfnGetInterfaceInfo;
pfnGetIpInterfaceEntry   g_pfnGetIpInterfaceEntry;
pfnSetIpInterfaceEntry   g_pfnSetIpInterfaceEntry;
pfnInitializeIpInterfaceEntry   g_pfnInitializeIpInterfaceEntry;

DWORD
CountListEntries(
    PLIST_ENTRY pListHead)
{
    PLIST_ENTRY pEntry;
    DWORD       nEntries = 0;

    for (pEntry = pListHead->Flink; pEntry != pListHead; pEntry = pEntry->Flink)
        nEntries++;

    return nEntries;
}


////////////////////////// Proxy ARP Support Module //////////////////////////
//
//  The IP Helper support for ARP Proxying is pretty primitive. It just allows
//  ARP proxying to be enabled one interface at a time. One particular limitation
//  is that it does not track the coming and going of interfaces.
//
//  This Proxy ARP Support Module keeps track of all the addresses being proxied,
//  and ensures that each one gets proxied on all applicable interfaces that are
//  available at any moment in time. When interfaces are added, ARP proxies will
//  be set up for them if appropriate. When interfaces are deleted, proxies will
//  be disabled from those interfaces.
//

LIST_ENTRY      g_ArpProxyList;

typedef struct
{
    LIST_ENTRY  link;
    DWORD       dwIpAddress; // Ip Address being proxied
    LIST_ENTRY  IfList;      // List of IfListEntry - interfaces this address proxied on

} ArpProxyListEntry, *PArpProxyListEntry;

typedef struct
{
    LIST_ENTRY  link;
    DWORD       dwIfIndex;
} IfListEntry, *PIfListEntry;

#define ARP_PROXY_DO_VALIDATION 1
#ifdef  ARP_PROXY_DO_VALIDATION

static void
ListValidate(
    IN PLIST_ENTRY List
    )
{
    ULONG Nodes = 0;
    PLIST_ENTRY Prev, Curr;

    Prev = List;
    Curr = List->Flink;
    while (TRUE)
    {
        if (Curr == NULL)
        {
            RETAILMSG(1, (L"PPP Server: Malformed list %x node #%u is NULL\n", List, Nodes));
            DebugBreak();
            break;
        }

        if (Curr->Blink != Prev)
        {
            RETAILMSG(1, (L"PPP Server: Malformed list %x at node %x #%u invalid Blink\n", List, Curr, Nodes));
            DebugBreak();
            break;
        }

        if (Curr == List)
        {
            // reached the end
            break;
        }

        Nodes++;

        if (Nodes > 10000)
        {
            RETAILMSG(1, (L"PPP Server: Malformed list %x has too many nodes\n", List));
            DebugBreak();
            break;
        }

        Prev = Curr;
        Curr = Curr->Flink;
    }
}

static void
APInsertTailList(
    IN PLIST_ENTRY pList,
    IN PLIST_ENTRY pNode)
{
    ListValidate(pList);
    InsertTailList(pList, pNode);
    ListValidate(pList);
}

static void
APRemoveEntryList(
    IN PLIST_ENTRY pList,
    IN PLIST_ENTRY pNode)
{
    ListValidate(pList);
    RemoveEntryList(pNode);
    ListValidate(pList);
}

static void
APValidateAllLists()
{
    PArpProxyListEntry pEntry;

    ListValidate(&g_ArpProxyList);

    for (pEntry =  (PArpProxyListEntry)(g_ArpProxyList.Flink);
         pEntry != (PArpProxyListEntry)(&g_ArpProxyList);
         pEntry =  (PArpProxyListEntry)pEntry->link.Flink)
    {
        ListValidate(&pEntry->IfList);
    }
}

#else

#define APInsertTailList(pList,pNode)  InsertTailList((pList),(pNode))
#define APRemoveEntryList(pList,pNode) RemoveEntryList((pNode))
#define APValidateAllLists()  (void)0         
#define ListValidate(pList)  (void)0         

#endif

BOOLEAN
PPPServerEnableForward(
    IN int index,
    IN BOOLEAN Enable
    )
{
    DWORD result;
    MIB_IPINTERFACE_ROW InterFace;

    memset((void*)&InterFace, 0, sizeof(MIB_IPINTERFACE_ROW));
    g_pfnInitializeIpInterfaceEntry(&InterFace);
    InterFace.InterfaceIndex = index;
    InterFace.Family = AF_INET;

    result = g_pfnGetIpInterfaceEntry(&InterFace);
    if(result)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("GetIpInterfaceEntry failed with %d\n"), result));
        return FALSE;
    }

    if (InterFace.ForwardingEnabled == Enable)
    {
        DEBUGMSG(ZONE_TRACE, (TEXT("success: return directly\n")));
        return TRUE;
    }
    else
    {
        DEBUGMSG(ZONE_TRACE, (TEXT("try to set ForwardingEnabled\n")));
        memset((void*)&InterFace, 0, sizeof(MIB_IPINTERFACE_ROW));
        g_pfnInitializeIpInterfaceEntry(&InterFace);
        InterFace.InterfaceIndex = index;
        InterFace.Family = AF_INET;
        InterFace.ForwardingEnabled = Enable;

        result = g_pfnSetIpInterfaceEntry(&InterFace);
        if(result)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("SetIpInterfaceEntry failed with %d\n"), result));
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN 
PPPServerUpdateInterfaceForward(
    IN pppSession_t *pSession,
    IN BOOLEAN Enable
    )
{
    // Declare and initialize variables
    PIP_INTERFACE_INFO pInfo = NULL;
    ULONG ulOutBufLen = 0;
    DWORD dwRetVal = 0;
    int i = 0;
    int cmpResult=0;
    PPPServerLineConfiguration_t *pLine;

    pLine = PPPServerFindLineWithSession(pSession); 
    
    if (!(g_pfnSetIpInterfaceEntry && g_pfnGetIpInterfaceEntry && g_pfnInitializeIpInterfaceEntry && g_pfnGetInterfaceInfo))
    {
        DEBUGMSG(ZONE_ERROR, 
                (TEXT("failed to find iphlpapi function pointers:\n")));
        DEBUGMSG(ZONE_TRACE, 
                (TEXT("g_pfnSetIpInterfaceEntry = 0x%08x, g_pfnGetIpInterfaceEntry = 0x%08x, g_pfnInitializeIpInterfaceEntry = 0x%08x, g_pfnGetInterfaceInfo = 0x%08x\n"),
                g_pfnSetIpInterfaceEntry,
                g_pfnGetIpInterfaceEntry,
                g_pfnInitializeIpInterfaceEntry,
                g_pfnGetInterfaceInfo
                ));
        
        return FALSE;
    }
    // Make an initial call to GetInterfaceInfo to get
    // the necessary size in the ulOutBufLen variable
    dwRetVal = g_pfnGetInterfaceInfo(NULL, &ulOutBufLen);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pInfo = (IP_INTERFACE_INFO *) malloc(ulOutBufLen);
        if (pInfo == NULL) {
            DEBUGMSG(ZONE_ERROR, 
                (TEXT("Unable to allocate memory needed to call GetInterfaceInfo\n")));
            return FALSE;
        }
    }
    // Make a second call to GetInterfaceInfo to get
    // the actual data we need
    dwRetVal = g_pfnGetInterfaceInfo(pInfo, &ulOutBufLen);
    if (dwRetVal == NO_ERROR) {
        DEBUGMSG(ZONE_TRACE, (TEXT("Number of Adapters: %ld\n\n"), pInfo->NumAdapters));
        for (i = 0; i < (int) pInfo->NumAdapters; i++) 
        {
            
            DEBUGMSG(ZONE_TRACE, (TEXT("Adapter Index[%d]: %ld\n"), i,
                   pInfo->Adapter[i].Index));
            DEBUGMSG(ZONE_TRACE, (TEXT("Adapter Name[%d]: %s\n\n"), i,
                   pInfo->Adapter[i].Name));
            DEBUGMSG(ZONE_TRACE, (TEXT("pSession->AdapterName: %s\n\n"),
                   pSession->AdapterName));
            cmpResult = _wcsicmp(pSession->AdapterName, pInfo->Adapter[i].Name);
            DEBUGMSG(ZONE_TRACE, (TEXT("cmpResult=%d\n\n"),
                   cmpResult));
            
            if (cmpResult == 0)
            {
                pLine->dwIfIndex = pInfo->Adapter[i].Index;
                DEBUGMSG(ZONE_TRACE, (TEXT("set pLine->dwIfIndex: %u\n"),
                   pInfo->Adapter[i].Index));
            }
            
            if (!PPPServerEnableForward(pInfo->Adapter[i].Index, Enable))
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("Failed to enable forward for interface %d\n"), pInfo->Adapter[i].Index));
            }
        }
    } else if (dwRetVal == ERROR_NO_DATA) {
        DEBUGMSG(ZONE_ERROR, 
            (TEXT("There are no network adapters with IPv4 enabled on the local system\n")));
        return FALSE;
    } else {
        DEBUGMSG(ZONE_ERROR, (TEXT("GetInterfaceInfo failed with error: %d\n"), dwRetVal));
        return FALSE;
    }

    return TRUE;
}


BOOLEAN
IFTypeIsLAN(
    DWORD dwIfIndex)
//
//  Determine whether the specified interface is of the LAN type.
//
{
    BOOL        bIsLAN = TRUE;
    MIB_IFROW   row;
    DWORD       dwResult;

    if (g_pfnGetIfEntry)
    {
        row.dwIndex = dwIfIndex;
        dwResult = g_pfnGetIfEntry(&row);
        if (dwResult == NO_ERROR)
        {
            if (row.dwType == MIB_IF_TYPE_ETHERNET || row.dwType == MIB_IF_TYPE_TOKENRING)
            {
                // Probably a LAN. Special case for "Virtual Ethernet Miniport" interfaces,
                // which are not real LAN interfaces.
                if (0 == memcmp(row.bPhysAddr, " RASL", 5))
                {
                    // Is a VEM interface
                    bIsLAN = FALSE;
                }
            }
            else
            {
                // Not a LAN type
                bIsLAN = FALSE;
            }
        }
    }
    return bIsLAN;
}

DWORD
PPPServerGetIPAddrTable(
    OUT PMIB_IPADDRTABLE    *ppTable,
    OUT PDWORD              pcbTable OPTIONAL)
//
//  Call the IP helper API GetIPAddrTable to allocate
//  space for and retrieve the table.  The caller must
//  free the returned table pointer when done with it.
//
{
    DWORD               dwResult = ERROR_NOT_SUPPORTED;
    PMIB_IPADDRTABLE    pTable = NULL;
    DWORD               cbTable = 0;

    if (g_pfnGetIpAddrTable)
    {
        dwResult = g_pfnGetIpAddrTable(pTable, &cbTable, FALSE);
        pTable = LocalAlloc(LPTR, cbTable);
        if (pTable == NULL)
        {
            dwResult = ERROR_OUTOFMEMORY;
            cbTable = 0;
        }
        else
        {
            dwResult = g_pfnGetIpAddrTable(pTable, &cbTable, FALSE);
            if (dwResult != NO_ERROR)
            {
                LocalFree(pTable);
                pTable = NULL;
                cbTable = 0;
            }
        }
    }
    *ppTable = pTable;
    if (pcbTable)
        *pcbTable = cbTable;

    return dwResult;
}

DWORD
PPPServerCreateProxyArpEntry(
    DWORD   dwIpAddressToProxy,
    DWORD   dwMask,
    DWORD   dwIfIndex)
{
    DWORD   dwResult = ERROR_NOT_SUPPORTED;

    if (g_pfnCreateProxyArpEntry)
    {
        DEBUGMSG(ZONE_TRACE, (TEXT("PPP: Creating Proxy ARP entry for address %u.%u.%u.%u on IF %x\n"),
            (dwIpAddressToProxy >> 24) & 0xFF, 
            (dwIpAddressToProxy >> 16) & 0xFF,
            (dwIpAddressToProxy >>  8) & 0xFF, 
            (dwIpAddressToProxy      ) & 0xFF, 
            dwIfIndex));
        dwResult = g_pfnCreateProxyArpEntry(htonl(dwIpAddressToProxy), dwMask, dwIfIndex);
        DEBUGMSG(ZONE_ERROR && dwResult, (TEXT("PPP: ERROR %d Creating Proxy ARP entry for address %u.%u.%u.%u on IF %x\n"),
            dwResult, 
            (dwIpAddressToProxy >> 24) & 0xFF, 
            (dwIpAddressToProxy >> 16) & 0xFF,
            (dwIpAddressToProxy >>  8) & 0xFF, 
            (dwIpAddressToProxy      ) & 0xFF, 
            dwIfIndex));
    }

    return dwResult;
}

DWORD
PPPServerDeleteProxyArpEntry(
    DWORD   dwIpAddressToProxy,
    DWORD   dwMask,
    DWORD   dwIfIndex)
{
    DWORD   dwResult = ERROR_NOT_SUPPORTED;

    if (g_pfnDeleteProxyArpEntry)
    {
        DEBUGMSG(ZONE_TRACE, (TEXT("PPP: Deleting Proxy ARP entry for address %u.%u.%u.%u on IF %x\n"),
            (dwIpAddressToProxy >> 24) & 0xFF, 
            (dwIpAddressToProxy >> 16) & 0xFF,
            (dwIpAddressToProxy >>  8) & 0xFF, 
            (dwIpAddressToProxy      ) & 0xFF, 
            dwIfIndex));
        dwResult = g_pfnDeleteProxyArpEntry(htonl(dwIpAddressToProxy), dwMask, dwIfIndex);
    }

    return dwResult;
}


PArpProxyListEntry
ArpProxyListEntryNew(
    DWORD   dwIpAddressToProxy)
//
//  Create a new arp proxy list entry and add it to the global list.
//
{
    PArpProxyListEntry pEntry;

    pEntry = pppAllocateMemory(sizeof(*pEntry));
    if (pEntry)
    {
        pEntry->dwIpAddress = dwIpAddressToProxy;
        InitializeListHead(&pEntry->IfList);

        APInsertTailList(&g_ArpProxyList, &pEntry->link);

        DEBUGMSG(ZONE_TRACE, (TEXT("PPP: Start Managing ARP proxies for address %u.%u.%u.%u\n"),
            (dwIpAddressToProxy >> 24) & 0xFF, 
            (dwIpAddressToProxy >> 16) & 0xFF,
            (dwIpAddressToProxy >>  8) & 0xFF, 
            (dwIpAddressToProxy      ) & 0xFF));
    }

    return pEntry;
}

void
ArpProxyListEntryDelete(
    PArpProxyListEntry   pEntry)
{
    DEBUGMSG(ZONE_TRACE, (TEXT("PPP: Stop Managing ARP proxies for address %u.%u.%u.%u\n"),
        (pEntry->dwIpAddress >> 24) & 0xFF, 
        (pEntry->dwIpAddress >> 16) & 0xFF,
        (pEntry->dwIpAddress >>  8) & 0xFF, 
        (pEntry->dwIpAddress      ) & 0xFF));

    ASSERT(IsListEmpty(&pEntry->IfList));

    APRemoveEntryList(&g_ArpProxyList, &pEntry->link);

    pEntry->link.Flink = (PVOID)0xDEADDEAD;
    pEntry->link.Blink = (PVOID)0xDEADDEAD;
    pppFreeMemory(pEntry, sizeof(*pEntry));
}

PIfListEntry
ArpIfListEntryNew(
    IN  PArpProxyListEntry   pEntry,
    IN  DWORD                dwIfIndex)
//
//  Create a new IF entry and add it to the IF list for the particular IP address.
//  Proxy that IP address on the IF.
//
{
    PIfListEntry pIfEntry;

    pIfEntry = pppAllocateMemory(sizeof(*pIfEntry));
    if (pIfEntry)
    {
        pIfEntry->dwIfIndex = dwIfIndex;
        PPPServerCreateProxyArpEntry(pEntry->dwIpAddress, 0xFFFFFFFF, pIfEntry->dwIfIndex);
        APInsertTailList(&pEntry->IfList, &pIfEntry->link);
    }

    return pIfEntry;
}

void
ArpIfListEntryDelete(
    IN  PArpProxyListEntry   pEntry,
    IN  PIfListEntry         pIfEntry)
//
//  Remove the interface from the list of interfaces proxying the address.
//  Stop proxying the address on that interface.
//
{
    APRemoveEntryList(&pEntry->IfList, &pIfEntry->link);
    pIfEntry->link.Flink = (PVOID)0xDEADDEAD;
    pIfEntry->link.Blink = (PVOID)0xDEADDEAD;
    PPPServerDeleteProxyArpEntry(pEntry->dwIpAddress, 0xFFFFFFFF, pIfEntry->dwIfIndex);
    pppFreeMemory(pIfEntry, sizeof(*pIfEntry));
}

void
ArpProxyListEntryUnproxyAll(
    PArpProxyListEntry pEntry)
//
//  Unproxy this entry's address on all the entries for which it is currently proxied.
//
{
    while (!IsListEmpty(&pEntry->IfList))
    {
        ArpIfListEntryDelete(pEntry, (PIfListEntry)pEntry->IfList.Flink);
    }
}

PMIB_IPADDRROW
IpAddrTableFindRowByIndex(
    IN       PMIB_IPADDRTABLE   pTable,
    IN       DWORD              dwIfIndex)
{
    PMIB_IPADDRROW pRow = NULL;
    DWORD          i;

    for (i = 0; i < pTable->dwNumEntries; i++)
    {
        if (pTable->table[i].dwIndex == dwIfIndex)
        {
            pRow = &pTable->table[i];
            break;
        }
    }

    return pRow;
}

PIfListEntry
IfEntryFindByIndex(
   IN PLIST_ENTRY pIfList,
   IN DWORD       dwIfIndex)
{
    PIfListEntry        pIfEntry  = NULL;

    ListValidate(pIfList);
    for (pIfEntry = (PIfListEntry)(pIfList->Flink);
         TRUE;
         pIfEntry  = (PIfListEntry)pIfEntry->link.Flink)
    {
        if (pIfEntry == (PIfListEntry)pIfList)
        {
            // no match found
            pIfEntry = NULL;
            break;
        }

        if (pIfEntry->dwIfIndex == dwIfIndex)
            break;
    }

    return pIfEntry;
}

BOOL
ShouldBeProxied(
    IN  DWORD               dwIpAddress,
    IN  PMIB_IPADDRROW      pRow)
//
//  Return TRUE if the specified IP Address should be proxied on
//  the specified interface.
//
{
    BOOL  bShouldBeProxied = FALSE;
    DWORD dwIfNetMask,
          dwIfNetAddr;

    // Only do ARP proxying on LANs
    if (IFTypeIsLAN(pRow->dwIndex))
    {
        dwIfNetMask = ntohl(pRow->dwMask);
        dwIfNetAddr = ntohl(pRow->dwAddr);

        if (dwIfNetAddr == 0 || dwIfNetAddr == 0xFFFFFFFF || dwIfNetMask == 0)
        {
            // Invalid address or mask, do not proxy on it
        }
        else if ((dwIpAddress & dwIfNetMask) == (dwIfNetAddr & dwIfNetMask))
        {
            // If the the address is on the interfaces subnet,
            // then we should proxy it
            bShouldBeProxied = TRUE;
        }

        DEBUGMSG(ZONE_PPP, (TEXT("PPPSERVER: ShouldBeProxied %x on IF IP=%x MASK=%x = %d\n"), dwIpAddress, dwIfNetAddr, dwIfNetMask, bShouldBeProxied));
    }

    return bShouldBeProxied;
}

void
ArpProxyListEntryUpdateProxies(
    IN  OUT  PArpProxyListEntry pEntry,
    IN       PMIB_IPADDRTABLE   pTable)
//
//  For each interface that this entry should be proxied on and is not currently, proxy it.
//  For each interface that this entry is proxied on but should not, unproxy it.
//
{
    PIfListEntry        pIfEntry,
                        pIfEntryNext;
    PMIB_IPADDRROW      pRow;
    DWORD               i;

    ListValidate(&pEntry->IfList);

    //  For each interface that this entry is proxied on but should not, unproxy it.

    for (pIfEntry = (PIfListEntry)(pEntry->IfList.Flink);
         pIfEntry != (PIfListEntry)&pEntry->IfList;
         pIfEntry = pIfEntryNext)
    {
        pIfEntryNext = (PIfListEntry)pIfEntry->link.Flink;

        pRow = IpAddrTableFindRowByIndex(pTable, pIfEntry->dwIfIndex);
        if (pRow == NULL
        ||  !ShouldBeProxied(pEntry->dwIpAddress, pRow))
        {
            //
            // Interface we were proxying on no longer exists, or
            // Interface address/type has changed, don't want to proxy any more
            //
            ArpIfListEntryDelete(pEntry, pIfEntry);
        }
    }

    // For each interface, proxy this entry if it is not currently proxied but should be.

    for (i = 0; i < pTable->dwNumEntries; i++)
    {
        pRow = &pTable->table[i];

        if (ShouldBeProxied(pEntry->dwIpAddress, pRow))
        {
            pIfEntry = IfEntryFindByIndex(&pEntry->IfList, pRow->dwIndex);
            if (pIfEntry == NULL)
            {
                // Not currently proxied, Create a new IfEntry and add it to the list
                pIfEntry = ArpIfListEntryNew(pEntry, pRow->dwIndex);
            }
            else
            {
                // An interface could get deleted and re-added quickly, before we run.
                // However, it should get a new interface id in that case. So we shouldn't
                // have to worry about it
            }
        }
    }
}

PArpProxyListEntry
ArpProxyAddressFind(
    DWORD   dwIpAddressToProxy)
{
    PArpProxyListEntry pEntry = NULL;

    ListValidate(&g_ArpProxyList);

    for (pEntry = (PArpProxyListEntry)(g_ArpProxyList.Flink);
         TRUE;
         pEntry = (PArpProxyListEntry)pEntry->link.Flink)
    {
        if (pEntry == (PArpProxyListEntry)(&g_ArpProxyList))
        {
            // end of list, match not found
            pEntry = NULL;
            break;
        }

        if (pEntry->dwIpAddress == dwIpAddressToProxy)
        {
            // found match
            break;
        }
    }
    return pEntry;
}

DWORD
ArpProxyAddressAdd(
    DWORD   dwIpAddressToProxy)
//
//  Begin managing this proxy arp address.
//
{
    DWORD   dwResult = NO_ERROR;
    PArpProxyListEntry pEntry;
    PMIB_IPADDRTABLE    pTable;

    pEntry = ArpProxyAddressFind(dwIpAddressToProxy);
    if (pEntry == NULL)
        pEntry = ArpProxyListEntryNew(dwIpAddressToProxy);

    if (pEntry)
    {
        // Proxy the address on interfaces as appropriate

        dwResult = PPPServerGetIPAddrTable(&pTable, NULL);

        if (pTable)
        {
            ArpProxyListEntryUpdateProxies(pEntry, pTable);

            LocalFree(pTable);
        }
    }
    else
    {
        dwResult = ERROR_OUTOFMEMORY;
    }

    return dwResult;

}

DWORD
ArpProxyAddressDelete(
    DWORD   dwIpAddressBeingProxied)
//
//  Stop managing this proxy arp address.
//
{
    DWORD   dwResult = NO_ERROR;
    PArpProxyListEntry pEntry;

    pEntry = ArpProxyAddressFind(dwIpAddressBeingProxied);
    if (pEntry)
    {
        // Unproxy the address from any interfaces
        ArpProxyListEntryUnproxyAll(pEntry);

        ArpProxyListEntryDelete(pEntry);
    }

    return dwResult;
}

//
//  Format of the request messages sent to the ARP Proxy Manager thread.
//
typedef struct
{
    LIST_ENTRY  link;
    BOOL        bAddProxy;
    DWORD       dwIpAddress;
} ArpProxyManagerRequest, *PArpProxyManagerRequest;

LIST_ENTRY       g_ArpProxyManagerRequestList;
CRITICAL_SECTION g_CSArpProxyManagerRequestList;
HANDLE           g_hEventManagerRequestList;
HANDLE           g_hEventAddrChange;
HANDLE           g_hAddrChangeThread;
HANDLE           g_hProxyManagerThread;

DWORD
ArpProxyAddrChangeNotifierThread(
    PVOID unused)
{
    DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: Start AddrChangeNotifier\n")));

    if (g_pfnNotifyAddrChange)
    {
        while (g_pfnNotifyAddrChange(NULL, NULL) == NO_ERROR)
        {
            SetEvent(g_hEventAddrChange);
        }
    }

    DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: Exit AddrChangeNotifier\n")));

    return NO_ERROR;
}

DWORD
ArpProxyManagerThread(
    PVOID unused)
{
    DWORD dwResult;
    PArpProxyManagerRequest pRequest;
    PArpProxyListEntry      pEntry,
                            pEntryNext;
    PMIB_IPADDRTABLE        pTable;
    HANDLE                  hEventArray[2];

    DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: Start ARP Proxy Manager\n")));

    hEventArray[0] = g_hEventManagerRequestList;
    hEventArray[1] = g_hEventAddrChange;

    while (TRUE)
    {
        DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: ArpProxyManagerThread Wait\n")));

        APValidateAllLists();
        dwResult = WaitForMultipleObjects(2, &hEventArray[0], FALSE, INFINITE);
        APValidateAllLists();

        DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: ArpProxyManagerThread Wait Result = %x\n"), dwResult));

        if (dwResult == WAIT_OBJECT_0)
        {
            // g_hEventManagerRequestList signalled

            // Process all requests in the request list
            while (TRUE)
            {
                EnterCriticalSection(&g_CSArpProxyManagerRequestList);

                pRequest = NULL;
                if (!IsListEmpty(&g_ArpProxyManagerRequestList))
                    pRequest = (PArpProxyManagerRequest)RemoveHeadList(&g_ArpProxyManagerRequestList);

                LeaveCriticalSection(&g_CSArpProxyManagerRequestList);

                DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: ArpProxyManagerThread pRequest = %x\n"), pRequest));

                if (pRequest == NULL)
                    break;

                if (pRequest->bAddProxy)
                    ArpProxyAddressAdd(pRequest->dwIpAddress);
                else
                    ArpProxyAddressDelete(pRequest->dwIpAddress);

                LocalFree(pRequest);
            }
        }
        else if (dwResult == WAIT_OBJECT_0 + 1)
        {
            DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: ArpProxyManagerThread AddrChanged\n")));

            // g_hEventAddrChange signalled
            // Process any interface changes
            dwResult = PPPServerGetIPAddrTable(&pTable, NULL);

            if (pTable)
            {
                for (pEntry  = (PArpProxyListEntry)(g_ArpProxyList.Flink);
                     pEntry != (PArpProxyListEntry)(&g_ArpProxyList);
                     pEntry  =  pEntryNext)
                {
                    pEntryNext = (PArpProxyListEntry)pEntry->link.Flink;
                    ArpProxyListEntryUpdateProxies(pEntry, pTable);
                }
                LocalFree(pTable);
            }
        }
        else
        {
            // Error in WaitForMultiple
            DEBUGMSG(ZONE_ERROR, (TEXT("PPPSERVER: ArpProxyManagerThread error in WaitForMultiple %d\n"), GetLastError()));
            break;
        }
    }

    CloseHandle(g_hProxyManagerThread);
    g_hProxyManagerThread = NULL;

    DEBUGMSG(ZONE_TRACE, (TEXT("PPPSERVER: Exit ARP Proxy Manager\n")));

    return NO_ERROR;
}

DWORD
ArpProxyManagerIssueRequest(
    DWORD dwIpAddress,
    BOOL  bAddProxy)
//
//  Issue a request to the Arp Proxy manager to add or
//  remove an address form the proxy list.
//
{
    PArpProxyManagerRequest pRequest;
    DWORD                   dwResult = NO_ERROR;

    DEBUGMSG(ZONE_TRACE, (TEXT("PPP: Request %hs proxy for %u.%u.%u.%u\n"),
        bAddProxy ? "Add" : "Delete",
        (dwIpAddress >> 24) & 0xFF, 
        (dwIpAddress >> 16) & 0xFF,
        (dwIpAddress >>  8) & 0xFF, 
        (dwIpAddress      ) & 0xFF));

    pRequest = LocalAlloc(LPTR, sizeof(*pRequest));
    if (pRequest)
    {
        pRequest->dwIpAddress = dwIpAddress;
        pRequest->bAddProxy = bAddProxy;

        EnterCriticalSection(&g_CSArpProxyManagerRequestList);
        InsertTailList(&g_ArpProxyManagerRequestList, &pRequest->link);
        LeaveCriticalSection(&g_CSArpProxyManagerRequestList);

        SetEvent(g_hEventManagerRequestList);
    }
    else
    {
        dwResult = ERROR_OUTOFMEMORY;
    }

    return dwResult;
}

DWORD
ArpProxyManagerInitialize()
{
    DWORD dwResult = NO_ERROR;

    InitializeListHead(&g_ArpProxyList);
    InitializeListHead(&g_ArpProxyManagerRequestList);
    if (!g_hEventManagerRequestList)
        g_hEventManagerRequestList = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!g_hEventAddrChange)
    g_hEventAddrChange = CreateEvent(NULL, FALSE, FALSE, NULL);
    InitializeCriticalSection(&g_CSArpProxyManagerRequestList);

    if (g_hEventManagerRequestList == NULL || g_hEventAddrChange == NULL)
    {
        dwResult = ERROR_OUTOFMEMORY;
    }

    return dwResult;
}


DWORD
ArpProxyManagerStart()
{
    DWORD dwThreadId;
    DWORD dwResult = NO_ERROR;

    if (!g_hAddrChangeThread)
        g_hAddrChangeThread = CreateThread(NULL, 0, ArpProxyAddrChangeNotifierThread, NULL, 0, &dwThreadId);
    if (!g_hProxyManagerThread)
        g_hProxyManagerThread = CreateThread(NULL, 0, ArpProxyManagerThread, NULL, 0, &dwThreadId);

    if (g_hAddrChangeThread == NULL || g_hProxyManagerThread == NULL)
    {
        dwResult = ERROR_OUTOFMEMORY;
    }

    return dwResult;
}

//
// End ARP Proxy Manager
//
//////////////////////////////////////////////////////////////////////////////////////

// Global Variables

PPPServerConfiguration_t PPPServerConfig;
CRITICAL_SECTION         PPPServerCritSec;

PPPServerLineConfiguration_t *
PPPServerFindLineConfig(
    PLIST_ENTRY pLineList,
    RASDEVINFO  *pRasDevInfo)
//
//  If the list contains a config with the given name and type, return a pointer to it.
//  Otherwise, return NULL.
//
{
    PPPServerLineConfiguration_t *pLineConfig;

    for (pLineConfig = (PPPServerLineConfiguration_t *)(pLineList->Flink);
         TRUE;
         pLineConfig = (PPPServerLineConfiguration_t *)pLineConfig->listEntry.Flink)
    {
        if (pLineConfig == (PPPServerLineConfiguration_t *)pLineList)
        {
            // Reached end of the list
            pLineConfig = NULL;
            break;
        }

        // Check for a matching name and type
        if ((_tcscmp(pRasDevInfo->szDeviceName, pLineConfig->rasDevInfo.szDeviceName) == 0)
        &&  (_tcscmp(pRasDevInfo->szDeviceType, pLineConfig->rasDevInfo.szDeviceType) == 0))
            break;      
    }

    return pLineConfig;
}

PPPServerLineConfiguration_t *
PPPServerFindLineWithSession(
    pppSession_t *pSession)
{
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine, *pMatchingLine;

    pMatchingLine = NULL;
    for (pLine  = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
         pLine != (PPPServerLineConfiguration_t *)(&pConfig->LineList);
         pLine  = (PPPServerLineConfiguration_t *)pLine->listEntry.Flink)
    {
        if (pLine->pSession == pSession)
        {
            pMatchingLine = pLine;
            break;
        }
    }

    return pMatchingLine;
}

PPPServerLineConfiguration_t *
PPPServerCreateLineConfig(
    PLIST_ENTRY pLineList,
    RASDEVINFO  *pRasDevInfo)
//
//  Create a new line configuration structure, initialize it with default settings,
//  and add it to the list.
//
{
    PPPServerLineConfiguration_t *pLineConfig;

    pLineConfig = pppAllocateMemory(sizeof(*pLineConfig));
    if (!pLineConfig)
    {
        DEBUGMSG( ZONE_ERROR, (TEXT("PPP: ERROR, unable to allocate space for new line config\n")));
    }
    else
    {
        _tcscpy(pLineConfig->rasDevInfo.szDeviceName, pRasDevInfo->szDeviceName);
        _tcscpy(pLineConfig->rasDevInfo.szDeviceType, pRasDevInfo->szDeviceType);

        pLineConfig->pDevConfig = NULL;
        pLineConfig->dwDevConfigSize = 0;
        pLineConfig->bEnable = FALSE;
        pLineConfig->bmFlags = 0;
        pLineConfig->DisconnectIdleSeconds = 0;

        pLineConfig->IPAddrInfo[SERVER_INDEX].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        pLineConfig->IPAddrInfo[CLIENT_INDEX].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if (pLineConfig->IPAddrInfo[SERVER_INDEX].hEvent == NULL
        ||  pLineConfig->IPAddrInfo[CLIENT_INDEX].hEvent == NULL)
        {
            CloseHandle(pLineConfig->IPAddrInfo[SERVER_INDEX].hEvent);
            pppFreeMemory(pLineConfig, sizeof(*pLineConfig));
            pLineConfig = NULL;
        }
        else
        {
            InsertTailList(pLineList, &pLineConfig->listEntry);
        }
    }

    return pLineConfig;
}



PPPServerLineConfiguration_t *
PPPServerFindOrCreateLineConfig(
    PLIST_ENTRY pLineList,
    RASDEVINFO  *pRasDevInfo)
//
//  Find an existing line config by the given name.
//  If one does not already exist, create one and initialize with defaults.
//
{
    PPPServerLineConfiguration_t *pLineConfig;

    pLineConfig = PPPServerFindLineConfig(pLineList, pRasDevInfo);
    if (pLineConfig == NULL)
        pLineConfig = PPPServerCreateLineConfig(pLineList, pRasDevInfo);

    return pLineConfig;
}

void
PPPServerDestroyLineConfig(
    IN  OUT PLIST_ENTRY pLineList,
    IN  OUT PPPServerLineConfiguration_t *pLineConfig)
//
//  Remove a line config from the list and free it.
//
{
    DEBUGMSG( ZONE_FUNCTION, (TEXT("PPP: +PPPServerDestroyLineConfig %s\n"), pLineConfig->rasDevInfo.szDeviceName));

    RemoveEntryList(&pLineConfig->listEntry);
    pLineConfig->listEntry.Flink = NULL;
    pLineConfig->listEntry.Blink = NULL;

    CloseHandle(pLineConfig->IPAddrInfo[SERVER_INDEX].hEvent);
    CloseHandle(pLineConfig->IPAddrInfo[CLIENT_INDEX].hEvent);

    pppFreeMemory(pLineConfig, sizeof(*pLineConfig));
}

void
PPPServerRemoveLineConfigurationRegistrySettings(
    PPPServerLineConfiguration_t    *pLineConfig)
//
//  Remove a line's settings from the registry
//
//      HKLM\Comm\ppp\Server\Line\<LineName>\
//
{
    LONG    lResult;
    TCHAR   tszKeyName[MAX_PATH + 1];   

    ASSERT(PPP_REGKEY_LINE_LEN + 1 + RAS_MaxDeviceName + 1 < MAX_PATH);
    _stprintf(tszKeyName, TEXT("%s\\%s"), PPP_REGKEY_LINE, pLineConfig->rasDevInfo.szDeviceName);

    lResult = DeleteRegistryKeyAndContents(HKEY_LOCAL_MACHINE, tszKeyName);
}

void
PPPServerWriteLineConfigurationRegistrySettings(
    PPPServerLineConfiguration_t    *pLineConfig)
//
//  Write a line's settings out to the registry
//
//      HKLM\Comm\ppp\Server\Line\<LineName>\
//
{
    TCHAR   tszKeyName[MAX_PATH + 1];   

    ASSERT(PPP_REGKEY_LINE_LEN + 1 + RAS_MaxDeviceName + 1 < MAX_PATH);
    _stprintf(tszKeyName, TEXT("%s\\%s"), PPP_REGKEY_LINE, pLineConfig->rasDevInfo.szDeviceName);

    (void) WriteRegistryValues(
                HKEY_LOCAL_MACHINE, tszKeyName,
                RAS_PARMNAME_DEVICE_TYPE,            REG_SZ,    0,                pLineConfig->rasDevInfo.szDeviceType,sizeof(pLineConfig->rasDevInfo.szDeviceType) / sizeof(char),
                RAS_PARMNAME_DEVICE_INFO,            REG_BINARY,0,                pLineConfig->pDevConfig,             pLineConfig->dwDevConfigSize,
                RAS_PARMNAME_ENABLE,                 REG_DWORD, 0,               &pLineConfig->bEnable,                sizeof(pLineConfig->bEnable),
                RAS_PARMNAME_FLAGS,                  REG_DWORD, 0,               &pLineConfig->bmFlags,                sizeof(pLineConfig->bmFlags),
                RAS_PARMNAME_DISCONNECT_IDLE_SECONDS,REG_DWORD, 0,               &pLineConfig->DisconnectIdleSeconds,  sizeof(pLineConfig->DisconnectIdleSeconds),
                NULL);
}

DWORD
PPPServerLineComputeRasOptions(
    PPPServerLineConfiguration_t *pLine)
//
//  Compute the RASENTRY.dwfOptions settings for the line,
//  based upon the global and per-line configuration settings.
//
{
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    DWORD                     dwfOptions,
                              bmFlags;

    dwfOptions = PPPServerConfig.bmAuthenticationMethods;

    bmFlags = pLine->bmFlags | pConfig->bmFlags;

    if (bmFlags & PPPSRV_FLAG_REQUIRE_DATA_ENCRYPTION)
    {
        dwfOptions |= RASEO_RequireDataEncryption;
        //
        //  Encryption requires MS CHAP v1 or v2 authentication, force them on.
        //
        if ((dwfOptions & (RASEO_ProhibitMsCHAP | RASEO_ProhibitMsCHAP2)) ==
             (RASEO_ProhibitMsCHAP | RASEO_ProhibitMsCHAP2))
        {
            DEBUGMSG(ZONE_ERROR, (L"PPP: Server configuration error, REQUIRE_DATA_ENCRYPTION requires MSCHAP authentication\n"));
            dwfOptions &= ~(RASEO_ProhibitMsCHAP | RASEO_ProhibitMsCHAP2);
        }
    }

    if (!(bmFlags & PPPSRV_FLAG_NO_VJ_HEADER_COMPRESSION))
    {
        dwfOptions |= RASEO_IpHeaderCompression;
    }

    if (!(bmFlags & PPPSRV_FLAG_NO_DATA_COMPRESSION))
    {
        dwfOptions |= RASEO_SwCompression;
    }

    return dwfOptions;
}

VOID
PPPServerLineUpdateActiveSettings(
    IN  PPPServerLineConfiguration_t    *pLine)
{
    //
    //  If the line is currently open and listening for connections,
    //  then update the settings based upon the current bmFlags.
    //
    if (pLine->pSession)
    {
        pLine->pSession->rasEntry.dwfOptions = PPPServerLineComputeRasOptions(pLine);
        pppComputeAuthTypesAllowed(pLine->pSession);
    }
}

void
PPPServerReadLineConfigurationRegistrySettings(
    LIST_ENTRY  *pLineList)
//
//  Read the PPP Server's per line configuration settings from the registry keys:
//
//      HKLM\Comm\ppp\Server\Line\<LineName>\
//
{
    HKEY        hKey;
    LONG        hRes;
    DWORD       cbRequired = 0;
    DWORD       RetVal = ERROR_SUCCESS;
    DWORD       ccDeviceName;
    UINT        numLines = 0;
    PPPServerLineConfiguration_t    *pLineConfig;
    RASDEVINFO  devInfo;
    
    hRes = RegOpenKeyEx( HKEY_LOCAL_MACHINE, PPP_REGKEY_LINE, 0, KEY_READ, &hKey );

    if (ERROR_SUCCESS == hRes )
    {
        // Now loop over the sub-keys getting the names of each.

        while (TRUE)
        {
            ccDeviceName = sizeof(devInfo.szDeviceName) / sizeof(devInfo.szDeviceName[0]);
            hRes = RegEnumKeyEx (hKey, numLines, devInfo.szDeviceName, &ccDeviceName, NULL, NULL, NULL, NULL);
            if (hRes != ERROR_SUCCESS)
                break;

            DEBUGMSG( ZONE_RAS | ZONE_FUNCTION, (TEXT("PPP: Found Registry Key for device %s\r\n"), devInfo.szDeviceName));

            // Temp dummy type during creation, real type read from registry below
            devInfo.szDeviceType[0] = TEXT('\0');

            pLineConfig = PPPServerCreateLineConfig(pLineList, &devInfo);
            if (!pLineConfig)
            {
                DEBUGMSG( ZONE_ERROR, (TEXT("PPP: ERROR, Server unable to create device %s\n"), devInfo.szDeviceName));
            }
            else
            {
                //
                //  Read in the new line's configuration settings
                //
                (void) ReadRegistryValues(
                        hKey, devInfo.szDeviceName,
                        RAS_PARMNAME_DEVICE_TYPE,            REG_SZ,    0,                pLineConfig->rasDevInfo.szDeviceType,sizeof(pLineConfig->rasDevInfo.szDeviceType) / sizeof(char),
                        RAS_PARMNAME_DEVICE_INFO,            REG_BINARY,REG_ALLOC_MEMORY,&pLineConfig->pDevConfig,         &pLineConfig->dwDevConfigSize,
                        RAS_PARMNAME_ENABLE,                 REG_DWORD, 0,               &pLineConfig->bEnable,                sizeof(pLineConfig->bEnable),
                        RAS_PARMNAME_FLAGS,                  REG_DWORD, 0,               &pLineConfig->bmFlags,                sizeof(pLineConfig->bmFlags),
                        RAS_PARMNAME_DISCONNECT_IDLE_SECONDS,REG_DWORD, 0,               &pLineConfig->DisconnectIdleSeconds,  sizeof(pLineConfig->DisconnectIdleSeconds),
                        NULL);
            }
            numLines++;
        }
        RegCloseKey (hKey);
    }

    DEBUGMSG( ZONE_RAS | ZONE_FUNCTION, (TEXT("PPP: Configured %d server lines from registry\n"), numLines));
}

void
PPPServerWriteRegistrySettings(
    PPPServerConfiguration_t *pConfig)
{
    //
    // Write out server registry settings
    // A future optimization would be to only write out those settings which differ from the defaults.
    //

    (void) WriteRegistryValues(
            HKEY_LOCAL_MACHINE,                 PPP_REGKEY_PARMS,
            RAS_PARMNAME_STATIC_IP_ADDR_START,  REG_DWORD, 0, &pConfig->dwStaticIpAddrStart,    sizeof(pConfig->dwStaticIpAddrStart),
            RAS_PARMNAME_STATIC_IP_ADDR_COUNT,  REG_DWORD, 0, &pConfig->dwStaticIpAddrCount,    sizeof(pConfig->dwStaticIpAddrCount),
            RAS_PARMNAME_AUTHENTICATION_METHODS,REG_DWORD, 0, &pConfig->bmAuthenticationMethods,sizeof(pConfig->bmAuthenticationMethods),
            RAS_PARMNAME_ENABLE,                REG_DWORD, 0, &pConfig->bEnable,                sizeof(pConfig->bEnable),
            RAS_PARMNAME_FLAGS,                 REG_DWORD, 0, &pConfig->bmFlags,                sizeof(pConfig->bmFlags),
            RAS_PARMNAME_USE_AUTOIP_ADDRESSES,  REG_DWORD, 0, &pConfig->bUseAutoIpAddresses,    sizeof(pConfig->bUseAutoIpAddresses),
            RAS_PARMNAME_AUTOIP_SUBNET,         REG_DWORD, 0, &pConfig->dwAutoIpSubnet,         sizeof(pConfig->dwAutoIpSubnet),
            RAS_PARMNAME_AUTOIP_SUBNET_MASK,    REG_DWORD, 0, &pConfig->dwAutoIpSubnetMask,     sizeof(pConfig->dwAutoIpSubnetMask),
            RAS_PARMNAME_USE_DHCP_ADDRESSES,    REG_DWORD, 0, &pConfig->bUseDhcpAddresses,      sizeof(pConfig->bUseDhcpAddresses),
            RAS_PARMNAME_DHCP_IF_NAME,          REG_SZ,    0, &pConfig->wszDhcpInterface[0],    (wcslen(pConfig->wszDhcpInterface)+1) * sizeof(WCHAR),
            RAS_PARMNAME_IPV6_NET_PREFIX,       REG_BINARY,0, &pConfig->IPV6NetPrefix[0],       sizeof(pConfig->IPV6NetPrefix),
            RAS_PARMNAME_IPV6_NET_PREFIX_LEN,   REG_DWORD, 0, &pConfig->IPV6NetPrefixBitLength, sizeof(DWORD),
            RAS_PARMNAME_IPV6_NET_COUNT,        REG_DWORD, 0, &pConfig->IPV6NetCount,           sizeof(DWORD),
            NULL);
}

DWORD
PPPServerSetParameters(
    IN  PRASCNTL_SERVERSTATUS pBufIn,
    IN  DWORD                 dwLenIn)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine;
    DWORD                         dwRetVal = SUCCESS;

    if ((pBufIn == NULL)
    ||  (dwLenIn < sizeof(RASCNTL_SERVERSTATUS)))
    {
        dwRetVal = ERROR_INVALID_PARAMETER;
    }
    else
    {
        EnterCriticalSection( &PPPServerCritSec );

        pConfig->bmFlags = pBufIn->bmFlags;
        pConfig->dwStaticIpAddrStart = pBufIn->dwStaticIpAddrStart;
        pConfig->dwStaticIpAddrCount = pBufIn->dwStaticIpAddrCount;
        pConfig->bmAuthenticationMethods = pBufIn->bmAuthenticationMethods;
        pConfig->bmAuthenticationMethods &= RASEO_ProhibitPAP |
                                            RASEO_ProhibitCHAP |
                                            RASEO_ProhibitMsCHAP |
                                            RASEO_ProhibitMsCHAP2 |
                                            RASEO_ProhibitEAP;


        pConfig->bUseAutoIpAddresses = pBufIn->bUseAutoIpAddresses;
        pConfig->dwAutoIpSubnet      = pBufIn->dwAutoIpSubnet;
        pConfig->dwAutoIpSubnetMask  = pBufIn->dwAutoIpSubnetMask;

        pConfig->bUseDhcpAddresses = pBufIn->bUseDhcpAddresses;
        wcsncpy(&pConfig->wszDhcpInterface[0], &pBufIn->wszDhcpInterface[0], MAX_IF_NAME_LEN);
        pConfig->wszDhcpInterface[MAX_IF_NAME_LEN] = L'\0';

        PPPServerWriteRegistrySettings(pConfig);

        //
        //  Update any open lines for the new global flags settings.
        //
        for (pLine  = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
             pLine != (PPPServerLineConfiguration_t *)(&pConfig->LineList);
             pLine  = (PPPServerLineConfiguration_t *)pLine->listEntry.Flink)
        {
            PPPServerLineUpdateActiveSettings(pLine);
        }

        LeaveCriticalSection( &PPPServerCritSec );
    }

    return dwRetVal;
}

DWORD
PPPServerGetParameters(
    OUT PRASCNTL_SERVERSTATUS pBufOut,
    IN  DWORD                 dwLenOut,
    OUT PDWORD                pdwActualOut)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                        dwRetVal,
                                 dwSizeNeeded,
                                 dwNumLines;

    EnterCriticalSection( &PPPServerCritSec );

    //
    // Count the number of lines
    //
    dwNumLines = CountListEntries(&pConfig->LineList);

    dwSizeNeeded = sizeof(RASCNTL_SERVERSTATUS);

    if ((dwLenOut < dwSizeNeeded) || (NULL == pBufOut))
    {
        dwRetVal = ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        pBufOut->bEnable = pConfig->bEnable;
        pBufOut->bmFlags = pConfig->bmFlags;
        pBufOut->dwStaticIpAddrStart = pConfig->dwStaticIpAddrStart;
        pBufOut->dwStaticIpAddrCount = pConfig->dwStaticIpAddrCount;
        pBufOut->bmAuthenticationMethods = pConfig->bmAuthenticationMethods;
        pBufOut->dwNumLines = dwNumLines;

        pBufOut->bUseAutoIpAddresses = pConfig->bUseAutoIpAddresses;
        pBufOut->dwAutoIpSubnet      = pConfig->dwAutoIpSubnet;
        pBufOut->dwAutoIpSubnetMask  = pConfig->dwAutoIpSubnetMask;

        pBufOut->bUseDhcpAddresses = pConfig->bUseDhcpAddresses;
        wcsncpy(&pBufOut->wszDhcpInterface[0], &pConfig->wszDhcpInterface[0], MAX_IF_NAME_LEN);
        pBufOut->wszDhcpInterface[MAX_IF_NAME_LEN] = L'\0';

        dwRetVal = ERROR_SUCCESS;
    }

    if (pdwActualOut)
        *pdwActualOut = dwSizeNeeded;

    LeaveCriticalSection( &PPPServerCritSec );

    return dwRetVal;
}

DWORD
PPPServerGetIPV6NetPrefix(
    OUT PRASCNTL_SERVER_IPV6_NET_PREFIX pBufOut,
    IN  DWORD                           dwLenOut,
    OUT PDWORD                          pdwActualOut)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                         dwRetVal = ERROR_SUCCESS;

    if ((dwLenOut < sizeof(RASCNTL_SERVER_IPV6_NET_PREFIX)) || (NULL == pBufOut))
    {
        dwRetVal = ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        memcpy(pBufOut->IPV6NetPrefix, pConfig->IPV6NetPrefix, sizeof(pBufOut->IPV6NetPrefix));
        pBufOut->IPV6NetPrefixBitLength = pConfig->IPV6NetPrefixBitLength;
        pBufOut->IPV6NetPrefixCount = pConfig->IPV6NetCount;
    }

    return dwRetVal;
}

DWORD
PPPServerSetIPV6NetPrefix(
    IN  PRASCNTL_SERVER_IPV6_NET_PREFIX pBufIn,
    IN  DWORD                           dwLenIn)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                         dwRetVal = ERROR_SUCCESS;

    if ((dwLenIn < sizeof(RASCNTL_SERVER_IPV6_NET_PREFIX)) || (NULL == pBufIn))
    {
        dwRetVal = ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        memcpy(pConfig->IPV6NetPrefix, pBufIn->IPV6NetPrefix, sizeof(pConfig->IPV6NetPrefix));
        pConfig->IPV6NetPrefixBitLength = pBufIn->IPV6NetPrefixBitLength;
        pConfig->IPV6NetCount = pBufIn->IPV6NetPrefixCount;

        PPPServerWriteRegistrySettings(pConfig);
    }

    return dwRetVal;
}

DWORD
PPPServerGetStatus(
    OUT PRASCNTL_SERVERSTATUS pBufOut,
    IN  DWORD                 dwLenOut,
    OUT PDWORD                pdwActualOut)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLineConfig;
    DWORD                        dwRetVal,
                                 dwSizeNeeded,
                                 dwNumLines;
    RASCNTL_SERVERLINE           *pLineOut;

    EnterCriticalSection( &PPPServerCritSec );

    //
    // Get server globals
    //
    dwRetVal = PPPServerGetParameters(pBufOut, dwLenOut, pdwActualOut);

    //
    // Count the number of lines
    //
    dwNumLines = CountListEntries(&pConfig->LineList);

    dwSizeNeeded = sizeof(RASCNTL_SERVERSTATUS) + dwNumLines * sizeof(RASCNTL_SERVERLINE);

    if ((dwLenOut < dwSizeNeeded) || (NULL == pBufOut))
    {
        dwRetVal = ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        //
        // Get individual line states
        //
        // Line configuration information is placed into the buffer immediately
        // following the global server config.
        //
        pLineOut = (RASCNTL_SERVERLINE *)(&pBufOut[1]);
        pLineConfig = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
        
        while (dwNumLines--)
        {
            pLineOut->rasDevInfo            = pLineConfig->rasDevInfo;
            pLineOut->bEnable               = pLineConfig->bEnable;
            pLineOut->bmFlags               = pLineConfig->bmFlags;
            pLineOut->DisconnectIdleSeconds = pLineConfig->DisconnectIdleSeconds;

            pLineOut++;
            pLineConfig = (PPPServerLineConfiguration_t *)pLineConfig->listEntry.Flink;
        }
        dwRetVal = ERROR_SUCCESS;
    }

    if (pdwActualOut)
        *pdwActualOut = dwSizeNeeded;

    LeaveCriticalSection( &PPPServerCritSec );

    return dwRetVal;
}

DWORD
PPPServerLineAdd(
    IN  PRASCNTL_SERVERLINE pBufIn,
    IN  DWORD               dwLenIn)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine;
    DWORD                        dwRetVal = SUCCESS;

    if ((pBufIn == NULL)
    ||  (dwLenIn < sizeof(RASCNTL_SERVERLINE)))
    {
        dwRetVal = ERROR_INVALID_PARAMETER;
    }
    else
    {
        EnterCriticalSection( &PPPServerCritSec );

        pLine = PPPServerFindOrCreateLineConfig(&pConfig->LineList, &pBufIn->rasDevInfo);
        if (pLine)
        {
            PPPServerWriteLineConfigurationRegistrySettings(pLine);
        }

        LeaveCriticalSection( &PPPServerCritSec );
    }

    return dwRetVal;
}

DWORD
PPPServerLineRemove(
    IN  PPPServerLineConfiguration_t *pLine)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                         dwRetVal;

    dwRetVal = PPPServerLineDisable(pLine);

    PPPServerRemoveLineConfigurationRegistrySettings(pLine);

    PPPServerDestroyLineConfig(&pConfig->LineList, pLine);

    return dwRetVal;
}


#ifdef DEBUG

#define ENUM_TO_SZ_CASE(name) case(name): return #name

char *
GetRASConnectionStateName(
    RASCONNSTATE state)
{
    switch (state)
    {
    ENUM_TO_SZ_CASE(RASCS_OpenPort);
    ENUM_TO_SZ_CASE(RASCS_PortOpened);
    ENUM_TO_SZ_CASE(RASCS_ConnectDevice);
    ENUM_TO_SZ_CASE(RASCS_DeviceConnected);
    ENUM_TO_SZ_CASE(RASCS_AllDevicesConnected);
    ENUM_TO_SZ_CASE(RASCS_Authenticate);
    ENUM_TO_SZ_CASE(RASCS_AuthNotify);
    ENUM_TO_SZ_CASE(RASCS_AuthRetry);
    ENUM_TO_SZ_CASE(RASCS_AuthCallback);
    ENUM_TO_SZ_CASE(RASCS_AuthChangePassword);
    ENUM_TO_SZ_CASE(RASCS_AuthProject);
    ENUM_TO_SZ_CASE(RASCS_AuthLinkSpeed);
    ENUM_TO_SZ_CASE(RASCS_AuthAck);
    ENUM_TO_SZ_CASE(RASCS_ReAuthenticate);
    ENUM_TO_SZ_CASE(RASCS_Authenticated);
    ENUM_TO_SZ_CASE(RASCS_PrepareForCallback);
    ENUM_TO_SZ_CASE(RASCS_WaitForModemReset);
    ENUM_TO_SZ_CASE(RASCS_WaitForCallback);
    ENUM_TO_SZ_CASE(RASCS_Projected);
    ENUM_TO_SZ_CASE(RASCS_Interactive);
    ENUM_TO_SZ_CASE(RASCS_RetryAuthentication);
    ENUM_TO_SZ_CASE(RASCS_CallbackSetByCaller);
    ENUM_TO_SZ_CASE(RASCS_PasswordExpired);
    ENUM_TO_SZ_CASE(RASCS_Connected);
    ENUM_TO_SZ_CASE(RASCS_Disconnected);
    default: return "???";
    }
}

#endif

////////////////////////////////////
//  DHCP address allocation support
////////////////////////////////////

HINSTANCE                  g_hDhcpMod = NULL;
pfnDhcpRequestLeaseForRAS  g_pDhcpRequestLeaseForRAS = NULL;
pfnDhcpReleaseLeaseForRAS  g_pDhcpReleaseLeaseForRAS = NULL;

DWORD
PPPServerGetAdapterIndex(
    LPWSTR  AdapterName,
    PULONG  IfIndex)
{
    DWORD   dwResult = ERROR_NOT_SUPPORTED;

    if (g_pfnGetAdapterIndex)
    {
        dwResult = g_pfnGetAdapterIndex(AdapterName, IfIndex);
    }

    return dwResult;
}

DWORD
PPPServerCreateIpForwardEntry(
    PMIB_IPFORWARDROW   pRoute)
{
    DWORD   dwResult = ERROR_NOT_SUPPORTED;

    if (g_pfnCreateIpForwardEntry)
    {
        dwResult = g_pfnCreateIpForwardEntry(pRoute);
    }

    return dwResult;
}

DWORD
PPPServerDeleteIpForwardEntry(
    PMIB_IPFORWARDROW   pRoute)
{
    DWORD   dwResult = ERROR_NOT_SUPPORTED;

    if (g_pfnDeleteIpForwardEntry)
    {
        dwResult = g_pfnDeleteIpForwardEntry(pRoute);
    }

    return dwResult;
}

#define IPADDR(x)   (x) & 0xff, ((x)>>8) & 0xff, ((x)>>16) & 0xff, ((x)>>24) & 0xff

DWORD
PPPServerBuildRoute(
    IN  PPPServerLineConfiguration_t *pLine,
    OUT PMIB_IPFORWARDROW            pRoute)
{
    DWORD ClientIPAddr;

    ClientIPAddr = pLine->IPAddrInfo[CLIENT_INDEX].IPAddr;
    ClientIPAddr = htonl(ClientIPAddr);

    pRoute->dwForwardDest       = ClientIPAddr;
    pRoute->dwForwardMask       = 0xFFFFFFFF;       // Only forward exact matches of the client IP address
    pRoute->dwForwardNextHop    = ClientIPAddr;
    pRoute->dwForwardPolicy     = 0;
    pRoute->dwForwardIfIndex    = pLine->dwIfIndex;
    pRoute->dwForwardType       = 3;                // The next hop is the final destination (local route)
    pRoute->dwForwardProto      = PROTO_IP_NETMGMT; // Routes added by "route add" or through SNMP
    pRoute->dwForwardAge        = 0;
    pRoute->dwForwardNextHopAS  = 0;
    // The metric is set to 1 on Yamazaki.
    // But a limitation is added in iphlper on the new stack. The metric has to be greater then Interface_Metric. 
    // Otherwise, CreateIpForwardEntry will return a invalid parameter error.
    pRoute->dwForwardMetric1  = 21;                 
    pRoute->dwForwardMetric2  = -1;
    pRoute->dwForwardMetric3  = -1;
    pRoute->dwForwardMetric4  = -1;
    pRoute->dwForwardMetric5  = -1;

    DEBUGMSG(ZONE_TRACE, (TEXT("%d.%d.%d.%d     %d.%d.%d.%d     %d.%d.%d.%d        %d    %2d    %4d\n"), 
                        IPADDR(pRoute->dwForwardDest),
                        IPADDR(pRoute->dwForwardMask),
                        IPADDR(pRoute->dwForwardNextHop),
                        pRoute->dwForwardIfIndex,
                        pRoute->dwForwardType,
                        pRoute->dwForwardMetric1));

    return NO_ERROR;
}


void
PPPServerAddRouteToClient(
    pppSession_t *pSession)
//
//  Add a route that will cause received packets with an IP
//  address equal to that of the client to be sent through
//  the PPP server interface.
//
{
    DWORD                         dwResult = ERROR_DEVICENAME_NOT_FOUND;
    MIB_IPFORWARDROW              route;
    PPPServerLineConfiguration_t *pLine;

    pLine = PPPServerFindLineWithSession(pSession);
    if (pLine)
    {
        dwResult = NO_ERROR;
        if (pLine->dwIfIndex == 0)
        {
            dwResult = PPPServerGetAdapterIndex(pSession->AdapterName, &pLine->dwIfIndex);    
        }
        
        if (dwResult == NO_ERROR)
        {
            dwResult = PPPServerBuildRoute(pLine, &route);
            if (dwResult == NO_ERROR)
            {
                dwResult = PPPServerCreateIpForwardEntry(&route);
                if(dwResult)
                {
                    DEBUGMSG(ZONE_ERROR, (TEXT("PPPServerAddRouteToClient faild with %d\n"), dwResult));
                }
            }
        }
    }
}

void
PPPServerDeleteRouteToClient(
    pppSession_t        *pSession)
//
//  Delete a route to a client previously created by PPPServerAddRouteToClient.
//
{
    DWORD                         dwResult = ERROR_DEVICENAME_NOT_FOUND;
    MIB_IPFORWARDROW              route;
    PPPServerLineConfiguration_t *pLine;

    pLine = PPPServerFindLineWithSession(pSession);
    if (pLine)
    {
        dwResult = PPPServerBuildRoute(pLine, &route);
        if (dwResult == NO_ERROR)
        {
            dwResult = PPPServerDeleteIpForwardEntry(&route);
        }
    }
}


BOOL
PPPServerGetUniqueMacAddress(
    OUT __out_ecount(cbMacAddress) BYTE *pMacAddress,
    IN  UINT    cbMacAddress,
    OUT PDWORD  pdwIfIndex)
//
//  Find the MAC address of a LAN adapter installed in the server
//  to use to generate unique client hardware IDs for getting DHCP
//  leases for PPP connections.
//
//  Return the MAC address and the interface index for its adapter.
//
{
    BOOL                bSuccess = FALSE;

    PREFAST_ASSERT(cbMacAddress <= 6);
    memset(pMacAddress, 0, cbMacAddress);

    bSuccess = utilFindEUI(0, TRUE, pMacAddress, (1<<cbMacAddress), &cbMacAddress, pdwIfIndex);

    return bSuccess;
}

VOID
PPPServerFindIPInfoByIndex(
    DWORD                          Index,
    PPPServerLineConfiguration_t **ppLine,
    PDWORD                         pInfoType)
//
//  Find the line with IP Info having the specified Index
//  Return NULL if no match found.
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine;
    DWORD                         type;

    *ppLine = NULL;

    for (pLine  = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
         pLine != (PPPServerLineConfiguration_t *)(&pConfig->LineList);
         pLine  = (PPPServerLineConfiguration_t *)pLine->listEntry.Flink)
    {
        for (type = SERVER_INDEX; type <= CLIENT_INDEX; type++)
        {
            if (Index == pLine->IPAddrInfo[type].Index)
            {
                *ppLine = pLine;
                *pInfoType = type;
                break;
            }
        }

        if (*ppLine)
            break;
    }
}

VOID
PPPServerLineAssignUnusedIndices(
    PPPServerLineConfiguration_t *pLine)
//
//  Assign lowest available unused index values for the server and client IP info structures
//  These indexes become the last 4 bytes of the unique client hw address identifiers that
//  are used to reserve DHCP addresses.
//
{
    DWORD                         index;
    PPPServerLineConfiguration_t *pLineUsingIndex;
    DWORD                         type, typeUsingIndex;

    //
    //  We should have freed and zeroed any prior in use indices
    //
    ASSERT(pLine->IPAddrInfo[SERVER_INDEX].Index == 0);
    ASSERT(pLine->IPAddrInfo[CLIENT_INDEX].Index == 0);

    index = 0x8001;
    type = SERVER_INDEX;

    while (type <= CLIENT_INDEX)
    {
        PPPServerFindIPInfoByIndex(index, &pLineUsingIndex, &typeUsingIndex);
        if (pLineUsingIndex == NULL)
        {
            // index is not in use, assign it
            pLine->IPAddrInfo[type].Index = index;
            type++;
        }

        index++;

        // There should not be too many of these in use in practice
        ASSERT(index <= 0xFFFF);
    }
}

#define RAS_PREPEND_SIZE    strlen(RAS_PREPEND)

VOID
PPPServerLineBuildClientHWAddrs(
    PPPServerLineConfiguration_t *pLine,
    BYTE                         *pMacAddr,
    DWORD                         cbMacAddr)
//
//  Build the client hardware addresses to be used to acquire DHCP leases.
//
//  The address takes the form:
//
//      4 Bytes:    "RAS "
//      8 Bytes:    <MacAddress> (0 padded on the front)
//      4 Bytes:    <Index>
//
{
    DWORD   type, index;
    BYTE    *pHWAddr;

    PPPServerLineAssignUnusedIndices(pLine);

    if (cbMacAddr > 8)
        cbMacAddr = 8;

    for (type = SERVER_INDEX; type <= CLIENT_INDEX; type++)
    {
        //
        //  "RAS " part
        //
        pHWAddr = &pLine->IPAddrInfo[type].DhcpClientHWAddress[0];
        memcpy(pHWAddr, RAS_PREPEND, RAS_PREPEND_SIZE);
        pHWAddr += RAS_PREPEND_SIZE;

        //
        //  <MacAddress> part
        //
        memset(pHWAddr, 0, 8);
        memcpy(pHWAddr + (8 - cbMacAddr), pMacAddr, cbMacAddr);
        pHWAddr += 8;

        //
        //  <Index> part
        //
        index = pLine->IPAddrInfo[type].Index;
        index = htonl(index);
        memcpy(pHWAddr, (BYTE *)&index, sizeof(index));
    }
}


BOOL
PPPServerIPAddrInUseOnNet(
    DWORD   dwIpAddr)
//
//  Determine whether the specified IP address is assigned to some
//  other adapter on our device, or some other device on the subnet.
//
//  dwIpAddr will be in format 0xDDCCBBAA where the ip address is A.B.C.D
//
{
    BOOL                bInUse = FALSE;
    DWORD               dwResult;
    BYTE                macAddr[6];
    DWORD               cbMacAddr;
    PMIB_IPADDRTABLE    pTable;
    PMIB_IPADDRROW      pRow;
    DWORD               i;

    //
    // Check to see if the address is in use on the local device.
    //
    dwResult = PPPServerGetIPAddrTable(&pTable, NULL);
    if (dwResult == NO_ERROR)
    {
        // Search the table for an interface with a matching IpAddr
        for (i = 0; i < pTable->dwNumEntries; i++)
        {
            pRow = &pTable->table[i];
            if (pRow->dwAddr == dwIpAddr)
            {
                DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: IP Address %x is already in use by local IF Index %x\n", dwIpAddr, pRow->dwIndex));
                bInUse = TRUE;
                break;
            }
        }
        LocalFree(pTable);
    }

    //
    // If not in use on local device, check to see if some external
    // device is using it.
    //
    if (!bInUse && g_pfnSendARP)
    {
        cbMacAddr = sizeof(macAddr);
        dwResult = g_pfnSendARP(dwIpAddr, 0, (PULONG)(&macAddr[0]), &cbMacAddr);

        if (dwResult == NO_ERROR && cbMacAddr > 0)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: IP Address %x is already in use by device with MAC=%02X%02X%02X%02X%02X%02X\n",
                dwIpAddr, macAddr[0], macAddr[2], macAddr[2], macAddr[3], macAddr[4], macAddr[5]));
            bInUse = TRUE;
        }
    }

    return bInUse;
}

//* PPPServerDhcpSetAddressCallback - Called by DHCP when a lease is obtained or lost
//
//  Input:  Context - Context value we passed to DhcpRequestLeaseForRAS
//          Addr    - IP address to set.  0 if lease has been lost.
//          Mask    - Subnet mask for Addr.
//
//  Returns: IP_SUCCESS if we changed the address
//
DWORD
PPPServerDhcpSetAddressCallback(
    IN  UINT        Context,
    IN  IPAddr      Addr,
    IN  IPMask      Mask)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    UINT                          Result = IP_SUCCESS;
    PPPServerLineConfiguration_t *pLine;
    DWORD                         type;  // SERVER_INDEX or CLIENT_INDEX
    DWORD                         OldIPAddr, OldMask;
    BOOL                          bDhcpCompleted = TRUE;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PPPServerDhcpSetAddrCb(%x) Addr=%x Mask=%x\n"), Context, Addr, Mask));

    //
    //  Find the IPAddrInfo for the Context
    //
    PPPServerFindIPInfoByIndex(Context, &pLine, &type);

    if (pLine == NULL)
    {
        DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING: PPPServerDhcpSetAddrCb(%x) Addr=%x Mask=%x *NO LINE*\n"), Context, Addr, Mask));
    }
    else
    {
        OldIPAddr = pLine->IPAddrInfo[type].IPAddr;
        OldMask = pLine->IPAddrInfo[type].IPMask;
        pLine->IPAddrInfo[type].IPAddr = ntohl(Addr);
        pLine->IPAddrInfo[type].IPMask = ntohl(Mask);
        pLine->IPAddrInfo[type].GWAddr = 0;

        if (Addr == 0)
        {
            if (pLine->IPAddrInfo[type].bDhcpCollision)
            {
                //
                // DHCP calls us immediately after we report a collision
                // to take away the address that we rejected. We don't want
                // to signal the lease process is done in that case.
                //
                pLine->IPAddrInfo[type].bDhcpCollision = FALSE;
                bDhcpCompleted = FALSE; // DHCP will try again to get a lease
            }
            else
            {
                //
                //  If we lost the lease unexpectedly then we want to shut the line down.
                //  If we gave up the lease (disabled the line ourselves by setting the
                //  global or local line enable flags off), then we don't need to do
                //  anything.
                //
                if (pConfig->bEnable)
                {
                    DEBUGMSG(ZONE_WARN, (L"PPPSERVER: WARNING - DHCP Lease Failure, shutting down line\n"));
                    PPPServerLineDisable(pLine);
                }
            }

            // Stop using the address, the lease is not available

            if (OldIPAddr)
            {
                // Tell IP to stop handling proxy ARP requests for the address.

                ArpProxyManagerIssueRequest(OldIPAddr, FALSE);
            }
        }
        else
        {
            // Lease obtained

            if (PPPServerIPAddrInUseOnNet(Addr))
            {
                DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING: DHCP assigned Addr=%x is already IN USE by another device\n"), Addr));
                Result = DHCP_COLLISION;
                bDhcpCompleted = FALSE; // DHCP will try again to get a lease
                pLine->IPAddrInfo[type].bDhcpCollision = TRUE;
                pLine->IPAddrInfo[type].dwDhcpCollisionCount++;
            }
            else
            {
                // Configure IP to handle ARP requests for the address
                Result = ArpProxyManagerIssueRequest(ntohl(Addr), TRUE);
            }
        }

        //
        // We are done unless the address was a duplicate in which case
        // the attempt to get a lease continues.
        //
        if (bDhcpCompleted)
        {
            DEBUGMSG(ZONE_PPP, (L"PPP: SIGNALLING DHCP LEASE REQUEST COMPLETED Result=%d.\n", Result));

            SetEvent(pLine->IPAddrInfo[type].hEvent);
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -PPPServerDhcpSetAddrCb(%x) Result=%d\n"), Context, Result));

    return Result;
}

TCHAR cLineTypeSuffix[2] = 
{
    TEXT('s'),  // SERVER_INDEX
    TEXT('c')   // CLIENT_INDEX
};

DWORD
PPPServerLineRequestDHCPAddrs(
    PPPServerLineConfiguration_t *pLine)
//
//  Issue requests to DHCP to assign IP addresses for the line
//
{
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    TCHAR   tszAdapterName[RAS_MaxDeviceName+1+1];
    DWORD   dwResult;
    DWORD   type;
    DWORD   dwCollisionCount;
    DWORD   dhcpTimeouts;

    for (type = SERVER_INDEX; type <= CLIENT_INDEX; type++)
    {
        // tszAdapterName has to be unique for the Dhcp lease, 
        //   e.g. "VPN1s" for serverside, "VPN1c" for clientside
        _stprintf(tszAdapterName, TEXT("%s%c"), pLine->rasDevInfo.szDeviceName, cLineTypeSuffix[type]);

        DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: DHCP_REQUEST_ADDRESS for %s\n"), tszAdapterName));

        ResetEvent(pLine->IPAddrInfo[type].hEvent);

        if (NULL == g_pDhcpRequestLeaseForRAS)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: ERROR - g_pDhcpRequestLeaseForRAS is NULL\n"));
            dwResult = ERROR_NOT_SUPPORTED;
            break;
        }
        dwResult = g_pDhcpRequestLeaseForRAS(pLine->IPAddrInfo[type].Index, tszAdapterName, PPPServerDhcpSetAddressCallback);

        if (dwResult != DHCP_SUCCESS)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("PPP: DHCP_REQUEST_ADDRESS failed, error=%x\n"), dwResult));
            break;
        }
        else
        {
            // Wait for DHCP to get the lease

            for (dhcpTimeouts = 0; dhcpTimeouts <= pConfig->DhcpMaxTimeouts;  dhcpTimeouts++)
            {
                dwCollisionCount = pLine->IPAddrInfo[type].dwDhcpCollisionCount;

                LeaveCriticalSection( &PPPServerCritSec );

                DEBUGMSG(ZONE_PPP, (L"PPPSERVER: Wait for DHCP lease...\n"));
                dwResult = WaitForSingleObject(pLine->IPAddrInfo[type].hEvent, pConfig->DhcpTimeoutMs);
                DEBUGMSG(ZONE_PPP, (L"PPPSERVER: Lease request completed, result=%d.\n", dwResult));

                EnterCriticalSection( &PPPServerCritSec );

                // If didn't time out or had no activity, we are done
                if (dwResult != WAIT_TIMEOUT
                ||  dwCollisionCount == pLine->IPAddrInfo[type].dwDhcpCollisionCount)
                {
                    break;
                }
            }

            if (dwResult == WAIT_TIMEOUT)
            {
                // No response from DHCP to our request with 30 seconds
                DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: ERROR - DHCP Lease request timeout\n"));
                dwResult = ERROR_NO_IP_ADDRESSES;
                break;
            }

            if (pLine->IPAddrInfo[type].IPAddr == 0)
            {
                // DHCP failed to obtain a lease.
                dwResult = ERROR_NO_IP_ADDRESSES; 
                DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: ERROR - DHCP Lease request failure\n"));
                break;
            }

            dwResult = NO_ERROR;
        }

        DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: DHCP_REQUEST_ADDRESS for %s RESULT=%d\n"), tszAdapterName, dwResult));
    }
    DEBUGMSG(ZONE_ERROR && dwResult, (TEXT("PPP: PPPServerLineRequestDHCPAddrs FAILED result=%d\n"), dwResult));

    return dwResult;
}

DWORD
PPPServerLineReleaseDHCPAddrs(
    PPPServerLineConfiguration_t *pLine)
//
//  Issue requests to DHCP to release IP addresses for the line
//
{
    TCHAR   tszAdapterName[RAS_MaxDeviceName+1+1];
    DWORD   dwResult = ERROR_SUCCESS;
    DWORD   type;

    for (type = SERVER_INDEX; type <= CLIENT_INDEX; type++)
    {
        _stprintf(tszAdapterName, TEXT("%s%c"), pLine->rasDevInfo.szDeviceName, cLineTypeSuffix[type]);

        DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: DHCP_REQUEST_RELEASE for %s\n"), tszAdapterName));

        if (NULL == g_pDhcpReleaseLeaseForRAS)
        {
            DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: ERROR - g_pfnDhcpReleaseLeaseForRAS is NULL\n"));
            dwResult = ERROR_NOT_SUPPORTED;
            break;
        }

        dwResult = g_pDhcpReleaseLeaseForRAS(pLine->IPAddrInfo[type].Index);
    }

    return dwResult;
}

BOOL
PPPServerUsingIpAddr(
     DWORD ipAddr)
//
//  ipAddr should be in the format 0xAABBCCDD
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine;
    BOOL                          bInUse;

    // Determine if the address is in use by an existing PPP Server line

    bInUse = FALSE;

    for (pLine  = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
            pLine != (PPPServerLineConfiguration_t *)(&pConfig->LineList);
            pLine  = (PPPServerLineConfiguration_t *)pLine->listEntry.Flink)
    {
        if (ipAddr == pLine->IPAddrInfo[SERVER_INDEX].IPAddr 
        ||  ipAddr == pLine->IPAddrInfo[CLIENT_INDEX].IPAddr)
        {
            // Address in use, try another.
            bInUse = TRUE;
            break;
        }
    }

    return bInUse;
}

void
PPPServerRegisterWithDhcp()
//
//  Obtain and register hooks into the dhcp library
//
{
    //
    //  If the DHCP dll is not currently loaded, try to load it
    //
    if (g_hDhcpMod == NULL)
    {
        g_hDhcpMod = LoadLibrary(TEXT("dhcp.dll"));
    }

    if (g_hDhcpMod && !g_pDhcpRequestLeaseForRAS)
    {
        // Get handle to DhcpRegister
        g_pDhcpRequestLeaseForRAS = (pfnDhcpRequestLeaseForRAS)GetProcAddress(g_hDhcpMod, TEXT("DhcpRequestLeaseForRAS"));
        g_pDhcpReleaseLeaseForRAS = (pfnDhcpReleaseLeaseForRAS)GetProcAddress(g_hDhcpMod, TEXT("DhcpReleaseLeaseForRAS"));
    }
    
    if (g_pDhcpRequestLeaseForRAS && g_pDhcpReleaseLeaseForRAS)
    {
        DEBUGMSG(ZONE_INIT, (TEXT("PPP: registered w/ DHCP\n")));
    }
    else
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("PPP: DhcpRegister failed\n")));
    }
}

////////////////////////////////////
//  Auto IP address allocation support
////////////////////////////////////

DWORD
PPPServerLineGenerateAutoIpAddrs(
    PPPServerLineConfiguration_t *pLine)
//
//  Generate random IP addresses on the AutoIP subnet that are not in use.
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD   dwResult = NO_ERROR;
    DWORD   type;
    DWORD   randomIpAddr;
    DWORD   dwTriesRemaining;

    for (type = SERVER_INDEX; type <= CLIENT_INDEX; type++)
    {
        for (dwTriesRemaining = 10; dwTriesRemaining; dwTriesRemaining--)
        {
            // A host id of all 0's or all 1's is not allowed
            do
                FipsGenRandom(sizeof(randomIpAddr), (PBYTE)&randomIpAddr);
            while (((randomIpAddr & ~pConfig->dwAutoIpSubnetMask) == 0)
            ||     ((randomIpAddr & ~pConfig->dwAutoIpSubnetMask) == ~pConfig->dwAutoIpSubnetMask));
            
            randomIpAddr &= ~pConfig->dwAutoIpSubnetMask;
            randomIpAddr |=  pConfig->dwAutoIpSubnet;
            if (!PPPServerUsingIpAddr(randomIpAddr)
            &&  !PPPServerIPAddrInUseOnNet(htonl(randomIpAddr)))
                // random address is available, we'll use it
                break;
        }

        if (dwTriesRemaining == 0)
        {
            dwResult = ERROR_NO_IP_ADDRESSES;
            break;
        }

        pLine->IPAddrInfo[type].IPAddr = randomIpAddr;
        pLine->IPAddrInfo[type].IPMask = pConfig->dwAutoIpSubnetMask;

        // Configure IP to handle ARP requests for the address
        dwResult = ArpProxyManagerIssueRequest(randomIpAddr, TRUE);
    }

    return dwResult;
}

////////////////////////////////////
//  Static IP address allocation support
////////////////////////////////////

DWORD
PPPServerGetFreeStaticIpAddress()
//
//  Find an unused address in the static IP pool and return it.
//  Return 0 if no free address is found.
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                         ipAddr;
    DWORD                         i;

    for (i = 0; TRUE; i++)
    {
        if (i == pConfig->dwStaticIpAddrCount)
        {
            // All addresses in static pool are in use
            ipAddr = 0;
            break;
        }

        ipAddr = pConfig->dwStaticIpAddrStart + i;

        //
        // If the address is not in use, we are done.
        //
        if (!PPPServerUsingIpAddr(ipAddr))
        {
            break;
        }

        // Otherwise, keep looking for a free one
    }

    return ipAddr;
}

VOID
PPPServerLineReleaseIPAddresses(
    PPPServerLineConfiguration_t *pLine)
{

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PPPServerLineReleaseIPAddresses\n")));

    //
    // Static addresses are implicitly released when the line
    // is disabled.
    //
    // DHCP addresses need to be explicitly released.
    //
    if (pLine->bUsingDhcpAddress)
    {
        PPPServerLineReleaseDHCPAddrs(pLine);
        pLine->bUsingDhcpAddress = FALSE;
    }

    // Make sure any ARP proxying is terminated
    ArpProxyManagerIssueRequest(pLine->IPAddrInfo[SERVER_INDEX].IPAddr, FALSE);
    ArpProxyManagerIssueRequest(pLine->IPAddrInfo[CLIENT_INDEX].IPAddr, FALSE);

    pLine->IPAddrInfo[SERVER_INDEX].IPAddr = 0;
    pLine->IPAddrInfo[SERVER_INDEX].Index = 0;
    pLine->IPAddrInfo[CLIENT_INDEX].IPAddr = 0;
    pLine->IPAddrInfo[CLIENT_INDEX].Index = 0;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -PPPServerLineReleaseIPAddresses\n")));
}

DWORD
PPPServerLineGetIPAddresses(
    PPPServerLineConfiguration_t *pLine)
//
//  Obtain IP addresses for the server and client sides
//  of a PPP connection.
//
//  Return:
//      SUCCESS if it works,
//      ERROR_NO_IP_ADDRESS if no addresses were assigned
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                         dwResult = ERROR_NO_IP_ADDRESSES;
    BYTE                          MacAddress[6];
    BOOL                          bSuccess;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PPPServerLineGetIPAddresses\n")));

    ASSERT(pLine->IPAddrInfo[SERVER_INDEX].IPAddr == 0);
    ASSERT(pLine->IPAddrInfo[CLIENT_INDEX].IPAddr == 0);

    if (pConfig->bUseDhcpAddresses)
    {
        if (g_pDhcpRequestLeaseForRAS == NULL || g_pDhcpReleaseLeaseForRAS == NULL)
        {
            // The DHCP service is not available
            DEBUGMSG(ZONE_ERROR, (L"PPPSERVER: ERROR - DHCP Unavailable\n"));
            dwResult = ERROR_NOT_SUPPORTED;
        }
        else
        {
            //
            //  Get the MAC address of a DHCP enabled LAN
            //  adapter in the system.  Also find out the interface index for 
            //
            bSuccess = PPPServerGetUniqueMacAddress(MacAddress, sizeof(MacAddress), &pLine->dwDhcpIfIndex);

            if (bSuccess)
            {
                //
                //  Build the client HW address to use with DHCP
                //
                PPPServerLineBuildClientHWAddrs(pLine, MacAddress, sizeof(MacAddress));

                //
                //  Issue the request to DHCP to assign the client
                //  and server IP addresses
                //
                dwResult = PPPServerLineRequestDHCPAddrs(pLine);

                if (dwResult == SUCCESS)
                {
                    pLine->bUsingDhcpAddress = TRUE;
                }
            }
        }
    }
    else if (pConfig->bUseAutoIpAddresses)
    {
        // Auto IP address assignment

        pLine->bUsingDhcpAddress = FALSE;
        dwResult = PPPServerLineGenerateAutoIpAddrs(pLine);
    }
    else
    {
        // Static IP address assignment

        pLine->bUsingDhcpAddress = FALSE;
        pLine->IPAddrInfo[SERVER_INDEX].IPAddr = PPPServerGetFreeStaticIpAddress();
        pLine->IPAddrInfo[SERVER_INDEX].IPMask = IPGetNetMask(pLine->IPAddrInfo[SERVER_INDEX].IPAddr);
        pLine->IPAddrInfo[CLIENT_INDEX].IPAddr = PPPServerGetFreeStaticIpAddress();
        pLine->IPAddrInfo[CLIENT_INDEX].IPMask = IPGetNetMask(pLine->IPAddrInfo[CLIENT_INDEX].IPAddr);

        if (pLine->IPAddrInfo[SERVER_INDEX].IPAddr 
        &&  pLine->IPAddrInfo[CLIENT_INDEX].IPAddr)
        {
            dwResult = SUCCESS;
        }
        else
        {
            // No static IP addresses available
        }
    }
#ifdef DEBUG
    if (dwResult == SUCCESS)
    {
        DEBUGMSG(ZONE_TRACE, (TEXT("PPP: IPAddrs for line %s are client=%x server=%x\n"),
            pLine->rasDevInfo.szDeviceName, pLine->IPAddrInfo[SERVER_INDEX].IPAddr , pLine->IPAddrInfo[SERVER_INDEX].IPAddr));
    }
#endif

    if (dwResult != SUCCESS)
    {
        pLine->IPAddrInfo[SERVER_INDEX].IPAddr = 0;
        pLine->IPAddrInfo[SERVER_INDEX].Index = 0;
        pLine->IPAddrInfo[CLIENT_INDEX].IPAddr = 0;
        pLine->IPAddrInfo[CLIENT_INDEX].Index = 0;
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -PPPServerLineGetIPAddresses Result=%x\n"), dwResult));

    return dwResult;
}

DWORD
PPPServerLineClose(
    PPPServerLineConfiguration_t *pLine)
{
    DWORD   dwResult = SUCCESS;
    pppSession_t *s_p = pLine->pSession;

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:+PPPServerLineClose\n" ) ));

    if (s_p)
    {
        AfdRasHangUp((HRASCONN)s_p);
        pLine->pSession = NULL;
    }

    //
    //  Release any IP addresses allocated for the line
    //

    PPPServerLineReleaseIPAddresses(pLine);

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:-PPPServerLineClose\n" ) ));

    return dwResult;
}

DWORD
pppServerLineOpenAndListen(
    PVOID   pData)
{
    pppSession_t             *pSession = (pppSession_t *)pData;
    DWORD                     dwResult = ERROR_DEVICENAME_NOT_FOUND;
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    DWORD                     dwHoldSeconds = 0;

    if (PPPADDREF(pSession, REF_SERVERLINEOPEN))
    {
        if (pSession->dwConsecutiveClientAuthFails >= pConfig->AuthMaxConsecutiveFail)
        {
            dwHoldSeconds = pConfig->AuthFailHoldSeconds;
        }
        PPPDELREF(pSession, REF_SERVERLINEOPEN);
    }

    if (dwHoldSeconds)
    {
        DEBUGMSG(ZONE_PPP, (L"PPP Server: Holding line for %u seconds due to auth failure\n", dwHoldSeconds));
        Sleep(dwHoldSeconds * 1000);
        pSession->dwConsecutiveClientAuthFails = 0;
    }

    if (PPPADDREF(pSession, REF_SERVERLINEOPEN))
    {
        pppLock(pSession);

        // Start listening for connections on the line
        pppChangeOfState( pSession, RASCS_OpenPort, 0 );
        dwResult = pppMac_LineOpen(pSession->macCntxt);
        pppMac_LineListen(pSession->macCntxt);

        pppLcp_Open(pSession->lcpCntxt);

        pppUnLock(pSession);

        PPPDELREF(pSession, REF_SERVERLINEOPEN);
    }

    return dwResult;
}

DWORD
pppServerLineOpenAndListenThread(
    PVOID   pData)
{
    pppSession_t             *pSession = (pppSession_t *)pData;
    DWORD                     dwResult;

    dwResult = pppServerLineOpenAndListen(pSession);

    if (PPPADDREF(pSession, REF_SERVERLINEOPEN))
    {
        if (pSession->hOpenThread)
        {
            CloseHandle(pSession->hOpenThread);
            pSession->hOpenThread = NULL;
        }
        PPPDELREF(pSession, REF_SERVERLINEOPEN);
    }
    return dwResult;
}

void
pppServerLineCloseCompleteCallback(
    void *pData)
//
//  This function is called when the line has been closed while we are
//  in the process of restarting a PPP server connection.
//
{
    pppSession_t *pSession = (pppSession_t *)pData;
    DWORD         dwThreadId;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppServerLineCloseCompleteCallback\n")));

    if (!pSession->HangUpReq)
    {
        // Reopen the line to listen for a new connection
        //
        // Because we are in a callback, and the requests to open the line and listen
        // are synchronous, we must spin a thread to execute the requests to avoid
        // a potential hang. That is, if this callback is executed on a miniport driver
        // thread which is needed to process the line open requests, and those requests
        // are completed asynchronously, then we can potentially hang forever if we block
        // the miniport thread.

        if (NULL == pSession->hOpenThread)
            pSession->hOpenThread = CreateThread(NULL, 0, pppServerLineOpenAndListenThread, pSession, 0, &dwThreadId);

        DEBUGMSG(ZONE_ERROR && pSession->hOpenThread == NULL, (TEXT("PPP: ERROR-CreateThread failed %d\n"), GetLastError()));
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppServerLineCloseCompleteCallback\n")));
}

void
pppServerLcpCloseCompleteCallback(
    void *pData)
//
//  This function is called when the LCP layer has been brought
//  down as part of restarting a PPP server connection.
//
{
    pppSession_t *pSession = (pppSession_t *)pData;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppServerLcpCloseCompleteCallback\n")));

    pppMac_LineClose(pSession->macCntxt, pppServerLineCloseCompleteCallback, pSession);

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppServerLcpCloseCompleteCallback\n")));
}

DWORD
PPPServerBuildDefaultRoute(
    IN    DWORD dwPeerIpAddr,
    IN    DWORD dwForwardMask,
    IN    DWORD dwGatewayIpAddr,
    IN    DWORD dwIfIndex,
    IN    DWORD dwForwardType,
    IN    DWORD dwForwardMetric1,
    OUT PMIB_IPFORWARDROW pRoute)
{
    pRoute->dwForwardDest        = dwPeerIpAddr;
    pRoute->dwForwardMask        = dwForwardMask;         // Only forward exact matches of the client IP address
    pRoute->dwForwardNextHop    = dwGatewayIpAddr;
    pRoute->dwForwardPolicy     = 0;
    pRoute->dwForwardIfIndex    = dwIfIndex;
    pRoute->dwForwardType        = dwForwardType;
    pRoute->dwForwardProto        = PROTO_IP_NETMGMT;    // Routes added by "route add" or through SNMP
    pRoute->dwForwardAge        = 0;
    pRoute->dwForwardNextHopAS    = 0;
    pRoute->dwForwardMetric1    = dwForwardMetric1;
    pRoute->dwForwardMetric2    = (DWORD)-1;
    pRoute->dwForwardMetric3    = (DWORD)-1;
    pRoute->dwForwardMetric4    = (DWORD)-1;
    pRoute->dwForwardMetric5    = (DWORD)-1;

    DEBUGMSG(ZONE_TRACE, (TEXT("%d.%d.%d.%d     %d.%d.%d.%d     %d.%d.%d.%d        %d    %2d    %4d\n"), 
                        IPADDR(pRoute->dwForwardDest),
                        IPADDR(pRoute->dwForwardMask),
                        IPADDR(pRoute->dwForwardNextHop),
                        pRoute->dwForwardIfIndex,
                        pRoute->dwForwardType,
                        pRoute->dwForwardMetric1));
    return NO_ERROR;
}


void
PPPServerCheckDefaultRouteToClient(
    pppSession_t *pSession
    )
{
    DWORD                         dwResult = ERROR_DEVICENAME_NOT_FOUND;
    MIB_IPFORWARDROW              route;
    PPPServerLineConfiguration_t *pLine;
    PMIB_IPFORWARDTABLE pValidTable = NULL;
    DWORD ServerIPAddr;
    DWORD dwPeerIpAddr = 0;
    DWORD dwForwardMask = 0;
    DWORD dwGatewayIpAddr;
    DWORD dwIfIndex;
    DWORD dwForwardType;
    DWORD dwForwardMetric = 0;
    DWORD dwSize = 0;
    BOOL bOrder = FALSE;
    DWORD i;
    DWORD defaultRouteNum;
    DWORD gotit = 0;

    pLine = PPPServerFindLineWithSession(pSession);
    if (pLine)
    {
        ServerIPAddr = pLine->IPAddrInfo[SERVER_INDEX].IPAddr;
        ServerIPAddr = htonl(ServerIPAddr);
        dwGatewayIpAddr = ServerIPAddr;

        do{
            if(ERROR_INSUFFICIENT_BUFFER != g_pfnGetIpForwardTable(NULL,&dwSize, bOrder))
            {
                DEBUGMSG(ZONE_TRACE, (TEXT("Failed to get required buffer Size")));
                dwIfIndex = pLine->dwIfIndex;
                dwForwardType = 3;
                dwForwardMetric = 0;
                break;
            }
            pValidTable = (PMIB_IPFORWARDTABLE)malloc(dwSize);
            g_pfnGetIpForwardTable(pValidTable,&dwSize,bOrder);

            defaultRouteNum = pValidTable->dwNumEntries < MAXDFLT? pValidTable->dwNumEntries: MAXDFLT;
            for ( i = 0; i < defaultRouteNum; i++)
            {
                DEBUGMSG(ZONE_TRACE, (TEXT("i=%u, dwNumEntries=%u, dwForwardDest=%u, dwForwardMask=%u, dwForwardNextHop=%d.%d.%d.%d, dwForwardIfIndex=%u --- ServerIPAddr=%d.%d.%d.%d\n"),
                            i,
                            pValidTable->dwNumEntries,
                            pValidTable->table[i].dwForwardDest,
                            pValidTable->table[i].dwForwardMask,
                            IPADDR(pValidTable->table[i].dwForwardNextHop),
                            pValidTable->table[i].dwForwardIfIndex,
                            IPADDR(ServerIPAddr)
                            ));
                
                if ((pValidTable->table[i].dwForwardDest == 0) && 
                    (pValidTable->table[i].dwForwardMask == 0) &&
                    (pValidTable->table[i].dwForwardNextHop == ServerIPAddr) && 
                    ((pLine->dwIfIndex == 0 ) || (pValidTable->table[i].dwForwardIfIndex == pLine->dwIfIndex)))
                {
                    
                    dwIfIndex = pValidTable->table[i].dwForwardIfIndex;
                    dwForwardType = pValidTable->table[i].dwForwardType;
                    dwForwardMetric = pValidTable->table[i].dwForwardMetric1;
                    gotit = 1;
                    break;
                }
            }
        }while(0);

        if (!gotit)
        {
            dwIfIndex = pLine->dwIfIndex;
            dwForwardType = 3;
            dwForwardMetric = 0;
        }
        
        dwResult = PPPServerBuildDefaultRoute(dwPeerIpAddr, dwForwardMask, dwGatewayIpAddr, dwIfIndex, dwForwardType, dwForwardMetric, &route);
        if (dwResult == NO_ERROR)
        {
            PPPServerDeleteIpForwardEntry(&route);
        }
    }
}

void
PPPServerNotifierCallback(
    HRASCONN     hrasconn,
    UINT         unMsg,
    RASCONNSTATE rascs,
    DWORD        dwError,
    DWORD        dwExtendedError)
//
//  Called when the PPP state changes
//
{
    pppSession_t *pSession = (pppSession_t *)hrasconn;

    DEBUGMSG(ZONE_PPP, (TEXT("PPPServerNotifierCallback: State = %hs (%d)\n"), GetRASConnectionStateName(rascs), rascs));

    switch(rascs)
    {
    case RASCS_Disconnected:
        // Close and reopen the link to keep listening for new calls if we're not shutting down
        // the line.
        if (!pSession->HangUpReq)
        {
            pppLcp_Close(pSession->lcpCntxt, pppServerLcpCloseCompleteCallback, pSession);
        }
        break;

    case RASCS_AuthNotify:
        if (dwError != NO_ERROR)
        {
            // Authentication on an incoming call has failed, 

            pSession->dwConsecutiveClientAuthFails++;

            // Request LCP terminate link, when it terminates
            // (MAC layer disconnected) we will reopen it to
            // keep listening for new calls.

            pppLcp_Close(pSession->lcpCntxt, pppServerLcpCloseCompleteCallback, pSession);
        }
        else
        {
            // Auth successful, reset fail counter
            pSession->dwConsecutiveClientAuthFails = 0;
        }
        break;
        
    case RASCS_Connected:
        // We have to enable forwarding for physical interface and VPN interface.
        PPPServerUpdateInterfaceForward(pSession, TRUE);
        // Check if there are default routes to client
        PPPServerCheckDefaultRouteToClient(pSession);
        // Add a route to the new client
        PPPServerAddRouteToClient(pSession);
        break;
        
    }
}

DWORD
PPPServerLineOpen(
    PPPServerLineConfiguration_t *pLine)
//
//  Open the line and listen for incoming connections
//
{
    pppStart_t  Start;
    RASENTRY    RasEntry;
    DWORD       rc;
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:+PPPServerLineOpen\n" ) ));

    ASSERT(pLine->pSession == NULL);

    rc = PPPServerLineGetIPAddresses(pLine);
    if (rc)
    {
        return rc;
    }

    //
    //  Set up device identification.
    //
    memset(&RasEntry, 0, sizeof(RasEntry));
    RasEntry.dwSize = sizeof(RasEntry);
    _tcscpy (RasEntry.szDeviceType, pLine->rasDevInfo.szDeviceType);
    _tcscpy (RasEntry.szDeviceName, pLine->rasDevInfo.szDeviceName);

    RasEntry.dwfOptions = PPPServerLineComputeRasOptions(pLine);

    // Create the PPP Session - Fill in the start structure:

    memset(&Start, 0, sizeof(Start));
    Start.dwDevConfigSize = pLine->dwDevConfigSize;
    Start.lpbDevConfig  = pLine->pDevConfig;
    // Start.session       = NULL;                 // clear session pointer
    Start.notifierType  = 1;                    // RASDIALFUNC1 type
    Start.notifier      = PPPServerNotifierCallback; // notifier function
    // Start.rasDialParams = NULL;                  // No dial parameters since we are receiving calls, not making them
    Start.rasEntry      = &RasEntry;            // Pntr to the RASENTRY struct.
    Start.bIsServer     = TRUE;                 // We're a server listening for incoming calls

    //
    //  Note that if pppSessionNew is succesful then the new
    //  session will be exposed to applications via the RasEnumConnections
    //  API, and thus could be closed via a call to RasHangup at any time.
    //
    rc = pppSessionNew( &Start );

    if( SUCCESS != rc )
    {
        DEBUGMSG( ZONE_RAS | ZONE_ERROR, (TEXT( "PPP: ERROR PPPServerLineOpen - pppSessionNew failed\n" )));
        return rc;
    }

    //
    // Attach the line to the new session
    //
    pLine->pSession = Start.session;

    //
    //  Since we just told the app about the session with the above assign,
    //  the app could call RasHangup at any time.  Get a ref to the session
    //  to make sure that while we are starting things up it does not go
    //  away unexpectedly...
    //

    rc = pppServerLineOpenAndListen(Start.session);

    DEBUGMSG( ZONE_FUNCTION, ( TEXT( "PPP:-PPPServerLineOpen rc=%d\n" ),rc ));
    
    return rc;
}

DWORD
PPPServerAdjustLineState(
    IN  PPPServerLineConfiguration_t *pLine)
//
//  Based upon the current server and line enable settings,
//  and whether or not the line is currently opened,
//  open/close the line as necessary to make the line state
//  equal to the desired state.
//
{
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    DWORD   dwRetVal = SUCCESS;
    BOOL    bWantEnabled,
            bIsEnabled;

    bWantEnabled = pConfig->bEnable && pLine->bEnable;
    bIsEnabled = pLine->pSession != NULL;

    DEBUGMSG(ZONE_FUNCTION, (L"PPP: ServerAdjustLineState %s, Is=%d Want=%d\n", pLine->rasDevInfo.szDeviceName, bIsEnabled, bWantEnabled));
    if (bWantEnabled != bIsEnabled)
    {
        if (bWantEnabled)
        {
            // Refresh name server addresses
            PPPServerGetNameServerAddresses(NULL);

            // Want to enable a line which is currently disabled
            dwRetVal = PPPServerLineOpen(pLine);
        }
        else
        {
            // Want to disable a line which is currently enabled
            dwRetVal = PPPServerLineClose(pLine);
        }
    }
    DEBUGMSG(ZONE_ERROR && dwRetVal, (L"PPP: ERROR - ServerAdjustLineState %s, Is=%d Want=%d Result=%u\n", pLine->rasDevInfo.szDeviceName, bIsEnabled, bWantEnabled, dwRetVal));

    return dwRetVal;
}

DWORD 
PPPServerSetEnableState(
    BOOL bNewEnableState)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLineConfig;
    DWORD                         dwRetVal = SUCCESS;

    EnterCriticalSection( &PPPServerCritSec );
    if (pConfig->bEnable != bNewEnableState)
    {
        pConfig->bEnable = bNewEnableState;
        (void) WriteRegistryValues(
            HKEY_LOCAL_MACHINE,                 PPP_REGKEY_PARMS,
            RAS_PARMNAME_ENABLE,                REG_DWORD, 0, &pConfig->bEnable,                sizeof(pConfig->bEnable),
            NULL);

        if (bNewEnableState)
        {
            //
            // Startup the Arp Proxy Manager
            //
            ArpProxyManagerStart();

            //
            //  Attempt to register with DHCP
            //
            PPPServerRegisterWithDhcp();
        }

        //
        // Iterate through the list of lines and open/close each as appropriate for
        // the new server state.
        //
        for (pLineConfig = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
             pLineConfig != (PPPServerLineConfiguration_t *)&(pConfig->LineList);
             pLineConfig = (PPPServerLineConfiguration_t *)pLineConfig->listEntry.Flink)
        {
            (void) PPPServerAdjustLineState(pLineConfig);
        }
    }
    LeaveCriticalSection( &PPPServerCritSec );

    return dwRetVal;
}

DWORD
PPPServerLineEnable(
    IN  PPPServerLineConfiguration_t *pLine)
{
    DWORD   dwRetVal = SUCCESS;

    if (pLine->bEnable == FALSE)
    {
        pLine->bEnable = TRUE;
    
        PPPServerWriteLineConfigurationRegistrySettings(pLine);
        dwRetVal = PPPServerAdjustLineState(pLine);
        if (SUCCESS != dwRetVal)
        {
            // Unable to enable the line
            pLine->bEnable = FALSE;
        }
    }
    return dwRetVal;
}

DWORD
PPPServerLineDisable(
    IN  PPPServerLineConfiguration_t *pLine)
{
    DWORD   dwRetVal = SUCCESS;

    if (pLine->bEnable == TRUE)
    {
        pLine->bEnable = FALSE;
    
        PPPServerWriteLineConfigurationRegistrySettings(pLine);
        dwRetVal = PPPServerAdjustLineState(pLine);
    }
    
    return dwRetVal;
}

DWORD
PPPServerLineGetParameters(
    IN  PPPServerLineConfiguration_t    *pLine,
    OUT PRASCNTL_SERVERLINE             pBufOut,
    IN  DWORD                           dwLenOut,
    OUT PDWORD                          pdwActualOut)
{
    DWORD   dwRetVal = SUCCESS;
    DWORD   dwNeededSize;           

    // dwDevConfigSize will never be large enough to overflow the addition below.
    // Add this to help prefast understand that.
    PREFAST_ASSERT(pLine->dwDevConfigSize <= 100000);

    dwNeededSize = offsetof(RASCNTL_SERVERLINE, DevConfig) + pLine->dwDevConfigSize;

    if (dwNeededSize > dwLenOut)
    {
        dwRetVal = ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        pBufOut->rasDevInfo = pLine->rasDevInfo;
        pBufOut->bEnable = pLine->bEnable;
        pBufOut->bmFlags = pLine->bmFlags;
        pBufOut->DisconnectIdleSeconds = pLine->DisconnectIdleSeconds;
        pBufOut->dwDevConfigSize = pLine->dwDevConfigSize;
        memcpy(&pBufOut->DevConfig[0], pLine->pDevConfig, pLine->dwDevConfigSize);
    }
    if (pdwActualOut)
        *pdwActualOut = dwNeededSize;

    return dwRetVal;
}

DWORD
PPPServerLineSetParameters(
    IN  PPPServerLineConfiguration_t *pLine,
    IN  PRASCNTL_SERVERLINE           pBufIn,
    IN  DWORD                         dwLenIn)
{
    DWORD           dwRetVal = SUCCESS,
                    dwExpectedSize;
    PBYTE           pNewDevConfig = NULL;

    if (pBufIn->dwDevConfigSize > 0)
    {
        dwExpectedSize = offsetof(RASCNTL_SERVERLINE, DevConfig) + pBufIn->dwDevConfigSize;
        if (dwLenIn < dwExpectedSize)
            dwRetVal = ERROR_INVALID_PARAMETER;
        else
        {
            pNewDevConfig = pppAllocateMemory(pBufIn->dwDevConfigSize);
            if (pNewDevConfig == NULL)
                dwRetVal = ERROR_OUTOFMEMORY;
            else
                memcpy(pNewDevConfig, &pBufIn->DevConfig[0], pBufIn->dwDevConfigSize);
        }
    }

    if (dwRetVal == SUCCESS)
    {
        // Free the old dev config
        if (pLine->pDevConfig)
            pppFreeMemory(pLine->pDevConfig, pLine->dwDevConfigSize);

        pLine->bmFlags =    pBufIn->bmFlags;
        pLine->DisconnectIdleSeconds = pBufIn->DisconnectIdleSeconds;
        pLine->dwDevConfigSize = pBufIn->dwDevConfigSize;
        pLine->pDevConfig = pNewDevConfig;

        PPPServerWriteLineConfigurationRegistrySettings(pLine);

        PPPServerLineUpdateActiveSettings(pLine);
    }

    return dwRetVal;
}

DWORD
PPPServerLineGetConnectionInfo(
    IN  PPPServerLineConfiguration_t    *pLine,
    OUT PRASCNTL_SERVERCONNECTION       pBufOut,
    IN  DWORD                           dwLenOut,
    OUT PDWORD                          pdwActualOut)
{
    DWORD         dwRetVal = SUCCESS;
    DWORD         dwNeededSize;         
    pppSession_t *pSession = pLine->pSession;
    PAuthContext  pAuthContext;

    dwNeededSize = sizeof(RASCNTL_SERVERCONNECTION);

    if (dwNeededSize > dwLenOut)
    {
        dwRetVal = ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        memset(pBufOut, 0, dwNeededSize);
        pBufOut->rasDevInfo = pLine->rasDevInfo;
        pBufOut->hrasconn = (HRASCONN)pSession;
        pBufOut->dwServerIpAddress = pLine->IPAddrInfo[SERVER_INDEX].IPAddr;
        pBufOut->dwClientIpAddress = pLine->IPAddrInfo[CLIENT_INDEX].IPAddr;
        if (pSession)
        {
            pBufOut->RasConnState = pSession->RasConnState;
            pAuthContext = (PAuthContext)pSession->authCntxt;
            if (pAuthContext)
                wcscpy(pBufOut->tszUserName, pAuthContext->wszDomainAndUserName);
        }
    }
    if (pdwActualOut)
        *pdwActualOut = dwNeededSize;

    return dwRetVal;
}

BOOL
UserHasRasAccess(
    PWSTR wszUserName)
//
//  Determine whether this user is a member of the "RasUsers" group,
//  and thus allowed RAS access.
//
{
    DWORD dwResult;
    BOOL  bHasRasAccess = FALSE;
    PWCHAR mwszUserList, pwszUserName;
    DWORD  ccUserList = 0;


    BOOL fSuccess = NTLMGetUserList(RAS_USERS_GROUP_NAME, NULL, &ccUserList);
    dwResult = fSuccess ? ERROR_SUCCESS : GetLastError();

    if (dwResult == ERROR_SUCCESS)
    {   
        mwszUserList = LocalAlloc(0, ccUserList * sizeof(WCHAR));
        if (mwszUserList)
        {
            fSuccess = NTLMGetUserList(RAS_USERS_GROUP_NAME, mwszUserList, &ccUserList);
            dwResult = fSuccess ? ERROR_SUCCESS : GetLastError();

            if (dwResult == ERROR_SUCCESS)
            {
                //
                // Search through the multistring list of RAS users to see if
                // this user is on it, and thus allowed RAS access.
                //
                for (pwszUserName = mwszUserList;
                     pwszUserName[0] != L'\0'; 
                     pwszUserName += wcslen(pwszUserName) + 1)
                {
                    if (wcsicmp(wszUserName, pwszUserName) == 0)
                    {
                        bHasRasAccess = TRUE;
                        break;
                    }
                }
            }
            LocalFree(mwszUserList);
        }
    }

    DEBUGMSG(ZONE_ERROR && dwResult, (TEXT("PPPSERVER: ERROR - Unable to get RasUsers group list, error=%d\n"), dwResult));

    return bHasRasAccess;
}

BOOL 
PPPServerUserCredentialsGetByName(
    IN  PWCHAR                      wszDomainAndUserName,
    OUT PPPServerUserCredentials_t *pCredentials)
//
//  Find the user credentials given an input string of the form:
//      <DomainName>\<UserName>
//  
//  The <DomainName>\ is optional
//
{
    PWCHAR  pwszUserName;
    DWORD   dwDomainLen;
    BOOL    bFound;
    WCHAR   wszPassword[PWLEN + 1];
    DWORD   cbPassword;

    memset (pCredentials, 0, sizeof(*pCredentials));
    
    // Check that the user is allowed RAS access, that is,
    // the user is a member of the RAS GROUP
    if (!UserHasRasAccess(wszDomainAndUserName))
    {
        // Note that we don't want to provide an attacker with
        // any information, so we don't pass the error code
        // ERROR_NO_DIALIN_PERMISSION back, we just indicate
        // generic authentication failure errors to the client.
        bFound = FALSE;
    }
    else
    {
        pwszUserName = wcschr(wszDomainAndUserName, L'\\');
        if (pwszUserName == NULL)
        {
            // No domain name prefix, just a user name
            pwszUserName = wszDomainAndUserName;
        }
        else
        {
            // Domain name prefix exists
            dwDomainLen = pwszUserName - wszDomainAndUserName;
            if(dwDomainLen > DNLEN)
                dwDomainLen = DNLEN;

            wcsncpy(pCredentials->info.tszDomainName, wszDomainAndUserName, dwDomainLen);
            pCredentials->info.tszDomainName[DNLEN] = L'\0';
            // Skip over the '\' separator character
            pwszUserName++;
        }

        wcsncpy(pCredentials->info.tszUserName, pwszUserName, UNLEN);
        pCredentials->info.tszUserName[UNLEN]=L'\0';

        cbPassword = sizeof(wszPassword);
        bFound = NTLMGetUserInfo(wszDomainAndUserName, USERDB_DATA_TYPE_PASSWORD, (PBYTE)wszPassword, &cbPassword);
        if (bFound)
        {
            pCredentials->info.cbPassword = WideCharToMultiByte(CP_OEMCP, 0, wszPassword, -1, pCredentials->info.password, sizeof(pCredentials->info.password), NULL, NULL);
            // Remove trailing '\0' from count of password characters.
            if (pCredentials->info.cbPassword)
                pCredentials->info.cbPassword--;
        }
    }

    return bFound;
}

DWORD
PPPServerUserSetCredentials(
    IN  PRASCNTL_SERVERUSERCREDENTIALS  pBufIn,
    IN  DWORD                           dwLenIn)
{
    DWORD  dwResult;
    WCHAR  wszUser[DNLEN + 1 + UNLEN + 1];
    WCHAR  wszPassword[PWLEN + 1];

    if ((pBufIn == NULL)
    ||  (dwLenIn < sizeof(RASCNTL_SERVERUSERCREDENTIALS))
    ||  (pBufIn->tszUserName[0] == TEXT('\0'))  // check that username is at least 1 char
    ||  (wcslen(pBufIn->tszDomainName) > DNLEN)
    ||  (wcslen(pBufIn->tszUserName)   > UNLEN))
    {
        dwResult = ERROR_INVALID_PARAMETER;
    }
    else
    {
        // Enable saving of the user password in recoverable form
        NTLMSavePassword(TRUE);

        StringCchPrintfW(wszUser, COUNTOF(wszUser), L"%s%s%s", pBufIn->tszDomainName, pBufIn->tszDomainName[0] ? L"\\" : L"", pBufIn->tszUserName);

        memset(wszPassword, 0, sizeof(wszPassword));
        MultiByteToWideChar(CP_OEMCP, 0, pBufIn->password, pBufIn->cbPassword, wszPassword, PWLEN);

        // Save password
        if (NTLMSetUserInfo(wszUser, wszPassword))
        {
            //
            // Add the user to the group called "RasUsers"
            //

            // First, create the group
            BOOL fSuccess = NTLMAddGroup(RAS_USERS_GROUP_NAME);
            dwResult = fSuccess ? ERROR_SUCCESS : GetLastError();

            // If the group already existed, that's not an error
            if (dwResult == ERROR_ALREADY_EXISTS)
                dwResult = SUCCESS;

            if (dwResult == SUCCESS)
            {
                fSuccess = NTLMAddUserToGroup(RAS_USERS_GROUP_NAME, wszUser);
                dwResult = fSuccess ? ERROR_SUCCESS : GetLastError();

                // If the user was already in the group, that's not an error
                if (dwResult == ERROR_ALREADY_EXISTS)
                    dwResult = SUCCESS;
            }

            //
            // If we were unable to add the user to the RasUsers group,
            // we don't want to leave part of the user info (i.e. the name, password)
            // around. If this user info existed in the database prior to this
            // function call, we don't want to delete it. Currently, we don't know
            // that so to be safe we expunge the user from the database.
            //
            if (dwResult != SUCCESS)
            {
                NTLMDeleteUser(wszUser);
            }
        }
        else
        {
            dwResult = ERROR_BAD_USERNAME;
        }

    }

    return dwResult;
}

DWORD
PPPServerUserDeleteCredentials(
    IN  PRASCNTL_SERVERUSERCREDENTIALS  pBufIn,
    IN  DWORD                           dwLenIn)
{
    DWORD                        dwResult = SUCCESS;
    WCHAR                        wszUser[DNLEN + 1 + UNLEN + 1];

    if ((pBufIn == NULL)
    ||  (dwLenIn < sizeof(RASCNTL_SERVERUSERCREDENTIALS))
    ||  (pBufIn->tszUserName[0] == TEXT('\0'))  // check that username is at least 1 char
    ||  (wcslen(pBufIn->tszDomainName) > DNLEN)
    ||  (wcslen(pBufIn->tszUserName)   > UNLEN))
    {
        dwResult = ERROR_INVALID_PARAMETER;
    }
    else
    {
        StringCchPrintfW(wszUser, COUNTOF(wszUser), L"%s%s%s", pBufIn->tszDomainName, pBufIn->tszDomainName[0] ? L"\\" : L"", pBufIn->tszUserName);

        if (NTLMDeleteUser(wszUser))
        {
            dwResult = SUCCESS;
        }
        else
        {
            dwResult = ERROR_BAD_USERNAME;
        }
    }

    return dwResult;
}


DWORD
PPPServerLineIoCtl(
    IN  DWORD                 dwCode,
    IN  PRASCNTL_SERVERLINE   pBufIn,
    IN  DWORD                 dwLenIn,
    OUT PRASCNTL_SERVERLINE   pBufOut,
    IN  DWORD                 dwLenOut,
    OUT PDWORD                pdwActualOut)
//
//  Perform an IOCTL on an existing line
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine;
    DWORD                        dwRetVal;

    EnterCriticalSection( &PPPServerCritSec );
    if (dwLenIn < offsetof(RASCNTL_SERVERLINE, DevConfig))
    {
        dwRetVal = ERROR_INVALID_PARAMETER;
    }
    else if (pLine = PPPServerFindLineConfig(&pConfig->LineList, &pBufIn->rasDevInfo))
    {
        switch(dwCode)
        {
        case RASCNTL_SERVER_LINE_REMOVE :
            dwRetVal = PPPServerLineRemove(pLine);
            break;

        case RASCNTL_SERVER_LINE_ENABLE :
            dwRetVal = PPPServerLineEnable(pLine);
            break;

        case RASCNTL_SERVER_LINE_DISABLE :
            dwRetVal = PPPServerLineDisable(pLine);
            break;

        case RASCNTL_SERVER_LINE_GET_PARAMETERS :
            dwRetVal = PPPServerLineGetParameters(pLine, (PRASCNTL_SERVERLINE)pBufOut, dwLenOut, pdwActualOut);
            break;

        case RASCNTL_SERVER_LINE_SET_PARAMETERS :
            dwRetVal = PPPServerLineSetParameters(pLine, pBufIn, dwLenIn);
            break;

        case RASCNTL_SERVER_LINE_GET_CONNECTION_INFO :
            dwRetVal = PPPServerLineGetConnectionInfo(pLine, (PRASCNTL_SERVERCONNECTION)pBufOut, dwLenOut, pdwActualOut);
            break;

        default:
            ASSERT(FALSE);
            dwRetVal = ERROR_INVALID_PARAMETER;
            break;
        }
    }
    else
    {
        dwRetVal = ERROR_DEVICENAME_NOT_FOUND;
    }
    LeaveCriticalSection( &PPPServerCritSec );
    return dwRetVal;
}

DWORD
PPPServerGetSessionIPAddress(
    pppSession_t *pSession,
    DWORD         dwType)
{
    DWORD   dwIpAddr = 0;
    PPPServerLineConfiguration_t *pLine;

    pLine = PPPServerFindLineWithSession(pSession);
    ASSERT(pLine);
    if (pLine)
    {
        dwIpAddr = pLine->IPAddrInfo[dwType].IPAddr;
    }
    return dwIpAddr;
}

DWORD
PPPServerGetSessionServerIPAddress(
    pppSession_t *pSession)
//
//  Return the IP address allocated for the server on the line
//  for this session.
//
{
    return PPPServerGetSessionIPAddress(pSession, SERVER_INDEX);
}

DWORD
PPPServerGetSessionClientIPAddress(
    pppSession_t *pSession)
//
//  Return the IP address allocated for the client on the line
//  for this session.
//
{
    return PPPServerGetSessionIPAddress(pSession, CLIENT_INDEX);
}

DWORD
PPPServerGetSessionIPMask(
    pppSession_t *pSession)
//
//  Typically return 0xFFFFFFFF because we do not
//  want to add a subnet for a client connection. That is,
//  the only packets that the server should send through
//  the client PPP interface are those directed to the
//  specific IP address of the client, not all those
//  packets on the same subnet.
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    DWORD                         dwIpMask = 0xFFFFFFFF;
    PPPServerLineConfiguration_t *pLine;

    pLine = PPPServerFindLineWithSession(pSession);
    if (pLine
    && ((pConfig->bmFlags | pLine->bmFlags) & PPPSRV_FLAG_ADD_CLIENT_SUBNET))
    {
        dwIpMask = pLine->IPAddrInfo[SERVER_INDEX].IPMask;
    }

    return dwIpMask;
}

void
pppServerGenerateIPV6NetPrefix(
    IN  DWORD  offset,
    OUT PBYTE  pPrefix,
    OUT PDWORD pPrefixBitLength)
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PBYTE  pCurPrefixByte;
    DWORD  sum;

    // First set the base prefix value
    memcpy(pPrefix, pConfig->IPV6NetPrefix, sizeof(pConfig->IPV6NetPrefix));
    *pPrefixBitLength = pConfig->IPV6NetPrefixBitLength;

    //
    // Now add offset to the base prefix value
    //
    pCurPrefixByte = pPrefix + (pConfig->IPV6NetPrefixBitLength + 7 ) / 8;
    offset <<= (8 - (pConfig->IPV6NetPrefixBitLength % 8)) % 8;

    while (offset)
    {
        if (pCurPrefixByte <= pPrefix)
        {
            // Insufficient room to add 'offset' without overflowing
            // beyond the MSB of prefix
            *pPrefixBitLength = 0;
            break;
        }
        pCurPrefixByte--;

        sum = *pCurPrefixByte + offset;
        *pCurPrefixByte = (BYTE)sum;
        offset = (offset >> 8) + (sum >> 8);
    }
}

void
pppServerAllocateIPV6NetPrefix(
    IN  OUT PPPServerLineConfiguration_t *pRequestingLine)
//
//  Allocate an IPV6 network prefix that is not currently in use
//  and assign it to the specified line.
//
{
    PPPServerConfiguration_t     *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t *pLine;
    DWORD count;
    BOOL  bInUse;
    BYTE  newPrefix[16];
    DWORD newPrefixBitLength;

    for (count = 0; count < pConfig->IPV6NetCount; count++)
    {
        pppServerGenerateIPV6NetPrefix(count, &newPrefix[0], &newPrefixBitLength);

        if (0 == newPrefixBitLength)
            break;

        bInUse = FALSE;
        for (pLine  = (PPPServerLineConfiguration_t *)(pConfig->LineList.Flink);
            pLine != (PPPServerLineConfiguration_t *)(&pConfig->LineList);
            pLine  = (PPPServerLineConfiguration_t *)pLine->listEntry.Flink)
        {
            if (pLine != pRequestingLine
            &&  pLine->IPV6NetPrefixBitLength == newPrefixBitLength
            &&  0 == memcmp(pLine->IPV6NetPrefix, newPrefix, 16))
            {
                // Address in use, try another.
                bInUse = TRUE;
                break;
            }
        }

        if (!bInUse)
        {
            memcpy(pRequestingLine->IPV6NetPrefix, newPrefix, sizeof(pRequestingLine->IPV6NetPrefix));
            pRequestingLine->IPV6NetPrefixBitLength = newPrefixBitLength;
            break;
        }
    }
}

void
PPPServerGetSessionIPV6NetPrefix(
    IN  pppSession_t *pSession,
    OUT PBYTE         pPrefix,
    OUT PDWORD        pPrefixBitLength)
//
//  Assign an unused IPV6 network prefix for use by the session.
//  If unable to assign one, *pPrefixBitLength will be set to 0.
//
{
    DWORD   dwIpAddr = 0;
    PPPServerLineConfiguration_t *pLine;

    *pPrefixBitLength = 0;
    pLine = PPPServerFindLineWithSession(pSession);
    ASSERT(pLine);
    if (pLine)
    {
        // First check to see if a prefix has already been assigned to the line
        if (0 == pLine->IPV6NetPrefixBitLength)
        {
            // No current prefix, try to get one
            pppServerAllocateIPV6NetPrefix(pLine);
        }

        *pPrefixBitLength = pLine->IPV6NetPrefixBitLength;
        memcpy(pPrefix, &pLine->IPV6NetPrefix[0], sizeof(pLine->IPV6NetPrefix));
    }
}

BOOL
PPPServerGetSessionAuthenticationRequired(
    pppSession_t *pSession)
//
//  Return TRUE if authentication is mandatory for the session.
//
{
    PPPServerConfiguration_t        *pConfig = &PPPServerConfig;
    PPPServerLineConfiguration_t    *pLine;
    DWORD                           bmFlags;
    BOOL                            bAuthenticationRequired;

    pLine = PPPServerFindLineWithSession(pSession);
    ASSERT(pLine);
    if (!pLine)
        return FALSE;

    bmFlags = pConfig->bmFlags | pLine->bmFlags;

    if (bmFlags & PPPSRV_FLAG_REQUIRE_DATA_ENCRYPTION)
    {
        // Need MSCHAP v1 or v2 to derived encryption session key
        bAuthenticationRequired = TRUE;
    }
    else if (bmFlags & PPPSRV_FLAG_ALLOW_UNAUTHENTICATED_ACCESS)
    {
        bAuthenticationRequired = FALSE;
    }
    else if (pConfig->bmAuthenticationMethods ==
                   (RASEO_ProhibitPAP |
                    RASEO_ProhibitCHAP |
                    RASEO_ProhibitMsCHAP |
                    RASEO_ProhibitMsCHAP2 |
                    RASEO_ProhibitEAP))
    {
        // All authentication methods disabled
        bAuthenticationRequired = FALSE;
    }
    else
    {
        bAuthenticationRequired = TRUE;
    }

    return bAuthenticationRequired;
}

DWORD
format_inet_addr(char *pszIpAddr)
//
//  Convert a dot notation IP address a.b.c.d to a 32 bit value.
//
//  Returns 0 if unable to successfully parse the address.
//
{
    DWORD   dwAddr[4];
    DWORD   dwIpAddr = 0;
    
    if (sscanf(pszIpAddr, "%u.%u.%u.%u", &dwAddr[0], &dwAddr[1], &dwAddr[2], &dwAddr[3]) == 4)
    {
        // Could verify the each component is <= 255
        dwIpAddr = (dwAddr[0] << 24) | (dwAddr[1] << 16) | (dwAddr[2] << 8) | dwAddr[3];
    }

    return dwIpAddr;
}

//
//  These variables store the most recently obtained name server IP addresses.
//
DWORD   g_dwDNSIpAddr[2];
DWORD   g_dwWINSIpAddr[2];

DWORD
GetTable(
    DWORD   (*pGetTableFunc)(),
    void    **ppTable,
    PDWORD  pdwSize)
//
//  Common function to retrieve a dynamically sized table.
//
{
    DWORD   dwResult, dwSize;

    // First call is just to get required size
    dwSize = 0;
    dwResult = pGetTableFunc(NULL, &dwSize);

    // Allocate space for the table
    *ppTable = pppAllocateMemory(dwSize);

    if (*ppTable)
    {
        // Now do the real call
        *pdwSize = dwSize;
        dwResult = pGetTableFunc(*ppTable, pdwSize);
        if (dwResult != NO_ERROR)
        {
            pppFreeMemory(*ppTable, dwSize);
            *ppTable = NULL;
        }
    }
    else
    {
        dwResult = ERROR_OUTOFMEMORY;
    }
    return dwResult;
}

DWORD
PPPServerGetNameServerAddresses(
    PVOID unused)
//
//  This function attempts to retrieve the DNS and WINS server
//  addresses.  Addresses that it is unable to obtain will be
//  unmodified.
//
{
    DWORD               dwResult, dwSize;
    PFIXED_INFO         pFixedInfo;
    PIP_ADDR_STRING     pIpAddr;
    PIP_ADAPTER_INFO    pAdapterInfo;
    int                 i;

    // Get DNS server addresses if possible

    if (g_pfnGetNetworkParams)
    {
        dwResult = GetTable(g_pfnGetNetworkParams, &pFixedInfo, &dwSize);

        if (dwResult == ERROR_SUCCESS)
        {
            // Extract DNS server addresses

            EnterCriticalSection( &PPPServerCritSec );

            for (pIpAddr = &pFixedInfo->DnsServerList, i = 0;
                 pIpAddr && pIpAddr->IpAddress.String[0] && (i < COUNTOF(g_dwDNSIpAddr));
                 pIpAddr = pIpAddr->Next, i++)
            {
                g_dwDNSIpAddr[i] = format_inet_addr(pIpAddr->IpAddress.String);
            }

            LeaveCriticalSection( &PPPServerCritSec );

            pppFreeMemory(pFixedInfo, dwSize);
        }
    }

    // Get WINS addresses if possible

    if (g_pfnGetAdaptersInfo)
    {
        dwResult = GetTable(g_pfnGetAdaptersInfo, &pAdapterInfo, &dwSize);

        if (dwResult == ERROR_SUCCESS)
        {
            EnterCriticalSection( &PPPServerCritSec );
            // Extract WINS server addresses
            g_dwWINSIpAddr[0] = format_inet_addr(pAdapterInfo->PrimaryWinsServer.IpAddress.String);
            g_dwWINSIpAddr[1] = format_inet_addr(pAdapterInfo->SecondaryWinsServer.IpAddress.String);

            LeaveCriticalSection( &PPPServerCritSec );
            pppFreeMemory(pAdapterInfo, dwSize);
        }
    }

    return NO_ERROR;
}

void
PPPServerReadRegistrySettings(
    PPPServerConfiguration_t *pConfig)
{
    DWORD cbNetPrefix = sizeof(pConfig->IPV6NetPrefix);

    // Read in registry settings where provided to override default settings

    (void) ReadRegistryValues(
            HKEY_LOCAL_MACHINE,                 PPP_REGKEY_PARMS,
            RAS_PARMNAME_STATIC_IP_ADDR_START,  REG_DWORD, 0, &pConfig->dwStaticIpAddrStart,    sizeof(pConfig->dwStaticIpAddrStart),
            RAS_PARMNAME_STATIC_IP_ADDR_COUNT,  REG_DWORD, 0, &pConfig->dwStaticIpAddrCount,    sizeof(pConfig->dwStaticIpAddrCount),
            RAS_PARMNAME_AUTHENTICATION_METHODS,REG_DWORD, 0, &pConfig->bmAuthenticationMethods,sizeof(pConfig->bmAuthenticationMethods),
            RAS_PARMNAME_ENABLE,                REG_DWORD, 0, &pConfig->bEnable,                sizeof(pConfig->bEnable),
            RAS_PARMNAME_FLAGS,                 REG_DWORD, 0, &pConfig->bmFlags,                sizeof(pConfig->bmFlags),
            RAS_PARMNAME_USE_AUTOIP_ADDRESSES,  REG_DWORD, 0, &pConfig->bUseAutoIpAddresses,    sizeof(pConfig->bUseAutoIpAddresses),
            RAS_PARMNAME_AUTOIP_SUBNET,         REG_DWORD, 0, &pConfig->dwAutoIpSubnet,         sizeof(pConfig->dwAutoIpSubnet),
            RAS_PARMNAME_AUTOIP_SUBNET_MASK,    REG_DWORD, 0, &pConfig->dwAutoIpSubnetMask,     sizeof(pConfig->dwAutoIpSubnetMask),
            RAS_PARMNAME_USE_DHCP_ADDRESSES,    REG_DWORD, 0, &pConfig->bUseDhcpAddresses,      sizeof(pConfig->bUseDhcpAddresses),
            RAS_PARMNAME_DHCP_IF_NAME,          REG_SZ,    0, &pConfig->wszDhcpInterface[0],    (wcslen(pConfig->wszDhcpInterface)+1) * sizeof(WCHAR),
            RAS_PARMNAME_DHCP_TIMEOUT_MS,       REG_DWORD, 0, &pConfig->DhcpTimeoutMs,          sizeof(pConfig->DhcpTimeoutMs),
            RAS_PARMNAME_DHCP_MAX_TIMEOUTS,     REG_DWORD, 0, &pConfig->DhcpMaxTimeouts,        sizeof(pConfig->DhcpMaxTimeouts),
            RAS_PARMNAME_AUTH_FAIL_HOLD_SECONDS,REG_DWORD, 0, &pConfig->AuthFailHoldSeconds,    sizeof(pConfig->AuthFailHoldSeconds),
            RAS_PARMNAME_AUTH_FAIL_MAX,         REG_DWORD, 0, &pConfig->AuthMaxConsecutiveFail, sizeof(pConfig->AuthMaxConsecutiveFail),
            RAS_PARMNAME_IPV6_NET_PREFIX,       REG_BINARY,0, &pConfig->IPV6NetPrefix[0],      &cbNetPrefix,
            RAS_PARMNAME_IPV6_NET_PREFIX_LEN,   REG_DWORD, 0, &pConfig->IPV6NetPrefixBitLength, sizeof(DWORD),
            RAS_PARMNAME_IPV6_NET_COUNT,        REG_DWORD, 0, &pConfig->IPV6NetCount,          sizeof(DWORD),
            NULL);

    pConfig->bmAuthenticationMethods &= RASEO_ProhibitPAP |
                                        RASEO_ProhibitCHAP |
                                        RASEO_ProhibitMsCHAP |
                                        RASEO_ProhibitMsCHAP2 |
                                        RASEO_ProhibitEAP;

    PPPServerReadLineConfigurationRegistrySettings(&pConfig->LineList);
}

DWORD
PPPServerEnableThread(
    PVOID unused)
{
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    DWORD dwPrevLineCount,
          dwNewLineCount,
          dwUnchangedSeconds,
          dwStartupDelaySeconds;

    dwPrevLineCount = 0;
    dwUnchangedSeconds = 0;

    dwStartupDelaySeconds = PPP_SERVER_DEFAULT_STARTUP_DELAY_SECONDS;

    (void) ReadRegistryValues(
        HKEY_LOCAL_MACHINE,     PPP_REGKEY_PARMS,
        L"StartupDelaySeconds", REG_DWORD, 0, &dwStartupDelaySeconds, sizeof(dwStartupDelaySeconds),
        NULL);

    // Wait for miniports to complete any initialization
    while (TRUE)
    {
        Sleep(1000);
        if (pConfig->bEnable == FALSE)
        {
            // Disabled while still initializing, abort
            return 0;
        }

        dwNewLineCount = g_dwTotalLineCount;
        if (dwNewLineCount == dwPrevLineCount)
            dwUnchangedSeconds++;
        else
            dwUnchangedSeconds = 0;

        if (dwUnchangedSeconds >= dwStartupDelaySeconds)
            break;

        dwPrevLineCount = dwNewLineCount;
    }

    DEBUGMSG(ZONE_INIT, (L"PPP: Enabling Server\n"));

    if (pConfig->bEnable)
    {
        // Startup the server by toggling the enable flag
        pConfig->bEnable = FALSE;
        (void)PPPServerSetEnableState(TRUE);
    }
    return 0;
}

void
PPPServerInitialize()
//
//  Called once during system startup to initialize global variables for
//  the PPP server.
//
{
    PPPServerConfiguration_t *pConfig = &PPPServerConfig;
    DWORD                     dwThreadID;
    HANDLE                    hEnableThread;


    InitializeCriticalSection( &PPPServerCritSec );


    // First initialize default settings

    pConfig->bEnable                        = FALSE;
    pConfig->bmFlags                        = 0;
    pConfig->dwStaticIpAddrStart            = DEFAULT_STATIC_IP_ADDRESS_START;
    pConfig->dwStaticIpAddrCount            = DEFAULT_STATIC_IP_ADDRESS_COUNT;
    pConfig->bmAuthenticationMethods        = RASEO_ProhibitEAP;
    pConfig->bUseAutoIpAddresses            = TRUE;
    pConfig->dwAutoIpSubnet                 = 0xC0A80000; // 192.168.x.x
    pConfig->dwAutoIpSubnetMask             = 0xFFFF0000; // 255.255.0.0
    pConfig->bUseDhcpAddresses              = FALSE;
    pConfig->wszDhcpInterface[0]            = L'\0';
    pConfig->AuthMaxConsecutiveFail         = 3;
    pConfig->AuthFailHoldSeconds            = 30;
    pConfig->DhcpTimeoutMs                  = 30000;
    pConfig->DhcpMaxTimeouts                = 3;

    InitializeListHead(&pConfig->LineList);

    ArpProxyManagerInitialize();

    (void)CXUtilGetProcAddresses(TEXT("iphlpapi.dll"), &g_hIpHlpApiMod,
                TEXT("CreateIpForwardEntry"),  &g_pfnCreateIpForwardEntry,
                TEXT("CreateProxyArpEntry"),   &g_pfnCreateProxyArpEntry,
                TEXT("DeleteIpForwardEntry"),  &g_pfnDeleteIpForwardEntry,
                TEXT("DeleteProxyArpEntry"),   &g_pfnDeleteProxyArpEntry,
                TEXT("GetAdaptersInfo"),       &g_pfnGetAdaptersInfo,
                TEXT("GetAdapterIndex"),       &g_pfnGetAdapterIndex,
                TEXT("GetIfEntry"),            &g_pfnGetIfEntry,
                TEXT("GetInterfaceInfo"),      &g_pfnGetInterfaceInfo,
                TEXT("GetIpAddrTable"),        &g_pfnGetIpAddrTable,
                TEXT("GetIpForwardTable"),     &g_pfnGetIpForwardTable,
                TEXT("GetIpInterfaceEntry"),   &g_pfnGetIpInterfaceEntry,
                TEXT("GetNetworkParams"),      &g_pfnGetNetworkParams,
                TEXT("InitializeIpInterfaceEntry"),&g_pfnInitializeIpInterfaceEntry,
                TEXT("NotifyAddrChange"),      &g_pfnNotifyAddrChange,
                TEXT("SendARP"),               &g_pfnSendARP,
                TEXT("SetIpInterfaceEntry"),   &g_pfnSetIpInterfaceEntry,
                NULL);

#if 0 // Don't currently support this feature
    (void)CXUtilGetProcAddresses(TEXT("tcpstk.dll"), &g_hTcpIpMod,
        TEXT("CESetDHCPNTEByName"), &g_pfnCESetDHCPNTEByName,
        NULL);
#endif

    //
    // Now read in registry settings to update the defaults
    //

    PPPServerReadRegistrySettings(pConfig);

    //
    // Spawn thread to perform initial line opening for enabled lines
    //
    if (pConfig->bEnable)
    {
        hEnableThread = CreateThread(NULL, 0, PPPServerEnableThread, NULL, 0, &dwThreadID);
        if (hEnableThread)
            CloseHandle(hEnableThread);
    }
}


