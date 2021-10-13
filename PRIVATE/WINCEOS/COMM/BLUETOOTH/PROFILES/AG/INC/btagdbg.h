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

#ifndef __BTAGDBG_H__
#define __BTAGDBG_H__

#include <windows.h>

#ifdef DEBUG

// Debug ZONE defines
#define ZONE_OUTPUT         DEBUGZONE(0)
#define ZONE_MISC           DEBUGZONE(1)
#define ZONE_SERVICE        DEBUGZONE(2)
#define ZONE_PARSER         DEBUGZONE(3)
#define ZONE_HANDLER        DEBUGZONE(4)

// zones 5, 6, 7,  14, 15 are defined in btagpub.h
// since they are used by network & phoneext & bond.

#define MAX_DEBUG_BUF   512

void DbgPrintATCmd(DWORD dwZone, LPCSTR szCommand, int cbCommand);

#endif // DEBUG


#endif // __BTAGDBG_H__

