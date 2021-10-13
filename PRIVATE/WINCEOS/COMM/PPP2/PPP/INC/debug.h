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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*****************************************************************************
* 
*
*   @doc
*   @module debug.h | NCP Header File
*
*   Date: 1-8-96
*
*   @comm
*/


#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG

// Debug Zones.

#ifndef UNDER_CE
    extern int  DebugFlag;
#   define DEBUGZONE( b )    (DebugFlag&(0x00000001<<b))
#endif

#   define ZONE_INIT        DEBUGZONE(0)        // 0x00001
#   define ZONE_MAC         DEBUGZONE(1)        // 0x00002
#   define ZONE_LCP         DEBUGZONE(2)        // 0x00004
#   define ZONE_AUTH        DEBUGZONE(3)        // 0x00008
#   define ZONE_NCP         DEBUGZONE(4)        // 0x00010
#   define ZONE_CCP         DEBUGZONE(4)        // 0x00010
#   define ZONE_IPCP        DEBUGZONE(5)        // 0x00020
#   define ZONE_IPV6        DEBUGZONE(5)        // 0x00020
#   define ZONE_IPV6CP      DEBUGZONE(5)        // 0x00020
#   define ZONE_RAS         DEBUGZONE(6)        // 0x00040
#   define ZONE_PPP         DEBUGZONE(7)        // 0x00080
#   define ZONE_TIMING      DEBUGZONE(8)        // 0x00100
#   define ZONE_TRACE       DEBUGZONE(9)        // 0x00200
#   define ZONE_UNUSED10    DEBUGZONE(10)       // 0x00400
#   define ZONE_UNUSED11    DEBUGZONE(11)       // 0x00800
#   define ZONE_ALLOC       DEBUGZONE(12)       // 0x01000
#   define ZONE_FUNCTION    DEBUGZONE(13)       // 0x02000
#   define ZONE_WARN        DEBUGZONE(14)       // 0x04000
#   define ZONE_ERROR       DEBUGZONE(15)       // 0x08000
#   define ZONE_LOCK        DEBUGZONE(16)       // 0x10000
#   define ZONE_REFCNT      DEBUGZONE(17)       // 0x20000
#   define ZONE_VJ          DEBUGZONE(18)       // 0x40000
#   define ZONE_STATS       DEBUGZONE(19)       // 0x80000
#   define ZONE_PAP_DROP_RESPONSE DEBUGZONE(25) // 0x02000000
#   define ZONE_IPHEX       DEBUGZONE(30)       // 0x40000000
#   define ZONE_IPHEADER    DEBUGZONE(31)       // 0x80000000

#endif 

#endif
