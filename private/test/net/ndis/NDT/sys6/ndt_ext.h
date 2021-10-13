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
#ifndef __NDIS_EXT_H
#define __NDIS_EXT_H

//------------------------------------------------------------------------------

#define NDT_NdisBreakPoint  DbgBreakPoint

//------------------------------------------------------------------------------

#define NDIS30_PROTOCOL_MAJOR_VERSION     3
#define NDIS30_PROTOCOL_MINOR_VERSION     0
#define NDIS40_PROTOCOL_MAJOR_VERSION     4
#define NDIS40_PROTOCOL_MINOR_VERSION     0

//------------------------------------------------------------------------------

VOID NDT_NdisAllocateUnicodeString(
   IN OUT PNDIS_STRING DestinationString,
   IN PCWSTR SourceString
);

/*
NDIS_STATUS NDT_NdisAllocateString(
   IN OUT PNDIS_STRING DestinationString,
   IN PNDIS_STRING SourceString
);
*/

void NDT_NdisInitRandom();
ULONG NDT_NdisGetRandom(ULONG ulLow, ULONG ulHigh);
//VOID PrintPackets(PPNDIS_PACKET PacketArray,UINT NumberOfPackets);

//------------------------------------------------------------------------------

#endif
