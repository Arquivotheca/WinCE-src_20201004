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
#ifndef __MEDIUM_H
#define __MEDIUM_H

//------------------------------------------------------------------------------

#include "Object.h"

//------------------------------------------------------------------------------

class CPacket;

//------------------------------------------------------------------------------

class CMedium : public CObject
{
protected:
   CBinding* m_pBinding;
   
public:
   NDIS_MEDIUM m_medium;

   UINT m_uiAddressSize;                 // Size of address
   UINT m_uiHeaderSize;                  // Medium header size
   UINT m_uiMacOptions;                  // MAC options
   UINT m_uiMaxFrameSize;                // Maximal frame size
   UINT m_uiMaxTotalSize;                // Maximal total size
   UINT m_uiMaxSendPackes;               // Max packet for NdisSendPackets
   UINT m_uiLinkSpeed;                   // Link speed

public:
   CMedium(CBinding *pBinding, NDIS_MEDIUM medium);
   virtual ~CMedium();

   virtual NDIS_STATUS Init(UINT uiLookAheadSize);

   virtual NDIS_STATUS BuildMediaHeader(
      CPacket* pPacket, CMDLChain *pMDLChain, BYTE* pucDestAddr, BYTE* pucSrcAddr, UINT uiSize
   ) = 0;
   
   virtual NDIS_STATUS BuildReplyMediaHeader(
      CPacket* pPacket, CMDLChain *pMDLChain, UINT uiSize, CPacket* pRecvPacket
   ) = 0;
   
   virtual UINT CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize);

   virtual INT IsSendAddr(PVOID pvPacketHeader, UCHAR* pucAddr) = 0;
};

//------------------------------------------------------------------------------

#endif
