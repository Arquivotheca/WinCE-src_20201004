//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

