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
#ifndef __REQUEST_SEND_H
#define __REQUEST_SEND_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestSend : public CRequest
{
private:
   NDIS_TIMER m_timerBeat;                // Timer used for beats
   NDIS_SPIN_LOCK m_spinLock;             // The spin lock packet status
   
   NET_BUFFER_LIST *m_pNdisNBLs;          // NDIS NBL list for send beat
   PNET_BUFFER* m_apNdisNBs;              // NDIS NB array for send beat
   CPacket** m_apPackets;                 // Packet array for send beat

public:
   UINT  m_uiStartBeatDelay;              // Delay before first beat
   ULONG m_ulPacketsToSend;               // How many pakets remain to send
   ULONG m_ulCancelId;                    // Cancel Id
   
   // Input parameters
   UINT   m_cbSrcAddr;                    // Size of source address
   UCHAR* m_pucSrcAddr;                   // Source address
   UINT   m_cbDestAddr;                   // Size of destination address
   UINT   m_uiDestAddrs;                  // Number of destination adresses
   UCHAR* m_pucDestAddrs;                 // Destination addresses
   UCHAR  m_ucResponseMode;               // Response mode
   UCHAR  m_ucPacketSizeMode;             // How size of packet will change
   ULONG  m_ulPacketSize;                 // Size of packets to send
   ULONG  m_ulPacketCount;                // How many packets we want send
   ULONG m_ulConversationId;               // Prevent collisions on the LAN   
   UINT   m_uiBeatDelay;                  // How long it should take
   UINT   m_uiBeatGroup;                  // Requested send rate in packts/sec

   ULONGLONG  m_ulTimeout;                    // Timeout for reply receiving
   
   // Output parameters
   ULONG  m_ulPacketsSent;                // How many packets we tried sent
   ULONG  m_ulPacketsCompleted;           // How many packets was sent
   ULONG  m_ulPacketsCanceled;            // How many packets was canceled
   ULONG  m_ulPacketsUncanceled;          // How many packets wasn't canceled
   ULONG  m_ulPacketsReplied;             // How many replies we get
   LARGE_INTEGER  m_ulStartTime;                  // When we start send
   LARGE_INTEGER  m_ulLastTime;                   // Last time when we was called
   ULONG  m_cbSent;                       // How many bytes we send
   ULONG  m_cbReceived;                   // How many bytes we received
   
private:

   static VOID SendBeat(
      IN PVOID SystemSpecific1, IN PVOID FunctionContext,
      IN PVOID SystemSpecific2, IN PVOID SystemSpecific3
   );

   void Beat();
   void CalculateParameters();

public:
   CRequestSend(CBinding* pBinding = NULL);
   virtual ~CRequestSend();

   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute();

   void SendComplete(CPacket* pPacket, NDIS_STATUS status, BOOL bCancel);
   NDIS_STATUS Receive(CPacket* pPacket);

   void StopBeat();
};

//------------------------------------------------------------------------------

#endif
