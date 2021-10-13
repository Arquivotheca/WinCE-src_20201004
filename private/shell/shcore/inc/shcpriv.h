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
#pragma once

// Standard headers
#define _SHLWAPI_
#pragma warning (disable:4514) // 'function' : unreferenced inline function has been removed
#pragma warning (push, 3)
#include <windows.h>
#include <windowsx.h>
#pragma warning (pop)


// Typical USAGE:
//    #pragma message ([BUGBUG | WORKITEM] "my message")
#define __BUGBUG(n) __FILE__ "(" #n ") : BUGBUG - " 
#define _BUGBUG(n) __BUGBUG(n) 
#define BUGBUG _BUGBUG(__LINE__)
#define __WORKITEM(n) __FILE__ "(" #n ") : WORKITEM - " 
#define _WORKITEM(n) __WORKITEM(n)
#define WORKITEM _WORKITEM(__LINE__)


// Useful macros
#define lengthof(x) ( (sizeof((x))) / (sizeof(*(x))) )


// Debug Zones
#ifdef DEBUG  
  #define ZONE_FATAL      DEBUGZONE(0x00)
  #define ZONE_WARNING    DEBUGZONE(0x01)
  #define ZONE_VERBOSE    DEBUGZONE(0x02)
  #define ZONE_INFO       DEBUGZONE(0x03)
  #define ZONE_TRACE_4    DEBUGZONE(0x04)
  #define ZONE_TRACE_5    DEBUGZONE(0x05)
  #define ZONE_TRACE_6    DEBUGZONE(0x06)
  #define ZONE_TRACE_7    DEBUGZONE(0x07)
  #define ZONE_TRACE_8    DEBUGZONE(0x08)
  #define ZONE_TRACE_9    DEBUGZONE(0x09)
  #define ZONE_TRACE_A    DEBUGZONE(0x0A)
  #define ZONE_TRACE_B    DEBUGZONE(0x0B)
  #define ZONE_TRACE_C    DEBUGZONE(0x0C)
  #define ZONE_TRACE_D    DEBUGZONE(0x0D)
  #define ZONE_TRACE_E    DEBUGZONE(0x0E)
  #define ZONE_TRACE_F    DEBUGZONE(0x0F)
#else
  #define ZONE_FATAL      0
  #define ZONE_WARNING    0
  #define ZONE_VERBOSE    0
  #define ZONE_INFO       0
  #define ZONE_TRACE_4    0
  #define ZONE_TRACE_5    0
  #define ZONE_TRACE_6    0
  #define ZONE_TRACE_7    0
  #define ZONE_TRACE_8    0
  #define ZONE_TRACE_9    0
  #define ZONE_TRACE_A    0
  #define ZONE_TRACE_B    0
  #define ZONE_TRACE_C    0
  #define ZONE_TRACE_D    0
  #define ZONE_TRACE_E    0
  #define ZONE_TRACE_F    0
#endif // DEBUG

// Important externs
extern HINSTANCE HINST_SHCORE;
