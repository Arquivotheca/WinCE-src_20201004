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
#ifndef _MAIN_H_
#define _MAIN_H_

#include <ssdp.h>
#include <dbgapi.h>
#include <svsutil.hxx>

#if defined (DEBUG) || defined (_DEBUG)

#define IFDBG(x) x

#define ZONE_MISC       DEBUGZONE(0)
#define ZONE_INIT       DEBUGZONE(1)
#define ZONE_ENUM       DEBUGZONE(2)
#define ZONE_FINDER     DEBUGZONE(3)
#define ZONE_DEVICE     DEBUGZONE(4)
#define ZONE_DEVICES    DEBUGZONE(5)
#define ZONE_SERVICE    DEBUGZONE(6)
#define ZONE_SERVICES   DEBUGZONE(7)
#define ZONE_DOCUMENT   DEBUGZONE(8)
#define ZONE_CALLBACK   DEBUGZONE(9)
#define ZONE_TRACE      DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)

#else

#define IFDBG(x)

#define ZONE_MISC       0
#define ZONE_INIT       0
#define ZONE_ENUM       0
#define ZONE_FINDER     0
#define ZONE_DEVICE     0
#define ZONE_DEVICES    0
#define ZONE_SERVICE    0
#define ZONE_SERVICES   0
#define ZONE_DOCUMENT   0
#define ZONE_CALLBACK   0
#define ZONE_TRACE      0
#define ZONE_ERROR      0

#endif

#endif
