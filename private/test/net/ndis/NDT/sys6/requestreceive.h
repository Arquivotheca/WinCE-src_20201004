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
#ifndef __REQUEST_RECEIVE_H
#define __REQUEST_RECEIVE_H

//------------------------------------------------------------------------------

#include "Request.h"

class CPacket;

//------------------------------------------------------------------------------

class CRequestReceive : public CRequest
{
public:
   ULONG m_ulPacketsReceived;             // How many packets we received
   ULONG m_ulPacketsReplied;              // How many replies we get
   ULONG m_ulPacketsCompleted;            // How many packets was sent

   LARGE_INTEGER m_ulStartTime;                   // When we start send
   LARGE_INTEGER m_ulLastTime;                    // Last time when we was called
   ULONG m_cbSent;                        // How many bytes we send
   ULONG m_cbReceived;                    // How many bytes we received
   ULONG m_ulConversationId;               // Prevent collisions on the LAN
   
public:
   CRequestReceive(CBinding *pBinding = NULL);
   virtual ~CRequestReceive();

   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute();

   void Stop();
   NDIS_STATUS Receive(CPacket* pPacket);

   void SendComplete(CPacket* pPacket, NDIS_STATUS status);
};

//------------------------------------------------------------------------------

#endif
