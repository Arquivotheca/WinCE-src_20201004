//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Messages.h"

//------------------------------------------------------------------------------

LPCTSTR g_NDTLogMessageTableZero[] = {
   _T("Time to complete test: %02d:%02d:%02d total milliseconds: %u"),
   _T("The test %s with %d errors and %d warnings")
};

//------------------------------------------------------------------------------

LPCTSTR g_NDTLogMessageTableTwo[] = {
   _T("Failed register the NDT test protocol driver (code %d)"),
   _T("Failed open the NDT test protocol driver (code %d)"),
   _T("Failed close the NDT test protocol driver (code %d)"),
   _T("Failed deregister the NDT test protocol driver (code %d)"),
   _T("Failed allocate memory for an DeviceIOControl overlapped structure"),
   _T("Failed create a file mapping with name %s (code %d)"),
   _T("Failed map a file with name %s to memory (code %d)"),
   _T("Failed create an event with name %s (code %d)"),
   _T("The NDT test driver DeviceIOControl failed (code %d)"),
   _T("The NDT test driver request failed (NDIS code 0x%08x)"),
};

//------------------------------------------------------------------------------

LPCTSTR* g_NDTLogMessageTables[] = {
   g_NDTLogMessageTableZero, g_NDTLogMessageTableTwo
};

//------------------------------------------------------------------------------

LPCTSTR g_szFailStartup =
   _T("NDTStartup for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailCleanup =
   _T("NDTCleanup for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailOpen =
   _T("NDTOpen for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailClose =
   _T("NDTClose failed with hr=0x%08x");
LPCTSTR g_szFailLoadMiniport =
   _T("NDTLoadMiniport failed with hr=0x%08x");
LPCTSTR g_szFailUnloadMiniport =
   _T("NDTUnloadMiniport failed with hr=0x%08x");
LPCTSTR g_szFailQueryAdapters =
   _T("NDTQueryAdapters for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailQueryProtocols =
   _T("NDTQueryProtocols for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailQueryBindings =
   _T("NDTQueryBindings for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailBindProtocol =
   _T("NDTBindProtocol for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailUnbindProtocol =
   _T("NDTUnbindProtocol for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailWriteVerifyFlag =
   _T("NDTWriteVerifyFlag failed with hr=0x%08x");
LPCTSTR g_szFailDeleteVerifyFlag =
   _T("NDTDeleteVerifyFlag failed with hr=0x%08x");
LPCTSTR g_szFailBind =
   _T("NDTBind failed with hr=0x%08x");
LPCTSTR g_szFailUnbind =
   _T("NDTUnbind failed with hr=0x%08x");
LPCTSTR g_szFailReset =
   _T("NDTReset failed with hr=0x%08x");
LPCTSTR g_szFailGetCounter =
   _T("NDTGetCounter failed with hr=0x%08x");
LPCTSTR g_szFailSetOptions =
   _T("NDTSetOptions for '%s' failed with hr=0x%08x");
LPCTSTR g_szFailQueryInfo =
   _T("NDTQueryInfo failed with hr=0x%08x");
LPCTSTR g_szFailSetInfo =
   _T("NDTSetInfo failed with hr=0x%08x");
LPCTSTR g_szFailSend =
   _T("NDTSend failed with hr=0x%08x");
LPCTSTR g_szFailSendWait =
   _T("NDTSendWait failed with hr=0x%08x");
LPCTSTR g_szFailReceive =
   _T("NDTReceive failed with hr=0x%08x");
LPCTSTR g_szFailReceiveStop =
   _T("NDTReceiveStop failed with hr=0x%08x");
LPCTSTR g_szFailSetId =
   _T("NDTSetId failed with hr=0x%08x");
LPCTSTR g_szFailSendPacket =
   _T("NDTSendPacket failed with hr=0x%08x");
LPCTSTR g_szFailListen =
   _T("NDTListen failed with hr=0x%08x");
LPCTSTR g_szFailListenStop =
   _T("NDTListenStop failed with hr=0x%08x");
LPCTSTR g_szFailReceivePacket =
   _T("NDTReceivePacket failed with hr=0x%08x");
LPCTSTR g_szFailGetPermanentAddr=
   _T("NDTGetPermanentAddr failed with hr=0x%08x");
LPCTSTR g_szFailGetMaximumFrameSize =
   _T("NDTGetMaximumFrameSize failed with hr=0x%08x");
LPCTSTR g_szFailGetMaximumTotalSize =
   _T("NDTGetMaximumTotalSize failed with hr=0x%08x");
LPCTSTR g_szFailSetPacketFilter =
   _T("NDTSetPacketFilter failed with hr=0x%08x");
LPCTSTR g_szFailGetPhysicalMedium =
   _T("NDTGetPhysicalMedium failed with hr=0x%08x");
LPCTSTR g_szFailGetLinkSpeed =
   _T("NDTGetLinkSpeed failed with hr=0x%08x");
LPCTSTR g_szFailGetFilters =
   _T("NDTGetFilters failed with hr=0x%08x");
LPCTSTR g_szFailAddMulticastAddr =
   _T("NDTAddMulticastAddr failed with hr=0x%08x");
LPCTSTR g_szFailDeleteMulticastAddr =
   _T("NDTDeleteMulticastAddr failed with hr=0x%08x");

//------------------------------------------------------------------------------
