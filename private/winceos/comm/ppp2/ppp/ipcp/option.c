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

// PPP IPCP Layer Option Processing


//  Include Files

#include "windows.h"
#include "cclib.h"

#include "memory.h"
#include "cxport.h"
#include "crypt.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "layerfsm.h"
#include "auth.h"
#include "lcp.h"
#include "ipcp.h"
#include "mac.h"
#include "pppserver.h"
#include "ncp.h"

// DEBUG OUTPUT MACRO
#define IPADDROUT(a) (a)>>24, ((a)>>16)&0xFF, ((a)>>8)&0xFF, (a)&0xFF

#define IPADDR_TO_DWORD(dw, pB) \
    (dw) = ((pB)[0] << 24) | ((pB)[1] << 16) | ((pB)[2] <<  8) | ((pB)[3])

#define DWORD_TO_IPADDR(pB, dw) \
    (pB)[0] = (BYTE)(dw >> 24); (pB)[1] = (BYTE)(dw >> 16); (pB)[2] = (BYTE)(dw >> 8); (pB)[3] = (BYTE)(dw)


// Van Jacobson Compressed TCP/IP Protocol Identifier
BYTE g_abVJProtocolID[2] = {0x00, 0x2D};

//
//  Debug support functions to display option values as strings
//
void
ipAddressOptionToStringCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData,
    IN  OUT PSTR   *ppBuffer)
//
//  Convert a 4 byte IP address option value to dot-notation string (e.g. 1.2.3.4)
//
{
    PSTR pBuffer = *ppBuffer;

    //
    // Generic framework code should have validated IP address length at 4 bytes
    // before calling this function.
    //
    PREFAST_ASSERT(cbOptData == 4);

    if (cbOptData == 4)
        pBuffer += sprintf(pBuffer, "%u.%u.%u.%u", pOptData[0], pOptData[1], pOptData[2], pOptData[3]);

    *ppBuffer = pBuffer;
}

void
ipCompressionOptionToStringCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData,
    IN  OUT PSTR   *ppBuffer)
{
    PSTR pBuffer = *ppBuffer;

    ASSERT(cbOptData == 4);

    pBuffer += sprintf(pBuffer, "%02X%02X,MaxSlot=%u,CompSlot=%u", pOptData[0], pOptData[1], pOptData[2], pOptData[3]);

    *ppBuffer = pBuffer;
}

//////////////////////////////////////////
//  IPCP Option Negotation Callbacks
//////////////////////////////////////////

void
ipcpResetPeerOptionValuesCb(
    IN      PVOID  context)
//
//  This function is called whenever a new configure-request is
//  received from the peer prior to processing the options therein
//  contained. This function must reset all options to their default
//  values, such that any not explicity contained within the configure
//  request will use the default setting.
//
{
    PIPCPContext pContext = (PIPCPContext)context;

    pContext->peer.VJCompressionEnabled = FALSE;
    pContext->peer.ipAddress = 0;
}

void
ipcpOptionValueReset(
    PIPCPContext pContext)
//
//  Initialize the option states at the beginning of an
//  IPCP negotiation.
//
{
    pppSession_t    *pSession = (pppSession_t *)(pContext->session);
    RASPENTRY       *pRasEntry = &pSession->rasEntry;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "pppIpv6cp_OptValueInit\n" )));
    //
    // Disable Van Jacobsen TCP header compression until it is enabled via
    // option negotiation.
    //
    pContext->local.VJCompressionEnabled = FALSE;

    // local.MaxSlotID tells the peer what is the maximum slot number that
    // we want the peer to send to us.
    pContext->local.MaxSlotId = (BYTE)pContext->VJMaxSlotIdRx;
    pContext->local.CompSlotId = (BYTE)pContext->VJEnableSlotIdCompressionRx;
    pContext->local.ipAddress = 0;

    pContext->bNakReceived = FALSE;

    if (pSession->bIsServer)
    {
        // Server
        // Address will be retrieved when we are building the IPCP config request
    }
    else
    {
        // Client
        // We'll send an IP address of 0.0.0.0 to the server unless a flag specifies
        // otherwise.  Sending 0.0.0.0 requests the server to NAK with an IP address
        // for us to use.
        //
        if (pSession->dwAlwaysSuggestIpAddr || (pRasEntry->dwfOptions & RASEO_SpecificIpAddr))
        {
            memcpy( &pContext->local.ipAddress, &pRasEntry->ipaddr, sizeof( RASIPADDR ) );
            DEBUGMSG( ZONE_NCP, (TEXT("ipcp:RasEntry IP Address: 0x%x\r\n" ), pContext->local.ipAddress ));
        }
    }
}

////////// IP Compression Protocol option callbacks /////////////////////

DWORD
ipcpBuildIPCompressionOptionCb(
    IN                              PVOID       context,
    IN  OUT                         POptionInfo pInfo,
    __out_bcount(*pcbOptData)       PBYTE       pOptData,
    OUT                             PDWORD      pcbOptData)
//
//  Called to fill in the data for an IP-Compression-Protocol option
//  that is part of a configure-request we will send to the peer.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;

    //
    //  Make sure there is enough room to put our option data.
    //  Current PPP code always passes in sufficient space to encode all options
    //  IPCP requests.
    //
    PREFAST_ASSERT(*pcbOptData >= 4);

    memcpy(pOptData, &g_abVJProtocolID[0], sizeof(g_abVJProtocolID));
    pOptData[2] = pContext->local.MaxSlotId;
    pOptData[3] = pContext->local.CompSlotId;
    *pcbOptData = 4;

    return dwResult;
}

DWORD
ipcpAckIPCompressionOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the IP-Compression-Protocol option
//  that we sent in a CR.  Since we both now agree on the IP-Compression-Protocol,
//  we can commit to using it.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;

    pContext->local.VJCompressionEnabled = TRUE;

    return dwResult;
}

DWORD
ipcpNakIPCompressionOptionCb(
    IN      PVOID  context,
    IN OUT  struct OptionInfo *pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer sends us a configure-nak for an IP-Compression-Protocol option
//  to suggest an IP-Compression-Protocol that we should use.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;
    BYTE         MaxSlotId,
                 CompSlotId;
    //
    // We only support Van Jacobson, so if the peer is NAKing with anything else
    // we just drop the attempt to negotiate VJ
    //
    if ((cbOptData < 4)
    ||  memcmp(pOptData, &g_abVJProtocolID[0], sizeof(g_abVJProtocolID)) != 0)
    {
        // Peer is suggesting an unsupported compression protocol,
        // treat it as a reject so we don't attempt to configure it.
        pInfo->onsLocal = ONS_Rejected;
    }
    else
    {
        MaxSlotId = pOptData[2];
        CompSlotId = pOptData[3];

        //
        // If the peer wants to use a value <= our configured max (default 15),
        // then agree to use the peer's suggestion. Otherwise, ignore it.
        // If we agreed to let the peer send us VJ compressed headers with
        // slotIDs > the max slot ID then we wouldn't be able to decode those
        // packets.
        //
        if (MaxSlotId <= (pContext->VJMaxSlotIdRx))
        {
            // Save NAK value for use in our next Config-Request
            pContext->local.MaxSlotId    = MaxSlotId;
        }

        //
        // If the peer really wants to send compressed slot IDs, we'll play along
        // even if we wanted it disabled.
        //
        DEBUGMSG(ZONE_WARN && (0 == pContext->VJEnableSlotIdCompressionRx) && CompSlotId,
            (L"PPP: WARNING - Peer wants to send with VJ Slot ID Compression even though we want it disabled\n"));
        pContext->local.CompSlotId   = CompSlotId;
    }
    return dwResult;
}

DWORD
ipcpRequestIPCompressionCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive an IPCP configure request containing
//  the IP-Compression-Protocol option.  The peer is telling us what
//  compression protocol it wants to use to send packets to us.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;

    //
    // We only support Van Jacobson, so if the peer is requesting anything else
    // we NAK and suggest VJ.
    //
    if ((pOptData == NULL)   // Peer did not request compression, we want to suggest it
    ||  (cbOptData < 4)
    ||  (memcmp(pOptData, &g_abVJProtocolID[0], sizeof(g_abVJProtocolID)) != 0)
    ||  pOptData[2] > pContext->VJMaxSlotIdTx)
    {
        // Peer is suggesting an unsupported compression protocol,
        // or more states than we can handle for VJ compression.
        // Send a NAK back with our VJ options
        memcpy(&pContext->optDataVJ[0], &g_abVJProtocolID[0], sizeof(g_abVJProtocolID));
        pContext->optDataVJ[2] = (BYTE)pContext->VJMaxSlotIdTx;
        pContext->optDataVJ[3] = (BYTE)pContext->VJEnableSlotIdCompressionTx;
        *pCode = PPP_CONFIGURE_NAK;
        *ppOptData = &pContext->optDataVJ[0];
        *pcbOptData = 4;
    }
    else // VJ requested with acceptable maxStates
    {
        *pCode = PPP_CONFIGURE_ACK;

        // Save peer values
        // Note that we won't use SlotId Compression if we have disabled it locally,
        // even though we may be agreeing to it now (should always be able to send
        // ok with uncompressed SlotIds).
        pContext->peer.VJCompressionEnabled = TRUE;
        pContext->peer.MaxSlotId    = pOptData[2];
        pContext->peer.CompSlotId   = pOptData[3];
    }
    return dwResult;
}

/////////////// IP Address configuration callbacks ////////////////////////////

DWORD
ipcpBuildIPAddressOptionCb(
    IN                              PVOID       context,
    IN  OUT                         POptionInfo pInfo,
    __out_bcount(*pcbOptData)       PBYTE       pOptData,
    IN OUT                          PDWORD      pcbOptData)
//
//  Called to fill in the data for an IP-Address option
//  that is part of a configure-request we will send to the peer.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;

    //
    // The generic option framework code will have already checked that sufficient space
    // is available to encode this fixed length option.
    //
    PREFAST_ASSERT(*pcbOptData >= 4);

    if (pSession->bIsServer)
    {
        //
        //  If we are a server then get the IP address
        //  (static or DHCP) allocated when the line
        //  was enabled.
        //
        pContext->local.ipAddress = PPPServerGetSessionServerIPAddress(pSession);
        DEBUGMSG(ZONE_IPCP, (TEXT("PPP: Server assigning IP Address %u.%u.%u.%u to itself\n"), IPADDROUT(pContext->local.ipAddress)));
    }

    DWORD_TO_IPADDR(pOptData, pContext->local.ipAddress);
    *pcbOptData = 4;
 
    return dwResult;
}

DWORD
ipcpAckIPAddressOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the IP-Address option
//  that we sent in a CR.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;
    RASPENTRY   *pRasEntry = &pContext->session->rasEntry;

    return dwResult;
}

DWORD
ipcpNakIPAddressOptionCb(
    IN      PVOID  context,
    IN OUT  struct OptionInfo *pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer sends us a configure-nak for an IP-Address option
//  to suggest an IP-Address that we should use.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;
    DWORD        ipAddrSuggestedByPeer;

    //
    // The generic option framework code will have already checked that sufficient space
    // is available to encode this fixed length option.
    //
    PREFAST_ASSERT(cbOptData >= 4);

    IPADDR_TO_DWORD(ipAddrSuggestedByPeer, pOptData);

    pContext->bNakReceived = TRUE;

    if (ipAddrSuggestedByPeer == 0 || ipAddrSuggestedByPeer == 0xFFFFFFFF)
    {
        //
        //  IP address is 0 or the broadcast address, neither of which
        //  is valid.  We will ignore it and do no further negotiation
        //  of this option (treat it as if the peer had REJected it instead).
        //
        pInfo->onsLocal = ONS_Rejected;
        DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Server NAKed option %hs with Invalid IPAddr=%x\n"), 
            pInfo->pDescriptor->szName, ipAddrSuggestedByPeer));
    }
    else
    {
        //
        //  IP address seems reasonable. Save it for use in subsequent config-request
        //
        pContext->local.ipAddress = ipAddrSuggestedByPeer;
    }

    return dwResult;
}

DWORD
ipcpRejIPAddressOptionCb(
    IN      PVOID  context,
    IN OUT  struct OptionInfo *pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer sends us a configure-rej for an IP-Address option
//  to suggest an IP-Address that we should use.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;
    RASPENTRY   *pRasEntry = &pContext->session->rasEntry;

    // We really need to configure an IP address.
    // The only known case where the peer will
    // reject the IP address is when we are talking
    // to Win95 and send it address 0.0.0.0 (really
    // requesting it to configure our address).  In
    // that case, we fall back to using our static
    // assigned address and retry that on the configure
    // request.

    if (0 == memcmp(&pContext->local.ipAddress, &pRasEntry->ipaddr, 4))
    {
        // We have no alternative static IP address to try, and the server has
        // rejected the address we were trying. So, we have no way to recover from the server rejection.
        //
        // Set the require level to "Required", which in combination with the "Rejected"
        // status will cause us to fail out when we try to build our next configure request.
        //
        DEBUGMSG( ZONE_NCP, (TEXT("PPP: IP Address %x REJected\n" ), pContext->local.ipAddress ));
        pInfo->orlLocal = ORL_Required;
    }
    else
    {
        pInfo->onsLocal = ONS_Naked;
        memcpy( &pContext->local.ipAddress, &pRasEntry->ipaddr, 4 );
        DEBUGMSG( ZONE_NCP, (TEXT("PPP: IP Address REJected, using static: 0x%x\r\n" ), pContext->local.ipAddress ));
    }
    return dwResult;
}

DWORD
ipcpRequestIPAddressCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive an IPCP configure request containing
//  the IP-Address option.  The peer is telling us what
//  IP address it wants to use.  If the IP address is 0, then
//  the peer is requesting that we NAK with an IP address for it
//  to use.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;
    DWORD        peerRequestedIPAddress;
    static BYTE  IPAddrZero[4] = {0,0,0,0};

    if (pOptData == NULL)
    {
        //
        //  The peer did not send us its IP address in a Config-Request, and we want it to.
        //  So, we send a NAK with a 0 address.
        //
        *ppOptData = &IPAddrZero[0];
        *pcbOptData = 4;
    }
    else
    {
        //
        // The generic option handling code should have rejected any illegal sized option.
        //
        ASSERT(cbOptData == 4);

        IPADDR_TO_DWORD(peerRequestedIPAddress, pOptData);

        if (pSession->bIsServer)
        {
            if (pContext->peer.ipAddress == 0)
            {
                //
                // Get the IP address (static or DHCP) allocated
                // for the client side when the line was enabled
                //
                
                pContext->peer.ipAddress = PPPServerGetSessionClientIPAddress(pSession);
            }

            DEBUGMSG(ZONE_NCP, (TEXT("PPP: Server assigning IP Address %u.%u.%u.%u to client\n"), IPADDROUT(pContext->peer.ipAddress)));
            
            if (pContext->peer.ipAddress == 0)
            {
                // Unable to obtain an IP address for the client
                *pCode = PPP_CONFIGURE_REJ;
            }
            else
            {
                //
                //  If we are a server, then we will only allow the client to use
                //  the IP address we assign.  If the client requests any other
                //  address (including 0), we NAK and tell him the address to use.
                //
                if (peerRequestedIPAddress == pContext->peer.ipAddress)
                {
                    *pCode = PPP_CONFIGURE_ACK;
                }
                else
                {
                    DWORD_TO_IPADDR(&pContext->optDataIPAddress[0], pContext->peer.ipAddress);
                    *pCode = PPP_CONFIGURE_NAK;
                    *ppOptData = &pContext->optDataIPAddress[0];
                }
            }
        }
        else
        {
            //
            //  If we are a client, then we will allow the server to use any
            //  address it wants, except 0.
            //
            pContext->peer.ipAddress = peerRequestedIPAddress;

            if (peerRequestedIPAddress != 0)
            {
                *pCode = PPP_CONFIGURE_ACK;
            }
            else
            {
                RASPENTRY   *pRasEntry = &pSession->rasEntry;

                //
                // BUGBUGBUG
                // Pegasus PPP Hack.  If the peer is going to use a zero IP address
                // then we send him our address plus 1.  This allows Win95 to work
                // since it doesn't have a good way to assign a TCP/IP server
                // IP address without affecting dial-up connections
                //
                if (pContext->local.ipAddress == 0)
                {
                    memcpy( &pContext->local.ipAddress, &pRasEntry->ipaddr, 4 );
                }
                DWORD_TO_IPADDR(&pContext->optDataIPAddress[0], pContext->local.ipAddress);
                pContext->optDataIPAddress[3] += 1;
                *pCode = PPP_CONFIGURE_NAK;
                *ppOptData = &pContext->optDataIPAddress[0];

                DEBUGMSG( ZONE_IPCP, (TEXT( "PPP: Server Trying to use 0.0.0.0 IP ADDRESS, PROB W95, substituting...: 0x%X\r\n" ), pContext->local.ipAddress + 1 ) );
            }
        }
    }
    return dwResult;
}

/////////////   Name Server Address configuration callbacks //////////////

PBYTE
ipcpGetNSAddr(
    IN      pppSession_t *pSession,
    IN      POptionInfo pInfo)
//
//  Return a pointer to the appropriate Name Server IP Address field in the session
//  based upon the option info type.
//
{
    PBYTE pAddr = NULL;

    switch(pInfo->pDescriptor->type)
    {
    case IPCP_OPT_DNS_IP_ADDR:           pAddr = (PBYTE)&pSession->rasEntry.ipaddrDns;    break;
    case IPCP_OPT_WINS_IP_ADDR:          pAddr = (PBYTE)&pSession->rasEntry.ipaddrWins;   break;
    case IPCP_OPT_DNS_BACKUP_IP_ADDR:    pAddr = (PBYTE)&pSession->rasEntry.ipaddrDnsAlt; break;
    case IPCP_OPT_WINS_BACKUP_IP_ADDR:   pAddr = (PBYTE)&pSession->rasEntry.ipaddrWinsAlt;break;
    default:                             ASSERT(FALSE);                                   break;
    }
    return pAddr;
}


DWORD
ipcpBuildNSAddressOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    OUT     PBYTE  pOptData,
    IN  OUT PDWORD pcbOptData)
//
//  Called to fill in the data for a name server IP Address option
//  that is part of a configure-request we will send to the peer.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pAddr;

    //
    // The generic option framework code will have already checked that sufficient space
    // is available to encode this fixed length option.
    //
    PREFAST_ASSERT(*pcbOptData >= 4);

    pAddr = ipcpGetNSAddr(pSession, pInfo);
    if (NULL == pAddr)
        return ERROR_INVALID_PARAMETER;

    //
    // This is a workaround to support ActiveSync. ActiveSync silently drops 0.0.0.0 name
    // server addresses from its ACK messages without REJecting them. It will NAK the
    // DNS address. So, after receiving a NAK for one or more name server addresses
    // do not send 0.0.0.0 name server addresses in any subsequent config-requests.
    //
    if (pContext->bNakReceived && *(PDWORD)pAddr == 0)
    {
        dwResult = ERROR_PPP_SKIP_OPTION;
    }
    else
    {
        // RASIPADDR is stored LSB first, option is MSB first
        pOptData[0] = pAddr[3];
        pOptData[1] = pAddr[2];
        pOptData[2] = pAddr[1];
        pOptData[3] = pAddr[0];
        *pcbOptData = 4;
    }

    return dwResult;
}

DWORD
ipcpAckNSAddressOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs a name server IP address option
//  that we sent in a CR.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;

    // This is where we should commit to using the name server addresses...
    // Currently they are committed in the Nak callback.

    return dwResult;
}

DWORD
ipcpNakNSAddressOptionCb(
    IN      PVOID  context,
    IN OUT  struct OptionInfo *pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer NAKs a name server IP address option
//  that we sent in a CR.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;
    DWORD        ipAddrNameServer;

    //
    // We only send a configure-request with name server address options
    // if we are configured as a client.
    //
    ASSERT(!pSession->bIsServer);

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 4);

    IPADDR_TO_DWORD(ipAddrNameServer, pOptData);

    pContext->bNakReceived = TRUE;

    if (ipAddrNameServer == 0 || ipAddrNameServer == 0xFFFFFFFF)
    {
        //
        //  IP address is 0 or the broadcast address, neither of which
        //  is valid.  We will ignore it and do no further negotiation
        //  of this option (treat it as if the peer had REJected it instead).
        //
        pInfo->onsLocal = ONS_Rejected;
        DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Server NAKed option %hs with Invalid IPAddr=%x\n"), 
            pInfo->pDescriptor->szName, ipAddrNameServer));
    }
    else
    {
        //
        //  IP address seems reasonable. Save it for use in subsequent config-request
        //

        PVOID pAddr = ipcpGetNSAddr(pSession, pInfo);
        if (pAddr)
            memcpy( pAddr, &ipAddrNameServer, sizeof( DWORD ) );
    }

    return dwResult;
}

DWORD
ipcpRejNSAddressOptionCb(
    IN      PVOID  context,
    IN OUT  struct OptionInfo *pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer REJects a name server IP address option
//  that we sent in a CR.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;

    // Clear the rejected NS address to zero so that we won't use it for
    // anything.
    PVOID pAddr = ipcpGetNSAddr(pSession, pInfo);
    if (pAddr)
        memset(pAddr, 0, sizeof( DWORD ) );

    return dwResult;
}

DWORD
ipcpRequestNSAddressCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive an IPCP configure request containing
//  DNS or WINS primary or secondary IP Address option.  If the IP address is 0, then
//  the peer is requesting that we NAK with an IP address for it
//  to use.
//
{
    PIPCPContext pContext = (PIPCPContext)context;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;
    DWORD        peerRequestedNameServerAddress;
    DWORD        ipAddrNameServer;

    //
    // The generic option handling code will reject this option unless we are a server,
    // per our call to PppFsmOptionsAdd.
    //
    ASSERT(pContext->session->bIsServer);

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    ASSERT(cbOptData == 4);

    IPADDR_TO_DWORD(peerRequestedNameServerAddress, pOptData);

    //
    // If we are a server then the client requests the addresses of the name
    // servers by sending them in a CR with values of 0
    //
    if (peerRequestedNameServerAddress == 0)
    {
        //
        // Get IP address of appropriate name server
        //
        switch(pInfo->pDescriptor->type)
        {
        case IPCP_OPT_DNS_IP_ADDR:
            ipAddrNameServer = g_dwDNSIpAddr[0];
            break;

        case IPCP_OPT_DNS_BACKUP_IP_ADDR:
            ipAddrNameServer = g_dwDNSIpAddr[1];
            break;

        case IPCP_OPT_WINS_IP_ADDR:
            ipAddrNameServer = g_dwWINSIpAddr[0];
            break;

        case IPCP_OPT_WINS_BACKUP_IP_ADDR:
            ipAddrNameServer = g_dwWINSIpAddr[1];
            break;

        default:
            ASSERT(FALSE);
            ipAddrNameServer = 0;
            break;
        }

        //
        // If we don't have a valid name server, reject the option.
        // Otherwise, NAK and tell the client the name server address.
        //

        if (ipAddrNameServer == 0)
        {
            *pCode = PPP_CONFIGURE_REJ;
        }
        else
        {
            DWORD_TO_IPADDR(&pContext->optDataIPAddress[0], ipAddrNameServer);      
            *pCode = PPP_CONFIGURE_NAK;
            *ppOptData = &pContext->optDataIPAddress[0];
        }
    }
    else
    {
        // Presumably the client is sending the name server address
        // that we previously NAKed with.  Regardless, accept the name
        // server IP address the client has chosen.

        *pCode = PPP_CONFIGURE_ACK;
    }

    return dwResult;
}

//
//  Constant information describing IPCP options:
//
OptionDescriptor ipcpIPCompressionOptionDescriptor =
{
    IPCP_OPT_IPCOMPRESSION,         // type == IP-Compression-Protocol
    OPTION_VARIABLE_LENGTH,         // can be 2 or more octets
    "IP-Compression-Protocol",      // name
    ipcpBuildIPCompressionOptionCb,
    ipcpAckIPCompressionOptionCb,
    ipcpNakIPCompressionOptionCb,
    NULL,                           // no special reject handling
    ipcpRequestIPCompressionCb,
    ipCompressionOptionToStringCb       // debug routine
};

OptionDescriptor ipcpIPAddressOptionDescriptor =
{
    IPCP_OPT_IPADDRESS,             // type == IP-Address
    4,                              // always exactly 4 octets
    "IP-Address",                   // name
    ipcpBuildIPAddressOptionCb,
    ipcpAckIPAddressOptionCb,
    ipcpNakIPAddressOptionCb,
    ipcpRejIPAddressOptionCb,
    ipcpRequestIPAddressCb,
    ipAddressOptionToStringCb       // debug routine
};

OptionDescriptor ipcpDNSAddressOptionDescriptor =
{
    IPCP_OPT_DNS_IP_ADDR,           // type == DNS-Address
    4,                              // always exactly 4 octets
    "DNS-Address",                  // name
    ipcpBuildNSAddressOptionCb,
    ipcpAckNSAddressOptionCb,
    ipcpNakNSAddressOptionCb,
    ipcpRejNSAddressOptionCb,       // clear address to zero on reject
    ipcpRequestNSAddressCb,
    ipAddressOptionToStringCb       // debug routine
};

OptionDescriptor ipcpWINSAddressOptionDescriptor =
{
    IPCP_OPT_WINS_IP_ADDR,          // type == WINS-Address
    4,                              // always exactly 4 octets
    "WINS-Address",                 // name
    ipcpBuildNSAddressOptionCb,
    ipcpAckNSAddressOptionCb,
    ipcpNakNSAddressOptionCb,
    ipcpRejNSAddressOptionCb,       // clear address to zero on reject
    ipcpRequestNSAddressCb,
    ipAddressOptionToStringCb       // debug routine
};

OptionDescriptor ipcpDNSBackupAddressOptionDescriptor =
{
    IPCP_OPT_DNS_BACKUP_IP_ADDR,    // type == DNS-Backup-Address
    4,                              // always exactly 4 octets
    "DNS-Backup-Address",           // name
    ipcpBuildNSAddressOptionCb,
    ipcpAckNSAddressOptionCb,
    ipcpNakNSAddressOptionCb,
    ipcpRejNSAddressOptionCb,       // clear address to zero on reject
    ipcpRequestNSAddressCb,
    ipAddressOptionToStringCb       // debug routine
};

OptionDescriptor ipcpWINSBackupAddressOptionDescriptor =
{
    IPCP_OPT_WINS_BACKUP_IP_ADDR,   // type == WINS-Backup-Address
    4,                              // always exactly 4 octets
    "WINS-Backup-Address",          // name
    ipcpBuildNSAddressOptionCb,
    ipcpAckNSAddressOptionCb,
    ipcpNakNSAddressOptionCb,
    ipcpRejNSAddressOptionCb,       // clear address to zero on reject
    ipcpRequestNSAddressCb,
    ipAddressOptionToStringCb       // debug routine
};

DWORD
ipcpOptionInit(
    PIPCPContext pContext)
//
//  This function is called once during session initialization
//  to build the list of options that are supported.
//
{
    DWORD       dwResult;
    pppSession_t *pSession = pContext->session;
    RASPENTRY   *pRasEntry = &pSession->rasEntry;
    OptionRequireLevel orlIPCompressionLocal,
                       orlIPCompressionPeer,
                       orlNSLocal,
                       orlNSPeer;

    orlIPCompressionLocal = ORL_Unsupported;
    orlIPCompressionPeer  = ORL_Unsupported;
    if(pRasEntry->dwfOptions & RASEO_IpHeaderCompression )
    {
        //
        // Negotiate IP header compression - ie Request header compression
        // in our CR and accept it in our peer's CR.
        //
        orlIPCompressionLocal = ORL_Wanted;
        orlIPCompressionPeer  = ORL_Allowed;
    }

    orlNSLocal = ORL_Unsupported;
    orlNSPeer =  ORL_Unsupported;

    if (pSession->bIsServer)
    {
        //
        //  If we are a server, then we want to allow clients to obtain DNS
        //  and WINS server addresses from us.
        //
        orlNSPeer =  ORL_Allowed;
    }
    else
    {
        //
        //  If we are a client, then we want to try to obtain DNS and WINS
        //  server addresses from the server, if:
        //      bAlwaysRequestDNSandWINS registry flag is set, OR
        //      static DNS/WINS addresses were not set
        //
        if (pSession->dwAlwaysRequestDNSandWINS
        ||  (pRasEntry->dwfOptions & RASEO_SpecificNameServers) == 0)
        {
            orlNSLocal = ORL_Wanted;
        }
    }

    dwResult = PppFsmOptionsAdd(pContext->pFsm,
                            &ipcpIPCompressionOptionDescriptor,    orlIPCompressionLocal, orlIPCompressionPeer,
                            &ipcpIPAddressOptionDescriptor,        ORL_Wanted, ORL_Allowed,
                            &ipcpDNSAddressOptionDescriptor,       orlNSLocal, orlNSPeer,
                            &ipcpWINSAddressOptionDescriptor,      orlNSLocal, orlNSPeer,
                            &ipcpDNSBackupAddressOptionDescriptor, orlNSLocal, orlNSPeer,
                            &ipcpWINSBackupAddressOptionDescriptor,orlNSLocal, orlNSPeer,
                            NULL);

    return dwResult;
}
