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
// Layout of MIPS KPage
#define KPAGE_BASE	0xffffc000

    .struct KPAGE_BASE
KDBase:         .space  0x17C0      // 0x0000: ~6K kernel stack
KStack:         .space  8*8         // 0x17c0: arg space + temps <spaced for 64-bit MIPS>
KData:
lpvTls:         .space  4           // 0x1800: current TLS pointer
SystemHandles:  .space  4*32        // 0x1804:
ReschedFlag:    .space  1           // 0x1884: != 0 if NextThread must be called
KNest:          .space  1           // 0x1885: == 1 iff not in kernel
BPowerOff:      .space  1           // 0x1886: power off flag
ProfOn:         .space  1           // 0x1887: Profiler enabled?
dwKCRes:        .space  4           // 0x1888: reschedule needed?
pProcVM:        .space  4           // 0x188c: Current VM in effect
CurPrcPtr:      .space  4           // 0x1890: ptr to current PROCESS structure
CurThdPtr:      .space  4           // 0x1894: ptr to current THREAD structure
PtrAPIRet:      .space  4           // 0x1898: api call return for kernel mode
MForPT:         .space  4           // 0x189c: memory used for page tables
InDbgr:         .space  4           // 0x18a0: if we're in debugger
TOCAddr:        .space  4           // 0x18a4: pTOC
StartUpAddr:    .space  4           // 0x18a8: Startup address
PtrOEMGlob:     .space  4           // 0x18ac: ptr to OEM globals
PtrNKGlob:      .space  4           // 0x18b0: ptr to NK globals
g_CurFPUOwner:  .space  4           // 0x18b4: current FPU owner
PendEvents1:    .space  4           // 0x18b8: low (int 0-31) dword of interrupts pending (must be 8-byte aligned) 
PendEvents2:    .space  4           // 0x18bc: high (int 0-31) dword of interrupts pending
                .space  4*54        // 0x18c0: unused
TlbMissCnt:     .space  4           // 0x1998: # of TLB misses (used by profiler only)
CeLogStatus:    .space  4           // 0x199c: celog status
IntrEvents:     .space  SYSINTR_MAX_DEVICES*4 // 0x19a0: interrupt event pointers

SaveT0:         .space  8           // 0x1aa0: <spaced for 64-bit MIPS>
SaveK0:         .space  8           // 0x1aa8: <spaced for 64-bit MIPS>
SaveK1:         .space  8           // 0x1ab0: <spaced for 64-bit MIPS>
BasePSR:        .space  4           // 0x1ab8: base value for PSR
FalseInt:       .space  4           // 0x1abc: false interrupt entry
ISRTable:       .space  4*6         // 0x1ac0: first level intr service routines
PfnShift:       .space  4           // 0x1ad8: PFN_SHIFT
PfnIncr:        .space  4           // 0x1adc: PFN_INCR == (PAGE_SIZE >> PFN_SHIFT)
ArchFlags:      .space  4           // 0x1ae0: architecture flags
CoProcBits:     .space  4           // 0x1ae4: co-proc enable bits
_Padding:       .space  4*6         // 0x1ae8: padding
                                    
pProcNK:                            // 0x1B00: g_pprcNK (1st entry in KInfoTable)
KInfoTable:     .space  4*32        // 0x1B00: misc. kernel info

KData_size:;                        // 0x1B80: end of kernel data page
                                    

#define KPAGE_USER      0x00004000  // KPage address in user space

#define KPAGE_LENGTH    0x00002000

dwCurThId = SystemHandles+4
dwActvProcId = SystemHandles+8

