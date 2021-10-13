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
// mngprfx.h

#ifndef _MNGPRFX_H_
#define _MNGPRFX_H_

#include <windows.h>
#include <ntddip6.h>

BOOL IsPDEnabledInterface(PCHAR UpIfName, PCHAR DownIfName);

// Initializes this module; must be called before calling
// DHCPv6ManagePrefix or DHCPv6ManagePrefixPeriodicCleanup.
BOOL DHCPv6ManagePrefixInit();

// Called to deinitialize this module.
void DHCPv6ManagePrefixDeinit();

// Applies and/or refreshes a given prefix.
BOOL DHCPv6ManagePrefix(
    UINT OrigIfIdx,         // Interface on which prefix was received
    IPv6Addr Prefix,        // The prefix
    UINT PrefixLen,         //   Length
    UINT ValidLifetime,     //   Valid lifetime; Note: can use INFINITE_LIFETIME
    UINT PreferredLifetime  //   Preferred lifetime; Note: can use INFINITE_LIFETIME
);

// Must be called periodically to clean-up expired published routes
// created to support new prefixes.
BOOL DHCPv6ManagePrefixPeriodicCleanup();

#endif // _MNGPRFX_H_

