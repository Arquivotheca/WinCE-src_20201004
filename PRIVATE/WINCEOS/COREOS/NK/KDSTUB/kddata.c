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

BREAKPOINT_ENTRY KdpBreakpointTable[BREAKPOINT_TABLE_SIZE];
UCHAR KdpMessageBuffer[KDP_MESSAGE_BUFFER_SIZE];
