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

enum
{
    MAX_STACK_FRAMES_SYSTEM    = 1000,                  // Maximum number of frames to collext for SYSTEM dump
    MEMORY_IP_BEFORE_CONTEXT   =  64,                   // Amount of memory to save before Instruction Pointer for Context & Complete dump
    MEMORY_IP_AFTER_CONTEXT    =  64,                   // Amount of memory to save after Instruction Pointer for Context & Complete dump
    MEMORY_IP_BEFORE_SYSTEM    =  2048,                 // Amount of memory to save before Instruction Pointer for SYSTEM dump
    MEMORY_IP_AFTER_SYSTEM     =  2048,                 // Amount of memory to save after Instruction Pointer for SYSTEM dump
    FLEX_PTMI_BUFFER_SIZE      =  4096,                 // Size of buffer for making FlexPTMI requests
    MAX_MEMORY_VIRTUAL_BLOCKS  = 256,                   // Maximum number of virtual memory blocks that can be written to dump file (each block takes 8 Bytes RAM)
    MAX_MEMORY_PHYSICAL_BLOCKS = MAX_MEMORY_SECTIONS+3, // Maximum number of RAM sections + ARM FirstPT + KData + nk.bin
    EMPTY_BLOCK                = DWORD(-1),
};

extern WCHAR g_wzOEMString[MAX_PATH];
extern DWORD g_dwOEMStringSize;
extern WCHAR g_wzPlatformType[MAX_PATH];
extern DWORD g_dwPlatformTypeSize;
extern PLATFORMVERSION g_platformVersion[4];
extern DWORD g_dwPlatformVersionSize;

