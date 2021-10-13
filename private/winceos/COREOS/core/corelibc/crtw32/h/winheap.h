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
*winheap.h - Private include file for winheap directory.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains information needed by the C library heap code.
*
*       [Internal]
*
*Revision History:
*       10-01-92  SRW   Created.
*       10-28-92  SRW   Change winheap code to call Heap????Ex calls
*       11-05-92  SKS   Change name of variable "CrtHeap" to "_crtheap"
*       11-07-92  SRW   _NTIDW340 replaced by linkopts\betacmp.c
*       02-23-93  SKS   Update copyright to 1993
*       10-01-94  BWT   Add _nh_malloc prototype and update copyright
*       10-31-94  GJF   Added _PAGESIZE_ definition.
*       11-07-94  GJF   Changed _INC_HEAP to _INC_WINHEAP.
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       04-06-95  GJF   Updated (primarily Win32s DLL support) to re-
*                       incorporate into retail Crt build.
*       05-24-95  CFW   Add heap hook.
*       12-14-95  JWM   Add "#pragma once".
*       03-07-96  GJF   Added support for the small-block heap.
*       04-05-96  GJF   Changes to __sbh_page_t type to improve performance
*                       (see sbheap.c for details).
*       05-08-96  GJF   Several changes to small-block heap types.
*       02-21-97  GJF   Cleaned out obsolete support for Win32s.
*       05-22-97  RDK   Replaced definitions for new small-block support.
*       07-23-97  GJF   _heap_init changed slightly.
*       10-01-98  GJF   Added decl for __sbh_initialized. Also, changed
*                       __sbh_heap_init() slightly.
*       11-17-98  GJF   Resurrected support for old (VC++ 5.0) small-block and
*                       added support for multiple heap scheme (VC++ 6.1)
*       06-22-99  GJF   Removed old small-block heap from static libs.
*       11-30-99  PML   Compile /Wp64 clean.
*       08-07-00  PML   __active_heap not available on Win64
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       03-10-04  AJS   Prevent compiler warning on BITV_COMMIT_INIT
*       04-07-04  MSL   Double slash removal
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-08-05  PAL   Include crtdefs.h to define _CRT_PACKING.
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-01-05  MSL   Added _recalloc
*       11-01-06  AEI   Added _aligned_msize_base due to VSW#552904
*
****/

#pragma once

#ifndef _INC_WINHEAP
#define _INC_WINHEAP

#include <crtdefs.h>

#pragma pack(push,_CRT_PACKING)

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#ifdef  __cplusplus
extern "C" {
#endif

#include <sal.h>
#include <windows.h>


#define BYTES_PER_PARA      16
#define PARAS_PER_PAGE      256     /*  tunable value */
#define BYTES_PER_PAGE      (BYTES_PER_PARA * PARAS_PER_PAGE)

extern  HANDLE _crtheap;

_Check_return_  _Ret_maybenull_ _Post_writable_byte_size_(_Size) void * __cdecl _nh_malloc(_In_ size_t _Size, _In_ int _NhFlag);
_Check_return_  _Ret_maybenull_ _Post_writable_byte_size_(_Size) void * __cdecl _heap_alloc(_In_ size_t _Size);

extern int     __cdecl _heap_init(void);
extern void    __cdecl _heap_term(void);

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) extern _CRTIMP void *  __cdecl _malloc_base(_In_ size_t _Size);

extern _CRTIMP void    __cdecl _free_base(_Pre_maybenull_ _Post_invalid_ void * _Memory);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) extern void *  __cdecl _realloc_base(_Pre_maybenull_ _Post_invalid_  void * _Memory, _In_ size_t _NewSize);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) extern void *  __cdecl _recalloc_base(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size);

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) extern void *  __cdecl _expand_base(_Pre_notnull_ void * _Memory, _In_ size_t _NewSize);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) extern void *  __cdecl _calloc_base(_In_ size_t _Count, _In_ size_t _Size);

_Check_return_ extern size_t  __cdecl _msize_base(_Pre_notnull_ void * _Memory);
_Check_return_ extern size_t  __cdecl _aligned_msize_base(_Pre_notnull_ void * _Memory, _In_ size_t _Alignment, _In_ size_t _Offset);

#ifdef  HEAPHOOK
#ifndef _HEAPHOOK_DEFINED
/* hook function type */
typedef int (__cdecl * _HEAPHOOK)(int, size_t, void *, void *);
#define _HEAPHOOK_DEFINED
#endif  /* _HEAPHOOK_DEFINED */

extern _HEAPHOOK _heaphook;
#endif /* HEAPHOOK */

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_WINHEAP */
