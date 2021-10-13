//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// NDUMMY (NDIS DUMMY DRIVER)
//
// 
// Aug, 2003
//
// a dummy NDIS miniport driver
//

#include <ndis.h>
#include "ndummy.h"

#pragma hdrstop




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
IN NDIS_HANDLE hMiniportAdapterContext,      // The handle NDIS uses to refer to us
IN NDIS_HANDLE hWrapperConfiguration    // For use by NdisOpenConfiguration
)
{
    DEBUGMSG(ZONE_COMMENT, (L"==> NDUMMY.MiniportInitialize()"));
    //
    // Start off by retrieving our adapter context and storing
    // the Miniport handle in it.
    //
    NDUMMY_ADAPTER *pAdapter = NULL;
    NDIS_STATUS status = NdisAllocateMemoryWithTag((PVOID*)&pAdapter, sizeof(NDUMMY_ADAPTER), 'NDMY');
    if (status != NDIS_STATUS_SUCCESS) {
        DEBUGMSG(ZONE_ERROR, (L"NDUMMY::Initialize: Memory allocation failed"));
        return status;
    }
    NdisZeroMemory(pAdapter, sizeof(NDUMMY_ADAPTER));

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
        DEBUGMSG (ZONE_INIT, (L"Error!! ABORT: NDIS_STATUS_UNSUPPORTED_MEDIA..."));
        NdisFreeMemory(pAdapter, 0, 0);
        return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }
    *puiSelectedMediumIndex = uiMediumArraySize;
    //NdisMSetAttributesEx(hMiniportAdapterContext, pAdapter, 0, 0, NdisInterfaceIsa);
    NdisMSetAttributes(hMiniportAdapterContext, pAdapter, FALSE, NdisInterfaceIsa);

    DEBUGMSG(ZONE_COMMENT, (L"<== NDUMMY.MiniportInitialize()"));
    return NDIS_STATUS_SUCCESS;
}







UINT g_NdummySupportedOids[] = {
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
    { OID_GEN_LINK_SPEED, 10*10000 },   // 10M
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

UCHAR ucCurrentMacAddress[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };




NDIS_STATUS
MiniportQueryInformation
//
// NDIS calls to query for the value of the specified OID.
//
// Return:
//   NDIS_STATUS_SUCCESS when successful
//
(
IN NDIS_HANDLE hMiniportAdapterContext, // Pointer to the binding structure
IN NDIS_OID Oid,                    // The Oid for this query
IN PVOID pvInformationBuffer,       // Buffer for information
IN ULONG ulInformationBufferLength, // Size of this buffer
OUT PULONG pulBytesWritten,         // Specifies how much info is written
OUT PULONG pulBytesNeeded           // In case the buffer is smaller than what we need, tell them how much is needed
)
{
    DEBUGMSG(ZONE_OID, (L"NDUMMY.MiniportQueryInformation(0x%08X : %s)", Oid, OidString(Oid)));

    switch(Oid)
    {
    case OID_GEN_SUPPORTED_LIST:
        if(ulInformationBufferLength < sizeof(g_NdummySupportedOids))
        {
            DEBUGMSG(ZONE_ERROR, (L"MiniportQueryInformation() buffer too small"));
            break;
        }

        NdisMoveMemory(pvInformationBuffer, g_NdummySupportedOids, sizeof(g_NdummySupportedOids));
        if(pulBytesWritten)
            *pulBytesWritten = sizeof(g_NdummySupportedOids);
        if(pulBytesNeeded)
            *pulBytesNeeded = 0;
        return NDIS_STATUS_SUCCESS;
        break;

    case OID_802_3_CURRENT_ADDRESS:
        if(ulInformationBufferLength < sizeof(ucCurrentMacAddress))
        {
            DEBUGMSG(ZONE_ERROR, (L"MiniportQueryInformation() buffer too small"));
            break;
        }

        NdisMoveMemory(pvInformationBuffer, ucCurrentMacAddress, sizeof(ucCurrentMacAddress));
        if(pulBytesWritten)
            *pulBytesWritten = sizeof(ucCurrentMacAddress);
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
                    DEBUGMSG(ZONE_ERROR, (L"MiniportQueryInformation() buffer too small"));
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
        DEBUGMSG(ZONE_ERROR, (L"NDUMMY.MiniportQueryInformation(0x%08X : %s) not supported", Oid, OidString(Oid)));
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
IN NDIS_HANDLE hMiniportAdapterContext, // Pointer to the adapter structure
IN NDIS_OID Oid,                    // Oid for this query
IN PVOID pvInformationBuffer,       // Buffer for information
IN ULONG ulInformationBufferLength, // Size of this buffer
OUT PULONG pulBytesRead,            // Specifies how much info is read
OUT PULONG pulBytesNeeded           // In case the buffer is smaller than what we need, tell them how much is needed
)
{
   DEBUGMSG(ZONE_OID, (L"NDUMMY.MiniportSetInformation(0x%08X : %s)", Oid, OidString(Oid)));
   NDUMMY_ADAPTER* pAdapter = (NDUMMY_ADAPTER*)hMiniportAdapterContext;

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
IN NDIS_HANDLE hMiniportAdapterContext, // Pointer to the adapter
IN PNDIS_PACKET pPacket,    // Packet to send
IN UINT uiFlags             // Unused, passed down below
)
{
   NDUMMY_ADAPTER* pAdapter = (NDUMMY_ADAPTER*)hMiniportAdapterContext;
   DEBUGMSG(ZONE_SEND, (L"<== NDUMMY.MiniportSend()"));
   
   // we should send out this packet (through the hardware) at this point.
   // but we have no hardware...
   // I am simply ignore all send-out packets.

   DEBUGMSG(ZONE_SEND, (L"==> NDUMMY.MiniportSend()"));
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
IN NDIS_HANDLE hMiniportAdapterContext, // Pointer to the binding structure
IN NDIS_HANDLE hMiniportReceiveContext, // Context
IN UINT uiByteOffset,           // Offset into the packet for copying data
IN UINT uiBytesToTransfer       // How much to copy
)
{
   DEBUGMSG(ZONE_TRANSFER, (L">>> NDUMMY.MiniportTransferData(pPacket=%08X, uiByteOffset=%d, uiBytesToTransfer=%d)", pPacket, uiByteOffset, uiBytesToTransfer));
   NDUMMY_ADAPTER* pAdapter = (NDUMMY_ADAPTER*)hMiniportAdapterContext;

   // there should be no incoming packets
   // we don't have hardware...

   DEBUGMSG(ZONE_TRANSFER, (L"<<< NDUMMY.MiniportTransferData()"));
   return NDIS_STATUS_SUCCESS;
}





NDIS_STATUS
MiniportReset
//
// Reset Handler. We just don't do anything.
//
(
OUT PBOOLEAN  pbAddressingReset,    // To let NDIS know whether we need help with our reset
IN NDIS_HANDLE hMiniportAdapterContext  // Pointer to our binding
)
{
   DEBUGMSG(ZONE_COMMENT, (L"NDUMMY.MiniportReset()"));
   return NDIS_STATUS_SUCCESS;
}





VOID
MiniportHalt
//
// Halt handler. All the hard-work for clean-up is done here.
//
(
IN NDIS_HANDLE hMiniportAdapterContext  // Pointer to the binding structure
)
{
   DEBUGMSG(ZONE_COMMENT, (L"NDUMMY.MiniportHalt()"));
   NDUMMY_ADAPTER* pAdapter = (NDUMMY_ADAPTER*)hMiniportAdapterContext;

   // Free instance structure
   NdisFreeMemory(pAdapter, 0, 0);
}
