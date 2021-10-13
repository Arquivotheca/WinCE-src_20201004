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
/**********************************************************************/
/**                        Microsoft Windows                         **/
/**********************************************************************/

/*
    winsock2p.h

    Private master include file for the WS2_32



*/


#ifndef _WINSOCK2P_H_
#define _WINSOCK2P_H_


#define WINSOCK_API_LINKAGE

//
//  System include files.
//

#include <windows.h>
#include <types.h>
#include <wtypes.h>

// DEBUG ZONES
#ifdef DEBUG
#define ZONE_WSC		DEBUGZONE(0)
#define ZONE_WSP		DEBUGZONE(1)
#define ZONE_NSP		DEBUGZONE(2)
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


#define FILE	DWORD


//
//  Local include files.
//


#include "sslinc.h"

#include "cons.h"
#include "wsocknt.h"
//#include "uio.h"
//#include "nameser.h"
//#include "resolv.h"
#include "type.h"
#include "data.h"
#include "proc.h"

#define DLL_ASSERT	ASSERT

//#include "getutil.h"
//#include "sockreg.h"

//#include "wsasrv.h"

#include "pegcalls.h"
#include "socket.h"

#define WINSOCK_VERSION	MAKEWORD(2,2)

#endif  // _WINSOCK2P_H_

