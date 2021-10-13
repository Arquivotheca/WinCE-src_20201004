//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

