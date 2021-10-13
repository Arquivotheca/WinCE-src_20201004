//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//#include <winuser.h>

#define SSDP_PORT 1900

#define SSDP_ADDR_IPV4   { 239,255,255,250 }
#define SSDP_ADDR_IPV6   { 0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C }

#define NUM_RETRIES 3
#define RETRY_INTERVAL 3000
#define ANNOUNCE_MARGIN 120

#ifndef UNDER_CE
#undef ASSERT
#define ASSERT assert
#endif