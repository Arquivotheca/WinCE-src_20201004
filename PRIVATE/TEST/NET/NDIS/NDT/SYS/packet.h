//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __PACKET_H
#define __PACKET_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ProtocolHeader.h"

//------------------------------------------------------------------------------

class CBinding;
class CRequest;

//------------------------------------------------------------------------------

class CPacket : public CObject
{
   friend class CBinding;
   
private:
   CBinding* m_pBinding;                  // We belong to this binding

public:   
   CRequest* m_pRequest;                  // And we currently work for it
   NDIS_PACKET* m_pNdisPacket;            // Pointer to NDIS descriptor

   UINT   m_uiSize;                       // Actual packet size
   UCHAR* m_pucMediumHeader;              // Pointer to packet medium header
   PROTOCOL_HEADER* m_pProtocolHeader;    // Pointer to packet protocol header
   UCHAR* m_pucBody;                      // Pointer to packet body
   BOOL   m_bBodyStatic;                  // Is body static?

   ULONG  m_ulStateChange;                // Time when packet state changed
   union {
      struct {
         USHORT m_bSendCompleted : 1;     // Packet was completed
         USHORT m_bReplyReceived : 1;     // Packet was replied
      };
      struct {
         USHORT m_bTransferred : 1;       // Packet was transfered
      };
   };
   
public:
   CPacket(CBinding* pBinding, NDIS_PACKET* pNdisPacket);
   virtual ~CPacket();

   LONG ReleaseEx();                      // Release is a little different

   NDIS_STATUS ChainBufferAtFront(PVOID pv, UINT cb);
   NDIS_STATUS ChainBufferAtBack(PVOID pv, UINT cb);

   NDIS_STATUS AllocBufferAtFront(UINT cb, PVOID *ppv);
   NDIS_STATUS AllocBufferAtBack(UINT cb, PVOID *ppv);

   void FreeBufferAtFront();
   void FreeBufferAtBack();

   void UnchainAllBuffers();
   void FreeAllBuffers();
};

typedef CPacket *PCPacket;

//------------------------------------------------------------------------------

#endif
