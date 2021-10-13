//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __PROXYDBG_H__
#define __PROXYDBG_H__

#include "global.h"

#ifdef DEBUG

#define ZONE_OUTPUT			DEBUGZONE(0)
#define ZONE_CONNECT		DEBUGZONE(1)
#define ZONE_REQUEST		DEBUGZONE(2)
#define ZONE_RESPONSE		DEBUGZONE(3)
#define ZONE_PACKETS		DEBUGZONE(4)
#define ZONE_PARSER			DEBUGZONE(5)
#define ZONE_AUTH			DEBUGZONE(6)
#define ZONE_WARN			DEBUGZONE(14)
#define ZONE_ERROR			DEBUGZONE(15)

#define IFDBG(x) x

#else

#define IFDBG(x)

#endif // DEBUG

void DumpBuff(unsigned char *lpBuffer, unsigned int cBuffer);
void DebugOut (unsigned int cMask, WCHAR *lpszFormat, ...);
void DebugInit(void);
void DebugDeinit(void);

#endif // __PROXYDBG_H__

