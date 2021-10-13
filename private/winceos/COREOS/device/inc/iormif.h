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
// This header contains internal function prototypes for the I/O resource manager.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus

BOOL WINAPI IORM_ResourceCreateList(DWORD dwResId, DWORD dwMinimum, DWORD dwCount);
BOOL WINAPI IORM_ResourceDestroyList(DWORD dwResId);
BOOL WINAPI IORM_ResourceAdjust (DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fClaim);
BOOL WINAPI IORM_ResourceRequestEx(DWORD dwResId, DWORD dwId, DWORD dwLen, DWORD dwFlags);
BOOL WINAPI IORM_ResourceMarkAsShareable (DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fShareable);

#ifdef __cplusplus
}
#endif	// __cplusplus

