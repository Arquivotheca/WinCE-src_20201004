//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _FSDDBG_H_
#define _FSDDBG_H_

#ifdef UNDER_CE
#ifdef DEBUG

#define ZONEID_INIT             0
#define ZONEID_ERRORS           1
#define ZONEID_POWER            2
#define ZONEID_EVENTS           3
#define ZONEID_DISKIO           4
#define ZONEID_APIS             5
#define ZONEID_HELPER           6
#define ZONEID_STOREAPI         7

#define ZONEMASK_INIT           (1 << ZONEID_INIT)
#define ZONEMASK_ERRORS         (1 << ZONEID_ERRORS)
#define ZONEMASK_POWER          (1 << ZONEID_POWER)
#define ZONEMASK_EVENTS         (1 << ZONEID_EVENTS)
#define ZONEMASK_DISKIO         (1 << ZONEID_DISKIO)
#define ZONEMASK_APIS           (1 << ZONEID_APIS)
#define ZONEMASK_HELPER         (1 << ZONEID_HELPER)
#define ZONEMASK_STOREAPI       (1 << ZONEID_STOREAPI)

#define ZONE_INIT               DEBUGZONE(ZONEID_INIT)
#define ZONE_ERRORS             DEBUGZONE(ZONEID_ERRORS)
#define ZONE_POWER              DEBUGZONE(ZONEID_POWER)
#define ZONE_EVENTS             DEBUGZONE(ZONEID_EVENTS)
#define ZONE_DISKIO             DEBUGZONE(ZONEID_DISKIO)
#define ZONE_APIS               DEBUGZONE(ZONEID_APIS)
#define ZONE_HELPER             DEBUGZONE(ZONEID_HELPER)
#define ZONE_STOREAPI           DEBUGZONE(ZONEID_STOREAPI) 
#ifndef ZONEMASK_DEFAULT
#define ZONEMASK_DEFAULT        (ZONEMASK_INIT|ZONEMASK_ERRORS|ZONEMASK_POWER)
#endif

#define DEBUGONLY(s)            s
#define RETAILONLY(s)
#define VERIFYTRUE(c)           DEBUGCHK(c)
#define VERIFYNULL(c)           DEBUGCHK(!(c))
#define DEBUGBREAK(cond)         if (cond) DebugBreak(); else
#define DEBUGMSGBREAK(cond,msg)  if (cond) {DEBUGMSG(TRUE,msg); DebugBreak();} else
#define DEBUGMSGWBREAK(cond,msg) if (cond) {DEBUGMSGW(TRUE,msg); DebugBreak();} else

void DumpRegKey( DWORD dwZone, PCTSTR szKey, HKEY hKey);
#define DUMPREGKEY(dwZone, szKey, hKey) DumpRegKey(dwZone, szKey, hKey)

#else // DEBUG

#define ZONE_INIT               FALSE
#define ZONE_ERRORS             FALSE
#define ZONE_POWER              FALSE
#define ZONE_EVENTS             FALSE
#define ZONE_DISKIO             FALSE
#define ZONE_APIS               FALSE
#define ZONE_HELPER             FALSE

#define DEBUGONLY(s)
#define RETAILONLY(s)           s
#define VERIFYTRUE(c)           c
#define VERIFYNULL(c)           c
#define DEBUGBREAK(cond)
#define DEBUGMSGBREAK(cond,msg)
#define DEBUGMSGWBREAK(cond,msg)

#define DUMPREGKEY(dwZone, szKey, hKey)

#endif // DEBUG

#else
 
// NT ONLY

#define ZONE_INIT               0x00000001
#define ZONE_ERRORS             0x00000002
#define ZONE_POWER              0x00000003
#define ZONE_EVENTS             0x00000004
#define ZONE_DISKIO             0x00000010
#define ZONE_APIS               0x00000020
#define ZONE_HELPER             0x00000040
#define ZONE_STOREAPI           0x00000080

extern "C" BOOL NtCheckDebugZone(DWORD dwZone);
extern "C" void NtDebugPrint (LPCTSTR pszFormat, ...);

#define DEBUGCHK(a)
#define PREFAST_DEBUGCHK(a)
#define PREFAST_ASSERT(a)
#define DEBUGMSG(a,b)       do { if (NtCheckDebugZone(a)) { NtDebugPrint b ; } } while(FALSE)
#define RETAILMSG(a,b)
#define DEBUGREGISTER(a)
#define VERIFY(a)           a
#define DUMPREGKEY(dwZone, szKey, hKey)

#endif // UNDER_CE

#endif // _FSDDBG_H_
