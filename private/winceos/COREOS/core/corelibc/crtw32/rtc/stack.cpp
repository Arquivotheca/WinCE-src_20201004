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
*stack.cpp - RTC support
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*
*Revision History:
*       07-28-98  JWM   Module incorporated into CRTs (from KFrei)
*       05-11-99  KBF   Error if RTC support define not enabled
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       11-18-03  CSP   Added _alloca checking support  
*       08-18-04  SJ    Removing memset dependency in secfail.obj
*       10-01-04  AGH   Pass more information to _RTC_AllocaFailure.  Handle 
*                       exceptions due to corrupted RTC data.
*       04-11-05  PAL   Eliminate reference to memset altogether.
*
****/

#ifndef _RTC
#error  RunTime Check support not enabled!
#endif

#include "rtcpriv.h"

#if defined(_M_IX86)

/* Stack Checking Calls */
void
__declspec(naked) 
_RTC_CheckEsp() 
{
    __asm 
    {
        jne esperror    ; 
        ret

    esperror:
        ; function prolog

        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE

        push eax        ; save the old return value
        push edx

        push ebx
        push esi
        push edi
    }

    _RTC_Failure(_ReturnAddress(), _RTC_CHKSTK);

    __asm 
    {
        ; function epilog

        pop edi
        pop esi
        pop ebx

        pop edx         ; restore the old return value
        pop eax

        mov esp, ebp
        pop ebp
        ret
    }
}

#endif

#define STACKFILLPATTERN (0xcccccccc)

void __fastcall 
_RTC_CheckStackVars(void *frame, _RTC_framedesc *v)
{
    int i;
    for (i = 0; i < v->varCount; i++)
    {
        UNALIGNED int *head = (UNALIGNED int *)(((char *)frame) + v->variables[i].addr + v->variables[i].size);
        UNALIGNED int *tail = (UNALIGNED int *)(((char *)frame) + v->variables[i].addr - sizeof(int));
        
        if (*tail != STACKFILLPATTERN || *head != STACKFILLPATTERN) 
            _RTC_StackFailure(_ReturnAddress(), v->variables[i].name);
    }
}

/* 
 * memset maybe an actual call to the function even if we try to use the 
 * intrinsic. Replace memset call with the equivalent logic.
 */

#if defined(_M_IX86) || defined(_M_X64)
extern "C" void __stosb(BYTE *, BYTE, size_t);
#endif

// BE injects these at each _alloca site.
void __fastcall
_RTC_AllocaHelper(_RTC_ALLOCA_NODE *pAllocaBase,
                  size_t cbSize,
                  _RTC_ALLOCA_NODE **pAllocaInfoList)
{
    // validate arguments. let _RTC_CheckStackVars2 AV
    // later on.
    if (!pAllocaBase || !cbSize || !pAllocaInfoList)
        return;

    // fill _alloca with 0xcc
#if defined(_M_IX86) || defined(_M_X64)
    __stosb((BYTE*)pAllocaBase, (BYTE)STACKFILLPATTERN, cbSize);
#else
    {
        size_t i;
        BYTE *p;

        for (i = 0, p = (BYTE*)pAllocaBase; i < cbSize; i++) {
            *p++ = (BYTE)STACKFILLPATTERN;
        }
    }
#endif
    
    // update embedded _alloca checking data
    pAllocaBase->next = *pAllocaInfoList;
    pAllocaBase->allocaSize = cbSize;

    // update function _alloca list, to loop through at function
    // exit and check.
    *pAllocaInfoList = pAllocaBase;
}

// Extended version of _RTC_CheckStackVars, which handles
// _alloca off-by-one error checking.
void __fastcall
_RTC_CheckStackVars2(void *frame,
                     _RTC_framedesc *v,
                     _RTC_ALLOCA_NODE *allocaList)
{
    // Repeat original _RTC_CheckStackVars functionality, if frame descriptor
    // provided.
    if (v) {
        int i;
        for (i = 0; i < v->varCount; i++)
        {
            UNALIGNED int *head = (UNALIGNED int *)(((char *)frame) + v->variables[i].addr + v->variables[i].size);
            UNALIGNED int *tail = (UNALIGNED int *)(((char *)frame) + v->variables[i].addr - sizeof(int));

            if (*tail != STACKFILLPATTERN || *head != STACKFILLPATTERN) 
                _RTC_StackFailure(_ReturnAddress(), v->variables[i].name);
        }
    }

    // Loop through allocaList, checking for possible _alloca memory
    // mishandling.
    _RTC_ALLOCA_NODE *pn = allocaList;
    int count = 0;
    while ( pn )
    {
        count++;
        pn = pn->next;
    }
    pn = allocaList;
    
    while (pn) {        
        // Check head of _alloca for corruption.  There are
        // 3 guards because there is space for it, assuming
        // _alloca's have to be aligned to 16 bytes.
        if (   pn->guard1    != STACKFILLPATTERN
            || pn->guard2[0] != STACKFILLPATTERN
            || pn->guard2[1] != STACKFILLPATTERN
            || pn->guard2[2] != STACKFILLPATTERN)
        {
            _RTC_AllocaFailure(_ReturnAddress(), pn, count);
        }

        // Tail of _alloca - sizeof the guard to get the address of the guard
        UNALIGNED __int32 *pGuard = (UNALIGNED __int32 *)
            ((char *)pn + pn->allocaSize - sizeof(__int32));

        if (*pGuard != STACKFILLPATTERN) {
            _RTC_AllocaFailure(_ReturnAddress(), pn, count);
        }

        pn = pn->next;
        count--;
    }
}
