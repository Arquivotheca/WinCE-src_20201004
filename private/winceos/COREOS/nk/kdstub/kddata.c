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


Module Name:

    kddata.c

Abstract:

    This module contains global data for the portable kernel debgger.

--*/

#include "kdp.h"

/* This buffer needs to be aligned. It gets used all over KdStub, and it's
   assumed to be 4-byte aligned. I use 16-byte alignement so we can transfer
   any datatype without needing to use the UNALIGNED keyword. This also helps
   with performance. */
__declspec(align(16)) UCHAR g_abMessageBuffer [KDP_MESSAGE_BUFFER_SIZE];

volatile BOOL g_fDbgConnected;

BOOL g_fDbgKdStateMemoryChanged = FALSE; // Set this signal to TRUE to notify the host that target memory has changed and host-side must refreash

