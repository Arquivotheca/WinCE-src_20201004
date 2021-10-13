//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    dhcpv6dbg.h

Abstract:

    Common definitions for DHCPv6 Diagnosis



    FrancisD

Revision History:

--*/

#ifndef __DHCPV6DBG_H__
#define __DHCPV6DBG_H__

#define DHCPV6_LOG_LEVEL_FATAL   0
#define DHCPV6_LOG_LEVEL_ERROR   1
#define DHCPV6_LOG_LEVEL_WARN    2
#define DHCPV6_LOG_LEVEL_INFO    3
#define DHCPV6_LOG_LEVEL_TRACE   4
#define DHCPV6_LOG_LEVEL_NOISE   5

//
// Definitions for Software Tracing
//  DHCPV6_IOCTL: enable tracing for IOCTL activities
//  DHCPV6_PNP: enable tracing for PnP events
//
//  For packet processing (either send or receive), please
//  instrument it only on error path. Doing so on success
//  path won't give much useful information.
//
//#define WPP_CONTROL_GUIDS                                                   \
//    WPP_DEFINE_CONTROL_GUID(CtlGuid,(7B8E7B15,F0B0,4C3F,BF33,3B07175259AE), \
//        WPP_DEFINE_BIT(DHCPV6_IOCTL)                                        \
//        WPP_DEFINE_BIT(DHCPV6_PNP)                                          \
//        WPP_DEFINE_BIT(DHCPV6_WMI)                                          \
//        WPP_DEFINE_BIT(DHCPV6_ADAPT)                                        \
//        WPP_DEFINE_BIT(DHCPV6_EVENT)                                        \
//        WPP_DEFINE_BIT(DHCPV6_TIMER)                                        \
//        WPP_DEFINE_BIT(DHCPV6_OPTION)                                       \
//        WPP_DEFINE_BIT(DHCPV6_RECEIVE)                                      \
//        WPP_DEFINE_BIT(DHCPV6_SEND)                                         \
//        WPP_DEFINE_BIT(DHCPV6_MISC)                                         \
//    )

typedef struct DHCPV6_LOGHEXDUMP {
    USHORT usLength;
    PVOID pvBuffer;
} DHCPV6_LOGHEXDUMP, * PDHCPV6_LOGHEXDUMP;

//__inline DHCPV6_LOGHEXDUMP
//log_lenstr(ULONG len,char* buf)
//{
//    DHCPV6_LOGHEXDUMP xs ={(USHORT)len, buf};
//    return xs;
//}

#ifdef WPP_COMPID_LEVEL_ENABLED
#undef WPP_COMPID_LEVEL_ENABLED
#endif

#define WPP_COMPID_LEVEL_ENABLED(CTL,LEVEL)             \
    ((WPP_CONTROL(WPP_BIT_ ## CTL).Level >= LEVEL) &&   \
    (WPP_CONTROL(WPP_BIT_ ## CTL).Flags[WPP_FLAG_NO(WPP_BIT_ ## CTL)] & WPP_MASK(WPP_BIT_ ## CTL)))

#ifndef WPP_COMPID_LEVEL_LOGGER
#  define WPP_COMPID_LEVEL_LOGGER(CTL,LEVEL)      (WPP_CONTROL(WPP_BIT_ ## CTL).Logger),
#endif

#define WPP_LOGMACADDR(x)   WPP_LOGPAIR(6,x)

#define WPP_LOGDHCPV6HEXDUMP(x) WPP_LOGPAIR(2, &((x).usLength)) \
                            WPP_LOGPAIR((x).usLength, (x).pvBuffer)

#define WPP_LOGIPV6(x) WPP_LOGPAIR( (16), (x))


#endif // __DHCPV6DBG_H__
