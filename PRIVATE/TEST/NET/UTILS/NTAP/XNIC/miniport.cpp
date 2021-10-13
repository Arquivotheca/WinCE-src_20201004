//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// XNIC (Experimental Network Interface Card)
//

//
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
/*++


Module Name:
   miniport.cpp

Abstract:
   NDIS 4.0 driver




Environment:
   Windows CE

Revision History:
   2003-12-23 first version
 
--*/

//------------------------------------------------------------------------------

#include <ndis.h>
#include <msgqueue.h>
#include "xnic.h"

#pragma hdrstop


// 0050 = Microsoft MAC ID
XNIC_NDIS_MAC_ADDRESS g_ucDefaultMacAddress = { 0x00, 0x50, 0x33, 0x44, 0x55, 0x66 };

#define TOHEX(c)  ((c)<='9'?(c)-'0':(c)<='F'?(c)-'A'+0xA:(c)-'a'+0xA)



NDIS_STATUS
MiniportInitialize
//
// This is the initialize handler which gets called as a result of
// the BindAdapter handler calling NdisIMInitializeDeviceInstanceEx.
// The context parameter which we pass there is the adapter structure
// which we retrieve here.
//
// Return
//   NDIS_STATUS_SUCCESS unless something goes wrong
//
(
OUT PNDIS_STATUS pOpenStatus,   // Not used by us
OUT PUINT puiSelectedMediumIndex,   // Place-holder for what media we are using
IN PNDIS_MEDIUM aMediumArray,   // The array of ndis media passed down to us
IN UINT uiMediumArraySize,      // Size of the array
IN NDIS_HANDLE hMiniportAdapterHandle,      // The handle NDIS uses to refer to us
IN NDIS_HANDLE hWrapperConfiguration    // For use by NdisOpenConfiguration
)
{
    DEBUGMSG(ZONE_INIT, (L"==> XNIC.MiniportInitialize()"));

    for (; uiMediumArraySize > 0; uiMediumArraySize--)
    {
        if (aMediumArray[uiMediumArraySize - 1] == NdisMedium802_3)
        {
            uiMediumArraySize--;
            break;
        }
    }
    if (aMediumArray[uiMediumArraySize] != NdisMedium802_3)
    {
        DEBUGMSG (ZONE_INIT, (L"XNIC.Error!! ABORT: NDIS_STATUS_UNSUPPORTED_MEDIA..."));
        return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }
    *puiSelectedMediumIndex = uiMediumArraySize;

    //
    // Start off by retrieving our adapter context and storing
    // the Miniport handle in it.
    //
    XNIC_ADAPTER *pAdapter = NULL;
    NDIS_STATUS status = NdisAllocateMemoryWithTag((PVOID*)&pAdapter, sizeof(XNIC_ADAPTER), 'XNIC');
    if (status != NDIS_STATUS_SUCCESS) {
        DEBUGMSG(ZONE_ERROR, (L"XNIC.Initialize: Memory allocation failed"));
        return status;
    }
    NdisZeroMemory(pAdapter, sizeof(XNIC_ADAPTER));

    // default values
    pAdapter->hMiniportAdapterHandle = hMiniportAdapterHandle;
    pAdapter->dwOutgoingPacketId = 0;
    pAdapter->hMsgQ_OutgoingPackets = NULL;

    // generate random MAC address
    pAdapter->dwTimeMark = GetTickCount();
    NdisMoveMemory(pAdapter->MacAddress, g_ucDefaultMacAddress, sizeof(XNIC_NDIS_MAC_ADDRESS));
    NdisMoveMemory(pAdapter->MacAddress+2, &pAdapter->dwTimeMark, sizeof(DWORD));

    WCHAR szMacAddress1[] = L"00:00:00:00:00:00..";
    PNDIS_MINIPORT_BLOCK pM1 = (PNDIS_MINIPORT_BLOCK)hMiniportAdapterHandle;
    WCHAR szMsgQOutgoingPackets[MAX_PATH] = L"X-OUT-";
    wcscat(szMsgQOutgoingPackets, pM1->MiniportName.Buffer);
    for(WCHAR*p=szMsgQOutgoingPackets; *p; p++)   // queue name should not have '\'
    {
        if(*p == L'\\')
            *p = L'-';
    }
    WCHAR szEvent_QueueOutgoingPackets[MAX_PATH];
    wcscpy(szEvent_QueueOutgoingPackets, szMsgQOutgoingPackets);
    wcscat(szEvent_QueueOutgoingPackets, L"-START");

    NDIS_HANDLE hConfigurationHandle;
    NdisOpenConfiguration(&status, &hConfigurationHandle, hWrapperConfiguration);
    if(NDIS_STATUS_SUCCESS == status)
    {
        PNDIS_CONFIGURATION_PARAMETER pConfigParamValue;
        NDIS_CONFIGURATION_PARAMETER parm1;

        // Mac address
        NDIS_STRING nszMacAddressValueName = NDIS_STRING_CONST("X-MacAddress");
        NdisReadConfiguration(&status, &pConfigParamValue, hConfigurationHandle, &nszMacAddressValueName, NdisParameterString);
        if((NDIS_STATUS_SUCCESS == status) && (pConfigParamValue->ParameterType == NdisParameterString))
        {
            WCHAR *sz1 = pConfigParamValue->ParameterData.StringData.Buffer;
            wcscpy(szMacAddress1, sz1);
            for(unsigned i=0; i<sizeof(XNIC_NDIS_MAC_ADDRESS); i++)
                pAdapter->MacAddress[i] = (TOHEX(sz1[i*3])<<4) + TOHEX(sz1[i*3+1]);
        }
        else
        {   // save 
            wsprintf(szMacAddress1, L"%02X:%02X:%02X:%02X:%02X:%02X", 
                            pAdapter->MacAddress[0], pAdapter->MacAddress[1], pAdapter->MacAddress[2],
                            pAdapter->MacAddress[3], pAdapter->MacAddress[4], pAdapter->MacAddress[5]);
            NDIS_STRING nsz1;
            NdisInitUnicodeString(&nsz1, szMacAddress1);
            parm1.ParameterType = NdisParameterString;
            parm1.ParameterData.StringData = nsz1;
            NdisWriteConfiguration(&status, hConfigurationHandle, &nszMacAddressValueName, &parm1);
        }

        // MSG QUEUE
        NDIS_STRING nszMsgQOutgoingPacketsValueName = NDIS_STRING_CONST("X-MSGQ-OutgoingPackets");
        NdisReadConfiguration(&status, &pConfigParamValue, hConfigurationHandle, &nszMsgQOutgoingPacketsValueName, NdisParameterString);
        if((NDIS_STATUS_SUCCESS == status) && (pConfigParamValue->ParameterType == NdisParameterString))
            wcscpy(szMsgQOutgoingPackets, pConfigParamValue->ParameterData.StringData.Buffer);
        else
        {
            NDIS_STRING nsz1;
            NdisInitUnicodeString(&nsz1, szMsgQOutgoingPackets);
            parm1.ParameterType = NdisParameterString;
            parm1.ParameterData.StringData = nsz1;
            NdisWriteConfiguration(&status, hConfigurationHandle, &nszMsgQOutgoingPacketsValueName, &parm1);
        }

        // event that turns queueing on and off
        NDIS_STRING nszEventOutgoingPacketsValueName = NDIS_STRING_CONST("X-EVENT-OutgoingPackets");
        NdisReadConfiguration(&status, &pConfigParamValue, hConfigurationHandle, &nszEventOutgoingPacketsValueName, NdisParameterString);
        if((NDIS_STATUS_SUCCESS == status) && (pConfigParamValue->ParameterType == NdisParameterString))
            wcscpy(szEvent_QueueOutgoingPackets, pConfigParamValue->ParameterData.StringData.Buffer);
        else
        {
            NDIS_STRING nsz1;
            NdisInitUnicodeString(&nsz1, szEvent_QueueOutgoingPackets);
            parm1.ParameterType = NdisParameterString;
            parm1.ParameterData.StringData = nsz1;
            NdisWriteConfiguration(&status, hConfigurationHandle, &nszEventOutgoingPacketsValueName, &parm1);
        }

        NdisCloseConfiguration(hConfigurationHandle);
    }
    DEBUGMSG(ZONE_INIT, (L"XNIC.MacAddress = %02X:%02X:%02X:%02X:%02X:%02X",
        pAdapter->MacAddress[0], pAdapter->MacAddress[1], pAdapter->MacAddress[2],
        pAdapter->MacAddress[3], pAdapter->MacAddress[4], pAdapter->MacAddress[5]));

    pAdapter->hEvent_QueueOutgoingPackets = CreateEvent(NULL, TRUE, FALSE, szEvent_QueueOutgoingPackets);
    if(pAdapter->hEvent_QueueOutgoingPackets == INVALID_HANDLE_VALUE)
        DEBUGMSG(ZONE_ERROR, (L"XNIC.hEvent_QueueOutgoingPackets == INVALID_HANDLE_VALUE"));

    // create a message queue, queue name format = L"X-OUT-PCI-XNIC1"
    MSGQUEUEOPTIONS mqo;
    memset (&mqo, 0, sizeof (MSGQUEUEOPTIONS));
    mqo.dwSize = sizeof(MSGQUEUEOPTIONS);
    mqo.dwFlags = 0;
    mqo.dwMaxMessages = 8;
    mqo.cbMaxMessage = 2048*8;
    mqo.bReadAccess = FALSE;
    pAdapter->hMsgQ_OutgoingPackets = CreateMsgQueue(szMsgQOutgoingPackets, &mqo);
    if(NULL == pAdapter->hMsgQ_OutgoingPackets)
        DEBUGMSG(ZONE_WARNING, (L"XNIC.Initialize: CreateMsgQueue failed (packets will not be sent to the queue)"));
    DEBUGMSG(ZONE_COMMENT, (L"XNIC.MSGQ-OutgoingPackets = %s", szMsgQOutgoingPackets));
    NdisMoveMemory(pAdapter->ucbRawPacketData+8, pAdapter->szAdapterName, 16*sizeof(WCHAR));    // do once
    DEBUGMSG(ZONE_COMMENT, (L"XNIC.EVENT-OutgoingPackets = %s", szEvent_QueueOutgoingPackets));

    NdisMSetAttributes(hMiniportAdapterHandle, pAdapter, FALSE, NdisInterfaceIsa);

    DEBUGMSG(ZONE_INIT, (L"<== XNIC.MiniportInitialize()"));
    return NDIS_STATUS_SUCCESS;
}







UINT g_XNICSupportedOids[] = {
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_LINK_SPEED,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_RCV_OK,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_VENDOR_ID,
    OID_GEN_XMIT_ERROR,
    OID_GEN_XMIT_OK,
    };




typedef struct tagDataMiniportQueryInformation_ULONG
{
    NDIS_OID Oid;
    ULONG ulValue;
} MINIPORT_QUERY_INFORMATION_ULONG, *PMINIPORT_QUERY_INFORMATION_ULONG;

#define ETH_FRAME_SIZE  1514
#define TX_BUF_SIZE     2048


MINIPORT_QUERY_INFORMATION_ULONG g_MiniportQueryInformation_ULONG[] =
{

//OID_802_3_CURRENT_ADDRESS
    { OID_802_3_MAXIMUM_LIST_SIZE, 0x00000008 },
    { OID_GEN_DIRECTED_BYTES_RCV, 0x00000000 },
    { OID_GEN_DIRECTED_FRAMES_RCV, 0x00000000 },
    { OID_GEN_INIT_TIME_MS, 0x00000000 },
    { OID_GEN_LINK_SPEED, 10*10000 },   // 10Mbps
    { OID_GEN_MAC_OPTIONS,  NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                            NDIS_MAC_OPTION_RECEIVE_SERIALIZED  |
                            NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                            NDIS_MAC_OPTION_NO_LOOPBACK },
    { OID_GEN_MAXIMUM_FRAME_SIZE, 1500 },
    { OID_GEN_MAXIMUM_LOOKAHEAD, 0x000000EE },
    { OID_GEN_MEDIA_CAPABILITIES, 0x00000001 },
    { OID_GEN_MEDIA_CONNECT_STATUS, NdisMediaStateConnected },
    { OID_GEN_MEDIA_SENSE_COUNTS, 0x00000000 },
    { OID_GEN_PHYSICAL_MEDIUM, 0x00000000 },
    { OID_GEN_RCV_ERROR, 0x00000000 },
    { OID_GEN_RCV_OK, 0x00000000 },
    { OID_GEN_RESET_COUNTS, 0x00000000 },
    { OID_GEN_VENDOR_DRIVER_VERSION, 0x00010000 },
    { OID_GEN_XMIT_ERROR, 0x00000000 },
    { OID_GEN_XMIT_OK, 0x00000000 },
    { OID_PNP_CAPABILITIES, 0x00000000 },
    { OID_TCP_TASK_OFFLOAD, 0x00000001 },
    { 0x00FFFFFF, 0x00000000 }, //OID_unknown

    { OID_GEN_HARDWARE_STATUS, 0x00000000 },
    { OID_GEN_MAXIMUM_SEND_PACKETS, 0x00000001 },
    { OID_GEN_MAXIMUM_TOTAL_SIZE, ETH_FRAME_SIZE },
    { OID_GEN_TRANSMIT_BUFFER_SPACE, TX_BUF_SIZE * 2 },
    { OID_GEN_MEDIA_SUPPORTED, NdisMedium802_3 },
    { OID_GEN_MEDIA_IN_USE, NdisMedium802_3 },
    { OID_802_3_XMIT_MORE_COLLISIONS, 0x00000000 },
    { OID_802_3_XMIT_ONE_COLLISION, 0x00000000 },
    { (ULONG)-1, 0x00000000 },
};





NDIS_STATUS
MiniportQueryInformation
//
// NDIS calls to query for the value of the specified OID.
//
// Return:
//   NDIS_STATUS_SUCCESS when successful
//
(
IN XNIC_ADAPTER* pAdapter,			// Pointer to the binding structure
IN NDIS_OID Oid,                    // The Oid for this query
IN PVOID pvInformationBuffer,       // Buffer for information
IN ULONG ulInformationBufferLength, // Size of this buffer
OUT PULONG pulBytesWritten,         // Specifies how much info is written
OUT PULONG pulBytesNeeded           // In case the buffer is smaller than what we need, tell them how much is needed
)
{
    DEBUGMSG(ZONE_OID, (L"XNIC.MiniportQueryInformation(0x%08X : %s)", Oid, OidString(Oid)));

    switch(Oid)
    {
    case OID_GEN_SUPPORTED_LIST:
        if(ulInformationBufferLength < sizeof(g_XNICSupportedOids))
        {
            DEBUGMSG(ZONE_ERROR, (L"XNIC.MiniportQueryInformation() buffer too small"));
            break;
        }

        NdisMoveMemory(pvInformationBuffer, g_XNICSupportedOids, sizeof(g_XNICSupportedOids));
        if(pulBytesWritten)
            *pulBytesWritten = sizeof(g_XNICSupportedOids);
        if(pulBytesNeeded)
            *pulBytesNeeded = 0;
        return NDIS_STATUS_SUCCESS;
        break;

    case OID_802_3_CURRENT_ADDRESS:
        if(ulInformationBufferLength < sizeof(XNIC_NDIS_MAC_ADDRESS))
        {
            DEBUGMSG(ZONE_ERROR, (L"XNIC.MiniportQueryInformation() buffer too small"));
            break;
        }

        NdisMoveMemory(pvInformationBuffer, pAdapter->MacAddress, sizeof(XNIC_NDIS_MAC_ADDRESS));
        if(pulBytesWritten)
            *pulBytesWritten = sizeof(XNIC_NDIS_MAC_ADDRESS);
        if(pulBytesNeeded)
            *pulBytesNeeded = 0;
        return NDIS_STATUS_SUCCESS;
        break;

    default:
        for(PMINIPORT_QUERY_INFORMATION_ULONG p=g_MiniportQueryInformation_ULONG; p->Oid != (ULONG)(-1); p++)
        {
            if(p->Oid == Oid)
            {
                if(ulInformationBufferLength < sizeof(ULONG))
                {
                    DEBUGMSG(ZONE_ERROR, (L"XNIC.MiniportQueryInformation() buffer too small"));
                    break;
                }

                NdisMoveMemory(pvInformationBuffer, &(p->ulValue), sizeof(ULONG));
                if(pulBytesWritten)
                    *pulBytesWritten = sizeof(ULONG);
                if(pulBytesNeeded)
                    *pulBytesNeeded = 0;
                return NDIS_STATUS_SUCCESS;
            }
        }
        DEBUGMSG(ZONE_ERROR, (L"XNIC.MiniportQueryInformation(0x%08X : %s) not supported", Oid, OidString(Oid)));
    }

    if(pulBytesWritten)
        *pulBytesWritten = 0;
    if(pulBytesNeeded)
        *pulBytesNeeded = 0;
    return NDIS_STATUS_NOT_SUPPORTED;
}





NDIS_STATUS
MiniportSetInformation
//
// NDIS calls to set the value of the specified OID.
//
// Return:
//   NDIS_STATUS_SUCCESS if successful
//
(
IN XNIC_ADAPTER* pAdapter,			// Pointer to the adapter structure
IN NDIS_OID Oid,                    // Oid for this query
IN PVOID pvInformationBuffer,       // Buffer for information
IN ULONG ulInformationBufferLength, // Size of this buffer
OUT PULONG pulBytesRead,            // Specifies how much info is read
OUT PULONG pulBytesNeeded           // In case the buffer is smaller than what we need, tell them how much is needed
)
{
    DEBUGMSG(ZONE_OID, (L"XNIC.MiniportSetInformation(0x%08X : %s)", Oid, OidString(Oid)));

    // I simply says "everything is set successfully"
    if(pulBytesRead)
        *pulBytesRead = ulInformationBufferLength;
    if(pulBytesNeeded)
        *pulBytesNeeded = 0;
    return NDIS_STATUS_SUCCESS;
}





NDIS_STATUS
MiniportSend
//
// Send Packet handler.
//
// Return:
//   NDIS_STATUS code returned from NdisSend
//
(
IN XNIC_ADAPTER* pAdapter,	// Pointer to the adapter
IN PNDIS_PACKET pPacket,    // Packet to send
IN UINT uiFlags             // Unused, passed down below
)
{
    DEBUGMSG(ZONE_SEND, (L"<== XNIC.MiniportSend()"));

    if(WaitForSingleObject(pAdapter->hEvent_QueueOutgoingPackets, 0) != WAIT_OBJECT_0)
    {
        DEBUGMSG(ZONE_SEND, (L"==> XNIC.MiniportSend()"));
        return NDIS_STATUS_SUCCESS;
    }

    // we should send out this packet (through the hardware) at this point.
    // but we have no hardware...
    // I am simply put packet data to a message queue

    // content of the pAdapter->ucbRawPacketData
    /*
       +---------+
       |4 bytes  | DWORD dwOutgoingPacketId
       +---------+
       |4 bytes  | DWORD dwTimeMark
       +---------+
       |32 bytes | WCHAR szAdapterName[upto 16 WCHAR]
       |         |
       |         |
       +---------+
       |n bytes  | BYTE *pPacketData1 (packet data [max 1514])
       |         |
       |         |
       |         |
       +---------+
    */

    NdisMoveMemory(pAdapter->ucbRawPacketData, &pAdapter->dwOutgoingPacketId, sizeof(DWORD));
    pAdapter->dwTimeMark = GetTickCount();
    NdisMoveMemory(pAdapter->ucbRawPacketData+4, &pAdapter->dwTimeMark, sizeof(DWORD));
//    NdisMoveMemory(pAdapter->ucbRawPacketData+8, pAdapter->szAdapterName, 16*sizeof(WCHAR));    // already done

    PBYTE  pucbPacketData1 = pAdapter->ucbRawPacketData + 4 + 4 + 32;
	DWORD dwPacketDataLength = 0;
	PNDIS_BUFFER pPacketBuffer;

    // assemble complete packet data from linked-list of buffers
    NdisQueryPacket(pPacket, NULL, NULL, &pPacketBuffer, NULL);
    while (pPacketBuffer)
    {
    	PVOID pVirtualAddressOfBuffer;
    	UINT  uiBufferLength;
        NdisQueryBuffer(pPacketBuffer, &pVirtualAddressOfBuffer, &uiBufferLength);
        if ((dwPacketDataLength + uiBufferLength + (4 + 4 + 32)) > sizeof(pAdapter->ucbRawPacketData))
        {
            DEBUGMSG(ZONE_ERROR, (L"XNIC.NtapQueuePacketData(too large)"));
            break;
        }
        NdisMoveMemory(pucbPacketData1+dwPacketDataLength, pVirtualAddressOfBuffer, uiBufferLength);
        dwPacketDataLength += uiBufferLength;
        NdisGetNextBuffer(pPacketBuffer, &pPacketBuffer);
    }
    DEBUGMSG(ZONE_SEND, (L"XNIC.packet %d, length=%d", pAdapter->dwOutgoingPacketId, dwPacketDataLength));
    dwPacketDataLength += 4 + 4 + 32;

    if(!WriteMsgQueue(pAdapter->hMsgQ_OutgoingPackets, pAdapter->ucbRawPacketData, dwPacketDataLength, 0, 0))
        DEBUGMSG(ZONE_WARNING, (L"XNIC.WriteMsgQueue() error: queue full"));
    pAdapter->dwOutgoingPacketId++;
    DEBUGMSG(ZONE_SEND, (L"==> XNIC.MiniportSend()"));
    return NDIS_STATUS_SUCCESS;
}






NDIS_STATUS
MiniportTransferData
//
// Miniport's transfer data handler.
// NDIS calls when we indicate NDIS some packets.
//
// Return:
//   status of transfer
//
(
OUT PNDIS_PACKET pPacket,       // Destination packet
OUT PUINT puiBytesTransferred,  // Place holder for how much data was copied
IN XNIC_ADAPTER* pAdapter,		// Pointer to the binding structure
IN NDIS_HANDLE hMiniportReceiveContext, // Context
IN UINT uiByteOffset,           // Offset into the packet for copying data
IN UINT uiBytesToTransfer       // How much to copy
)
{
    DEBUGMSG(ZONE_TRANSFER, (L">>> XNIC.MiniportTransferData(pPacket=%08X, uiByteOffset=%d, uiBytesToTransfer=%d)", pPacket, uiByteOffset, uiBytesToTransfer));

    // there should be no incoming packets
    // we don't have hardware...

    DEBUGMSG(ZONE_TRANSFER, (L"<<< XNIC.MiniportTransferData()"));
    return NDIS_STATUS_SUCCESS;
}





NDIS_STATUS
MiniportReset
//
// Reset Handler. We just don't do anything.
//
(
OUT PBOOLEAN  pbAddressingReset,    // To let NDIS know whether we need help with our reset
IN XNIC_ADAPTER* pAdapter			// Pointer to our binding
)
{
    DEBUGMSG(ZONE_COMMENT, (L"XNIC.MiniportReset()"));
    return NDIS_STATUS_SUCCESS;
}





VOID
MiniportHalt
//
// Halt handler. All the hard-work for clean-up is done here.
//
(
IN XNIC_ADAPTER* pAdapter		// Pointer to the binding structure
)
{
    DEBUGMSG(ZONE_COMMENT, (L"XNIC.MiniportHalt()"));

    CloseMsgQueue(pAdapter->hMsgQ_OutgoingPackets);
    CloseHandle(pAdapter->hEvent_QueueOutgoingPackets);
    NdisFreeMemory(pAdapter, 0, 0);
}
