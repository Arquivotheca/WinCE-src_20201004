//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#pragma once

#define MAX_STACK_FRAMES_CONTEXT   20   // Maximum number of frames to collect for Context & Complete dump
#define MAX_STACK_FRAMES_SYSTEM    1000 // Maximum number of frames to collext for SYSTEM dump
#define MEMORY_IP_BEFORE_CONTEXT   64   // Amount of memory to save before Instruction Pointer for Context & Complete dump
#define MEMORY_IP_AFTER_CONTEXT    64   // Amount of memory to save after Instruction Pointer for Context & Complete dump
#define MEMORY_IP_BEFORE_SYSTEM    2048 // Amount of memory to save before Instruction Pointer for SYSTEM dump
#define MEMORY_IP_AFTER_SYSTEM     2048 // Amount of memory to save after Instruction Pointer for SYSTEM dump
#define FLEX_PTMI_BUFFER_SIZE      4096 // Size of buffer for making FlexPTMI requests
#define MAX_MEMORY_VIRTUAL_BLOCKS  256  // Maximum number of virtual memory blocks that can be written to dump file (each block takes 8 Bytes RAM)
#define MAX_MEMORY_PHYSICAL_BLOCKS MAX_MEMORY_SECTIONS+3 // Maximum number of RAM sections + ARM FirstPT + KData + nk.bin
#define EMPTY_BLOCK                DWORD(-1)

extern WCHAR g_wzOEMString[MAX_PATH];
extern DWORD g_dwOEMStringSize;
extern WCHAR g_wzPlatformType[MAX_PATH];
extern DWORD g_dwPlatformTypeSize;
extern PLATFORMVERSION g_platformVersion[4];
extern DWORD g_dwPlatformVersionSize;

