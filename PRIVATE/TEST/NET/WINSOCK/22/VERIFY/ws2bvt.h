//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __WS2BVT_H__
#define __WS2BVT_H__

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <svcguid.h>
#include <tux.h>
#include <katoex.h>

#ifndef UNDER_CE
#include <tchar.h>
#include <stdio.h>
#endif

#define __PROJECT__				_T("Ws 2.2 BVT")
#define	SEPARATOR				_T(": ")

#define __EXCEPTION__			0x00
#define __FAIL__				0x02
#define __ABORT__              	0x04
#define __SKIP__               	0x06
#define __NOT_IMPLEMENTED__  	0x08
#define __PASS__              	0x0A

#define	BUFFER_SIZE				0x400

enum infoType {FAIL, ECHO, DETAIL, ABORT, SKIP};

#define	countof(a) 				(sizeof(a)/sizeof(*(a)))

//
// #defines for the Tux Function Table and test cases
//
#define USE_IPv6					   0x40000000

#define  PRES					0
#define  TRAN					100

#define  PRES_IPV4				1
#define  PRES_IPV6				2

#define  TRANS_TCP_IPV4			1
#define  TRANS_TCP_IPV6			2

//
// Global logging and helper functions
//
LPTSTR GetStackName(int nFamily);
LPTSTR GetFamilyName(int nFamily);
LPTSTR GetTypeName(int nFamily);
LPTSTR GetProtocolName(int nFamily);
TESTPROCAPI getCode(void);

VOID Debug(LPCTSTR szFormat, ...);
void Log(infoType iType, LPCTSTR szFormat, ...);
VOID PrintIPv6Addr(infoType iType, SOCKADDR_IN6 *psaAddr);

#endif // __WS2BVT_H__
