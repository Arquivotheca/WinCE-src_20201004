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
//#include <winuser.h>

#define SSDP_PORT 1900

#define SSDP_ADDR_IPV4   { 239,255,255,250 }
#define SSDP_ADDR_IPV6   { 0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C }

#define NUM_RETRIES 4
#define REANNOUNCE_INTERVAL 400000
#define RETRY_INTERVAL 3000
#define ANNOUNCE_MARGIN 120

#ifndef UNDER_CE
#undef ASSERT
#define ASSERT assert
#endif