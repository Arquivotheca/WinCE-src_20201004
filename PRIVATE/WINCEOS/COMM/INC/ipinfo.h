//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/********************************************************************/
/* :ts=4 */

//** IPINFO.H - IP SNMP information definitions..
//
// This file contains all of the definitions for IP that are
// related to SNMP information gathering.

#ifndef IPINFO_INCLUDED
#define IPINFO_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef CTE_TYPEDEFS_DEFINED
#define CTE_TYPEDEFS_DEFINED

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned int uint;

#endif // CTE_TYPEDEFS_DEFINED


typedef struct IPSNMPInfo {
    ulong       ipsi_forwarding;
    ulong       ipsi_defaultttl;
    ulong       ipsi_inreceives;
    ulong       ipsi_inhdrerrors;
    ulong       ipsi_inaddrerrors;
    ulong       ipsi_forwdatagrams;
    ulong       ipsi_inunknownprotos;
    ulong       ipsi_indiscards;
    ulong       ipsi_indelivers;
    ulong       ipsi_outrequests;
    ulong       ipsi_routingdiscards;
    ulong       ipsi_outdiscards;
    ulong       ipsi_outnoroutes;
    ulong       ipsi_reasmtimeout;
    ulong       ipsi_reasmreqds;
    ulong       ipsi_reasmoks;
    ulong       ipsi_reasmfails;
    ulong       ipsi_fragoks;
    ulong       ipsi_fragfails;
    ulong       ipsi_fragcreates;
    ulong       ipsi_numif;
    ulong       ipsi_numaddr;
    ulong       ipsi_numroutes;
} IPSNMPInfo;

typedef struct ICMPStats {
    ulong       icmps_msgs;
    ulong       icmps_errors;
    ulong       icmps_destunreachs;
    ulong       icmps_timeexcds;
    ulong       icmps_parmprobs;
    ulong       icmps_srcquenchs;
    ulong       icmps_redirects;
    ulong       icmps_echos;
    ulong       icmps_echoreps;
    ulong       icmps_timestamps;
    ulong       icmps_timestampreps;
    ulong       icmps_addrmasks;
    ulong       icmps_addrmaskreps;
} ICMPStats;

typedef struct ICMPSNMPInfo {
    ICMPStats   icsi_instats;
    ICMPStats   icsi_outstats;
} ICMPSNMPInfo;

#define IP_FORWARDING       1
#define IP_NOT_FORWARDING   2

typedef struct IPAddrEntry {
    ulong       iae_addr;
    ulong       iae_index;
    ulong       iae_mask;
    ulong       iae_bcastaddr;
    ulong       iae_reasmsize;
    ushort      iae_context;
    ushort      iae_pad;
} IPAddrEntry;

typedef struct IPRouteEntry {
    ulong       ire_dest;
    ulong       ire_index;
    ulong       ire_metric1;
    ulong       ire_metric2;
    ulong       ire_metric3;
    ulong       ire_metric4;
    ulong       ire_nexthop;
    ulong       ire_type;
    ulong       ire_proto;
    ulong       ire_age;
    ulong       ire_mask;
    ulong       ire_metric5;
#if defined(NT) || defined (UNDER_CE)
    ulong       ire_context;
#endif
} IPRouteEntry;

typedef struct IPRouteBlock {
    ulong       numofroutes;
    IPRouteEntry route[1];
} IPRouteBlock;

//
// Route with multiple nexthops and associated defns
//

typedef struct IPRouteNextHopEntry {
    ulong       ine_iretype;
    ulong       ine_nexthop;
    ulong       ine_ifindex;
#if defined(NT) || defined (UNDER_CE)
    ulong       ine_context;
#endif
} IPRouteNextHopEntry;


typedef struct IPMultihopRouteEntry {
    ulong               imre_numnexthops;
    ulong               imre_flags;
    IPRouteEntry        imre_routeinfo;
    IPRouteNextHopEntry imre_morenexthops[1];
} IPMultihopRouteEntry;

#define IMRE_FLAG_DELETE_DEST   0x00000001

//
// Input context to pass when querying a route
//

typedef enum {
    IPNotifyNotification = 0,
    IPNotifySynchronization,
    IPNotifyMaximumVersion
} IPNotifyVersion;

typedef struct IPNotifyData {
    ulong       Version;   // See IPNotifyVersion above.
    ulong       Add;
    char        Info[1];
} IPNotifyData, *PIPNotifyData;

typedef struct IPNotifyOutput {
    ulong       ino_addr;
    ulong       ino_mask;
    ulong       ino_info[6];
} IPNotifyOutput, *PIPNotifyOutput;

typedef union IPRouteNotifyOutput {
    IPNotifyOutput irno_info;
    struct {
        ulong   irno_dest;
        ulong   irno_mask;
        ulong   irno_nexthop;
        ulong   irno_proto;
        ulong   irno_ifindex;
        ulong   irno_metric;
        ulong   irno_flags;
    };
} IPRouteNotifyOutput, *PIPRouteNotifyOutput;

#define IRNO_FLAG_ADD       0x00000001
#define IRNO_FLAG_DELETE    0x00000002

//
// Input context to pass when querying a route
//
typedef struct IPRouteLookupData {
    ulong       Version;   //version of this structure
    ulong       DestAdd;
    ulong       SrcAdd;
    char        Info[1];
} IPRouteLookupData, *PIPRouteLookupData;

typedef struct AddrXlatInfo {
    ulong       axi_count;
    ulong       axi_index;
} AddrXlatInfo;

#define IRE_TYPE_OTHER          1
#define IRE_TYPE_INVALID        2
#define IRE_TYPE_DIRECT         3
#define IRE_TYPE_INDIRECT       4

#define IRE_PROTO_OTHER         1
#define IRE_PROTO_LOCAL         2
#define IRE_PROTO_NETMGMT       3
#define IRE_PROTO_ICMP          4
#define IRE_PROTO_EGP           5
#define IRE_PROTO_GGP           6
#define IRE_PROTO_HELLO         7
#define IRE_PROTO_RIP           8
#define IRE_PROTO_IS_IS         9
#define IRE_PROTO_ES_IS         10
#define IRE_PROTO_CISCO         11
#define IRE_PROTO_BBN           12
#define IRE_PROTO_OSPF          13
#define IRE_PROTO_BGP           14
#define IRE_PROTO_PERSIST_LOCAL 10010

#define IRE_METRIC_UNUSED       0xffffffff

#define IP_MIB_STATS_ID                 1
#define IP_MIB_RTCHANGE_NOTIFY_ID       2
#define ICMP_MIB_STATS_ID               1

#define AT_MIB_ADDRXLAT_INFO_ID         1
#define AT_MIB_ADDRXLAT_ENTRY_ID        0x101

#define IP_MIB_RTTABLE_ENTRY_ID         0x101
#define IP_MIB_ADDRTABLE_ENTRY_ID       0x102
#define IP_MIB_RTTABLE_ENTRY_ID_EX      0x103

#define IP_INTFC_FLAG_P2P                 1
#define IP_INTFC_FLAG_P2MP                2
#define IP_INTFC_FLAG_UNIDIRECTIONAL      4


typedef struct IPInterfaceInfo {
    ulong       iii_flags;
    ulong       iii_mtu;
    ulong       iii_speed;
    ulong       iii_addrlength;
    uchar       iii_addr[1];
} IPInterfaceInfo;

#define IP_INTFC_INFO_ID                0x103
#define IP_MIB_SINGLE_RT_ENTRY_ID       0x104
#define IP_GET_BEST_SOURCE              0x105

#ifndef s6_addr

struct in6_addr {
    union {
        unsigned char Byte[16];
        unsigned short Word[8];
    } u;
};

#define in_addr6 in6_addr

/*
** Defines to match RFC 2553.
*/
#define _S6_un     u
#define _S6_u8     Byte
#define s6_addr    _S6_un._S6_u8

/*
** Defines for our implementation.
*/
#define s6_bytes   u.Byte
#define s6_words   u.Word

#endif

/* IPv6 socket address structure, RFC 2553 */

struct sockaddr_in6 {
    short	sin6_family;   	        /* AF_INET6 */
    unsigned short sin6_port;       /* Transport level port number */
    unsigned long sin6_flowinfo;    /* IPv6 flow information */
    struct in6_addr sin6_addr;      /* IPv6 address */
    unsigned long sin6_scope_id;    /* set of interfaces for a scope */
};


typedef struct in6_addr IN6_ADDR;
typedef struct in6_addr *PIN6_ADDR;
typedef struct in6_addr FAR *LPIN6_ADDR;

typedef struct sockaddr_in6 SOCKADDR_IN6;
typedef struct sockaddr_in6 *PSOCKADDR_IN6;
typedef struct sockaddr_in6 FAR *LPSOCKADDR_IN6;


typedef struct IP6RouteEntry {
    ulong           ire_Length;
    struct in6_addr ire_Source;
    ulong           ire_ScopeId;
    ulong           ire_IfIndex;
} IP6RouteEntry;

#define IP6_GET_BEST_ROUTE_ID  3

/* Structure used in getaddrinfo() call */
typedef struct addrinfo {
    int ai_flags;              /* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
    int ai_family;             /* PF_xxx */
    int ai_socktype;           /* SOCK_xxx */
    int ai_protocol;           /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
    size_t ai_addrlen;         /* Length of ai_addr */
    char *ai_canonname;        /* Canonical name for nodename */
    struct sockaddr *ai_addr;  /* Binary address */
    struct addrinfo *ai_next;  /* Next structure in linked list */
} ADDRINFO, FAR * LPADDRINFO;

/* Flags used in "hints" argument to getaddrinfo() */
#define AI_PASSIVE     0x1  /* Socket address will be used in bind() call */
#define AI_CANONNAME   0x2  /* Return canonical name in first ai_canonname */
#define AI_NUMERICHOST 0x4  /* Nodename must be a numeric address string */

int
getaddrinfo(
    IN const char FAR * nodename,
    IN const char FAR * servname,
    IN const struct addrinfo FAR * hints,
    OUT struct addrinfo FAR * FAR * res
    );

void
freeaddrinfo(
    IN struct addrinfo FAR * ai
    );

typedef
int
(* LPFN_GETADDRINFO)(
    IN const char FAR * nodename,
    IN const char FAR * servname,
    IN const struct addrinfo FAR * hints,
    OUT struct addrinfo FAR * FAR * res
    );

typedef
void
(* LPFN_FREEADDRINFO)(
    IN struct addrinfo FAR * ai
    );

typedef struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;  /* IPv6 multicast address */
    unsigned int    ipv6mr_interface;  /* Interface index */
} IPV6_MREQ;

#define IPPROTO_IPV6    41

/* Options to use with [gs]etsockopt at the IPPROTO_IPV6 level */

#define IPV6_UNICAST_HOPS       4  /* Set/get IP unicast hop limit */
#define IPV6_MULTICAST_IF       9  /* Set/get IP multicast interface */
#define IPV6_MULTICAST_HOPS     10 /* Set/get IP multicast ttl */
#define IPV6_MULTICAST_LOOP     11 /* Set/get IP multicast loopback */
#define IPV6_ADD_MEMBERSHIP     12 /* Add an IP group membership */
#define IPV6_DROP_MEMBERSHIP    13 /* Drop an IP group membership */
#define IPV6_JOIN_GROUP         IPV6_ADD_MEMBERSHIP
#define IPV6_LEAVE_GROUP        IPV6_DROP_MEMBERSHIP

#endif // IPINFO_INCLUDED
