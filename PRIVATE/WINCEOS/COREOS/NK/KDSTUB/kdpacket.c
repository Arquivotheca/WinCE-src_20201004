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
    DEBUGGERMSG(KDZONE_PACKET, (L"++KdRecv\r\n"));
    g_kdKernData.pINTERRUPTS_OFF();
    fRet = g_kdKernData.pKITLIoCtl (IOCTL_EDBG_RECV, pbuf, EDBG_SVC_KDBG, pdwLen, INFINITE, NULL);

    DEBUGGERMSG(KDZONE_PACKET, (L"--KdRecv: %d\r\n", fRet));
    return fRet;
}

static BOOL KdSend (BYTE * pbData, DWORD dwLen)
{
    BOOL fRet = TRUE;
    DEBUGGERMSG(KDZONE_PACKET, (L"++KdSend\r\n"));
    KD_ASSERT (dwLen <= EDBG_MAX_DATA_SIZE);

    g_kdKernData.pINTERRUPTS_OFF();
    fRet = g_kdKernData.pKITLIoCtl (IOCTL_EDBG_SEND, pbData, EDBG_SVC_KDBG, NULL, dwLen, NULL);
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
    IN BYTE* pbPacket,
    IN DWORD dwPacketSize,
    OUT STRING* MessageHeader,
    OUT STRING* MessageData,
    OUT DWORD* pdwDataLength,
    OUT GUID *pguidClient
)
{
    WORD wRet = KDP_PACKET_UNEXPECTED;

    KD_PACKET_HEADER *pPacketHeader = (KD_PACKET_HEADER *)pbPacket;
    DWORD dwToRead;
    BYTE *pbData = pbPacket + sizeof(KD_PACKET_HEADER);
    BYTE *pbDst;
    DWORD dwRemainingPacketSize = dwPacketSize;
    DWORD cbToCopy;

    DEBUGGERMSG(KDZONE_PACKET, (L"++DecodePacket: PacketSize=%d\r\n", dwPacketSize));
    
    KD_ASSERT(pguidClient);
    KD_ASSERT(MessageHeader);

    KD_ASSERT(KDBG_PACKET_VERSION == pPacketHeader->dwRevision);

    if (KDBG_PACKET_VERSION == pPacketHeader->dwRevision)
    {
        KD_ASSERT(PACKET_TYPE_KD_CMD == pPacketHeader->wPacketType || PACKET_TYPE_KD_CMD == pPacketHeader->wPacketType);
        
        if (PACKET_TYPE_KD_RESYNC == pPacketHeader->wPacketType)
        {
            wRet = KDP_PACKET_RESEND;
        }
        else if (PACKET_TYPE_KD_CMD == pPacketHeader->wPacketType)
        {
            if (pguidClient) *pguidClient = pPacketHeader->guidClient;

            dwRemainingPacketSize -= sizeof(KD_PACKET_HEADER);    // take away header
            KD_ASSERT((dwRemainingPacketSize > MessageHeader->MaximumLength)
                && (pPacketHeader->wDataByteCount <= (WORD)PACKET_DATA_MAX_SIZE)
                && (pPacketHeader->wDataByteCount >= (WORD)MessageHeader->MaximumLength));

            DEBUGGERMSG(KDZONE_PACKET, (L"  DecodePacket: Copying MessageHeader pbDst=0x%08x, pbSrc=0x%08x, bytes=%d.\r\n",
                            MessageHeader->Buffer, pbData, MessageHeader->MaximumLength));
            // copy header
            memcpy(MessageHeader->Buffer, pbData, MessageHeader->MaximumLength);

            // update pointers
            pbData += MessageHeader->MaximumLength;
            dwRemainingPacketSize -= MessageHeader->MaximumLength;
            MessageHeader->Length = (WORD)MessageHeader->MaximumLength;

            // the message data.
            pbDst = (BYTE *)MessageData->Buffer;
            *pdwDataLength = pPacketHeader->wDataByteCount - MessageHeader->MaximumLength;
            dwToRead = *pdwDataLength;
            DEBUGGERMSG(KDZONE_PACKET, (L"  DecodePacket: dwDataLength = %d - %d = %d\r\n", pPacketHeader->wDataByteCount, MessageHeader->MaximumLength, dwToRead));

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

WORD KdpReceiveCmdPacket
(
    OUT STRING* MessageHeader,
    OUT STRING* MessageData,
    OUT DWORD* pdwDataLength,
    OUT GUID *pguidClient
)
{
    static BYTE pbPacket[EDBG_MAX_DATA_SIZE];

    WORD wRet = KDP_PACKET_NONE;
    DWORD dwRead;
    KD_PACKET_HEADER *pPacketHeader = (KD_PACKET_HEADER *) pbPacket;
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
            DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceiveCmdPacket: Error - no data to receive!\n"));
            wRet = KDP_PACKET_RESEND;
            break;
        }

        KD_ASSERT(dwRead >= sizeof (KD_PACKET_HEADER));

        wRet = DecodePacket (pbPacket, dwRead, MessageHeader, MessageData, pdwDataLength, pguidClient);        
        DEBUGGERMSG(KDZONE_PACKET, (L"KdpReceiveCmdPacket: DecodePacket returned = 0x%04X\n",wRet));
        
        if (wRet == KDP_PACKET_UNEXPECTED)
        {
            DEBUGGERMSG (KDZONE_PACKET, (L"KdpReceiveCmdPacket: Unexpected Packet\n"));
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
            memcpy (SndBuf + dwSize, pbData, dwToSend);
            dwSize += dwToSend;
        }
        KdSend (SndBuf, dwSize);
    }
    DEBUGGERMSG(KDZONE_PACKET, (L"--KdpSendPacket\r\n"));
}
