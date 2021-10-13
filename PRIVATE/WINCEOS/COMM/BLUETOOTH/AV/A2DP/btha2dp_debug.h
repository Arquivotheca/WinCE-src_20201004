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
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


#ifndef _BTHA2DP_DEBUG_H_
#define _BTHA2DP_DEBUG_H_
#include <wavedbg.h>

#define ZONEID_ERROR            15
#define ZONEID_INFO             1
#define ZONEID_TRACE            2
#define ZONEID_FUNCTION         3
#define ZONEID_VERBOSE          7
#define ZONEID_CODEC_TRACE      5
#define ZONEID_CODEC_VERBOSE    6


#define ZONEMASK_ERROR              (1<<ZONEID_ERROR)
#define ZONEMASK_INFO               (1<<ZONEID_INFO)
#define ZONEMASK_TRACE              (1<<ZONEID_TRACE)
#define ZONEMASK_FUNCTION           (1<<ZONEID_FUNCTION)
#define ZONEMASK_VERBOSE            (1<<ZONEID_VERBOSE)
#define ZONEMASK_CODEC_TRACE        (1<<ZONEID_CODEC_TRACE)
#define ZONEMASK_CODEC_VERBOSE      (1<<ZONEID_CODEC_VERBOSE)


#ifdef DEBUG
// These macros are used as the first arg to DEBUGMSG.
#define ZONE_A2DP_ERROR                  ZONE_ERROR
#define ZONE_A2DP_INFO                   DEBUGZONE(ZONEID_INFO)
#define ZONE_A2DP_TRACE                  ZONE_MISC
#ifndef ZONE_FUNCTION
#define ZONE_FUNCTION                    DEBUGZONE(ZONEID_FUNCTION)
#endif
#define ZONE_A2DP_VERBOSE                DEBUGZONE(ZONEID_VERBOSE)
#define ZONE_A2DP_CODEC_TRACE            DEBUGZONE(ZONEID_CODEC_TRACE)
#define ZONE_A2DP_CODEC_VERBOSE          DEBUGZONE(ZONEID_CODEC_VERBOSE)


#else
// For release configurations, these conditionals are always 0.
#define ZONE_A2DP_ERROR                  0
#define ZONE_A2DP_INFO                   0
#define ZONE_A2DP_TRACE                  0
#ifndef ZONE_FUNCTION
#define ZONE_FUNCTION                    0
#endif
#define ZONE_A2DP_VERBOSE                0
#define ZONE_A2DP_CODEC_TRACE            0
#define ZONE_A2DP_CODEC_VERBOSE          0
#endif


#endif // _CSPVOICE_DEBUG_H_
