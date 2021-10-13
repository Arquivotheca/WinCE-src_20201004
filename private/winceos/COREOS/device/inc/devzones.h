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

// 
// This file contains definitions for debug zones used by the device manager.
//

#pragma once

//
// Debug zones
//
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARNING    DEBUGZONE(1)
#define ZONE_FUNCTION   DEBUGZONE(2)
#define ZONE_INIT       DEBUGZONE(3)
#define ZONE_ROOT       DEBUGZONE(4)
#define ZONE_ACTIVE     DEBUGZONE(6)
#define ZONE_RESMGR     DEBUGZONE(7)
#define ZONE_FSD        DEBUGZONE(8)
#define ZONE_DYING      DEBUGZONE(9)
#define ZONE_BOOTSEQ    DEBUGZONE(10)
#define ZONE_PNP        DEBUGZONE(11)
#define ZONE_SERVICES   DEBUGZONE(12)
#define ZONE_IO         DEBUGZONE(13)
#define ZONE_FILE       DEBUGZONE(14)

#ifndef SHIP_BUILD
#ifdef __cplusplus
extern "C" {
#endif

extern DBGPARAM    dpCurSettings;

#ifdef __cplusplus
}
#endif

#endif // SHIP_BUILD
