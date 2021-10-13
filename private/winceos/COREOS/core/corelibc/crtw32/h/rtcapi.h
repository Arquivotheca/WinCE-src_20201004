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
*rtcapi.h - declarations and definitions for RTC use
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the declarations and definitions for all RunTime Check
*       support.
*
*Revision History:
*       ??-??-??  KBF   Created public header for RTC
*       05-11-99  KBF   Wrap RTC support in #ifdef.
*       05-26-99  KBF   Removed RTCl & RTCv, added _RTC_ADVMEM stuff
*       06-13-01  PML   Compile clean -Za -W4 -Tc (vs7#267063)
*       10-17-03  SJ    Added UNICODE RTC Reporting
*       11-12-03  CSP   Added _alloca checking feature.
*       04-07-04  MSL   Deprecated a lot of internal stuff we never intended outsiders to use
*                       and removed it in /clr mode
*                       VSW #267256
*       10-01-04  AGH   Retype _RTC_ALLOCA_NODE::allocaSize as size_t
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       03-30-05  PAL   Make parameter names ANSI-compliant
*
****/

#ifndef _INC_RTCAPI
#define _INC_RTCAPI

#ifdef  _RTC

#include <crtdefs.h>

#pragma pack(push,_CRT_PACKING)

/*
Previous versions of this header included declarations of functions used by RTC but
not intended for use by end-users. These functions are now declared deprecated by default
and may be removed in a future version.
*/
#ifndef _CRT_ENABLE_RTC_INTERNALS
#define _RTCINTERNAL_DEPRECATED _CRT_DEPRECATE_TEXT("These internal RTC functions are obsolete and should not be used")
#else
#define _RTCINTERNAL_DEPRECATED 
#endif



#ifdef  __cplusplus

extern "C" {

#endif

    /* General User API */

typedef enum _RTC_ErrorNumber {
    _RTC_CHKSTK = 0,
    _RTC_CVRT_LOSS_INFO,
    _RTC_CORRUPT_STACK,
    _RTC_UNINIT_LOCAL_USE,
#ifdef _RTC_ADVMEM
    _RTC_INVALID_MEM,
    _RTC_DIFF_MEM_BLOCK,
#endif
    _RTC_CORRUPTED_ALLOCA,
    _RTC_ILLEGAL 
} _RTC_ErrorNumber;
 
#   define _RTC_ERRTYPE_IGNORE -1
#   define _RTC_ERRTYPE_ASK    -2

#ifndef _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif

    typedef int (__cdecl *_RTC_error_fn)(int, const char *, int, const char *, const char *, ...);
    typedef int (__cdecl *_RTC_error_fnW)(int, const wchar_t *, int, const wchar_t *, const wchar_t *, ...);

    /* User API */
    int           __cdecl _RTC_NumErrors(void);
    const char *  __cdecl _RTC_GetErrDesc(_RTC_ErrorNumber  _Errnum);
    int           __cdecl _RTC_SetErrorType(_RTC_ErrorNumber  _Errnum, int _ErrType);
    _RTC_error_fn __cdecl _RTC_SetErrorFunc(_RTC_error_fn);
    _RTC_error_fnW __cdecl _RTC_SetErrorFuncW(_RTC_error_fnW);
#ifdef _RTC_ADVMEM
    void          __cdecl _RTC_SetOutOfMemFunc(int (*_Func)(void));
#endif

    /* Power User/library API */

#ifdef _RTC_ADVMEM

    void __cdecl _RTC_Allocate(void *_Addr, unsigned _Size, short _Level);
    void __cdecl _RTC_Free(void *_Mem, short _Level);

#endif

    /* Init functions */

    /* These functions all call _CRT_RTC_INIT */
    void __cdecl _RTC_Initialize(void);
    void __cdecl _RTC_Terminate(void);

    /*
     * If you're not using the CRT, you have to implement _CRT_RTC_INIT
     * Just return either null, or your error reporting function
     * *** Don't mess with res0/res1/res2/res3/res4 - YOU'VE BEEN WARNED! ***
     */
    _RTC_error_fn __cdecl _CRT_RTC_INIT(void *_Res0, void **_Res1, int _Res2, int _Res3, int _Res4);
    _RTC_error_fnW __cdecl _CRT_RTC_INITW(void *_Res0, void **_Res1, int _Res2, int _Res3, int _Res4);
    
    /* Compiler generated calls (unlikely to be used, even by power users) */
    /* Types */
    typedef struct _RTC_vardesc {
        int addr;
        int size;
        char *name;
    } _RTC_vardesc;

    typedef struct _RTC_framedesc {
        int varCount;
        _RTC_vardesc *variables;
    } _RTC_framedesc;

    /* NOTE: 
        Changing this structure requires a matching compiler backend
        update, because the offsets are hardcoded inside there.
    */
#pragma pack(push, 1)
    /*  Structure padded under 32-bit x86, to get consistent
        execution between 32/64 targets.
    */
    typedef struct _RTC_ALLOCA_NODE {
        __int32 guard1;
        struct _RTC_ALLOCA_NODE *next;
#if defined(_M_IX86) || defined(_M_ARM)
        __int32 dummypad;
#endif
        size_t allocaSize;
#if defined(_M_IX86) || defined(_M_ARM)
        __int32 dummypad2;
#endif
        __int32 guard2[3];
    } _RTC_ALLOCA_NODE;
#pragma pack(pop)

#if !defined(_M_CEE) && !defined(_M_CEE_PURE)
    /* These unsupported functions are deprecated in native mode and not supported at all in /clr mode */

    /* Shortening convert checks - name indicates src bytes to target bytes */
    /* Signedness is NOT checked */
    _RTCINTERNAL_DEPRECATED char   __fastcall _RTC_Check_2_to_1(short _Src);
    _RTCINTERNAL_DEPRECATED char   __fastcall _RTC_Check_4_to_1(int _Src);
    _RTCINTERNAL_DEPRECATED char   __fastcall _RTC_Check_8_to_1(__int64 _Src);
    _RTCINTERNAL_DEPRECATED short  __fastcall _RTC_Check_4_to_2(int _Src);
    _RTCINTERNAL_DEPRECATED short  __fastcall _RTC_Check_8_to_2(__int64 _Src);
    _RTCINTERNAL_DEPRECATED int    __fastcall _RTC_Check_8_to_4(__int64 _Src);
#endif

#ifdef _RTC_ADVMEM
    /* A memptr is a user pointer */
    typedef signed int memptr;
    /* A memref refers to a user pointer (ptr to ptr) */
    typedef memptr  *memref;
    /* memvals are the contents of a memptr */
    /* thus, they have sizes */
    typedef char    memval1;
    typedef short   memval2;
    typedef int     memval4;
    typedef __int64 memval8;
#endif
    
    /* Stack Checking Calls */
#if defined(_M_IX86)
    void   __cdecl     _RTC_CheckEsp();
#endif

#if !defined(_M_CEE) && !defined(_M_CEE_PURE)
    /* These unsupported functions are deprecated in native mode and not supported at all in /clr mode */

    _RTCINTERNAL_DEPRECATED  void   __fastcall _RTC_CheckStackVars(void *_Esp, _RTC_framedesc *_Fd);
    _RTCINTERNAL_DEPRECATED  void   __fastcall _RTC_CheckStackVars2(void *_Esp, _RTC_framedesc *_Fd, _RTC_ALLOCA_NODE *_AllocaList);
    _RTCINTERNAL_DEPRECATED  void   __fastcall _RTC_AllocaHelper(_RTC_ALLOCA_NODE *_PAllocaBase, size_t _CbSize, _RTC_ALLOCA_NODE **_PAllocaInfoList);
#ifdef _RTC_ADVMEM
    _RTCINTERNAL_DEPRECATED  void   __fastcall _RTC_MSAllocateFrame(memptr _Frame, _RTC_framedesc *_V);
    _RTCINTERNAL_DEPRECATED  void   __fastcall _RTC_MSFreeFrame(memptr _Frame, _RTC_framedesc *_V);
#endif
#endif
    /* Unintialized Local call */
    void   __cdecl     _RTC_UninitUse(const char *_Varname);

#if !defined(_M_CEE) && !defined(_M_CEE_PURE)
    /* These unsupported functions are deprecated in native mode and not supported at all in /clr mode */

#ifdef _RTC_ADVMEM
    /* Memory checks */
    _RTCINTERNAL_DEPRECATED void    __fastcall _RTC_MSPtrAssignAdd(memref _Dst, memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED void    __fastcall _RTC_MSAddrAssignAdd(memref _Dst, memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED void    __fastcall _RTC_MSPtrAssign(memref _Dst, memref _Src);
    
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrAssignR0(memref _Src);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrAssignR0Add(memref src, int _Offset);
    _RTCINTERNAL_DEPRECATED void    __fastcall _RTC_MSR0AssignPtr(memref _Dst, memptr _Src);
    _RTCINTERNAL_DEPRECATED void    __fastcall _RTC_MSR0AssignPtrAdd(memref _Dst, memptr _Src, int _Offset);
    
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrPushAdd(memref _Dst, memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSAddrPushAdd(memref _Dst, memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrPush(memref _Dst, memref _Src);
    
    _RTCINTERNAL_DEPRECATED memval1 __fastcall _RTC_MSPtrMemReadAdd1(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memval2 __fastcall _RTC_MSPtrMemReadAdd2(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memval4 __fastcall _RTC_MSPtrMemReadAdd4(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memval8 __fastcall _RTC_MSPtrMemReadAdd8(memref _Base, int _Offset);
    
    _RTCINTERNAL_DEPRECATED memval1 __fastcall _RTC_MSMemReadAdd1(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memval2 __fastcall _RTC_MSMemReadAdd2(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memval4 __fastcall _RTC_MSMemReadAdd4(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memval8 __fastcall _RTC_MSMemReadAdd8(memptr _Base, int _Offset);
    
    _RTCINTERNAL_DEPRECATED memval1 __fastcall _RTC_MSPtrMemRead1(memref _Base);
    _RTCINTERNAL_DEPRECATED memval2 __fastcall _RTC_MSPtrMemRead2(memref _Base);
    _RTCINTERNAL_DEPRECATED memval4 __fastcall _RTC_MSPtrMemRead4(memref _Base);
    _RTCINTERNAL_DEPRECATED memval8 __fastcall _RTC_MSPtrMemRead8(memref _Base);
    
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheckAdd1(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheckAdd2(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheckAdd4(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheckAdd8(memref _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheckAddN(memref _Base, int _Offset, unsigned _Size);
    
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSMemCheckAdd1(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSMemCheckAdd2(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSMemCheckAdd4(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSMemCheckAdd8(memptr _Base, int _Offset);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSMemCheckAddN(memptr _Base, int _Offset, unsigned _Size);
    
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheck1(memref _Base);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheck2(memref _Base);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheck4(memref _Base);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheck8(memref _Base);
    _RTCINTERNAL_DEPRECATED memptr  __fastcall _RTC_MSPtrMemCheckN(memref _Base, unsigned _Size);
#endif
#endif

    /* Subsystem initialization stuff */
    void    __cdecl    _RTC_Shutdown(void);
#ifdef _RTC_ADVMEM
    void    __cdecl    _RTC_InitAdvMem(void);
#endif
    void    __cdecl    _RTC_InitBase(void);
    

#ifdef  __cplusplus

    void* _ReturnAddress();
}

#endif

#pragma pack(pop)

#endif

#endif /* _INC_RTCAPI */
