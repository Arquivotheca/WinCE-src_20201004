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

// list of functions exported to the kernel.
    HDSTUB_SHARED_GLOBALS *pHdSharedGlobals;
    
    BOOL (*pfnKdSanitize)(BYTE*, VOID*, ULONG, BOOL);
    VOID (*pfnKdReboot)(BOOL);
    VOID (*pfnKdPostInit)();
    BOOL (*pfnKdIgnoreIllegalInstruction)(DWORD);

    BOOL (*pfnException)(PEXCEPTION_RECORD, CONTEXT *, BOOLEAN);
    void (*pfnVmPageIn)(DWORD, BOOL);
    void (*pfnVmPageOut)(DWORD, DWORD);
    void (*pfnModLoad)(DWORD, BOOL);
    void (*pfnModUnload)(DWORD, BOOL);
    BOOL (*pfnIsDataBreakpoint)(DWORD, BOOL);
    void (*pfnStopAllCpuCB)(void);
    BOOL (*pfnConnectClient)(KDBG_DLLINITFN, void*);
    void (*pfnUpdateKernelPointers)(struct _OSAXS_KERN_POINTERS_2 *);
