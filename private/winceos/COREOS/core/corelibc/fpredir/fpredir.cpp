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
#include "fpredir.h"


// this is the the assembly thunk that saves
// r12, calls the real fixup function and then jumps to
// the fixed import [r12]
//
extern "C" void _FixupFPCrtThunk();


// This is the only data export from COREDLL that used
// to be defined in the FPCRT, so we have to provide a definition here
//
extern "C"
unsigned int _HUGE[2] = {0x00000000, 0x7ff00000};


// FP function exports will use a small assembly thunk plus
// a pfn array that will be fixed to point to the FPCRT implementations
// when the first redirected function is called.
//
// The assembly thunk looks like:
//
//      ldr r12, [pc]
//      ldr pc, [r12]
//      <ppfn> ---------------> <pfn entry>
//
//      <pfn entry> will initially point to _FixupFPCrtThunk,
//      which will call the real fixup function. After the fixup
//      <pfn entry> will point to the real implementation in FPCRT
//
#define FP_CRT(api_name)                                                \
                                                                        \
    extern _FPAPI_ENTRY _fpapi_##api_name;                              \
                                                                        \
    extern "C"                                                          \
    __declspec(selectany)                                               \
    __declspec(allocate(FPTHUNKS_SECTION))                              \
    _FPAPI_THUNK api_name =                                             \
    {                                                                   \
        0xe59fc000, /* ldr r12, [pc]    */                              \
        0xe59cf000, /* ldr pc, [r12]    */                              \
                                                                        \
        reinterpret_cast<__dword32>(&_fpapi_##api_name.pfn)             \
    };                                                                  \
                                                                        \
    _declspec(selectany)                                                \
    __declspec(allocate(INITIALIZER_SECTION_ENTRY))                     \
    _FPAPI_ENTRY _fpapi_##api_name =                                    \
    {                                                                   \
        reinterpret_cast<__dword32>(&_FixupFPCrtThunk),                 \
        _FPCRT_ORDINAL_##api_name                                       \
    };                                                                  \


// FP_CRT_VFP is identical to FP_CRT in this case
//
#define FP_CRT_VFP(api_name)    FP_CRT(api_name)


#include <fpcrt_ordinals.h>
#include <fpcrt_api.h>    


