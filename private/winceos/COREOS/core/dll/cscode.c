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
// included from dbg.c, apis.c, and stubs.c

#include <ntstatus.h>

#define ID_CRIT_ENTER   ID_HCALL(0)
#define ID_CRIT_LEAVE   ID_HCALL(1)

#if defined (KERN_CORE)

// kernel explicitly define OwnerThread such that it will not compile if referenced.
// we need to undefine it in this file as we'll reference OwnerThread here.
#undef OwnerThread

//
// KERN_CORE should only be defined for things linked directly into kernel
// AND References Critical section in kernel directly.
//
#undef UCurThread
#define UCurThread()    ((HANDLE) dwCurThId)
#define EnterCS         CRITEnter
#define LeaveCS         CRITLeave
#define CreateCS        CRITCreate
#define DeleteCS        CRITDelete

#define IS_PROCESS_EXITING 0

extern void CheckTakeCritSec(LPCRITICAL_SECTION lpcs);

#else

EXTERN_C VOID WINAPI xxx_RaiseException (DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD cArgs, CONST DWORD * lpArgs);

#if defined (KCOREDLL)

#define ID_CRIT_DELETE   ID_HCALL(2)

// Not using WIN32_CALL because the direct-call flag doesn't build properly
// when included in the kernel.  The direct-call flag is not necessary in this case.
#define CreateCS            (*(HANDLE (*) (LPCRITICAL_SECTION)) g_ppfnWin32Calls[W32_CRITCreate])

#define EnterCS             (*(void (*) (HANDLE, LPCRITICAL_SECTION)) g_ppfnCritCalls[ID_CRIT_ENTER])
#define LeaveCS             (*(void (*) (HANDLE, LPCRITICAL_SECTION)) g_ppfnCritCalls[ID_CRIT_LEAVE])
#define DeleteCS            (*(void (*) (HANDLE)) g_ppfnCritCalls[ID_CRIT_DELETE])

#define IS_PROCESS_EXITING IsExiting

#else

#define _CRIT_CALL(type, api, args)     IMPLICIT_DECL(type, HT_CRITSEC, ID_CRIT_ ## api, args)
#define WIN32_CALL(type, api, args)     IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)

// Create is WIN32 call, the rest are handle based calls
#define CreateCS    WIN32_CALL(HANDLE, CRITCreate, (LPCRITICAL_SECTION))

#define EnterCS     _CRIT_CALL(void, ENTER, (HANDLE, LPCRITICAL_SECTION))
#define LeaveCS     _CRIT_CALL(void, LEAVE, (HANDLE, LPCRITICAL_SECTION))
#define DeleteCS    CloseHandle

#define IS_PROCESS_EXITING IsExiting

#endif // KCOREDLL
#endif // KERN_CORE


/*
    @doc BOTH EXTERNAL

    @func VOID | InitializeCriticalSection | Initializes a critical section object.
    @parm LPCRITICAL_SECTION | lpcsCriticalSection | address of critical section object

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
VOID WINAPI InitializeCriticalSection(__out LPCRITICAL_SECTION lpcs)
{

    memset (lpcs, 0, sizeof (CRITICAL_SECTION));
    lpcs->hCrit = CreateCS (lpcs);
    if (!lpcs->hCrit) {
        RETAILMSG (1, (L"InitializeCriticalSection failed, lpcs = %8.8lx\r\n", lpcs));
        DEBUGCHK(0);
    }
}

/*
    @doc BOTH EXTERNAL

    @func VOID | DeleteCriticalSection | Releases all resources used by an unowned
    critical section object.
    @parm LPCRITICAL_SECTION | lpCriticalSection | address of critical section object

    @comm Follows the Win32 reference description without restrictions or modifications.

*/

VOID WINAPI DeleteCriticalSection(__inout LPCRITICAL_SECTION lpcs) {
    /* Just clear out the fields */

    // No other thread should be currently waiting on or own the CS
    // Make an exception for process exit and current thread owning CS
    // since too much existing code gets this wrong.
    ASSERT_RETAIL (IS_PROCESS_EXITING ||
        (!lpcs->needtrap && (!IS_CS_OWNED (lpcs) || (OWNER_ID_OF_CS (lpcs) == PcbGetCurThreadId ()))));

    if (!lpcs->hCrit) {
        DEBUGMSG (1, (L"Deleting an uninitialized critical section 0x%8.8lx, ignored!\n", lpcs));

    } else {
        DeleteCS (lpcs->hCrit);
    }

    memset(lpcs,0,sizeof(CRITICAL_SECTION));
}

/* If fastpath, adjust critical section, else trap to kernel for rescheduling */

/*
    @doc BOTH EXTERNAL

    @func VOID | EnterCriticalSection | Waits for ownership of the specified critical
    section object. The function returns when the calling thread is granted ownership.
    @parm LPCRITICAL_SECTION | lpCriticalSection | address of critical section object

    @comm Follows the Win32 reference description without restrictions or modifications.

*/

VOID WINAPI EnterCriticalSection(__inout LPCRITICAL_SECTION lpcs)
{
    DWORD dwThId = PcbGetCurThreadId ();
    volatile CRITICAL_SECTION *vpcs = lpcs;

#ifndef SHIP_BUILD
    HANDLE hOldCrit;
#endif

#ifdef DOCHECKCS
    CheckTakeCritSec(lpcs);
#elif defined (COREDLL) && !defined (SHIP_BUILD)
    if (!vpcs->hCrit || (0xcccccccc == (DWORD) vpcs->hCrit)) {
        RETAILMSG (1, (L"!ERROR: Calling EnterCriticalSection on a CS (%8.8lx) that is not initialized!\r\n", lpcs));
        xxx_RaiseException((DWORD)STATUS_INVALID_HANDLE, 0, 0, NULL);
    }
#endif

    DEBUGCHK(!(dwThId & 1));
    if (OWNER_ID_OF_CS (vpcs) != dwThId) {

        // try to grab the CS
        if (InterlockedCompareExchange (&lpcs->lOwnerInfo, dwThId, 0) != 0) {

            // CS is contented
            LONG lOwnerInfo;

            // increment needtrap count. NOTE: this must be done before setting trap bit or there can be race.        
            InterlockedIncrement ((PLONG)&lpcs->needtrap);
            
            // try to set the trap bit, or grab the CS
            do {
                lOwnerInfo = vpcs->lOwnerInfo;
            } while (InterlockedCompareExchange (&vpcs->lOwnerInfo, lOwnerInfo? (lOwnerInfo|1) : dwThId, lOwnerInfo) != lOwnerInfo);


            // at this point, either we own the CS, or the trap bit is set and LeaveCriticalSection will always trap
            if (OWNER_ID_OF_CS (vpcs) != dwThId) {

#ifndef SHIP_BUILD
                hOldCrit = lpcs->hCrit;
                EnterCS (lpcs->hCrit, lpcs);
                if (hOldCrit != vpcs->hCrit)
                {
                    RETAILMSG (1, (L"!ERROR: CRITICAL_SECTION %8.8lx freed/corrupted while in EnterCriticalSection!\r\n", lpcs));
                    DebugBreak();
                }
#else
                EnterCS (lpcs->hCrit, lpcs);
#endif
            }

            // decrement needtrap count
            InterlockedDecrement ((PLONG)&lpcs->needtrap);
        }

        // got the CS, update OwnerThread and LockCount
        vpcs->OwnerThread = (HANDLE) dwThId;
        vpcs->LockCount   = 1;

    } else {
        vpcs->LockCount++; /* We are the owner - increment count */
    }
}

BOOL WINAPI TryEnterCriticalSection(__inout LPCRITICAL_SECTION lpcs)
{
    BOOL bRet = TRUE;
    DWORD dwThId = PcbGetCurThreadId ();
    volatile CRITICAL_SECTION *vpcs = lpcs;

#if !defined (SHIP_BUILD) && defined (COREDLL)
    DEBUGCHK(!(dwThId & 1));
    if (!vpcs->hCrit || (0xcccccccc == (DWORD) vpcs->hCrit)) {
        RETAILMSG (1, (L"!ERROR: Calling TryEnterCriticalSection on a CS (%8.8lx) that is not initialized!\r\n", lpcs));
        xxx_RaiseException((DWORD)STATUS_INVALID_HANDLE, 0, 0, NULL);
    }
#endif

    if (OWNER_ID_OF_CS (lpcs) == dwThId) {
        vpcs->LockCount++; /* We are the owner - increment count */

    } else if (InterlockedCompareExchange (&lpcs->lOwnerInfo, dwThId, 0) == 0) {
        vpcs->OwnerThread = (HANDLE) dwThId;
        vpcs->LockCount = 1;

    } else {
        bRet = FALSE;
    }

    return bRet;
}


/* If fastpath, adjust critical section, else trap to kernel for rescheduling */

/*
    @doc BOTH EXTERNAL

    @func VOID | LeaveCriticalSection | Releases ownership of the specified critical
    section object.
    @parm LPCRITICAL_SECTION | lpcsCriticalSection | address of critical section object

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
VOID WINAPI LeaveCriticalSection(__inout LPCRITICAL_SECTION lpcs)
{
    DWORD dwThId = PcbGetCurThreadId ();
    volatile CRITICAL_SECTION *vpcs = lpcs;

    /* Check for leave without enter */
    if (OWNER_ID_OF_CS (vpcs) == dwThId) {

        if (vpcs->LockCount == 1) {

            // clear OwnerThread before actually releasing the CS
            vpcs->OwnerThread = NULL;

            if (vpcs->needtrap
                || (InterlockedCompareExchange (&vpcs->lOwnerInfo, 0, dwThId) != (LONG) dwThId)) {
                // either needtrap is set or trap-bit is set. Trap into kernel to release the CS
                LeaveCS (lpcs->hCrit, lpcs);
            }
        } else {
            vpcs->LockCount--; /* Simply decrement count if more than 1 */
        }
    }
#if !defined (SHIP_BUILD)
    else {
        RETAILMSG (1, (L"!!ERROR: Calling LeaveCriticalSection without calling EnterCriticalSection\r\n"));
        RETAILMSG (1, (L"         dwThId = %8.8lx, vpcs->lOwnerInfo = %8.8lx, lpcs = %8.8lx\r\n", dwThId, vpcs->lOwnerInfo, vpcs));
        DebugBreak();
    }
#endif
}


