//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// included from dbg.c, apis.c, and stubs.c

#ifdef IN_KERNEL
#undef UCurThread
#define UCurThread() hCurThread
#endif

#include "lmemdebug.h"
PFN_LMEMAddTrackedItem v_pfnLMEMAddTrackedItem;
PFN_LMEMRemoveTrackedItem v_pfnLMEMRemoveTrackedItem;

extern void CheckTakeCritSec(LPCRITICAL_SECTION lpcs);

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
	lpcs->hCrit = CreateCrit(lpcs);
	DEBUGCHK(lpcs->hCrit);
    if (v_pfnLMEMAddTrackedItem) {
        (v_pfnLMEMAddTrackedItem) (0, lpcs, LMEM_ALLOC_SIZE_CS, 0, (PCHAR)0);
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
    if (v_pfnLMEMAddTrackedItem) {
        (v_pfnLMEMRemoveTrackedItem) (0, lpcs);
    }
    CreateCrit((LPCRITICAL_SECTION)((DWORD)lpcs|1));
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


#ifdef ARM
#pragma optimize("",off)
#endif

VOID WINAPI EnterCriticalSection(LPCRITICAL_SECTION lpcs) {
	HANDLE hTh = UCurThread();
    volatile CRITICAL_SECTION *vpcs = lpcs;
#ifdef DOCHECKCS
	CheckTakeCritSec(lpcs);
#else
    DEBUGCHK (vpcs->hCrit && (0xcccccccc != (DWORD) vpcs->hCrit));
#endif
	DEBUGCHK(!((DWORD)hTh & 1));
	if (vpcs->OwnerThread == hTh)
		vpcs->LockCount++; /* We are the owner - increment count */
	else if (InterlockedTestExchange((LPLONG)&lpcs->OwnerThread,0,(LONG)hTh) == 0)
		vpcs->LockCount = 1;
	else {
        // Keep track of the number of contentions for perf measurements
        InterlockedIncrement(&(lpcs->dwContentions));
		TakeCritSec(lpcs);
    }
}

#ifdef COREDLL
BOOL WINAPI TryEnterCriticalSection(LPCRITICAL_SECTION lpcs) {
	BOOL bRet = TRUE;
	HANDLE hTh = UCurThread();
    volatile CRITICAL_SECTION *vpcs = lpcs;
	DEBUGCHK(!((DWORD)hTh & 1));
    DEBUGCHK (vpcs->hCrit && (0xcccccccc != (DWORD) vpcs->hCrit));
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
			LeaveCritSec(lpcs);
		else
			InterlockedTestExchange((PLONG)&vpcs->OwnerThread, (DWORD)hTh | 1, 0L);
	}
}


#ifdef ARM
#pragma optimize("",on)
#endif

