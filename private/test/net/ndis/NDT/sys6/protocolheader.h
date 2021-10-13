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
#ifndef __PROTOCOL_HEADER_H
#define __PROTOCOL_HEADER_H

//------------------------------------------------------------------------------

typedef UNALIGNED struct 
{
   UCHAR    ucDSAP;              // always 0xAA
   UCHAR    ucSSAP;              // always 0xAA
   UCHAR    ucControl;           // always 0x03
   UCHAR    ucPID0;              // always 0x00
   UCHAR    ucPID1;              // always 0x00
   UCHAR    ucPID2;              // always 0x00
   USHORT   usDIX;               // always 0x3781
   ULONG    ulSignature;         // always 'NDIS'
   USHORT   usTargetPortId;      // target port id
   USHORT   usSourcePortId;      // local port id
   ULONG    ulSequenceNumber;    // packet sequence number for a command
   ULONG    ulConversationId;       // converstation id to distinguish packets meant for this machine
   UCHAR    ucResponseMode;      // response required
   UCHAR    ucFirstByte;         // value of first byte in the packet body
   USHORT   usReplyId;           // ID used for reply identification
   UINT     uiSize;              // Header & body size
   ULONG    ulCheckSum;          // packet header check sum
} PROTOCOL_HEADER;

//------------------------------------------------------------------------------

#endif
