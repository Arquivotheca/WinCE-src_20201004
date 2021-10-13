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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

#elif defined (KCOREDLL)

// Not using WIN32_CALL because the direct-call flag doesn't build properly
// when included in the kernel.  The direct-call flag is not necessary in this case.
#define CreateCS            (*(HANDLE (*) (LPCRITICAL_SECTION)) g_ppfnWin32Calls[W32_CRITCreate])

#define EnterCS(hcs, pcs)    _DIRECT_HANDLE_CALL(void, HT_CRITSEC, ID_CRIT_ENTER, 0, (HANDLE, LPCRITICAL_SECTION), 1, (hcs, pcs))
#define LeaveCS(hcs, pcs)    _DIRECT_HANDLE_CALL(void, HT_CRITSEC, ID_CRIT_LEAVE, 0, (HANDLE, LPCRITICAL_SECTION), 1, (hcs, pcs))
#define DeleteCS            CloseHandle

#else

#define _CRIT_CALL(type, api, args)	    IMPLICIT_DECL(type, HT_CRITSEC, ID_CRIT_ ## api, args)
#define WIN32_CALL(type, api, args)		IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)

// Create is WIN32 call, the rest are handle based calls
#define CreateCS    WIN32_CALL(HANDLE, CRITCreate, (LPCRITICAL_SECTION))

#define EnterCS     _CRIT_CALL(void, ENTER, (HANDLE, LPCRITICAL_SECTION))
#define LeaveCS     _CRIT_CALL(void, LEAVE, (HANDLE, LPCRITICAL_SECTION))
#define DeleteCS    CloseHandle

#endif


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
	if (!(lpcs->hCrit = CreateCS (lpcs))) {
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
    DeleteCS (lpcs->hCrit);
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

VOID WINAPI EnterCriticalSection(LPCRITICAL_SECTION lpcs) {
	HANDLE hTh = UCurThread();
    volatile CRITICAL_SECTION *vpcs = lpcs;
#ifdef DOCHECKCS
	CheckTakeCritSec(lpcs);
#elif defined (COREDLL) && !defined (SHIP_BUILD)
    if (!vpcs->hCrit || (0xcccccccc == (DWORD) vpcs->hCrit)) {
        RETAILMSG (1, (L"!ERROR: Calling EnterCriticalSection on a CS (%8.8lx) that is not initialized!\r\n", lpcs));
        DEBUGCHK (0);
    }
#endif
	DEBUGCHK(!((DWORD)hTh & 1));
	if (vpcs->OwnerThread == hTh)
		vpcs->LockCount++; /* We are the owner - increment count */
	else if (InterlockedTestExchange((LPLONG)&lpcs->OwnerThread,0,(LONG)hTh) == 0)
		vpcs->LockCount = 1;
	else {
        // Keep track of the number of contentions for perf measurements
        InterlockedIncrement(&(lpcs->dwContentions));
		EnterCS (lpcs->hCrit, lpcs);
    }
}

#ifdef COREDLL
BOOL WINAPI TryEnterCriticalSection(LPCRITICAL_SECTION lpcs) {
	BOOL bRet = TRUE;
	HANDLE hTh = UCurThread();
    volatile CRITICAL_SECTION *vpcs = lpcs;
#if !defined (SHIP_BUILD)
	DEBUGCHK(!((DWORD)hTh & 1));
    if (!vpcs->hCrit || (0xcccccccc == (DWORD) vpcs->hCrit)) {
        RETAILMSG (1, (L"!ERROR: Calling TryEnterCriticalSection on a CS (%8.8lx) that is not initialized!\r\n", lpcs));
        DEBUGCHK (0);
    }
#endif
	if (vpcs->OwnerThread == hTh)
		vpcs->LockCount++; /* We are the owner - increment count */
	else if (InterlockedTestExchange((LPLONG)&lpcs->OwnerThread,0,(LONG)hTh) == 0)
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
VOID WINAPI LeaveCriticalSection(LPCRITICAL_SECTION lpcs) {
HANDLE hTh = UCurThread();
    volatile CRITICAL_SECTION *vpcs = lpcs;
	/* Check for leave without enter */
	DEBUGCHK(vpcs->OwnerThread == hTh);
	if (vpcs->OwnerThread != hTh)
		return;
	if (vpcs->LockCount != 1)
		vpcs->LockCount--; /* Simply decrement count if more than 1 */
	else {
		vpcs->OwnerThread = (HANDLE)((DWORD)hTh | 1);
		if (vpcs->needtrap)
			LeaveCS (lpcs->hCrit, lpcs);
		else
			InterlockedTestExchange((PLONG)&vpcs->OwnerThread, (DWORD)hTh | 1, 0L);
	}
}


