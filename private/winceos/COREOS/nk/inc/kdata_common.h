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
// CPU independent part of KData
//
// NOTE: DO NOT INCLUDE THIS FILE DIRECTLY. Inlucde nkxxx.h instead.
//

    //
    // PCB field - offset from 0 - MAX_PCB_OFST (0x084)
    //
    LPDWORD     lpvTls_pcb;             // 0x000 Current thread local storage pointer (OFFSET MUST BE AT 0 for BC reason)
    char        bResched_pcb;           // 0x004 reschedule flag
    char        cNest_pcb;              // 0x005 kernel exception nesting
    WORD        fIdle_pcb;              // 0x006 is the CPU idle?
    DWORD       CurThId_pcb;            // 0x008 current thread id (OFFSET MUST BE 0x08 for BC reason)
    DWORD       ActvProcId_pcb;         // 0x00c active process id (OFFSET MUST BE 0x0c for BC reason)
    HANDLE      CurTok_pcb;             // 0x010 current token     (OFFSET MUST BE 0x10 for BC reason)
    DWORD       dwHardwareCPUId;        // 0x014 Hardware CPU ID
    DWORD       dwKCRes_pcb;            // 0x018 resched needed?
    PPROCESS    pVMPrc_pcb;             // 0x01c current VM in effect
    PPROCESS    pCurPrc_pcb;            // 0x020 ptr to current PROCESS struct
    PTHREAD     pCurThd_pcb;            // 0x024 ptr to current THREAD struct
    DWORD       ownspinlock_pcb;        // 0x028 own a spinlock?
    PTHREAD     pCurFPUOwner_pcb;       // 0x02c current FPU owner
    DWORD       aPend1_pcb;             // 0x030 low (int 0-31) dword of interrupts pending (must be 8-byte aligned)
    DWORD       aPend2_pcb;             // 0x034 high (int 32-63) dword of interrupts pending
    LPBYTE      pKStack_pcb;            // 0x038 per cpu kstack
    PTHREAD     pthSched_pcb;           // 0x03c running thread (== RunList.pth)
    DWORD       dwCpuId_pcb;            // 0x040 current CPU id
    DWORD       dwCpuState_pcb;         // 0x044 CPU State
    LPVOID      pSavedContext_pcb;      // 0x048 Context saved by CPU when processing an IPI
    DWORD       dwTlbMissCnt_pcb;       // 0x04c # of TLB misses
    DWORD       dwSyscallReturnTrap;    // 0x050 randomized trap address for syscall return
    DWORD       dwPslTrapSeed;          // 0x054 ASLR support for PSL trapping address
    struct _PCB *pSelf_pcb;             // 0x058 ptr to self
    WORD        wIPIPending_pcb;        // 0x05c if we need to send IPI on spinlock release
    WORD        wIPIOperation_pcb;      // 0x05e IPI operation needs to be performed
    DWORD       dwPrevReschedTime_pcb;  // 0x060 previous reschedule time
    DWORD       dwPrevTimeModeTime_pcb; // 0x064 last time thread time is calculated
    LARGE_INTEGER liIdleTime_pcb;       // 0x068 per-CPU idle time (must be 8-byte aligned)
    DWORD       OwnerProcId_pcb;        // 0x070 current thread's owner process ID
    DWORD       unused_pcb[4];          // 0x074 reserved room of PCB growth

    
    // CPU independent KDATA fields - offset from MAX_PCB_OFST (0x084) to 0x2a0
    WORD        wPad;                   // 0x084 padding
    char        bPowerOff;              // 0x086 TRUE during "power off" processing (OFFSET MUST BE AT 0x086 for BC reason)
    char        bProfileOn;             // 0x087 non-zero if profiler is on.
    DWORD       nCpus;                  // 0x088 total # of CPUs
    DWORD       dwKVMStart;             // 0x08c kernel VM start
    DWORD       dwKDllFirst;            // 0x090 Kernel DLL Start
    DWORD       dwKDllEnd;              // 0x094 kernel DLL End
    ulong       pAPIReturn;             // 0x098 direct API return address for kernel mode
    long        nMemForPT;              // 0x09c Memory used for PageTables
    DWORD       dwInDebugger;           // 0x0a0 !0 when in debugger
    DWORD       dwTOCAddr;              // 0x0a4 address of pTOC
    DWORD       dwStartUpAddr;          // 0x0a8 startup address (for soft reset handler)
    LPVOID      pOem;                   // 0x0ac ptr to OEM globals
    LPVOID      pNk;                    // 0x0b0 ptr to NK globals
    DWORD       dwCelogStatus;          // 0x0b4 celog status
    LONGLONG    i64RawOfst;             // 0x0b8 Raw time offset (to calculate mono-increasing time)
    DWORD       dwOfstSysLocale;        // 0x0c0 offset to system locale in NLS file
    DWORD       dwOfstUsrLocale;        // 0x0c4 offset to user locale in NLS file
    struct _OSAXS_KERN_POINTERS_2 *pOsAxsDataBlock; // 0x0c8 Pointer to OsAxs data block
    DWORD       dwIdKitlIST;            // 0x0cc thread id of KITL IST
    DWORD       dwIdKitlTimer;          // 0x0d0 thread id of KITL Timer thread
    struct _OSAXS_HWTRAP *pOsAxsHwTrap; // 0x0d4 point to the OsAxsHwTrap/Image info struct.

    // NOTE: affinityQ IS a PCB FIELD IN KDATA Area, due to BC reason, bPowerOff needs to be at fixed offset
    //       and the structure is too big to fit in PCB area.
    //       It's not expected that anyone will be referencing affinity Queue from outside of kernel. 
    //       i.e. __GetUserKData never called with the offset to affinity queue.
    TWO_D_QUEUE affinityQ_pcb;          // 0x0d8 per CPU affinity queue 
    LONG        PageFreeCount;          // 0x11c # of free pages in system
    LONG        MinPageFree;            // 0x120 minimum number of free pages in system

    LPVOID      unused_kData[31];       // 0x124 reserved room for KData growth
    PEVENT      alpeIntrEvents[SYSINTR_MAX_DEVICES]; // 0x1a0 - 0x2a0

//
