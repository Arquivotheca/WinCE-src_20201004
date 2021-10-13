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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

// Abstract:
//
//    Exported const structures declared in ws2tcpip.h.

// Force constants to use dllexport linkage so we can use the safe DATA
// keyword in the .def file, rather than the unsafe CONSTANT.
//#define WINSOCK_API_LINKAGE __declspec(dllexport)

#include "winsock2p.h"
#include <ws2tcpip.h>

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

