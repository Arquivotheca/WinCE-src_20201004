//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// UsrExceptDmp.h


#pragma once

#include <kernel.h>


BOOL GenerateDumpFileContent (HANDLE hDumpFile, const DEBUG_EVENT *DebugEvent);


#define MAX_NAME_SIZE (128)
#define MAX_CRASH_THREADS (1024)
#define MAX_CRASH_MODULES (256)

#define MAX_STRING_SIZE (1024 + MAX_NAME_SIZE)

