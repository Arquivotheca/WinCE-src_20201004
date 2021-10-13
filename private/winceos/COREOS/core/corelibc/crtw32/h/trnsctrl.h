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
*trnsctrl.h - routines that do special transfer of control
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Declaration of routines that do special transfer of control.
*       (and a few other implementation-dependant things).
*
*       Implementations of these routines are in assembly (very implementation
*       dependant).  Currently, these are implemented as naked functions with
*       inline asm.
*
*       [Internal]
*
*Revision History:
*       05-24-93  BS    Module created.
*       03-03-94  TL    Added Mips (_M_MRX000 >= 4000) changes
*       09-02-94  SKS   This header file added.
*       09-13-94  GJF   Merged in changes from/for DEC Alpha (from Al Doser,
*                       dated 6/21).
*       10-09-94  BWT   Add unknown machine merge from John Morgan
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       04-11-95  JWM   _CallSettingFrame() is now extern "C".
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       06-01-97  TL    Added P7 changes
*       05-17-99  PML   Remove all Macintosh support.
*       06-05-01  GB    AMD64 Eh support Added.
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       04-02-04  AJS   Exported _CreateFrameInfo, _IsExceptionObjectToBeDestroyed,
*                       and _FindAndUnlinkFrame for use from managed code.
*       04-07-04  MSL   Double slash removal
*       10-08-04  AGH   Format #pragma message like a warning
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       03-08-05  PAL   Include crtdefs.h to define _CRT_PACKING.
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_TRNSCTRL
#define _INC_TRNSCTRL

#include <crtdefs.h>

#pragma pack(push,_CRT_PACKING)

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#if defined(_M_X64) /*IFSTRIP=IGN*/

typedef struct FrameInfo {
    PVOID               pExceptionObject;   
    struct FrameInfo *  pNext;
} FRAMEINFO;

extern void             _UnlinkFrame(FRAMEINFO *pFrameInfo);
extern FRAMEINFO*       _FindFrameInfo(PVOID, FRAMEINFO*);
extern VOID             _JumpToContinuation(unsigned __int64, CONTEXT*, EHExceptionRecord*);
extern BOOL             _ExecutionInCatch(DispatcherContext*,  FuncInfo*);
extern VOID             __FrameUnwindToEmptyState(EHRegistrationNode*, DispatcherContext*, FuncInfo*);
extern "C" _CRTIMP VOID __cdecl _FindAndUnlinkFrame(FRAMEINFO *);
extern "C" _CRTIMP FRAMEINFO* __cdecl _CreateFrameInfo(FRAMEINFO*, PVOID);
extern "C" _CRTIMP int __cdecl _IsExceptionObjectToBeDestroyed(PVOID);
extern "C" VOID         _UnwindNestedFrames( EHRegistrationNode*,
                                             EHExceptionRecord*,
                                             CONTEXT* ,
                                             EHRegistrationNode*,
                                             void*,
                                             __ehstate_t,
                                             FuncInfo *,
                                             DispatcherContext*,
                                             BOOLEAN
                                             );
extern "C" PVOID        __CxxCallCatchBlock(EXCEPTION_RECORD*);
extern "C" PVOID        _CallSettingFrame( void*, EHRegistrationNode*, ULONG );
extern "C" BOOL         _CallSETranslator( EHExceptionRecord*,
                                           EHRegistrationNode*,
                                           CONTEXT*,
                                           DispatcherContext*,
                                           FuncInfo*,
                                           ULONG,
                                           EHRegistrationNode*);
extern "C" VOID         _EHRestoreContext(CONTEXT* pContext);
extern "C" _CRTIMP unsigned __int64 __cdecl _GetImageBase(VOID);
extern "C" _CRTIMP unsigned __int64 __cdecl _GetThrowImageBase(VOID);
extern "C" _CRTIMP VOID __cdecl _SetImageBase(unsigned __int64 ImageBaseToRestore);
extern "C" _CRTIMP VOID __cdecl _SetThrowImageBase(unsigned __int64 NewThrowImageBase);
extern TryBlockMapEntry *_GetRangeOfTrysToCheck(EHRegistrationNode *,
                                                FuncInfo *,
                                                int,
                                                __ehstate_t,
                                                unsigned *,
                                                unsigned *,
                                                DispatcherContext*
                                                );
extern EHRegistrationNode *_GetEstablisherFrame(EHRegistrationNode*,
                                                DispatcherContext*,
                                                FuncInfo*,
                                                EHRegistrationNode*
                                                );

#define _CallMemberFunction0(pthis, pmfn)               (*(VOID(*)(PVOID))pmfn)(pthis)
#define _CallMemberFunction1(pthis, pmfn, pthat)        (*(VOID(*)(PVOID,PVOID))pmfn)(pthis,pthat)
#define _CallMemberFunction2(pthis, pmfn, pthat, val2 ) (*(VOID(*)(PVOID,PVOID,int))pmfn)(pthis,pthat,val2)

#define OffsetToAddress( off, FP )  (void*)(((char*)(FP)) + (off))

#define UNWINDSTATE(base,offset) *((int*)((char*)base + offset))
#define UNWINDTRYBLOCK(base,offset) *((int*)( (char*)(OffsetToAddress(offset,base)) + 4 ))
#define UNWINDHELP(base,offset) *((__int64*)((char*)base + offset))

#elif   defined(_M_ARM) /*IFSTRIP=IGN*/

typedef struct FrameInfo {
	PVOID				pExceptionObject;
    EHRegistrationNode *pRN;
	struct _s_FuncInfo *pFuncInfo;
    __ehstate_t         state;
    struct FrameInfo *  pNext;
} FRAMEINFO;

extern FRAMEINFO *FindFrameInfo(EHRegistrationNode *);
extern FRAMEINFO *FindCatchFrameInfo(EHRegistrationNode *);

extern void             _UnlinkFrame(FRAMEINFO *pFrameInfo);

extern FRAMEINFO*       _FindFrameInfo(PVOID, FRAMEINFO*);

extern void             _JumpToContinuation( void *, EHRegistrationNode * );

extern BOOL             _ExecutionInCatch(DispatcherContext*,  FuncInfo*);

extern VOID             __FrameUnwindToEmptyState(EHRegistrationNode*, DispatcherContext*, FuncInfo*);

#define IsExceptionObjectToBeDestroyed(p) _IsExceptionObjectToBeDestroyed(p)

//extern void 			_UnwindNestedFrames( EHRegistrationNode*, EHExceptionRecord* );
extern "C" VOID         _UnwindNestedFrames( EHRegistrationNode*,
                                             EHExceptionRecord*,
                                             CONTEXT* ,
                                             EHRegistrationNode*,
                                             void*,
                                             __ehstate_t,
                                             FuncInfo *,
                                             DispatcherContext*,
                                             BOOLEAN
                                             );

extern "C" PVOID        __CxxCallCatchBlock(EXCEPTION_RECORD*);

#if defined(_M_ARM_NT)
extern "C" PVOID        _CallSettingFrame( void*, EHRegistrationNode*, PULONG, ULONG );
#else
extern "C" PVOID        _CallSettingFrame( void*, EHRegistrationNode*, ULONG );
#endif

extern "C" BOOL         _CallSETranslator( EHExceptionRecord*,
                                           EHRegistrationNode*,
                                           CONTEXT*,
                                           DispatcherContext*,
                                           FuncInfo*,
                                           ULONG,
                                           EHRegistrationNode*);

extern "C" VOID         _EHRestoreContext(CONTEXT* pContext);

extern "C" unsigned __int32      _GetImageBase(VOID);

extern "C" void         _SetImageBase(unsigned __int32 ImageBaseToRestore);

extern "C" unsigned __int32      _GetThrowImageBase(VOID);

extern "C" VOID         _SetThrowImageBase(unsigned __int32 NewThrowImageBase);

extern TryBlockMapEntry *_GetRangeOfTrysToCheck(EHRegistrationNode *,
                                                FuncInfo *,
                                                int,
                                                __ehstate_t,
                                                unsigned *,
                                                unsigned *,
                                                DispatcherContext*
                                                );

extern EHRegistrationNode *_GetEstablisherFrame(EHRegistrationNode*,
                                                DispatcherContext*,
                                                FuncInfo*,
                                                EHRegistrationNode*
                                                );
/*
For calling member functions:
*/
extern void  _CallMemberFunction0( void *pthis, void *pmfn );
extern void  _CallMemberFunction1( void *pthis, void *pmfn, void *pthat );
extern void  _CallMemberFunction2( void *pthis, void *pmfn, void *pthat, int val2 );

//
// Do the nitty-gritty of calling the catch block safely
//
void *_CallCatchBlock2( EHRegistrationNode *, FuncInfo*, void*, int, unsigned long );

typedef struct CatchFrameInfo {
    EHRegistrationNode *pRN ;
    ULONG   NestLevel ;
    FuncInfo *pFuncInfo ;
    struct  CatchFrameInfo  *next ;
} CATCHFRAMEINFO ;

extern EHRegistrationNode *FindCatchFrame (EHRegistrationNode *pRN, ULONG pNestLevel);
extern EHRegistrationNode *FindFrameForUnwind (EHRegistrationNode *pRN, FuncInfo *pFuncInfo) ;

#define OffsetToAddress( off, FP )  (void*)(((char*)FP) + off)

#define UNWINDSTATE(base,offset) *((int*)((char*)base + offset))
#define UNWINDTRYBLOCK(base,offset) *((int*)( (char*)(OffsetToAddress(offset,base)) + 4 ))
#define UNWINDHELP(base,offset) *((__int64*)((char*)base + offset))

#elif   defined(_M_IX86)  /*      x86 */

/*
For calling funclets (including the catch)
*/
extern "C" void * __stdcall _CallSettingFrame( void *, EHRegistrationNode *, unsigned long );
extern void   __stdcall _JumpToContinuation( void *, EHRegistrationNode * );

/*
For calling member functions:
*/
extern void __stdcall _CallMemberFunction0( void *pthis, void *pmfn );
extern void __stdcall _CallMemberFunction1( void *pthis, void *pmfn, void *pthat );
extern void __stdcall _CallMemberFunction2( void *pthis, void *pmfn, void *pthat, int val2 );

/*
Translate an ebp-relative offset to a hard address based on address of
registration node:
*/
#if     !CC_EXPLICITFRAME
#define OffsetToAddress( off, RN )      \
                (void*)((char*)(RN) \
                                + FRAME_OFFSET \
                                + (off))
#else
#define OffsetToAddress( off, RN )      (void*)(((char*)((RN)->frame)) + (off))
#endif

/*
Call RtlUnwind in a returning fassion
*/
extern void __stdcall _UnwindNestedFrames( EHRegistrationNode*, EHExceptionRecord* );

/*
Do the nitty-gritty of calling the catch block safely
*/
void *_CallCatchBlock2( EHRegistrationNode *, FuncInfo*, void*, int, unsigned long );

/*
Link together all existing catch objects to determine when they should be destroyed
*/
typedef struct FrameInfo {
    PVOID               pExceptionObject;   
    struct FrameInfo *  pNext;
} FRAMEINFO;

/*
Ditto the SE translator
*/
BOOL _CallSETranslator( EHExceptionRecord*, EHRegistrationNode*, void*, DispatcherContext*, FuncInfo*, int, EHRegistrationNode*);

extern TryBlockMapEntry *_GetRangeOfTrysToCheck(FuncInfo *, int, __ehstate_t, unsigned *, unsigned *);
#else

#include <crtwrn.h>
#pragma _CRT_WARNING( _NO_SPECIAL_TRANSFER )

#endif

extern "C" _CRTIMP FRAMEINFO * __cdecl _CreateFrameInfo(FRAMEINFO*, PVOID);
extern "C" _CRTIMP BOOL __cdecl _IsExceptionObjectToBeDestroyed(PVOID);
extern "C" _CRTIMP void __cdecl _FindAndUnlinkFrame(FRAMEINFO*);

typedef struct WinRTExceptionInfo
{
    void* description;
    void* restrictedErrorString;
    void* restrictedErrorReference;
    void* capabilitySid;
    long hr;
    void* restrictedInfo;
    ThrowInfo* throwInfo;
} WINRTEXCEPTIONINFO;

#pragma pack(pop)

#endif  /* _INC_TRNSCTRL */
