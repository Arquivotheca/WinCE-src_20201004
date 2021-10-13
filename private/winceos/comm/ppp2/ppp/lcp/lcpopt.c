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

// PPP Link Control Protocol (LCP) Option Negotiation Handling


//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"
#include "crypt.h"

#include "ndis.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "mac.h"
#include "auth.h"
#include "ras.h"
#include "raserror.h"
#include "pppserver.h"




/////////////   MRU option negotiation callbacks //////////////

void
lcpMRUOptionToStringCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData,
    IN  OUT PSTR   *ppBuffer)
//
//  Debug pretty print routine for MRU option data
//
{
    PSTR   pBuffer = *ppBuffer;
    USHORT MRU;

    PREFAST_ASSERT(cbOptData == 2);

    BYTES_TO_USHORT(MRU, pOptData);
    pBuffer += sprintf(pBuffer, "%u", MRU);
    *ppBuffer = pBuffer;
}

DWORD
lcpBuildMRUOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    OUT     PBYTE  pOptData,
    IN  OUT PDWORD pcbOptData)
//
//  Called to fill in the MRU value field for the MRU option
//  that is part of a configure-request we will send to the peer.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;

    PREFAST_ASSERT(*pcbOptData >= 2);

    USHORT_TO_BYTES(pOptData,  pContext->local.MRU);
    *pcbOptData = 2;

    return dwResult;
}

DWORD
lcpAckMRUOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the MRU option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;
    USHORT       MRU;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 2);

    BYTES_TO_USHORT(MRU, pOptData);

    //
    // The acked value will have been verified by the generic ACK code
    // to be identical to that we requested.
    //
    ASSERT(MRU == pContext->local.MRU);

    return dwResult;
}

DWORD
lcpNakMRUOptionCb(
    IN      PVOID  context,
    IN OUT  POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer NAKs the MRU option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    RASPENTRY    *pRasEntry = &pSession->rasEntry;
    DWORD        dwResult = NO_ERROR;
    USHORT       PeerSuggestedMRU;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 2);

    BYTES_TO_USHORT(PeerSuggestedMRU, pOptData);

    // If the peer NAKs our MRU option they might be telling
    // us that they won't be sending packets that big.  That
    // is, it is acceptable for them to suggest a lower value.
    // They can't NAK with a larger value, and it would be
    // dumb of them to NAK with an equal value.

    if (PeerSuggestedMRU <= pContext->linkMaxRecvPPPInfoFieldSize)
    {
        DEBUGMSG(ZONE_LCP, (TEXT("LCP: Peer is suggesting MRU %u <= link maxRecv %u\n"),
            PeerSuggestedMRU, pContext->linkMaxRecvPPPInfoFieldSize));

        pContext->local.MRU = PeerSuggestedMRU;
    }
    else
    {
        DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Peer suggesting MRU %u > link maxRecv %u\n"),
            PeerSuggestedMRU, pContext->linkMaxRecvPPPInfoFieldSize));

        if (PeerSuggestedMRU <= 1500)
        {
            //
            // All PPP implementations are required to allow an MRU of 1500. So if the peer insists
            // on using 1500, we must use 1500.
            // However, if the peer sends packets that are larger than our link MRU, this may wind up
            // causing trouble down the road. Usually the peer will not, as the link MRU that is imposed
            // on our receiver is also imposed on the peer's send engine, it just doesn't have an LCP
            // implementation that is aware of that constraint.
            //
            pContext->local.MRU = PeerSuggestedMRU;
        }
        else
        {
            // Ignore the peer's suggestion.
        }
    }

    return dwResult;
}

DWORD
lcpRequestMRUOptionCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive a LCP configure request containing MRU option,
//  or, when a LCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    RASPENTRY    *pRasEntry = &pSession->rasEntry;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;
    USHORT       PeerRequestedMRU;

    //
    // We never set ORL_Wanted for MRU, so we should only be called
    // when the peer sends a config-request WITH the MRU option,
    // never without it.
    //
    // ASSERT(pOptData);

    //
    // Generic option length checking should have verified this.
    //
    ASSERT(cbOptData == 2);

    BYTES_TO_USHORT(PeerRequestedMRU, pOptData);

    // The peer is telling us the maximum receive packet size it can handle.
    // The key requirement for us is that we don't send a packet larger than
    // the requested size.  We are not required to send packets of the
    // maximum size.  So, we can just ACK whatever size they request.
    // See RFC 1661 Section 6.1 Implementation Note.

    // Alternatively, we can NAK with our maximum send size if it is less
    // than the MRU which may allow the peer to reduce its receive packet
    // buffer space.  This might confuse some PPP implemntations, though,
    // so we'll keep things simple and ack everything.

    //
    // Exception: If the peer is requesting an unreasonably small MRU that
    // would in effect make it impossible to send them much of anything,
    // we NAK with our minimum allowed MRU size.
    //
    if( PeerRequestedMRU < pSession->dwMinimumMRU)
    {
        DEBUGMSG( ZONE_WARN, (TEXT( "PPP: WARNING - Peer requested MRU %u too small (< min %u)\n" ), PeerRequestedMRU, pSession->dwMinimumMRU));

        pOptData = pContext->abScratchOpt;
        USHORT_TO_BYTES(pOptData, (USHORT)pSession->dwMinimumMRU);
        *ppOptData = pOptData;
        *pCode = PPP_CONFIGURE_NAK;
    }
    else
    {
        //
        // We don't update the max send size unless it is less than the link
        // max send size.  Even though we accept the peer's larger MRU with an
        // ack, we thus won't ever use values larger than the link max send size.
        //
        if( PeerRequestedMRU <= pContext->linkMaxSendPPPInfoFieldSize)
        {
            DEBUGMSG( ZONE_LCP, (TEXT( "PPP: LCP-Peer MRU: %u (was %u)\n" ), PeerRequestedMRU, pContext->peer.MRU ));
            pContext->peer.MRU = PeerRequestedMRU;
        }
        // Packet OK - ACK it
        *pCode = PPP_CONFIGURE_ACK;
    }

    return dwResult;
}

/////////////   ACCM option negotiation callbacks //////////////

DWORD
lcpBuildACCMOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    OUT     PBYTE  pOptData,
    IN  OUT PDWORD pcbOptData)
//
//  Called to fill in the ACCM value field for the ACCM option
//  that is part of a configure-request we will send to the peer.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;

    PREFAST_ASSERT(*pcbOptData >= 4);

    DWORD_TO_BYTES(pOptData, pContext->DesiredACCM);
    *pcbOptData = 4;

    return dwResult;
}

DWORD
lcpAckACCMOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the ACCM option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;
    DWORD        ACCM;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 4);

    BYTES_TO_DWORD(ACCM, pOptData);

    //
    // The acked value will have been verified by the generic ACK code
    // to be identical to that we requested.
    //
    pContext->local.ACCM = ACCM;

    return dwResult;
}

DWORD
lcpNakACCMOptionCb(
    IN      PVOID  context,
    IN OUT  POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer NAKs the ACCM option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;
    DWORD        PeerSuggestedAccm;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 4);

    BYTES_TO_DWORD(PeerSuggestedAccm, pOptData);

    DEBUGMSG(ZONE_LCP, (TEXT("LCP: Peer is suggesting ACCM %u\n"), PeerSuggestedAccm));

    // If the peer NAKs our ACCM value, hopefully it is a superset,
    // of our request.  Add their bits to the set.

    pContext->DesiredACCM |= PeerSuggestedAccm;

    return dwResult;
}

DWORD
lcpRequestACCMOptionCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive a LCP configure request containing ACCM option,
//  or, when a LCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;
    DWORD        PeerRequestedAccm,
                 DesiredPeerAccm;

    if (pOptData == NULL)
    {
        pOptData = pContext->abScratchOpt;
        DWORD_TO_BYTES(pOptData, pContext->DesiredACCM);
        *ppOptData = pOptData;
        *pcbOptData = 4;
        *pCode = PPP_CONFIGURE_NAK;
    }
    else
    {
        //
        // Generic option length checking should have verified this.
        //
        ASSERT(cbOptData == 4);

        BYTES_TO_DWORD(PeerRequestedAccm, pOptData);

        // Check whether the peer accm includes all the characters in our set
        if ((~PeerRequestedAccm & pContext->DesiredACCM) == 0)
        {
            // It does. ACK the option and save it
            DEBUGMSG( ZONE_LCP, (TEXT( "PPP: LCP-Peer ACCM: %x\n" ), PeerRequestedAccm));
            pContext->peer.ACCM = PeerRequestedAccm;
            *pCode = PPP_CONFIGURE_ACK;
        }
        else
        {
            // Need to NAK to inform the peer about the characters we need escaped

            DesiredPeerAccm = pContext->DesiredACCM | PeerRequestedAccm;

            pOptData = pContext->abScratchOpt;
            DWORD_TO_BYTES(pOptData, DesiredPeerAccm);
            *ppOptData = pOptData;
            *pcbOptData = 4;
            *pCode = PPP_CONFIGURE_NAK;

            DEBUGMSG( ZONE_LCP, (TEXT("PPP: LCP - NAKing %x ACCM, suggesting %x\n"), PeerRequestedAccm, DesiredPeerAccm));
        }
    }

    return dwResult;
}

/////////////   Auth option negotiation callbacks //////////////


//
//  Array of authentication protocol information, used to translate
//  authentication protocols to their indices
//

typedef struct
{
    PCHAR   szName;
    USHORT  Protocol;
    BYTE    Algorithm;
} AuthProtocolInfo_t;

AuthProtocolInfo_t AuthProtocolInfoTable[] =
{
    {   "PAP",      PPP_PROTOCOL_PAP,       0                   },
    {   "CHAP-MD5", PPP_PROTOCOL_CHAP,      PPP_CHAP_MD5        },
    {   "CHAP-MS",  PPP_PROTOCOL_CHAP,      PPP_CHAP_MSEXT      },
    {   "CHAP-MSV2",PPP_PROTOCOL_CHAP,      PPP_CHAP_MSEXT_V2   },
    {   "EAP",      PPP_PROTOCOL_EAP,       0                   },
    {   "???",      0,                      0                   }
};

BOOL
authProtocolToIndex(
    IN  USHORT  Protocol,
    IN  BYTE    Algorithm, OPTIONAL
    OUT int     *pIndex)
//
//  Search through the protocol info table looking for the requested protocol
//  and algorithm.  Return TRUE if found, setting *pdwIndex to the protocol index.
//
{
    BOOL    bFound = FALSE;
    DWORD   iTable;

    *pIndex = -1;
    for (iTable = 0; AuthProtocolInfoTable[iTable].Protocol != 0; iTable++)
    {
        if (AuthProtocolInfoTable[iTable].Protocol == Protocol
        &&  AuthProtocolInfoTable[iTable].Algorithm == Algorithm)
        {
            DEBUGMSG( ZONE_LCP, (TEXT("PPP: LCP - Requested Auth Protocol is %hs\n"), AuthProtocolInfoTable[iTable].szName));
            *pIndex = iTable;
            bFound = TRUE;
            break;
        }
    }

    return bFound;
}

AuthProtocolInfo_t *
findAuthInfo(
    IN      PBYTE   pOptData,
    IN      DWORD   cbOptData,
    OUT     PUSHORT pProtocol,
    OUT     PBYTE   pAlgorithm,
    OUT     int    *pIndex)
//
//  Look up the auth protocol option specified by the data.
//
{
    AuthProtocolInfo_t *pInfo = NULL;
    int                index = -1;
    USHORT             Protocol = 0;
    BYTE               Algorithm = 0;

    if (cbOptData >= 2)
    {
        BYTES_TO_USHORT(Protocol, pOptData);
        if (cbOptData >= 3)
            Algorithm = pOptData[2];

        authProtocolToIndex(Protocol, Algorithm, &index);
    }

    if (index >= 0)
        pInfo = &AuthProtocolInfoTable[index];

    if (pProtocol)
    {
        *pProtocol = Protocol;
        *pAlgorithm = Algorithm;
    }

    if (pIndex)
        *pIndex = index;

    return pInfo;
}

void
lcpAuthToStringCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData,
    IN  OUT PSTR   *ppBuffer)
//
//  Debug pretty print routine for AUTH option data
//
{
    PSTR   pBuffer = *ppBuffer;
    USHORT Protocol = 0;
    BYTE   Algorithm = 0;
    AuthProtocolInfo_t *pAuthInfo;

    pAuthInfo = findAuthInfo(pOptData, cbOptData, NULL, NULL, NULL);

    strcpy(pBuffer, pAuthInfo ? pAuthInfo->szName : "???");
    pBuffer += strlen(pBuffer);
    *ppBuffer = pBuffer;
}

DWORD
lcpBuildAuthOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    OUT     PBYTE  pOptData,
    OUT     PDWORD pcbOptData)
//
//  Called to fill in the auth value field for the auth option
//  that is part of a configure-request we will send to the peer.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;

    ASSERT(pContext->session->bIsServer);

    // We should have reserved a sufficiently sized TX CR buffer
    // to always have enough space for our options.

    PREFAST_ASSERT(*pcbOptData >= 3);

    //
    // Auth protocol option value will have been set already.
    // If 0, it means that we have no auth protocol type left to try.
    //
    if (pContext->local.Auth.Protocol == 0)
    {
        if (pInfo->orlLocal == ORL_Required)
            dwResult = ERROR_CAN_NOT_COMPLETE;
        else
            dwResult = ERROR_PPP_SKIP_OPTION;
    }
    else
    {
        USHORT_TO_BYTES(pOptData, pContext->local.Auth.Protocol);
        *pcbOptData = 2;
        if (pContext->local.Auth.Protocol == PPP_PROTOCOL_CHAP)
        {
            // Add the CHAP algorithm byte
            pOptData[2] = pContext->local.Auth.Algorithm;
            *pcbOptData = 3;
        }
    }

    return dwResult;
}

DWORD
lcpAckAuthOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the auth protocol
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;

    return dwResult;
}

DWORD
lcpServerPickAuthProto(
    PLCPContext pContext)
{
    pppSession_t       *pSession = pContext->session;
    DWORD               bmAuthTypesAllowed = pSession->bmAuthTypesAllowed;
    int                 iTable;
    AuthProtocolInfo_t *pAuthInfo;
    DWORD               dwResult = NO_ERROR;

    ASSERT(pSession->bIsServer);

    pContext->iRequestedAuthProto = -1;
    pContext->local.Auth.Protocol = 0;

    //
    // Search through the auth supported types table and pick
    // the strongest authentication type that we support taat
    // has not been NAKed by the peer.
    //
    for (iTable = AUTH_BITNUM_EAP; TRUE; iTable--)
    {
        if (iTable < 0)
        {
            break;
        }

        if ((bmAuthTypesAllowed & (1<<iTable))
        &&  !(pContext->bmPeerNakedProtocols & (1 << iTable)))
        {
            pContext->iRequestedAuthProto = iTable;
            pAuthInfo = &AuthProtocolInfoTable[iTable];
            pContext->local.Auth.Protocol = pAuthInfo->Protocol;
            pContext->local.Auth.Algorithm = pAuthInfo->Algorithm;
            break;
        }
    }

    return dwResult;
}

DWORD
lcpNakAuthOptionCb(
    IN      PVOID  context,
    IN OUT  POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer NAKs the Auth protocol
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    RASPENTRY    *pRasEntry = &pSession->rasEntry;
    DWORD        dwResult = NO_ERROR,
                 bmAuthTypesAllowed = pSession->bmAuthTypesAllowed;
    USHORT       PeerSuggestedProtocol;
    BYTE         PeerSuggestedAlgorithm;
    int          iAuthProto;

    findAuthInfo(pOptData, cbOptData, &PeerSuggestedProtocol, &PeerSuggestedAlgorithm, &iAuthProto);

    if ((iAuthProto >= 0) && (bmAuthTypesAllowed & (1 << iAuthProto)))
    {
        // We support and allow the protocol requested by the peer, use it
        pContext->local.Auth.Protocol = PeerSuggestedProtocol;
        pContext->local.Auth.Algorithm = PeerSuggestedAlgorithm;
        pContext->iRequestedAuthProto = iAuthProto;
    }
    else
    {
        // We don't like the peer's suggestion, and he doesn't like the value we requested.
        // Add the value we requested to the list of values not to try again, then
        // find a new type.
        if (pContext->iRequestedAuthProto >= 0)
            pContext->bmPeerNakedProtocols |= 1 << pContext->iRequestedAuthProto;

        lcpServerPickAuthProto(pContext);
    }

    return dwResult;
}

void
lcpPickAuthProto(
    IN      PLCPContext  pContext,
    OUT     PBYTE        pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD       pcbOptData)
//
//  Pick an auth protocol to request the server to use.
//
{
    int                 iTable;
    AuthProtocolInfo_t *pAuthInfo;
    PBYTE               pOptData;
    DWORD               bmAuthTypesAllowed = pContext->session->bmAuthTypesAllowed;

    //
    //  Unrecognized or disallowed type, look for an alternate
    //  to suggest to the server.
    //
    for (iTable = AUTH_BITNUM_EAP; TRUE; iTable--)
    {
        if (iTable < 0)
        {
            //
            //  No supported auth type.  REJect the authentication option.
            //
            DEBUGMSG( ZONE_LCP, (TEXT("PPP: LCP - REJing Authentication protocol\n")));

            *pCode = PPP_CONFIGURE_REJ;
            break;
        }

        if (bmAuthTypesAllowed & (1<<iTable))
        {
            //
            //  We can use this type. NAK to suggest it.
            //
            pOptData = pContext->abScratchOpt;
            pAuthInfo = &AuthProtocolInfoTable[iTable];
            USHORT_TO_BYTES(pOptData, pAuthInfo->Protocol);
            *pcbOptData = 2;
            if (pAuthInfo->Protocol == PPP_PROTOCOL_CHAP)
            {
                pOptData[2] = pAuthInfo->Algorithm;
                *pcbOptData = 3;
            }
            *ppOptData = pOptData;
            *pCode = PPP_CONFIGURE_NAK;
            break;
        }
    }
}

DWORD
lcpRequestAuthOptionCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive a LCP configure request containing AUTH protocol option,
//  or, when an LCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    RASPENTRY    *pRasEntry = &pSession->rasEntry;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;
    int          iPeerRequestedAuthProto;
    USHORT       PeerRequestedProtocol;
    BYTE         PeerRequestedAlgorithm;
    DWORD               bmAuthTypesAllowed = pSession->bmAuthTypesAllowed;
    AuthProtocolInfo_t *pAuthInfo;

    if (pOptData == NULL)
    {
        // The peer did not have an auth option in its config-request,
        // and we want to do authentication.

        lcpPickAuthProto(pContext, pCode, ppOptData, pcbOptData);
        ASSERT(*pCode == PPP_CONFIGURE_NAK);
    }
    else
    {
        //
        //  Search through the protocol info table looking for the requested protocol
        //  and algorithm.
        //
        pAuthInfo = findAuthInfo(pOptData, cbOptData, &PeerRequestedProtocol, &PeerRequestedAlgorithm, &iPeerRequestedAuthProto);

        //
        //  If we don't recognize the protocol or if it is a type we are not allowing,
        //  then select an alternative and NAK it.  If there is no alternative then
        //  REJect it.
        //
        if (iPeerRequestedAuthProto == -1 
        ||  !(bmAuthTypesAllowed & (1<<iPeerRequestedAuthProto)))
        {
            //
            //  Unrecognized or disallowed type, look for an alternate
            //  to suggest to the server.
            //
            lcpPickAuthProto(pContext, pCode, ppOptData, pcbOptData);
        }
        else
        {
            //
            // Supported and allowed auth type, ack it
            //
            DEBUGMSG( ZONE_LCP, (TEXT("PPP: LCP - ACKing Authentication protocol %hs\n"), pAuthInfo->szName));
            pContext->peer.Auth.Protocol = PeerRequestedProtocol;
            pContext->peer.Auth.Algorithm = PeerRequestedAlgorithm;
            *pCode = PPP_CONFIGURE_ACK;
        }
    }

    return dwResult;
}

/////////////   Magic Number option negotiation callbacks //////////////

//
//  Note: We don't support sending configure-requests containing the magic number option.
//        So, we just need to ACK and save the peer's requested magic number.
//

#ifdef SUPPORT_SENDING_MAGIC_NUMBER_CR

DWORD
lcpBuildMagicOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    OUT     PBYTE  pOptData,
    IN  OUT PDWORD pcbOptData)
//
//  Called to fill in the Magic Number option
//  that is part of a configure-request we will send to the peer.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;
    DWORD        RandomNumber = 0;

    PREFAST_ASSERT(*pcbOptData >= 4);

    // 
    // Before this Configuration Option is requested, an implementation
    // MUST choose its Magic-Number.  It is recommended that the Magic-
    // Number be chosen in the most random manner possible in order to
    // guarantee with very high probability that an implementation will
    // arrive at a unique number.

    while (0 == RandomNumber)
    {
        CeGenRandom((PBYTE)&RandomNumber, sizeof(RandomNumber));
    }
    DWORD_TO_BYTES(pOptData, RandomNumber);
    *pcbOptData = sizeof(RandomNumber);

    return dwResult;
}

DWORD
lcpAckMagicOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the Magic option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;
    DWORD        MagicNumber;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 4);

    BYTES_TO_DWORD(MagicNumber, pOptData);

    //
    // The acked value will have been verified by the generic ACK code
    // to be identical to that we requested.
    //
    pContext->local.MagicNumber = MagicNumber;

    return dwResult;
}

DWORD
lcpNakMagicOptionCb(
    IN      PVOID  context,
    IN OUT  POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer NAKs the Magic option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    DWORD        dwResult = NO_ERROR;
    DWORD        PeerSuggestedMagicNumber;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    PREFAST_ASSERT(cbOptData == 4);

    BYTES_TO_DWORD(PeerSuggestedMagicNumber, pOptData);

    DEBUGMSG(ZONE_LCP, (TEXT("PPP: Peer is suggesting MagicNumber %u\n"), PeerSuggestedMagicNumber));

    //pContext->DesiredMagicNumber = PeerSuggestedMagicNumber;

    return dwResult;
}

#endif // SUPPORT_SENDING_MAGIC_NUMBER_CR

DWORD
lcpRequestMagicOptionCb(
    IN  OUT PVOID       context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive a LCP configure request containing Magic Number option,
//  or, when a LCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    pppSession_t *pSession = pContext->session;
    RASPENTRY    *pRasEntry = &pSession->rasEntry;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;
    DWORD        PeerRequestedMagicNumber;

    //
    // We never set ORL_Wanted for MagicNumber, so we should only be called
    // when the peer sends a config-request WITH the MRU option,
    // never without it.
    //
    // ASSERT(pOptData);

    //
    // Generic option length checking should have verified this.
    //
    ASSERT(cbOptData == 4);

    BYTES_TO_DWORD(PeerRequestedMagicNumber, pOptData);

    //
    // Per STD51 Section 6.4:
    //   "A Magic-Number of zero is illegal and MUST always be Nak'd, if it is
    //    not rejected outright."
    //
    if (PeerRequestedMagicNumber == 0)
    {
        // Peer implementation of magic numbers appears broken, best to abandon it.
        *pCode = PPP_CONFIGURE_REJ;
    }
    else
    {
#ifdef SUPPORT_SENDING_MAGIC_NUMBER_CR
        // STD51 Section 6.4:
        // When a Configure-Request is received with a Magic-Number Configuration Option,
        // the received Magic-Number is compared with the Magic-Number of the last
        // Configure-Request sent to the peer. If the two Magic-Numbers are different,
        // then the link is not looped-back, and the Magic-Number SHOULD be acknowledged.
        // If the two Magic-Numbers are equal, then it is possible, but not certain,
        // that the link is looped-back and that this Configure-Request is actually
        // the one last sent. To determine this, a Configure-Nak MUST be sent specifying
        // a different Magic-Number value.
        // TODO
#endif
        pContext->peer.MagicNumber = PeerRequestedMagicNumber;

            // Packet OK - ACK it
        *pCode = PPP_CONFIGURE_ACK;
    }

    return dwResult;
}

/////////////   PFC option negotiation callbacks //////////////


DWORD
lcpAckPFCOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the PFC option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    ASSERT(cbOptData == 0);

    pContext->local.bPFC = TRUE;

    return dwResult;
}

DWORD
lcpRequestPFCOptionCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive a LCP configure request containing PFC option,
//  or, when a LCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;

    if (pOptData == NULL)
    {
        // Peer did not send config-request with PFC option, and we want it.
        *pCode = PPP_CONFIGURE_NAK;
        *pcbOptData = 0;
    }
    else
    {
        //
        // Generic option length checking should have verified this.
        //
        ASSERT(cbOptData == 0);

        // Packet OK - ACK it
        pContext->peer.bPFC = TRUE;
        *pCode = PPP_CONFIGURE_ACK;
    }

    return dwResult;
}

/////////////   ACFC option negotiation callbacks //////////////


DWORD
lcpAckACFCOptionCb(
    IN      PVOID  context,
    IN  OUT POptionInfo pInfo,
    IN      PBYTE  pOptData,
    IN      DWORD  cbOptData)
//
//  Called when the peer ACKs the ACFC option
//  that we sent in a CR.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;

    //
    // The generic option handling code should have rejected any illegal sized option.
    //
    ASSERT(cbOptData == 0);

    pContext->local.bACFC = TRUE;

    return dwResult;
}

DWORD
lcpRequestACFCOptionCb(
    IN  OUT PVOID context,
    IN  OUT POptionInfo pInfo,
        OUT PBYTE       pCode,
    IN  OUT PBYTE       *ppOptData,
    IN  OUT PDWORD      pcbOptData)
//
//  Called when we receive a LCP configure request containing ACFC option,
//  or, when a LCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
    PLCPContext  pContext = (PLCPContext)context;
    DWORD        dwResult = NO_ERROR;
    PBYTE        pOptData = *ppOptData;
    DWORD        cbOptData= *pcbOptData;

    if (pOptData == NULL)
    {
        // Peer did not send config-request with PFC option, and we want it.
        *pCode = PPP_CONFIGURE_NAK;
        *pcbOptData = 0;
    }
    else
    {
        //
        // Generic option length checking should have verified this.
        //
        ASSERT(cbOptData == 0);

        // Packet OK - ACK it
        pContext->peer.bACFC = TRUE;
        *pCode = PPP_CONFIGURE_ACK;
    }

    return dwResult;
}


void
lcpResetPeerOptionValuesCb(
    IN      PVOID  context)
//
//  This function is called whenever a new configure-request is
//  received from the peer prior to processing the options therein
//  contained. This function must reset all options to their default
//  values, such that any not explicity contained within the configure
//  request will use the default setting.
//
{
    PLCPContext         pContext = (PLCPContext)context;

    //
    // Make sure that we don't try to send packets longer
    // than the max the MAC layer can handle.
    //
    pContext->peer.MRU = PPP_DEFAULT_MTU;
    if (pContext->linkMaxSendPPPInfoFieldSize < PPP_DEFAULT_MTU)
        pContext->peer.MRU = pContext->linkMaxSendPPPInfoFieldSize;

    pContext->peer.ACCM = LCP_DEFAULT_ACCM;
    pContext->peer.Auth.Protocol = 0;
    pContext->peer.Auth.Algorithm = 0;
    pContext->peer.MagicNumber = 0;
    pContext->peer.bPFC = FALSE;
    pContext->peer.bACFC = FALSE;
}

void
lcpOptionValueReset(
    PLCPContext pContext)
//
//  Initialize the option states at the beginning of an
//  LCP negotiation.
//
{
    pppSession_t       *pSession = pContext->session;

    DEBUGMSG( ZONE_LCP, ( TEXT( "PPP: lcpOptionValueReset\n" )));

    pContext->local.MRU = pContext->linkMaxRecvPPPInfoFieldSize;
    pContext->local.ACCM = LCP_DEFAULT_ACCM;

    pContext->iRequestedAuthProto = -1;
    pContext->local.Auth.Protocol = 0;
    pContext->local.Auth.Algorithm = 0;

    pContext->bmPeerNakedProtocols = 0;

    if (pSession->bIsServer
    &&  PPPServerGetSessionAuthenticationRequired(pSession))
    {
        (void)lcpServerPickAuthProto(pContext);
    }

    pContext->local.MagicNumber = 0;
    pContext->local.bPFC = FALSE;
    pContext->local.bACFC = FALSE;
}

OptionDescriptor lcpMRUOptionDescriptor =
{
    LCP_OPT_MRU,                    // type == Maximum Receive Unit (MRU)
    2,                              // fixedLength = 2
    "MRU",                          // name
    lcpBuildMRUOptionCb,
    lcpAckMRUOptionCb,
    lcpNakMRUOptionCb,
    NULL,                           // no special reject handling
    lcpRequestMRUOptionCb,
    lcpMRUOptionToStringCb      
};

OptionDescriptor lcpACCMOptionDescriptor =
{
    LCP_OPT_ACCM,                   // type == Async Control Character Map
    4,                              // fixedLength = 4
    "ACCM",                         // name
    lcpBuildACCMOptionCb,
    lcpAckACCMOptionCb,
    lcpNakACCMOptionCb,
    NULL,                           // no special reject handling
    lcpRequestACCMOptionCb,
    NULL        
};

OptionDescriptor lcpAuthOptionDescriptor =
{
    LCP_OPT_AUTH_PROTOCOL,          // type == Authentication Protocol
    OPTION_VARIABLE_LENGTH,              
    "Auth-Protocol",                // name
    lcpBuildAuthOptionCb,
    lcpAckAuthOptionCb,
    lcpNakAuthOptionCb,
    NULL,                           // no special reject handling
    lcpRequestAuthOptionCb,
    lcpAuthToStringCb       
};

OptionDescriptor lcpMagicOptionDescriptor =
{
    LCP_OPT_MAGIC_NUMBER,           // type == Magic Number
    4,                              // fixedLength = 4
    "Magic",                        // name
#ifdef SUPPORT_SENDING_MAGIC_NUMBER_CR
    lcpBuildMagicOptionCb,
    lcpAckMagicOptionCb,
    lcpNakMagicOptionCb,
#else
    NULL,
    NULL,
    NULL,
#endif
    NULL,                           // no special reject handling
    lcpRequestMagicOptionCb,
    NULL        
};

OptionDescriptor lcpPFCOptionDescriptor =
{
    LCP_OPT_PFC,                    // type == Protocol Field Compression
    0,                              // fixedLength = 0
    "PFC",                          // name
    NULL,                           // no build option handler since it is a flag
    lcpAckPFCOptionCb,
    NULL,                           // no nak option handler since it is a flag
    NULL,                           // no special reject handling
    lcpRequestPFCOptionCb,
    NULL                            // no pretty print routine since it is a flag
};

OptionDescriptor lcpACFCOptionDescriptor =
{
    LCP_OPT_ACFC,                   // type == Address/Control Field Compression
    0,                              // fixedLength = 0
    "ACFC",                         // name
    NULL,                           // no build option handler since it is a flag
    lcpAckACFCOptionCb,
    NULL,                           // no nak option handler since it is a flag
    NULL,                           // no special reject handling
    lcpRequestACFCOptionCb,
    NULL                            // no pretty print routine since it is a flag
};

DWORD
lcpOptionInit(
    PLCPContext pContext)
//
//  This function is called once during session initialization
//  to build the list of options that are supported.
//
{
    pppSession_t       *pSession = pContext->session;
    RASPENTRY          *pRasEntry = &pSession->rasEntry;
    DWORD              dwResult;
    OptionRequireLevel orlLocalAuth, orlPeerAuth;
    OptionRequireLevel orlLocalACCM = ORL_Wanted;
    OptionRequireLevel orlPeerACCM = ORL_Allowed;
    OptionRequireLevel orlPFC  = ORL_Wanted;
    OptionRequireLevel orlACFC = ORL_Wanted;

    orlLocalAuth = ORL_Unsupported;
    orlPeerAuth = ORL_Unsupported;
    if (!pSession->bIsServer)
    {
        // Client
        orlPeerAuth = ORL_Allowed;
        //
        // MPPE Encryption keys are derived from MSCHAP or EAP authentication info.
        // So if encryption is required, then authentication is required.
        //
        if (NEED_ENCRYPTION(pRasEntry))
            orlPeerAuth = ORL_Required;
    }

    // PPPoE does not allow ACCM or ACFC to be negotiated
    // PPPoE recommends that ACFC not be negotiated
    if (_tcscmp(pRasEntry->szDeviceType, RASDT_PPPoE) == 0)
    {
        orlLocalACCM = ORL_Unsupported;
        orlPeerACCM = ORL_Unsupported;
        orlPFC  = ORL_Allowed;
        orlACFC = ORL_Unsupported;
    }

    dwResult = PppFsmOptionsAdd(pContext->pFsm,
                        &lcpMRUOptionDescriptor,    ORL_Allowed,     ORL_Allowed,
                        &lcpACCMOptionDescriptor,   orlLocalACCM,    orlPeerACCM,
                        &lcpAuthOptionDescriptor,   orlLocalAuth,    orlPeerAuth,
#ifdef SUPPORT_SENDING_MAGIC_NUMBER_CR
                        &lcpMagicOptionDescriptor,  ORL_Wanted,      ORL_Allowed,
#else
                        &lcpMagicOptionDescriptor,  ORL_Unsupported, ORL_Allowed,
#endif
                        &lcpPFCOptionDescriptor,    orlPFC,          ORL_Allowed,
                        &lcpACFCOptionDescriptor,   orlACFC,         ORL_Allowed,
                        NULL);

    return dwResult;
}
