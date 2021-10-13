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
/*++

Copyright (c) 2004 Microsoft Corporation

Module Name:

    wlanutil.h

Abstract:

    WLAN utility routines.

Environment:

    User mode only

Revision History:

--*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

DWORD
WlanStringToSsid(
    __in LPCWSTR strSsid,
    __out PDOT11_SSID pDot11Ssid
    );

DWORD
WlanSsidToDisplayName(
    __in PDOT11_SSID pDot11Ssid,
    __in BOOL bHexFallback,
    __out_ecount_opt(*pcchDisplayName) LPWSTR strDisplayName,
    __inout DWORD *pcchDisplayName
    );

BOOL
WlanIsActiveConsoleUser(
    );

#ifdef __cplusplus
}
#endif

