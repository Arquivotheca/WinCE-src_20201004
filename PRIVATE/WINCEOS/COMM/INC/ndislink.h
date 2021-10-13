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


 Module Name:    ndislink.h

 Abstract:       Provides function typedefs for dynamically linking with
                 ndis.lib.

 Contents:

--*/

#ifndef _NDISLINK_H_
#define _NDISLINK_H_

typedef NDIS_STATUS (NDISAPI *INDIS_NdisAllocateMemory)(
    OUT PVOID *VirtualAddress,
    IN UINT Length,
    IN UINT MemoryFlags,
    IN NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress
    );

typedef VOID (NDISAPI *INDIS_NdisFreeMemory)(
    IN PVOID VirtualAddress,
    IN UINT Length,
    IN UINT MemoryFlags
    );

typedef VOID (NDISAPI *INDIS_NdisAcquireSpinLock)(
    IN PNDIS_SPIN_LOCK SpinLock
    );

typedef VOID (NDISAPI *INDIS_NdisAllocateSpinLock)(
    IN PNDIS_SPIN_LOCK SpinLock
    );

typedef VOID (NDISAPI *INDIS_NdisFreeSpinLock)(
    IN PNDIS_SPIN_LOCK SpinLock
    );

typedef VOID (NDISAPI *INDIS_NdisReleaseSpinLock)(
    IN PNDIS_SPIN_LOCK SpinLock
    );

typedef VOID (NDISAPI *INDIS_NdisDprAcquireSpinLock)(
    IN PNDIS_SPIN_LOCK SpinLock
    );

typedef VOID (NDISAPI *INDIS_NdisDprReleaseSpinLock)(
    IN PNDIS_SPIN_LOCK SpinLock
    );

typedef VOID (NDISAPI *INDIS_NdisMSleep)(
    IN ULONG MicrosecondsToSleep
    );

typedef BOOLEAN (NDISAPI *INDIS_NdisMSynchronizeWithInterrupt)(
    IN PNDIS_MINIPORT_INTERRUPT Interrupt,
    IN PVOID SynchronizeFunction,
    IN PVOID SynchronizeContext
    );

typedef VOID (NDISAPI *INDIS_NdisMInitializeTimer)(
    IN OUT PNDIS_MINIPORT_TIMER Timer,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_TIMER_FUNCTION TimerFunction,
    IN PVOID FunctionContext
    );

typedef VOID (NDISAPI *INDIS_NdisSetTimer)(
    IN PNDIS_TIMER Timer,
    IN UINT MillisecondsToDelay
    );

typedef VOID (NDISAPI *INDIS_NdisInitializeListHead)(
    IN PLIST_ENTRY ListHead
);

typedef PLIST_ENTRY (NDISAPI *INDIS_NdisInterlockedInsertHeadList)(
    IN  PLIST_ENTRY             ListHead,
    IN  PLIST_ENTRY             ListEntry,
    IN  PNDIS_SPIN_LOCK         SpinLock
);

typedef PLIST_ENTRY (NDISAPI *INDIS_NdisInterlockedInsertTailList)(
    IN  PLIST_ENTRY             ListHead,
    IN  PLIST_ENTRY             ListEntry,
    IN  PNDIS_SPIN_LOCK         SpinLock
);

typedef PLIST_ENTRY (NDISAPI *INDIS_NdisInterlockedRemoveHeadList)(
    IN  PLIST_ENTRY             ListHead,
    IN  PNDIS_SPIN_LOCK         SpinLock
);


typedef VOID (NDISAPI *INDIS_NdisMSetPeriodicTimer)(
    IN PNDIS_MINIPORT_TIMER Timer,
    IN UINT MillisecondsPeriod
    );

typedef VOID (NDISAPI *INDIS_NdisMCancelTimer)(
    IN PNDIS_MINIPORT_TIMER Timer,
    OUT PBOOLEAN TimerCancelled
    );

typedef VOID (NDISAPI *INDIS_NdisInitializeWrapper)(
    OUT PNDIS_HANDLE NdisWrapperHandle,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

typedef VOID (NDISAPI *INDIS_NdisTerminateWrapper)(
    IN NDIS_HANDLE NdisWrapperHandle,
    IN PVOID SystemSpecific
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisMRegisterMiniport)(
    IN NDIS_HANDLE NdisWrapperHandle,
    IN PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
    IN UINT CharacteristicsLength
    );

typedef VOID (NDISAPI *INDIS_NdisMSetAttributes)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN BOOLEAN BusMaster,
    IN NDIS_INTERFACE_TYPE AdapterType
    );

typedef VOID (NDISAPI *INDIS_NdisMSetAttriubtesEx)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN UINT CheckForHangTimeInSeconds OPTIONAL,
    IN ULONG AttributeFlags,
    IN NDIS_INTERFACE_TYPE AdapterType
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisMRegisterIoPortRange)(
    OUT PVOID *PortOffset,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT InitialPort,
    IN UINT NumberOfPorts
    );

typedef VOID (NDISAPI *INDIS_NdisMDeregisterIoPortRange)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT InitialPort,
    IN UINT NumberOfPorts,
    IN PVOID PortOffset    
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisMRegisterInterrupt)(
    OUT PNDIS_MINIPORT_INTERRUPT Interrupt,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT InterruptVector,
    IN UINT InterruptLevel,
    IN BOOLEAN RequestIsr,
    IN BOOLEAN SharedInterrupt,
    IN NDIS_INTERRUPT_MODE InterruptMode
    );

typedef VOID (NDISAPI *INDIS_NdisMDeregisterInterrupt)(
    IN PNDIS_MINIPORT_INTERRUPT Interrupt
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisMMapIoSpace)(
    OUT PVOID *VirtualAddress,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN UINT Length
    );

typedef VOID (NDISAPI *INDIS_NdisMUnmapIoSpace)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PVOID VirtualAddress,
    IN UINT Length
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisQueryMapRegisterCount)(
    IN NDIS_INTERFACE_TYPE BusType,
    OUT PUINT  MapRegisterCount
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisMAllocateMapRegisters)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT DmaChannel,
    IN BOOLEAN Dma32BitAddresses,
    IN ULONG PhysicalMapRegistersNeeded,
    IN ULONG MaximumPhysicalMapping
    );

typedef VOID (NDISAPI *INDIS_NdisMFreeMapRegisters)(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

typedef VOID (NDISAPI *INDIS_NdisMStartBufferPhysicalMapping)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG PhysicalMapRegister,
    IN BOOLEAN WriteToDevice,
    OUT PNDIS_PHYSICAL_ADDRESS_UNIT PhysicalAddressArray,
    OUT PUINT ArraySize
    );

typedef VOID (NDISAPI *INDIS_NdisMCompleteBufferPhysicalMapping)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_BUFFER Buffer,
    IN ULONG PhysicalMapRegister
    );

typedef VOID (NDISAPI *INDIS_NdisMAllocateSharedMemory)(
    IN NDIS_HANDLE  MiniportAdapterHandle,
    IN ULONG  Length,
    IN BOOLEAN  Cached,
    OUT PVOID *VirtualAddress,
    OUT PNDIS_PHYSICAL_ADDRESS PhysicalAddress    
    );

typedef VOID (NDISAPI *INDIS_NdisMFreeSharedMemory)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    IN PVOID VirtualAddress,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    );

typedef ULONG (NDISAPI *INDIS_NdisReadPciSlotInformation)(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    );

typedef ULONG (NDISAPI *INDIS_NdisWritePciSlotInformation)(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN ULONG SlotNumber,
    IN ULONG Offset,
    IN PVOID Buffer,
    IN ULONG Length
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisMPciAssignResources)(
    IN NDIS_HANDLE MiniportHandle,
    IN ULONG SlotNumber,
    OUT PNDIS_RESOURCE_LIST *AssignedResources
    );

typedef VOID (NDISAPI *INDIS_NdisRegisterProtocol)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE NdisProtocolHandle,
    IN PNDIS_PROTOCOL_CHARACTERISTICS ProtocolCharacteristics,
    IN UINT CharacteristicsLength
    );

typedef VOID (NDISAPI *INDIS_NdisDeregisterProtocol)(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE NdisProtocolHandle
    );

typedef VOID (NDISAPI *INDIS_NdisOpenConfiguration)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE ConfigurationHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    );

typedef VOID (NDISAPI *INDIS_NdisOpenProtocolConfiguration)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING ProtocolSection
    );

typedef VOID (NDISAPI *INDIS_NdisCloseConfiguration)(
    IN NDIS_HANDLE ConfigurationHandle
    );

typedef VOID (NDISAPI *INDIS_NdisReadConfiguration)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_CONFIGURATION_PARAMETER *ParameterValue,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING Keyword,
    IN NDIS_PARAMETER_TYPE ParameterType
    );

typedef VOID (NDISAPI *INDIS_NdisWriteConfiguration)(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE ConfigurationHandle,
    IN PNDIS_STRING Keyword,
    IN PNDIS_CONFIGURATION_PARAMETER ParameterValue
    );

typedef VOID (NDISAPI *INDIS_NdisReadNetworkAddress)(
    OUT PNDIS_STATUS Status,
    OUT PVOID *NetworkAddress,
    OUT PUINT NetworkAddressLength,
    IN NDIS_HANDLE ConfigurationHandle
    );

typedef VOID (NDISAPI *INDIS_NdisAllocateBufferPool)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE PoolHandle,
    IN UINT NumberOfDescriptors
    );

typedef VOID (NDISAPI *INDIS_NdisFreeBufferPool)(
    IN NDIS_HANDLE PoolHandle
    );

typedef VOID (NDISAPI *INDIS_NdisAllocateBuffer)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_BUFFER *Buffer,
    IN NDIS_HANDLE PoolHandle,
    IN PVOID VirtualAddress,
    IN UINT Length
    );

typedef VOID (NDISAPI *INDIS_NdisFreeBuffer)(
    IN PNDIS_BUFFER Buffer    
    );

typedef VOID (NDISAPI *INDIS_NdisAdjustBufferLength)(
    IN PNDIS_BUFFER Buffer,
    IN UINT Length
    );

typedef VOID (NDISAPI *INDIS_NdisQueryBuffer)(
    IN PNDIS_BUFFER Buffer,
    OUT PVOID *VirtualAddress OPTIONAL,
    OUT PUINT Length
    );

typedef VOID (NDISAPI *INDIS_NdisQueryBufferOffset)(
    IN PNDIS_BUFFER Buffer,
    OUT PUINT Offset,
    OUT PUINT Length
    );

typedef VOID (NDISAPI *INDIS_NdisUnchainBufferAtFront)(
    IN OUT PNDIS_PACKET Packet,
    OUT PNDIS_BUFFER *Buffer
    );

typedef VOID (NDISAPI *INDIS_NdisUnchainBufferAtBack)(
    IN OUT PNDIS_PACKET Packet,
    OUT PNDIS_BUFFER *Buffer
    );

typedef VOID (NDISAPI *INDIS_NdisAllocatePacketPool)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_HANDLE PoolHandle,
    IN UINT NumberOfDescriptors,
    IN UINT ProtocolReservedLength
    );

typedef VOID (NDISAPI *INDIS_NdisFreePacketPool)(
    IN NDIS_HANDLE PoolHandle
    );

typedef VOID (NDISAPI *INDIS_NdisAllocatePacket)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_PACKET *Packet,
    IN NDIS_HANDLE PoolHandle
    );

typedef VOID (NDISAPI *INDIS_NdisFreePacket)(
    IN PNDIS_PACKET Packet
    );

typedef VOID (NDISAPI *INDIS_NdisCopyFromPacketToPacket)(
    IN PNDIS_PACKET Destination,
    IN UINT DestinationOffset,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Source,
    IN UINT SourceOffset,
    OUT PUINT BytesCopied
    );

typedef VOID (NDISAPI *INDIS_NdisInitializeString)(
    OUT PNDIS_STRING Destination,
    IN PUCHAR Source
    );

typedef VOID (NDISAPI *INDIS_NdisInitAnsiString)(
    IN OUT PANSI_STRING DestinationString,
    IN PCSTR SourceString
    );

typedef VOID (NDISAPI *INDIS_NdisInitUnicodeString)(
    IN OUT PUNICODE_STRING DestinationString,
    IN PCWSTR SourceString
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisAnsiStringToUnicodeString)(
    IN OUT PUNICODE_STRING DestinationString,
    IN PANSI_STRING SourceString
    );

typedef NDIS_STATUS (NDISAPI *INDIS_NdisUnicodeStringToAnsiString)(
    IN OUT PANSI_STRING DestinationString,
    IN PUNICODE_STRING SourceString
    );

typedef BOOLEAN (NDISAPI *INDIS_NdisEqualString)(
    IN PNDIS_STRING String1,
    IN PNDIS_STRING String2,
    IN BOOLEAN CaseInsensitive
    );

typedef VOID (NDISAPI *INDIS_NdisMTransferDataComplete)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );

typedef VOID (NDISAPI *INDIS_NdisOpenAdapter)(
    OUT PNDIS_STATUS Status,
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PNDIS_HANDLE NdisBindingHandle,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE NdisProtocolHandle,
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_STRING AdapterName,
    IN UINT OpenOptions,
    IN PSTRING AddressingInformation OPTIONAL
    );

typedef VOID (NDISAPI *INDIS_NdisCloseAdapter)(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE NdisBindingHandle
    );

typedef VOID (NDISAPI *INDIS_NdisReturnPackets)(
    IN PNDIS_PACKET *PacketsToReturn,
    IN UINT NumberOfPackets
    );

typedef VOID (NDISAPI *INDIS_NdisReset)(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE NdisBindingHandle
    );

typedef VOID (NDISAPI *INDIS_NdisMIndicateStatus)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    );

typedef VOID (NDISAPI *INDIS_NdisMIndicateStatusComplete)(
    IN NDIS_HANDLE MiniportAdapterHandle
    );

typedef VOID (NDISAPI *INDIS_NdisMQueryInformationComplete)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status
    );

typedef VOID (NDISAPI *INDIS_NdisMSetInformationComplete)(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status
    );

typedef VOID (NDISAPI *INDIS_NdisWriteErrorLogEntry)(
    IN NDIS_HANDLE NdisAdapterHandle,
    IN NDIS_ERROR_CODE ErrorCode,
    IN ULONG NumberOfErrorValues,
    ...
    );

typedef VOID (NDISAPI *INDIS_NdisMRegisterAdapterShutdownHandler)(
    IN NDIS_HANDLE MiniportHandle,
    IN PVOID ShutdownContext,
    IN ADAPTER_SHUTDOWN_HANDLER ShutdownHandler
    );

typedef VOID (NDISAPI *INDIS_NdisM_DeregisterAdapterShutdownHandler)(
    IN NDIS_HANDLE MiniportHandle
    );

typedef BOOLEAN (NDISAPI *INDIS_NdisIMSwitchToMiniport)(
    IN  NDIS_HANDLE MiniportAdapterHandle,
    OUT PNDIS_HANDLE SwitchHandle
    );

typedef BOOLEAN (NDISAPI *INDIS_NdisIMRevertBack)(
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  NDIS_HANDLE SwitchHandle
    );

//
// These are the entry points to NDIS that IrDAStk and ARP both need.
// In fact ARP doesn't need many of these (it requires a subset of irda's 
// entrypoints).
//

typedef struct _NDIS_PROTOCOL_VTABLE
{
    // Spin lock routines.
    INDIS_NdisAllocateSpinLock             NdisAllocateSpinLock;
    INDIS_NdisFreeSpinLock                 NdisFreeSpinLock;
    INDIS_NdisAcquireSpinLock              NdisAcquireSpinLock;
    INDIS_NdisReleaseSpinLock              NdisReleaseSpinLock;

    // Memory.
    INDIS_NdisAllocateMemory               NdisAllocateMemory;
    INDIS_NdisFreeMemory                   NdisFreeMemory;

    INDIS_NdisAllocatePacketPool           NdisAllocatePacketPool;
    INDIS_NdisFreePacketPool               NdisFreePacketPool;
    INDIS_NdisAllocateBufferPool           NdisAllocateBufferPool;
    INDIS_NdisFreeBufferPool               NdisFreeBufferPool;

    INDIS_NdisAllocateBuffer               NdisAllocateBuffer;
    INDIS_NdisFreeBuffer                   NdisFreeBuffer;
    INDIS_NdisAllocatePacket               NdisAllocatePacket;
    INDIS_NdisFreePacket                   NdisFreePacket;

    // Buffer manipulation.
    INDIS_NdisQueryBuffer                  NdisQueryBuffer;
    INDIS_NdisUnchainBufferAtFront         NdisUnchainBufferAtFront;

    // Configuration.
    INDIS_NdisOpenConfiguration            NdisOpenConfiguration;
    INDIS_NdisCloseConfiguration           NdisCloseConfiguration;
    INDIS_NdisReadConfiguration            NdisReadConfiguration;

    // Protocol specific.
    INDIS_NdisRegisterProtocol             NdisRegisterProtocol;
    INDIS_NdisDeregisterProtocol           NdisDeregisterProtocol;
    INDIS_NdisOpenAdapter                  NdisOpenAdapter;
    INDIS_NdisCloseAdapter                 NdisCloseAdapter;
    INDIS_NdisReturnPackets                NdisReturnPackets;

    // Synchronization
    INDIS_NdisInitializeListHead           NdisInitializeListHead;
    INDIS_NdisInterlockedInsertHeadList    NdisInterlockedInsertHeadList;
    INDIS_NdisInterlockedInsertTailList    NdisInterlockedInsertTailList;
    INDIS_NdisInterlockedRemoveHeadList    NdisInterlockedRemoveHeadList;

} NDIS_PROTOCOL_VTABLE, *PNDIS_PROTOCOL_VTABLE;

#define NDISDLL     TEXT("ndis.dll")

HINSTANCE               g_hInstNdisDll;
NDIS_PROTOCOL_VTABLE    g_INdis;

#define LOAD_NDIS_PROC(_EntryName)                                             \
{                                                                              \
    g_INdis.##_EntryName = (INDIS_##_EntryName)GetProcAddress(g_hInstNdisDll,  \
        TEXT(#_EntryName));                                                    \
    ASSERT(g_INdis.##_EntryName);                                              \
    if (g_INdis.##_EntryName == NULL)                                          \
        bSuccess = FALSE;                                                      \
}

__inline
BOOL LoadNdisProtocolVtable()
{
    BOOL bSuccess = TRUE;

    // Only call this function once.
    ASSERT(g_hInstNdisDll == NULL);
    g_hInstNdisDll = LoadDriver(NDISDLL);
    if (g_hInstNdisDll == NULL)
    {
        return (FALSE);
    }

    LOAD_NDIS_PROC(NdisAllocateSpinLock);
    LOAD_NDIS_PROC(NdisFreeSpinLock);
    LOAD_NDIS_PROC(NdisAcquireSpinLock);
    LOAD_NDIS_PROC(NdisReleaseSpinLock);
    LOAD_NDIS_PROC(NdisAllocateMemory);
    LOAD_NDIS_PROC(NdisFreeMemory);
    LOAD_NDIS_PROC(NdisAllocatePacketPool);
    LOAD_NDIS_PROC(NdisFreePacketPool);
    LOAD_NDIS_PROC(NdisAllocateBufferPool);
    LOAD_NDIS_PROC(NdisFreeBufferPool);
    LOAD_NDIS_PROC(NdisAllocateBuffer);
    LOAD_NDIS_PROC(NdisFreeBuffer);
    LOAD_NDIS_PROC(NdisAllocatePacket);
    LOAD_NDIS_PROC(NdisFreePacket);
    LOAD_NDIS_PROC(NdisQueryBuffer);
    LOAD_NDIS_PROC(NdisUnchainBufferAtFront);
    LOAD_NDIS_PROC(NdisReturnPackets);
    LOAD_NDIS_PROC(NdisOpenConfiguration);
    LOAD_NDIS_PROC(NdisCloseConfiguration);
    LOAD_NDIS_PROC(NdisReadConfiguration);
    LOAD_NDIS_PROC(NdisRegisterProtocol);
    LOAD_NDIS_PROC(NdisDeregisterProtocol);
    LOAD_NDIS_PROC(NdisOpenAdapter);
    LOAD_NDIS_PROC(NdisCloseAdapter);
    LOAD_NDIS_PROC(NdisInitializeListHead);
    LOAD_NDIS_PROC(NdisInterlockedInsertHeadList);
    LOAD_NDIS_PROC(NdisInterlockedInsertTailList);
    LOAD_NDIS_PROC(NdisInterlockedRemoveHeadList);

    if (bSuccess == FALSE)
    {
        FreeLibrary(g_hInstNdisDll);
    }
    return (bSuccess);
}

#endif // _NDISLINK_H_
