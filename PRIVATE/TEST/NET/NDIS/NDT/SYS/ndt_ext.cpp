//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT_Ext.h"

//------------------------------------------------------------------------------

NDIS_STATUS NDT_NdisAllocateUnicodeString(
   IN OUT PNDIS_STRING DestinationString,
   IN PCWSTR SourceString
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   DestinationString->Length = 0;
   DestinationString->MaximumLength = 0;
   DestinationString->Buffer = NULL;

   if (SourceString != NULL) {
      ULONG Length;
      PCWSTR String = SourceString;
      NDIS_PHYSICAL_ADDRESS phAddr = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1); 

      while (*String != L'\0') String++;
      Length = (ULONG)String - (ULONG)SourceString;

      status = NdisAllocateMemory(
         (PVOID*)&DestinationString->Buffer, Length + sizeof(WCHAR), 0, phAddr
      );
      if (status == NDIS_STATUS_SUCCESS) {
         DestinationString->Length = (USHORT)Length;
         DestinationString->MaximumLength = (USHORT)(Length + sizeof(WCHAR));
         NdisMoveMemory(
            DestinationString->Buffer, (PVOID)SourceString, 
            DestinationString->MaximumLength
         );
      }
   }
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS NDT_NdisAllocateString(
   IN OUT PNDIS_STRING DestinationString,
   IN PNDIS_STRING SourceString
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   DestinationString->Length = 0;
   DestinationString->MaximumLength = 0;
   DestinationString->Buffer = NULL;

   if (SourceString->Buffer != NULL) {
      NDIS_PHYSICAL_ADDRESS phAddr = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1); 

      status = NdisAllocateMemory(
         (PVOID*)&DestinationString->Buffer, SourceString->MaximumLength, 0, 
         phAddr
      );
      if (status == NDIS_STATUS_SUCCESS) {
         DestinationString->Length = SourceString->Length;
         DestinationString->MaximumLength = SourceString->MaximumLength;
         NdisMoveMemory(
            DestinationString->Buffer, SourceString->Buffer, 
            SourceString->MaximumLength
         );
      }
   }
   return status;
}

//------------------------------------------------------------------------------

static ULONG ulRandomNumber;

//------------------------------------------------------------------------------

void NDT_NdisInitRandom()
{
   NdisGetSystemUpTime(&ulRandomNumber);
}

//------------------------------------------------------------------------------

ULONG NDT_NdisGetRandom(ULONG ulLow, ULONG ulHigh)
{
   const ULONG ulRandomMultiplier =  9301;
   const ULONG ulRandomAddend     = 49297;

   ulRandomNumber = (ulRandomNumber * ulRandomMultiplier) + ulRandomAddend;
   return ulRandomNumber % (ulHigh - ulLow + 1) + ulLow;
}

//------------------------------------------------------------------------------
