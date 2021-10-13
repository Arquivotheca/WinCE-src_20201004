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

// --------------------------------------------------------------------------

#if ( !defined _DEBUG_H_ )
#define _DEBUG_H_

// --------------------------------------------------------------------------
// --- Debug Zones.

#ifndef UNDER_CE
extern int  DebugFlag;
#define DEBUGZONE( b )                  (DebugFlag&(0x00000001<<b))
#endif



#define PPP_ZONE_BPOS_ERROR             0
#define PPP_ZONE_BPOS_ALWAYS            0

#define PPP_ZONE_BPOS_WARN              1
#define PPP_ZONE_BPOS_FUNCTION          2
#define PPP_ZONE_BPOS_INIT              3
#define PPP_ZONE_BPOS_MAC               4
#define PPP_ZONE_BPOS_LCP               5
#define PPP_ZONE_BPOS_AUTH              6

#define PPP_ZONE_BPOS_NCP               7
#define PPP_ZONE_BPOS_CCP               7

#define PPP_ZONE_BPOS_IPCP              8
#define PPP_ZONE_BPOS_IPV6              8
#define PPP_ZONE_BPOS_IPV6CP            8

#define PPP_ZONE_BPOS_RAS               9
#define PPP_ZONE_BPOS_PPP               10
#define PPP_ZONE_BPOS_TIMING            11
#define PPP_ZONE_BPOS_TRACE             12
#define PPP_ZONE_BPOS_ALLOC             13
#define PPP_ZONE_BPOS_LOCK              14
#define PPP_ZONE_BPOS_REFCNT            15

#define ZONE_ERROR                      DEBUGZONE( PPP_ZONE_BPOS_ERROR    )
#define ZONE_ALWAYS                     DEBUGZONE( PPP_ZONE_BPOS_ALWAYS   )

#define ZONE_WARN                       DEBUGZONE( PPP_ZONE_BPOS_WARN     )
#define ZONE_FUNCTION                   DEBUGZONE( PPP_ZONE_BPOS_FUNCTION )
#define ZONE_INIT                       DEBUGZONE( PPP_ZONE_BPOS_INIT     )
#define ZONE_MAC                        DEBUGZONE( PPP_ZONE_BPOS_MAC      )
#define ZONE_LCP                        DEBUGZONE( PPP_ZONE_BPOS_LCP      )
#define ZONE_AUTH                       DEBUGZONE( PPP_ZONE_BPOS_AUTH     )

#define ZONE_NCP                        DEBUGZONE( PPP_ZONE_BPOS_NCP      )
#define ZONE_CCP                        DEBUGZONE( PPP_ZONE_BPOS_CCP      )

#define ZONE_IPCP                       DEBUGZONE( PPP_ZONE_BPOS_IPCP     )
#define ZONE_IPV6                       DEBUGZONE( PPP_ZONE_BPOS_IPV6     )
#define ZONE_IPV6CP                     DEBUGZONE( PPP_ZONE_BPOS_IPV6CP   )
                                                           
#define ZONE_RAS                        DEBUGZONE( PPP_ZONE_BPOS_RAS      )
#define ZONE_PPP                        DEBUGZONE( PPP_ZONE_BPOS_PPP      )
#define ZONE_TIMING                     DEBUGZONE( PPP_ZONE_BPOS_TIMING   )
#define ZONE_TRACE                      DEBUGZONE( PPP_ZONE_BPOS_TRACE    )
#define ZONE_ALLOC                      DEBUGZONE( PPP_ZONE_BPOS_ALLOC    )
#define ZONE_LOCK                       DEBUGZONE( PPP_ZONE_BPOS_LOCK     )
#define ZONE_REFCNT                     DEBUGZONE( PPP_ZONE_BPOS_REFCNT   )


// Not settable via PB
#define ZONE_VJ                         DEBUGZONE(18)       // 0x40000
#define ZONE_STATS                      DEBUGZONE(19)       // 0x80000
#define ZONE_PAP_DROP_RESPONSE          DEBUGZONE(25)       // 0x02000000
#define ZONE_IPHEX                      DEBUGZONE(30)       // 0x40000000
#define ZONE_IPHEADER                   DEBUGZONE(31)       // 0x80000000


#define PPP_SET_LOG_ERROR               ( 1 << PPP_ZONE_BPOS_ERROR    )
#define PPP_SET_LOG_ALWAYS              ( 1 << PPP_ZONE_BPOS_ALWAYS   )

#define PPP_SET_LOG_WARN                ( 1 << PPP_ZONE_BPOS_WARN     )
#define PPP_SET_LOG_FUNCTION            ( 1 << PPP_ZONE_BPOS_FUNCTION )
#define PPP_SET_LOG_INIT                ( 1 << PPP_ZONE_BPOS_INIT     )
#define PPP_SET_LOG_MAC                 ( 1 << PPP_ZONE_BPOS_MAC      )
#define PPP_SET_LOG_LCP                 ( 1 << PPP_ZONE_BPOS_LCP      )
#define PPP_SET_LOG_AUTH                ( 1 << PPP_ZONE_BPOS_AUTH     )

#define PPP_SET_LOG_NCP                 ( 1 << PPP_ZONE_BPOS_NCP      )
#define PPP_SET_LOG_CCP                 ( 1 << PPP_ZONE_BPOS_CCP      )

#define PPP_SET_LOG_IPCP                ( 1 << PPP_ZONE_BPOS_IPCP     )
#define PPP_SET_LOG_IPV6                ( 1 << PPP_ZONE_BPOS_IPV6     )
#define PPP_SET_LOG_IPV6CP              ( 1 << PPP_ZONE_BPOS_IPV6CP   )

#define PPP_SET_LOG_RAS                 ( 1 << PPP_ZONE_BPOS_RAS      )
#define PPP_SET_LOG_PPP                 ( 1 << PPP_ZONE_BPOS_PPP      )
#define PPP_SET_LOG_TIMING              ( 1 << PPP_ZONE_BPOS_TIMING   )
#define PPP_SET_LOG_TRACE               ( 1 << PPP_ZONE_BPOS_TRACE    )
#define PPP_SET_LOG_ALLOC               ( 1 << PPP_ZONE_BPOS_ALLOC    )
#define PPP_SET_LOG_LOCK                ( 1 << PPP_ZONE_BPOS_LOCK     )
#define PPP_SET_LOG_REFCNT              ( 1 << PPP_ZONE_BPOS_REFCNT   )


#define PPP_SET_LOG_DEFAULT             \
        ( PPP_SET_LOG_ERROR | PPP_SET_LOG_ALWAYS | PPP_SET_LOG_WARN )



// --------------------------------------------------------------------------
// --- Utility logging macros

#define PPP_LOG_FN_ENTER()                              \
        RETAILMSG( ZONE_FUNCTION,                       \
                   ( TEXT( "PPP: (%hs) - ==>\r\n" ),    \
                   __FUNCTION__  ) );

#define PPP_LOG_FN_LEAVE( _Sts )                                \
        RETAILMSG( ZONE_FUNCTION,                               \
                   ( TEXT( "PPP: (%hs) - <== Sts: %u\r\n" ),    \
                   __FUNCTION__, _Sts  ) );

#define PPP_LOG_FN_ENTER_FORCE()                        \
        RETAILMSG( ZONE_ALWAYS,                         \
                   ( TEXT( "PPP: (%hs) - ==>\r\n" ),    \
                   __FUNCTION__  ) );

#define PPP_LOG_FN_LEAVE_FORCE( _Sts )                          \
        RETAILMSG( ZONE_ALWAYS,                                 \
                   ( TEXT( "PPP: (%hs) - <== Sts: %u\r\n" ),    \
                   __FUNCTION__, _Sts  ) );


// --------------------------------------------------------------------------
// --- Debug logging global control struct

extern DBGPARAM dpCurSettings;

// --------------------------------------------------------------------------

#endif  // ( !defined _DEBUG_H_ )

// --------------------------------------------------------------------------
// --- End of file ---

