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
*malloc.h - declarations and definitions for memory allocation functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the function declarations for memory allocation functions;
*       also defines manifest constants and types used by the heap routines.
*       [System V]
*
*       [Public]
*
*Revision History:
*       01-08-87  JMB   Standardized header, added heap consistency routines
*       02-26-87  BCM   added the manifest constant _HEAPBADPTR
*       04-13-87  JCR   Added size_t and "void *" to declarations
*       04-24-87  JCR   Added 'defined' statements around _heapinfo
*       05-15-87  SKS   Cleaned up _CDECL and near/far ptr declarations
*                       corrected #ifdef usage, and added _amblksiz
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-05-88  JCR   Added DLL _amblksiz support
*       02-10-88  JCR   Cleaned up white space
*       04-21-88  WAJ   Added _FAR_ to halloc/_fmalloc/_nmalloc
*       05-13-88  GJF   Added new heap functions
*       05-18-88  GJF   Removed #defines, added prototypes for _heapchk, _heapset
*                       and _heapwalk
*       05-25-88  GJF   Added _bheapseg
*       08-22-88  GJF   Modified to also work for the 386 (small model only,
*                       no far or based heap support)
*       12-07-88  JCR   DLL refers to _amlbksiz directly now
*       01-10-89  JCR   Removed sbrk() prototype
*       04-28-89  SKS   Put parentheses around negative constants
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-01-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-27-89  JCR   Removed near (_n) and _freect/memavl/memmax prototypes
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       03-01-90  GJF   Added #ifndef _INC_MALLOC and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes.
*       04-10-90  GJF   Made stackavail() _CALLTYPE1, _amblksiz _VARTYPE1.
*       01-17-91  GJF   ANSI naming.
*       08-20-91  JCR   C++ and ANSI naming
*       09-23-91  JCR   stackavail() not supported in WIN32
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       01-18-92  JCR   put _CRTHEAP_ ifdefs in; this stuff is only needed
*                       for a heap implemented in the runtime (not OS)
*       08-05-92  GJF   Function calling type and variable type macros.
*       08-22-92  SRW   Add alloca intrinsic pragma for MIPS
*       09-03-92  GJF   Merged two changes above.
*       09-23-92  SRW   Change winheap code to call NT directly always
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-02-93  SRW   Removed bogus semicolon on #pragma
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       04-20-93  SKS   _alloca is a compiler intrinsic so alloca must be
*                       #define-d, not aliased as link time using OLDNAMES.LIB.
*       09-27-93  GJF   Merged NT SDK and Cuda versions. Also, changed the
*                       value of _HEAP_MAXREQ to be more useful (smaller
*                       value to guarantee page-rounding will not overflow).
*       10-13-93  GJF   Replaced _MIPS_ with _M_MRX000.
*       12-13-93  SKS   Add _heapused().
*       04-12-94  GJF   Special definition for _amblksiz for _DLL. Also,
*                       conditionally include win32s.h for DLL_FOR_WIN32S.
*       05-03-94  GJF   Made special definition of _amblksiz for _DLL also
*                       conditional on _M_IX86.
*       10-02-94  BWT   Add PPC support.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       05-22-95  GJF   Updated _HEAP_MAXREQ.
*       05-24-95  CFW   Add heap hook.
*       12-14-95  JWM   Add "#pragma once".
*       03-07-96  GJF   Added prototypes for small-block heap's get/set 
*                       threshold functions.
*       02-04-97  GJF   Cleaned out obsolete support for Win32s, _CRTAPI* and
*                       _NTSDK.
*       08-13-97  GJF   Strip __p__amblksiz prototype from release version.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       11-08-99  GB    Add _aligned_malloc(), _aligned_realloc, _aligned_free()
*                       _aligned_offset_malloc() and _aligned_offset_realloc().
*       12-10-99  GB    Add _resetstkoflw().
*       12-16-99  GB    Changed HEAP_MAXREQ to 0xFFFFFFFFFFFFFFE0 for Win64
*       01-07-00  GB    Fixed macro _mm_free.
*       12-08-00  PML   Make _resetstkoflw() available to POSIX build.
*       04-17-01  PML   _resetstkoflw() now returns an int success code.
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       04-01-02  PML   Add _get_heap_handle (and intptr_t)
*       03-04-02  PML   Add __declspec(noalias, restrict) support
*       08-06-03  AC    Added push_macro and pop_macro around malloc functions
*       01-13-04  AC    Added _alloca_s and _freea_s
*       04-10-04  AJS   Disabled access to data exports under /clr:pure
*       10-10-04  ESC   Use _CRT_PACKING
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-20-05  AC    Added _CRT_INSECURE_DEPRECATE_GLOBALS
*                       VSW#438137
*       01-24-05  GR    Add _CRT_JIT_INTRINSIC annotations for JIT64 use
*       02-02-05  AC    Added _malloca and _freea
*                       VSW#416634
*       03-15-05  PAL   Resolve data export to function call under /clr:pure
*                       VSW 242824
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-04-05  JL    Remove _alloca_s and _freea_s VSW#452275
*       04-01-05  MSL   Added _recalloc family of functions to support integer overflow fixes
*       04-14-05  MSL   Compile under MIDL
*                       Deprecate some low level heap things under managed
*       05-13-05  MSL   Move _freea to inline to ensure allocator matches _malloca
*       05-18-05  PML   _freea should assert on bad marker
*       06-15-05  AC    Added calling conv to _freea
*                       VSW#518148
*
****/

#pragma once

#ifndef _INC_MALLOC
#define _INC_MALLOC

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

/* Maximum heap request the heap manager will attempt */

#ifdef  _WIN64
#define _HEAP_MAXREQ    0xFFFFFFFFFFFFFFE0
#else
#define _HEAP_MAXREQ    0xFFFFFFE0
#endif

/* _STATIC_ASSERT is for enforcing boolean/integral conditions at compile time. */

#ifndef _STATIC_ASSERT
#define _STATIC_ASSERT(expr) typedef char __static_assert_t[ (expr) ]
#endif

/* Constants for _heapchk/_heapset/_heapwalk routines */

#define _HEAPEMPTY      (-1)
#define _HEAPOK         (-2)
#define _HEAPBADBEGIN   (-3)
#define _HEAPBADNODE    (-4)
#define _HEAPEND        (-5)
#define _HEAPBADPTR     (-6)
#define _FREEENTRY      0
#define _USEDENTRY      1

#ifndef _HEAPINFO_DEFINED
typedef struct _heapinfo {
        int * _pentry;
        size_t _size;
        int _useflag;
        } _HEAPINFO;
#define _HEAPINFO_DEFINED
#endif

#define _mm_free(a)      _aligned_free(a)
#define _mm_malloc(a, b)    _aligned_malloc(a, b)

/* Function prototypes */

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("calloc")
#pragma push_macro("free")
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("_recalloc")
#pragma push_macro("_aligned_free")
#pragma push_macro("_aligned_malloc")
#pragma push_macro("_aligned_offset_malloc")
#pragma push_macro("_aligned_realloc")
#pragma push_macro("_aligned_recalloc")
#pragma push_macro("_aligned_offset_realloc")
#pragma push_macro("_aligned_offset_recalloc")
#pragma push_macro("_aligned_msize")
#pragma push_macro("_freea")
#undef calloc
#undef free
#undef malloc
#undef realloc
#undef _recalloc
#undef _aligned_free
#undef _aligned_malloc
#undef _aligned_offset_malloc
#undef _aligned_realloc
#undef _aligned_recalloc
#undef _aligned_offset_realloc
#undef _aligned_offset_recalloc
#undef _aligned_msize
#undef _freea
#endif

#ifndef _CRT_ALLOCATION_DEFINED
#define _CRT_ALLOCATION_DEFINED
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRT_JIT_INTRINSIC _CRTNOALIAS _CRTRESTRICT    void * __cdecl calloc(_In_ size_t _Count, _In_ size_t _Size);
_CRTIMP                     _CRTNOALIAS                                                                             void   __cdecl free(_Pre_maybenull_ _Post_invalid_ void * _Memory);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) _CRTIMP _CRT_JIT_INTRINSIC _CRTNOALIAS _CRTRESTRICT                              void * __cdecl malloc(_In_ size_t _Size);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP _CRTNOALIAS _CRTRESTRICT                           void * __cdecl realloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _NewSize);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                       void * __cdecl _recalloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size);
_CRTIMP                     _CRTNOALIAS                                                                             void   __cdecl _aligned_free(_Pre_maybenull_ _Post_invalid_ void * _Memory);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_malloc(_In_ size_t _Size, _In_ size_t _Alignment);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_offset_malloc(_In_ size_t _Size, _In_ size_t _Alignment, _In_ size_t _Offset);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_realloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _NewSize, _In_ size_t _Alignment);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                       void * __cdecl _aligned_recalloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size, _In_ size_t _Alignment);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_offset_realloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _NewSize, _In_ size_t _Alignment, _In_ size_t _Offset);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                       void * __cdecl _aligned_offset_recalloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size, _In_ size_t _Alignment, _In_ size_t _Offset);
_Check_return_ _CRTIMP                                                  size_t __cdecl _aligned_msize(_Pre_notnull_ void * _Memory, _In_ size_t _Alignment, _In_ size_t _Offset);
#endif

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("calloc")
#pragma pop_macro("free")
#pragma pop_macro("malloc")
#pragma pop_macro("realloc")
#pragma pop_macro("_recalloc")
#pragma pop_macro("_aligned_free")
#pragma pop_macro("_aligned_malloc")
#pragma pop_macro("_aligned_offset_malloc")
#pragma pop_macro("_aligned_realloc")
#pragma pop_macro("_aligned_recalloc")
#pragma pop_macro("_aligned_offset_realloc")
#pragma pop_macro("_aligned_offset_recalloc")
#pragma pop_macro("_aligned_msize")
#pragma pop_macro("_freea")
#endif

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP int     __cdecl _resetstkoflw (void);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#define _MAX_WAIT_MALLOC_CRT 60000

_CRTIMP unsigned long __cdecl _set_malloc_crt_max_wait(_In_ unsigned long _NewValue);

#ifndef _POSIX_

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_expand")
#pragma push_macro("_msize")
#undef _expand
#undef _msize
#endif

_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP void *  __cdecl _expand(_Pre_notnull_ void * _Memory, _In_ size_t _NewSize);
_Check_return_ _CRTIMP size_t  __cdecl _msize(_Pre_notnull_ void * _Memory);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_expand")
#pragma pop_macro("_msize")
#endif

_Ret_notnull_ _Post_writable_byte_size_(_Size) void *          __cdecl _alloca(_In_ size_t _Size);

#if defined(_DEBUG) || defined(_CRT_USE_WINAPI_FAMILY_DESKTOP_APP)
_CRTIMP _CRT_MANAGED_HEAP_DEPRECATE int     __cdecl _heapwalk(_Inout_ _HEAPINFO * _EntryInfo);
_CRTIMP intptr_t __cdecl _get_heap_handle(void);
#endif

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRTIMP int     __cdecl _heapadd(_In_ void * _Memory, _In_ size_t _Size);
_Check_return_ _CRTIMP int     __cdecl _heapchk(void);
_Check_return_ _CRTIMP int     __cdecl _heapmin(void);
_CRTIMP int     __cdecl _heapset(_In_ unsigned int _Fill);
_CRTIMP size_t  __cdecl _heapused(size_t * _Used, size_t * _Commit);
#endif

#define _ALLOCA_S_THRESHOLD     1024
#define _ALLOCA_S_STACK_MARKER  0xCCCC
#define _ALLOCA_S_HEAP_MARKER   0xDDDD

#if defined(_M_IX86)
#define _ALLOCA_S_MARKER_SIZE   8
#elif defined(_M_X64)
#define _ALLOCA_S_MARKER_SIZE   16
#elif defined(_M_ARM)
#define _ALLOCA_S_MARKER_SIZE   8
#elif !defined (RC_INVOKED)
#error Unsupported target platform.
#endif

_STATIC_ASSERT(sizeof(unsigned int) <= _ALLOCA_S_MARKER_SIZE);

#if !defined(__midl) && !defined(RC_INVOKED)
#pragma warning(push)
#pragma warning(disable:6540)
__inline void *_MarkAllocaS(_Out_opt_ __crt_typefix(unsigned int*) void *_Ptr, unsigned int _Marker)
{
    if (_Ptr)
    {
        *((unsigned int*)_Ptr) = _Marker;
        _Ptr = (char*)_Ptr + _ALLOCA_S_MARKER_SIZE;
    }
    return _Ptr;
}
#pragma warning(pop)
#endif

#if defined(_DEBUG)
#if !defined(_CRTDBG_MAP_ALLOC)
#undef _malloca
#define _malloca(size) \
__pragma(warning(suppress: 6255)) \
        _MarkAllocaS(malloc((size) + _ALLOCA_S_MARKER_SIZE), _ALLOCA_S_HEAP_MARKER)
#endif
#else
#undef _malloca
#define _malloca(size) \
__pragma(warning(suppress: 6255)) \
    ((((size) + _ALLOCA_S_MARKER_SIZE) <= _ALLOCA_S_THRESHOLD) ? \
        _MarkAllocaS(_alloca((size) + _ALLOCA_S_MARKER_SIZE), _ALLOCA_S_STACK_MARKER) : \
        _MarkAllocaS(malloc((size) + _ALLOCA_S_MARKER_SIZE), _ALLOCA_S_HEAP_MARKER))
#endif

#undef _FREEA_INLINE
#ifndef _INTERNAL_IFSTRIP_
#ifndef _CRT_NOFREEA
#define _FREEA_INLINE
#else
#undef _FREEA_INLINE
#endif
#else
#define _FREEA_INLINE
#endif

#ifdef _FREEA_INLINE
/* _freea must be in the header so that its allocator matches _malloca */
#if !defined(__midl) && !defined(RC_INVOKED)
#if !(defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC))
#undef _freea
__pragma(warning(push))
__pragma(warning(disable: 6014))
_CRTNOALIAS __inline void __CRTDECL _freea(_Pre_maybenull_ _Post_invalid_ void * _Memory)
{
    unsigned int _Marker;
    if (_Memory)
    {
        _Memory = (char*)_Memory - _ALLOCA_S_MARKER_SIZE;
        _Marker = *(unsigned int *)_Memory;
        if (_Marker == _ALLOCA_S_HEAP_MARKER)
        {
            free(_Memory);
        }
#if defined(_ASSERTE)
        else if (_Marker != _ALLOCA_S_STACK_MARKER)
        {
            _ASSERTE(("Corrupted pointer passed to _freea", 0));
        }
#endif
    }
}
__pragma(warning(pop))
#endif
#endif
#endif

#if     !__STDC__
/* Non-ANSI names for compatibility */
#define alloca  _alloca
#endif  /* __STDC__*/

#endif  /* _POSIX_ */

#ifdef  HEAPHOOK
#ifndef _HEAPHOOK_DEFINED
/* hook function type */
typedef int (__cdecl * _HEAPHOOK)(int, size_t, void *, void **);
#define _HEAPHOOK_DEFINED
#endif  /* _HEAPHOOK_DEFINED */

/* set hook function */
_CRTIMP _HEAPHOOK __cdecl _setheaphook(_In_opt_ _HEAPHOOK _NewHook);

/* hook function must handle these types */
#define _HEAP_MALLOC    1
#define _HEAP_CALLOC    2
#define _HEAP_FREE      3
#define _HEAP_REALLOC   4
#define _HEAP_MSIZE     5
#define _HEAP_EXPAND    6
#endif  /* HEAPHOOK */


#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_MALLOC */
