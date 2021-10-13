//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
   UCHAR    ucResponseMode;      // response required
   UCHAR    ucFirstByte;         // value of first byte in the packet body
   USHORT   usReplyId;           // ID used for reply identification
   UINT     uiSize;              // Header & body size
   ULONG    ulCheckSum;          // packet header check sum
} PROTOCOL_HEADER;

//------------------------------------------------------------------------------

#endif
