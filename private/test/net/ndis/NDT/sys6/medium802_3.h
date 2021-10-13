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
#ifndef __MEDIUM_802_3_H
#define __MEDIUM_802_3_H

//------------------------------------------------------------------------------

#include "Medium.h"
#include "MDLChain.h"

//------------------------------------------------------------------------------

class CMedium802_3 : public CMedium
{
protected:
   BYTE m_abPermanentAddress[6];
   
public:
   CMedium802_3(CBinding *pBinding);
   virtual ~CMedium802_3();

   virtual NDIS_STATUS Init(UINT uiLookAheadSize);

   virtual NDIS_STATUS BuildMediaHeader(
      CPacket* pPacket, CMDLChain* pMDL, PBYTE pbDestAddr, PBYTE pbSrcAddr, UINT uiSize
   );
   virtual NDIS_STATUS BuildReplyMediaHeader(
      CPacket* pPacket, CMDLChain* pMDL, UINT uiSize, CPacket* pRecvPacket
   );

   virtual UINT CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize);

   virtual INT IsSendAddr(PVOID pvPacketHeader, UCHAR* pucAddr);
};

//------------------------------------------------------------------------------

#endif
