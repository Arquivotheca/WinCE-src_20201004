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
    DEBUGGERMSG(KDZONE_PACKET, (L"++KdRecv\r\n"));
    fRet = KITLIoctl(IOCTL_EDBG_RECV, pbuf, EDBG_SVC_KDBG, pdwLen, INFINITE, NULL);
    DEBUGGERMSG(KDZONE_PACKET, (L"--KdRecv: %d\r\n", fRet));
    return fRet;
}

static BOOL KdSend (BYTE * pbData, DWORD dwLen)
{
    BOOL fRet = TRUE;
    DEBUGGERMSG(KDZONE_PACKET, (L"++KdSend\r\n"));
    KD_ASSERT (dwLen <= EDBG_MAX_DATA_SIZE);
    fRet = KITLIoctl(IOCTL_EDBG_SEND, pbData, EDBG_SVC_KDBG, NULL, dwLen, NULL);
    DEBUGGERMSG(KDZONE_PACKET, (L"--KdSend: %d\r\n", fRet));
    return fRet;
}


/*++

Routine Description:


Arguments:

    pbPacket - buffer containing the complete KD packet
    dwPacketSize - Size in byte of the packet
    dwPacketType - Supplies the type of packet that is excepted.
    MessageHeader - Supplies a pointer to a string descriptor for the input
        message.
    MessageData - Supplies a pointer to a string descriptor for the input data.
    pdwDataLength - Supplies pointer to DWORD to receive length of recv. data.

Return Value:

    KDP_PACKET_RECEIVED - if packet received.
    KDP_PACKET_UNEXPECTED - if a non-data packet was rec'd during processing

--*/

WORD DecodePacket
(
    BYTE* pbPacket,
    DWORD dwPacketSize,
    STRING* MessageData,
    DWORD* pdwDataLength,
    GUID *pguidClient
)
{
    WORD wRet = KDP_PACKET_UNEXPECTED;

    KD_PACKET_HEADER *pPacketHeader;
    DWORD dwToRead;
    BYTE *pbData;
    BYTE *pbDst;
    DWORD dwRemainingPacketSize;
    DWORD cbToCopy;

    DEBUGGERMSG(KDZONE_PACKET, (L"++DecodePacket: PacketSize=%d\r\n", dwPacketSize));

    pPacketHeader = (KD_PACKET_HEADER *)pbPacket;
    pbData = pbPacket + sizeof(KD_PACKET_HEADER);
    dwRemainingPacketSize = dwPacketSize - sizeof(KD_PACKET_HEADER);

    KD_ASSERT(KDBG_PACKET_VERSION == pPacketHeader->dwRevision);
    if (KDBG_PACKET_VERSION == pPacketHeader->dwRevision)
    {
        KD_ASSERT(PACKET_TYPE_KD_CMD == pPacketHeader->wPacketType || PACKET_TYPE_KD_RESYNC == pPacketHeader->wPacketType);
        
        if (PACKET_TYPE_KD_RESYNC == pPacketHeader->wPacketType)
        {
            wRet = KDP_PACKET_RESEND;
        }
        else if (PACKET_TYPE_KD_CMD == pPacketHeader->wPacketType)
        {
            *pguidClient = pPacketHeader->guidClient;

            // the message data.
            pbDst = (BYTE *)MessageData->Buffer;
            *pdwDataLength = pPacketHeader->wDataByteCount;
            dwToRead = *pdwDataLength;
            DEBUGGERMSG(KDZONE_PACKET, (L"  DecodePacket: dwDataLength = %d\r\n", pPacketHeader->wDataByteCount));

            cbToCopy = (dwRemainingPacketSize < *pdwDataLength)? dwRemainingPacketSize : *pdwDataLength;
            DEBUGGERMSG(KDZONE_PACKET, (L"  DecodePacket: Copying MessageData. pbDst=0x%08x, pbSrc=0x%08x, bytes=%d\r\n",
                            pbDst, pbData, cbToCopy));
            memcpy (pbDst, pbData, cbToCopy);

            DEBUGGERMSG(KDZONE_PACKET, (L"  DecodePacket: Done.\r\n"));

            // if the packet is bigger than the MTU, we need to receive
            // the rest of the packet
            pbDst += dwRemainingPacketSize;
            dwToRead -= dwRemainingPacketSize;

            while (dwToRead > dwRemainingPacketSize)
            {
                DWORD dwRead = (dwToRead < EDBG_MAX_DATA_SIZE) ? dwToRead : EDBG_MAX_DATA_SIZE;

                DEBUGGERMSG(KDZONE_PACKET, (L"  DecodePacket: Reading extra bytes: %d\r\n", dwRead));
                if (!KdRecv (pbPacket, &dwRead))
                {
                    KD_ASSERT(0); // shouldn't happen
                }

                memcpy(pbDst, pbPacket, dwRead);

                pbDst += dwRead;
                dwToRead -= dwRead;
            }

            MessageData->Length = (WORD)*pdwDataLength;

            wRet = KDP_PACKET_RECEIVED;
        }
    }
    DEBUGGERMSG(KDZONE_PACKET, (L"--DecodePacket: %d\r\n", wRet));
    return wRet;
}


/*++

Routine Description:

    This routine receives a packet from the host machine that is running
    the kernel debugger UI.  This routine is ALWAYS called after packet being
    sent by caller.  It first waits for ACK packet for the packet sent and
    then waits for the packet desired.

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

WORD KdpReceivePacket
(
    STRING* MessageData,
    DWORD* pdwDataLength,
    GUID *pguidClient
)
{
    static BYTE pbPacket[EDBG_MAX_DATA_SIZE];

    WORD wRet = KDP_PACKET_NONE;
    DWORD dwRead;
    BOOL fAlreadyReadPacket = FALSE;

    DEBUGGERMSG(KDZONE_PACKET, (L"++KdpReceiveCmdPacket\r\n"));

    do
    {
        dwRead = EDBG_MAX_DATA_SIZE;

        if (fAlreadyReadPacket)
        {
            dwRead = *pdwDataLength;
            fAlreadyReadPacket = FALSE;
        }
        else if (!KdRecv (pbPacket, &dwRead))
        {
            DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceiveCmdPacket: Error - no data to receive!\r\n"));
            wRet = KDP_PACKET_RESEND;
            break;
        }

        KDASSERT(dwRead >= sizeof (KD_PACKET_HEADER));
        wRet = DecodePacket (pbPacket, dwRead, MessageData, pdwDataLength, pguidClient);
        DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceiveCmdPacket: DecodePacket returned = 0x%04X\r\n",wRet));
        
        if (wRet == KDP_PACKET_UNEXPECTED)
        {
            DEBUGGERMSG (KDZONE_PACKET, (L"KdpReceiveCmdPacket: Unexpected Packet\r\n"));
            // A non-data packet was received during the processing of a multi-packet message
            // It's sitting in the receive buffer waiting to be processed, *pdwDataLength contains
            // the size
            wRet = KDP_PACKET_NONE;
            fAlreadyReadPacket = TRUE;
        }
    }
    while (wRet == KDP_PACKET_NONE);

    DEBUGGERMSG(KDZONE_PACKET, (L"--KdpReceiveCmdPacket: wRet = 0x%04X\r\n", wRet));

    return wRet;
}


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

void KdpSendPacket
(
    IN WORD dwPacketType,
    IN GUID guidClient,
    IN STRING * MessageHeader,
    IN OPTIONAL STRING * MessageData
)
{
    DEBUGGERMSG(KDZONE_PACKET, (L"++KdpSendPacket (type=%i)\r\n", dwPacketType));
    if (MessageHeader)
    {
        static BYTE SndBuf[EDBG_MAX_DATA_SIZE];
        KD_PACKET_HEADER *pPacketHeader = (KD_PACKET_HEADER *)SndBuf;
        DWORD MessageDataLength = 0;
        DWORD dwSize;

        if (dwPacketType == PACKET_TYPE_KD_CMD)
        { // Debug command request response packet
            // Notify the host that target memory has changed if PB is capable of
            // handling the request
            if (g_fDbgKdStateMemoryChanged)
            { // Mem refresh supported and occuring
                ((DBGKD_COMMAND*) (MessageHeader->Buffer))->dwKdpFlags |= DBGKD_STATE_MEMORY_CHANGED;
                g_fDbgKdStateMemoryChanged = FALSE; // Reset the signal
            }
        }

        if (MessageData)
        {
            MessageDataLength = MessageData->Length;
        }

        // Initialize and send the packet header.
        pPacketHeader->wDataByteCount = (WORD) (MessageHeader->Length + MessageDataLength);
        pPacketHeader->wPacketType    = (WORD) dwPacketType;
        pPacketHeader->dwRevision     = KDBG_PACKET_VERSION;
        pPacketHeader->guidClient     = guidClient;

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

            if (dwToSend > 0)
            {
                // Just in case (total_data_length mod EDBG_MAX_DATA_SIZE) <> 0
                memcpy (SndBuf + dwSize, pbData, dwToSend);
                dwSize += dwToSend;
            }
        }
        if (dwSize > 0)
        {
            // Just in case some data remains to be sent.
            KdSend (SndBuf, dwSize);
        }
    }
    DEBUGGERMSG(KDZONE_PACKET, (L"--KdpSendPacket\r\n"));
}
