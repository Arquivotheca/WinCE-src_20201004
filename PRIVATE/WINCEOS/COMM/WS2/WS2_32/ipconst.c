//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

