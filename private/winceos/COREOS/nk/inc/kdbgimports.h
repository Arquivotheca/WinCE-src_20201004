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

// list of imported variables and functions from the kernel.
    DWORD StartOfKernelImports;
    struct KDataStruct *pKData;
    PPROCESS pprcNK;
    PCB **rgpPCBs;
    DWORD cPCBs;

    DWORD *pdwProcessorFeatures;
    DWORD dwCEInstructionSet;

    PPVOID ppCaptureDumpFileOnDevice;
    PPVOID ppKCaptureDumpFileOnDevice;

    volatile SPINLOCK *pKitlSpinLock;
    volatile SPINLOCK *pSchedLock;
    volatile SPINLOCK *pPhysLock;
    volatile SPINLOCK *pOalLock;

    ULONG *pulHDEventFilter;

    const CINFO **pSystemAPISets;
    const ROMHDR *pRomHdr;
    fslog_t *pLogPtr;
    HANDLE*  phCoreDll;
    HANDLE*  phKCoreDll;
    
    LPBYTE* ppKHeapBase;
    LPBYTE* ppCurKHeapFree;

    PADDRMAP pOEMAddressTable;
    BOOL    *pfIntrusivePaging;

    BOOL (*pfnINTERRUPTS_ENABLE) (BOOL fEnable);

    void (*pfnAcquireSpinLock)(volatile struct _SPINLOCK *);
    void (*pfnReleaseSpinLock)(volatile struct _SPINLOCK *);

    VOID (*pfnDeleteCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnEnterCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnInitializeCriticalSection)(LPCRITICAL_SECTION);
    VOID (*pfnLeaveCriticalSection)(LPCRITICAL_SECTION);

    BOOL (*pfnDoThreadGetContext)(PTHREAD, LPCONTEXT);

    void (*pfnMDCaptureFPUContext)(CONTEXT *);

    BOOL (WINAPI* pfnEventModify)(HANDLE, DWORD);
    BOOL (* pfnEVNTModify) (PEVENT lpe, DWORD type); // Non-blocking.  Safe to use in non-preemptable state.

    PPAGETABLE (*pfnGetPageTable)(PPAGEDIRECTORY, DWORD);
    DWORD (*pfnGetPFNOfProcess)(PROCESS *, void const *);
    BOOL (*pfnVMCreateKernelPageMapping)(LPVOID , DWORD);
    DWORD (*pfnVMRemoveKernelPageMapping)(LPVOID);
    PROCESS* (*pfnGetProcPtr)(PHDATA);
    PHDATA (*pfnh2pHDATA)(HANDLE, PHNDLTABLE);

    void  (*pfnHDCleanup)();
    BOOL  (*pKDCleanup)(VOID);

    void  (*pfnHwTrap)(struct _HDSTUB_EVENT2 *);
    int   (*pfnInSysCall)(void);                        // arm only

    LONG  (*pfnInterlockedDecrement)(LPLONG Target);    // x86 uses an intrinsic interlocked functions
    LONG  (*pfnInterlockedIncrement)(LPLONG Target);    

    BOOL  (*pfnIsDesktopDbgrExist)();
    int (*pfnKCall)(PKFN, ...);
    void  (*pfnKdpInvalidateRange)(PVOID, ULONG);
    BOOL (*pfnKdpIsROM) (PPROCESS, LPVOID, DWORD);
    BOOL (*pfnKITLIoctl) (DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    void (*pfnNKCacheRangeFlush) (LPVOID, DWORD, DWORD);
    BOOL (*pfnNKRegisterDbgZones) (PMODULE, LPDBGPARAM);
    VOID  (*pfnNKvDbgPrintfW)(LPCWSTR, va_list);
    ULONG (*pfnNKGetThreadCallStack) (PTHREAD, ULONG, LPVOID, DWORD, DWORD, PCONTEXT);
    BOOL (*pfnNKIsProcessorFeaturePresent)(DWORD dwProcessorFeature);
    BOOL (*pfnNKKernelLibIoControl) (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    int  (*pfnNKwvsprintfW) (LPWSTR, LPCWSTR, va_list, int);

    HANDLE (*pfnNKLoadKernelLibrary)(LPCWSTR);
    PFN_GetProcAddrA pfnGetProcAddressA;

    struct _REGISTER_DESC * (*pfnOEMGetRegDesc) (DWORD *);
    HRESULT (*pfnOEMReadRegs) (BYTE *, DWORD *, const DWORD);
    HRESULT (*pfnOEMWriteRegs) (BYTE *, const DWORD);

    PFN_OEMKDIoControl pfnOEMKDIoControl;
    
    PVOID (*pfnPfn2Virt)(DWORD);

    void (*pfnResumeAllOtherCPUs)(void);
    void (*pfnStopAllOtherCPUs)(void);

    BOOL (*pfnSCHL_SetThreadBasePrio) (PTHREAD, DWORD);
    BOOL (*pfnSCHL_SetThreadQuantum) (PTHREAD, DWORD);

    PROCESS *(*pfnSwitchActiveProcess) (PROCESS *);
    PROCESS *(*pfnSwitchVM) (PROCESS*);

    LPVOID  (*pfnVMAlloc)(PPROCESS, LPVOID, DWORD, DWORD, DWORD);
    BOOL (*pfnVMFreeAndRelease)(PPROCESS, LPVOID, DWORD);
    DWORD (*pfnMakeWritableEntry)(PPROCESS, DWORD, DWORD);
    DWORD (*pfnOEMGetTickCount) ();
    
#ifdef x86
    EXCEPTION_DISPOSITION (__cdecl *p_except_handler3)(EXCEPTION_RECORD *,
        void *, CONTEXT *, DISPATCHER_CONTEXT *);
#else
    EXCEPTION_DISPOSITION (* p__C_specific_handler)(PEXCEPTION_RECORD, PVOID, PCONTEXT, PDISPATCHER_CONTEXT);
#endif
    BOOL (*p__abnormal_termination)(void);             // x86 only
    DWORD (*pfnNKSnapshotSMPStart)();
    DWORD EndOfKernelImports;
