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
#ifdef DEBUG
#define DEBUGONLY(s)            s
#define RETAILONLY(s)           
#define VERIFYTRUE(c)           DEBUGCHK(c)
#define VERIFYNULL(c)           DEBUGCHK(!(c))
#else
#define DEBUGONLY(s)
#define RETAILONLY(s)           s
#define VERIFYTRUE(c)           c
#define VERIFYNULL(c)           c
#endif


/*  Debug-zone stuff
 */

#ifdef DEBUG

#define DEBUGBREAK(cond)         if (cond) DebugBreak(); else
#define DEBUGMSGBREAK(cond,msg)  if (cond) {DEBUGMSG(TRUE,msg); DebugBreak();} else
#define DEBUGMSGWBREAK(cond,msg) if (cond) {DEBUGMSGW(TRUE,msg); DebugBreak();} else
#else
#define DEBUGBREAK(cond)
#define DEBUGMSGBREAK(cond,msg)
#define DEBUGMSGWBREAK(cond,msg)
#endif

// Debug Zone Definitions
#ifdef DEBUG
// Zone Id 
#define ZONEID_INIT             0
#define ZONEID_DEINIT           1
#define ZONEID_MAIN             2
#define ZONEID_API              3
#define ZONEID_IO               4
#define ZONEID_HELPER           11
#define ZONEID_CURRENT          12
#define ZONEID_WARNING          14
#define ZONEID_ERROR            15

// Zones
#define ZONE_INIT               DEBUGZONE(ZONEID_INIT)
#define ZONE_DEINIT             DEBUGZONE(ZONEID_DEINIT)
#define ZONE_MAIN               DEBUGZONE(ZONEID_MAIN)
#define ZONE_API                DEBUGZONE(ZONEID_API)
#define ZONE_IO                 DEBUGZONE(ZONEID_IO)
#define ZONE_HELPER             DEBUGZONE(ZONEID_HELPER)
#define ZONE_WARNING            DEBUGZONE(ZONEID_WARNING)
#define ZONE_ERROR              DEBUGZONE(ZONEID_ERROR)
#define ZONE_CURRENT            DEBUGZONE(ZONEID_CURRENT)

/* zone masks */

#define ZONEMASK_INIT           ( 1 << ZONEID_INIT)
#define ZONEMASK_DEINIT         ( 1 << ZONEID_DEINIT)
#define ZONEMASK_MAIN           ( 1 << ZONEID_MAIN)
#define ZONEMASK_APIS           ( 1 << ZONEID_API)
#define ZONEMASK_IO             ( 1 << ZONEID_IO)
#define ZONEMASK_HELPER         ( 1 << ZONEID_HELPER)
#define ZONEMASK_CURRENT        ( 1 << ZONEID_CURRENT)
#define ZONEMASK_WARNING        ( 1 << ZONEID_WARNING)
#define ZONEMASK_ERROR          ( 1 << ZONEID_ERROR)

#endif

#ifdef DEBUG
void PrintROMHeader(ROMHDR *pRomHdr);
void PrintFileListing(LPVOID pv);
#define PRINTROMHEADER(r)       PrintROMHeader(r)
#define PRINTFILELISTING(pv)    PrintFileListing(pv)
#else
#define PRINTROMHEADER(r)
#define PRINTFILELISTING(pv)    
#endif



