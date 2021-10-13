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
#ifndef __NDP_H
#define __NDP_H

//------------------------------------------------------------------------------

#ifdef UNDER_CE

#include "windows.h"
#include "tchar.h"
#include "winioctl.h"

#else

#include <ntddk.h>
//#include <ndis.h>

typedef ULONG DWORD, * PDWORD;
typedef void * LPVOID;
typedef BOOLEAN BOOL;
typedef unsigned char BYTE, *PBYTE;

#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "ndis.h"

#ifdef __cplusplus
}
#endif

#include "ndp_ioctl.h"

//------------------------------------------------------------------------------

#define ETH_ADDR_SIZE               6
#define ETH_HEADER_SIZE             14

#define NDP_PROTOCOL_COOKIE         ((DWORD)'NDPp')
#define NDP_ADAPTER_COOKIE          ((DWORD)'NDPa')

#define NDP_PACKET_ARRAY_COOKIE     ((DWORD)'NDPx')
#define NDP_PACKET_BODY_COOKIE      ((DWORD)'NDPb')

//------------------------------------------------------------------------------

struct __NDP_ADAPTER;         // declaration.
struct __NDP_PROTOCOL;

typedef struct __NDP_PROTOCOL {

   DWORD magicCookie;                        // Structure signature
   NDIS_HANDLE hProtocol;                    // NDIS protocol handle
   struct __NDP_ADAPTER * pAdapterList;				 // List of Adapters
   NDIS_SPIN_LOCK      Lock;				 // General purpose Lock

#ifndef UNDER_CE

   // Initialization sequence flag
   ULONG               ulInitSeqFlag;

// values for ulInitSeqFlag
#define  CREATED_IO_DEVICE      0x00000001
#define  REGISTERED_SYM_NAME    0x00000002
#define  REGISTERED_WITH_NDIS   0x00000004
   
   ULONG               ulNumCreates;
   
   // Global Properties - maintained for reference (to return in Get calls)
   ULONG               ulRecvDelay;        // millisecs
   ULONG               ulSendDelay;        // millisecs
   ULONG               ulSimulateSendPktLoss;  // Percentage
   ULONG               ulSimulateRecvPktLoss;  // Percentage
   BOOLEAN             fSimulateResLimit;
   BOOLEAN             fDontRecv;
   BOOLEAN             fDontSend;
   
   NDIS_HANDLE         MiniportHandle;
   
   ULONG               ulAdapterCount;
   PDRIVER_OBJECT      pDriverObject;
   PDEVICE_OBJECT      pDeviceObject;

#endif

} NDP_PROTOCOL, *PNDP_PROTOCOL;

//------------------------------------------------------------------------------

typedef struct _NDP_PACKET_QUEUE {
   NDIS_SPIN_LOCK spinLock;
   NDIS_EVENT hEvent;
   NDIS_PACKET *pFirstPacket;
   NDIS_PACKET *pLastPacket;
} NDP_PACKET_QUEUE;

//------------------------------------------------------------------------------

typedef struct __NDP_ADAPTER {

   DWORD magicCookie;                        // Structure signature
   struct __NDP_PROTOCOL *pProtocol;         // Parent protocol
   struct __NDP_ADAPTER * pAdapterNext;				 // Next Adapter
   
   NDIS_HANDLE hAdapter;                     // NDIS adapter handle

   UINT macOptions;                          // OID_GEN_MAC_OPTIONS
   UINT maxTotalSize;                        // OID_GEN_MAXIMUM_TOTAL_SIZE
   UINT maxSendPackets;                      // OID_GEN_MAXIMIM_SEND_PACKETS
   UCHAR srcAddr[ETH_ADDR_SIZE];             // OID_802_3_CURRENT_ADDRESS
   
   NDIS_EVENT hPendingEvent;                 // Event for pending operation
   NDIS_STATUS status;                       // Pending operation result

   NDIS_HANDLE hSendPacketPool;              // Packet pool for send
   NDIS_HANDLE hSendBufferPool;              // Buffer pool for send

   NDIS_HANDLE hRecvPacketPool;              // Packet pool for receive
   NDIS_HANDLE hRecvBufferPool;              // Buffer pool for receive

   BOOLEAN fStressMode;                      // Is adapter in stress mode?
   BOOLEAN fReceiveMode;                     // Is adapter in receive mode?
   UINT packetType;                          // Receive this packet type             

   NDP_PACKET_QUEUE sendQueue;               // Packets used in send
   NDP_PACKET_QUEUE recvQueue;               // Packets used in receive
   NDP_PACKET_QUEUE workQueue;               // Received packets in process
   NDP_PACKET_QUEUE waitQueue;               // Received packets

   ULONG packetsInSend;                      // Packets pending send
   ULONG packetsSent;                        // How many packets was sent
   ULONG bytesSent;                          // How many bytes was sent

   ULONG packetsInReceive;                   // Packets pending receive
   ULONG packetsReceived;                    // How many packets was received
   ULONG bytesReceived;                      // How many bytes was received

   ULONG startTime;                          // Time on first packet
   ULONG lastTime;                           // Time on last packet
   DWORD startIdleTime;                      // Idle time on first packet 
   DWORD lastIdleTime;                       // Idle time on last packet

   LONG sendCompleteCounter;
} NDP_ADAPTER, *PNDP_ADAPTER;

NDIS_STATUS NDPInitProtocol(NDP_PROTOCOL* pDevice);
NDIS_STATUS NDPDeInitProtocol(NDP_PROTOCOL* pDevice);
NDIS_STATUS NDPInitAdapter(NDP_ADAPTER* pAdapter);
NDIS_STATUS NDPDeInitAdapter(NDP_ADAPTER* pAdapter);
NDIS_STATUS QueryOid(NDP_ADAPTER* pAdapter, NDIS_OID oid, PVOID pBuffer, PDWORD psize);
NDIS_STATUS SetOid(NDP_ADAPTER* pAdapter, NDIS_OID oid, PVOID pBuffer, PDWORD psize);
BOOL ndp_OpenAdapter(NDP_ADAPTER* pAdapter, LPCTSTR szAdapter);
BOOL ndp_CloseAdapter(NDP_ADAPTER* pAdapter);
BOOL ndp_SendPacket(NDP_ADAPTER* pAdapter, NDP_SEND_PACKET_INP* pInp);
BOOL ndp_Listen(NDP_ADAPTER* pAdapter, NDP_LISTEN_INP* pInp);
BOOL ndp_RecvPacket(NDP_ADAPTER* pAdapter, NDP_RECV_PACKET_INP* pInp, NDP_RECV_PACKET_OUT* pOut);
BOOL ndp_StressSend(NDP_ADAPTER* pAdapter, NDP_STRESS_SEND_INP* pInp, NDP_STRESS_SEND_OUT* pOut);
BOOL ndp_StressReceive(NDP_ADAPTER* pAdapter, NDP_STRESS_RECV_INP* pInp, NDP_STRESS_RECV_OUT* pOut);


//------------------------------------------------------------------------------

#endif
