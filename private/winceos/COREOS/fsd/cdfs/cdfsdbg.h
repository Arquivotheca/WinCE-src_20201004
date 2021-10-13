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
//+-------------------------------------------------------------------------
//
//
//  File:       udfsdbg.h
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------
#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef DEBUG

#define ZONEID_ERROR       0
#define ZONEID_WARNING     1
#define ZONEID_FUNCTION    2
#define ZONEID_INIT        3
#define ZOINEID_IO         4
#define ZONEID_APIS        5
#define ZONEID_HANDLES     6
#define ZONEID_MEDIA       7
#define ZONEID_ALLOC       8
#define ZONEID_CURRENT     14
#define ZONEID_INITBREAK   15 

//
// Debug zones
//
#define ZONE_ERROR         DEBUGZONE(ZONEID_ERROR)
#define ZONE_WARNING       DEBUGZONE(ZONEID_WARNING)
#define ZONE_FUNCTION      DEBUGZONE(ZONEID_FUNCTION)
#define ZONE_INIT          DEBUGZONE(ZONEID_INIT)
#define ZONE_IO            DEBUGZONE(ZOINEID_IO)
#define ZONE_APIS          DEBUGZONE(ZONEID_APIS)
#define ZONE_HANDLES       DEBUGZONE(ZONEID_HANDLES)
#define ZONE_MEDIA         DEBUGZONE(ZONEID_MEDIA)
#define ZONE_ALLOC         DEBUGZONE(ZONEID_ALLOC)
#define ZONE_CURRENT       DEBUGZONE(ZONEID_CURRENT)
#define ZONE_INITBREAK     DEBUGZONE(ZONEID_INITBREAK)


#define ZONEMASK_ERROR      ( 1 << ZONEID_ERROR)
#define ZONEMASK_WARNING    ( 1 << ZONEID_WARNING)
#define ZONEMASK_FUNCTION   ( 1 << ZONEID_FUNCTION)
#define ZONEMASK_INIT       ( 1 << ZONEID_INIT)
#define ZONEMASK_IO         ( 1 << ZOINEID_IO)
#define ZONEMASK_APIS       ( 1 << ZONEID_APIS)
#define ZONEMASK_HANDLES    ( 1 << ZONEID_HANDLES)
#define ZONEMASK_MEDIA      ( 1 << ZONEID_MEDIA)
#define ZONEMASK_ALLOC      ( 1 << ZONEID_ALLOC)
#define ZONEMASK_CURRENT    ( 1 << ZONEID_CURRENT)
#define ZONEMASK_INITBREAK  ( 1 << ZONEID_INITBREAK)


void DumpRegKey( DWORD dwZone, PTSTR szKey, HKEY hKey);
#define DUMPREGKEY(dwZone, szKey, hKey) DumpRegKey(dwZone, szKey, hKey)

#else
#define DUMPREGKEY(dwZone, szKey, hKey)
#endif  // DEBUG

#endif //_DEBUG_H_



