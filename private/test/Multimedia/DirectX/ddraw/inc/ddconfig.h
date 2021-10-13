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

#ifndef __DDCONFIG_H__
#define __DDCONFIG_H__

// External Dependencies
// ---------------------
#include <platform_config.h>

// ===============================================================
// Support for Platform specific DirectDraw configuration settings
// These are system-wide, test-independent configuration settings
// ===============================================================

#define CFG_Skip_Postponed_Bugs                         1

#if CFG_PLATFORM_DREAMCAST
    #define CFG_DirectDraw_Overlay                      0
#endif

#if CFG_PLATFORM_DX
    #define CFG_DirectDraw_Overlay                      1
#endif

#if CFG_PLATFORM_ASTB
    #define CFG_DirectDraw_Overlay                      1
#endif

#if CFG_PLATFORM_DESKTOP
    #define CFG_DirectDraw_Overlay                      1
#endif

// Is DDraw giving us decent errors, or is it always returning DDERR_GENERIC?
#define CFG_GENERIC_ERRORS 

#endif 
