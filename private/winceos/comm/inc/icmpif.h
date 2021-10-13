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
/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/********************************************************************/
/* :ts=4 */

//** ICMPIF.H - ICMP echo private kernel/user request interface
//

#ifndef	ICMPIF_INCLUDED
#define	ICMPIF_INCLUDED


//
// Common ICMP request structure
//
typedef struct icmp_echo_request {
    unsigned long         Address;          // Destination address
    unsigned long         Timeout;          // Request timeout
    unsigned short        DataOffset;       // Echo data
    unsigned short        DataSize;         // Echo data size
    unsigned char         OptionsValid;     // nonzero if options data is valid.
    unsigned char         Ttl;              // IP header Time To Live
    unsigned char         Tos;              // IP header Type of Service
    unsigned char         Flags;            // IP header flags
    unsigned short        OptionsOffset;    // IP options data
    unsigned char         OptionsSize;      // IP options data size
    unsigned char         Padding;          // 32-bit alignment padding
} ICMP_ECHO_REQUEST, *PICMP_ECHO_REQUEST;


//
// The reply structure is defined in ipexport.h
//

#endif // ICMPIF_INCLUDED
