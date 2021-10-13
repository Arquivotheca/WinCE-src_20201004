//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//-***********************************************************************
//
//  This file contains all of the code that sits under an IP interface
//  and does the necessary multiplexing/demultiplexing for WAN connections.
//
//-***********************************************************************

#include "windows.h"
#include "cxport.h"
#include "types.h"

// IP Header files to bind to the LLIP interface
#include "ndis.h"
#include "ntddip.h"
#include "ip.h"
#include "llipif.h"
#include "tdiinfo.h"
#include "ipinfo.h"
#include "llinfo.h"
#include "tdistat.h"
#include "cellip.h"

#include "memory.h"
#include "afdfunc.h"
#include "cclib.h"

#include "iphlpapi.h"

// VJ Compression Include Files

#define _TCPIP_H_NOIP
#include "tcpip.h"
#include "vjcomp.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ncp.h"
#include "mac.h"
#include "ip_intf.h"
#include "ipv6intf.h"
#include "crypt.h"
#include "pppserver.h"
#include "auth.h"
#include "util.h"
#include "debug.h"

#include <ndistapi.h>
#include <cenet.h>

// Function pointers into the upper layer IP stack

HMODULE         g_hTcpStkModule;
IPRcvRtn        IPRcv ;
IPTDCmpltRtn    IPTDComplete ;
IPTxCmpltRtn    IPSendComplete ;
IPStatusRtn     IPStatus ;
IPRcvCmpltRtn   IPRcvComplete ;
IPSetNTEAddrRtn IPSetNTEAddr ;
CEIPAddInterfaceRtn	g_pfnIPAddInterface;
CEIPDelInterfaceRtn	g_pfnIPDelInterface;

BOOL
GetIPProcAddresses(void)
//
//	Initialize function pointers into tcpstk.dll for adding
//	and removing interfaces.
//
{
	BOOL bOk = TRUE;
	DWORD dwResult;

	if (g_hTcpStkModule == NULL)
	{
		dwResult = CXUtilGetProcAddresses(TEXT("tcpstk.dll"), &g_hTcpStkModule,
						TEXT("CEIPAddInterface"), &g_pfnIPAddInterface,
						TEXT("CEIPDelInterface"), &g_pfnIPDelInterface,
						NULL);

		if (dwResult != NO_ERROR)
		{
			RETAILMSG(1, (TEXT("PPP: ERROR: Can't find CEIPAddInterface or CEIPDelInterface in tcpstk.dll!\n")));
			bOk = FALSE;
		}
	}

	return bOk;
}

#define IS_BCAST(x) ((((ulong)(x)) & 0x000000ffL) == 0x000000ffL)

//
// Local Functions
//

static  BOOL
IPConvertStringToAddress( WCHAR *AddressString, DWORD *AddressValue );

/******************************************************************************

 @doc INTERNAL

 @api ULONG | IPGetNetMask | Get the netmask for an address

 @parm ULONG | Addr | an IP address

 @rdesc ULONG | The netmask

********************************************************************/
ULONG
IPGetNetMask(ULONG Addr)
{
    ULONG   AddrType;

	//
	// Get the high byte of the address, which specifies the type
	// (A, B, C, D, E) or IP address.
	//
    AddrType = Addr >> 24;

    if (AddrType <= 127)
    {
		// Class A: Network address is upper 8 bits of an IP address
        return 0xff000000;
    }
    else if (AddrType <= 191)
    {
		// Class B: Network address is upper 16 bits of an IP address
        return 0xffff0000;
    }
    else
    {
		// Class C: Network address is upper 24 bits on an IP addres
        // Don't sweat the wierd stuff (class D or E, ...)

        return 0xffffff00;
    }

}

#ifdef DEBUG
#define PROTOCOL_TCP	6		// from tcpipw\h\tcp.h
#define IP_VER_FLAG		0xF0	// from tcpipw\ip\ipdef.h

BOOL
DumpIP (BOOL Direction, PBYTE Buffer, DWORD Len)
{
	IPHeader IPHdr;
	TCPHeader TCPHdr;

	if (Len < sizeof(IPHeader)) {
		return FALSE;
	}

	memcpy ((char *)&IPHdr, Buffer, sizeof(IPHdr));

	if (IPHdr.iph_protocol == PROTOCOL_TCP) {
		memcpy ((char *)&TCPHdr, Buffer + (IPHdr.iph_verlen & 0x0F)*4, sizeof(TCPHeader));
		NKDbgPrintfW (TEXT("%hs TCP %hs%hs%hs%hs%hs%hs, %d.%d.%d.%d(%d) => %d.%d.%d.%d(%d) Seq=%d-%d Ack=%d Win=%d\r\n"),
					  Direction ? "XMIT" : "RECV",
					  TCPHdr.tcp_flags & TCP_FLAG_URG ? "U" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_ACK  ? "A" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_PUSH ? "P" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_RST  ? "R" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_SYN  ? "S" : ".",
					  TCPHdr.tcp_flags & TCP_FLAG_FIN  ? "F" : ".",
					  IPHdr.iph_src & 0xFF,
					  (IPHdr.iph_src >> 8) & 0xFF,
					  (IPHdr.iph_src >> 16) & 0xFF,
					  (IPHdr.iph_src >> 24) & 0xFF,
					  net_short(TCPHdr.tcp_src),
					  IPHdr.iph_dest & 0xFF,
					  (IPHdr.iph_dest >> 8) & 0xFF,
					  (IPHdr.iph_dest >> 16) & 0xFF,
					  (IPHdr.iph_dest >> 24) & 0xFF,
					  net_short(TCPHdr.tcp_dest),
					  net_long(TCPHdr.tcp_seq),
					  net_long(TCPHdr.tcp_seq) + net_short(IPHdr.iph_length) -
					  ((IPHdr.iph_verlen & 0x0F)*4 +
					   (net_short(TCPHdr.tcp_flags) >> 12)*4),
					  net_long(TCPHdr.tcp_ack),
					  net_short(TCPHdr.tcp_window));
		return FALSE;
	} else {
		NKDbgPrintfW (TEXT("%hs IP ver=%d hlen=%d tos=0x%X len=%d id=%d offset=%d ttl=%d prot=%d xsum=%d Src=%d.%d.%d.%d Dest=%d.%d.%d.%d\r\n"),
					  Direction ? "XMIT" : "RECV",
					  (IPHdr.iph_verlen & IP_VER_FLAG) >> 4,
					  (IPHdr.iph_verlen & 0x0F)*4,
					  IPHdr.iph_tos,
					  net_short(IPHdr.iph_length),
					  net_short(IPHdr.iph_id), net_short(IPHdr.iph_offset),
					  IPHdr.iph_ttl, IPHdr.iph_protocol, net_short(IPHdr.iph_xsum),
					  IPHdr.iph_src & 0xFF,
					  (IPHdr.iph_src >> 8) & 0xFF,
					  (IPHdr.iph_src >> 16) & 0xFF,
					  (IPHdr.iph_src >> 24) & 0xFF,
					  IPHdr.iph_dest & 0xFF,
					  (IPHdr.iph_dest >> 8) & 0xFF,
					  (IPHdr.iph_dest >> 16) & 0xFF,
					  (IPHdr.iph_dest >> 24) & 0xFF);

	}

	return FALSE;
}
#endif

NDIS_STATUS
PppSendIPvX(
    PVOID		 Context,       // Pointer to ppp context.
    PNDIS_PACKET Packet,        // Pointer to packet to be transmitted.
	DWORD		 ipProto)		// 4 or 6
{
	PPP_CONTEXT *pContext = (PPP_CONTEXT *)Context;
	pppSession_t   *s_p = (pppSession_t *)(pContext->Session);
	NDIS_STATUS	 Status = NDIS_STATUS_SUCCESS;

	ASSERT(ipProto == 4 || ipProto == 6);

	//
	// Invoke packet debug extension if it is loaded.
	//
	if (g_pfnLogTxNdisPacket)
	{
		BOOL bDoSend;

		bDoSend = g_pfnLogTxNdisPacket(L"PPP", pContext->AdapterName, ipProto == 4 ? L"IP" : L"IPV6", Packet);
		if (!bDoSend)
			return Status;
	}

	// Validate the session and make sure it doesn't go away until transmit is done


	if (!PPPADDREF(s_p, REF_SENDIPVX))
	{
		Status = NDIS_STATUS_INVALID_PACKET;
	}
	else
    {
		// Protect the PPP Send
		pppLock( s_p );

		switch( s_p->Mode )
		{
		case PPPMODE_PPP:
			// Route to IP Data Transmit
			pppNcp_SendIPvX( s_p->ncpCntxt, Packet, ipProto == 4 ? NCP_IPCP : NCP_IPV6CP );
			break;

		case PPPMODE_SLIP:
			pppSendNdisBufferChainViaSLIP( s_p, Packet->Private.Head );
			break;

		case PPPMODE_CSLIP:
			// TBD - VJ Header compression
			// TBD - Send the packet
			break;

		default:
			ASSERT( 0 );
		}

		pppUnLock( s_p );

		// Transmit complete, it's ok if the session gets deleted now

		PPPDELREF(s_p, REF_SENDIPVX);
	}

	return Status;
}


//  Transmit
//
//  Function: Callout function for IP to tx data
//

NDIS_STATUS
Transmit( void              *Context,
          PNDIS_PACKET      Packet,
          IPAddr            Destination,
          RouteCacheEntry   *RCE )
{
    DEBUGMSG(ZONE_PPP, (TEXT( "PPP Transmit(%08X, %08X, %08X, %08X)\r\n" ),
          Context, Packet, Destination, RCE ));

#ifdef DEBUG
	if (dpCurSettings.ulZoneMask & 0xF0000000) {
		char		 TempPacket[2048];
		PNDIS_BUFFER pBuffer;
		DWORD		 Length = 0;
		PVOID		 VirtualAddress;
		UINT		 BufferLength;

		NdisQueryPacket(Packet, NULL, NULL, &pBuffer, NULL);
		while (pBuffer)
		{
			NdisQueryBuffer(pBuffer, &VirtualAddress, &BufferLength);
			if ((Length + BufferLength) > sizeof(TempPacket)) {
				NKDbgPrintfW (TEXT("PPP:Transmit Packet too large, truncating at %d\r\n"), Length);
				break;
			}
			memcpy (TempPacket+Length, VirtualAddress, BufferLength);
			Length += BufferLength;
			NdisGetNextBuffer(pBuffer, &pBuffer);
		}

		// Check top bit of zone mask.
		// Turn this on via zo m NNN 0x80000000
		if (dpCurSettings.ulZoneMask & 0x80000000) {
			// Try to decode the first chunk
			DumpIP (TRUE, TempPacket, Length);
		}
		if (dpCurSettings.ulZoneMask & 0x40000000) {
			NKDbgPrintfW (TEXT("PPP:Transmit len=%d\r\n"), Length);
			DumpMem(TempPacket, Length);            // Dump packet contents
		}
	}
#endif

	return PppSendIPvX(Context, Packet, 4);
}

//  Receive
//
//  Function: Send data up to IP
//

void
Receive(
	void		*session,
	pppMsg_t	*pMsg)
{
	HANDLE              threadId;
	PPP_CONTEXT         *pCurContext = (PPP_CONTEXT *)session;

    DEBUGMSG( ZONE_PPP|ZONE_LOCK,
              (TEXT("IPRcv LockWaitCnt=%d Locked=%d LockedCount=%d\r\n"),
              pCurContext->Session->LockWaitCnt,
              pCurContext->Session->Locked,
              pCurContext->Session->SesCritSec.LockCount));

	//
	// Invoke packet debug extension if it is loaded.
	//
	if (g_pfnLogRxContigPacket)
	{
		BOOL bDoReceive;

		bDoReceive = g_pfnLogRxContigPacket(L"PPP", pCurContext->AdapterName, L"IP", pMsg->data, pMsg->len);
		if (!bDoReceive)
			return;
	}

    // Print if lock count exceeds 1 as the receive
    // path should only have the lock once.

    if( pCurContext->Session->SesCritSec.LockCount > 1 )
    {
        DEBUGMSG( ZONE_ERROR, ( TEXT("IPRcv LockCount=%d\r\n"),
        pCurContext->Session->SesCritSec.LockCount) );
    }

    threadId = (HANDLE )GetCurrentThreadId();

    DEBUGMSG( ZONE_LOCK, (TEXT( "iprcv:Current    thread Id 0x%x\n" ), threadId ));
    DEBUGMSG( ZONE_LOCK, (TEXT( "iprcv:Lock Owner thread Id 0x%x\n" ),
        pCurContext->Session->SesCritSec.OwnerThread ));

    ASSERT( pCurContext->Session->SesCritSec.OwnerThread == threadId );

	// Intercept any DHCP ACK packet containing domain name
	if (pCurContext->Session->bDhcpTimerRunning)
	{
		if (PppDhcpRxPacket(pCurContext->Session, pMsg->data, pMsg->len))
		{
			return;
		}
	}

    pppUnLock( pCurContext->Session );

#ifdef DEBUG
	// Check top bit of zone mask.
	// Turn this on via zo m NNN 0x80000000
	if (dpCurSettings.ulZoneMask & 0x80000000) {
		// Try to decode the first chunk
		DumpIP (FALSE, pMsg->data, pMsg->len);
	}
	if (dpCurSettings.ulZoneMask & 0x40000000) {
		NKDbgPrintfW (TEXT("PPP: Recieve Len=%d\r\n"), pMsg->len);
		DumpMem(pMsg->data, pMsg->len);            // Dump packet contents
	}
#endif

	//
	//	IPCP may be OPEN but the interface not registered if we
	//	RequireDataEncryption is set and CCP is not OPEN yet.  In
	//	this case IPContext will be NULL until CCP enters the OPEN
	//	state and registers the interface with IP.  Until the
	//	interface is registered IP packets are discarded.
	//
	if (pCurContext->IPContext != NULL)
	{
		IPRcv( pCurContext->IPContext,      // IP's Context
			   pMsg->data,                  // The Packet
			   pMsg->len,                   // Data Size
			   pMsg->len,                   // Total Size
			   0,                           // Context1
			   0,                           // Context2
			   FALSE,                       // fBroadcast
			   NULL);                       // LINK
	}

    pppLock( pCurContext->Session );
}

void
ReceiveComplete(
	void		*session)
{
	if (IPRcvComplete != NULL)
		IPRcvComplete();
}




//  XferData
//
//  Function: Copy
//

NDIS_STATUS

XferData(   void            *Context,
            NDIS_HANDLE     MACContext,
            uint            MyOffset,
            uint            ByteOffset,
            uint            BytesWanted,
            PNDIS_PACKET    Packet,
            uint            *Transferred )
{
    DEBUGMSG (ZONE_PPP, (
    TEXT( "PPP XferData(%08X, %08X, %d, %d, %d, %08X, %08X)\r\n"),
           Context, MACContext, MyOffset, ByteOffset,
           BytesWanted, Packet, Transferred));

    ASSERT( 0 );

    return( NDIS_STATUS_SUCCESS );
}

void

Close( void *Context )
{
    PPP_CONTEXT     *pCurContext = (PPP_CONTEXT *)Context;

    DEBUGMSG (ZONE_PPP, (TEXT("PPP Close(%08X)\r\n"), Context));

    pCurContext->fOpen = FALSE;

    return;
}

void

Invalidate( void *Context, RouteCacheEntry *RCE )
{
    DEBUGMSG (ZONE_PPP, (TEXT("PPP Invalidate(%08X,%08X)\r\n"), Context, RCE));

    // No action

    return;
}

uint

AddAddr( void *Context, uint Type, IPAddr Address, IPMask Mask, PVOID pControlBlock )
{
    DEBUGMSG( ZONE_PPP, (
    TEXT( "PPP AddAddr(%08X,%d,%08X, %08X)\r\n"), Context,Type,Address,Mask));

    return( TRUE );
}

uint

DeleteAddr( void *Context, uint Type, IPAddr Address, IPMask Mask )
{
    DEBUGMSG (ZONE_PPP, (TEXT("PPP DeleteAddr(%08X,%d,%08X, %08X)\r\n"),
                  Context, Type, Address, Mask));

    return( TRUE );
}

void

Open( void *Context )
{
    DEBUGMSG (ZONE_PPP, (TEXT("PPP Open(%08X)\r\n"), Context));

    // What would we do here?

    return;
}

int
QueryInfo
    (   void        *IFContext,
        TDIObjectID *ID,
        PNDIS_BUFFER Buffer,
        uint        *Size,
        void        *Context )
{
    PPP_CONTEXT     *pCurContext = (PPP_CONTEXT *)IFContext;
	pppSession_t    *s_p = pCurContext->Session;
	PLCPContext		lcp_p  = (PLCPContext)s_p->lcpCntxt;
    uint            Offset       = 0;
    uint            BufferSize   = *Size;
    uchar           InfoBuff[ sizeof( IFEntry ) ];
    uint            Entity;
    uint            Instance;

    DEBUGMSG (ZONE_PPP, (
    TEXT( "PPP QueryInfo( %08X, %08X, %08X, %08X, %08X )\r\n"),
           IFContext, ID, Buffer, Size ) );

    Entity = ID->toi_entity.tei_entity;
    Instance = ID->toi_entity.tei_instance;

    // We support only Interface MIBs - no address xlation -
    // pretty much like a loopback i/f (per Henry circa 1994).

    if( (Entity != IF_ENTITY) || (Instance != pCurContext->ifinst) )
    {
        return( TDI_INVALID_REQUEST );
    }

    if( ID->toi_type != INFO_TYPE_PROVIDER )
    {
        return( TDI_INVALID_PARAMETER );
    }

    *Size = 0 ;                                     // a safe initialization.

    if( ID->toi_class == INFO_CLASS_GENERIC )
    {
        if( ID->toi_id == ENTITY_TYPE_ID )
        {
            // Identify what type we are.

            if( BufferSize >= sizeof( uint ) )
            {
                *(uint *)&InfoBuff[ 0 ] =
                (Entity == AT_ENTITY) ? AT_ARP : IF_MIB;

                (void )CopyFlatToNdis(Buffer, InfoBuff, sizeof(uint), &Offset);
                *Size = sizeof(uint);
                return( TDI_SUCCESS );
            }
            else
            {
                return( TDI_BUFFER_TOO_SMALL );
            }
        }

        // That's the only generic request supported.

        return( TDI_INVALID_PARAMETER );
    }

    if( ID->toi_class != INFO_CLASS_PROTOCOL )
    {
        return( TDI_INVALID_PARAMETER );
    }

    // Asking for interface level information.
    // Do we support what he's asking for.

    if( ID->toi_id == IF_MIB_STATS_ID )
    {
        // We should probably fill get some statistics for it.

        IFEntry  *IFE = (IFEntry *)InfoBuff;

        if( BufferSize < IFE_FIXED_SIZE )
        {
            return( TDI_BUFFER_TOO_SMALL );
        }

        // Buffer can hold the fixeded part. Build
        // the IFEntry structure, and copy it in.

        IFE->if_index       = pCurContext->index;
        IFE->if_type        = IF_TYPE_PPP;
        IFE->if_physaddrlen = ARP_802_ADDR_LENGTH;
		switch( s_p->Mode )
		{
		case PPPMODE_PPP:
			// Use the negotiated peer MRU
			IFE->if_mtu         = lcp_p->peer.MRU;
			break;

		case PPPMODE_SLIP:
		case PPPMODE_CSLIP:
			IFE->if_mtu         = SLIP_DEFAULT_MTU;
			break;

		default: ASSERT( 0 );
		}
		DEBUGMSG (ZONE_PPP, ( TEXT( "PPP QueryInfo +pppMac_GetCallSpeed\n")));
		pppMac_GetCallSpeed(s_p->macCntxt, &IFE->if_speed);
		DEBUGMSG (ZONE_PPP, ( TEXT( "PPP QueryInfo -pppMac_GetCallSpeed\n")));

        CTEMemSet( IFE->if_physaddr, 0, ARP_802_ADDR_LENGTH );

		//
		//	Since PPP dynamically registers/deregisters adapters,
		//	it can only be queried by TCP/IP when it is "UP".
		//
		IFE->if_adminstatus = IF_STATUS_UP;
		IFE->if_operstatus  = IF_STATUS_UP;

		
        IFE->if_lastchange = 0;

        // Get the MAC layers stats

        // Rx Statistics

        IFE->if_inoctets        = s_p->Stats.BytesRcvd;
        IFE->if_inucastpkts     = s_p->Stats.FramesRcvd;
        IFE->if_innucastpkts    = 0;
        IFE->if_indiscards      = s_p->Stats.CRCErrors;
        IFE->if_inerrors        = s_p->Stats.TimeoutErrors +
                                  s_p->Stats.AlignmentErrors +
                                  s_p->Stats.SerialOverrunErrors +
                                  s_p->Stats.FramingErrors +
                                  s_p->Stats.BufferOverrunErrors;
        IFE->if_inunknownprotos = 0;

        // Tx Statistics

        IFE->if_outoctets       = s_p->Stats.BytesSent;
        IFE->if_outucastpkts    = s_p->Stats.FramesRcvd;
        IFE->if_outnucastpkts   = 0;
        IFE->if_outdiscards     = 0;
        IFE->if_outerrors       = 0;
        IFE->if_outqlen         = 0;    // no queue

        IFE->if_descrlen        = 0;

        Buffer = CopyFlatToNdis( Buffer, (uchar *)IFE, IFE_FIXED_SIZE, &Offset);

        // We should copy a description over....

        *Size = IFE_FIXED_SIZE;
		DEBUGMSG (ZONE_PPP, ( TEXT( "PPP -QueryInfo SUCCESS\n")));
        return TDI_SUCCESS;
    }

    return( TDI_INVALID_PARAMETER );
}

int

SetInfo( void *Context, TDIObjectID *ID, void *Buffer, uint Size )
{
    DEBUGMSG (ZONE_PPP, (TEXT("PPP SetInfo(%08X, %08X, %08X, %d)\r\n"),
                  Context, ID, Buffer, Size));

    return( TRUE );
}

int

GetEList(void *Context, TDIEntityID *EntityList, uint *Count)
{
    PPP_CONTEXT     *pCurContext = (PPP_CONTEXT *)Context;
    uint            ECount;
    uint            MyIFBase;
    uint            i;
    TDIEntityID		*pEntity = NULL;
    int				Status = TRUE;

#ifndef MAX
#define MAX(x,y)    ((x) > (y) ? (x) : (y))
#endif

    DEBUGMSG (ZONE_PPP, (TEXT("PPP GetEList(%08X,%08X,%08X)\r\n"),
                  Context, EntityList, Count));

    ECount = *Count;

    // Walk down the list, looking for existing AT or IF entities, and
    // adjust our base instance accordingly.

    MyIFBase = 0;
    for (i = 0; i < ECount; i++, EntityList++)
    {
        if (EntityList->tei_entity == IF_ENTITY)
        {
	        if ( EntityList->tei_instance == pCurContext->ifinst &&
	             EntityList->tei_instance != INVALID_ENTITY_INSTANCE ) {
	            pEntity    = EntityList;
	            break;
	        } else {
	            MyIFBase = MAX(MyIFBase, EntityList->tei_instance + 1);
	        }
        }
    }

	if (pEntity) {

		if (! pCurContext->fOpen)
			pEntity->tei_instance = (DWORD)INVALID_ENTITY_INSTANCE;

	} else {

		if (pCurContext->fOpen) {
		    // EntityList points to the start of where we want to begin filling in.
		    // Make sure we have enough room. We need one for the ICMP instance,
		    // and one for the CL_NL instance.

		    if ((ECount + 1) > MAX_TDI_ENTITIES)
		    {
		        Status = FALSE;

		    } else {

			    // At this point we've figure out our base instance. Save for later use.

			    pCurContext->ifinst = MyIFBase;

			    // Now fill it in.
			    EntityList->tei_entity = IF_ENTITY;
			    EntityList->tei_instance = MyIFBase;
			    *Count += 1;
			}
		}
	}

    return( Status );
}

LONG
RegSetIPAddrMultiSzValue(
	HKEY	 hKey,
	PTCHAR	 tszValueName,
	...)
//
//	Set a registry value which is a MULTI_SZ list of IP addresses.
//
//	The variable parameters to this function are a NULL terminated list of
//	pdwIPAddrs to convert to strings to build the multi_sz.
//
//	NOTE: Does not set addresses with a value of 0.  If not addresses are
//	      set, the registry value is deleted.
//
//	@alert: if memory allocation fails, registry value is not deleted
//
{
	TCHAR	*ptszMultiIpAddr = NULL, *tszCur = NULL;
	DWORD	dwIPAddr, *pdwIPAddr;
	va_list ArgList;
	LONG	lResult;
	HLOCAL hMem = NULL;

	#ifndef BLOCK_LENGTH
	#undef	BLOCK_LENGTH
	#endif /* BLOCK_LENGTH */
	#define	BLOCK_LENGTH	(0x100)

	INT nContentLength = 0;
	INT nAllocatedLength = BLOCK_LENGTH;

	va_start (ArgList, tszValueName);

	hMem = LocalAlloc(LMEM_ZEROINIT, nAllocatedLength * sizeof(TCHAR));			// ref: +alloc

	if (!hMem)
	{
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	ptszMultiIpAddr = LocalLock(hMem);											// ref: +lock
	tszCur = ptszMultiIpAddr;

	// Build the multi_sz
	while (pdwIPAddr = va_arg(ArgList, PDWORD))
	{
		dwIPAddr = *pdwIPAddr;

		if (dwIPAddr)
		{
			/* since the function accepts variable list of pdwIPAddrs, it's
			necessary to reallocate the memory holding the string. BLOCK_LENGTH
			is basically (max IP adddress length (string) + safe length) and it
			can be fine tuned to the exact specifications */

			if ((nContentLength > 0) && ((nAllocatedLength - nContentLength) > BLOCK_LENGTH))
			{
				HLOCAL hTmp = NULL;

				LocalUnlock(hMem);												// ref: -lock

				hTmp = LocalReAlloc(hMem, (nAllocatedLength + BLOCK_LENGTH) *
					sizeof(TCHAR), (LMEM_MODIFY | LMEM_MOVEABLE));				// ref: *alloc

				if (!hTmp)
				{
					LocalFree(hMem);											// ref: -alloc
					return ERROR_NOT_ENOUGH_MEMORY;
				}
				else
				{
					nAllocatedLength += BLOCK_LENGTH;
				}

				hMem = hTmp;
				ptszMultiIpAddr = LocalLock(hMem);								// ref: +lock
				tszCur = (ptszMultiIpAddr + nContentLength);	// set tszCur to current location
			}

			tszCur += _stprintf (tszCur, TEXT("%d.%d.%d.%d\0"),
							(dwIPAddr >> 24) & 0xFF, (dwIPAddr >> 16) & 0xFF,
							(dwIPAddr >>  8) & 0xFF,  dwIPAddr        & 0xFF) + 1;

			nContentLength = (tszCur - ptszMultiIpAddr);
		}
	}
	va_end(ArgList);

	if (tszCur > ptszMultiIpAddr)
	{
		// We have 1 or more nonzero IP addresses
		//
		// Add the extra terminator to denote end of the multi_sz
		*tszCur++ = TEXT('\0');

		DEBUGMSG (ZONE_PPP, (TEXT("PPP: Setting multi_sz list for %s - 1=%s\n"), tszValueName, ptszMultiIpAddr));

		lResult = RegSetValueEx (hKey, tszValueName, 0, REG_MULTI_SZ, (char *)ptszMultiIpAddr, (tszCur - ptszMultiIpAddr) * sizeof(TCHAR));
	}
	else
	{
		DEBUGMSG (ZONE_PPP, (TEXT("PPP: Deleting registry value %s - all IP addresses for it are 0\n"), tszValueName));

		lResult = RegDeleteValue(hKey, tszValueName);
	}

	if (hMem)
	{
		LocalUnlock(hMem);														// ref: -lock
		LocalFree(hMem);														// ref: -alloc
	}

	#ifndef BLOCK_LENGTH
	#undef	BLOCK_LENGTH
	#endif /* BLOCK_LENGTH */

	return lResult;
}

const WCHAR TcpipParametersKey[] =  L"Comm\\Tcpip\\Parms";


BOOL
PPPSetAdapterIPRegistrySettings(
	IN		PPP_CONTEXT    *pContext,
	IN	OUT	PNDIS_STRING	pConfigName,
		OUT	PDWORD		    pdwDefaultGateway)
//
//	Set the adapter's IP registry values based upon the IPCP negotiated values.
//
	/*
		[HKEY_LOCAL_MACHINE\Comm\<adapter name/#>\Parms\TcpIp]
	   "EnableDHCP"       = dword:0
	   "DefaultGateway"   = multi_sz:"<IPCP peer ip addr>"
	   "UseZeroBroadcast" = dword:0
	   "IpAddress"        = multi_sz:"<IPCP negotiated ip address>"
	   "Subnetmask"       = multi_sz:"<derived from IP address>"
	   "DNS"              = multi_sz:"<IPCP assigned primary DNS server address>","<DNS secondary>"
	   "WINS"             = multi_sz:"<IPCP assigned primary WINS>","<WINS secondary>"
	*/
{
	pppSession_t    *s_p    = pContext->Session;
	ncpCntxt_t      *ncp_p  = s_p->ncpCntxt;
	ipcpCntxt_t     *ipcp_p = (ipcpCntxt_t *)ncp_p->protocol[ 1 ].context;
	HKEY			hKey;
	LONG			hRes;
	DWORD			zero = 0;
	DWORD			dwSubNetMask, dwIPAddr, dwDefaultGatewayIPAddr,
		            dwAddDefaultGateway, dwDontAddDefaultGateway;
    WCHAR Buffer[MAX_PATH];

    if (s_p->szDomain[0] != 0)
    {
        memset(Buffer, 0, sizeof(Buffer));
        MultiByteToWideChar(CP_OEMCP, 0, &s_p->szDomain[0], -1, Buffer, MAX_PATH);
        //
        // Set system-wide DNSDomain name
        //
        hRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TcpipParametersKey, 0, 0, &hKey);
        if (ERROR_SUCCESS == hRes)
        {
            RegSetValueEx (hKey, TEXT("DNSDomain"), 0, REG_SZ, (PBYTE)&Buffer[0], (wcslen(Buffer) + 1) * sizeof(WCHAR));
            RegCloseKey(hKey);
        }
    }
    
    
    //
	// Set the per adapter TcpIp parameters.
	//
	_stprintf(pConfigName->Buffer, TEXT("Comm\\%s\\Parms\\TcpIp"), &pContext->AdapterName[0]);

	pConfigName->Length = wcslen(pConfigName->Buffer) * sizeof(WCHAR);
	ASSERT(pConfigName->Length < pConfigName->MaximumLength);
	hRes = RegCreateKeyEx (HKEY_LOCAL_MACHINE, pConfigName->Buffer, 0, NULL,
								   REG_OPTION_NON_VOLATILE, 0, NULL,
								   &hKey, NULL);
	if (hRes != ERROR_SUCCESS)
	{
		DEBUGMSG (ZONE_ERROR, (TEXT("PPP: Unable to create reg key '%s'\n"), pConfigName->Buffer));
		return FALSE;
	}

	//
	// Set the TCP/IP domain for this adapter if we obtained a domain
	// via DHCP over PPP.
	//
	if (s_p->szDomain[0] != 0)
	{
        RegSetValueEx (hKey, TEXT("Domain"), 0, REG_SZ, (PBYTE)&Buffer[0], (wcslen(Buffer) + 1) * sizeof(WCHAR));
	} else {
        RegDeleteValue (hKey, TEXT("Domain"));
    }

	// Don't run DHCP over the PPP link, the IP address is set by the IPCP negotiation
	RegSetValueEx (hKey, TEXT("EnableDHCP"), 0, REG_DWORD, (char *)&(zero), sizeof(DWORD));
	RegSetValueEx (hKey, TEXT("UseZeroBroadcast"), 0, REG_DWORD, (char *)&(zero), sizeof(DWORD));

	//
	//	Only want to set up default routes (gateways) if we are a RAS client
	//  and the RASENTRY option is set.
	//
	dwAddDefaultGateway = !s_p->bIsServer && (s_p->dwAlwaysAddDefaultRoute || (s_p->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway));
	dwDontAddDefaultGateway =  !dwAddDefaultGateway;
	RegSetValueEx (hKey, TEXT("DontAddDefaultGateway"), 0, REG_DWORD, (char *)&dwDontAddDefaultGateway, sizeof(DWORD));

    switch( s_p->Mode )
    {
    case PPPMODE_PPP:
		dwIPAddr = ipcp_p->local.ipAddress;
		dwDefaultGatewayIPAddr = ipcp_p->peer.ipAddress;

		//
		// If the server did not tell us its IP address, generate
		// a default gateway address based upon our address.
		//
		// If the server told us that its address is the same as
		// our address, that's strange and really confuses our routing
		// tables, so replace it with a fake address instead.
		//
		if (dwDefaultGatewayIPAddr == 0 || dwDefaultGatewayIPAddr == dwIPAddr)
		{
			dwDefaultGatewayIPAddr = dwIPAddr;
			DEBUGMSG(ZONE_WARN, (TEXT("PPP: Server did not tell us its IP address, using own IP as default gateway\n")));
		}
		break;

	case PPPMODE_SLIP:
    case PPPMODE_CSLIP:
		dwIPAddr = *(PDWORD)&s_p->rasEntry.ipaddr;

		//
		// We need to have a default gateway assigned so that IP will add a default route to the route
		// table.  The IP address of the default gateway doesn't matter so long as it is on the same
		// subnet as the SLIP adapter's IP address.  Since SLIP has no mechanism to tell us what our
		// SLIP peer's IP address is, we just fudge an address on the same subnet.  The gateway's IP
		// address will get passed into our send handler by IP when the default route kicks in, but
		// PPP just ignores this address as PPP only has one place to send IP packets to.  On a LAN
		// adapter, the gateway's MAC address would get put into the LAN packet.
		//
		dwDefaultGatewayIPAddr = dwIPAddr;
		break;
	}

	if (s_p->bIsServer)
	{
		dwSubNetMask = PPPServerGetSessionIPMask(s_p);
	}
	else
	{
		// Just derive the subnet from the IP address (class A, B, or C)
		dwSubNetMask = IPGetNetMask( dwIPAddr );
	}

	//
	// If we are adding a default route, make sure that the peer's IP address is
	// within the same subnet as our IP address. Otherwise, with the peer IP outside
	// the subnet, the routing table will be configured such that PPP interface may
	// not be used to send packets matching the PPP default route!
	//
	if ((dwIPAddr & dwSubNetMask) != (dwDefaultGatewayIPAddr & dwSubNetMask))
	{
		DEBUGMSG(ZONE_WARN, (L"PPP: Peer IP addr %x outside our subnet %x/%x, using own IP as gateway\n", dwDefaultGatewayIPAddr, dwIPAddr, dwSubNetMask));
		dwDefaultGatewayIPAddr = dwIPAddr;
	}

	if ((s_p->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
	&&  !(s_p->dwAlwaysAddSubnetRoute))
	{
		//
		// We are adding a default route for this PPP interface, and don't
		// want a subnet route as well.
		//
		// So, we set the subnet mask so that there is no subnet route added to the route table.
		//
		dwSubNetMask = 0xFFFFFFFF;
	}


	RegSetIPAddrMultiSzValue(hKey, TEXT("IpAddress"), &dwIPAddr, NULL);
	RegSetIPAddrMultiSzValue(hKey, TEXT("Subnetmask"), &dwSubNetMask, NULL);
	RegSetIPAddrMultiSzValue(hKey, TEXT("DefaultGateway"), &dwDefaultGatewayIPAddr, NULL);
	RegSetIPAddrMultiSzValue(hKey, TEXT("DNS"), &s_p->rasEntry.ipaddrDns, &s_p->rasEntry.ipaddrDnsAlt, NULL);
	RegSetIPAddrMultiSzValue(hKey, TEXT("WINS"), &s_p->rasEntry.ipaddrWins, &s_p->rasEntry.ipaddrWinsAlt, NULL);

	RegCloseKey (hKey);

	*pdwDefaultGateway = dwDefaultGatewayIPAddr;

	return TRUE;
}

int
__stdcall
PPPDynRegister(
	PWSTR    Adapter,
	void    *IPContext,
	IPRcvRtn RcvRtn,
    IPTxCmpltRtn TxCmpltRtn,
	IPStatusRtn StatusRtn,
	IPTDCmpltRtn TDCmpltRtn,
    IPRcvCmpltRtn RcvCmpltRtn,
	IPSetNTEAddrRtn IPSetNTEAddrRtn,
    struct LLIPBindInfo *Info,
	uint NumIFBound)
//
//	This function is called back by IP when PPPBindNewAdapter calls IPAddInterface.
//
{
	PPP_CONTEXT    *pContext = (PPP_CONTEXT *)Info->lip_context;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+PPPDynRegister: %s\n"), Adapter));

    pContext->IPContext = IPContext;
    pContext->index = NumIFBound;

    // Save IP's functions.

    IPRcv           = RcvRtn;
    IPSendComplete  = TxCmpltRtn;
    IPStatus        = StatusRtn;
    IPTDComplete    = TDCmpltRtn;
    IPRcvComplete   = RcvCmpltRtn;
    IPSetNTEAddr    = IPSetNTEAddrRtn;

    // TCPTRACE(("Arp Interface %lx ai_context %lx ai_index %lx\n",Interface, Interface->ai_context, Interface->ai_index));
    return TRUE;
}

HINSTANCE              g_IpHlpApiMod;

typedef DWORD (*pfnGetIpForwardTable) (PMIB_IPFORWARDTABLE pIpForwardTable,PULONG pdwSize,BOOL bOrder);
pfnGetIpForwardTable g_pfnGetIpForwardTable;
typedef DWORD (*pfnSetIpForwardEntry) (PMIB_IPFORWARDROW pRoute);
pfnSetIpForwardEntry g_pfnSetIpForwardEntry;

void
PPPChangeDefaultRoutesMetric(
	IN		int				 metricDelta)
//
//	Modify the Metric1 for the default routes in the net table by the specified delta.
//
{
	int					newMetric;
	PMIB_IPFORWARDTABLE	pTable;
	DWORD				dwTableSize;
	PMIB_IPFORWARDROW	pRow;
	DWORD				numRoutes,
						dwResult;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +ChangeDefaultRoutesMetric delta=%d\n"), metricDelta));

    if (!g_IpHlpApiMod)
    {
		dwResult = CXUtilGetProcAddresses(TEXT("iphlpapi.dll"), &g_IpHlpApiMod,
						TEXT("GetIpForwardTable"), &g_pfnGetIpForwardTable,
						TEXT("SetIpForwardEntry"), &g_pfnSetIpForwardEntry,
						NULL);
		if (dwResult != NO_ERROR)
		{
            RETAILMSG(1,(TEXT("!PPPChangeDefaultRoutesMetric could not load IPHlpAPI fcn pointers, RASDefaultGateway option disabled\r\n")));
            return;
        }
    }

	dwTableSize = 0;
	g_pfnGetIpForwardTable(NULL, &dwTableSize, FALSE);

	pTable = pppAllocateMemory(dwTableSize);
	if (pTable == NULL)
	{
		DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR, unable to allocate %d bytes for route table \n"), dwTableSize));
	}
	else
	{
		dwResult = g_pfnGetIpForwardTable(pTable, &dwTableSize, FALSE);

		if (dwResult != ERROR_SUCCESS)
		{
			DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR %d from GetIpForwardTable\n"), dwResult));
		}
		else
		{
			for (pRow = &pTable->table[0], numRoutes = pTable->dwNumEntries;
				 numRoutes--;
				 pRow++)
			{
				// Check to see if the route is a default route
				if (pRow->dwForwardDest == 0)
				{
					newMetric = pRow->dwForwardMetric1 + metricDelta;

					// Don't try to reduce a metric below 1.

					if (newMetric > 0)
					{
						pRow->dwForwardMetric1 = (ULONG)newMetric;

						

						if (pRow->dwForwardType == 4 /*IRE_TYPE_INDIRECT*/)
							pRow->dwForwardType =  3 /*IRE_TYPE_DIRECT*/;

						dwResult = g_pfnSetIpForwardEntry(pRow);
						if (dwResult != NO_ERROR)
						{
							DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR %d from SetIPForwardEntry\n"), dwResult));
						}
					}
				}
			}
		}

		pppFreeMemory(pTable, dwTableSize);
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -ChangeDefaultRoutesMetric\n")));
}

#define IPADDROUT(a) (a)>>24, ((a)>>16)&0xFF, ((a)>>8)&0xFF, (a)&0xFF

void
PPPMakeDefaultRoute(
	DWORD	IPAddrGateway,
	DWORD	IfIndex)
//
//	Set the default route for the interface to have a metric of 1,
//	so it becomes the cheapest default route.
//
{
	MIB_IPFORWARDROW	row;
	DWORD				dwResult;

	if (g_pfnSetIpForwardEntry)
	{
		memset(&row, 0, sizeof(row));
		// row.dwForwardDest = 0.0.0.0
		// row.dwForwardMask = 0.0.0.0
		// row.dwForwardPolicy = 0;
		row.dwForwardNextHop    = htonl(IPAddrGateway);
		row.dwForwardIfIndex    = IfIndex;
		row.dwForwardType       = 3 /*IRE_TYPE_DIRECT*/;
		row.dwForwardProto      = PROTO_IP_NETMGMT;
		// row.dwForwardAge     = 0;
		// row.dwForwardNextHopAS  = 0;
		row.dwForwardMetric1    = 1;

		DEBUGMSG(1, (TEXT("PPP: MakeDefaultRoute to %u.%u.%u.%u for IF %u\n"), IPADDROUT(IPAddrGateway), IfIndex));
		dwResult = g_pfnSetIpForwardEntry(&row);
		if (dwResult != NO_ERROR)
		{
			DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR %d from SetIPForwardEntry\n"), dwResult));
		}
	}
}


BOOL
PPPAddInterface(
	PPP_CONTEXT			*pContext)
//
//	This function is called by the ppp protocol driver to notify
//	IP that a new wan adapter instance has been created.
//
{
	struct LLOldIPBindInfo  Info;
	IP_STATUS			 Status;
	NDIS_STRING			 ndsAdapterName,
						 ndsConfigName;
	TCHAR				 tszKeyName[128];
	BOOL				 bResult;
	DWORD				 dwDefaultGateway;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+PPPAddInterface: %s\n"), &pContext->AdapterName[0]));

	// Setup registry info, including NoDhcp, IP address, subnet, DNS, WINS addresses,
	// before telling IP about the new adapter.
	ndsConfigName.Length = 0;
	ndsConfigName.MaximumLength = sizeof(tszKeyName);
	ndsConfigName.Buffer = &tszKeyName[0];
	PPPSetAdapterIPRegistrySettings(pContext, &ndsConfigName, &dwDefaultGateway);

	memset(&Info, 0, sizeof(Info));

    Info.lip_context   = pContext;
    pppMac_GetCallSpeed(pContext->Session->macCntxt, &Info.lip_speed);
	DEBUGMSG(ZONE_PPP, (TEXT("PPP: Interface Bit Rate is %u b/s\n"), Info.lip_speed));
    Info.lip_transmit  = Transmit;
    Info.lip_transfer  = XferData;
    Info.lip_close     = Close;
    Info.lip_invalidate = Invalidate;
    Info.lip_addaddr   = AddAddr;
    Info.lip_deladdr   = DeleteAddr;
    Info.lip_open      = Open;
    Info.lip_qinfo     = QueryInfo;
    Info.lip_setinfo   = SetInfo;
    Info.lip_getelist  = GetEList;
    Info.lip_flags     = LIP_P2P_FLAG | LIP_COPY_FLAG ;

	if (pContext->Session->Mode == PPPMODE_PPP)
	{
		// Set the IP MaxSegmentSize (MSS) to the maximum frame size that the PPP peer can
		// receive.  This usually is 1500, but the adapter (e.g. AsyncMac) may restrict
		// the max send size and/or it may be changed during LCP negotiation by the peer sending
		// a Maximum-Receive-Unit option.

		Info.lip_mss       = ((PLCPContext)(pContext->Session->lcpCntxt))->peer.MRU;
	}
	else // SLIP
	{
		Info.lip_mss = SLIP_DEFAULT_MTU;
	}

    Info.lip_addrlen   = ARP_802_ADDR_LENGTH ;
    Info.lip_addr      = pContext->addr;    // This never seems to be used.

	Info.lip_arpresolveip = NULL;

	//
	//	If the RAS option to make the new PPP connection be the default gateway has been
	//	set, then increment the cost (metric) of all the current default routes by 1 so
	//	that the new PPP connection will be chosen.
	//

	if (pContext->Session->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
	{
		PPPChangeDefaultRoutesMetric(1);
	}

	RtlInitUnicodeString(&ndsAdapterName, &pContext->AdapterName[0]);
    Status = g_pfnIPAddInterface(&ndsAdapterName, &ndsConfigName, NULL, pContext, PPPDynRegister, &Info);
    if (Status != IP_SUCCESS)
	{
		if (pContext->Session->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
		{
			PPPChangeDefaultRoutesMetric(-1);
		}
		bResult = FALSE;
	}
	else
	{
		if (pContext->Session->bIsServer)
		{
			PPPServerAddRouteToClient(pContext->Session);
		}
		else if (pContext->Session->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
		{
			// To make the new interface be the default route, set the metric for it to 1
			PPPMakeDefaultRoute(dwDefaultGateway, pContext->index);
		}
		bResult = TRUE;
	}

	DEBUGMSG(ZONE_FUNCTION || (ZONE_ERROR && bResult == FALSE),
		(TEXT("-PPPAddInterface: %s, result=%d\n"), &pContext->AdapterName[0], bResult));

	return bResult;
}

void
PPPDeleteInterface(
	PPP_CONTEXT			*pContext,
	pppSession_t        *pSession)
//
//	This function is called by the ppp protocol driver to notify
//	IP that a WAN link is no longer available.
//
{
	PVOID	         pIPContext;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+PPPDeleteInterface: %s\n"), &pContext->AdapterName[0]));

	pIPContext = (PVOID)InterlockedExchange((PULONG)&pContext->IPContext, (LONG)NULL);

	if (pIPContext)
	{
		if (pSession->bIsServer)
		{
			PPPServerDeleteRouteToClient(pSession);
		}
		//
		//	If we incremented the default routes when we added this interface, undo it.
		//
		if (pSession->rasEntry.dwfOptions & RASEO_RemoteDefaultGateway)
		{
			PPPChangeDefaultRoutesMetric(-1);
		}

		g_pfnIPDelInterface(pIPContext);
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("-PPPDeleteInterface: %s\n"), &pContext->AdapterName[0]));
}

void
SetPPPPeerIPAddress(
	PPP_CONTEXT *pppContext)
//
//	Set the IP address that the name "ppp_peer" will resolve to.
//	Note that this name is only present for internal use.
//
//	If pppContext is NULL, then the ppp_peer name is set to
//	no longer be resolvable to an IP address.
//
{
	char			**pppAddr = NULL;
	DWORD			pAddr[2];
	char			*ppAddr[2];

	if (pppContext != NULL)
	{
		pppSession_t    *s_p    = (pppSession_t *)pppContext->Session;
		ncpCntxt_t      *ncp_p  = (ncpCntxt_t  *)s_p->ncpCntxt;
		ipcpCntxt_t     *ipcp_p = (ipcpCntxt_t *)ncp_p->protocol[ NCP_IPCP ].context;

		pppAddr = &ppAddr[0];

        // Set peer's IP address into hosts file

		ppAddr[0] = (char *)&pAddr[0];
		ppAddr[1] = NULL;
		pAddr[0] = htonl(ipcp_p->peer.ipAddress);
		pAddr[1] = 0;	// don't actually need this but to be safe
		DEBUGMSG(ZONE_PPP, (TEXT("PPP: Setting ppp_peer IPAddr = %x\n"), pAddr[0]));
	}
	else
	{
		DEBUGMSG(ZONE_PPP, (TEXT("PPP: Clearing ppp_peer IPAddr\n")));
	}

    AFDAddIPHostent(TEXT("ppp_peer"), pppAddr, NULL, 0);
}

//  LinkUpIndication
//
//  Function:   Handles link up indication to IP. Called by
//              IPCP when link can xfer data.
//

void
LinkUpIndication( PPP_CONTEXT *pppContext )
{
	LLIPMTUChange   mtuchange;
	pppSession_t    *s_p    = (pppSession_t *)pppContext->Session;
	PLCPContext     lcp_p  = (PLCPContext)s_p->lcpCntxt;
	BOOL            bAddWorked;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +LinkUpIndication( %s )\n"), pppContext->AdapterName ));

    ASSERT( pppContext );

	if (s_p->bIsServer)
		pppContext->fOpen = TRUE;

	// Unlock the session for calls into TCP/IP module
	pppUnLock( s_p );

    DEBUGCHK( s_p->SesCritSec.OwnerThread != (HANDLE)GetCurrentThreadId());

	// Register the new interface with IP
	bAddWorked = PPPAddInterface(pppContext);

	if (bAddWorked)
	{
		// Notify upper layer of MTU change according to Mode

		switch( s_p->Mode )
		{
		case PPPMODE_PPP:
			SetPPPPeerIPAddress(pppContext);

			// Use the negotiated peer MTU
			mtuchange.lmc_mtu = lcp_p->peer.MRU;
			break;

		case PPPMODE_SLIP:
		case PPPMODE_CSLIP:

			mtuchange.lmc_mtu = SLIP_DEFAULT_MTU;
			break;

		default: ASSERT( 0 );
		}

		// Indicate MTU change to IP

		if (pppContext->fOpen && pppContext->IPContext)
		{
			IPStatus( pppContext->IPContext,
					LLIP_STATUS_MTU_CHANGE,
					&mtuchange,
					sizeof( LLIPMTUChange ),
					NULL);
		}
	}

	pppLock (s_p);

	if (!bAddWorked)
	{
		//  Unable to register with IP!!!
		//
		//	IP may be unable to register the interface because the IP address
		//	is invalid or in use, or there was insufficient memory.
		//	If this happens, we need to terminate the PPP connection.
		//

		DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - Unable to register interface %s with IP\n"), pppContext->AdapterName));

		// Request LCP terminate link

		pppLcp_Close(lcp_p, NULL, NULL);
	}


	DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -LinkUpIndication( %s )\n"), pppContext->AdapterName ));
}

//  LinkDownIndication
//
//  Function:   Handles link dn indication to IP. Called by
//              IPCP and CCP when link can not xfer data,
//              or by SLIP when the MAC layer is down.
//

void
LinkDownIndication( PPP_CONTEXT *pppContext, pppSession_t *s_p )
{
	PPP_CONTEXT *pppContextYoungestActive;

    DEBUGMSG( ZONE_PPP, (TEXT("LinkDownIndication( 0x%X, 0x%X )\r\n"), pppContext, s_p ));

	// This function must be called with the session in use.
    ASSERT( s_p->RefCnt > 0 );

	// Can't hold session lock for call into IP
    pppUnLock (s_p);

    pppContext->fOpen = FALSE;

    DEBUGCHK( s_p->SesCritSec.OwnerThread != (HANDLE)GetCurrentThreadId());

    // Process according to Mode

    switch( s_p->Mode )
    {
    case PPPMODE_PPP:
		//
		//	Set the ppp_peer's IP address to be that of the youngest
		//	remaining active PPP session, or NULL if none.
		//
		EnterCriticalSection (&v_ListCS);

		for (pppContextYoungestActive = pppContextList;
			 pppContextYoungestActive != NULL;
			 pppContextYoungestActive = pppContextYoungestActive->Next)
		{
			if (pppContextYoungestActive != pppContext
			&&  pppContextYoungestActive->fOpen)
			{
				break;
			}
		}

		LeaveCriticalSection (&v_ListCS);

		SetPPPPeerIPAddress(pppContextYoungestActive);

        break;

    case PPPMODE_SLIP:
    case PPPMODE_CSLIP:
        break;

    default: ASSERT( 0 );
    }

	// Delete the IP interface
	PPPDeleteInterface(s_p->context, s_p->context->Session);

    pppLock (s_p);
}



