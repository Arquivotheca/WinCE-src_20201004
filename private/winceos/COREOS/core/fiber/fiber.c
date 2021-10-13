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
#include <windows.h>
#include <coredll.h>

#ifdef  _MIPS64
#define REGISTER    ULONGLONG
#else
#define REGISTER    ULONG
#endif

// the fiber context structure. 
typedef struct _FIBERCTX {
    REGISTER rgLr;             // return address, must be first
    REGISTER rgArg1;           // arg 1, must be 2nd
    REGISTER rgArg2;           // arg 2, must be 3rd
    REGISTER rgWhatEver[1];    // other register, size is machine dependent
} FIBERCTX, *PFIBERCTX;

#define SIZE_RESERVED   (16*sizeof(DWORD))

// the following values MUST be invalid for any handle and MUST BE odd number.
#define FIBER_SWITCHING     ((DWORD) -1)
#define FIBER_DELETING      ((DWORD) -3)

extern const DWORD dwSizeCtx;

extern BOOL xxx_TerminateThread(
    HANDLE  hThread,
    DWORD   dwExitcode
    );


void MDFiberSwitch (PFIBERSTRUCT pFiber, DWORD dwThrdId, LPDWORD pTlsPtr, BOOL fFpuUsed);

static void FiberBaseFunc (LPFIBER_START_ROUTINE pfnFiberProc, LPVOID lpParam)
{
    PFIBERSTRUCT pFiber;
    DWORD dwThrdId;
    //NKDbgPrintfW (L"pfnFiberProc = %8.8lx, lpParam = %8.8lx\r\n", pfnFiberProc, lpParam);
    pfnFiberProc (lpParam);
    pFiber = GetCurrentFiber ();
    dwThrdId = GetCurrentThreadId ();
    if (InterlockedTestExchange ((PLONG) &pFiber->dwThrdId, dwThrdId, FIBER_SWITCHING) != (LONG)dwThrdId) {
        // we are being deleted, let him do it
        Sleep (INFINITE);
    }
    pFiber->dwThrdId = dwThrdId | 1;// restore and set LSB, so the PFIBERSTRUCT can be freed
    ExitThread (0);
}

LPVOID WINAPI CreateFiber (DWORD dwStackSize, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter)
{
    static DWORD dwPageSize;    // page size
    PFIBERSTRUCT pFiber;        // the fiber
    LPVOID       lpStack;       // stack of the fiber
    DWORD        cbStack;       // size of stack
    LPDWORD      lpArgs;        // pointer to arguments on stack
    PFIBERCTX    pFiberCtx;     // pointer to fiber context on stack
    
    // figure out page size and segment size
    if (!dwPageSize) {
        SYSTEM_INFO si;
        GetSystemInfo (&si);
        dwPageSize = si.dwPageSize;
    }

    // validate arguments
    if (!lpStartAddress) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return NULL;
    }

    // allocate a fiber structure and initialize it
    pFiber = (PFIBERSTRUCT) LocalAlloc(LMEM_FIXED, sizeof(FIBERSTRUCT));
    if (!pFiber) {
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    // stack size always the same as the thread's stack size
    cbStack = UStkSize ();

    DEBUGMSG (1, (L"CreateFiber: Using stacksize %8.8lx\r\n", cbStack));

    // reserve VM for stack
    lpStack = VirtualAlloc (NULL, cbStack, MEM_RESERVE|MEM_AUTO_COMMIT,PAGE_NOACCESS);
    if (!lpStack) {
        LocalFree (pFiber);
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        DEBUGMSG(1,(L"CreateFiber exit -- out of memory\r\n"));
        return NULL;
    }
    DEBUGMSG(1,(L"CreateFiber lpStack = %8.8lx\r\n",lpStack));

    // save 16 DWORDS for arguments (need 4 only, but reserving more if we need anything in the future)
    lpArgs = (LPDWORD) ((ulong) lpStack + cbStack - SIZE_RESERVED);
    // fiber context directly above arguments
    pFiberCtx = (PFIBERCTX) ((ulong) lpArgs - dwSizeCtx);

    // update pFiber
    pFiber->dwStkBase  = (DWORD) lpStack;
    pFiber->dwStkPtr   = (DWORD) pFiberCtx;
    pFiber->dwStkBound = pFiber->dwStkPtr & ~(dwPageSize-1);
    pFiber->dwStkSize  = cbStack;
    pFiber->lpParam    = lpParameter;
    pFiber->dwThrdId   = 0;
    
    // commit the memory we need on the stack
    if (!VirtualAlloc ((LPVOID)pFiberCtx, SIZE_RESERVED+dwSizeCtx, MEM_COMMIT,PAGE_READWRITE)) {
        LocalFree (pFiber);
        VirtualFree(lpStack,0,MEM_RELEASE);
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    // NOTE: for cpus that pass argument with register (e.g. mips), just setup the argument registers and
    // have SwitchToFiber will "return" to the FiberBaseFunc and it'll pickup the arguments from the
    // argument registers. For cpus with stack-based argument passing (e.g. x86), we not only need to make
    // FiberBaseFunc the "return address", but also make the stack looks like that FiberBaseFunc is
    // called from other function after we 'return' to FiberBaseFunc. 
    // Therefore we need one extra slot to hold the "return address" of the faked function calling 
    // FiberBaseFunc. Since FiberBaseFunc never returns (it calls ExitThread), the value of the slot 
    // is irrevalent. Here's what the stack should looks like
    //      |               |
    //      +---------------+
    //      | FiberBaseFunc |   <---- pFiberCtx (rgLr == FiberBaseFunc, rgArg1 == arg1, rgArg2 == arg2)
    //      +---------------+
    //      |     arg1      |
    //      +---------------+
    //      |     arg2      |
    //      +---------------+
    //      |               |
    //      |  other regs   |
    //      |               |
    //      +---------------+
    //      | FiberBaseFunc |   <----- lpArgs
    //      +---------------+
    //      |  don't care   |
    //      +---------------+
    //      |     arg1      |
    //      +---------------+
    //      |     arg2      |
    //      +---------------+

    pFiberCtx->rgLr   = lpArgs[0] = (DWORD) FiberBaseFunc;
    pFiberCtx->rgArg1 = lpArgs[2] = (DWORD) lpStartAddress;
    pFiberCtx->rgArg2 = lpArgs[3] = (DWORD) lpParameter;

    DEBUGMSG(1,(L"CreateFiber exit: %8.8lx\r\n", pFiber));
    return pFiber;
}

LPVOID WINAPI ConvertThreadToFiber (LPVOID lpParameter)
{
    PFIBERSTRUCT pFiber;
    DWORD cbStack = ((DWORD) UTlsPtr () - UStkBase () + 0xffff) & 0xffff0000;   // round up to 64k
    // we only allow threads with default process stack size to be converting to fiber.
    // The restriction is to make it possible to free fiber stacks on thread exit, without
    // kernel knowing the details of fiber
    if (UCurFiber () || (cbStack != UStkSize ())) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
    // allocate a fiber structure and initialize it
    if (NULL == (pFiber = (PFIBERSTRUCT) LocalAlloc(LMEM_FIXED, sizeof(FIBERSTRUCT)))) {
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    pFiber->lpParam = lpParameter;
    // use existing stack
    pFiber->dwStkBase  = UStkBase () | 1;    // main fiber has LSB of stack base set
    pFiber->dwStkBound = UStkBound ();  
    pFiber->dwStkSize  = UStkSize ();
    // it's now the current fiber
    pFiber->dwThrdId = GetCurrentThreadId ();
    UCurFiber () = (DWORD) pFiber;

    return pFiber;
}

VOID WINAPI DeleteFiber (PFIBERSTRUCT pFiber)
{
    DWORD dwPrevThrdId;
    DWORD dwStkBase;
    DWORD dwStkSize;
    DWORD n = 0;

    // validate arguments
    if (!pFiber) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return;
    }

    // NOTE: the reference to pFiber fields might generate an exception, in case
    //       the fiber is being deleted by other threads.
    do {
        // try to replace the dwThrdId field with a special id so subsequent calls 
        // to SwitchToFiber and DeleteFiber to the same fiber fail.
        dwPrevThrdId = InterlockedTestExchange ((PLONG) &pFiber->dwThrdId, 0, FIBER_DELETING);

        switch (dwPrevThrdId) {
        case 0:
            // deleting a free fiber
            break;
        
        case FIBER_DELETING:
            // another thread is deleting the fiber, let him do it
            SetLastError (ERROR_INVALID_PARAMETER);
            return;
        
        case FIBER_SWITCHING:
            // yield to let the fiber switch to complete
            Sleep (n);
            n = 1;
            break;
        
        default:
            if (InterlockedTestExchange ((PLONG) &pFiber->dwThrdId, dwPrevThrdId, FIBER_DELETING) != (LONG)dwPrevThrdId) {
                dwPrevThrdId = FIBER_SWITCHING;
            }
        }
        
    } while (FIBER_SWITCHING == dwPrevThrdId);

    // save the stack base/size before deleting the Fiber structure
    dwStkBase = pFiber->dwStkBase;
    dwStkSize = pFiber->dwStkSize;
    LocalFree (pFiber);

    if (GetCurrentThreadId () == dwPrevThrdId) {
        // we are the current fiber, exit the current thread
        ExitThread (0);
    } else if (dwPrevThrdId) {
        // deleting a fiber which is the current fiber of another thread, or
        // the fiber has already exited
        if (!(dwPrevThrdId & 1)) {
            xxx_TerminateThread ((HANDLE) dwPrevThrdId, 0);
        }
    } else {
        // only free the stack if it's not the main fiber
        if (!(dwStkBase & 1)) {
            VirtualFree ((LPVOID) dwStkBase, dwStkSize, MEM_DECOMMIT);
            VirtualFree ((LPVOID) dwStkBase, 0, MEM_RELEASE);
        }
    }
}

LPVOID WINAPI GetCurrentFiber (VOID)
{
    return (PFIBERSTRUCT) UCurFiber ();
}

LPVOID WINAPI GetFiberData (VOID)
{
    PFIBERSTRUCT pFiber = GetCurrentFiber ();
    return pFiber? pFiber->lpParam : NULL;
}

VOID WINAPI SwitchToFiber (PFIBERSTRUCT pFiber)
{
    PFIBERSTRUCT    pSelf   = GetCurrentFiber ();
    DWORD           dwCurId = GetCurrentThreadId ();
    LPDWORD         pTlsPtr = UTlsPtr ();

    if (!pSelf || !pFiber) {
        // only a fiber can switch to another fiber
        SetLastError (ERROR_INVALID_PARAMETER);
        return;
    }

    // indicate that we're in the middle of fiber switching
    if (InterlockedTestExchange ((PLONG)&pSelf->dwThrdId, dwCurId, FIBER_SWITCHING) != (LONG) dwCurId) {
        // we are being deleted!!
        // just return
        return;
    } 

    if (InterlockedTestExchange ((PLONG)&pFiber->dwThrdId, 0, FIBER_SWITCHING)) {
        // The fiber we intend to switch to is already run by another thread,
        // or is being deleted. Either we raise an exception or just return. 
        // We'll just return for now.
        pSelf->dwThrdId = dwCurId;  // restore the current fiber threadid
        return;
    }

    // preform cpu-dependent Fiber switch
    MDFiberSwitch (pFiber, dwCurId, pTlsPtr, pTlsPtr[TLSSLOT_KERNEL] & TLSKERN_FPU_USED);
}

