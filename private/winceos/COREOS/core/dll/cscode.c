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

#define ID_CRIT_ENTER   ID_HCALL(0)
#define ID_CRIT_LEAVE   ID_HCALL(1)

#if defined (KERN_CORE)
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

#else

#define _CRIT_CALL(type, api, args)     IMPLICIT_DECL(type, HT_CRITSEC, ID_CRIT_ ## api, args)
#define WIN32_CALL(type, api, args)     IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)

// Create is WIN32 call, the rest are handle based calls
#define CreateCS    WIN32_CALL(HANDLE, CRITCreate, (LPCRITICAL_SECTION))

#define EnterCS     _CRIT_CALL(void, ENTER, (HANDLE, LPCRITICAL_SECTION))
#define LeaveCS     _CRIT_CALL(void, LEAVE, (HANDLE, LPCRITICAL_SECTION))
#define DeleteCS    CloseHandle

#endif // KCOREDLL
#endif // KERN_CORE


/*
    @doc BOTH EXTERNAL

    @func VOID | InitializeCriticalSection | Initializes a critical section object. 
    @parm LPCRITICAL_SECTION | lpcsCriticalSection | address of critical section object  

    @comm Follows the Win32 reference description without restrictions or modifications. 
*/
VOID WINAPI InitializeCriticalSection(LPCRITICAL_SECTION lpcs) {
    /* Init the structure */
    lpcs->LockCount = 0;
    lpcs->OwnerThread = 0;
    lpcs->needtrap = 0;
    lpcs->dwContentions = 0;
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

VOID WINAPI DeleteCriticalSection(LPCRITICAL_SECTION lpcs) {
    /* Just clear out the fields */
    // the debug check is move into kernel to provide more precise analysis
    // DEBUGCHK(!lpcs->OwnerThread);
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

VOID WINAPI EnterCriticalSection(LPCRITICAL_SECTION lpcs)
{
    DWORD dwThId = PcbGetCurThreadId ();
    volatile CRITICAL_SECTION *vpcs = lpcs;

#ifdef DOCHECKCS
    CheckTakeCritSec(lpcs);
#elif defined (COREDLL) && !defined (SHIP_BUILD)
    if (!vpcs->hCrit || (0xcccccccc == (DWORD) vpcs->hCrit)) {
        RETAILMSG (1, (L"!ERROR: Calling EnterCriticalSection on a CS (%8.8lx) that is not initialized!\r\n", lpcs));
        xxx_RaiseException((DWORD)STATUS_INVALID_HANDLE, 0, 0, NULL);
    }
#endif

    DEBUGCHK(!(dwThId & 1));
    if (vpcs->OwnerThread != (HANDLE) dwThId)
        if (InterlockedTestExchange((LPLONG)&lpcs->OwnerThread,0,dwThId) == 0) {
            vpcs->LockCount = 1;
        } else {
            InterlockedIncrement ((PLONG)&lpcs->needtrap);
            InterlockedIncrement ((PLONG)&lpcs->dwContentions);// Keep track of the number of contentions for perf measurements
            EnterCS (lpcs->hCrit, lpcs);
            InterlockedDecrement ((PLONG)&lpcs->needtrap);
        }
    else {
        vpcs->LockCount++; /* We are the owner - increment count */
    }
}

#ifdef COREDLL
BOOL WINAPI TryEnterCriticalSection(LPCRITICAL_SECTION lpcs)
{
    BOOL bRet = TRUE;
    DWORD dwThId = PcbGetCurThreadId ();
    volatile CRITICAL_SECTION *vpcs = lpcs;
    
#if !defined (SHIP_BUILD)
    DEBUGCHK(!(dwThId & 1));
    if (!vpcs->hCrit || (0xcccccccc == (DWORD) vpcs->hCrit)) {
        RETAILMSG (1, (L"!ERROR: Calling TryEnterCriticalSection on a CS (%8.8lx) that is not initialized!\r\n", lpcs));
        xxx_RaiseException((DWORD)STATUS_INVALID_HANDLE, 0, 0, NULL);

    }
#endif

    if (vpcs->OwnerThread == (HANDLE) dwThId)
        vpcs->LockCount++; /* We are the owner - increment count */
    else if (InterlockedTestExchange((LPLONG)&lpcs->OwnerThread,0,dwThId) == 0)
        vpcs->LockCount = 1;
    else
        bRet = FALSE;
    return bRet;
}
#endif

/* If fastpath, adjust critical section, else trap to kernel for rescheduling */

/*
    @doc BOTH EXTERNAL
    
    @func VOID | LeaveCriticalSection | Releases ownership of the specified critical 
    section object. 
    @parm LPCRITICAL_SECTION | lpcsCriticalSection | address of critical section object  

    @comm Follows the Win32 reference description without restrictions or modifications. 
*/
VOID WINAPI LeaveCriticalSection(LPCRITICAL_SECTION lpcs)
{
    DWORD dwThId = PcbGetCurThreadId ();
    volatile CRITICAL_SECTION *vpcs = lpcs;

    /* Check for leave without enter */
    if (vpcs->OwnerThread == (HANDLE) dwThId) {
        if (vpcs->LockCount == 1) {
            // set "vpcs->OwnerThread = (HANDLE)((DWORD)hTh | 1)". use interlock to guarantee memory barrier
            InterlockedIncrement ((PLONG)&vpcs->OwnerThread);
            if (vpcs->needtrap)
                LeaveCS (lpcs->hCrit, lpcs);
            else
                InterlockedTestExchange((PLONG)&vpcs->OwnerThread, dwThId | 1, 0L);
        } else {
            vpcs->LockCount--; /* Simply decrement count if more than 1 */
        }
    }
#if !defined (SHIP_BUILD)
    else {
        RETAILMSG (1, (L"!!ERROR: Calling LeaveCriticalSection without calling EnterCriticalSection\r\n"));
        RETAILMSG (1, (L"         dwThId = %8.8lx, vpcs->OwnerThread = %8.8lx, lpcs = %8.8lx\r\n", dwThId, vpcs->OwnerThread, vpcs));
        DebugBreak ();
    }
#endif
}


