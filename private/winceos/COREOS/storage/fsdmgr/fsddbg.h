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
#ifndef _FSDDBG_H_
#define _FSDDBG_H_

#ifdef DEBUG

#include <dbgapi.h>

extern  DBGPARAM        dpCurSettings;

#ifdef UNDER_CE
#define OWNCRITICALSECTION(cs)  ((cs)->LockCount > 0 && (DWORD)(cs)->OwnerThread == GetCurrentThreadId())
#else // UNDER_CE
#define OWNCRITICALSECTION(cs)  (TRUE)
#endif // !UNDER_CE

#define ZONEID_INIT             0
#define ZONEID_ERRORS           1
#define ZONEID_POWER            2
#define ZONEID_EVENTS           3
#define ZONEID_DISKIO           4
#define ZONEID_APIS             5
#define ZONEID_HELPER           6
#define ZONEID_STOREAPI         7
#define ZONEID_STOREMGR         8
#define ZONEID_VERBOSE          9
#define ZONEID_SECURITY        10

#define ZONEMASK_INIT           (1 << ZONEID_INIT)
#define ZONEMASK_ERRORS         (1 << ZONEID_ERRORS)
#define ZONEMASK_POWER          (1 << ZONEID_POWER)
#define ZONEMASK_EVENTS         (1 << ZONEID_EVENTS)
#define ZONEMASK_DISKIO         (1 << ZONEID_DISKIO)
#define ZONEMASK_APIS           (1 << ZONEID_APIS)
#define ZONEMASK_HELPER         (1 << ZONEID_HELPER)
#define ZONEMASK_STOREAPI       (1 << ZONEID_STOREAPI)
#define ZONEMASK_STOREMGR       (1 << ZONEID_STOREMGR)
#define ZONEMASK_VERBOSE        (1 << ZONEID_VERBOSE)
#define ZONEMASK_SECURITY       (1 << ZONEID_SECURITY)

#define ZONE_INIT               DEBUGZONE(ZONEID_INIT)
#define ZONE_ERRORS             DEBUGZONE(ZONEID_ERRORS)
#define ZONE_POWER              DEBUGZONE(ZONEID_POWER)
#define ZONE_EVENTS             DEBUGZONE(ZONEID_EVENTS)
#define ZONE_DISKIO             DEBUGZONE(ZONEID_DISKIO)
#define ZONE_APIS               DEBUGZONE(ZONEID_APIS)
#define ZONE_HELPER             DEBUGZONE(ZONEID_HELPER)
#define ZONE_STOREAPI           DEBUGZONE(ZONEID_STOREAPI) 
#define ZONE_STOREMGR           DEBUGZONE(ZONEID_STOREMGR)
#define ZONE_VERBOSE            DEBUGZONE(ZONEID_VERBOSE)
#define ZONE_SECURITY           DEBUGZONE(ZONEID_SECURITY)
#define ZONEMASK_DEFAULT        (ZONEMASK_ERRORS)

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
#define OWNCRITICALSECTION(cs)  TRUE

#endif // DEBUG

#endif // _FSDDBG_H_
