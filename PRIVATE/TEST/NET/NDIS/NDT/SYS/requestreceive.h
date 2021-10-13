//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

   ULONG m_ulStartTime;                   // When we start send
   ULONG m_ulLastTime;                    // Last time when we was called
   ULONG m_cbSent;                        // How many bytes we send
   ULONG m_cbReceived;                    // How many bytes we received
   
public:
   CRequestReceive(CBinding *pBinding = NULL);
   
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute();

   void Stop();
   NDIS_STATUS Receive(CPacket* pPacket);

   void SendComplete(CPacket* pPacket, NDIS_STATUS status);
};

//------------------------------------------------------------------------------

#endif
