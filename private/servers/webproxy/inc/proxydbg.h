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

#ifndef __PROXYDBG_H__
#define __PROXYDBG_H__

#include "global.h"

#ifdef DEBUG

#define ZONE_OUTPUT            DEBUGZONE(0)
#define ZONE_CONNECT        DEBUGZONE(1)
#define ZONE_REQUEST        DEBUGZONE(2)
#define ZONE_RESPONSE        DEBUGZONE(3)
#define ZONE_PACKETS        DEBUGZONE(4)
#define ZONE_PARSER            DEBUGZONE(5)
#define ZONE_AUTH            DEBUGZONE(6)
#define ZONE_SERVICE        DEBUGZONE(7)
#define ZONE_SESSION        DEBUGZONE(8)
#define ZONE_FILTER         DEBUGZONE(9)
#define ZONE_WARN            DEBUGZONE(14)
#define ZONE_ERROR            DEBUGZONE(15)

#define IFDBG(x) x

#else

#define IFDBG(x)

#endif // DEBUG

void DumpBuff(unsigned char *lpBuffer, unsigned int cBuffer);
void DebugOut (unsigned int cMask, WCHAR *lpszFormat, ...);
void DebugInit(void);
void DebugDeinit(void);

#endif // __PROXYDBG_H__

