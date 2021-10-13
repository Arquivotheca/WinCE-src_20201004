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
/**********************************************************************/
/**                        Microsoft Windows                         **/
/**********************************************************************/

/*
    wscntl.h

    private include file for the WsControl API in the Windows Sockets implementation.


*/


#ifndef _WSCNTL_H_
#define _WSCNTL_H_

//
// Type structure definitions.
//

typedef struct HOST_CACHE_ENTRY {
    size_t cbName;
    DWORD cbNameOffset;   // Offset relative to start of struct array, not individual struct
    FILETIME ftExpireTime;
    DWORD cAddrs;
    DWORD cbAddrLen;  // The length of 1 address, not total
    DWORD cbAddrsOffset;  // Offset relative to start of struct array, not individual struct
    // Aliases are a double NULL terminated list of NULL terminated strings
    DWORD cbAliases;
    DWORD cbAliasesOffset;   // Offset relative to start of struct array, not individual struct
    INT TTL;
    BOOL bV6Entry;
    BOOL bNegativeEntry;
    BOOL bManagedResponse;
    ULONG InterfaceIndex;
} HOST_CACHE_ENTRY;

typedef struct HOST_CACHE {
    DWORD cEntries;
    HOST_CACHE_ENTRY pHostCacheEntry[1];
} HOST_CACHE;

//
//  Function prototypes.
//

DWORD
FAR PASCAL
WsControl(
    DWORD   Protocol,
    DWORD   Action,
    LPVOID  InputBuffer,
    LPDWORD InputBufferLength,
    LPVOID  OutputBuffer,
    LPDWORD OutputBufferLength
    );

typedef DWORD (FAR PASCAL * LPWSCONTROL)( DWORD   Protocol,
                                          DWORD   Action,
                                          LPVOID  InputBuffer,
                                          LPDWORD InputBufferLength,
                                          LPVOID  OutputBuffer,
                                          LPDWORD OutputBufferLength );


//
//  Ws Control action codes.
//

#define WSCNTL_TCPIP_QUERY_INFO             0x00000000
#define WSCNTL_TCPIP_SET_INFO               0x00000001
#define WSCNTL_TCPIP_ICMP_ECHO              0x00000002
#define WSCNTL_TCPIP_TEST                   0x00000003
#define WSCNTL_AFD_INFO                     0x00000004
#define WSCNTL_AFD_GATHER_RAND              0x00000005
#define WSCNTL_AFD_FLUSH_RESOLVER_CACHE     0x00000006
#define WSCNTL_AFD_QUERY_RESOLVER_CACHE     0x00000007

// For WSCNTL_AFD_INFO
// All selections must fit in the mask.
#define WSCNTL_AFD_INFO_MASK                0x0000ffff
#define WSCNTL_AFD_INFO_LOCK                0x00000001
#define WSCNTL_AFD_INFO_SOCK                0x00000002
#define WSCNTL_AFD_INFO_CONN                0x00000004
#define WSCNTL_AFD_INFO_ENDP                0x00000008
#define WSCNTL_AFD_INFO_BUFFER              0x00000010
#define WSCNTL_AFD_IRDA_INFO                0x00000020
#define WSCNTL_AFD_CXPORT_INFO              0x00000040
#define WSCNTL_AFD_SET_MACHINE_NAME         0x00000080

// Option bits.
#define WSCNTL_AFD_INFO_VERBOSE             0x80000000


#endif  // _WSCNTL_H_


