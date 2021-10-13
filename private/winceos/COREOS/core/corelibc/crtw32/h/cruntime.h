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
#ifndef _INC_CRUNTIME
#define _INC_CRUNTIME

#include <windows.h>
#include <stdlib.h>

#define REG1
#define REG2
#define REG3
#define REG4

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Storage accessors for CRT functions that need thread-local storage
 */
DWORD*      __crt_get_storage_fsr();
DWORD*      __crt_get_storage_flags();
DWORD*      __crt_get_kernel_flags();
char *      __crt_get_storage__fcvt();
long *      __crt_get_storage_rand();
char * *    __crt_get_storage_strtok();
wchar_t * * __crt_get_storage_wcstok();

/*
 * Numeric error helpers
 */
void __raise_int_divide_by_zero();
void __raise_int_overflow();

/*
 * Win32 API surrogates
 */
DECLSPEC_NORETURN
VOID
__crt_ExitProcess(
    DWORD uExitCode
    );

/*
 * Map references to avoid a conflict with the VSD CRT
 */
#define __crtExitProcess __crt_ExitProcess

VOID
__crtRaiseException(
    DWORD         dwExceptionCode,
    DWORD         dwExceptionFlags,
    DWORD         nNumberOfArguments,
    CONST DWORD * lpArguments
    );

BOOL
__crtIsDebuggerPresent(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _INC_CRUNTIME */

