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
// Layout of MIPS KPage
#define KPAGE_BASE  0xffffc000

    .struct KPAGE_BASE
// PCB fields
KDBase:                 .space  0x17C0      // 0x0000: ~6K kernel stack
KStack:                 .space  8*8         // 0x17c0: arg space + temps <spaced for 64-bit MIPS>
KData:
lpvTls:                 .space  4           // 0x1800: current TLS pointer
ReschedFlag:            .space  1           // 0x1804: != 0 if NextThread must be called
KNest:                  .space  1           // 0x1805: == 1 iff not in kernel
fIdle:                  .space  2           // 0x1806: is the CPU idle?
dwCurThId:              .space  4           // 0x1808: Current thread id
dwActvProcId:           .space  4           // 0x180c: actiev process id
sec_reserved:           .space  4           // 0x1810: reserved
dwHwCpuId:              .space  4           // 0x1814: hardware CPU id
dwKCRes:                .space  4           // 0x1818: reschedule needed?
pProcVM:                .space  4           // 0x181c: Current VM in effect
CurPrcPtr:              .space  4           // 0x1820: ptr to current PROCESS structure
CurThdPtr:              .space  4           // 0x1824: ptr to current THREAD structure
ownspinlock:            .space  4           // 0x1828: own a spinlock? 
PCBCurFPUOwner:         .space  4           // 0x182c: current FPU owner
PendEvents1:            .space  4           // 0x1830: low (int 0-31) dword of interrupts pending (must be 8-byte aligned) 
PendEvents2:            .space  4           // 0x1834: high (int 0-31) dword of interrupts pending
pkstk:                  .space  4           // 0x1838: per CPU kstack
pthSched:               .space  4           // 0x183c: running thread 
dwCpuId:                .space  4           // 0x1840: current CPU id
CpuState:               .space  4           // 0x1844: CPU state
psavedctx:              .space  4           // 0x1848: ptr to saved context
TlbMissCnt:             .space  4           // 0x184c: # of TLB misses (used by profiler only)
dwSyscallReturnTrap:    .space  4           // 0x1850: randomized return address for PSL return
dwPslTrapSeed:          .space  4           // 0x1854: random seed for PSL Trap address
pself:                  .space  4           // 0x1858: ptr to slef
wIpiPending:            .space  2           // 0x185c: ipi pending
wIpiOperation:          .space  2           // 0x185e: IPI operation
prevreschedtime:        .space  4           // 0x1860: previous reschedule time
prevtmtime:             .space  4           // 0x1864: previous time time-mode changed.
liIdleTime:             .space  8           // 0x1868: per CPU idle time
OwnerProcId:            .space  4           // 0x1870: current thread owner process id
                        .space  4*4         // 0x1874: reserved for pcb growth
//
// KData fields - Only certain fields can be used in assembly.
//  SaveT0, SaveK0, SaveK1 - they are actually PCB data.
//  PtrAPIRet_KData, PfnShif_KData, PfnIncr_KData - reference only at bootup.
//  ArchFlags, CoProcBits, pProcNK - replicated into every PCB for every CPU.
//
                        .space  2           // padding
BPowerOff_DoNotUse:     .space  1           // 0x1886: power off flag
ProfOn_DoNotUse:        .space  1           // 0x1887: Profiler enabled?
nCpus_KData:            .space  4           // 0x1888: total # of CPUs
dwKVMStart_DoNotUse:    .space  4           // 0x188c: kerenl VM start
dwKDllFirst_DoNotUse:   .space  4           // 0x1890: Kerenl DLL Start
dwKDllEnd_DoNotUse:     .space  4           // 0x1894: kernel DLL End
PtrAPIRet_KData:        .space  4           // 0x1898: api call return for kernel mode
MForPT_DoNotUse:        .space  4           // 0x189c: Memory used for PageTables
InDbgr_DoNotUse:        .space  4           // 0x18a0: if we're in debugger
TOCAddr_KData:          .space  4           // 0x18a4: pTOC
StartUpAddr_DoNotUse:   .space  4           // 0x18a8: Startup address
PtrOEMGlob_DoNotUse:    .space  4           // 0x18ac: ptr to OEM globals
PtrNKGlob_DoNotUse:     .space  4           // 0x18b0: ptr to NK globals
CeLogStatus_KData:      .space  4           // 0x18b4: celog status
i64rawofst_DoNotUse:    .space  8           // 0x18b8: Raw time offset (to calculate mono-increasing time
ofstsyslocale_DoNotUse: .space  4           // 0x18c0: system locale
ofstusrlocale_DoNotUse: .space  4           // 0x18c4: user locale
osaxsdata_DoNotUse:     .space  4           // 0x18c8: Pointer to OsAxs data block
kitlist_DoNotUse:       .space  4           // 0x18cc: KITL IST ID
kitltimer_DoNotUse:     .space  4           // 0x18d0: KITL Timer theread ID
osaxstrap_DoNotUse:     .space  4           // 0x18d4: point to the OsAxsHwTrap/Image info struct

// NOTE: affinityQ IS a PCB FIELD IN KDATA Area, due to BC reason, bPowerOff needs to be at fixed offset
//       and the structure is too big to fit in PCB area.
//       It's not expected that anyone will be referencing affinity Queue from outside of kernel. 
//       i.e. __GetUserKData never called with the offset to affinity queue.
affinityQ_DoNotUse:     .space  4*17        // 0x18d8: per cpu affinity queue
pagefreecnt_DoNoUse:    .space  4           // 0x191c: page free count
minpagefree_DoNoUse:    .space  4           // 0x1920: min # of pages free
                        .space  4*31        // 0x1924: unused
IntrEvents_DoNotUse:    .space  SYSINTR_MAX_DEVICES*4 // 0x19a0: interrupt event pointers

SaveT0:                 .space  8           // 0x1aa0: <spaced for 64-bit MIPS> (PCB field)
SaveK0:                 .space  8           // 0x1aa8: <spaced for 64-bit MIPS> (PCB field)
SaveK1:                 .space  8           // 0x1ab0: <spaced for 64-bit MIPS> (PCB field)
BasePSR_KData:          .space  4           // 0x1ab8: base value for PSR (PCB field)
FalseInt_KData:         .space  4           // 0x1abc: false interrupt entry
ISRTable_KData:         .space  4*6         // 0x1ac0: first level intr service routines
PfnShift_KData:         .space  4           // 0x1ad8: PFN_SHIFT (reference during bootup only)
PfnIncr_KData:          .space  4           // 0x1adc: PFN_INCR (reference during bootup only)
ArchFlags:              .space  4           // 0x1ae0: architecture flags (replicated in each PCB)
CoProcBits:             .space  4           // 0x1ae4: co-proc enable bits (replicated in each PCB)
_Padding:               .space  4*6         // 0x1ae8: padding
                                    
pProcNK:                                    // 0x1B00: g_pprcNK (1st entry in KInfoTable)
KInfoTable_DoNotUse:    .space  4*32        // 0x1B00: misc. kernel info

KData_size:;                                // 0x1B80: end of kernel data page

#define OFST_TOCADDR        TOCAddr_KData-KDBase
#define OFST_FALSEINT       FalseInt_KData-KDBase
#define OFST_ISRTABLE       ISRTable_KData-KDBase
#define OFST_BASEPSR        BasePSR_KData-KDBase
#define OFST_CELOGSTATUS    CeLogStatus_KData-KDBase
#define OFST_NCPUS          nCpus_KData-KDBase

#define KPAGE_USER      0x00004000          // KPage even page address in user space
#define PPCB_PAGE_USER  0x0000c000          // PCB even page address in user space

#define ISR_TABLE       KPAGE_USER+OFST_ISRTABLE    // read-only
#define BASEPSR         KPAGE_USER+OFST_BASEPSR     // read-only
#define CELOG_STATUS    KPAGE_USER+OFST_CELOGSTATUS // read-only
#define NUM_CPUS        KPAGE_USER+OFST_NCPUS       // read-only

#define KPAGE_LENGTH    0x00002000


