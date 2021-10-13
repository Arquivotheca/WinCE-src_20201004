//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#endif //_DRVDBG_H_



