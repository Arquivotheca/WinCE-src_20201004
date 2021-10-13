//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

NDIS_STATUS NDT_NdisAllocateUnicodeString(
   IN OUT PNDIS_STRING DestinationString,
   IN PCWSTR SourceString
);

NDIS_STATUS NDT_NdisAllocateString(
   IN OUT PNDIS_STRING DestinationString,
   IN PNDIS_STRING SourceString
);

void NDT_NdisInitRandom();
ULONG NDT_NdisGetRandom(ULONG ulLow, ULONG ulHigh);

//------------------------------------------------------------------------------

#endif
