//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
