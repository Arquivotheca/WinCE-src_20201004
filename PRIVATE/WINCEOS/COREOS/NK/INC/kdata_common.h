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



//
// CPU independent part of KData
//
// NOTE: DO NOT INCLUDE THIS FILE DIRECTLY. Inlucde nkxxx.h instead.
//

    LPDWORD     lpvTls;         /* 0x000 Current thread local storage pointer */
    HANDLE      ahSys[NUM_SYS_HANDLES]; /* 0x004 If this moves, change kapi.h */
    char        bResched;       /* 0x084 reschedule flag */
    char        cNest;          /* 0x085 kernel exception nesting */
    char        bPowerOff;      /* 0x086 TRUE during "power off" processing */
    char        bProfileOn;     /* 0x087 TRUE if profiling enabled */
    DWORD       dwKCRes;        /* 0x088 resched needed? */
    PPROCESS    pVMPrc;         /* 0x08c current VM in effect */
    PPROCESS    pCurPrc;        /* 0x090 ptr to current PROCESS struct */
    PTHREAD     pCurThd;        /* 0x094 ptr to current THREAD struct */
    ulong       pAPIReturn;     /* 0x098 direct API return address for kernel mode */
    long        nMemForPT;      /* 0x09c Memory used for PageTables */
    DWORD       dwInDebugger;   /* 0x0a0 !0 when in debugger */
    DWORD       dwTOCAddr;      /* 0x0a4 address of pTOC */
    DWORD       dwStartUpAddr;  /* 0x0a8 startup address (for soft reset handler) */
    LPVOID      pOem;           /* 0x0ac ptr to OEM globals */
    LPVOID      pNk;            /* 0x0b0 ptr to NK globals */
    PTHREAD     pCurFPUOwner;   /* 0x0b4 current FPU owner */
    DWORD       aPend1;         /* 0x0b8 - low (int 0-31) dword of interrupts pending (must be 8-byte aligned) */
    DWORD       aPend2;         /* 0x0bc - high (int 32-63) dword of interrupts pending */
    DWORD       dwOfstSysLocale;/* 0x0c0 - offset to system locale in NLS file */
    DWORD       dwOfstUsrLocale;/* 0x0c4 - offset to user locale in NLS file */
    struct _OSAXS_KERN_POINTERS_2 *pOsAxsDataBlock; /* 0x0c8 Pointer to OsAxs data block */
    DWORD       dwIdKitlIST;    /* 0x0cc - thread id of KITL IST */
    DWORD       dwIdKitlTimer;  /* 0x0d0 - thread id of KITL Timer thread */
    struct _OSAXS_HWTRAP *pOsAxsHwTrap; /* 0x0d4 point to the OsAxsHwTrap/Image info struct. */
    LPVOID      unused[46];     /* 0x0d8 unused */
    LONGLONG    i64RawOfst;     /* 0x190 Raw time offset (to calculate mono-increasing time */
    DWORD       dwTlbMissCnt;   /* 0x198 # of TLB misses */
    DWORD       dwCelogStatus;  /* 0x19c celog status */
    PEVENT      alpeIntrEvents[SYSINTR_MAX_DEVICES];/* 0x1a0 */

