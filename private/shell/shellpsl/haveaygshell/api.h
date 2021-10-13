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
/*---------------------------------------------------------------------------*\
 *  module: api.h
\*---------------------------------------------------------------------------*/
#ifndef __API_H__
#define __API_H__

/////////////////////////////////////////////////////////////////////////////

#include <windows.h>
/////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define ZONE_INIT        DEBUGZONE(0)        // 0x00001
//#define ZONE_???       DEBUGZONE(1)        // 0x00002
//#define ZONE_???       DEBUGZONE(2)        // 0x00004
//#define ZONE_???       DEBUGZONE(3)        // 0x00008
//#define ZONE_???       DEBUGZONE(4)        // 0x00010
//#define ZONE_???       DEBUGZONE(5)        // 0x00020
//#define ZONE_???       DEBUGZONE(6)        // 0x00040
//#define ZONE_???       DEBUGZONE(7)        // 0x00080
//#define ZONE_???       DEBUGZONE(8)        // 0x00100
#define ZONE_DST         DEBUGZONE(9)        // 0x00200
#define ZONE_INTERFACE   DEBUGZONE(10)       // 0x00400
#define ZONE_MISC        DEBUGZONE(11)       // 0x00800
#define ZONE_ALLOC       DEBUGZONE(12)       // 0x01000
#define ZONE_FUNCTION    DEBUGZONE(13)       // 0x02000
#define ZONE_WARN        DEBUGZONE(14)       // 0x04000
#define ZONE_ERROR       DEBUGZONE(15)       // 0x08000
#endif 
	

	
VOID RegisterShellAPIs(VOID);

/////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif	
	
#endif /* __API_H__ */
