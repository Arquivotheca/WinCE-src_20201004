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
ROM2RAM_PAGE_ENTRY g_aRom2RamPageTable [NB_ROM2RAM_PAGES];
BYTE g_abRom2RamDataPool [((NB_ROM2RAM_PAGES + 1) * PAGE_SIZE) - 1];

BOOL g_fDbgConnected;

BOOL g_fDbgKdStateMemoryChanged = FALSE; // Set this signal to TRUE to notify the host that target memory has changed and host-side must refreash

