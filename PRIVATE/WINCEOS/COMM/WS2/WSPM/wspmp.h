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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

wspmp.h

common private header file for WSPM


FILE HISTORY:
	OmarM     13-Sep-2000
	

*/

#ifndef _WSPMP_H_
#define _WSPMP_H_


//#define WINSOCK_API_LINKAGE

//
//  System include files.
//

#include <windows.h>
#include <types.h>
#include <wtypes.h>

// DEBUG ZONES
#ifdef DEBUG
#define ZONE_ACCEPT		DEBUGZONE(0)
#define ZONE_BIND		DEBUGZONE(1)
#define ZONE_CONNECT	DEBUGZONE(2)
#define ZONE_SELECT		DEBUGZONE(3)
#define ZONE_RECV		DEBUGZONE(4)
#define ZONE_SEND		DEBUGZONE(5)
#define ZONE_SOCKET		DEBUGZONE(6)
#define ZONE_LISTEN		DEBUGZONE(7)
#define ZONE_IOCTL		DEBUGZONE(8)
#define ZONE_SSL		DEBUGZONE(9)
#define ZONE_INTERFACE	DEBUGZONE(10)
#define ZONE_MISC		DEBUGZONE(11)
#define ZONE_ALLOC		DEBUGZONE(12)
#define ZONE_FUNCTION	DEBUGZONE(13)
#define ZONE_WARN		DEBUGZONE(14)
#define ZONE_ERROR		DEBUGZONE(15)
#endif


#include <winerror.h>
#include <winsock2.h>
#include <ws2spi.h>
#include <winsock.h>
#include <ctype.h>

#include <string.h>

//#include <ptype.h>
#include <memory.h>

#undef WINVER
#define WINVER	0x0400	
#include <winnetwk.h>

//
//  Project include files.
//

#include <linklist.h>
// Define LPSOCK_INFO for winsock
#define LPSOCK_INFO_DEFINED
#include <wsock.h>
typedef SOCKHAND LPSOCK_INFO;

#endif  // _WSPMP_H_

