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
#pragma once

typedef enum _CEDUMP_TYPE 
{
    ceDumpTypeUndefined   = 0,
    ceDumpTypeContext     = 1,
    ceDumpTypeSystem      = 2,
    ceDumpTypeComplete    = 3
} CEDUMP_TYPE;

// Used to pass registry settings from DwXfer.dll to OsAccess.dll via reserved memory
typedef struct _WATSON_DUMP_SETTINGS
{
    DWORD       dwSizeOfHeader;         // Size of this struct
    BOOL        fDumpEnabled;           // Indicates that dump generation is enabled
    BOOL        fDumpFirstChance;       // Indicates that dump generation should occure on 1st chance exception
    CEDUMP_TYPE ceDumpType;             // Type of dump file to generate
    DWORD       dwMaxDumpFileSize;      // Max size for dump file 
    DWORD       dwDumpWritten;          // Size of dump file written to reserved memory
    DWORD       dwDumpFileCRC;          // CRC of the dump file in the reserved memory
    WCHAR       wzExtraFilesDirectory[MAX_PATH]; // Path to directory where user app can place extra files
    WCHAR       wzExtraFilesPath[MAX_PATH];      // Path to extra files set by user app to upload with crash dump
} WATSON_DUMP_SETTINGS, *PWATSON_DUMP_SETTINGS;

#define WATSON_TRANSFER_BUFFER_SIZE 4096        // Size of Read/Write buffer for transfer of dump file to file system


#define MAX_STACK_FRAMES_CONTEXT 20                    // Maximum number of frames to collect for Context & Complete dump