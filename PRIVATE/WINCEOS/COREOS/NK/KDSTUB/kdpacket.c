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

    KdPacket.c

Abstract:

    This module implements the Packetization of KdApi commands

Revision History:

--*/

#include "kdp.h"
#include "ethdbg.h"

// TODO: check checksums

// Pointer to buffer for formatting KDBG commands to send over ether.  Is set in the
// EdbgRegisterDfltClient function. Size of buffer is EDBG_MAX_DATA_SIZE.
static UCHAR *KdbgEthBuf;

// static BOOL fResetSent = FALSE;

static BOOL KdRecv (BYTE * pbuf, DWORD * pdwLen)
{
    BOOL fRet;
    // Note that we can't trust our flag to tell us whether ints are enabled
    // at this point - if the debugger needs to access the loader for example
    // for a page fault, ints may be turned on by the kernel.
    g_kdKernData.pINTERRUPTS_OFF();
    fRet = g_kdKernData.pKITLIoCtl (IOCTL_EDBG_RECV, pbuf, EDBG_SVC_KDBG, pdwLen, INFINITE, NULL);
    return fRet;
}

static BOOL KdSend (BYTE * pbData, DWORD dwLen)
{
    BOOL fRet = TRUE;
    KD_ASSERT (dwLen <= EDBG_MAX_DATA_SIZE);
    
    g_kdKernData.pINTERRUPTS_OFF();
    fRet = g_kdKernData.pKITLIoCtl (IOCTL_EDBG_SEND, pbData, EDBG_SVC_KDBG, NULL, dwLen, NULL);
    return fRet;
}


/*++

Routine Description:

    This routine computes the checksum for the string passed in.

Arguments:

    Buffer - Supplies a pointer to the string.

    Length - Supplies the length of the string.

Return Value:

    A DWORD is return as the checksum for the input string.

--*/

DWORD KdpComputeChecksum (/* [in] */ BYTE * Buffer, /* [in] */ DWORD Length)
{
    DWORD dwChecksum = 0;

//  We haven't found a good reason for flushing the cache here, and at least on SH3, it
//  results in an invalid checksum.
//    KeSweepCurrentIcache ();

    while (Length > 0)
    {
        dwChecksum = dwChecksum + (DWORD) *Buffer++;
        Length--;
    }

//    KeSweepCurrentIcache ();
    return dwChecksum;
}


static void KdpSendControlPacket
(
    /* [in] */ WORD wPacketType,
    /* [in, optional] */ DWORD dwPacketId
)
/*++

Routine Description:

    This routine sends a control packet to the host machine that is running the
    kernel debugger and waits for an ACK.

Arguments:

    dwPacketType - Supplies the type of packet to send.

    dwPacketId - Supplies packet id, optionally.

Return Value:

    None.

--*/
{
    KD_PACKET_HEADER PacketHeader;

    //
    // Initialize and send the packet header.
    //
    PacketHeader.dwPacketLeader = CONTROL_PACKET_LEADER;
    PacketHeader.wPacketType    = wPacketType;
    PacketHeader.wDataByteCount = 0;
    PacketHeader.dwPacketId     = dwPacketId;
    PacketHeader.dwDataChecksum = 0;

    KdSend ((BYTE *)&PacketHeader, sizeof(KD_PACKET_HEADER));
}


WORD HandleBreakInPacket
(
    KD_PACKET_HEADER *pPacketHeader
)
/*++

Routine Description:

    This routine was (apparently) originally used to break in to the standard
    protocol during exceptional conditions.  As it currently serves no
    purpose, it is implemented as a no-op, but remains here in case a need
    arises.

Arguments:

    pPacketHeader - pointer to a break-in packet header

Return Value:

    Not used

--*/
{
    return KDP_PACKET_NONE;
}


WORD HandleControlPacket
(
    KD_PACKET_HEADER *pPacketHeader
)
{
    WORD wRet = KDP_PACKET_NONE;
    
    
    switch (pPacketHeader->wPacketType) {
    case PACKET_TYPE_KD_RESEND:
        // Got Resend request
        wRet = KDP_PACKET_RESEND;    
        break;
    case PACKET_TYPE_KD_RESET:
//        if (!fResetSent) 
//        {
            // Got Reset and send Reset
//            KdpNextPacketIdToSend = INITIAL_PACKET_ID;
//            KdpPacketIdExpected = INITIAL_PACKET_ID;
            KdpSendControlPacket(PACKET_TYPE_KD_RESET, 0L);
//            fResetSent = TRUE;
            wRet = KDP_PACKET_RESEND;
//        }
        break;
//    case PACKET_TYPE_KD_ACKNOWLEDGE:
        // toss it.
//        break;
    default:
        KD_ASSERT (0);
        
    }

    return wRet;
}


WORD HandleDataPacket
(
    /* [in]  */ BYTE* pbPacket,
    /* [in]  */ DWORD dwRead,
    /* [in]  */ DWORD dwPacketType,
    /* [out] */ STRING* MessageHeader,
    /* [out] */ STRING* MessageData,
    /* [out] */ DWORD* pdwDataLength
)
/*++

Routine Description:


Arguments:

    pbPacket - buffer containing the complete KD packet
    dwPacketType - Supplies the type of packet that is excepted.
    MessageHeader - Supplies a pointer to a string descriptor for the input
        message.
    MessageData - Supplies a pointer to a string descriptor for the input data.
    pdwDataLength - Supplies pointer to DWORD to receive length of recv. data.

Return Value:

    KDP_PACKET_RECEIVED - if packet received.
    KDP_PACKET_UNEXPECTED - if a non-data packet was rec'd during processing

--*/
{
    WORD wRet = KDP_PACKET_RECEIVED;
    KD_PACKET_HEADER *pPacketHeader = (KD_PACKET_HEADER *)pbPacket;
    DWORD dwToRead;
    BYTE *pbData = pbPacket + sizeof(KD_PACKET_HEADER);
    BYTE *pbDst;

    KD_ASSERT(MessageHeader);

    dwRead -= sizeof(KD_PACKET_HEADER);    // take away header
    KD_ASSERT((dwRead > MessageHeader->MaximumLength)
        && (pPacketHeader->wDataByteCount <= (WORD)PACKET_DATA_MAX_SIZE) 
        && (pPacketHeader->wDataByteCount >= (WORD)MessageHeader->MaximumLength));

    // copy header
    memcpy(MessageHeader->Buffer, pbData, MessageHeader->MaximumLength);

    // update pointers
    pbData += MessageHeader->MaximumLength;
    dwRead -= MessageHeader->MaximumLength;
    MessageHeader->Length = (WORD)MessageHeader->MaximumLength;
    
    // the message data.
    pbDst = (BYTE *)MessageData->Buffer;
    *pdwDataLength = pPacketHeader->wDataByteCount - MessageHeader->MaximumLength;
    dwToRead = *pdwDataLength + 1; // add trailing byte

    memcpy(pbDst, pbData, (dwRead < *pdwDataLength) ? dwRead : *pdwDataLength);

    // if the packet is bigger than the MTU, we need to receive
    // the rest of the packet
    pbDst += dwRead;
    dwToRead -= dwRead;

    while (dwToRead > dwRead)
    {
        DWORD dwRead = (dwToRead < EDBG_MAX_DATA_SIZE) ? dwToRead : EDBG_MAX_DATA_SIZE;
        
        if (!KdRecv(pbPacket, &dwRead))
        {
            KD_ASSERT(0); // shouldn't happen
        }

        //
        // it's not outside the realm of imagination that the protocol could be interrupted
        // somehow, so for every packet, we need to make sure the host hasn't sent us a
        // non-data packet hoping to recover our state
        //
        // WARNING: this creates a certain pattern of data that is impossible to send to
        // the target, i.e. a data payload spanning multiple KITL frames containing a
        // false CONTROL_PACKET_LEADER at the start of one of its subsequent frames.
        // Hopefully we can play the odds on this one.  The host rarely (if ever) sends
        // fat frames to the target, anyway.
        //
        // pdwDataLength has been mildly overloaded to return the size of the packet
        //
        if (pPacketHeader->dwPacketLeader == CONTROL_PACKET_LEADER)
        {
            wRet = KDP_PACKET_UNEXPECTED;
            *pdwDataLength = dwRead;
            goto L_Done;
        }

        memcpy(pbDst, pbPacket, dwRead);

        pbDst += dwRead;
        dwToRead -= dwRead;
    }

    // TODO: Add a sequence or something to the packet, e.g. use dwPacketId
    // TODO: Check for the trailing byte
    
    MessageData->Length = (WORD)*pdwDataLength;

    // Check wPacketType is what we are waiting for.
    KD_ASSERT(dwPacketType == pPacketHeader->wPacketType);

L_Done:

    return wRet;
}


WORD KdpReceivePacket
(
    /* [in] */  DWORD dwPacketType,
    /* [out] */ STRING* MessageHeader,
    /* [out] */ STRING* MessageData,
    /* [out] */ DWORD* pdwDataLength
)
/*++

Routine Description:

    This routine receives a packet from the host machine that is running
    the kernel debugger UI.  This routine is ALWAYS called after packet being
    sent by caller.  It first waits for ACK packet for the packet sent and
    then waits for the packet desired.

    N.B. If caller is KdPrintString, the parameter dwPacketType is
       PACKET_TYPE_KD_ACKNOWLEDGE.  In this case, this routine will return
       right after the ack packet is received.

Arguments:

    dwPacketType - Supplies the type of packet that is excepted.

    MessageHeader - Supplies a pointer to a string descriptor for the input
        message.

    MessageData - Supplies a pointer to a string descriptor for the input data.

    pdwDataLength - Supplies pointer to DWORD to receive length of recv. data.

Return Value:

    KDP_PACKET_RESEND - if resend is required.
    KDP_PAKCET_TIMEOUT - if timeout.
    KDP_PACKET_RECEIVED - if packet received.

--*/
{
    static BYTE pbPacket[EDBG_MAX_DATA_SIZE];

    WORD wRet = KDP_PACKET_NONE;
    DWORD dwRead;
    KD_PACKET_HEADER *pPacketHeader = (KD_PACKET_HEADER *) pbPacket;
    BOOL fAlreadyReadPacket = FALSE;

    DEBUGGERMSG(KDZONE_PACKET, (L"++KdpReceivePacket\r\n"));

    do
    {
        dwRead = EDBG_MAX_DATA_SIZE;

        if (fAlreadyReadPacket)
        {
            dwRead = *pdwDataLength;
            fAlreadyReadPacket = FALSE;
        }
        else if (!KdRecv(pbPacket, &dwRead))
        {
            DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceivePacket: Error - no data to receive!\n"));
            KD_ASSERT(0);       // shouldn't happen
            
            wRet = KDP_PACKET_RESEND;
            break;
        }

        KD_ASSERT(dwRead >= sizeof (KD_PACKET_HEADER));

        DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceivePacket: Packet Leader = 0x%08X\n", pPacketHeader->dwPacketLeader));
    
        switch (pPacketHeader->dwPacketLeader)
        {
            case BREAKIN_PACKET:
                // handle breakin packet
                wRet = HandleBreakInPacket(pPacketHeader);
                break;

            case CONTROL_PACKET_LEADER:
                // handle control packet
                wRet = HandleControlPacket(pPacketHeader);
                break;

            case DATA_PACKET_LEADER:
                // handle data packet
                wRet = HandleDataPacket(pbPacket, dwRead, dwPacketType, MessageHeader, MessageData, pdwDataLength);
                DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceivePacket: HandleDataPacket returned = 0x%04X\n",wRet));
                if (wRet == KDP_PACKET_UNEXPECTED)
                {
                    DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceivePacket: Unexpected Packet\n"));
                    // A non-data packet was received during the processing of a multi-packet message
                    // It's sitting in the receive buffer waiting to be processed, *pdwDataLength contains
                    // the size
                    wRet = KDP_PACKET_NONE;
                    fAlreadyReadPacket = TRUE;
                }
                break;

            default:
                KD_ASSERT(0);
        }
    }
    while (wRet == KDP_PACKET_NONE);

    DEBUGGERMSG(KDZONE_PACKET, (L"--KdpReceivePacket: wRet = 0x%04X\r\n", wRet));

    return wRet;
}


void KdpSendPacket
(
    /* [in] */ DWORD dwPacketType,
    /* [in] */ STRING * MessageHeader,
    /* [in, optional] */ STRING * MessageData
)
/*++

Routine Description:

    This routine sends a packet to the host machine that is running the
    kernel debugger and waits for an ACK.

Arguments:

    dwPacketType - Supplies the type of packet to send.

    MessageHeader - Supplies a pointer to a string descriptor that describes
        the message information.

    MessageData - Supplies a pointer to a string descriptor that describes
        the optional message data.

Return Value:

    None.

--*/
{
    static BYTE SndBuf[EDBG_MAX_DATA_SIZE];
    KD_PACKET_HEADER *pPacketHeader = (KD_PACKET_HEADER *)SndBuf;
    DWORD MessageDataLength;
    DWORD dwSize;

    if (MessageData)
    {
        MessageDataLength = MessageData->Length;
        pPacketHeader->dwDataChecksum = KdpComputeChecksum(MessageData->Buffer, MessageDataLength);
    }
    else
    {
        MessageDataLength = 0;
        pPacketHeader->dwDataChecksum = 0;
    }

    pPacketHeader->dwDataChecksum += KdpComputeChecksum(MessageHeader->Buffer, MessageHeader->Length);

    //
    // Initialize and send the packet header.
    //
    pPacketHeader->dwPacketLeader = DATA_PACKET_LEADER;
    pPacketHeader->wDataByteCount = (WORD)(MessageHeader->Length + MessageDataLength);
    pPacketHeader->wPacketType    = (WORD)dwPacketType;
    pPacketHeader->dwPacketId     = 0; // TODO: use this

    dwSize = sizeof(KD_PACKET_HEADER) + MessageHeader->Length;
    KD_ASSERT(dwSize < EDBG_MAX_DATA_SIZE);
    
    memcpy(SndBuf + sizeof(KD_PACKET_HEADER), MessageHeader->Buffer, MessageHeader->Length);

    if (MessageDataLength) 
    {
        DWORD dwMax = EDBG_MAX_DATA_SIZE - dwSize;
        DWORD dwToSend = MessageData->Length;
        BYTE* pbData = (BYTE*)MessageData->Buffer;

        while (dwMax <= dwToSend)
        {
            memcpy(SndBuf + dwSize, pbData, dwMax);
            KdSend(SndBuf, EDBG_MAX_DATA_SIZE);
            dwToSend -= dwMax;
            pbData += dwMax;
            dwSize = 0;
            dwMax = EDBG_MAX_DATA_SIZE;
        }
        memcpy(SndBuf + dwSize, pbData, dwToSend);
        dwSize += dwToSend;
    }

    SndBuf[dwSize++] = PACKET_TRAILING_BYTE;        // append trailing byte
    KdSend(SndBuf, dwSize);
}
