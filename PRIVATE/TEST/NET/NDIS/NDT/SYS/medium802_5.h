//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __MEDIUM_802_5_H
#define __MEDIUM_802_5_H

//------------------------------------------------------------------------------

#include "Medium.h"

//------------------------------------------------------------------------------

class CMedium802_5 : public CMedium
{
protected:
   BYTE m_abPermanentAddress[6];
   
public:
   CMedium802_5(CBinding *pBinding);
   virtual ~CMedium802_5();

   virtual NDIS_STATUS Init(UINT uiLookAheadSize);

   virtual NDIS_STATUS BuildMediaHeader(
      CPacket* pPacket, PBYTE pbDestAddr, PBYTE pbSrcAddr, UINT uiSize
   );
   virtual NDIS_STATUS BuildReplyMediaHeader(
      CPacket* pPacket, UINT uiSize, CPacket* pRecvPacket
   );

   virtual UINT CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize);

   virtual INT IsSendAddr(PVOID pvPacketHeader, UCHAR* pucAddr);
};

//------------------------------------------------------------------------------

#endif
