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

    optionmgr.c

Abstract:

    Options Manager for DhcpV6 client.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
//#include "precomp.h"
//#include "optionmgr.tmh"



DWORD
IniDhcpV6OptionMgrFindOption(
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule,
    DHCPV6_OPTION_TYPE DhcpV6OptionType,
    PDHCPV6_OPTION *ppDhcpV6Option
    )
{
    DWORD dwError = STATUS_NOT_FOUND;
    PDHCPV6_OPTION pDhcpV6Option = pDhcpV6OptionModule->OptionsTable;
    ULONG uIndex = 0;


    for (uIndex = 0; uIndex < DHCPV6_MAX_OPTIONS; uIndex++, pDhcpV6Option++) {
        if (pDhcpV6Option->DhcpV6OptionType == DhcpV6OptionType) {
            dwError = ERROR_SUCCESS;
            *ppDhcpV6Option = pDhcpV6Option;
            break;
        }
    }

    return dwError;
}


DWORD
IniDhcpV6OptionMgrSetOption(
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule,
    DHCPV6_OPTION_TYPE DhcpV6OptionType,
    BOOL bEnabled,
    BOOL bRequired
    )
{
    DWORD dwError = 0;
    PDHCPV6_OPTION pDhcpV6Option = NULL;
    ULONG uLength = 0;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Set Option Type: %d, Enabled: %d", DhcpV6OptionType, bEnabled));

    dwError = IniDhcpV6OptionMgrFindOption(
                    pDhcpV6OptionModule,
                    DhcpV6OptionType,
                    &pDhcpV6Option
                    );
    BAIL_ON_WIN32_ERROR(dwError);

    if (! bEnabled)
        bRequired = FALSE;
    
    pDhcpV6Option->bRequired = bRequired;
    if (bEnabled == pDhcpV6Option->bEnabled) {
        goto error;
    }

    pDhcpV6Option->bEnabled = bEnabled;
    if (bEnabled) {
        DHCPV6_INCREMENT(pDhcpV6OptionModule->uNumOfOptionsEnabled);
    } else {
        DHCPV6_DECREMENT(pDhcpV6OptionModule->uNumOfOptionsEnabled);
    }

error:

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Set Option with Error: %!status!", dwError));

    return dwError;
}


DWORD
IniDhcpV6OptionMgrLoadOptions(
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Load Option"));
    // NOTE: Read from registry later when there may be more types
    //       For now hardcode
#ifdef UNDER_CE
    dwError = IniDhcpV6OptionMgrSetOption(pDhcpV6OptionModule, 
        DHCPV6_OPTION_TYPE_DNS_SERVERS, TRUE, TRUE);
    dwError = IniDhcpV6OptionMgrSetOption(pDhcpV6OptionModule, 
        DHCPV6_OPTION_TYPE_DOMAIN_LIST, TRUE, FALSE);
    if (gbDHCPV6PDEnabled) {
        dwError = IniDhcpV6OptionMgrSetOption(pDhcpV6OptionModule, 
            DHCPV6_OPTION_TYPE_IA_PD, TRUE, FALSE);
        dwError = IniDhcpV6OptionMgrSetOption(pDhcpV6OptionModule, 
            DHCPV6_OPTION_TYPE_CLIENTID, TRUE, FALSE);
        dwError = IniDhcpV6OptionMgrSetOption(pDhcpV6OptionModule, 
            DHCPV6_OPTION_TYPE_SERVERID, TRUE, FALSE);
    }
#else
    dwError = IniDhcpV6OptionMgrSetOption(pDhcpV6OptionModule, DHCPV6_OPTION_TYPE_DNS, TRUE);
#endif
    BAIL_ON_WIN32_ERROR(dwError);

error:

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Load Option with Error: %!status!", dwError));

    return dwError;
}


DWORD
InitDhcpV6OptionMgr(
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule, BOOL UsePD
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Initializing Option Module"));

    memset(pDhcpV6OptionModule, 0, sizeof(DHCPV6_OPTION_MODULE));

    // Initialize the Options Table
#ifdef UNDER_CE
    // note: OptionsTable[i].bEnabled & .bRequired are already memset to FALSE
    pDhcpV6OptionModule->OptionsTable[0].DhcpV6OptionType = DHCPV6_OPTION_TYPE_DNS_SERVERS;
    pDhcpV6OptionModule->OptionsTable[0].pDhcpV6OptionProcessRecv = DhcpV6OptionMgrProcessRecvDNS;

    pDhcpV6OptionModule->OptionsTable[1].DhcpV6OptionType = DHCPV6_OPTION_TYPE_DOMAIN_LIST;
    pDhcpV6OptionModule->OptionsTable[1].pDhcpV6OptionProcessRecv = DhcpV6OptionMgrProcessRecvDomainList;

    if (UsePD) {
        pDhcpV6OptionModule->OptionsTable[2].DhcpV6OptionType = 
            DHCPV6_OPTION_TYPE_IA_PD;
        pDhcpV6OptionModule->OptionsTable[2].pDhcpV6OptionProcessRecv = 
            DhcpV6OptionMgrProcessRecvPD;
    }
    
    pDhcpV6OptionModule->OptionsTable[3].DhcpV6OptionType = DHCPV6_OPTION_TYPE_CLIENTID;
    pDhcpV6OptionModule->OptionsTable[3].pDhcpV6OptionProcessRecv = DhcpV6OptionMgrProcessRecvClientID;

    pDhcpV6OptionModule->OptionsTable[4].DhcpV6OptionType = DHCPV6_OPTION_TYPE_SERVERID;
    pDhcpV6OptionModule->OptionsTable[4].pDhcpV6OptionProcessRecv = DhcpV6OptionMgrProcessRecvServerID;
    
#else

    pDhcpV6OptionModule->uNumOfOptionsEnabled = 0;

    pDhcpV6OptionModule->OptionsTable[0].DhcpV6OptionType = DHCPV6_OPTION_TYPE_DNS;
    pDhcpV6OptionModule->OptionsTable[0].bEnabled = FALSE;
    pDhcpV6OptionModule->OptionsTable[0].pDhcpV6OptionProcessRecv = DhcpV6OptionMgrProcessRecvDNS;
#endif

    dwError = IniDhcpV6OptionMgrLoadOptions(pDhcpV6OptionModule);
    BAIL_ON_WIN32_ERROR(dwError);

error:
    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Option Module with Error: %!status!", dwError));

    return dwError;
}


DWORD
DeInitDhcpV6OptionMgr(
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin DeInitializing Option Module"));

    memset(pDhcpV6OptionModule, 0, sizeof(DHCPV6_OPTION_MODULE));

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End DeInitializing Option Module"));

    return dwError;
}


DWORD
DhcpV6OptionMgrCreateOptionRequestPD(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule,
    PUCHAR pucOptionMessageBuffer,
    USHORT usOptionLength
    )
{
    DWORD   dwError = 0;
    USHORT  cLen;
    USHORT  *psCurLoc = (USHORT *)pucOptionMessageBuffer;
    DHCPV6_IA_PD_OPTION PacketPD;
    

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Create Option Request"));

    // note: handle increments separately from the adds, I think some of the
    // compilers didn't handle it correctly 

    // let's do ClientID first

    cLen = (USHORT)pDhcpV6Adapt->cPhysicalAddr;
    ASSERT(0 == (cLen & 1));
    cLen &= 0xfffe;
    cLen += 4;  // add 2 for DUID-type + 2 for hardware type

    *psCurLoc = htons(DHCPV6_OPTION_TYPE_CLIENTID);
    psCurLoc++;
    *psCurLoc = htons(cLen);
    psCurLoc++;
    
    *psCurLoc = htons(3);  // DUID-type LL link-layer address
    psCurLoc++;
    *psCurLoc = htons(1);   // hardware type
    psCurLoc++;
    memcpy(psCurLoc, pDhcpV6Adapt->PhysicalAddr, pDhcpV6Adapt->cPhysicalAddr);
    psCurLoc += pDhcpV6Adapt->cPhysicalAddr >> 1;

    if (pDhcpV6Adapt->pServerID &&
        ( (dhcpv6_state_srequest == pDhcpV6Adapt->DhcpV6State) ||
        (dhcpv6_state_T1 ==  pDhcpV6Adapt->DhcpV6State))) {
        // do the ServerID

        cLen = pDhcpV6Adapt->cServerID;
        ASSERT(0 == (cLen & 1));
        
        *psCurLoc = htons(DHCPV6_OPTION_TYPE_SERVERID);
        psCurLoc++;
        *psCurLoc = htons(cLen);
        psCurLoc++;
        
        memcpy(psCurLoc, pDhcpV6Adapt->pServerID, cLen);
        psCurLoc += cLen >> 1;   
    }

    // handle ORO option
    //ASSERT(usOptionLength == 4);
    
    *psCurLoc = htons(DHCPV6_OPTION_TYPE_ORO);
    psCurLoc++;
    *psCurLoc = htons(4);
    psCurLoc++;
    *psCurLoc = htons(DHCPV6_OPTION_TYPE_DNS_SERVERS);
    psCurLoc++;
    *psCurLoc = htons(DHCPV6_OPTION_TYPE_IA_PD);
    psCurLoc++;

    // handle elapsed time option
    *psCurLoc = htons(DHCPV6_OPTION_TYPE_ELAPSED_TIME);
    psCurLoc++;
    *psCurLoc = htons(2);
    psCurLoc++;
    *psCurLoc = 0;
    psCurLoc++;

    // handle the IA_PD option
    *psCurLoc = htons(DHCPV6_OPTION_TYPE_IA_PD);
    psCurLoc++;
    
    if ((dhcpv6_state_srequest == pDhcpV6Adapt->DhcpV6State) ||
        (dhcpv6_state_T1 == pDhcpV6Adapt->DhcpV6State) ||
        (dhcpv6_state_T2 == pDhcpV6Adapt->DhcpV6State) ||
        (dhcpv6_state_rebindconfirm== pDhcpV6Adapt->DhcpV6State)){
        
        DHCPV6_IA_PREFIX_OPTION PacketPrefix;
        DHCPV6_IA_PREFIX        *pPrefix;

        *psCurLoc =  htons((IA_PD_OPTION_LEN + IA_PREFIX_OPTION_LEN + 4));
        psCurLoc++;

        // perhaps we should just store it in network order!
        PacketPD.IAID = ntohl(pDhcpV6Adapt->pPdOption->IAID);
        PacketPD.T1 = ntohl(pDhcpV6Adapt->pPdOption->T1);
        PacketPD.T2 = ntohl(pDhcpV6Adapt->pPdOption->T2);
        memcpy(psCurLoc, &(PacketPD.IAID), IA_PD_OPTION_LEN);
        psCurLoc += 6;

        // now do prefix stuff
        pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;

        PacketPrefix.OptionCode = htons(DHCPV6_OPTION_TYPE_IA_PREFIX);
        PacketPrefix.OptionLen = htons(IA_PREFIX_OPTION_LEN);
        PacketPrefix.PreferredLifetime = htonl(pPrefix->PreferedLifetime);
        PacketPrefix.ValidLifetime = htonl(pPrefix->ValidLifetime);
        PacketPrefix.PrefixLength = pPrefix->cPrefix;
        memcpy(&(PacketPrefix.IPv6Prefix), &pPrefix->PrefixAddr, 
            sizeof(IN6_ADDR));

        // this time we're copying the header too so add 4
        memcpy(psCurLoc, &PacketPrefix, IA_PREFIX_OPTION_LEN + 4);
        // should we update psCurLoc's location?
        
    } else {
        *psCurLoc = htons(IA_PD_OPTION_LEN);
        psCurLoc++;
    }

//error:

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Creating Option Request with Error: %!status!", dwError));

    return dwError;
}


DWORD
DhcpV6OptionMgrCreateOptionRequest(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule,
    PUCHAR pucOptionMessageBuffer,
    USHORT usOptionLength
    )
{
    DWORD dwError = 0;
    PDHCPV6_OPTION pDhcpV6OptionsTable = pDhcpV6OptionModule->OptionsTable;
    PDHCPV6_OPTION_HEADER pDhcpV6OptionHeader = (PDHCPV6_OPTION_HEADER)pucOptionMessageBuffer;
    PUSHORT pusHeaderOptions = 0;
    ULONG uIndex = 0;
    USHORT  *psCurLoc;
    USHORT  cLen;

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Create Option Request"));

    if (usOptionLength != sizeof(DHCPV6_OPTION_HEADER) + (2 * sizeof(USHORT))) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_ERROR, ("ERROR Invalid Option Buffer Length"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    // Let's do Elapsed Time
    pDhcpV6OptionHeader->OptionCode = htons(DHCPV6_OPTION_TYPE_ELAPSED_TIME);
    pDhcpV6OptionHeader->OptionLength = htons(2);
    psCurLoc = (PUSHORT)(&pDhcpV6OptionHeader[1]);
    
    *psCurLoc = htons(0);
    psCurLoc++;

    // let's do Client ID

    cLen = (USHORT)pDhcpV6Adapt->cPhysicalAddr;
    ASSERT(0 == (cLen & 1));
    cLen &= 0xfffe;
    cLen += 4;  // add 2 for DUID-type + 2 for hardware type + 

    pDhcpV6OptionHeader = (PDHCPV6_OPTION_HEADER)psCurLoc;
    pDhcpV6OptionHeader->OptionCode = htons(DHCPV6_OPTION_TYPE_CLIENTID);
    pDhcpV6OptionHeader->OptionLength = htons(cLen);

    psCurLoc = (PUSHORT)(&pDhcpV6OptionHeader[1]);
    
    *psCurLoc = htons(3);  // DUID-type LL link-layer address
    psCurLoc++;
    *psCurLoc = htons(1);   // hardware type
    psCurLoc++;

    memcpy(psCurLoc, pDhcpV6Adapt->PhysicalAddr, pDhcpV6Adapt->cPhysicalAddr);
    psCurLoc += pDhcpV6Adapt->cPhysicalAddr >> 1;

    // vendor class: values copied from what desktop sends out so that the server responds
    pDhcpV6OptionHeader = (PDHCPV6_OPTION_HEADER)psCurLoc;    
    pDhcpV6OptionHeader->OptionCode = htons(DHCPV6_OPTION_TYPE_VENDOR_CLASS);
    pDhcpV6OptionHeader->OptionLength = htons(0xe);
    psCurLoc += 2;

    *psCurLoc = htons(0);
    psCurLoc++;
    *psCurLoc = htons(0x0137);
    psCurLoc++;

    *psCurLoc = htons(8);
    psCurLoc++;
    *psCurLoc = htons(0x4D53);
    psCurLoc++;
    *psCurLoc = htons(0x4654);
    psCurLoc++;
    *psCurLoc = htons(0x2035);
    psCurLoc++;
    *psCurLoc = htons(0x2e30);
    psCurLoc++;

    // now handle the ORO option:
    pDhcpV6OptionHeader = (PDHCPV6_OPTION_HEADER)psCurLoc;    
    pDhcpV6OptionHeader->OptionCode = htons(DHCPV6_OPTION_TYPE_ORO);

    usOptionLength -= sizeof(DHCPV6_OPTION_HEADER);
    pDhcpV6OptionHeader->OptionLength = htons(usOptionLength);

    pusHeaderOptions = (PUSHORT)(&pDhcpV6OptionHeader[1]);

    while(usOptionLength > 0 && uIndex < DHCPV6_MAX_OPTIONS) {
        if (pDhcpV6OptionsTable[uIndex].bEnabled) {
            *pusHeaderOptions = htons(pDhcpV6OptionsTable[uIndex].DhcpV6OptionType);
            pusHeaderOptions++;
        }

        uIndex++;
        usOptionLength -= sizeof(USHORT);
    }

error:

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Creating Option Request with Error: %!status!", dwError));

    return dwError;
}


DWORD
IniDhcpV6OptionMgrVerifyReply(
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule,
    PUCHAR pucMessageBuffer,
    ULONG uMessageBufferLength
    )
{
    DWORD dwError = 0;
    ULONG uIndex = 0;
    PDHCPV6_OPTION pDhcpV6OptionsTable = pDhcpV6OptionModule->OptionsTable;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Verifying Reply"));

    for (uIndex = 0; uIndex < DHCPV6_MAX_OPTIONS; uIndex++) {
        ULONG uOptionBufferLength = 0;
        UNALIGNED PDHCPV6_OPTION_HEADER pDhcpV6OptionHeader = NULL;
        USHORT usOptionLength = 0;
        BOOL bFound = FALSE;
        PUCHAR pucOptionMessage = NULL;

        if (pDhcpV6OptionsTable[uIndex].bEnabled == FALSE) {
            continue;
        }
        if (pDhcpV6OptionsTable[uIndex].bRequired == FALSE) {
            continue;
        }

        uOptionBufferLength = uMessageBufferLength;
        pucOptionMessage = pucMessageBuffer;
        while(uOptionBufferLength > 0) {
            DHCPV6_OPTION_TYPE DhcpV6OptionType = 0;

            if (uOptionBufferLength < sizeof(DHCPV6_OPTION_HEADER)) {
                dwError = ERROR_INVALID_PARAMETER;
                DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Buffer Length Invalid - Packet should be discarded"));
                BAIL_ON_WIN32_ERROR(dwError);
            }
            pDhcpV6OptionHeader = (PDHCPV6_OPTION_HEADER)pucOptionMessage;
            uOptionBufferLength -= sizeof(DHCPV6_OPTION_HEADER);
            pucOptionMessage += sizeof(DHCPV6_OPTION_HEADER);

            usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);
            if (uOptionBufferLength < usOptionLength) {
                dwError = ERROR_INVALID_PARAMETER;
                DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Length Invalid - Packet should be discarded"));
                BAIL_ON_WIN32_ERROR(dwError);
            }
            uOptionBufferLength -= usOptionLength;
            pucOptionMessage += usOptionLength;

            DhcpV6OptionType = ntohs(pDhcpV6OptionHeader->OptionCode);
            if (DhcpV6OptionType == pDhcpV6OptionsTable[uIndex].DhcpV6OptionType) {
                bFound = TRUE;
                break;
            }

            if (DHCPV6_OPTION_TYPE_STATUS_CODE == DhcpV6OptionType && 
                usOptionLength >= 2) {

                USHORT  *pHdr = (USHORT *)pDhcpV6OptionHeader;
                DHCPV6_STATUS_CODE  StatusCode; 
                
                // oh, oh we got some sort of status notification
                // check to see if they don't support prefix-del

                StatusCode = ntohs(pHdr[2]);
                
                if (DHCPV6_STATUS_NOPREFIXAVAIL == StatusCode) {
                    dwError = ERROR_INVALID_LEVEL;
                    goto error;
                }
            }
            
        }

        if (!bFound) {
            // Option not found
            dwError = ERROR_NOT_FOUND;
            DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Requirement: Type: %d Not met - Packet should be discarded", pDhcpV6OptionsTable[uIndex].DhcpV6OptionType));
            BAIL_ON_WIN32_ERROR(dwError);
        }
    }

error:

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Verifying Reply with Error: %!status!", dwError));

    return dwError;
}


DWORD
DhcpV6OptionMgrProcessReply(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule,
    PUCHAR pucMessageBuffer,
    ULONG uMessageBufferLength
    )
{
    DWORD dwError = 0;
    PDHCPV6_OPTION pDhcpV6OptionsTable = pDhcpV6OptionModule->OptionsTable;
    ULONG uIndex = 0;
    PUCHAR pucOptionMessage = NULL;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Reply for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    dwError = IniDhcpV6OptionMgrVerifyReply(
                    pDhcpV6OptionModule,
                    pucMessageBuffer,
                    uMessageBufferLength
                    );
    BAIL_ON_WIN32_ERROR(dwError);

    pucOptionMessage = pucMessageBuffer;
    while(uMessageBufferLength > 0) {
        DHCPV6_OPTION_TYPE DhcpV6OptionType = 0;
        USHORT usOptionLength = 0;
        UNALIGNED DHCPV6_OPTION_HEADER  *  pDhcpV6OptionHeader = NULL;

        if (uMessageBufferLength < sizeof(DHCPV6_OPTION_HEADER)) {
            dwError = ERROR_INVALID_PARAMETER;
            DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Buffer Length Invalid - Packet should be discarded"));
            BAIL_ON_WIN32_ERROR(dwError);
        }
        pDhcpV6OptionHeader = (PDHCPV6_OPTION_HEADER)pucOptionMessage;
        uMessageBufferLength -= sizeof(DHCPV6_OPTION_HEADER);
        pucOptionMessage += sizeof(DHCPV6_OPTION_HEADER);

        usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);
        if (uMessageBufferLength < usOptionLength) {
            dwError = ERROR_INVALID_PARAMETER;
            DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Length Invalid - Packet should be discarded"));
            BAIL_ON_WIN32_ERROR(dwError);
        }
        uMessageBufferLength -= usOptionLength;
        pucOptionMessage += usOptionLength;

        DhcpV6OptionType = ntohs(pDhcpV6OptionHeader->OptionCode);
        for (uIndex = 0; uIndex < DHCPV6_MAX_OPTIONS; uIndex++) {
            if (pDhcpV6OptionsTable[uIndex].DhcpV6OptionType != DhcpV6OptionType) {
                continue;
            }

            if (pDhcpV6OptionsTable[uIndex].bEnabled == FALSE) {
                break;
            }

            dwError = pDhcpV6OptionsTable[uIndex].pDhcpV6OptionProcessRecv(
                                                        pDhcpV6Adapt,
                                                        pDhcpV6OptionHeader
                                                        );
            BAIL_ON_WIN32_ERROR(dwError);

            break;
        }
    }

error:

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Process Reply with Error: %!status!", dwError));

    return dwError;
}


DWORD
DhcpV6OptionMgrProcessRecvDNS(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    UNALIGNED DHCPV6_OPTION_HEADER *pDhcpV6OptionHeader
    )
{
    DWORD dwError = 0;
    ULONG uNumOfDNSEntries = 0;
    PIN6_ADDR pIpv6DNSServers = NULL;
    ULONG uLength = 0;
    USHORT usOptionLength;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Received DNS for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);

    if (usOptionLength % sizeof(IN6_ADDR) != 0) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (pDhcpV6Adapt->pIpv6DNSServers != NULL) {
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Releasing Previous %d DNS Entries Received", pDhcpV6Adapt->uNumOfDNSServers));

        pDhcpV6Adapt->uNumOfDNSServers = 0;
        FreeDHCPV6Mem(pDhcpV6Adapt->pIpv6DNSServers);
        pDhcpV6Adapt->pIpv6DNSServers = NULL;
    }

    uNumOfDNSEntries = usOptionLength / sizeof(IN6_ADDR);
    uLength = uNumOfDNSEntries * sizeof(IN6_ADDR);

    pIpv6DNSServers = AllocDHCPV6Mem(uLength);
    if (!pIpv6DNSServers) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pIpv6DNSServers, 0, uLength);

    memcpy(pIpv6DNSServers, &(pDhcpV6OptionHeader[1]), uLength);
    pDhcpV6Adapt->pIpv6DNSServers = pIpv6DNSServers;
    pDhcpV6Adapt->uNumOfDNSServers = uNumOfDNSEntries;

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_INFO, ("DNS Entries Received: %d\n %!DHCPHEXDUMP!",
                    pDhcpV6Adapt->uNumOfDNSServers,
                    LOG_DHCPHEXDUMP(pDhcpV6Adapt->uNumOfDNSServers * sizeof(IN6_ADDR), (PUCHAR)pDhcpV6Adapt->pIpv6DNSServers)));

error:
    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Process Received DNS with Error: %!status!", dwError));

    return dwError;
}

DWORD
DhcpV6OptionMgrProcessRecvDomainList(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    UNALIGNED DHCPV6_OPTION_HEADER *pDhcpV6OptionHeader
    )
{
    DWORD   dwError = 0;
    ULONG   uLength, uAddLen;
    USHORT  usOptionLength;
    PCHAR   p;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Received Domain List for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);

    if (usOptionLength < 1) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (pDhcpV6Adapt->pDomainList) {
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, 
            ("Releasing Previous Domain list len %d", pDhcpV6Adapt->cDomainList));

        pDhcpV6Adapt->cDomainList = 0;
        FreeDHCPV6Mem(pDhcpV6Adapt->pDomainList);
        pDhcpV6Adapt->pDomainList = NULL;
    }

    uLength = usOptionLength;
    // always terminate domain list with '\0'
    p = (char *)&(pDhcpV6OptionHeader[1]);
    if (p[uLength - 1] != '\0')
        uAddLen = 1;
    else
        uAddLen = 0;

    pDhcpV6Adapt->pDomainList = AllocDHCPV6Mem(uLength + uAddLen);
    if (! pDhcpV6Adapt->pDomainList) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }

    memcpy(pDhcpV6Adapt->pDomainList, p, uLength);
    if (uAddLen)
        pDhcpV6Adapt->pDomainList[uLength] = '\0';

    pDhcpV6Adapt->cDomainList = uLength + uAddLen;

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_INFO, ("Domain List Entries Received: %d\n %!DHCPHEXDUMP!",
            uLength,
            LOG_DHCPHEXDUMP(uLength, (PUCHAR)pDhcpV6Adapt->pDomainList)));

error:
    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Process Received DNS with Error: %!status!", dwError));

    return dwError;
}


DWORD
DhcpV6OptionMgrProcessRecvPD(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    UNALIGNED DHCPV6_OPTION_HEADER *pDhcpV6OptionHeader
    )
{
    DWORD   dwError = 0;
    USHORT  cPrefixOption;
    USHORT  usOptionLength;
    DHCPV6_IA_PD_OPTION UNALIGNED       *pPacketPD;
    DHCPV6_IA_PREFIX_OPTION UNALIGNED   *pPacketPrefix;
    DWORD   T1, T2, PrefLife, ValidLife;


    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Received PD for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);

    if (usOptionLength < 41) {  // 12 + 4 + 25
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, 
            ("WARNING: Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pPacketPD = (DHCPV6_IA_PD_OPTION *)pDhcpV6OptionHeader;
    pPacketPrefix = pPacketPD->Prefix;
    if (DHCPV6_OPTION_TYPE_IA_PREFIX != ntohs(pPacketPrefix->OptionCode)) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, 
            ("WARNING: Prefix Option Code Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    cPrefixOption = ntohs(pPacketPrefix->OptionLen);

    if (cPrefixOption < 25) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, 
            ("WARNING: Prefix Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }
    
    if ((usOptionLength > 41) && (cPrefixOption > 25)) {
        if (pPacketPrefix->PrefixOptions[0]) {
            dwError = ERROR_INVALID_PARAMETER;
            DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, 
                ("WARNING: PD error code %d - Packet should be discarded", 
                pPacketPrefix->PrefixOptions[0]));
            BAIL_ON_WIN32_ERROR(dwError);
        }
    }

    T1 = ntohl(pPacketPD->T1);
    T2 = ntohl(pPacketPD->T2);
    if ((T1 && T2) && (T1 > T2)) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, 
            ("WARNING: Prefix Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    PrefLife = ntohl(pPacketPrefix->PreferredLifetime);
    ValidLife = ntohl(pPacketPrefix->ValidLifetime);
    if ((PrefLife > ValidLife) || (0 == ValidLife)) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, 
            ("WARNING: Prefix Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (! T1)
        T1 = PrefLife >> 1;
    if (! T2)
        T2 = PrefLife * 4 /5;
    if (T1 > T2)
        T1 = T2 >> 1;

    if (pDhcpV6Adapt->pPdOption) {
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, 
            ("Releasing Previous PD Option Received"));

        FreeDHCPV6Mem(pDhcpV6Adapt->pPdOption);
        pDhcpV6Adapt->pPdOption = NULL;
    }

    pDhcpV6Adapt->pPdOption = AllocDHCPV6Mem(sizeof(DHCPV6_PD_OPTION));
    if (!pDhcpV6Adapt->pPdOption) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pDhcpV6Adapt->pPdOption->IAID = 0;
    ASSERT(0 == pPacketPD->IAID);

    pDhcpV6Adapt->pPdOption->T1 = T1;
    pDhcpV6Adapt->pPdOption->T2 = T2;

    pDhcpV6Adapt->pPdOption->IAPrefix.PreferedLifetime = PrefLife;
    pDhcpV6Adapt->pPdOption->IAPrefix.ValidLifetime = ValidLife;
    pDhcpV6Adapt->pPdOption->IAPrefix.cPrefix = pPacketPrefix->PrefixLength;

    memcpy(&(pDhcpV6Adapt->pPdOption->IAPrefix.PrefixAddr),
        pPacketPrefix->IPv6Prefix, sizeof(IN6_ADDR));

    GetCurrentFT (&(pDhcpV6Adapt->pPdOption->IAPrefix.IALeaseObtained));

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_INFO, ("PD Entry recv'd: "));

error:
    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Process Received DNS with Error: %!status!", dwError));

    return dwError;
}


DWORD
DhcpV6OptionMgrProcessRecvClientID(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    UNALIGNED DHCPV6_OPTION_HEADER *pDhcpV6OptionHeader
    )
{
    DWORD   dwError = 0;
    USHORT  usOptionLength;
    USHORT  *psCurLoc = (USHORT *)&(pDhcpV6OptionHeader[1]);

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Received Domain List for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);

    if (usOptionLength < (4 + pDhcpV6Adapt->cPhysicalAddr)) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (htons(3) != *psCurLoc)
        dwError = ERROR_INVALID_PARAMETER;
    psCurLoc++;
    if (htons(1) != *psCurLoc)
        dwError = ERROR_INVALID_PARAMETER;
    psCurLoc++;
    BAIL_ON_WIN32_ERROR(dwError);

    if (memcmp(psCurLoc, pDhcpV6Adapt->PhysicalAddr, pDhcpV6Adapt->cPhysicalAddr)) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_WIN32_ERROR(dwError);
    }

error:
    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Process Received DNS with Error: %!status!", dwError));

    return dwError;
}


DWORD
DhcpV6OptionMgrProcessRecvServerID(
    PDHCPV6_ADAPT pDhcpV6Adapt,
    UNALIGNED DHCPV6_OPTION_HEADER *pDhcpV6OptionHeader
    )
{
    DWORD   dwError = 0;
    USHORT  usOptionLength;

    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("Begin Process Received Domain List for Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    usOptionLength = ntohs(pDhcpV6OptionHeader->OptionLength);

    if (usOptionLength < 4 ) {
        dwError = ERROR_INVALID_PARAMETER;
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_WARN, ("WARNING: Option Length Invalid - Packet should be discarded"));
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (pDhcpV6Adapt->pServerID) {
        DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, 
            ("Releasing Previous PD Option Received"));

        FreeDHCPV6Mem(pDhcpV6Adapt->pServerID);
        pDhcpV6Adapt->pServerID = NULL;
        pDhcpV6Adapt->cServerID = 0;
    }

    pDhcpV6Adapt->pServerID = AllocDHCPV6Mem(usOptionLength);
    if (! pDhcpV6Adapt->pServerID) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }

    pDhcpV6Adapt->cServerID = usOptionLength;
    memcpy(pDhcpV6Adapt->pServerID, &(pDhcpV6OptionHeader[1]), usOptionLength);

error:
    DhcpV6Trace(DHCPV6_OPTION, DHCPV6_LOG_LEVEL_TRACE, ("End Process Received DNS with Error: %!status!", dwError));

    return dwError;
}

