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
*heap.h - Heap code include file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains information needed by the C library heap code.
*
*       [Internal]
*
*Revision History:
*       05-16-89  JCR   Module created
*       06-02-89  GJF   Removed naming conflict
*       06-29-89  JCR   Completely new for "New heap - rev 2"
*       06-29-89  GJF   Added _HDRSIZE, fixed some minor glitches.
*       06-29-89  GJF   Added _BLKSIZE(), fixed more minor bugs.
*       06-30-89  GJF   Changed several macros to operate on a pointer to a
*                       descriptor, rather than a descriptor itself.
*       06-30-89  JCR   Corrected/updated several macros
*       07-06-89  JCR   Added region support, misc improvements, etc.
*       07-07-89  GJF   Minor bug in _ROUND() macro
*       07-07-89  JCR   Added _DUMMY status
*       07-19-89  GJF   Removed _PBACKPTR macro
*       07-20-89  JCR   Region routine prototypes, _HEAPFIND values
*       07-21-89  JCR   #define _heap_growsize to _amblksiz for compatibility
*       07-25-89  GJF   Added prototypes for calloc, free and malloc
*       07-28-89  GJF   Added prototype for _msize
*       08-28-89  JCR   Added _HEAP_COALESCE value
*       10-30-89  GJF   Fixed copyright
*       11-03-89  GJF   Added _DISTTOBNDRY(), _NEXTSEGBNDRY() macros and
*                       prototypes for _flat_malloc(), _heap_advance_rover(),
*                       _heap_split_block() functions
*       11-07-89  GJF   Added _SEGSIZE_, added prototype for _heap_search()
*                       restored function prototype for_heap_grow_region()
*       11-08-89  JCR   Added non-pow2 rounding macro
*       11-10-89  JCR   Added _heap_free_region prototype
*       11-10-89  GJF   Added prototypes and macros for multi-thread support
*       11-16-89  JCR   If DEBUG defined include <assert.h>, added sanity check
*       12-13-89  GJF   Removed prototypes duplicated in malloc.h
*       12-20-89  GJF   Removed plastdesc from _heap_desc_ struct, removed
*                       _DELHEAP and _ADDHEAP macros (unused and wrong), added
*                       explicit _cdecl to function prototypes
*       01-08-89  GJF   Use assert macro from assertm.h instead of assert.h
*       03-01-90  GJF   Added #ifndef _INC_HEAP and #include <cruntime.h>
*                       stuff. Also, removed some unused DEBUG286 stuff.
*       03-22-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes.
*       07-25-90  SBM   Replaced <assertm.h> by <assert.h>
*       08-13-90  SBM   Added casts to macros for clean compiles at -W3
*       12-28-90  SRW   Fixed _heap_split_block prototype to match code
*       12-28-90  SRW   Changed _HEAP_GROWSIZE to be 0x10000 [_WIN32_]
*       03-05-91  GJF   Added decl for _heap_resetsize, removed proto for
*                       _heap_advance_rover (both conditioned on _OLDROVER_
*                       not being #define-d).
*       03-13-91  GJF   Made _HEAP_GROWSIZE 32K for [_CRUISER_].
*       04-09-91  PNT   Added _MAC_ definitions
*       08-20-91  JCR   C++ and ANSI naming
*       03-30-92  DJM   POSIX support.
*       08-06-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-16-93  SKS   Change _HEAP_REGIONSIZE from 4 MB to 16 KB.
*                       Low memory environments such as Win32S cannot commit
*                       memory without actually allocating it.
*       04-20-93  SKS   Add new constant _HEAP_MAXREGIONSIZE.
*       04-26-93  SKS   Constants _HEAP_REGIONSIZE and _HEAP_MAXREGIONSIZE
*                       split to handle both small and large memory modes
*                       _HEAP_MAXREGIONSIZE becomes variable "_heap_maxregsize"
*       09-01-93  GJF   Merged NT SDK and Cuda version (this include file is
*                       not used for the winheap\*.* heap manager).
*       12-09-93  GJF   Added _GRANULARITY and defined _HDRSIZE in terms of it.
*       12-13-93  SKS   Add _heapused_nolock().  Move here declarations of
*                       _heap_descpages and _HEAP_EMPTYLIST_SIZE.
*       03-02-94  GJF   Deleted _GETEMPTY() macro and prototype for
*                       _heap_grow_emptylist() (now static). Added prototype
*                       for __getempty(). Changed _heap_split_block() return
*                       type.
*       03-31-94  GJF   Made declarations of:
*                           _heap_descpages
*                           _heap_maxregsize
*                           _heap_regions
*                           _heap_regionsize
*                           _heap_resetsize
*                       conditional on ndef DLL_FOR_WIN32S. Also, made the
*                       definition of _heap_growsize conditional in the same
*                       manner and conditionally include win32s.h.
*       07-29-94  GJF   Page are twice as big on the DEC Alpha.
*       08-04-94  GJF   DEC Alpha needs 8 byte alignment too.
*       09-06-94  CFW   Remove _CRTHEAP_ switch.
*       09-21-94  SKS   Fix typo: no leading _ on "DLL_FOR_WIN32S"
*       10-09-94  BWT   PPC changes from John Morgan.
*       11-02-94  SKS   Change _HEAP_MAXREGIONSIZE_S from 256 KB to 16 MB
*                       (same as _HEAP_MAXREGIONSIZE_L) for users who want a
*                       LOT of memory under Win32s.  This is reasonable now
*                       that Win32s supports reserved but uncommitted memory.
*       12-05-94  CFW   Fix debug new handler support.
*       02-06-95  GJF   Removed some of the Mac-merge changes.
*       02-06-95  CFW   Remove assert.h, DEBUG -> _DEBUG
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       04-24-95  CFW   Add heap hook.
*       12-14-95  JWM   Add "#pragma once".
*       04-23-96  SKS   Return type of _heap_init changed from void to int.
*       02-21-97  GJF   Cleaned out obsolete support for DLL_FOR_WIN32S.
*                       Replaced defined(_M_MPPC) || defined(_M_M68K) with
*                       defined(_MAC). Also, detab-ed.
*       10-07-97  RDL   Added IA64.
*       05-17-99  PML   Remove all Macintosh support.
*       07-07-99  AWB   Changes for 64-bit size_t. (v-aborni)
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-15-04  CFW   Support system debug runtimes (_SYSCRT_DEBUG).
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#if     _MSC_VER > 1000 /*IFSTRIP=IGN*/
#pragma once
#endif

#ifndef _INC_HEAP
#define _INC_HEAP

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Heap block descriptor
 */

struct _block_descriptor {
        struct _block_descriptor *pnextdesc;    /* ptr to next descriptor */
        void *pblock;               /* ptr to memory block */
};

#define _BLKDESC    struct _block_descriptor
#define _PBLKDESC   struct _block_descriptor *


/*
 * Useful constants
 */

/*
 * Unit of allocation. All allocations are of size n * _GRANULARITY. Note
 * that _GRANULARITY must be a power of 2, or it cannot be used with the
 * _ROUND2 macro.
 */
#if     defined(_M_IA64)
#define _GRANULARITY    8
#else
#define _GRANULARITY    4
#endif

/*
 * Size of the header in a memory block. Note that we must have
 * sizeof(void *) <= _HDRSIZE so the header is big enough to hold a pointer
 * to the descriptor.
 */

#define _HDRSIZE    1 * _GRANULARITY

/* _heapchk/_heapset parameter */
#define _HEAP_NOFILL    0x7FFFFFF


/*
 * Descriptor status values
 */

#define _INUSE      0
#define _FREE       1
#define _DUMMY      2


#if     _INUSE != 0 /*IFSTRIP=IGN*/
#error *** Heap code assumes _INUSE value is 0! ***
#endif


/*
 * Macros for manipulating heap memory block descriptors
 *      stat = one of the status values
 *      addr = user-visible address of a heap block
 */

#define _STATUS_MASK    0x3 /* last 2 bits are status */

#define _ADDRESS(pdesc)     ( (void *) ((unsigned)((pdesc)->pblock) & \
                    (~_STATUS_MASK)) )
#define _STATUS(pdesc)      ( (unsigned) ((unsigned)((pdesc)->pblock) & \
                    _STATUS_MASK) )

#define _SET_INUSE(pdesc)   ( pdesc->pblock = (void *) \
                       ((unsigned)_ADDRESS(pdesc) | _INUSE) )
#define _SET_FREE(pdesc)    ( pdesc->pblock = (void *) \
                       ((unsigned)_ADDRESS(pdesc) | _FREE) )
#define _SET_DUMMY(pdesc)   ( pdesc->pblock = (void *) \
                       ((unsigned)_ADDRESS(pdesc) | _DUMMY) )

#define _IS_INUSE(pdesc)    ( _STATUS(pdesc) == _INUSE )
#define _IS_FREE(pdesc)     ( _STATUS(pdesc) == _FREE )
#define _IS_DUMMY(pdesc)    ( _STATUS(pdesc) == _DUMMY )

#define _BLKSIZE(pdesc)     ( (unsigned) ( \
                      (char *)_ADDRESS(pdesc->pnextdesc) - \
                      (char *)_ADDRESS(pdesc) - _HDRSIZE ) )

#define _MEMSIZE(pdesc)     ( (char *)_ADDRESS(pdesc->pnextdesc) - \
                      (char *)_ADDRESS(pdesc) )

#define _BACKPTR(addr)      ( *(_PBLKDESC*)((char *)(addr) - _HDRSIZE) )

#define _CHECK_PDESC(pdesc) ( (*(_PBLKDESC*) (_ADDRESS(pdesc))) == pdesc )

#define _CHECK_BACKPTR(addr)    ( ((char *)(_BACKPTR(addr)->pblock) + _HDRSIZE) \
                    == addr)


/*
 * Heap descriptor
 */

struct _heap_desc_ {

        _PBLKDESC pfirstdesc;   /* pointer to first descriptor */
        _PBLKDESC proverdesc;   /* rover pointer */
        _PBLKDESC emptylist;    /* pointer to empty list */

        _BLKDESC  sentinel; /* Sentinel block for end of heap list */

};

extern struct _heap_desc_ _heap_desc;

/*
 * Region descriptor and heap grow data
 */

struct _heap_region_ {
        void * _regbase;    /* base address of region */
        unsigned _currsize; /* current size of region */
        unsigned _totalsize;    /* total size of region */
        };

#define _heap_growsize _amblksiz

#ifndef _OLDROVER_
extern unsigned int _heap_resetsize;
#endif  /* _OLDROVER_ */
extern unsigned int _heap_regionsize;
extern unsigned int _heap_maxregsize;
extern struct _heap_region_ _heap_regions[];
extern void ** _heap_descpages;

#define _PAGESIZE_      0x1000      /* one page */

#define _SEGSIZE_       0x10000     /* one segment (i.e., 64 Kb) */

#define _HEAP_REGIONMAX     0x40        /* Max number of regions: 64 */
                                        /* For small memory systems: */
#define _HEAP_REGIONSIZE_S  0x4000      /* Initial region size (16K) */
#define _HEAP_MAXREGSIZE_S  0x1000000   /* Maximum region size (16M) */
                                        /* For large memory systems: */
#define _HEAP_REGIONSIZE_L  0x100000    /* Initial region size  (1M) */
#define _HEAP_MAXREGSIZE_L  0x1000000   /* Maximum region size (16M) */

#define _HEAP_GROWSIZE      0x10000     /* Default grow increment (64K) */

#define _HEAP_GROWMIN       _PAGESIZE_  /* Minimum grow inc (1 page) */
#define _HEAP_GROWSTART     _PAGESIZE_  /* Startup grow increment */
#define _HEAP_COALESCE      -1      /* Coalesce heap value */

#define _HEAP_EMPTYLIST_SIZE    (1 * _PAGESIZE_)

/*
 * Values returned by _heap_findaddr() routine
 */

#define _HEAPFIND_EXACT     0   /* found address exactly */
#define _HEAPFIND_WITHIN    1   /* address is within a block */
#define _HEAPFIND_BEFORE    -1  /* address before beginning of heap */
#define _HEAPFIND_AFTER     -2  /* address after end of heap */
#define _HEAPFIND_EMPTY     -3  /* address not found: empty heap */

/*
 * Arguments to _heap_param
 */

#define _HP_GETPARAM    0       /* get heap parameter value */
#define _HP_SETPARAM    1       /* set heap parameter value */

#define _HP_AMBLKSIZ    1       /* get/set _amblksiz value (aka */
#define _HP_GROWSIZE    _HP_AMBLKSIZ    /* _heap_growsize */
#define _HP_RESETSIZE   2       /* get/set _heap_resetsize value */


/*
 * Macros to round numbers
 *
 * _ROUND2 = rounds a number up to a power of 2
 * _ROUND = rounds a number up to any other numer
 *
 * n = number to be rounded
 * pow2 = must be a power of two value
 * r = any number
 */

#define _ROUND2(n,pow2) \
        ( ( n + pow2 - 1) & ~(pow2 - 1) )

#define _ROUND(n,r) \
        ( ( (n/r) + ((n%r)?1:0) ) * r)

/*

   Macros for accessing heap descriptor lists:

        _PUTEMPTY(x) = Puts an empty heap desc on the empty list

        (x = _PBLKDESC = pointer to heap block descriptor)
*/

#if defined(_DEBUG) || defined(_SYSCRT_DEBUG)

#define _PUTEMPTY(x) \
{                               \
        (x)->pnextdesc = _heap_desc.emptylist;      \
                                \
        (x)->pblock = NULL;             \
                                \
        _heap_desc.emptylist = (x);         \
}

#else

#define _PUTEMPTY(x) \
{                               \
        (x)->pnextdesc = _heap_desc.emptylist;      \
                                \
        _heap_desc.emptylist = (x);         \
}

#endif


/*
 * Macros for finding the next 64 Kb boundary from a pointer
 */

#define _NXTSEGBNDRY(p)     ((void *)((unsigned)(p) & 0xffff0000 + 0x10000))

#define _DISTTOBNDRY(p)     ((unsigned)(0x10000 - (0x0000ffff & (unsigned)(p))))


/*
 * Prototypes
 */

_Check_return_  _Ret_opt_bytecap_(_Size) void * __cdecl _nh_malloc(_In_ size_t _Size, _In_ int _NhFlag);
_Check_return_  _Ret_opt_bytecap_(_Size) void * __cdecl _heap_alloc(_In_ size_t _Size);
_Check_return_  _Ret_opt_bytecap_(_Size) void * __cdecl _flat_malloc(_In_ size_t _Size);
_Check_return_ _PBLKDESC __getempty(void);
void __cdecl _heap_abort(void);
_Check_return_ int __cdecl _heap_addblock(_In_ void * _Block, _In_ unsigned int _Size);

#ifdef  _OLDROVER_
void __cdecl _heap_advance_rover(void);
#endif  /* _OLDROVER_ */

void __cdecl _heap_free_region(_In_ int _Index);
_Check_return_ int __cdecl _heap_findaddr(_In_ void * _Block, _Out_ _PBLKDESC * _PDesc);
_Check_return_ int __cdecl _heap_grow(_In_ unsigned int _Size);
_Check_return_ int __cdecl _heap_grow_region(_In_ unsigned _Index, _In_ size_t _Size);
int __cdecl _heap_init(void);

#ifndef _OLDROVER_
int __cdecl _heap_param(_In_ int _Flag, _In_ int _ParamID, void * _Value);
#endif  /* _OLDROVER_ */

_Check_return_ _PBLKDESC __cdecl _heap_search(_In_ unsigned _Size);
_Check_return_ _PBLKDESC __cdecl _heap_split_block(_In_ _PBLKDESC _Pdesc, _In_ size_t _NewSize);

#if defined(_DEBUG) || defined(_SYSCRT_DEBUG)
void __cdecl _heap_print_all(void);
void __cdecl _heap_print_regions(void);
void __cdecl _heap_print_desc(void);
void __cdecl _heap_print_emptylist(void);
void __cdecl _heap_print_heaplist(void);
#endif


/*
 * Prototypes and macros for multi-thread support
 */

#ifdef  _MT

void __cdecl _free_nolock(_Inout_opt_ void * _Memory);
_Check_return_ size_t __cdecl _msize_nolock(_In_ void * _Memory);
size_t __cdecl _heapused_nolock(_Out_opt_ size_t *_PUsed, _Out_opt_ size_t *_PFree);

#if defined(_DEBUG) || defined(_SYSCRT_DEBUG)
void __cdecl _heap_print_regions_nolock(void);
void __cdecl _heap_print_desc_nolock(void);
void __cdecl _heap_print_emptylist_nolock(void);
void __cdecl _heap_print_heaplist_nolock(void);
#endif

#else   /* ndef _MT */

#define _free_nolock(p) free(p)
#define _msize_nolock(p)    _msize(p)

#if defined(_DEBUG) || defined(_SYSCRT_DEBUG)
#define _heap_print_regions_nolock()    _heap_print_regions()
#define _heap_print_desc_nolock()       _heap_print_desc()
#define _heap_print_emptylist_nolock()  _heap_print_emptylist()
#define _heap_print_heaplist_nolock()   _heap_print_heaplist()
#endif

#endif  /* _MT */

#ifdef  HEAPHOOK
#ifndef _HEAPHOOK_DEFINED
/* hook function type */
typedef int (__cdecl * _HEAPHOOK)(int, size_t, void *, void *);
#define _HEAPHOOK_DEFINED
#endif  /* _HEAPHOOK_DEFINED */

extern _HEAPHOOK _heaphook;
#endif  /* HEAPHOOK */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_HEAP */
