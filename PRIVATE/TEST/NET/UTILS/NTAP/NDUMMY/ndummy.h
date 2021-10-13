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
   NDUMMY.h

Abstract:

Author: 


Environment:
   Windows CE

Revision History:
   None.
 
--*/

//------------------------------------------------------------------------------


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
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesWritten,
   OUT PULONG pulBytesNeeded
);


NDIS_STATUS
MiniportSetInformation(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID pvInformationBuffer,
   IN ULONG ulInformationBufferLength,
   OUT PULONG pulBytesRead,
   OUT PULONG pulBytesNeeded
);


NDIS_STATUS
MiniportSend(
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN PNDIS_PACKET pPacket,
   IN UINT uiFlags
);


NDIS_STATUS
MiniportTransferData(
   OUT PNDIS_PACKET pPacket,
   OUT PUINT puiBytesTransferred,
   IN NDIS_HANDLE hMiniportAdapterContext,
   IN NDIS_HANDLE hMiniportReceiveContext,
   IN UINT uiByteOffset,
   IN UINT uiBytesToTransfer
);

void
MiniportHalt(
   IN NDIS_HANDLE hMiniportAdapterContext
);

NDIS_STATUS
MiniportReset(
   OUT PBOOLEAN pbAddressingReset,
   IN NDIS_HANDLE hMiniportAdapterContext
);


#ifdef __cplusplus
}   // extern "C"
#endif



typedef struct _tagNDUMMY_ADAPTER
{
    WCHAR szRegistryPath[MAX_PATH];
} NDUMMY_ADAPTER, *PNDUMMY_ADAPTER;

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
