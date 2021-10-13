//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    wmi.h

Abstract:

    Contains routines for interacting with WMI



    FrancisD

Revision History:

--*/

#ifndef __DHCPV6_WMI_H__
#define __DHCPV6_WMI_H__

DWORD
DhcpV6InitWMI(
    VOID
    );

VOID
DhcpV6DeInitWMI(
    VOID
    );

#endif // __DHCPV6_WMI_H__

