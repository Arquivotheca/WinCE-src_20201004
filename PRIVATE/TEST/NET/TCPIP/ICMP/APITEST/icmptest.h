//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __ICMPAPITEST_H__
#define __ICMPAPITEST_H__

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <svcguid.h>
#include <cmdline.h>
#include <ntddip6.h>
#include <icmpapi.h>
#include <tux.h>

#ifndef UNDER_CE
#include <tchar.h>
#include <stdio.h>

#define RETAILMSG(cond,printf_exp); \
   if( cond ) {                     \
      _tprintf printf_exp ; \
   }
#else

#ifdef RETAILMSG
#undef RETAILMSG
#endif
#define RETAILMSG(cond,printf_exp); \
   if( cond ) {                     \
      NKDbgPrintfW printf_exp ; \
   }
#endif

#define USE_IPv6					   0x40000000
#define USE_ASYNC					   0x80000000

//Globals for IP addresses of server and client...
extern SOCKADDR_IN g_saServerAddr;
extern SOCKADDR_IN6 g_saServerAddrV6;

#define  ICMP_INVALID_HANDLE			11
#define  ICMP_NULL_ADDR					12
#define  ICMP_NULL_SEND_BUFFER			13
#define  ICMP_SEND_BUFFER_FRAG			14
#define  ICMP_SEND_BUFFER_NO_FRAG		15
#define  ICMP_SEND_BUFFER_TOO_SMALL		16
#define  ICMP_INVALID_SEND_LEN			17
#define  ICMP_NULL_REPLY_BUFFER			18
#define  ICMP_INVALID_REPLY_LENGTH		19
#define  ICMP_INVALID_SEND_OPTIONS		110
#define  ICMP_INVALID_TIMEOUT			111
#define  ICMP_UNREACH_DESTINATION		112
#define  ICMP_VALID_REQUEST				113 
#define  ICMP_VALID_PARAMS				114
#define  ICMP_INVALID_LEN				115
#define  ICMP_INVALID_BUFFER			116



#endif // __ICMPAPITEST_H__
