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
/***
*rthelper.h - EH helper header
*
****/

#include <excpt.h>

#pragma warning(disable: 4985) // attributes not present on previous declaration

/* The x86 compiler uses the __stdcall calling convention for the helper
 *  API.  __stdcall has been #defined away in CE, so we must override that
 *  here.
 *
 * The calling convention of the ctors and dtors also differs between
 *  x86 and RISC.  For x86, the type is specified with the __thiscall
 *  keyword, available only when compiling with "-d1Binl" (see comment
 *  above.)  For RISC, the convention should be __cdecl.
 */
#undef __stdcall

#ifdef _M_IX86
#define HELPERFNAPI __stdcall
#define CALLTYPE    __thiscall
#else
#define HELPERFNAPI __cdecl
#define CALLTYPE    __cdecl
#endif

void __ArrayUnwind(
    _In_bytecount_x_(size*count) void*          ptr,           // Pointer to array to destruct
    _In_                        size_t          size,          // Size of each element (including padding)
    _In_                        int             count,         // Number of elements in the array
    _In_                        void(CALLTYPE   *pDtor)(void*) // The destructor to call
);

