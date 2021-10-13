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

#include <pcl_c.hpp>

// mask used to determine if PCL should be enabled for an application.
#define PCL_ENABLED_MASK 0x1

/// <summary>
/// Get the PCL System Font being used by the thunking layer.
///
/// This font differs from the real System Font in that it is specially created to 
/// match font of a Pocket PC.
/// </summary>
/// <returns>
/// <para>Returns HGDIOBJ type</para>
/// </returns>
HGDIOBJ GetPclSystemFont();

/// <summary>
///  Get the PCL Button Font being used by the thunking layer.
///
///  This font is a system font (but we'll use the PCL system font) combined with 
///  some styles (boldness) that are defined in the registry
/// </summary>
/// <returns>
/// <para>Returns HGDIOBJ type</para>
/// </returns>
HGDIOBJ GetPclButtonFont();


/// <summary>
/// Determines if PCL has been initialized and enabled for this MsgQueue
/// </summary>
/// <param name="void"></param>
/// <returns>
/// <para>Returns BOOL type</para>
/// </returns>
BOOL IsPclEnabled(void);

/// <summary>
///  Registers the given hWnd as a Legacy Application
/// </summary>
/// <param name="hwnd">
///  Window to set Legacy mode for
/// </param>
void RegisterWindowAsLegacy(HWND hWnd);
