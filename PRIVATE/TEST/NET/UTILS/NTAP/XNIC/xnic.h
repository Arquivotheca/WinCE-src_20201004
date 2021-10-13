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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


Module Name:
   XNIC.h

Abstract:

Author:


Environment:
   Windows CE

Revision History:
   2003-12-23 first version
 
--*/

//------------------------------------------------------------------------------


#ifndef __XNIC_H__
#define __XNIC_H__


typedef UCHAR   XNIC_NDIS_MAC_ADDRESS[6];


typedef struct _tagXNIC_ADAPTER
{
    NDIS_HANDLE hMiniportAdapterHandle;
    XNIC_NDIS_MAC_ADDRESS MacAddress;
    DWORD dwOutgoingPacketId;
    DWORD dwTimeMark;
    WCHAR szAdapterName[MAX_PATH];
    HANDLE hMsgQ_OutgoingPackets;
    HANDLE hEvent_QueueOutgoingPackets;
    BYTE ucbRawPacketData[2048];
} XNIC_ADAPTER, *PXNIC_ADAPTER;





#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
DriverEntry(
   IN PDRIVER_OBJECT pDriverObject,
   IN PUNICODE_STRING psRegistryPath
);

void
DriverUnload(
   IN PDRIVER_OBJECT pDriverObject
);




//------------------------------------------------------------------------------
//
// Miniport prototypes
//

NDIS_STATUS
MiniportInitialize(
   OUT PNDIS_STATUS pOpenStatus,
   OUT PUINT puiSelectedMediumIndex,
   IN PNDIS_MEDIUM aMediumArray,
   IN UINT uiMediumArraySize,
   IN NDIS_HANDLE hMiniportAdapterHandle,
   IN NDIS_HANDLE hWrapperConfigurationContext
);


NDIS_STATUS
MiniportQueryInformation(
   IN XNIC_ADAPTER* pAdapter,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesWritten,
   OUT PULONG pulBytesNeeded
);


NDIS_STATUS
MiniportSetInformation(
   IN XNIC_ADAPTER* pAdapter,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesRead,
   OUT PULONG pulBytesNeeded
);


NDIS_STATUS
MiniportSend(
   IN XNIC_ADAPTER* pAdapter,
   IN PNDIS_PACKET pPacket,
   IN UINT uiFlags
);


NDIS_STATUS
MiniportTransferData(
   OUT PNDIS_PACKET pPacket,
   OUT PUINT puiBytesTransferred,
   IN XNIC_ADAPTER* pAdapter,
   IN NDIS_HANDLE hMiniportReceiveContext,
   IN UINT uiByteOffset,
   IN UINT uiBytesToTransfer
);

void
MiniportHalt(
   IN XNIC_ADAPTER* pAdapter
);

NDIS_STATUS
MiniportReset(
   OUT PBOOLEAN pbAddressingReset,
   IN XNIC_ADAPTER* pAdapter
);


#ifdef __cplusplus
}   // extern "C"
#endif



//------------------------------------------------------------------------------




#ifdef DEBUG

#define ZONE_INIT       DEBUGZONE(0)
#define ZONE_OID        DEBUGZONE(1)
#define ZONE_SEND       DEBUGZONE(2)
#define ZONE_TRANSFER   DEBUGZONE(3)
// ...
#define ZONE_COMMENT    DEBUGZONE(13)
#define ZONE_WARNING    DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)

extern DBGPARAM dpCurSettings;

#endif

extern WCHAR *OidString(IN NDIS_OID Oid);


#endif // __XNIC_H__
