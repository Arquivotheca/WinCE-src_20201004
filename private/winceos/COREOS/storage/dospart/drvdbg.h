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
#ifndef _DRVDBG_H_
#define _DRVDBG_H_

#ifdef DEBUG
//
// Debug zones
//
#define ZONEID_STORE        0
#define ZONEID_PARTITION    1
#define ZONEID_SEARCH       2
#define ZONEID_API          3
#define ZONEID_WARNING      14
#define ZONEID_ERROR        15


#define ZONE_STORE        DEBUGZONE(ZONEID_STORE)
#define ZONE_PARTITION    DEBUGZONE(ZONEID_PARTITION)
#define ZONE_SEARCH       DEBUGZONE(ZONEID_SEARCH)
#define ZONE_API          DEBUGZONE(ZONEID_API)
#define ZONE_WARNING      DEBUGZONE(ZONEID_WARNING)
#define ZONE_ERROR        DEBUGZONE(ZONEID_ERROR)

#define ZONEMASK_STORE        (1 << ZONEID_STORE)
#define ZONEMASK_PARTITION    (1 << ZONEID_PARTITION)
#define ZONEMASK_SEARCH       (1 << ZONEID_SEARCH)
#define ZONEMASK_API          (1 << ZONEID_API)
#define ZONEMASK_WARNING      (1 << ZONEID_WARNING)
#define ZONEMASK_ERROR        (1 << ZONEID_ERROR)

void DumpRegKey( DWORD dwZone, PCTSTR szKey, HKEY hKey);
#define DUMPREGKEY(dwZone, szKey, hKey) DumpRegKey(dwZone, szKey, hKey)

#else
#define DUMPREGKEY(dwZone, szKey, hKey)
#endif  // DEBUG

#ifndef UNDER_CE
// NT ONLY
#include <crtdbg.h>
#define DEBUGCHK(a) _ASSERTE(a)
#define DEBUGMSG(a,b)
#define RETAILMSG(a,b)
#define DEBUGREGISTER(a)
#define VERIFY(a)           a
#endif 

#endif //_DRVDBG_H_



