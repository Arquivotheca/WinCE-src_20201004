//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Layout of last page of storage

    .struct KPAGE_BASE
KData:          .space  0x17C0      // 0x0000: ~6K kernel stack
KStack:         .space  8*8         // 0x17c0: arg space + temps <spaced for 64-bit MIPS>

lpvTls:         .space  4           // 0x1800: current TLS pointer
SystemHandles:  .space  4*32        // 0x1804:
ReschedFlag:    .space  1           // 0x1884: != 0 if NextThread must be called
KNest:          .space  1           // 0x1885: == 1 iff not in kernel
                .space  1           // 0x1886: alignment padding
BPowerOff:      .space  1           // 0x1887: power off flag
SaveT0:         .space  8           // 0x1888: <spaced for 64-bit MIPS>
BasePSR:        .space  4           // 0x1890: base value for PSR
                .space  4           // 0x1894: (CurMSec) # of milliseconds since boot
                .space  4           // 0x1898: <used to be DiffMSec>
CurAKey:        .space  4           // 0x189c: current thread's access key
DbgEntry:       .space  4           // 0x18a0: kernel debugger entry point
FalseInt:       .space  4           // 0x18a4: false interrupt entry
ISRTable:       .space  4*6         // 0x18a8: first level intr service routines
                                    
SectionTable:   .space  4*64        // 0x18c0: virtual memory section array
IntrEvents:     .space  SYSINTR_MAX_DEVICES*4 // 0x19c0: interrupt event pointers
IntrData:       .space  SYSINTR_MAX_DEVICES*4 // 0x1A40: interrupt handler data pointers
CurThdPtr:      .space  4           // 0x1Ac0: ptr to current THREAD structure
CurPrcPtr:      .space  4           // 0x1Ac4: ptr to current PROCESS structure
HandleBase:     .space  4           // 0x1Ac8: base address of HDATA array.
PtrAPIRet:      .space  4           // 0x1acc: api call return for kernel mode
dwKCRes:        .space  4           // 0x1ad0:
InDbgr:         .space  4           // 0x1ad4: if we're in debugger
PfnShift:       .space  4           // 0x1ad8: PFN_SHIFT
MIPS16Sup:      .space  4           // 0x1adc: if MIPS16 instruction is supported
PfnIncr:        .space  4           // 0x1ae0: PFN_INCR == (PAGE_SIZE >> PFN_SHIFT)
bPadding:       .space  20          // 0x1ae4:
g_CurFPUOwner:  .space  4           // 0x1af8:
MForPT:         .space  4           // 0x1afc: always 0 on MIPS
                                    
KInfoTable:     .space  4*16        // 0x1B00: misc. kernel info
PendEvents:     .space  4           // 0x1B40: bitmask of pending events
                .space  4*9         // 0x1B44: more misc. kernel info
CeLogStatus:    .space  4           // 0x1B68: bitmask of pending events
                .space  4*17        // 0x1B6C: more misc. kernel info
SaveK0:         .space  8           // 0x1BB0: <spaced for 64-bit MIPS>
SaveK1:         .space  8           // 0x1BB8: <spaced for 64-bit MIPS>

TlbShift:       .space  4           // 0x1BC0: shift amount in TLB handler ((VA_PAGE+1) - PFN_SHIFT)
IsR41XX:        .space  4           // 0x1BC4: if this is R41XX CPU
KData_size:;                        // 0x1BC8: end of kernel data page
                                    

#define KPAGE_USER      0x00004000  // KPage address in user space

#define KPAGE_LENGTH    0x00002000

hWin32 = SystemHandles
hCurThread = SystemHandles+4
hCurProc = SystemHandles+8

