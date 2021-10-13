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
// conntrk.h
// Connection Tracker - Shuts down inactive sockets.
// Note: only one instance of Connection Tracker can appear in a program.

#ifndef _CONNTRK_
#define _CONNTRK_

#ifdef SUPPORT_IPV6
#include <winsock2.h>
#else
#include <winsock.h>
#endif

typedef void *CONNTRACK_HANDLE;

VOID ConnTrack_Touch(CONNTRACK_HANDLE handle);
CONNTRACK_HANDLE ConnTrack_Insert(SOCKET sock);
BOOL ConnTrack_Remove(CONNTRACK_HANDLE handle);
BOOL ConnTrack_Init(DWORD MaxSocks, DWORD Freq);
VOID ConnTrack_Deinit();

#endif

