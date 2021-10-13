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

#ifndef _ARM_
#error FPCRT is available only on ARM platforms
#endif


// We use special sections for the redirection thunks and
// entries, so we can group them together. We need this
// so that we can enumerate the fixup entries at runtime.
//
#define INITIALIZER_SECTION         ".CCRT"
#define INITIALIZER_SECTION_BEGIN   ".CCRT$A"
#define INITIALIZER_SECTION_ENTRY   ".CCRT$M"
#define INITIALIZER_SECTION_END     ".CCRT$Z"

#define FPTHUNKS_SECTION            ".FPCRT"

#pragma section(INITIALIZER_SECTION_BEGIN, read, write)
#pragma section(INITIALIZER_SECTION_ENTRY, read, write)
#pragma section(INITIALIZER_SECTION_END,   read, write)

#pragma section(FPTHUNKS_SECTION, read, execute)


// 32bit unsigned integer type
// (we can't use the standard windows or crt definitions here)
//
typedef unsigned long __dword32;


// the FPCRT function redirection thunk
//
struct _FPAPI_THUNK
{
    __dword32 ldr_r12_pc;
    __dword32 ldr_pc_r12;
    __dword32 ppfnIndir;
};


// FPCRT redirection entry
//
struct _FPAPI_ENTRY
{
    __dword32 pfn;
    __dword32 ordinal;
};


