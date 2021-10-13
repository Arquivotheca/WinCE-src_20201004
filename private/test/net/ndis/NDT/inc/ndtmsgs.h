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
#ifndef __NDT_MSGS_H
#define __NDT_MSGS_H

//------------------------------------------------------------------------------

extern LPCTSTR g_szFailStartup;
extern LPCTSTR g_szFailCleanup;
extern LPCTSTR g_szFailOpen;
extern LPCTSTR g_szFailClose;
extern LPCTSTR g_szFailLoadMiniport;
extern LPCTSTR g_szFailUnloadMiniport;
extern LPCTSTR g_szFailQueryAdapters;
extern LPCTSTR g_szFailQueryProtocols;
extern LPCTSTR g_szFailQueryBindings;
extern LPCTSTR g_szFailBindProtocol;
extern LPCTSTR g_szFailUnbindProtocol;
extern LPCTSTR g_szFailWriteVerifyFlag;
extern LPCTSTR g_szFailDeleteVerifyFlag;
extern LPCTSTR g_szFailBind;
extern LPCTSTR g_szFailUnbind;
extern LPCTSTR g_szFailReset;
extern LPCTSTR g_szFailGetCounter;
extern LPCTSTR g_szFailSetOptions;
extern LPCTSTR g_szFailQueryInfo;
extern LPCTSTR g_szFailSetInfo;
extern LPCTSTR g_szFailSend;
extern LPCTSTR g_szFailSendWait;
extern LPCTSTR g_szFailReceive;
extern LPCTSTR g_szFailReceiveStop;
extern LPCTSTR g_szFailSetId;
extern LPCTSTR g_szFailSendPacket;
extern LPCTSTR g_szFailListen;
extern LPCTSTR g_szFailListenStop;
extern LPCTSTR g_szFailReceivePacket;

extern LPCTSTR g_szFailGetPermanentAddr;
extern LPCTSTR g_szFailGetMaximumFrameSize;
extern LPCTSTR g_szFailGetMaximumTotalSize;
extern LPCTSTR g_szFailSetPacketFilter;
extern LPCTSTR g_szFailGetPhysicalMedium;
extern LPCTSTR g_szFailGetLinkSpeed;
extern LPCTSTR g_szFailGetFilters;
extern LPCTSTR g_szFailAddMulticastAddr;
extern LPCTSTR g_szFailDeleteMulticastAddr;
extern LPCTSTR g_szFailGetConversationId;
//------------------------------------------------------------------------------

#endif
