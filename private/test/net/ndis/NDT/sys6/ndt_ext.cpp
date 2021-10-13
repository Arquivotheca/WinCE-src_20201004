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
#include "StdAfx.h"
#include "NDT_Ext.h"

//------------------------------------------------------------------------------

VOID NDT_NdisAllocateUnicodeString(
   IN OUT PNDIS_STRING DestinationString,
   IN PCWSTR SourceString
)
{
   DestinationString->Length = 0;
   DestinationString->MaximumLength = 0;
   DestinationString->Buffer = NULL;

   if (SourceString != NULL) {
      ULONG Length;
      PCWSTR String = SourceString;
      
      while (*String != L'\0') String++;
      Length = (ULONG)String - (ULONG)SourceString;

      DestinationString->Buffer = (PWSTR) NdisAllocateMemoryWithTagPriority(
         NULL, Length + sizeof(WCHAR), NULL, NormalPoolPriority);
      
      if (DestinationString->Buffer != NULL) {
         DestinationString->Length = (USHORT)Length;
         DestinationString->MaximumLength = (USHORT)(Length + sizeof(WCHAR));
         NdisMoveMemory(
            DestinationString->Buffer, (PVOID)SourceString, 
            DestinationString->MaximumLength
         );
      }
   }
}

//------------------------------------------------------------------------------
/*
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
*/
//------------------------------------------------------------------------------

static ULONG ulRandomNumber;

//------------------------------------------------------------------------------

void NDT_NdisInitRandom()
{
   LARGE_INTEGER liUptime;

   NdisGetSystemUpTimeEx(&liUptime);
   ulRandomNumber = liUptime.LowPart;
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
/*
VOID PrintPackets(PPNDIS_PACKET PacketArray,UINT NumberOfPackets)
{
    for (UINT i = 0; i < NumberOfPackets; ++i)
    {
        PNDIS_PACKET p = PacketArray[i];
        RETAILMSG (1,(TEXT("NDT:: Packet %x"), p));
        PNDIS_BUFFER pBuffd = (PNDIS_BUFFER) p->Private.Head;
        UINT j;

        j=0;
        while (pBuffd)
        {
            UINT Length; PNDIS_BUFFER pTempBuffd;
            PBYTE pbBuff = NULL;
            ++j;
            NdisQueryBuffer(pBuffd, &pbBuff, &Length);
            RETAILMSG (1,(TEXT("NDT:: P: %x :: Bufdp%d: %x Len:%d"),p,j,pBuffd,Length));
            pTempBuffd = pBuffd;            
            NdisGetNextBuffer(pTempBuffd,&pBuffd);
        }
    }
}
*/
//------------------------------------------------------------------------------

