//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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

