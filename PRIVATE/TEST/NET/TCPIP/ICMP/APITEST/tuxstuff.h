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
#ifndef __TUXSTUFF_H__
#define __TUXSTUFF_H__

#include "IcmpTest.h"

#define USE_IPv6					   0x40000000
#define USE_ASYNC					   0x80000000

// Test function prototypes (TestProc's)
TESTPROCAPI IcmpAPITest						(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

#define ICMP_SEND_ECHO		0

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
TEXT(   "ICMP API Tests"                                     ), 1,									0,					0, NULL,
TEXT(   "IcmpSendEcho() Tests"                               ), 2,									0,					0, NULL,
TEXT(         "IPV4_ICMP_INVALID_HANDLE"                     ), 3,                 ICMP_INVALID_HANDLE, ICMP_SEND_ECHO+  1, IcmpAPITest,
TEXT(         "IPV4_ICMP_NULL_ADDR"						     ), 3,				  	    ICMP_NULL_ADDR, ICMP_SEND_ECHO+  2, IcmpAPITest,
TEXT(         "IPV4_ICMP_NULL_SEND_BUFFER"                   ), 3,	 			 ICMP_NULL_SEND_BUFFER, ICMP_SEND_ECHO+  3, IcmpAPITest,
TEXT(         "IPV4_ICMP_SEND_BUFFER_FRAG"                   ), 3, 				 ICMP_SEND_BUFFER_FRAG, ICMP_SEND_ECHO+  4, IcmpAPITest,
TEXT(         "IPV4_ICMP_SEND_BUFFER_NO_FRAG"                ), 3,			  ICMP_SEND_BUFFER_NO_FRAG, ICMP_SEND_ECHO+  5, IcmpAPITest,
TEXT(         "IPV4_ICMP_SEND_BUFFER_TOO_SMALL"              ), 3,          ICMP_SEND_BUFFER_TOO_SMALL, ICMP_SEND_ECHO+  6, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_SEND_LEN"                   ), 3,               ICMP_INVALID_SEND_LEN, ICMP_SEND_ECHO+  7, IcmpAPITest,
TEXT(         "IPV4_ICMP_NULL_REPLY_BUFFER"                  ), 3,              ICMP_NULL_REPLY_BUFFER, ICMP_SEND_ECHO+  8, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_REPLY_LENGTH"               ), 3,           ICMP_INVALID_REPLY_LENGTH, ICMP_SEND_ECHO+  9, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_SEND_OPTIONS"               ), 3,           ICMP_INVALID_SEND_OPTIONS, ICMP_SEND_ECHO+ 10, IcmpAPITest,
TEXT(         "IPV4_ICMP_INVALID_TIMEOUT"                    ), 3,                ICMP_INVALID_TIMEOUT, ICMP_SEND_ECHO+ 11, IcmpAPITest,
TEXT(         "IPV4_ICMP_UNREACHABLE_DESTINATION"            ), 3,            ICMP_UNREACH_DESTINATION, ICMP_SEND_ECHO+ 12, IcmpAPITest,
TEXT(         "IPV4_ICMP_VALID_REQUEST"                      ), 3,                  ICMP_VALID_REQUEST, ICMP_SEND_ECHO+ 13, IcmpAPITest,
TEXT(   "Icmp6SendEcho2() Tests"                             ), 2,									0,					 0, NULL,
TEXT(         "IPV6_ICMP_INVALID_HANDLE"                     ), 3,USE_IPv6 |       ICMP_INVALID_HANDLE, ICMP_SEND_ECHO+ 101, IcmpAPITest,
TEXT(         "IPV6_ICMP_NULL_ADDR"						     ), 3,USE_IPv6 |            ICMP_NULL_ADDR, ICMP_SEND_ECHO+ 102, IcmpAPITest,
TEXT(         "IPV6_ICMP_NULL_SEND_BUFFER"                   ), 3,USE_IPv6 |     ICMP_NULL_SEND_BUFFER, ICMP_SEND_ECHO+ 103, IcmpAPITest,
TEXT(         "IPV6_ICMP_SEND_BUFFER_FRAG"                   ), 3,USE_IPv6 |     ICMP_SEND_BUFFER_FRAG, ICMP_SEND_ECHO+ 104, IcmpAPITest,
TEXT(         "IPV6_ICMP_SEND_BUFFER_NO_FRAG"                ), 3,USE_IPv6 |  ICMP_SEND_BUFFER_NO_FRAG, ICMP_SEND_ECHO+ 105, IcmpAPITest,
TEXT(         "IPV6_ICMP_SEND_BUFFER_TOO_SMALL"              ), 3,USE_IPv6 |ICMP_SEND_BUFFER_TOO_SMALL, ICMP_SEND_ECHO+ 106, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_SEND_LEN"                   ), 3,USE_IPv6 |     ICMP_INVALID_SEND_LEN, ICMP_SEND_ECHO+ 107, IcmpAPITest,
TEXT(         "IPV6_ICMP_NULL_REPLY_BUFFER"                  ), 3,USE_IPv6 |    ICMP_NULL_REPLY_BUFFER, ICMP_SEND_ECHO+ 108, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_REPLY_LENGTH"               ), 3,USE_IPv6 | ICMP_INVALID_REPLY_LENGTH, ICMP_SEND_ECHO+ 109, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_SEND_OPTIONS"               ), 3,USE_IPv6 | ICMP_INVALID_SEND_OPTIONS, ICMP_SEND_ECHO+ 110, IcmpAPITest,
TEXT(         "IPV6_ICMP_INVALID_TIMEOUT"                    ), 3,USE_IPv6 |      ICMP_INVALID_TIMEOUT, ICMP_SEND_ECHO+ 111, IcmpAPITest,
TEXT(         "IPV6_ICMP_UNREACHABLE_DESTINATION"            ), 3,USE_IPv6 |  ICMP_UNREACH_DESTINATION, ICMP_SEND_ECHO+ 112, IcmpAPITest,
TEXT(         "IPV6_ICMP_VALID_REQUEST"                      ), 3,USE_IPv6 |        ICMP_VALID_REQUEST, ICMP_SEND_ECHO+ 113, IcmpAPITest,
TEXT(   "IcmpSendEcho2() Tests"                              ), 2,									0,					 0, NULL,
TEXT(         "ASYNC_ICMP_INVALID_HANDLE"                    ), 3,USE_ASYNC |       ICMP_INVALID_HANDLE, ICMP_SEND_ECHO+ 201, IcmpAPITest,
TEXT(         "ASYNC_ICMP_NULL_ADDR"						 ), 3,USE_ASYNC |            ICMP_NULL_ADDR, ICMP_SEND_ECHO+ 202, IcmpAPITest,
TEXT(         "ASYNC_ICMP_NULL_SEND_BUFFER"                  ), 3,USE_ASYNC |     ICMP_NULL_SEND_BUFFER, ICMP_SEND_ECHO+ 203, IcmpAPITest,
TEXT(         "ASYNC_ICMP_SEND_BUFFER_FRAG"                  ), 3,USE_ASYNC |     ICMP_SEND_BUFFER_FRAG, ICMP_SEND_ECHO+ 204, IcmpAPITest,
TEXT(         "ASYNC_ICMP_SEND_BUFFER_NO_FRAG"               ), 3,USE_ASYNC |  ICMP_SEND_BUFFER_NO_FRAG, ICMP_SEND_ECHO+ 205, IcmpAPITest,
TEXT(         "ASYNC_ICMP_SEND_BUFFER_TOO_SMALL"             ), 3,USE_ASYNC |ICMP_SEND_BUFFER_TOO_SMALL, ICMP_SEND_ECHO+ 206, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_SEND_LEN"                  ), 3,USE_ASYNC |     ICMP_INVALID_SEND_LEN, ICMP_SEND_ECHO+ 207, IcmpAPITest,
TEXT(         "ASYNC_ICMP_NULL_REPLY_BUFFER"                 ), 3,USE_ASYNC |    ICMP_NULL_REPLY_BUFFER, ICMP_SEND_ECHO+ 208, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_REPLY_LENGTH"              ), 3,USE_ASYNC | ICMP_INVALID_REPLY_LENGTH, ICMP_SEND_ECHO+ 209, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_SEND_OPTIONS"              ), 3,USE_ASYNC | ICMP_INVALID_SEND_OPTIONS, ICMP_SEND_ECHO+ 210, IcmpAPITest,
TEXT(         "ASYNC_ICMP_INVALID_TIMEOUT"                   ), 3,USE_ASYNC |      ICMP_INVALID_TIMEOUT, ICMP_SEND_ECHO+ 211, IcmpAPITest,
TEXT(         "ASYNC_ICMP_UNREACHABLE_DESTINATION"           ), 3,USE_ASYNC |  ICMP_UNREACH_DESTINATION, ICMP_SEND_ECHO+ 212, IcmpAPITest,
TEXT(         "ASYNC_ICMP_VALID_REQUEST"                     ), 3,USE_ASYNC |        ICMP_VALID_REQUEST, ICMP_SEND_ECHO+ 213, IcmpAPITest,
TEXT(   "IcmpParseReplies() Tests"                           ), 2,									0,					 0, NULL,
TEXT(         "PARSE4_INVALID_BUFFER"                        ), 3,					ICMP_INVALID_BUFFER, ICMP_SEND_ECHO+ 301, IcmpAPITest,
TEXT(         "PARSE4_INVALID_LEN"						     ), 3,					   ICMP_INVALID_LEN, ICMP_SEND_ECHO+ 302, IcmpAPITest,
TEXT(         "PARSE4_VALID_PARAMS"                          ), 3,                    ICMP_VALID_PARAMS, ICMP_SEND_ECHO+ 303, IcmpAPITest,
TEXT(   "Icmp6ParseReplies() Tests"                          ), 2,									0,					 0, NULL,
TEXT(         "PARSE6_INVALID_BUFFER"                        ), 3,USE_IPv6 |        ICMP_INVALID_BUFFER, ICMP_SEND_ECHO+ 401, IcmpAPITest,
TEXT(         "PARSE6_INVALID_LEN"						     ), 3,USE_IPv6 |           ICMP_INVALID_LEN, ICMP_SEND_ECHO+ 402, IcmpAPITest,
TEXT(         "PARSE6_VALID_PARAMS"                          ), 3,USE_IPv6 |          ICMP_VALID_PARAMS, ICMP_SEND_ECHO+ 403, IcmpAPITest,
NULL,													   0,								 0,					 0, NULL  // marks end of list
};

#endif // __TUXSTUFF_H__
