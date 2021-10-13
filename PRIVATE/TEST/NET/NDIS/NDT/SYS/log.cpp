//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Log.h"

//------------------------------------------------------------------------------

LPCTSTR g_aszMessageTableZero[] = {
   _T("Memory: Memory leaks detected\n"),
   _T("Memory: The memory block at 0x%08x has an invalid signature\n"),
   _T("Memory: The memory block at 0x%08x with total length = %d bytes\n"),
   _T("Memory: The trailer for this memory block has been overwritten\n"),
   _T("Memory: %03x0 = %s\n"),
   _T("HeartBeat: %d bindings\n"),
   _T("HeartBeat: FreeRecv %2u  FreeSend %2u  Sent %2u  Received %2u\n"),
   _T("HeartBeat:   RequestSend: Remain %lu\n")
};

//------------------------------------------------------------------------------

LPCTSTR g_aszMessageTableOne[] = {
   _T("+++ ProtocolBind0: 0x%08x, %s, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolBind0: 0x%08x\n"),
   _T("+++ ProtocolBind1: 0x%08x, %s, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolBind1: 0x%08x\n"),
   _T("+++ ProtocolUnload0\n"),
   _T("--- ProtocolUnload0\n"),
   _T("+++ ProtocolUnload1\n"),
   _T("--- ProtocolUnload1\n"),
   _T("+++ ProtocolPNPEvent0: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolPNPEvent0: 0x%08x\n"),
   _T("+++ ProtocolPNPEvent1: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolPNPEvent1: 0x%08x\n"),
   _T("+++ ProtocolOpenAdapterComplete: 0x%08x, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolOpenAdapterComplete\n"),
   _T("+++ ProtocolCloseAdapterComplete: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolCloseAdapterComplete\n"),
   _T("+++ ProtocolResetComplete: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolResetComplete\n"),
   _T("+++ ProtocolRequestComplete: 0x%08x, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolRequestComplete\n"),
   _T("+++ ProtocolStatus: 0x%08x, 0x%08x, 0x%08x, %d\n"),
   _T("--- ProtocolStatus\n"),
   _T("+++ ProtocolStatusComplete: 0x%08x\n"),
   _T("--- ProtocolStatusComplete\n"),
   _T("+++ ProtocolSendComplete: 0x%08x, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolSendComplete\n"),
   _T("+++ ProtocolTransferDataComplete: 0x%08x, 0x%08x, 0x%08x, %d\n"),
   _T("--- ProtocolTransferDataComplete\n"),
   _T("+++ ProtocolReceive: 0x%08x, 0x%08x, 0x%08x, %d, 0x%08x, %d, %d\n"),
   _T("--- ProtocolReceive: 0x%08x\n"),
   _T("+++ ProtocolReceiveComplete: 0x%08x\n"),
   _T("--- ProtocolReceiveComplete\n"),
   _T("+++ ProtocolReceivePacket: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolReceivePacket: %d\n"),
   _T("+++ ProtocolUnbind: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolUnbind: 0x%08x\n"),
   _T("+++ ProtocolCoSendComplete: 0x%08x, 0x%08x, 0x%08x, 0x%08x, %d\n"),
   _T("--- ProtocolCoSendComplete\n"),
   _T("+++ ProtocolCoStatus: 0x%08x, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolCoStatus\n"),
   _T("+++ ProtocolCoReceivePacket: 0x%08x, 0x%08x, 0x%08x\n"),
   _T("--- ProtocolCoReceivePacket: %d\n"),
   _T("+++ ProtocolCoAFRegistryNotify: 0x%08x, 0x%08x\n"),
   _T("--- ProtocolCoAFRegistryNotify\n")
};

//------------------------------------------------------------------------------

LPCTSTR g_aszMessageTableTwo[] = {
   _T("CDriver::Init - Create CProtocol object for NDIS 4.0 failed\n"),
   _T("CDriver::Init - RegisterProtocol for NDIS 4.0 failed, hr=%08x\n"),
   _T("CDriver::Init - Create CProtocol object for NDIS 5.x failed\n"),
   _T("CDriver::Init - RegisterProtocol for NDIS 5.x failed, hr=%08x\n")
};

//------------------------------------------------------------------------------

LPCTSTR g_aszMessageTableThree[] = {
   _T("CBind::OpenAdapterComplete - The event not expected\n"),
   _T("CBind::CloseAdapterComplete - The event not expected\n"),
   _T("CBind::ResetComplete - The event not expected\n"),
   _T("CBind::RequestComplete -  Event not expected\n"),
   _T("CBind::SendComplete - The packet didn't contain a protocol info\n"),
   _T("CBind::SendComplete - The packet didn't contain an associated request\n"),
   _T("CBind::SendComplete - The packet contains unknown request type (%d)\n"),
   _T("CBind::Receive - The packet with a zero length received\n"),
   _T("CBind::Receive - The indicated packet length %d is larger than the medium limit %d\n"),
   _T("CBind::Receive - NdisCopyFromPacketToPacket failed from %d bytes only %d was copied\n"),
   _T("CBind::Receive - The received packet contains only %d bytes from %d expected\n"),
   _T("CBind::Receive - The indicated packet length %d is smaler than lookhead buffer size %d\n"),
   _T("CBind::Receive - There are no packet descriptors avaiable, a packet was ignored\n"),
   _T("CBind::Receive - NdisTransferData failed, hr=%08x\n"),
   _T("CBind::TransferDataComplete - The packet didn't contain a protocol info\n"),
   _T("CBind::TransferDataComplete - The data transfer failed, hr=%08x\n"),
   _T("CBind::ReceivePacket - There are no packet descriptors avaiable, a packet was ignored\n")
};

//------------------------------------------------------------------------------

LPCTSTR* g_a2szMessageTables[] = {
   g_aszMessageTableZero, g_aszMessageTableOne, g_aszMessageTableTwo, 
   g_aszMessageTableThree
};

//------------------------------------------------------------------------------

ULONG g_ulLogLevel = NDT_INF;

//------------------------------------------------------------------------------

void LogSetLevel(ULONG ulLogLevel)
{
   g_ulLogLevel = ulLogLevel;
}

//------------------------------------------------------------------------------

void LogV(LPCTSTR szFormat, va_list arglist)
{
   TCHAR szBuffer[1024];

   wvsprintf(szBuffer, szFormat, arglist);
   OutputDebugString(szBuffer);
}

//------------------------------------------------------------------------------

void Log(ULONG ulId, ...)
{
   ULONG x = ulId & 0xFF000000;
   x = x >> 24;
   if (((ulId & 0xFF000000) >> 24) > g_ulLogLevel) return;

   va_list pArgs;
   LPCTSTR* aszFormatTable = g_a2szMessageTables[(ulId & 0x00FF0000) >> 16];

   va_start(pArgs, ulId);
   LogV(aszFormatTable[ulId & 0x0000FFFF], pArgs);
   va_end(pArgs); 
}

//------------------------------------------------------------------------------

void LogX(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(szFormat, pArgs);
   va_end(pArgs); 
}

//------------------------------------------------------------------------------
