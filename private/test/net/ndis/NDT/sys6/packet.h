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
#ifndef __PACKET_H
#define __PACKET_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ProtocolHeader.h"

//------------------------------------------------------------------------------

class CBinding;
class CRequest;
class CMDLChain;
//------------------------------------------------------------------------------

class CPacket : public CObject
{
   friend class CBinding;
   
private:
   CBinding* m_pBinding;                  // We belong to this binding

public:   
   CRequest* m_pRequest;                  // And we currently work for it
   NET_BUFFER* m_pNdisNB;            // Pointer to NDIS NB

   UINT   m_uiSize;                       // Actual packet size
   UCHAR* m_pucMediumHeader;              // Pointer to packet medium header
   PROTOCOL_HEADER* m_pProtocolHeader;    // Pointer to packet protocol header
   UCHAR* m_pucBody;                      // Pointer to packet body
   BOOL   m_bBodyStatic;                  // Is body static?

   LARGE_INTEGER  m_ulStateChange;                // Time when packet state changed
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
   CPacket(CBinding* pBinding, PNET_BUFFER pNB);
   virtual ~CPacket();

   void PopulateNetBuffer(CMDLChain *pMDLChain, SIZE_T sDataLength);
   void DestroyNetBufferMDLs();
};

typedef CPacket *PCPacket;

//------------------------------------------------------------------------------

#endif
