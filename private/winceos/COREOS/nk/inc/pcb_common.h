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

//
// CPU independent part of processor control block
//
// NOTE: DO NOT INCLUDE THIS FILE DIRECTLY. Inlucde nkxxx.h instead.
//

    //
    // PCB field - offset from 0 - MAX_PCB_OFST (0x084)
    //
    LPDWORD     lpvTls;                 // 0x000 Current thread local storage pointer (OFFSET MUST BE AT 0 for BC reason)
    char        bResched;               // 0x004 reschedule flag
    char        cNest;                  // 0x005 kernel exception nesting
    WORD        fIdle;                  // 0x006 is the CPU idle?
    DWORD       CurThId;                // 0x008 current thread id (OFFSET MUST BE 0x08 for BC reason)
    DWORD       ActvProcId;             // 0x00c active process id (OFFSET MUST BE 0x0c for BC reason)
    DWORD       sec_reserved;           // 0x010 reserved
    DWORD       dwHardwareCPUId;        // 0x014 Hardware CPU ID
    DWORD       dwKCRes;                // 0x018 resched needed?
    PPROCESS    pVMPrc;                 // 0x01c current VM in effect
    PPROCESS    pCurPrc;                // 0x020 ptr to current PROCESS struct
    PTHREAD     pCurThd;                // 0x024 ptr to current THREAD struct
    DWORD       ownspinlock;            // 0x028 own a spinlock?
    PTHREAD     pCurFPUOwner;           // 0x02c current FPU owner
    DWORD       aPend1;                 // 0x030 low (int 0-31) dword of interrupts pending (must be 8-byte aligned)
    DWORD       aPend2;                 // 0x034 high (int 32-63) dword of interrupts pending
    LPBYTE      pKStack;                // 0x038 per cpu kstack
    PTHREAD     pthSched;               // 0x03c running thread (== RunList.pth)
    DWORD       dwCpuId;                // 0x040 current CPU id
    DWORD       dwCpuState;             // 0x044 CPU State (running, powering off, powered off, powering on)
    LPVOID      pSavedContext;          // 0x048 Context saved by CPU when processing an IPI
    DWORD       dwTlbMissCnt;           // 0x04c # of TLB misses
    DWORD       dwSyscallReturnTrap;    // 0x050 randomized trap address for syscall return
    DWORD       dwPslTrapSeed;          // 0x054 ASLR support for PSL trapping address
    struct _PCB *pSelf;                 // 0x058 ptr to self
    WORD        wIPIPending;            // 0x05c if we need to send IPI on spinlock release
    WORD        wIPIOperation;          // 0x05e IPI operation needs to be performed
    DWORD       dwPrevReschedTime;      // 0x060 previous reschedule time
    DWORD       dwPrevTimeModeTime;     // 0x064 last time thread time is calculated
    LARGE_INTEGER liIdleTime;           // 0x068 per-CPU idle time (must be 8-byte aligned)
    DWORD       OwnerProcId;            // 0x070 current thread's owner process ID
    DWORD       unused_pcb[4];          // 0x074 reserved room of PCB growth


    // CPU independent KDATA fields - offset from MAX_PCB_OFST (0x084) to 0x2a0
    WORD        wPad_kData;             // 0x084 padding
    char        bPowerOff_kData;        // 0x086 TRUE during "power off" processing (OFFSET MUST BE AT 0x086 for BC reason)
    char        bProfileOn_kData;       // 0x087 non-zero if profiler is on.
    DWORD       nCpus_kData;            // 0x088 total # of CPUs
    DWORD       dwKVMStart_kData;       // 0x08c kernel VM start
    DWORD       dwKDllFirst_kData;      // 0x090 Kernel DLL Start
    DWORD       dwKDllEnd_kData;        // 0x094 kernel DLL End
    ulong       pAPIReturn_kData;       // 0x098 direct API return address for kernel mode
    long        nMemForPT_kData;        // 0x09c Memory used for PageTables
    DWORD       dwInDebugger_kData;     // 0x0a0 !0 when in debugger
    DWORD       dwTOCAddr_kData;        // 0x0a4 address of pTOC
    DWORD       dwStartUpAddr_kData;    // 0x0a8 startup address (for soft reset handler)
    LPVOID      pOem_kData;             // 0x0ac ptr to OEM globals
    LPVOID      pNk_kData;              // 0x0b0 ptr to NK globals
    DWORD       dwCelogStatus_kData;    // 0x0b4 celog status
    LONGLONG    i64RawOfst_kData;       // 0x0b8 Raw time offset (to calculate mono-increasing time)
    DWORD       dwOfstSysLocale_kData;  // 0x0c0 offset to system locale in NLS file
    DWORD       dwOfstUsrLocale_kData;  // 0x0c4 offset to user locale in NLS file
    struct _OSAXS_KERN_POINTERS_2 *pOsAxsDataBlock_kData; // 0x0c8 Pointer to OsAxs data block
    DWORD       dwIdKitlIST_kData;      // 0x0cc thread id of KITL IST
    DWORD       dwIdKitlTimer_kData;    // 0x0d0 thread id of KITL Timer thread
    struct _OSAXS_HWTRAP *pOsAxsHwTrap_kData;   // 0x0d4 point to the OsAxsHwTrap/Image info struct.

    // NOTE: affinityQ IS a PCB FIELD IN KDATA Area, due to BC reason, bPowerOff needs to be at fixed offset
    //       and the structure is too big to fit in PCB area.
    //       It's not expected that anyone will be referencing affinity Queue from outside of kernel. 
    //       i.e. __GetUserKData never called with the offset to affinity queue.
    TWO_D_QUEUE affinityQ;              // 0x0d8 per CPU affinity queue 
    LONG        PageFreeCount_kData;    // 0x11c # of free pages in system
    LONG        MinPageFree_kData;      // 0x120 minimum number of free pages in system

    LPVOID      unused_kData[31];       // 0x124 reserved room for KData growth
    PEVENT      alpeIntrEvents_unused[SYSINTR_MAX_DEVICES]; // 0x1a0 - 0x2a0

