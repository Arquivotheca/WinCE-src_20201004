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

// PPP CCP Layer Message Processing

#include "windows.h"
#include "cclib.h"

#include "memory.h"
#include "cxport.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "auth.h"
#include "ccp.h"
#include "ncp.h"
#include "mac.h"
#include "ip_intf.h"

void
ccpRxResetRequest(
	IN	PVOID	context,
	IN	BYTE	code,
	IN  BYTE    id,
	IN	PBYTE	pData,
	IN	DWORD	cbData)
//
//	Called when we receive a CCP Reset-Request packet
//
{
	PCCPContext  pContext = (PCCPContext)context;

	DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT( "PPP: ccpRxResetRequest\n" )));

    // A compression reset forces a TX flush sequence. We set the flush
    // flag to force the next tx packet to be uncompressed and re-initialize
    // the send context.

    // Flush next tx packet

    pContext->txFlush = TRUE;

    initsendcontext( &pContext->mppcSndCntxt );

    // NOTE:
    // The Microsoft implementation does not send an ACK
    // but rather sets the PACKET_FLUSH bit on the next outgoing packet
    // to signal a reset.
}

void
ccpRxResetAck(
	IN	PVOID	context,
	IN	BYTE	code,
	IN  BYTE    id,
	IN	PBYTE	pData,
	IN	DWORD	cbData)
//
//	Called when we receive a CCP Reset-Ack packet
//
{
	PCCPContext  pContext = (PCCPContext)context;

	DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT( "PPP: ccpRxResetAck\n" )));

	// MS CCP does not use the ACK mechanism.
}

void
ccpTxResetRequest(
	IN PCCPContext  pContext)
//
//  Send Reset Request Packet
//
{
	BYTE    resetRequestPacket[PPP_PACKET_HEADER_SIZE];

    pContext->idResetRequest++;
	PPP_INIT_PACKET( &resetRequestPacket[0], CCP_RESET_REQ, pContext->idResetRequest);
	pppSendData(pContext->session, PPP_PROTOCOL_CCP, resetRequestPacket, PPP_PACKET_HEADER_SIZE);
}

void
CpktProcessRxPacket(
	IN  OUT PVOID        context,
	IN  OUT pppMsg_t    *pMsg)
//
//	Process a compressed/encrypted data packet received
//  from the peer.
//
{	PCCPContext pContext = (PCCPContext)context;
	PBYTE           pDecompressed;
    DWORD           cbDecompressed;
	USHORT          coherencyHdr;

    DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT("PPP: RX CCP Data Message\n")));

	if (pContext->pFsm->state != PFS_Opened)
	{
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Ignoring CCP data packet, CCP not opened\n")));
		return;
	}

#ifdef DEBUG
	if (ZONE_TRACE)
	{
		DEBUGMSG (1, (TEXT("PPP: CCP: RX Packet (%d):\n"), pMsg->len));
		DumpMem (pMsg->data, pMsg->len);
	}
#endif // DEBUG

    // Access the Coherency Header and adjust msg

	if (pMsg->len < 2)
	{
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - CCP compressed packet has no coherency header\n")));
		return;
	}

    coherencyHdr = (pMsg->data[0] << 8) + pMsg->data[1];
    pMsg->data += 2;
    pMsg->len  -= 2;

    DEBUGMSG( ZONE_NCP, (TEXT( "PPP: CCP: RX: coherency hdr= 0x%x\n" ), coherencyHdr ));

    // Force a resync for a FLUSHED packet

    if( coherencyHdr & (PACKET_FLUSHED << 8) )
    {
        DEBUGMSG (ZONE_CCP || ZONE_TRACE, (TEXT( "PPP: CCP RX FLUSH LastGood=0x%x New=0x%x.\n"), pContext->rxCoherency, coherencyHdr));           

		//
		//	If the peer acked our config-request for encryption, then
		//  the peer will be sending encrypted packets to us so we need
		//  to reset the receive encrypt context.
		//

		if (pContext->local.SupportedBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128))
		{
			//
			// If it appears that this FLUSHED packet is stale, that is, we have
			// already changed our session key to the next one because we passed
			// a flag packet, then ignore it.
			//
			// That is, if we receive packets out of order due to some strangeness
			// at the MAC layer like this:
			// 
			//      RX CC = 0FE
			//      RX CC = 0FF (Session Key changed)
			//      RX CC = 100
			//      RX CC = 0FC FLUSHED	
			//
			// Then the CC=0FC packet is probably being received out of order. If we
			// accepted it as valid and in order then that would mean that we missed
			// the almost 4000 packets from 101 to 0FB, and thus would need to change
			// our session key 15 times to account for the intervening missed flag
			// packets. More likely, however, is that the 0FC packet is old and delivered
			// out of order for some reason. If we change our session key 15 times then
			// we will permanently lose sync with the transmitter's encryption key.
			//
			// So, rather than changing the encryption key 15 times we ignore the packet.
			// As a simplification, we ignore any FLUSHED packet with a CC that is in the
			// range of the 256 most recent CC's we received.
			//
			if ((((coherencyHdr & 0x0fff) - pContext->rxCoherency) & 0x0fff) >= 0x0f00)
			{
				DEBUGMSG(ZONE_WARN, (L"PPP: CCP RX Ignore FLUSHED Out-of-order - rx=%04x exp=%04x\n", coherencyHdr, pContext->rxCoherency));
				return;
			}
           
			//
			//  Each packet that gets encrypted modifies the current encryption key state. As a result, if a packet
			//  is lost the receiver will not be able to decode subsequent packets because its decryption key will
			//  not be in sync with the encryption key. To solve this issue, the MPPE protocol specifies that when
			//  a CCP packet is sent with the PACKET_FLUSHED bit set, the encryption key is reset to discard all
			//  prior packet data that has modified its state.
			//
			//  That is, suppose the receiver processes packets with CC=0 through CC=2A. The encryption key for both
			//  sender and receiver has been modified by all the data in these packets. But then CC=2B is dropped.
			//  The sender's encryption key has been modified by the data of packet 2B, but the receiver's has not,
			//  so the receiver cannot decrypt any more packets in this stream. So, the receiver sends a CCP Reset
			//  request to the sender. This causes the sender to reset its encryption key, discarding all the packet
			//  data history from CC=0 to CC=2B prior to encrypting its next packet, which it sends with FLUSHED=1.
			//  The receiver, when it sees the FLUSHED bit, then resets its decryption key discarding all prior
			//  packet data and thus is able to once again decrypt packets in the stream.
			//
			
			EncryptionInfoReinitializeKey(pContext->prxEncryptionInfo);					 
		}

		//
		//	If the peer acked our config-request for MCCP_COMPRESSION, then
		//  the peer will be sending compressed packets to us so we need
		//  to reset the receive compress context.
		//
		if (pContext->local.SupportedBits & MCCP_COMPRESSION)
		{
			initrecvcontext( &pContext->mppcRcvCntxt );          // reset rcv context
		}

        //
        //  This is now our new coherency count.
        //

        pContext->rxCoherency = coherencyHdr & 0x0fff;

        //
        //  We are back in sync...
        //

        pContext->bRxOutOfSync = FALSE;
        
    }

    // If the received coherency counters agree then we are in sync. 
    // Note: if the packet was a flush packet then the above logic 
    // forces the coherency counters to agree and resets the context 
    // so we are in sync here, albeit, without any history.

    if ((pContext->bRxOutOfSync)
	|| ((coherencyHdr & 0x0fff) != pContext->rxCoherency))
    {     
        DEBUGMSG( ZONE_ERROR | ZONE_NCP, (TEXT( "PPP: CCP:rx:TOSS PKT - BAD COHERENCY (got %x expected %x\n"),
			coherencyHdr, pContext->rxCoherency));         

        // Changes for coherency fix 
        // this indicates a lost packet, try re-syncing our
        // coherency counter, reseting the context, and the
        // sending a reset request packet

        pContext->bRxOutOfSync = TRUE;     
   		ccpTxResetRequest(pContext);
        return;        
    }

    //
    //  This is the next good coherency number we expect.
    //

    pContext->rxCoherency = (pContext->rxCoherency + 1) & 0x0FFF;


	// If packet is encrypted then attempt to decrypt
    if( coherencyHdr & (PACKET_ENCRYPTED << 8) )
	{
        DEBUGMSG( ZONE_CCP, (TEXT( "PPP: CCP - Decrypting packet\n" )));

		DecryptRxData(pContext, pMsg->data, pMsg->len);

#ifdef DEBUG
		if (ZONE_TRACE)
		{
			DEBUGMSG (1, (TEXT("PPP: CCP: RX Decrypted packet (%d):\n"), pMsg->len));
			DumpMem (pMsg->data, pMsg->len);
		}
#endif
	}

    // If packet is compressed attempt to decompress

    if( coherencyHdr & (PACKET_COMPRESSED << 8) ) 
    {
		if (!(pContext->local.SupportedBits & MCCP_COMPRESSION))
		{
			DEBUGMSG(ZONE_ERROR, (TEXT("PPP: CCP-ERROR Rx compressed packet with compression disabled\n")));
			return;
		}

        if( decompress( pMsg->data, 
                        pMsg->len, 
                        ((coherencyHdr & (PACKET_AT_FRONT << 8)) >> 8), 
                        &pDecompressed, 
                        &cbDecompressed, 
                        &pContext->mppcRcvCntxt ) == FALSE )
        {
            // Coherency is lost - Request a reset from Peer

            DEBUGMSG( ZONE_CCP | ZONE_ERROR, (TEXT( "CCP:RX:PKT DECOMPRESS FAILED - REQUEST RESET\r\n" )));

            //
            //  Should not reduce the pContext->rxCoherency here because we have already
            //  done the DecryptRxData().
            //
            
			ccpTxResetRequest(pContext);

			//
			//  We are in out of sync state.   Only flush packet can get us back...
			//
			
            pContext->bRxOutOfSync = TRUE;
            return;

        }
		//
		// The decompressed data pointer points into the history buffer.
		// If NAT is active, the receive packet data that
		// we indicate up may be modified. Since the receive
		// packet data is in the CCP history buffer, this
		// will corrupt the history. So, we must copy the
		// packet to a new buffer prior to indicating it.
		//

		if (cbDecompressed > pContext->cbScratchRxBuf)
		{
			// We were sent a packet which is too big. Discard.
			DEBUGMSG( ZONE_ERROR, (TEXT( "PPP: CCP-ERROR decompressed packet is %d bytes > Max allowable %d\n" ),
				cbDecompressed, pContext->cbScratchRxBuf));
			return;
		}

		memcpy(pContext->ScratchRxBuf, pDecompressed, cbDecompressed);
		pMsg->data = pContext->ScratchRxBuf;
		pMsg->len  = cbDecompressed;

		DEBUGMSG( ZONE_CCP, (TEXT( "PPP: CCP RX decompress OK\n" )));

#ifdef DEBUG
		if (ZONE_TRACE)
		{
			DEBUGMSG (1, (TEXT("PPP: CCP: RX Decompressed packet (%d):\n"), pMsg->len));
			DumpMem (pMsg->data, pMsg->len);
		}
#endif
    }
    else
    {
        DEBUGMSG( ZONE_CCP, ( TEXT( "CCP: RX Packet NOT COMPRESSED\n" )));
    }

	// Pass the payload of the CCP packet back through the receive packet engine
	PPPRxPacketHandler(pContext->session, pMsg);
}

void
CcpProcessRxPacket(
	IN  PVOID        context,
	IN  pppMsg_t    *pMsg)
//
//	Process a CCP packet received from the peer.
//
{
	PCCPContext pContext  = (PCCPContext )context;

    ASSERT (PPP_PROTOCOL_CCP == pMsg->ProtocolType );

	// If the layer below CCP (usually auth) has not indicated that it is up yet,
	// then receiving a CCP packet means that we probably lost an
	// authentication success packet from the peer. So, for some
	// authentication protocols the reception of a network layer
	// packet (e.g. CCP, IPCP) is implicit authentication success.
	//
	if (pContext->pFsm->state <= PFS_Starting)
	{
		AuthRxImpliedSuccessPacket(pContext->session->authCntxt);
	}

	// If we did not open the FSM earlier, because we did
	// not want compression/encryption, open the FSM now
	// that the peer has indicated that it wants
	// compression/encryption.
	if (pContext->pFsm->state == PFS_Closed)
	{
		DEBUGMSG(ZONE_CCP, (TEXT("PPP: Doing deferred CCP FSM Open as peer has requested CCP options\n")));
		PppFsmOpen(pContext->pFsm);
	}

	PppFsmProcessRxPacket(pContext->pFsm, pMsg->data, pMsg->len);
}

/*****************************************************************************
* 
*   @func   void | pppCcp_Compress | CCP Datagram compression
*   
*   @parm   pppSession_t * | session | PPP session context.
*   @parm   PNDIS_WAN_PACKET * | ppPacket   | Pointer to a ptr to NDIS_WAN_PACKET
*               
*   @comm   This function will compress a packet if tx compression is active,
*			and encrypt it if tx encryption is active.
*
*/

BOOL
pppCcp_Compress(
	IN      pppSession_t     *pSession,
	IN  OUT PNDIS_WAN_PACKET *ppPacket,
	IN  OUT USHORT           *pwProtocol )
//
//  Compress and/or encrypt the input packet.
//
//  Return FALSE if the packet should be discarded because CCP is not open
//  and encryption is required.
//  Otherwise, return TRUE (regardless of whether the packet was compressed
//  and/or encrypted) indicating that the packet should be sent.
//
{
	ncpCntxt_t         *ncp_p    = (ncpCntxt_t *)pSession->ncpCntxt;
	PCCPContext         pContext = (PCCPContext)ncp_p->protocol[ NCP_CCP ].context;
	RASPENTRY	       *pRasEntry = &pSession->rasEntry;
	PNDIS_WAN_PACKET	pCompressedPacket;
	PNDIS_WAN_PACKET	pPacket;
	USHORT				coherencyHeader;

	DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppCcp_Compress( 0x%X, 0x%X, 0x%X)\r\n" ), pSession, ppPacket, pwProtocol ));
    DEBUGMSG(ZONE_NCP && (pContext->peer.SupportedBits == 0), (TEXT("PPP: CCP called to send packet but TX COMPRESSION/ENCRYPTION OFF\n")));

	//
	// If CCP is not opened, or it is opened but the we agreed not to
	// encrypt or compress packets sent to the peer, then we cannot send
	// packets using CCP compression/encryption, we must send them uncompressed.
	//
	if (pContext->pFsm->state != PFS_Opened
	||  pContext->peer.SupportedBits == 0)
	{
		// Cannot compress or encrypt packet
		if (NEED_ENCRYPTION(pRasEntry))
		{
			DEBUGMSG(ZONE_WARN, (L"PPP: WARNING - must encrypt but CCP state=%x\n", pContext->pFsm->state));

            // Encryption of packets is mandatory, so we just discard it
			//
			// If we  needed encryption on this connection then we should NEVER
			// have allowed CCP to enter the Opened state without negotiating
			// the ability to send encrypted packets to the peer.
			//
			ASSERT(pContext->pFsm->state != PFS_Opened);
			return FALSE;
		}
		else
		{
			// Just send the packet unencrypted, uncompressed
			return TRUE;
		}
	}

	// CCP is open, and we negotiated the use of compression and/or encryption

	pPacket = *ppPacket;
	
    // Encode the protocol type into the packet data.
	// It needs to be there because it is subject to compression and encryption.
    // NOTE: currently ignoring whether protocol field compression is enabled or not.
	//
	pPacket->CurrentBuffer -= 2;
	pPacket->CurrentLength += 2;
	pPacket->CurrentBuffer[0] = (UCHAR )( *pwProtocol >> 8 );
    pPacket->CurrentBuffer[1] = (UCHAR )( *pwProtocol );

	// The new compressed/encrypted packet type is 0x00FD, which will be encoded into
	// the packet outside of this function, in front of the coherency header.
	*pwProtocol = PPP_PROTOCOL_COMPRESSION;

    DEBUGMSG( ZONE_NCP, (TEXT( "ccp:tx:raw pkt length: %d\n" ), pPacket->CurrentLength ));

    // Setup the compression/encryption packet header, the "ABCD" bits and the coherency count.
	coherencyHeader = pContext->txCoherency;
	ASSERT((coherencyHeader & 0xF000) == 0);

    DEBUGMSG( ZONE_NCP, (TEXT("ccp:tx:coherency cnt: %x\n"), coherencyHeader));

    // Bump coherency counter with 12 bit limit
    pContext->txCoherency = (pContext->txCoherency + 1) & 0x0fff;

	//
	// If compression is enabled, then try to compress the packet data
	//
	if( pContext->peer.SupportedBits & MCCP_COMPRESSION)
	{
		// Get a new WAN packet into which to try to compress the packet data.
		pCompressedPacket = pppMac_GetPacket (pSession->macCntxt);

		// If we are unable to get a packet we will just send it uncompressed.

		if (!pCompressedPacket)
		{
			if( pContext->peer.SupportedBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128)) {

				coherencyHeader |= PACKET_FLUSHED << 8;
				initsendcontext(&pContext->mppcSndCntxt);
				// force a start over next time
				pContext->mppcSndCntxt.CurrentIndex = HISTORY_SIZE+1 ;

				// Set txFlush to force encryption to reinitialize its table.
				pContext->txFlush = TRUE;
			}
		}
		else
		{
			pCompressedPacket->ProtocolReserved2 = pPacket->ProtocolReserved2;

			// Reserve the following (worst case) header bytes:
			//    2 - possible address and control bytes (FF 03)
			//    2 - CCP protocol type (00 FD) (sometimes gets compressed to 1)
			//    2 - CCP coherency header
			//
			// Do not need to reserve 2 bytes for the encapsulated protocol type,
			// since it is part of the compressed payload, not header.
			//
			pCompressedPacket->CurrentBuffer += 6;
			pCompressedPacket->CurrentLength -= 6;

#ifdef DEBUG
			if (ZONE_TRACE) {
				DEBUGMSG (1, (TEXT("pppCcp_Compress: About to compress packet (%d):\n"),
							  pPacket->CurrentLength));
				DumpMem (pPacket->CurrentBuffer, pPacket->CurrentLength);
			}
#endif

			// Compress the packet data in pPacket into pCompressedPacket
			// Set the "FLUSHED" and "COMPRESSED" bits in the coherency header. 

			pCompressedPacket->CurrentLength = pPacket->CurrentLength;
			coherencyHeader |= compress( pPacket->CurrentBuffer,
										 pCompressedPacket->CurrentBuffer,
										&pCompressedPacket->CurrentLength,
										&pContext->mppcSndCntxt );
		
			DEBUGMSG( ZONE_NCP, ( TEXT( "ccp:tx:compress %hs- Len: orig=%d comp=%d\n" ),
				(coherencyHeader & (PACKET_FLUSHED << 8)) ? "FAILED" : "OK",
				pPacket->CurrentLength, pCompressedPacket->CurrentLength));

			// If compression failed, free pCompressedPacket.
			// Otherwise, switch to use pCompressedPacket instead of the original, uncompressed packet.
			// 
			// Note: If compression fails the PACKET_FLUSHED bit is set in the coherency
			// header such that our peer resets its history.
			if( coherencyHeader & (PACKET_FLUSHED << 8))
			{
				// Compression Failed - Send pOrigPacket
				// Free the temporary packet we allocated.
				NdisWanFreePacket (pSession->macCntxt, pCompressedPacket);

				// Set txFlush to force encryption to reinitialize its table.
				ASSERT(0 == (coherencyHeader & (PACKET_COMPRESSED << 8)));
				pContext->txFlush = TRUE;
			}
			else
			{
				// Packet compressed - discard original packet and use the new compressed packet
				NdisWanFreePacket (pSession->macCntxt, pPacket);

				*ppPacket = pPacket = pCompressedPacket;
			}
		}
	}

	//
	// Encrypt the data if encryption is enabled
	//
	if( pContext->peer.SupportedBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128))
	{
#ifdef DEBUG
		if (ZONE_TRACE) {
			DEBUGMSG (1, (TEXT("pppCcp_Compress: About to encrypt packet (%d):\n"),
						  pPacket->CurrentLength));
			DumpMem (pPacket->CurrentBuffer, pPacket->CurrentLength);
		}
#endif
		coherencyHeader |= EncryptTxData(pContext, pPacket->CurrentBuffer, pPacket->CurrentLength);
	}

    // If the txFlush flag is set then we received a RESET REQUEST
    // packet from our peer and the tx context was reset and the out
    // going packet was created without any history. Therefore, add in
    // the PACKET_FLUSHED bit to the outgoing packet.

    if( pContext->txFlush )
	{
        DEBUGMSG( ZONE_NCP, (TEXT("PPP: CCP - TX sending PACKET_FLUSHED\n" )));

		pContext->txFlush = FALSE;
		coherencyHeader |= PACKET_FLUSHED << 8;
    }

	//
	// Add the 2 byte coherency header to the packet:
	//		ABCD bits:         4 bits
	//		Coherency Count:  12 bits
	//
	pPacket->CurrentBuffer -= 2;
	pPacket->CurrentLength += 2;
	pPacket->CurrentBuffer[0] = (UCHAR )( coherencyHeader >> 8 );
    pPacket->CurrentBuffer[1] = (UCHAR )  coherencyHeader;

	return TRUE;
}
