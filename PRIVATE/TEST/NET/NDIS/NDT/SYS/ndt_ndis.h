//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __NDT_NDIS_H
#define __NDT_NDIS_H

//------------------------------------------------------------------------------

#include "ndis.h"

//------------------------------------------------------------------------------

inline VOID NDT_NdisRegisterProtocol(
   OUT PNDIS_STATUS Status, OUT PNDIS_HANDLE NdisProtocolHandle, 
   IN PNDIS_PROTOCOL_CHARACTERISTICS ProtocolCharacteristics, 
   IN UINT CharacteristicsLength
) {
   NdisRegisterProtocol(
      Status, NdisProtocolHandle, ProtocolCharacteristics, CharacteristicsLength
   );
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisDeregisterProtocol(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE NdisProtocolHandle
) {
   NdisDeregisterProtocol(Status, NdisProtocolHandle);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisOpenAdapter(
   OUT PNDIS_STATUS Status, OUT PNDIS_STATUS OpenErrorStatus, 
   OUT PNDIS_HANDLE NdisBindingHandle, OUT PUINT SelectedMediumIndex, 
   IN PNDIS_MEDIUM MediumArray, IN UINT MediumArraySize, 
   IN NDIS_HANDLE NdisProtocolHandle, IN NDIS_HANDLE ProtocolBindingContext,
   IN PNDIS_STRING AdapterName, IN UINT OpenOptions, 
   IN PSTRING AddressingInformation OPTIONAL
) {
   NdisOpenAdapter(
      Status, OpenErrorStatus, NdisBindingHandle, SelectedMediumIndex, 
      MediumArray, MediumArraySize, NdisProtocolHandle, ProtocolBindingContext,
      AdapterName, OpenOptions, AddressingInformation
   );
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisCloseAdapter(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE NdisBindingHandle
) {
   NdisCloseAdapter(Status, NdisBindingHandle);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisRequest(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE NdisBindingHandle,
   IN PNDIS_REQUEST NdisRequest
) {
   NdisRequest(Status, NdisBindingHandle, NdisRequest);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisReset(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE NdisBindingHandle
) {
   NdisReset(Status, NdisBindingHandle);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisSend(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE NdisBindingHandle,
   IN PNDIS_PACKET Packet
) {
   NdisSend(Status, NdisBindingHandle, Packet);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisSendPackets(
   IN NDIS_HANDLE NdisBindingHandle, IN PPNDIS_PACKET PacketArray,
   IN UINT NumberOfPackets
) {
   NdisSendPackets(NdisBindingHandle, PacketArray, NumberOfPackets);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisTransferData(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE NdisBindingHandle,
   IN NDIS_HANDLE MacReceiveContext, IN UINT ByteOffset,
   IN UINT BytesToTransfer, IN OUT PNDIS_PACKET Packet,
   OUT PUINT BytesTransferred
) {
   NdisTransferData(
      Status, NdisBindingHandle, MacReceiveContext, ByteOffset, BytesToTransfer,
      Packet, BytesTransferred
   );
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisCancelSendPackets(
   IN NDIS_HANDLE NdisBindingHandle, IN PVOID CancelId
) {
   NdisCancelSendPackets(NdisBindingHandle, CancelId);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisAllocatePacketPool(
   OUT PNDIS_STATUS Status, OUT PNDIS_HANDLE PoolHandle,
    IN UINT NumberOfDescriptors, IN UINT ProtocolReservedLength
) {
   NdisAllocatePacketPool(
      Status, PoolHandle, NumberOfDescriptors, ProtocolReservedLength
   );
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisFreePacketPool(IN NDIS_HANDLE PoolHandle) {
   NdisFreePacketPool(PoolHandle);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisAllocateBufferPool(
   OUT PNDIS_STATUS Status, OUT PNDIS_HANDLE PoolHandle,
   IN UINT NumberOfDescriptors
) {
   NdisAllocateBufferPool(Status, PoolHandle, NumberOfDescriptors);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisFreeBufferPool(IN NDIS_HANDLE PoolHandle) {
   NdisFreeBufferPool(PoolHandle);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisInitializeEvent(IN PNDIS_EVENT Event) {
   NdisInitializeEvent(Event);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisFreeEvent(IN PNDIS_EVENT Event) {
   NdisFreeEvent(Event);
}

//------------------------------------------------------------------------------

inline BOOL NDT_NdisWaitEvent(IN PNDIS_EVENT Event, IN UINT msToWait) {
   return NdisWaitEvent(Event, msToWait);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisSetEvent(IN PNDIS_EVENT Event) {
   NdisSetEvent(Event);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisResetEvent(IN PNDIS_EVENT Event) {
   NdisResetEvent(Event);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisInitializeTimer(
   IN OUT PNDIS_TIMER Timer, IN PNDIS_TIMER_FUNCTION TimerFunction, 
   IN PVOID FunctionContext
) {
   NdisInitializeTimer(Timer, TimerFunction, FunctionContext);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisSetTimer(
   IN PNDIS_TIMER Timer, IN UINT MillisecondsToDelay
) {
   NdisSetTimer(Timer, MillisecondsToDelay);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisCancelTimer(
   IN PNDIS_TIMER Timer, OUT PBOOLEAN TimerCanceled
) {
   NdisCancelTimer(Timer, TimerCanceled);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisAllocatePacket(
   OUT PNDIS_STATUS Status, OUT PNDIS_PACKET* Packet, IN NDIS_HANDLE PoolHandle
) {
   NdisAllocatePacket(Status, Packet, PoolHandle);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisFreePacket(IN PNDIS_PACKET Packet) {
   NdisFreePacket(Packet);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisAllocateBuffer(
   OUT PNDIS_STATUS Status, OUT PNDIS_BUFFER *Buffer, IN NDIS_HANDLE PoolHandle,
   IN PVOID VirtualAddress, IN UINT Length
) {
   NdisAllocateBuffer(Status, Buffer, PoolHandle, VirtualAddress, Length);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisFreeBuffer(IN PNDIS_BUFFER Buffer) {
   NdisFreeBuffer(Buffer);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisChainBufferAtFront(
   IN OUT PNDIS_PACKET Packet, IN OUT PNDIS_BUFFER Buffer
) {
   NdisChainBufferAtFront(Packet, Buffer);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisUnchainBufferAtFront(
   IN OUT PNDIS_PACKET Packet, OUT PNDIS_BUFFER *Buffer
) {
   NdisUnchainBufferAtFront(Packet, Buffer);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisChainBufferAtBack(
   IN OUT PNDIS_PACKET Packet, IN OUT PNDIS_BUFFER Buffer
) {
   NdisChainBufferAtBack(Packet, Buffer);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisUnchainBufferAtBack(
   IN OUT PNDIS_PACKET Packet, OUT PNDIS_BUFFER *Buffer
) {
   NdisUnchainBufferAtBack(Packet, Buffer);
}

//------------------------------------------------------------------------------

inline PVOID NDT_NdisBufferVirtualAddress(IN PNDIS_BUFFER Buffer) {
   return NdisBufferVirtualAddress(Buffer);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisAllocateSpinLock(IN PNDIS_SPIN_LOCK SpinLock) {
   NdisAllocateSpinLock(SpinLock);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisFreeSpinLock(IN PNDIS_SPIN_LOCK SpinLock) {
   NdisFreeSpinLock(SpinLock);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisAcquireSpinLock(IN PNDIS_SPIN_LOCK SpinLock) {
   NdisAcquireSpinLock(SpinLock);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisReleaseSpinLock(IN PNDIS_SPIN_LOCK SpinLock) {
   NdisReleaseSpinLock(SpinLock);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisGetSystemUpTime(OUT PULONG pSystemUpTime) {
   NdisGetSystemUpTime(pSystemUpTime);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisZeroMemory(IN PVOID Destination, IN ULONG Length) {
   NdisZeroMemory(Destination, Length);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisMoveMemory(
   OUT PVOID Destination, IN PVOID Source, IN ULONG Length
) {
   NdisMoveMemory(Destination, Source, Length);
}

//------------------------------------------------------------------------------

inline NDIS_STATUS NDT_NdisAllocateMemory(
   OUT PVOID *VirtualAddress, IN UINT Length, IN UINT MemoryFlags,
    IN NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress
) {
   return NdisAllocateMemory(
      VirtualAddress, Length, MemoryFlags, HighestAcceptableAddress
   );
}

//------------------------------------------------------------------------------

inline NDIS_STATUS NDT_NdisAllocateMemoryWithTag(
   OUT PVOID *VirtualAddress, IN UINT Length, IN ULONG Tag
) {
   return NdisAllocateMemoryWithTag(VirtualAddress, Length, Tag);
}

//------------------------------------------------------------------------------
    
inline VOID NDT_NdisFreeMemory(
   IN PVOID VirtualAddress, IN UINT Length, IN UINT MemoryFlags
) {
   NdisFreeMemory(VirtualAddress, Length, MemoryFlags);
}

//------------------------------------------------------------------------------

inline VOID NDT_NdisCopyLookaheadData(
   IN PVOID Destination, IN PVOID Source, IN ULONG Length, IN ULONG MacOptions
) {
   NdisCopyLookaheadData(Destination, Source, Length, MacOptions);
}

//------------------------------------------------------------------------------

#endif
