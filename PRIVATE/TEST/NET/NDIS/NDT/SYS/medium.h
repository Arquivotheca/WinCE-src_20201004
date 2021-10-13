//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
      CPacket* pPacket, BYTE* pucDestAddr, BYTE* pucSrcAddr, UINT uiSize
   ) = 0;
   
   virtual NDIS_STATUS BuildReplyMediaHeader(
      CPacket* pPacket, UINT uiSize, CPacket* pRecvPacket
   ) = 0;
   
   virtual UINT CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize);

   virtual INT IsSendAddr(PVOID pvPacketHeader, UCHAR* pucAddr) = 0;
};

//------------------------------------------------------------------------------

#endif
