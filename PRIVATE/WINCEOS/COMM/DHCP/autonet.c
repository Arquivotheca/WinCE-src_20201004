//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**                   Microsoft Windows                                     **/
/*****************************************************************************/

/*
    autonet.c

  DESCRIPTION:
    Automatic IP configuration functions


*/

#include "dhcpp.h"
#include "dhcp.h"
#include "protocol.h"
#include "icmpif.h"
#include "netui.h"

#define DHCP_IPAUTOCONFIGURATION_ATTEMPTS   20

void ProcessAutoIP(CTETimer *pCteTimer, DhcpInfo *pDhcp);
extern DhcpInfo *FindDhcp(DhcpInfo *pDhcp, PTSTR pName);  // dhcp.c
extern uint DerefDhcpInfo(DhcpInfo *pDhcp);         // dhcp.c
extern void CloseDhcpSocket(DhcpInfo *pDhcp);       // dhcp.c
extern STATUS SetDhcpConfig(DhcpInfo *pDhcp);       // dhcp.c
extern STATUS PutNetUp(DhcpInfo *pDhcp);            // dhcp.c
extern BOOL CanUseCachedLease(DhcpInfo * pDhcp);    // dhcp.c
extern void StartDhcpDialogThread(DWORD dwMsg);     // dhcp.c

#define DHCP_ICMP_WAIT_TIME     1000
#define DHCP_ICMP_RCV_BUF_SIZE  2048
#define DHCP_ICMP_SEND_MESSAGE  "DHCPC"

typedef ULONG (* PFN_VXDECHOREQUEST)(void  * InBuf, ulong * InBufLen, void  * OutBuf, ulong * OutBufLen);

//
// Function to call ARP's SetDHCPNTE for auto IP - caller must have pDhcp->Lock
//
BOOL
SetAutoCfgDHCPNTE(
    DhcpInfo * pDhcp
    )
{
    BOOL RetStatus = FALSE;
    DWORD cAddr;

    // Verify that caller owns lock
    ASSERT( pDhcp->Lock.OwnerThread == (HANDLE )GetCurrentThreadId() );

    if (0 == (pDhcp->SFlags & DHCPSTATE_DHCPNTE_SET)) {
        pDhcp->SFlags |= DHCPSTATE_DHCPNTE_SET;
        RetStatus = pDhcp->pProtocolContext->pfnSetNTE(pDhcp->NteContext, NULL, NULL, &cAddr);
        if (!RetStatus)
            pDhcp->SFlags &= ~DHCPSTATE_DHCPNTE_SET;
    }

    return RetStatus;
}


//
// Ping the specified IP address with "DHCPC".
//
BOOL
PingDhcp(
    DWORD dwIPAddr
    )
{
    DWORD inlen;
    DWORD outlen;
    DWORD status;
    BYTE ReqBuffer[sizeof(ICMP_ECHO_REQUEST)+8];
    PICMP_ECHO_REQUEST pReq;
    BYTE ReplyBuffer[DHCP_ICMP_RCV_BUF_SIZE];
    PICMP_ECHO_REPLY EchoReplies;
    PFN_VXDECHOREQUEST pfnEchoRequest;
    HANDLE hTcpstk;
    BOOL bRet;
#ifdef DEBUG
    TCHAR Addr[32];
    TCHAR Reply[32];
#endif

    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:+PingDhcp\n"));

    hTcpstk = LoadLibrary(L"tcpstk.dll");
    if (hTcpstk == NULL) {
        DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-PingDhcp: LoadLibrary(tcpstk.dll) failed\n"));
        return FALSE;
    }

    bRet = FALSE;
    pfnEchoRequest = (PFN_VXDECHOREQUEST)GetProcAddress(hTcpstk, L"VXDEchoRequest");
    if (pfnEchoRequest == NULL) {
        DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:PingDhcp: GetProcAddress(VXDEchoRequest) failed\n"));
        goto pd_exit;
    }

    pReq = (PICMP_ECHO_REQUEST)ReqBuffer;
    pReq->Address = dwIPAddr;
    pReq->Timeout = DHCP_ICMP_WAIT_TIME;
    pReq->DataOffset = sizeof(ICMP_ECHO_REQUEST);
    pReq->DataSize = strlen(DHCP_ICMP_SEND_MESSAGE);
    strcpy(ReqBuffer+sizeof(ICMP_ECHO_REQUEST), DHCP_ICMP_SEND_MESSAGE);
    pReq->OptionsValid = 0;
    pReq->Ttl = 1;
    pReq->Tos = 0;
    pReq->Flags = 0;
    pReq->OptionsOffset = 0;
    pReq->OptionsSize = 0;

    inlen = sizeof(ReqBuffer);
    outlen = sizeof(ReplyBuffer);

    status = pfnEchoRequest(
                (void *)ReqBuffer,
                &inlen,
                (void *)ReplyBuffer,
                &outlen
                );

    if (status) {
        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:PingDhcp: VXDEchoRequest failed %d\n", status));
        goto pd_exit;
    }

    EchoReplies = (PICMP_ECHO_REPLY)ReplyBuffer;
    if (EchoReplies->Status) {
        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:PingDhcp: Echo status = %d\n", EchoReplies->Status));
        goto pd_exit;
    }

    if (EchoReplies->Address != dwIPAddr) {
        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:PingDhcp: Echo IPAddr(%s) != Request IPAddr(%s)\n",
            AddrToString(EchoReplies->Status, Reply), AddrToString(dwIPAddr, Addr)));
        goto pd_exit;
    }

    bRet = TRUE;

pd_exit:
    FreeLibrary(hTcpstk);
    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-PingDhcp returning %s\n", bRet ? L"TRUE" : L"FALSE"));
    return bRet;
}   // PingDhcp

//
// If there were no DHCP servers and this interface has a leased IP addr, then ping
// the default gateway to make sure we are on the same subnet. This will avoid going
// to auto IP prematurely.
//
// This function will bring up the interface and leave it up if the default gateway
// could be ping'd.
//
STATUS
CouldPingGateway(
    DhcpInfo * pDhcp
    )
{
    STATUS  Status;
#ifdef DEBUG
    TCHAR Addr[32];
    TCHAR Gateway[32];
#endif

    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:+CouldPingGateway\n"));

    if ((pDhcp->IPAddr == 0) || (pDhcp->Gateway == 0)) {
        DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-CouldPingGateway: IPAddr = %s, Gateway = %s\n",
            AddrToString(pDhcp->IPAddr, Addr), AddrToString(pDhcp->Gateway, Gateway)));
        return DHCP_FAILURE;
    }

    if (!CanUseCachedLease(pDhcp)) {
        DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-CouldPingGateway: In T2 stage!\n"));
        return DHCP_FAILURE;
    }

    //StartDhcpDialogThread(NETUI_GETNETSTR_CACHED_LEASE);

    if (DHCP_SUCCESS != (Status = PutNetUp(pDhcp))) {
        DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-CouldPingGateway: PutNetUp(%s) failed\n",
            AddrToString(pDhcp->IPAddr, Addr)));
        return Status;
    }

    if (PingDhcp(pDhcp->Gateway)) {
        StartDhcpDialogThread(NETUI_GETNETSTR_CACHED_LEASE);
        DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-CouldPingGateway: Received response from default gateway\n"));
        return DHCP_SUCCESS;
    }

    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-CouldPingGateway: No response to ping\n"));    
    TakeNetDown(pDhcp, FALSE, TRUE);      // retain IPAddr
    return DHCP_FAILURE;

}   // CouldPingGateway


//
// Schedule a CTE timer so ProcessAutoIP can look for a DHCP server again.
//
void
StartAutoIPTimer(
    DhcpInfo * pDhcp,
    DWORD AutoInterval
    )
{
    CTEStartTimer(&pDhcp->AutonetTimer, AutoInterval * 1000, (CTEEventRtn)ProcessAutoIP, pDhcp);
    DEBUGMSG(ZONE_AUTOIP, (L"DHCP:StartAutoIPTimer - AutoIP event scheduled\n"));
}

//
// When we autoconfigure our IP address we must continuously look for a DHCP server appearing on our net.
// The default interval is 5 minutes.
//
void
ProcessAutoIP(
    CTETimer *pCteTimer, 
    DhcpInfo *pDhcp
    )
{
    int     cPkt;
    STATUS  Status;
    DhcpPkt Pkt;
    uint    IPAddr;

    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:+ProcessAutoIP\n"));

    Status = DHCP_SUCCESS;
    if (FindDhcp(pDhcp, NULL)) {
        if (SetAutoCfgDHCPNTE(pDhcp)) {
            if (DHCP_SUCCESS == (Status = DhcpInitSock(pDhcp, 0))) {
    
                //
                // Save current auto config IP address and set dhcp IP address to 0, to follow spec.
                // Neither ciaddr nor DHCP_REQ_IP_OP should reflect the auto config IP address; they
                // should both be 0.
                //
                IPAddr = pDhcp->IPAddr;                
                pDhcp->IPAddr = 0;
                BuildDhcpPkt(pDhcp, &Pkt, DHCPDISCOVER, NEW_PKT_FL, 
                    pDhcp->ReqOptions, &cPkt);

                pDhcp->IPAddr = IPAddr;

                if (pDhcp->SFlags & DHCPSTATE_WIRELESS_ADHOC)
                {                  
                    //
                    //  Adhoc wireless, do 4 retries (the very first time).
                    //  Otherwise 1 retry.
                    //
                    
                    pDhcp->SFlags &= ~DHCPSTATE_WIRELESS_ADHOC;
                    Status = SendDhcpPkt(pDhcp, &Pkt, cPkt, DHCPOFFER, DFT_LOOP_FL|BCAST_FL);
                }
                else
                {
                    Status = SendDhcpPkt(pDhcp, &Pkt, cPkt, DHCPOFFER, ONCE_FL|BCAST_FL);
                }

                if (DHCP_DELETED == Status) {
                    // bail out!
                    goto pai_exit;
                }
                    
                
                if (DHCP_SUCCESS == Status) {
                    //
                    // If the OFFER is from a DHCP allocator, then we should request our auto config IP address
                    // It is a DHCP allocator if it is on the same subnet as auto config.
                    //
                    if ((pDhcp->IPAddr & pDhcp->SubnetMask) == pDhcp->AutoSubnet) {
                        pDhcp->IPAddr = IPAddr;
                        BuildDhcpPkt(pDhcp, &Pkt, DHCPREQUEST, RIP_PKT_FL, 
                            pDhcp->ReqOptions, &cPkt);

                        Status = SendDhcpPkt(pDhcp, &Pkt, cPkt, DHCPACK, BCAST_FL | DFT_LOOP_FL);

                        if (DHCP_DELETED == Status) {
                            // bail out
                            goto pai_exit;
                            
                        } else if (DHCP_SUCCESS == Status) {
                            DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ProcessAutoIP - Leased auto cfg IP addr with ICS DHCP allocater/server.\n"));
                            SetDhcpConfig(pDhcp);
                            goto pai_exit;

                        } else {
                            Status = DHCP_SUCCESS;  // Error, fall through to switch-over case
                            DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ProcessAutoIP - Trouble with ICS DHCP allocater/server.\n"));
                        }
                    }
                }

                CloseDhcpSocket(pDhcp);
                ClearDHCPNTE(pDhcp);
        
                if (DHCP_SUCCESS == Status) {
                    //
                    // A DHCP server has appeared on the net. We need to discard the auto IP address and DHCP for one.
                    //
                    DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ProcessAutoIP - Saw a DHCP server, discarding auto IP\n")); 
                    //
                    // Remember what was in this DHCPOFFER so we don't send another DISCOVER
                    SetDhcpConfig(pDhcp);
    
                    pDhcp->IPAddr = IPAddr;
                    TakeNetDown(pDhcp, FALSE, TRUE);  // FALSE == retain IPAddr from the DHCPOFFER

                    pDhcp->SFlags &= ~DHCPSTATE_AUTO_CFG_IP; // IPAddr is not autocfg

                    RenewDHCPAddr(
                        pDhcp->pProtocolContext,
                        pDhcp->Name,
                        pDhcp->Nte,
                        pDhcp->NteContext,
                        pDhcp->ClientHWAddr,
                        pDhcp->ClientHWAddrLen
                        );

                    CTEFreeLock(&pDhcp->Lock, 0);
                    DerefDhcpInfo(pDhcp);

                    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-ProcessAutoIP\n"));
                    return;
                } else {
                    //
                    // No DHCP server on this net, reschedule this timer to keep checking.
                    //
                    StartAutoIPTimer(pDhcp, pDhcp->AutoInterval);
                }
            } else {
                DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ProcessAutoIP - DhcpInitSock failed!\n"));
            }
        } else {
            DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ProcessAutoIP - SetDHCPNTE failed!\n"));
        }

        CTEFreeLock(&pDhcp->Lock, 0);
        DerefDhcpInfo(pDhcp);

    } else {
        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ProcessAutoIP - FindDhcp failed!\n"));
    }
    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-ProcessAutoIP\n"));
    return;

pai_exit:
    CloseDhcpSocket(pDhcp);
    ClearDHCPNTE(pDhcp);
    CTEFreeLock(&pDhcp->Lock, 0);
    DerefDhcpInfo(pDhcp);
    return;

}   // ProcessAutoIP


//extern DhcpInfo    *v_pCurrent;

//
// This function is called by the IP component to indicate the status of a gratuitous ARP
// (i.e. "no response" is good. A response usually implies an IP address conflict)
//
void
ARPResult(
    IPAddr IPAddr,
    uint Result
    )
{
    DhcpInfo *pDhcp;

    DEBUGMSG(ZONE_AUTOIP, (L"DHCP:+ARPResult\n"));

    CTEGetLock(&v_GlobalListLock, 0);
    for (pDhcp = (DhcpInfo *)v_EstablishedList.Flink;
         pDhcp != (DhcpInfo *)&v_EstablishedList;
         pDhcp = (DhcpInfo *)pDhcp->DhcpList.Flink)
    {

        if (pDhcp->IPAddr == IPAddr) {
            pDhcp->ARPResult = Result;
            if (pDhcp->ARPEvent) {
                DEBUGMSG(ZONE_AUTOIP, (L"DHCP:ARPResult - setting event\n"));
                SetEvent(pDhcp->ARPEvent);
            }
            CTEFreeLock(&v_GlobalListLock, 0);
            return;
        }
    }
    CTEFreeLock(&v_GlobalListLock, 0);
    
    DEBUGMSG(ZONE_AUTOIP, (L"DHCP:-ARPResult\n"));
}   // AutoIP_ARPResult


#define GRAND_HASHING_RETRIES 5 

//================================================================================
//  GrandHashing does all the work.  It updates seed and returns an ip address
//  is in the specified subnet (with required mask) based on the random # and the
//  hardware address.
//================================================================================
uint                            //  Ip address o/p after hashing
GrandHashing(
    IN      uchar * HwAddress,  //  Hardware addres of the card
    IN      uint    HwLen,      //  Hardware length
    IN OUT  uint *  Seed,       //  In: orig value, Out: final value
    IN      uint    Mask,       //  Subnet mask to generate ip addr in
    IN      uint    Subnet      //  Subnet address to generate ip addr in
    )
{
    DWORD   Hash, Shift;
    uint    Result;
    int     cRetries;
    uchar * Addr;
    uint    Len;

    if (*Seed == 0) {
        *Seed = (uint)CeGetRandomSeed();
    }

    for (cRetries = GRAND_HASHING_RETRIES; cRetries; cRetries--) {
        Addr = HwAddress;
        Len = HwLen;
        *Seed = (*Seed)*1103515245 + 12345 ;//  Random # generator as in K&R
        Hash = (*Seed) >> 16 ;
        Hash <<= 16;
        *Seed = (*Seed)*1103515245 + 12345 ;//  Random # generator as in K&R
        Hash += (*Seed) >> 16;
    
        // Now Hash contains the 32 bit Random # we need.
        Shift = Hash % sizeof(uint) ;      //  Decide the # of bytes to shift hw-address
    
        while( Len -- ) {
            Hash += (*Addr++) << (8 * Shift);
            Shift = (Shift + 1 )% sizeof(uint);
        }

        Result = Hash & ~Mask;

        // Check for bcast addresses
        if (Result && (Result != ~Mask)) {
            return Result | Subnet;       //  Now restrict hash to be in required subnet
        }
    }

    // In the highly unlikely case that 5 consecutive hashes yielded bcast addresses,
    // return a non-bcast address
    return (GRAND_HASHING_RETRIES & ~Mask) | Subnet;
}


//
// This function is called when the DHCP client has not received any response from a DHCP server
//
STATUS
AutoIP(
    DhcpInfo *pDhcp
    )
{
    uint i;
    uint Result;
    uint Seed;
    uint HwLen;
    uint AttemptedAddress;
    uint Mask;
    uint Subnet;
    uchar * HwAddress;
    uint SaveIPAddr;
    uint AutoInterval;
    uint SetAddrResult;
#ifdef DEBUG
    TCHAR Addr[32];
#endif

    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:+AutoIP\n"));

    pDhcp->SFlags &= ~DHCPSTATE_AUTO_CFG_IP;    // IPAddr not autoconfigured.

    if (pDhcp->Flags & DHCPCFG_FAST_AUTO_IP_FL) {
        pDhcp->Flags &= ~DHCPCFG_FAST_AUTO_IP_FL;
    } else {
        // If there is no DHCP server out there and we have a valid IPAddr, ping the default
        // gateway to make sure we are still on the same subnet.
        if (DHCP_SUCCESS == CouldPingGateway(pDhcp)) {
            DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-AutoIP - Still on DHCP server subnet\n"));
            return DHCP_SUCCESS;
        }
    }

    Mask      = pDhcp->AutoMask;
    Subnet    = pDhcp->AutoSubnet;
    HwAddress = pDhcp->ClientHWAddr;
    HwLen     = pDhcp->ClientHWAddrLen;
    Seed      = pDhcp->AutoSeed;
    SaveIPAddr = pDhcp->IPAddr;
    Result = ERROR_GEN_FAILURE;

    //
    // Tell tcpstk we are doing DHCP + AutoCfg
    //
    if (FALSE == SetAutoCfgDHCPNTE(pDhcp)) {
        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:AutoIP - SetDHCPNTE failed!\n"));
        goto ai_exit;
    }

    if (pDhcp->AutoIPAddr) {
        //
        // Use our previously autoconfigured IP address
        //
        AttemptedAddress = pDhcp->AutoIPAddr;
    } else {
        AttemptedAddress = GrandHashing( HwAddress, HwLen, &Seed, Mask, Subnet );
    }

    i = DHCP_IPAUTOCONFIGURATION_ATTEMPTS;

    do {
        if (pDhcp->SFlags & DHCPSTATE_SAW_DHCP_SRV) {
            //
            // If we ever see a DHCP server on our net, then abort IP autoconfiguration immediately
            //
            DEBUGMSG(ZONE_AUTOIP, (L"DHCP:AutoIP:DHCP server on net. Aborting autoconfig\n"));
            goto ai_exit;
        }

        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:AutoIP:Trying autoconfig address: %s\n", AddrToString(AttemptedAddress, Addr)));

        if (((AttemptedAddress & DHCP_RESERVED_AUTOCFG_MASK) == DHCP_RESERVED_AUTOCFG_SUBNET) ||
            ((AttemptedAddress & DHCP_RESERVED_AUTOCFG_MASK) == DHCP_IPAUTOCONFIGURATION_DEFAULT_SUBNET)) {
            // address is in reserved range, dont use it..
            goto ai_next;
        } 

        pDhcp->ARPResult = ERROR_SUCCESS;
        pDhcp->IPAddr = AttemptedAddress;
        ResetEvent(pDhcp->ARPEvent);

        SetAddrResult = pDhcp->pProtocolContext->pfnSetAddr((ushort)pDhcp->NteContext, NULL, pDhcp->IPAddr, Mask, 0);

        if (SetAddrResult == IP_PENDING)
        {
            //
            // Among other things, setting the NTE addr causes ARP to probe for existence of this IP addr.
            // Just wait a few seconds to see if it is a dup.
            //
            WaitForSingleObject(pDhcp->ARPEvent, ARP_RESPONSE_WAIT);
            
            
            SetAddrResult = IP_SUCCESS;
        }

        if (SetAddrResult == IP_SUCCESS)
        {
            if (ERROR_SUCCESS == pDhcp->ARPResult)
            {
                //
                //  If we get here from fast AutoIP for Adhoc wireless
                //  connection, then wait 10 seconds before starting
                //  the DHCP process.
                //  If the peer is desktop, it may have connect damping that can prevent
                //  us from getting to its dhcp server.
                //
                if (pDhcp->SFlags & DHCPSTATE_WIRELESS_ADHOC)
                { 
                    AutoInterval = 10;
                }
                else                    
                {                
                    AutoInterval = 2;            // Send an additional DHCP discover right away.
                }                    
                    
                pDhcp->AutoIPAddr   = AttemptedAddress;
                pDhcp->AutoSeed     = Seed;    
                pDhcp->SubnetMask   = Mask;
                pDhcp->SFlags |= DHCPSTATE_AUTO_CFG_IP;     // IPAddr is autoconfigured.

                ASSERT(!(DHCPSTATE_AFD_INTF_UP_FL & pDhcp->SFlags));
                pDhcp->SFlags |= DHCPSTATE_AFD_INTF_UP_FL;                
                CallAfd(AddInterface)(pDhcp->Name, pDhcp->Nte, 
                    pDhcp->NteContext, 0, pDhcp->IPAddr, 
                    pDhcp->SubnetMask, 0, pDhcp->DNS, 0, pDhcp->WinsServer); 
                Result = DHCP_SUCCESS;

                StartAutoIPTimer(pDhcp, AutoInterval);
                break;
            }
            else
            {
                //
                // Tell IP to stop using the address and mark this NTE for another try
                //
                pDhcp->pProtocolContext->pfnSetAddr((ushort)pDhcp->NteContext, NULL, 0, 0, 0);
                pDhcp->pProtocolContext->pfnSetNTE(pDhcp->NteContext, NULL, NULL, &Seed);
            }
        }
        else
        {
            DEBUGMSG(ZONE_AUTOIP, (L"DHCP:AutoIP - IPSetNTEAddr failed!\n"));
            i = 1;
        }
        
ai_next:
        AttemptedAddress = GrandHashing( HwAddress, HwLen, &Seed, Mask, Subnet );
        i --;

    } while (i);

    if (0 == i) {                      //  Tried everything and still could not match
        pDhcp->pProtocolContext->pfnSetAddr((ushort)pDhcp->NteContext, NULL, 0, 0, 0);
        DEBUGMSG(ZONE_AUTOIP, (L"DHCP:AutoIP - Gave up autoconfiguration\n"));
    }

ai_exit:
    if (Result) {
        pDhcp->IPAddr = SaveIPAddr;
    }
    ClearDHCPNTE(pDhcp);
    DEBUGMSG(ZONE_FUNCTION|ZONE_AUTOIP, (L"DHCP:-AutoIP %d\n", Result));
    return Result;
}   // AutoIP



