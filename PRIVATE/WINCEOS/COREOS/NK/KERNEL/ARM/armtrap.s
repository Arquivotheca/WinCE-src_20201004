;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; This source code is licensed under Microsoft Shared Source License
; Version 1.0 for Windows CE.
; For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
;
        TTL ARM Interrupt and Exception Processing
;-------------------------------------------------------------------------------
;++
;
;
; Module Name:
;
;    armtrap.s
;
; Abstract:
;
;    This module implements the code necessary to field and process ARM
;    interrupt and exception conditions.
;
;    WARNING: This module executes in KSEG0 and, in general, cannot
;       tolerate a TLB Miss. Registers k0 and k1 are used during initial
;       interrupt and exception processing, and therefore, extreme care must
;       be exercised when modifying this module.
;
; Environment:
;
;    Kernel mode only.
;
;--
;-------------------------------------------------------------------------------
        OPT     2       ; disable listing
        INCLUDE ksarm.h
        OPT     1       ; reenable listing


PERFORMCALLBACK EQU     -113
RAISEEXCEPTION  EQU     -114
KERN_TRUST_FULL EQU     2

; must match the value in kernel.h
SECURESTK_RESERVE       EQU     48      ; (SIZE_PRETLS + 16)
MAX_PSL_ARGS            EQU     56      ;
CALLEE_SAVED_REGS       EQU     32      ; (r4-r11)


; must match the value defined in pkfuncs.h
CACHE_SYNC_FLUSH_TLB    EQU     0x18

;;; P2 h/w defines
  IF {TRUE}
HKEEP_FPGA_REGS_BASE    EQU     0xA4000000      ; housekeeping FPGA reg base
LED_ALPHA       EQU     HKEEP_FPGA_REGS_BASE+0x00060000
LED_DISCRETE    EQU     HKEEP_FPGA_REGS_BASE+0x00040000
SPIN_DELAY      EQU     0x40000
  ENDIF

BANK_SIZE EQU   0x00100000      ; 1MB per bank in MemoryMap table
BANK_SHIFT EQU  20

        MACRO
        BREAKPOINT
        DCD     0xE6000010      ; special undefined instruction
        MEND

; Resister save frame for Code Aborts, Data Aborts and IRQs.
FrameR0 EQU     0
FrameR1 EQU     4
FrameR2 EQU     8
FrameR3 EQU     12
FrameR12 EQU    16
FrameLR EQU     20

; High memory layout:
;       FFFD0000 - first level page table (uncached)
;       FFFD4000 - not used
;       FFFE0000 - disabled for protection
;       FFFF0000 - exception vectors
;       FFFF03E0 - exception vector jump table
;       FFFF0400 - not used (r/o)
;       FFFF1000 - disabled for protection
;       FFFF2000 - r/o (physical overlaps with vectors)
;       FFFF2400 - Interrupt stack (1k)
;       FFFF2800 - r/o (physical overlaps with Abort stack)
;       FFFF3000 - disabled for protection
;       FFFF4000 - r/o (physical memory overlaps with vectors & intr. stack & FIQ stack)
;       FFFF4900 - Abort stack (2k - 256 bytes)
;       FFFF5000 - disabled for protection
;       FFFF6000 - r/o (physical memory overlaps with vectors & intr. stack)
;       FFFF6800 - FIQ stack (256 bytes)
;       FFFF6900 - r/o (physical memory overlaps with Abort stack)
;       FFFF7000 - disabled
;       FFFFC000 - kernel stack
;       FFFFC800 - KDataStruct
;       FFFFCC00 - r/o for protection (2nd level page table for 0xFFF00000)

        ^ 0xFFFD0000
FirstPT         # 0x4000
                # 0x4000
                # 0x8000
                # 0x10000       ; not mapped
ExVector        # 0x1000
                # 0x1400        ; not mapped
                # 0x0400        ; 1K interrupt stack
IntStack        # 0x2000        ; not mapped                    (ffff2800)
                # 0x0100        ; not mapped (FIQ stack)        (ffff4800)
                # 0x0700        ; 2K-256 abort stack            (ffff4900)
AbortStack      # 0x1800        ; not mapped                    (ffff5000)
                # 0x0100        ; not mapped (FIQ stack)        (ffff6800)
FIQStack        # 0xC000-0x6900 ; not mapped                    (ffff6900)
KDBase          # 0x07E0        ; 2K-32 kernel stack
KStack          # 0x0020        ; temporary register save area
KData           # 0x400         ; kernel data area


        ;-----------------------------------------------------------------------
        ; .KDATA area is used to reserve physical memory for the above structures.
        AREA |.KDATA|,DATA,NOINIT
        
        EXPORT  ExceptionVectors
        
KDataArea
PTs     %       0x4000          ; space for first-level page table
ExceptionVectors 
        %       0x0400          ; space for exception vectors
        %       0x0400          ; space for interrupt stack
        %       0x0100          ; space for FIQ stack
        %       0x0700          ; space for Abort stack
KPage   %       0x0c00          ; space for kernel stack & KDataStruct
HighPT  %       0x0400          ; space for 2nd level page table to map 0xFFF00000
KDEnd   %       0


        MACRO
        mtc15   $cpureg, $cp15reg
        mcr     p15,0,$cpureg,$cp15reg,c0,0
        MEND

        MACRO
        mfc15   $cpureg, $cp15reg
        mrc     p15,0,$cpureg,$cp15reg,c0,0
        MEND



        MACRO
        WRITELED $val
  IF {FALSE}
        stmfd   sp!, {r0-r1}
        ldr     r0, =LED_ALPHA
        ldr     r1, $val
        str     r1, [r0]
        ldmfd   sp!, {r0-r1}
  ENDIF
        MEND

        MACRO
        WRITELED_REG $reg
  IF {FALSE}
        stmfd   sp!, {r0}
        ldr     r0, =LED_ALPHA
        str     $reg, [r0]
        ldmfd   sp!, {r0}
  ENDIF
        MEND



 IF Interworking :LOR: Thumbing

        MACRO
        CALL $Fn
        ldr     r12, =$Fn
        mov     lr, pc
        bx      r12
        MEND
        
        MACRO
        CALLEQ $Fn
        ldreq   r12, =$Fn
        moveq   lr, pc
        bxeq    r12
        MEND
        
        MACRO
        RETURN
        bx      lr
        MEND
        
        MACRO
        RETURN_EQ
        bxeq    lr
        MEND
        
        MACRO
        RETURN_NE
        bxne    lr
        MEND

        MACRO
        RETURN_LT
        bxlt    lr
        MEND
  ELSE

        MACRO
        CALL $Fn
        bl      $Fn
        MEND
        
        MACRO
        CALLEQ $Fn
        bleq    $Fn
        MEND
        
        MACRO
        RETURN
        mov     pc, lr
        MEND
        
        MACRO
        RETURN_EQ
        moveq   pc, lr
        MEND
        
        MACRO
        RETURN_NE
        movne   pc, lr
        MEND

        MACRO
        RETURN_LT
        movlt   pc, lr
        MEND

  ENDIF

        ;-----------------------------------------------------------------------
        ; Globals
        AREA |.data|, DATA

        
        ;-----------------------------------------------------------------------
        ; Code (Read-only TEXT section)
        TEXTAREA
        IMPORT  ObjectCall
        IMPORT  ServerCallReturn
        IMPORT  PerformCallBackExt
        IMPORT  CallbackReturn
        IMPORT  NKSection
        IMPORT  LoadPageTable
        IMPORT  HandleException
        IMPORT  NextThread
        IMPORT  ExceptionDispatch
        IMPORT  OEMInterruptHandler
        IMPORT  OEMInterruptHandlerFIQ
        IMPORT  ARMInit
        IMPORT  KernelInit
        IMPORT  idleconv
        IMPORT  OEMCacheRangeFlush
        IMPORT  OEMARMCacheMode
        IMPORT  VectorTable
        EXPORT  MD_CBRtn
        
  IF CELOG
        IMPORT  CeLogInterrupt
  ENDIF
        IMPORT  CeLogThreadMigrate

  IF {FALSE}
        IMPORT  WhereAmI
        IMPORT  WriteHex
        IMPORT  WriteByte
        IMPORT  PutHex
  ENDIF


;-------------------------------------------------------------------------------
; PaFromVa: Figure out Physical Address of a Virtual Address
;
; (r0) = VA
; (r1) = ptr to OEMAddressTable
;
; On return
; (r0) = PA
;
; Register used:
; r0 - r3, r12
;
;-------------------------------------------------------------------------------
        LEAF_ENTRY PaFromVa
        mov     r12, r0                         ; (r12) = VA
        mov     r0, #-1                         ; (r0) = -1 (initialize return value to an invalid PA)
101
        ldr     r3, [r1]                        ; (r3) = OEMAddressTable[i].vaddr
        
        mov     r2, r3, LSR #BANK_SHIFT         ; make 1MB aligned
        mov     r3, r2, LSL #BANK_SHIFT

        cmp     r3, #0                          ; EndOfTable?
        moveq   pc, lr                          ; INVALID OEMAddressTable, VA not found
        
        cmp     r12, r3                         ; Is (VA >= OEMAddressTable[i].vaddr)?
        blt     %F102                           ; go to next entry if not

        ldr     r2, [r1, #8]                    ; (r2) = size in MB
        add     r2, r3, r2, LSL #BANK_SHIFT     ; (r2) = OEMAddressTable[i].vaddr + region size
        cmp     r12, r2                         ; Is (VA < OEMAddressTable[i].vaddr + region size)?
        blt     %F103                           ; Found if it's true

102
        add     r1, r1, #12                     ; i ++ (move to next entry)
        b       %B101                           ; test next entry

        ; found the entry
        ; (r1) = &OEMAddressTable[i]
        ; (r3) = OEMAddressTable[i].vaddr
103
        ldr     r0, [r1, #4]                    ; (r0) = OEMAddressTable[i].paddr
        mov     r0, r0, LSR #BANK_SHIFT         ; (r0) >>= 20
        add     r0, r12, r0, LSL #BANK_SHIFT    ; (r0) = VA + (r0 << 20)
        sub     r0, r0, r3                      ; (r0) -= OEMAddressTable[i].vaddr
        
        mov     pc, lr                          ; return
        ENTRY_END


;-------------------------------------------------------------------------------
; VaFromPa: Figure out Virtual Address of a Physical Address
;
; (r0) = PA
; (r1) = ptr to OEMAddressTable
;
; On return
; (r0) = VA
;
; Register used:
; r0 - r3, r12
;
;-------------------------------------------------------------------------------
        LEAF_ENTRY VaFromPa
        mov     r12, r0                         ; (r12) = PA
        mov     r0, #0                          ; (r0) = 0 (initialize return value to an invalid VA)
201
        ldr     r3, [r1, #8]                    ; (r3) = OEMAddressTable[i].size
        ldr     r2, [r1, #4]                    ; (r2) = OEMAddressTable[i].paddr
        
        cmp     r3, #0                          ; EndOfTable?
        moveq   pc, lr                          ; PA not found if end of table
        
        cmp     r12, r2                         ; Is (PA >= OEMAddressTable[i].paddr)?
        blt     %F202                           ; go to next entry if not

        add     r2, r2, r3, LSL #BANK_SHIFT     ; (r2) = OEMAddressTable[i].vaddr + region size
        cmp     r12, r2                         ; Is (PA < OEMAddressTable[i].vaddr + region size)?
        blt     %F203                           ; Found if it's true

202
        add     r1, r1, #12                     ; i ++ (move to next entry)
        b       %B201                           ; test next entry

203
        ; found the entry
        ; (r1) = &OEMAddressTable[i]
        ldr     r0, [r1]                        ; (r0) = OEMAddressTable[i].vaddr
        ldr     r3, [r1, #4]                    ; (r3) = OEMAddressTable[i].paddr
        mov     r0, r0, LSR #BANK_SHIFT         ; (r0) >>= 20
        add     r0, r12, r0, LSL #BANK_SHIFT    ; (r0) = PA + (r0 << 20)
        sub     r0, r0, r3                      ; (r0) -= OEMAddressTable[i].paddr
        
        mov     pc, lr                          ; return
        ENTRY_END


;-------------------------------------------------------------------------------
; KernelStart - kernel main entry point
;
;       The OEM layer will setup any platform or CPU specific configuration that is
; required for the kernel to have access to ROM and DRAM and jump here to start up
; the system. Any processor specific cache or MMU initialization should be completed.
; The MMU and caches should not enabled.
;
;       This routine will initialize the first-level page table based up the contents of
; the MemoryMap array and enable the MMU and caches.
;
; NOTE: Until the MMU is enabled, kernel symbolic addresses are not valid and must be
;       translated via the MemoryMap array to find the correct physical address.
;
;       Entry   (r0) = pointer to MemoryMap array in physical memory
;       Exit    returns if MemoryMap is invalid
;-------------------------------------------------------------------------------
        LEAF_ENTRY KernelStart

        mov     r11, r0                         ; (r11) = &MemoryMap (save pointer)

        ; figure out the virtual address of OEMAddressTable
        mov     r1, r11                         ; (r1) = &MemoryMap (2nd argument to VaFromPa)
        bl      VaFromPa
        mov     r6, r0                          ; (r6) = VA of MemoryMap

        ; convert base of PTs to Physical address
        ldr     r4, =PTs                        ; (r4) = virtual address of FirstPT
        mov     r0, r4                          ; (r0) = virtual address of FirstPT
        mov     r1, r11                         ; (r1) = &MemoryMap (2nd argument to PaFromVa)
        bl      PaFromVa

        mov     r10, r0                         ; (r10) = ptr to FirstPT (physical)

;       Zero out page tables & kernel data page

        mov     r0, #0                          ; (r0-r3) = 0's to store
        mov     r1, #0
        mov     r2, #0
        mov     r3, #0
        mov     r4, r10                         ; (r4) = first address to clear
        add     r5, r10, #KDEnd-PTs             ; (r5) = last address + 1
18      stmia   r4!, {r0-r3}
        stmia   r4!, {r0-r3}
        cmp     r4, r5
        blo     %B18


;       Setup 2nd level page table to map the high memory area which contains the
; first level page table, 2nd level page tables, kernel data page, etc.

        add     r4, r10, #HighPT-PTs            ; (r4) = ptr to high page table
        orr     r0, r10, #0x051                 ; (r0) = PTE for 64K, kr/w kr/w r/o r/o page, uncached unbuffered
        str     r0, [r4, #0xD0*4]               ; store the entry into 8 consecutive slots
        str     r0, [r4, #0xD1*4]
        str     r0, [r4, #0xD2*4]
        str     r0, [r4, #0xD3*4]
        add     r8, r10, #ExceptionVectors-PTs  ; (r8) = ptr to vector page
        bl      OEMARMCacheMode                 ; places C and B bit values in r0 as set by OEM
        mov     r2, r0
        orr     r0, r8, #0x002                  ; construct the PTE
        orr     r0, r0, r2
        str     r0, [r4, #0xF0*4]               ; store entry for exception vectors
        orr     r0, r0, #0x500                  ; (r0) = PTE for 4k r/o r/o kr/w kr/w C+B page
        str     r0, [r4, #0xF4*4]               ; store entry for abort stack
        str     r0, [r4, #0xF6*4]               ; store entry for FIQ stack  (access permissions overlap for abort and FIQ stacks, same 1k)
        orr     r0, r8, #0x042
        orr     r0, r0, r2                      ; (r0)= PTE for 4K r/o kr/w r/o r/o (C+B as set by OEM)
        str     r0, [r4, #0xF2*4]               ; store entry for interrupt stack
        add     r9, r10, #KPage-PTs             ; (r9) = ptr to kdata page
        orr     r0, r9, #0x002
        orr     r0, r0, r2                      ; (r0)=PTE for 4K (C+B as set by OEM)
        orr     r0, r0, #0x250                  ; (r0) = set perms kr/w kr/w kr/w+ur/o r/o
        str     r0, [r4, #0xFC*4]               ; store entry for kernel data page
        orr     r0, r4, #0x001                  ; (r0) = 1st level PTE for high memory section
        add     r1, r10, #0x4000
        str     r0, [r1, #-4]                   ; store PTE in last slot of 1st level table
  IF {FALSE}
        mov     r0, r4
        mov     r1, #256                        ; dump 256 words
        CALL    WriteHex
  ENDIF

;       Fill in first level page table entries to create "un-mapped" regions
; from the contents of the MemoryMap array.
;
;       (r9) = ptr to KData page
;       (r10) = ptr to 1st level page table
;       (r11) = ptr to MemoryMap array

        add     r10, r10, #0x2000               ; (r10) = ptr to 1st PTE for "unmapped space"
        mov     r7, #2                          ; (r7) = pass counter

        mov     r0, #0x02
        orr     r0, r0, r2                      ; (r0)=PTE for 0: 1MB (C+B as set by OEM)
        orr     r0, r0, #0x400                  ; set kernel r/w permission
20      mov     r1, r11                         ; (r1) = ptr to MemoryMap array


25      ldr     r2, [r1], #4                    ; (r2) = virtual address to map Bank at
        ldr     r3, [r1], #4                    ; (r3) = physical address to map from
        ldr     r4, [r1], #4                    ; (r4) = num MB to map

        cmp     r4, #0                          ; End of table?
        beq     %F29

        ldr     r5, =0x1FF00000
        and     r2, r2, r5                      ; VA needs 512MB, 1MB aligned.

        ldr     r5, =0xFFF00000
        and     r3, r3, r5                      ; PA needs 4GB, 1MB aligned.

        add     r2, r10, r2, LSR #18
        add     r0, r0, r3                      ; (r0) = PTE for next physical page

28      str     r0, [r2], #4
        add     r0, r0, #0x00100000             ; (r0) = PTE for next physical page

        sub     r4, r4, #1                      ; Decrement number of MB left
        cmp     r4, #0
        bne     %B28                            ; Map next MB

        bic     r0, r0, #0xF0000000             ; Clear Section Base Address Field
        bic     r0, r0, #0x0FF00000             ; Clear Section Base Address Field
        b       %B25                            ; Get next element


29
        bic     r0, r0, #0x0C                   ; clear cachable & bufferable bits in PTE
        add     r10, r10, #0x0800               ; (r10) = ptr to 1st PTE for "unmapped uncached space"
        subs    r7, r7, #1                      ; decrement pass counter
        bne     %B20                            ; go setup PTEs for uncached space if we're not done
        sub     r10, r10, #0x3000               ; (r10) = restore address of 1st level page table
  IF {FALSE}
        mov     r0, r10
        mov     r1, #4096                       ; dump 4096 words
        CALL    WriteHex
  ENDIF

; Setup the vector area.
;
;       (r8) = ptr to exception vectors

        add     r7, pc, #VectorInstructions - (.+8)
        ldmia   r7!, {r0-r3}                    ; load 4 instructions
        stmia   r8!, {r0-r3}                    ; store the 4 vector instructions
        ldmia   r7!, {r0-r3}                    ; load 4 instructions
        stmia   r8!, {r0-r3}                    ; store the 4 vector instructions
  IF {FALSE}
        sub     r0, r8, #8*4
        mov     r1, #8  ; dump 8 words
        CALL    WriteHex
  ENDIF
        ; convert VectorTable to Physical Address
        ldr     r0, =VectorTable                ; (r0) = VA of VectorTable
        mov     r1, r11                         ; (r1) = &OEMAddressTable[0]
        bl      PaFromVa
        mov     r7, r0                          ; (r7) = PA of VectorTable
        add     r8, r8, #0x3E0-(8*4)            ; (r8) = target location of the vector table
        ldmia   r7!, {r0-r3}
        stmia   r8!, {r0-r3}
        ldmia   r7!, {r0-r3}
        stmia   r8!, {r0-r3}
  IF {FALSE}
        sub     r0, r8, #8*4
        mov     r1, #8  ; dump 8 words
        CALL    WriteHex
  ENDIF

; The page tables and exception vectors are setup. Initialize the MMU and turn it on.

        mov     r1, #1
        mtc15   r1, c3                          ; setup access to domain 0
        mtc15   r10, c2

        mov     r0, #0
        mcr     p15, 0, r0, c8, c7, 0           ; Flush the I&D TLBs

        mfc15   r1, c1
        orr     r1, r1, #0x007F                 ; changed to read-mod-write for ARM920 Enable: MMU, Align, DCache, WriteBuffer
        orr     r1, r1, #0x3200                 ; vector adjust, ICache, ROM protection
        ldr     r0, VirtualStart
        cmp     r0, #0                          ; make sure no stall on "mov pc,r0" below
        mtc15   r1, c1                          ; enable the MMU & Caches
        mov     pc, r0                          ;  & jump to new virtual address
        nop

; MMU & caches now enabled.
;
;       (r10) = physcial address of 1st level page table

VStart  ldr     sp, =KStack
        add     r4, sp, #KData-KStack           ; (r4) = ptr to KDataStruct
        mov     r0, #0x80000000
        str     r0, [r4, #hBase]                ; set HandleBase
        ldr     r0, =DirectRet
        str     r0, [r4, #pAPIReturn]           ; set DirectReturn address

; Initialize stacks for each mode.
        mov     r1, #ABORT_MODE:OR:0xC0
        msr     cpsr_c, r1                      ; switch to Abort Mode w/IRQs disabled
        add     sp, r4, #AbortStack-KData
        mov     r1, sp
        mov     r2, #IRQ_MODE:OR:0xC0
        msr     cpsr_c, r2                      ; switch to IRQ Mode w/IRQs disabled
        add     sp, r4, #IntStack-KData
        mov     r2, sp

        mov     r3, #FIQ_MODE:OR:0xC0
        msr     cpsr_c, r3                      ; switch to FIQ Mode w/IRQs disabled
        add     sp, r4, #FIQStack-KData

        mov     r3,  #UNDEF_MODE:OR:0xC0
        msr     cpsr_c, r3                      ; switch to Undefined Mode w/IRQs disabled
        mov     sp, r4                          ; (sp_undef) = &KData
        mov     r3, sp

        mov     r0, #SVC_MODE:OR:0xC0
        msr     cpsr_c, r0                      ; switch to Supervisor Mode w/IRQs disabled
        mfc15   r0, c0                          ; (r0) = processor ID

        stmfd   sp!, {r6}                       ; 5th argument: &OEMAddressTable
        CALL    ARMInit
        ldmfd   sp!, {r6}

        mov     r1, #SVC_MODE 
        msr     cpsr_c, r1                      ; switch to Supervisor Mode w/IRQs enabled
        CALL    KernelInit                      ; initialize scheduler, etc.
        mov     r0, #0                          ; no current thread
        mov     r1, #ID_RESCHEDULE
        b       FirstSchedule

VirtualStart DCD VStart

VectorInstructions
        ldr     pc, [pc, #0x3E0-8]              ; reset
        ldr     pc, [pc, #0x3E0-8]              ; undefined instruction
        ldr     pc, [pc, #0x3E0-8]              ; SVC
        ldr     pc, [pc, #0x3E0-8]              ; Prefetch abort
        ldr     pc, [pc, #0x3E0-8]              ; data abort
        ldr     pc, [pc, #0x3E0-8]              ; unused vector location
        ldr     pc, [pc, #0x3E0-8]              ; IRQ
        ldr     pc, [pc, #0x3E0-8]              ; FIQ

        LTORG

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY    UndefException
 IF Interworking :LOR: Thumbing
        stmdb   sp, {r0-r3, lr}
        mrs     r0, spsr
        tst     r0, #THUMB_STATE                ; Entered from Thumb Mode ?
        subne   lr, lr, #2                      ; Update return address
        subeq   lr, lr, #4                      ;   accordingly
        str     lr, [sp, #-4]                   ; update lr on stack
 ELSE
        sub     lr, lr, #4                      ; (lr) = address of undefined instruction
        stmdb   sp, {r0-r3, lr}
 ENDIF
        PROLOG_END
  IF {FALSE}
        ldr     r0, =LED_ALPHA
10      ldr     r1, =0x0DEF
        str     r1, [r0]
        mov     r1, #SPIN_DELAY
15      subs    r1, r1, #1
        bgt     %B15
        str     lr, [r0]
        mov     r1, #SPIN_DELAY
17      subs    r1, r1, #1
        bgt     %B17
  ENDIF

        mov     r1, #ID_UNDEF_INSTR
        b       CommonHandler
        ENTRY_END UndefException

        LEAF_ENTRY SWIHandler
  IF {FALSE}
        ldr     r0, =LED_ALPHA
10      ldr     r1, =0x99999999
        str     r1, [r0]
        mov     r1, #SPIN_DELAY
15      subs    r1, r1, #1
        bgt     %B15
        str     lr, [r0]
        mov     r1, #SPIN_DELAY
17      subs    r1, r1, #1
        bgt     %B17
        b       %B10
  ENDIF
        movs    pc, lr
        ENTRY_END SWIHandler



;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY FIQHandler
        sub     lr, lr, #4                      ; fix return address
        stmfd   sp!, {r0-r3, r12, lr}
        PROLOG_END

        CALL    OEMInterruptHandlerFIQ

        ldmfd   sp!, {r0-r3, r12, pc}^          ; restore regs & return for NOP
        ENTRY_END FIQHandler


        LTORG

;-------------------------------------------------------------------------------
;++
; The following code is never executed. Its purpose is to support unwinding
; through the calls to API dispatch and return routines.
;--
;-------------------------------------------------------------------------------

; field description for OBJCALLSTRUCT/SVRRTNSTRUCT
apiMethod      equ   0                    ; API method
objcallstr     equ   apiMethod            ; argument to ObjectCall
apiPrevSP      equ   (apiMethod + 4)      ; previous SP if stack changed
svrrtnstr      equ   apiPrevSP            ; argument to ServerCallReturn
apiMode        equ   (apiMethod + 8)      ; which mode we call from
apiExtra       equ   (apiMethod + 12)     ; extra info 
apiRa          equ   (apiMethod + 16)     ; return address
size_api_args  equ   (apiMethod + 20)     ; length of stack frame

        NESTED_ENTRY    PrefetchAbortEH
        stmfd   sp!, {r0-r3}
        sub     sp, sp, #8
        PROLOG_END

        ALTERNATE_ENTRY PrefetchAbort

        sub     lr, lr, #0xF0000004
        cmp     lr, #0x00010400
        bhs     ProcessPrefAbort
        mrs     r12, spsr
        and     r12, r12, #0x0ff
        orr     r12, r12, lr, LSL #8            ; (r12) = old mode + 24 lower bits of LR. 
        msr     cpsr_c, #SYSTEM_MODE            ; switch to System Mode

; Find the address of the API function to dispatch to by:
;  ObjectCall(OBJCALLSTRUCT*)
;
;       (r0-r3) = function parameters or return values
;       (r12) = abort address - 0xF0000000 (bits 31-8) (encodes API set & method index)
;               combined with the previous mode (bits 7-0)


        ; we need some scratch registers but we don't have any to use here. So, we'll have to 
        ; use caller's stack to save off the registers. Need to be careful not to forget about
        ; it when we switch stack
        stmfd   sp!, {r0-r3}                    ; save function parameters onto the stack
        mov     r0, #FIRST_METHOD - 0xF0000000
        rsb     r0, r0, r12, LSR #8
        mov     r0, r0, ASR #2                  ; (r0) = iMethod
        cmp     r0, #-1
        beq     CBRtn                           ; process callback return if it is

        ;
        ; PSL call
        ;
        mov     r1, #0                          ; (r1) = prevSP, init to 0
        and     r2, r12, #0x1f                  ; (r2) = previous mode (User/Kernel)
        cmp     r2, #SYSTEM_MODE
        beq     KPSLCall

        ;
        ; PSL Call from USER MODE
        ;

        ; special case for RaiseException
        mov     r3, #RAISEEXCEPTION
        cmp     r0, r3                          ; RaiseException?
        beq     PSLCallCommon                   ; do not switch stack if it is
        
        ; check trust level
        ldr     r3, =KData                      ; (r3) = ptr to KData page
        ldr     r2, [r3,#pCurPrc]               ; (r2) = pCurProc
        ldrb    r2, [r2, #PrcTrust]             ; (r2) = pCurProc->bTrustLevel
        cmp     r2, #KERN_TRUST_FULL            ; Is it fully trusted?
        mov     r2, #USER_MODE                  ; (r2) = USER_MODE
        beq     PSLCallCommon                   ; go to common code if fully trusted

        ;
        ; not trusted, switch stack (r3) = KData
        ;

        ; update TLS first
        ldr     r2, [r3, #pCurThd]              ; (r2) = pCurThread
        ldr     r1, [r2, #ThTlsSecure]          ; (r1) = pCurThread->tlsSecure
        str     r1, [r2, #ThTlsPtr]             ; pCurThread->tlsPtr = pCurThread->tlsSecure
        str     r1, [r3, #lpvTls]               ; lpvTls = pCurThread->tlsSecure
        
        sub     r1, r1, #SECURESTK_RESERVE      ; (r1) = new stack, init to (tlsptr - SECURESTK_RESERVE)
        
        ; find the 'real' callstack (SEH might put in a faked one)
        ldr     r2, [r2, #ThPcstkTop]           ; (r2) = pCurThread->pcstkTop
NextCStk
        tst     r2, r2                          ; pcstk == NULL?
        beq     CopyStack

        ldr     r3, [r2, #CstkAkyLast]          ; (r3) = pcstk->akyLast
        tst     r3, r3
        bne     FoundCStk                       ; pcstk->akyLast != 0 ==> Found

        ldr     r2, [r2, #CstkNext]             ; pcstk = pcstk->pcstkNext
        b       NextCStk

FoundCStk
        ldr     r1, [r2, #CstkPrevSP]           ; (r1) = pcstk->dwPrevSP (new stack)

CopyStack
        ; (r1) = new STACK
        ; (sp) = old stack (original r0 - r3 are on stack too)
        ;       I'd like to use stm/ldm, but we're out of registers...

        ; need to update SP before touch the new stack or we might have a inconsistant
        ; stack bound
        mov     r2, r1                          ;\   ; don't remove this or '\' at the end of line will bite
        mov     r1, sp                          ; * exchange old and new stack
        sub     sp, r2, #MAX_PSL_ARGS           ;/   ; also make room for PSL args

        ldr     r2, [r1]
        str     r2, [sp]
        ldr     r2, [r1, #4]
        str     r2, [sp, #4]
        ldr     r2, [r1, #8]
        str     r2, [sp, #8]
        ldr     r2, [r1, #12]
        str     r2, [sp, #12]
        ldr     r2, [r1, #16]
        str     r2, [sp, #16]
        ldr     r2, [r1, #20]
        str     r2, [sp, #20]
        ldr     r2, [r1, #24]
        str     r2, [sp, #24]
        ldr     r2, [r1, #28]
        str     r2, [sp, #28]
        ldr     r2, [r1, #32]
        str     r2, [sp, #32]
        ldr     r2, [r1, #36]
        str     r2, [sp, #36]
        ldr     r2, [r1, #40]
        str     r2, [sp, #40]
        ldr     r2, [r1, #44]
        str     r2, [sp, #44]
        ldr     r2, [r1, #48]
        str     r2, [sp, #48]
        ldr     r2, [r1, #52]
        str     r2, [sp, #52]

        add     r1, r1, #16                     ; we've pushed 4 args onto the old stack, take it out

        ; (r0) = iMethod
        ; (r1) = old SP 
        mov     r2, #USER_MODE                  ; (r2) = USER_MODE
        b       PSLCallCommon

KPSLCall
        cmp     r0, #PERFORMCALLBACK            ; is it callback?
        beq     DoPerformCallBack

        ; fall thru to common code
        
PSLCallCommon

        ; (r0) = iMethod
        ; (r1) = old sp
        ; (r2) = previous mode
        ; (r3) = linkage (don't care in ARM)
        ; (lr) = return address
        ; original r0 - r3 are on stack
        ;
        ; The following stmfd instruction is assuming that it's going to put
        ; the registers in the stack from high to low. i.e. we're assuming
        ; it's equivalent to 
        ;       push lr         (Ra in OBJCALLSTRUCT)
        ;       push r3         (linkage)
        ;       push r2         (mode)
        ;       push r1         (prevSP)
        ;       push r0         (iMethod)
        ;
        stmfd   sp!, {r0-r3, lr}
        mov     r0, sp                          ; (r0) = ptr to OBJCALLSTRUCT
        CALL    ObjectCall

        ; always run PSL in KMODE
        add     sp, sp, #size_api_args          ; pop of the 5 args we pushed for ObjectCall
        mov     r12, r0                         ; (r12) = function to call

        ldmfd   sp!, {r0-r3}                    ; reload function paramters
        add     lr, pc, #DirectRet-(.+8)        ; update return address
  IF Interworking :LOR: Thumbing
        bx      r12                             ; invoke function in "kernel mode"
  ELSE
        mov     pc, r12                         ; invoke function in "kernel mode"
  ENDIF

DirectRet
        ;
        ;       (r0) = return value
        ;nop
        ;nop
        sub     sp, sp, #size_api_args          ;  make room for ServerCallReturn arugments

        str     r0, [sp, #apiRa]                ; save return value
        add     r0, sp, #svrrtnstr              ; (r0) = ptr to SVRRTNSTRUCT

        CALL    ServerCallReturn

        ; (r0) = address to continue
        mov     r12, r0                         ; (r12) = address to Continue
        ldr     r0, [sp, #apiRa]                ; restore return value
        ldr     r2, [sp, #apiMode]              ; (r1) = return mode
        ldr     r3, [sp, #apiPrevSP]            ; (r3) = target sp
        
        add     sp, sp, #size_api_args          ; pop the temp stuffs
        cmp     r2, #USER_MODE                  ; return to user mode?

  IF Interworking :LOR: Thumbing
        bxne    r12                             ; return in "kernel mode"
  ELSE
        movne   pc, r12                         ; return in "kernel mode"
  ENDIF
  
        ; returning to user mode. check if stack need to be changed
        tst     r3, r3
        beq     RtnToUMode

        ; need to switch STACK 
        mov     sp, r3                          ; update SP
        ; update TLS
        ldr     r3, =KData                      ; (r3) = ptr to KData page
        ldr     r2, [r3, #pCurThd]              ; (r2) = pCurThread
        ldr     r1, [r2, #ThTlsNonSecure]       ; (r1) = pCurThread->tlsNonSecure
        str     r1, [r2, #ThTlsPtr]             ; pCurThread->tlsPtr = pCurThread->tlsNonSecure
        str     r1, [r3, #lpvTls]               ; lpvTls = pCurThread->tlsNonSecure
        
RtnToUMode
        ; return to user mode
        ;  (r12) = address to continue
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to SVC Mode w/IRQs disabled

        tst     r12, #0x01                      ; returning to Thumb mode ??
        msrne   spsr_c, #USER_MODE | THUMB_STATE
        msreq   spsr_c, #USER_MODE              ; set the Psr according to returning state

        movs    pc, r12                         ; switch to User Mode and return


DoPerformCallBack
        ; orignal r0 - r3 are on stack already
        add     r1, sp, #16                     ; (r1) = original SP (+16 for r0-r3 we pushed)
        sub     sp, sp, #size_api_args          ; make room for PerformCallBackExt arguments

        ; setup PerformCallBackExt arguments
        str     lr, [sp, #apiRa]                ; return address
        str     r1, [sp, #apiPrevSP]            ; original SP

        add     r0, sp, #objcallstr             ; (r0) = ptr to OBJCALLSTRUCT
        CALL    PerformCallBackExt              ; call PerformCallBackExt

        ; (r0) = function to call
        mov     r12, r0                         ; (r12) = function to call
        ldr     r3, [sp, #apiMode]              ; (r3) = mode to call into
        ldr     r1, [sp, #apiPrevSP]            ; (r1) = target SP if non-zero
        add     sp, sp, #size_api_args          ; pop the temp stuffs

        tst     r1, r1                          ; need stack switch?
        beq     CBCommon                        ; go to common code if not

        ; need to switch stack
        ;
        ; must update TLS and SP before the copy, or stack bound might become inconsistent
        ; (r1) = new SP
        ; (r3) = mode to call into
        ; (r12) = function to call
        ;

        ; update TLS
        ldr     r0, =KData                      ; (r0) = ptr to KData page
        ldr     r2, [r0, #pCurThd]              ; (r2) = pCurThread
        ldr     r0, [r2, #ThTlsNonSecure]       ; (r0) = pCurThread->tlsNonSecure
        str     r0, [r2, #ThTlsPtr]             ; pCurThread->tlsPtr = pCurThread->tlsNonSecure
        ldr     r2, =KData                      ; (r0) = ptr to KData page
        str     r0, [r2, #lpvTls]               ; lpvTls = pCurThread->tlsNonSecure

        ;
        ; update SP
        mov     r0, sp                          ; (r0) = old sp (original r0 - r3 saved on stack)
        sub     sp, r1, #MAX_PSL_ARGS           ; room for arguments on the new stack

        ; copy arguments
        ldr     r1, [r0]
        ldr     r2, [r0, #4]
        str     r1, [sp]
        str     r2, [sp, #4]
        ldr     r1, [r0, #8]
        ldr     r2, [r0, #12]
        str     r1, [sp, #8]
        str     r2, [sp, #12]
        ldr     r1, [r0, #16]
        ldr     r2, [r0, #20]
        str     r1, [sp, #16]
        str     r2, [sp, #20]
        ldr     r1, [r0, #24]
        ldr     r2, [r0, #28]
        str     r1, [sp, #24]
        str     r2, [sp, #28]

        ; save callee saved registers
        ; NOTE: we're relying on the fact that we only save 8 registers
        ;       and will never trigger a DemmandCommit on the secure stack
        ;       since the apiArgs and {r0-r3} were saved on the secure stack
        ;       before and the area we need would have been committed.
        sub     r0, r0, #(CALLEE_SAVED_REGS - 16)   ; already have room for {r0 - r3}
        stmia   r0, {r4-r11}                    ; save the caller registers

CBCommon
        ; common code for perform callback
        ;       (r12) = function to call
        ;       (r3) = mode to call into

        ldr     lr, =SYSCALL_RETURN             ; return via a trap
        cmp     r3, #USER_MODE                  ; callback to user mode proc?
        ldmfd   sp!, {r0-r3}                    ;       reload function arguments
        beq     RtnToUMode                      ; return to user mode if so

        ; calling callback in kmode
        add     lr, pc, #MD_CBRtn-(.+8)
  IF Interworking :LOR: Thumbing
        bx      r12                             ; invoke function in "kernel mode"
  ELSE
        mov     pc, r12                         ; invoke function in "kernel mode"
  ENDIF

CBRtn
        ; return from CALLBACK
        ;       r0 - r3 saved on stack
        ldmfd   sp!, {r0-r3}                    ; restore r0-r3

        ; fall thru to general callback return processing
MD_CBRtn
        ; (r0) = return value
        ldr     r3, =KData                      ; (r3) = ptr to KData page
        ldr     r2, [r3, #pCurThd]              ; (r2) = pCurThread
        ldr     r1, [r2, #ThPcstkTop]           ; (r1) = pCurThread->pcstkTop
        ldr     r1, [r1, #CstkPrevSP]           ; (r1) = pCurThread->pcstkTop->dwPrevSP

        tst     r1, r1                          ; need to switch stack?
        beq     CBRtnCommon                     ; go to common code if not
        ;
        ; stack switch is needed.
        ;

        ; restore callee saved registers
        ldmia   r1, {r4-r11}
        
        ; update TLS
        ldr     r12, [r2, #ThTlsSecure]         ; (r12) = pCurThread->tlsSecure
        str     r12, [r2, #ThTlsPtr]            ; pCurThread->tlsPtr = pCurThread->tlsSecure
        str     r12, [r3, #lpvTls]              ; update global TLS

        ; update SP
        add     sp, r1, #CALLEE_SAVED_REGS

CBRtnCommon
        ;
        ; common code for callback return
        ;
        sub     sp, sp, #size_api_args          ; make room for CallbackReturn arguments
        str     r0, [sp, #apiRa]                ; save return value
        add     r0, sp, #svrrtnstr              ; r0 = ptr to SVRRTNSTRUCT
        CALL    CallbackReturn                  ; call CallbackReturn

        ; (r0) = return address 
        mov     r12, r0                         ; (r12) = return address 
        ldr     r0, [sp, #apiRa]                ; (r0) = return value
        add     sp, sp, #size_api_args          ; pop the temp stuffs

        ; continue at return address in kmode
  IF Interworking :LOR: Thumbing
        bx      r12                             ; return in "kernel mode"
  ELSE
        mov     pc, r12                         ; return in "kernel mode"
  ENDIF

        ENTRY_END PrefetchAbortEH

; Process a prefetch abort.

ProcessPrefAbort
        add     lr, lr, #0xF0000000             ; repair continuation address
        stmfd   sp!, {r0-r3, r12, lr}
  IF {FALSE}
        ldr     r0, =LED_ALPHA
10      ldr     r1, =0xC0DEAB00
        str     r1, [r0]
        mov     r1, #SPIN_DELAY
15      subs    r1, r1, #1
        bgt     %B15
        str     lr, [r0]
        mov     r1, #SPIN_DELAY
17      subs    r1, r1, #1
        bgt     %B17
  ENDIF
        mov     r0, lr                          ; (r0) = faulting address
        mfc15   r2, c13                         ; (r2) = process base address
        tst     r0, #0xFE000000                 ; slot 0 reference?
        orreq   r0, r0, r2                      ; (r0) = process slot based address
        CALL    LoadPageTable                   ; (r0) = !0 if entry loaded
        tst     r0, r0
        ldmnefd sp!, {r0-r3, r12, pc}^          ; restore regs & continue
        ldmfd   sp!, {r0-r3, r12}
        ldr     lr, =KData-4
        stmdb   lr, {r0-r3}
        ldmfd   sp!, {r0}
        str     r0, [lr]                        ; save resume address
        mov     r1, #ID_PREFETCH_ABORT          ; (r1) = exception ID
        b       CommonHandler


        LTORG

;-------------------------------------------------------------------------------
; Common fault handler code.
;
;       The fault handlers all jump to this block of code to process an exception
; which cannot be handled in the fault handler or to reschedule.
;
;       (r1) = exception ID
;       original r0-r3 & lr saved at KData-0x14.
;-------------------------------------------------------------------------------
        ALTERNATE_ENTRY CommonHandler
        mrs     r2, spsr
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to Supervisor mode w/IRQs disabled
        ldr     r3, =KData                      ; (r3) = ptr to KData page


; Save the processor state into a thread structure. If the previous state was
; User or System and the kernel isn't busy, then save the state into the current
; thread. Otherwise, create a temporary thread structure on the kernel stack.
;
;       (r1) = exception ID
;       (r2) = SPSR
;       (r3) = ptr to KData page
;       Interrupted r0-r3, and Pc saved at (r3-0x14)
;       In Supervisor Mode.
        ALTERNATE_ENTRY SaveAndReschedule
  IF {FALSE}
        ldrb    r0, [r3,#cNest]                 ; (r0) = kernel nest level
        subs    r0, r0, #1                      ; in a bit deeper (0 if first entry)
        strb    r0, [r3,#cNest]                 ; save new nest level
  ENDIF
        and     r0, r2, #0x1f                   ; (r0) = previous mode
        cmp     r0, #USER_MODE                  ; 'Z' set if from user mode
        cmpne   r0, #SYSTEM_MODE                ; 'Z' set if from System mode
        bne     %F50                            ; reentering kernel, save state on stack

        ldr     r0, [r3,#pCurThd]               ; (r0) = ptr to current thread
        add     r0, r0, #TcxR4                  ; (r0) = ptr to r4 save
        stmia   r0, {r4-r14}^                   ; save User bank registers
10      ldmdb   r3, {r3-r7}                     ; load saved r0-r3 & Pc
        stmdb   r0!, {r2-r6}                    ; save Psr, r0-r3
        sub     r0, r0, #THREAD_CONTEXT_OFFSET  ; (r0) = ptr to Thread struct
        str     r7, [r0,#TcxPc]                 ; save Pc
        mfc15   r2, c6                          ; (r2) = fault address
        mfc15   r3, c5                          ; (r3) = fault status

; Process an exception or reschedule request.

FirstSchedule
20      msr     cpsr_c, #SVC_MODE               ; enable interrupts
        CALL    HandleException
        ldr     r2, [r0, #TcxPsr]               ; (r2) = target status
        and     r1, r2, #0x1f                   ; (r1) = target mode
        cmp     r1, #USER_MODE
        cmpne   r1, #SYSTEM_MODE
        bne     %F30                            ; not going back to user or system mode
        add     r0, r0, #TcxR3
        ldmia   r0, {r3-r14}^                   ; reload user/system mode registers
        ldr     r1, =KData
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; disable all interrupts
        ldrb    r1, [r1, #bResched]             ; (r1) = nest level + reschedule flag
        cmp     r1, #1
        mov     r1, #ID_RESCHEDULE
        beq     %B20                            ; interrupted, reschedule again
        msr     spsr, r2
        ldr     lr, [r0, #TcxPc-TcxR3]
        ldmdb   r0, {r0-r2}
        movs    pc, lr                          ; return to user or system mode

; Return to a non-preemptible privileged mode.
;
;       (r0) = ptr to THREAD structure
;       (r2) = target mode

30
  IF Interworking :LOR: Thumbing

        tst     r2, #THUMB_STATE                ; returning to Thumb code ?
        addne   r1, r0, #THREAD_CONTEXT_OFFSET  ; create pointer to context
        bne     ThumbDispatch                   ; and branch to thumb dispatch code

  ENDIF

        msr     cpsr, r2                        ; switch to target mode
        add     r0, r0, #TcxR0
        ldmia   r0, {r0-r15}                    ; reload all registers & return

; Save registers for fault from a non-preemptible state.

50      sub     sp, sp, #TcxSizeof              ; allocate space for temp. thread structure
        cmp     r0, #SVC_MODE
        bne     %F55                            ; must mode switch to save state
        add     r0, sp, #TcxR4                  ; (r0) = ptr to r4 save area
        stmia   r0, {r4-r14}                    ; save SVC state registers
        add     r4, sp, #TcxSizeof              ; (r4) = old SVC stack pointer
        str     r4, [r0, #TcxSp-TcxR4]          ; update stack pointer value
        b       %B10

55
  IF Interworking :LOR: Thumbing
        bic     r0, r2, #THUMB_STATE            ; Ensure Thumb bit is clear
        msr     cpsr, r0                        ; and switch to mode exception came from
  ELSE
        msr     cpsr, r2                        ; switch to mode exception came from
  ENDIF

        add     r0, sp, #TcxR4                  ; (r0) = ptr to r4 save area
        stmia   r0, {r4-r14}                    ; save mode's register state
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; back to supervisor mode
        b       %B10                            ; go save remaining state

        LTORG


;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY    DataAbortHandler
        sub     lr, lr, #8                      ; repair continuation address
        stmfd   sp!, {r0-r3, r12, lr}
        PROLOG_END

        sub     r0, lr, #INTERLOCKED_START
        cmp     r0, #INTERLOCKED_END-INTERLOCKED_START
        bllo    CheckInterlockedRestart
        mfc15   r0, c6                          ; (r0) = FAR
        mfc15   r1, c5                          ; (r1) = FSR

  IF {FALSE}
        ldr     r2, =LED_DISCRETE
        str     r1, [r2]
        ldr     r2, =LED_ALPHA
10      ldr     r3, =0xDA2AAB00
        str     r3, [r2]
        mov     r3, #SPIN_DELAY
15      subs    r3, r3, #1
        bgt     %B15
        str     lr, [r2]
        mov     r3, #SPIN_DELAY
17      subs    r3, r3, #1
        bgt     %B17
        str     r0, [r2]
        mov     r3, #SPIN_DELAY
19      subs    r3, r3, #1
        bgt     %B19
;;;     b       %B10
  ENDIF

        mfc15   r2, c13                         ; (r2) = process base address
        tst     r0, #0xFE000000                 ; slot 0 reference?
        orreq   r0, r0, r2                      ; (r0) = process slot based address
        and     r1, r1, #0x0D                   ; type of data abort
        cmp     r1, #0x05                       ; translation error?
        movne   r0, #0
        CALLEQ  LoadPageTable                   ; (r0) = !0 if entry loaded
        tst     r0, r0
        ldmnefd sp!, {r0-r3, r12, pc}^          ; restore regs & continue
        ldr     lr, =KData-4
        ldmfd   sp!, {r0-r3, r12}
        stmdb   lr, {r0-r3}
        ldmfd   sp!, {r0}
        str     r0, [lr]                        ; save resume address
        mov     r1, #ID_DATA_ABORT              ; (r1) = exception ID
        b       CommonHandler

        ENTRY_END DataAbortHandler



;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY IRQHandler
        sub     lr, lr, #4                      ; fix return address
        stmfd   sp!, {r0-r3, r12, lr}
        PROLOG_END

  IF {FALSE}
        ldr     r0, =LED_ALPHA
10      ldr     r1, =0x09090a0a
        str     r1, [r0]
        mov     r1, #SPIN_DELAY
15      subs    r1, r1, #1
        bgt     %B15
        str     lr, [r0]
        mov     r1, #SPIN_DELAY
17      subs    r1, r1, #1
        bgt     %B17
  ENDIF
        ;
        ; Test interlocked API status.
        ;
        sub     r0, lr, #INTERLOCKED_START
        cmp     r0, #INTERLOCKED_END-INTERLOCKED_START
        bllo    CheckInterlockedRestart

  IF CELOG
        mov     r0,#0x80000000                  ; mark as ISR entry
        stmfd   sp!, {r0, lr}
        CALL    CeLogInterrupt
        ldmfd   sp!, {r0, lr}
        
        ;
        ; mark that we are going a level deeper
        ;
        
        ldr     r1, =KData
        ldrsb   r0, [r1,#cNest]                 ; (r0) = kernel nest level
        add     r0, r0, #1                      ; in a bit deeper (0 if first entry)
        strb    r0, [r1,#cNest]                 ; save new nest level
  ENDIF

        ;
        ; CAREFUL! The stack frame is being altered here. It's ok since
        ; the only routine relying on this was the Interlock Check. Note that
        ; we re-push LR onto the stack so that the incoming argument area to
        ; OEMInterruptHandler will be correct.
        ;
        mrs     r1, spsr                        ; (r1) = saved status reg
        stmfd   sp!, {r1}                       ; save SPSR onto the IRQ stack
        mov     r0,lr                           ; parameter to OEMInterruptHandler

        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to supervisor mode w/IRQs disabled
        stmfd   sp!, {lr}                       ; save LR onto the SVC stack
        stmfd   sp!, {r0}                       ; save IRQ LR (in R0) onto the SVC stack (param)

        ;
        ; Now we call the OEM's interrupt handler code. It is up to them to
        ; enable interrupts if they so desire. We can't do it for them since
        ; there's only on interrupt and they haven't yet defined their nesting.
        ;

        CALL    OEMInterruptHandler

        ldmfd   sp!, {r1}                       ; dummy pop (parameter)
        ldmfd   sp!, {lr}                       ; restore SVC LR from the SVC stack
        msr     cpsr_c, #IRQ_MODE:OR:0x80       ; switch back to IRQ mode w/IRQs disabled

        ;
        ; Restore the saved program status register from the stack.
        ;
        ldmfd   sp!, {r1}                       ; restore IRQ SPSR from the IRQ stack
        msr     spsr, r1                        ; (r1) = saved status reg

  IF CELOG
        ;
        ; call CeLogInterrupt with cNest << 16 | SYSINTR
        ;
        stmfd   sp!, {r0}
        
        ldr     lr, =KData
        ldrsb   r1, [lr,#cNest]                 ; (r1) = cNest value (1, 2, etc)
        sub     r1, r1, #1                      ; make one level less
        orr     r0, r0, r1, LSL #16             ; (r0) = cNest value << 16 + SYSINTR
        strb    r1, [lr,#cNest]        
        
        stmfd   sp!, {r0}
        CALL    CeLogInterrupt
        ldmfd   sp!, {r0}
        
        ldmfd   sp!, {r0}
  ENDIF

        ldr     lr, =KData                      ; (lr) = ptr to KDataStruct
        cmp     r0, #SYSINTR_RESCHED
        beq     %F10
        sub     r0, r0, #SYSINTR_DEVICES
        cmp     r0, #SYSINTR_MAX_DEVICES
        ;
        ; If not a device request (and not SYSINTR_RESCHED)
        ;        
        ldrhsb  r0, [lr, #bResched]             ; (r0) = reschedule flag
        bhs     %F20                            ; not a device request
        
        ldr     r2, [lr, #PendEvents]           ; (r2) = pending interrupt event mask
        mov     r1, #1
        orr     r2, r2, r1, LSL r0              ; (r2) = new pending mask
        str     r2, [lr, #PendEvents]           ; save it

        ;
        ; mark reschedule needed
        ;
10      ldrb    r0, [lr, #bResched]             ; (r0) = reschedule flag
        orr     r0, r0, #1                      ; set "reschedule needed bit"
        strb    r0, [lr, #bResched]             ; update flag

20      mrs     r1, spsr                        ; (r1) = saved status register value
        and     r1, r1, #0x1F                   ; (r1) = interrupted mode
        cmp     r1, #USER_MODE                  ; previously in user mode?
        cmpne   r1, #SYSTEM_MODE                ; if not, was it system mode?
        cmpeq   r0, #1                          ; user or system: is resched == 1
        ldmnefd sp!, {r0-r3, r12, pc}^          ; can't reschedule right now so return
        
        sub     lr, lr, #4
        ldmfd   sp!, {r0-r3, r12}
        stmdb   lr, {r0-r3}
        ldmfd   sp!, {r0}
        str     r0, [lr]                        ; save resume address
        mov     r1, #ID_RESCHEDULE              ; (r1) = exception ID

        b       CommonHandler

        ENTRY_END IRQHandler

        ;;;NESTED_ENTRY FIQResched



;-------------------------------------------------------------------------------
; CheckInterlockedRestart - check for restarting an InterlockedXXX API call
;
;       This routine is called by the Data Abort and IRQ handlers when the PC of
; the aborted or interrupted instruction is inside the interlocked api region
; contained at the tail of the kernel data page.  If the PC points to a MOV
; instruction then the operation has completed so no restart is needed.
; Otherwise, a backwards scan is made to look for a "ldr r12, [r0]" instruction
; which is the beginning of all of the interlocked api routines.
;
;       Entry   (r0) = interrupted PC - INTERLOCKED_START
;               (sp) = ptr to saved registers ({r0-r3, r12, lr}
;       Exit    LR value in register frame updated if necessary
;       Uses    r0, r1, r2
;-------------------------------------------------------------------------------
        LEAF_ENTRY CheckInterlockedRestart
  IF {FALSE}
        ldr     r1, =LED_ALPHA
        ldr     r2, =0xCEFF00AA
        str     r2, [r1]
  ENDIF
        add     r0, r0, #INTERLOCKED_START      ; (r0) = interrupted PC
        ldr     r1, [r0]                        ; (r1) = interrupted instruction
        ldr     r2, =0xE1A0                     ; (r2) = "mov rx, ry" opcode
        cmp     r2, r1, LSR #16                 ; is it a MOV instruction?
  IF Interworking :LOR: Thumbing
        ldrne   r2, =0xE12FFF1                  ; or is it a BX instruction ?
        cmpne   r2, r1, LSR #4
  ENDIF
        moveq   pc, lr                          ; Y: return to caller
        ldr     r2, =0xE590C000                 ; (r2) = "ldr r12, [r0]" instruction
        cmp     r1, r2                          ; at start of routine?
        moveq   pc, lr                          ;   Y: return to caller
10      ldr     r1, [r0, #-4]!                  ; (r1) = previous instruction
        cmp     r1, r2                          ; found start of routine?
        bne     %B10                            ;   N: keep backing up
        str     r0, [sp, #FrameLR]              ; update return address in stack frame

  IF {FALSE}
        ldr     r1, =LED_ALPHA
        ldr     r2, =0xCEBACBAC
        str     r2, [r1]
  ENDIF
        mov     pc, lr                          ; return



;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY ZeroPage
;       void ZeroPage(void *vpPage)
;
;       Entry   (r0) = (vpPage) = ptr to address of page to zero
;       Return  none
;       Uses    r0-r3, r12

        mov     r1, #0
        mov     r2, #0
        mov     r3, #0
        mov     r12, #0
10      stmia   r0!, {r1-r3, r12}               ; clear 16 bytes (64 bytes per loop)
        stmia   r0!, {r1-r3, r12}
        stmia   r0!, {r1-r3, r12}
        stmia   r0!, {r1-r3, r12}
        tst     r0, #0xFF0
        bne     %B10

        RETURN                                  ; return to caller



;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY GetHighPos

        add     r1, pc, #PosTable-(.+8)
        mov     r2, #-1
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        teq     r3, #0
        bne     done

        mov     r0, r0, lsr #8
        add     r2, r2, #8
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        teq     r3, #0
        bne     done

        mov     r0, r0, lsr #8
        add     r2, r2, #8
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        teq     r3, #0
        bne     done

        mov     r0, r0, lsr #8
        add     r2, r2, #8
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        teq     r3, #0
        bne     done

        add     r3, r3, #9
done
        add     r0, r3, r2
        RETURN                                  ; return to caller


PosTable
       DCB 0,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 8,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1



;-------------------------------------------------------------------------------
; INTERRUPTS_ON - enable interrupts
;-------------------------------------------------------------------------------
        LEAF_ENTRY INTERRUPTS_ON
        mrs     r0, cpsr                        ; (r0) = current status
        bic     r1, r0, #0x80                   ; clear interrupt disable bit
        msr     cpsr, r1                        ; update status register

        RETURN                                  ; return to caller


;-------------------------------------------------------------------------------
; INTERRUPTS_OFF - disable interrupts
;-------------------------------------------------------------------------------
        LEAF_ENTRY INTERRUPTS_OFF
        mrs     r0, cpsr                        ; (r0) = current status
        orr     r1, r0, #0x80                   ; set interrupt disable bit
        msr     cpsr, r1                        ; update status register

        RETURN                                  ; return to caller

;-------------------------------------------------------------------------------
; INTERRUPTS_ENABLE - enable/disable interrupts based on arguemnt and return current status
;-------------------------------------------------------------------------------
        LEAF_ENTRY INTERRUPTS_ENABLE

        mrs     r1, cpsr                        ; (r1) = current status
        cmp     r0, #0                          ; eanble or disable?
        orreq   r2, r1, #0x80                   ; disable - set interrupt disable bit
        bicne   r2, r1, #0x80                   ; enable - clear interrupt disable bit
        msr     cpsr, r2                        ; update status register

        ands    r1, r1, #0x80                   ; was interrupt enabled?
        moveq   r0, #1                          ; yes, return 1
        movne   r0, #0                          ; no, return 0

        RETURN                                  ; return to caller


;-------------------------------------------------------------------------------
; _SetCPUASID - setup processor specific address space information
;
;  SetCPUASID will update the virtual memory tables so that new current process
; is mapped into slot 0 and set the h/w address space ID register for the new
; process.
;
;       Entry   (r0) = ptr to THREAD structure
;       Return  nothing
;       Uses    r0-r3, r12
;-------------------------------------------------------------------------------
        LEAF_ENTRY _SetCPUASID

        ldr     r12, =KData                     ; (r12) = ptr to KDataStruct
        ldr     r1, [r0, #ThProc]               ; (r1) = ptr to thread's current process
        ldr     r2, [r12, #CeLogStatus]         ; (r2) = KInfoTable[KINX_CELOGSTATUS]
        ldr     r0, [r1, #PrcHandle]            ; (r0) = handle of new process

        ; (r0) = handle of new process
        ; (r1) = pTh->pProc
        ; (r2) = KInfoTable[KINX_CELOGSTATUS]
        ; (r12) = ptr to KData
        ;
        cmp     r2, #1                          ; if status == CELOGSTATUS_ENABLED_GENERAL
        beq     %F35                            ; then do CeLog call
31
        ;   (r0) = pTh->pProc->hTh (handle of new process)
        ;   (r1) = pTh->pProc
        ;   (r12) = ptr to KData
        ;
        ldr     r3, [r1, #PrcVMBase]            ; (r3) = current proc's slot base address
        str     r1, [r12, #pCurPrc]             ; set pCurProc
        str     r0, [r12, #hCurProc]            ; set hCurProc
        add     r12, r12, #aSections            ; (r12) = ptr to SectionTable

        ldrb    r0, [r1, #PrcID]                ; (r0) = pCurProc->procnum
        cmp     r0, #0                          ; is it NK?
        ldreq   r2, =NKSection                  ; use NK Section if it is
        ldrne   r2, [r12, r3, LSR #VA_SECTION-2]  ; use per-process's Section if not,
                                                ; (r2) = process's memory section
                                                
        mtc15   r3, c13                         ; set process base address register
        str     r2, [r12]                       ; copy to slot 0 for VirtualAlloc & friends
        ; Set DOMAIN Access.

        RETURN                                  ; return to caller

35        
        ; --------- CeLogThreadMigrate ---------
        ldr     r2, [r12, #hCurProc]            ; (r2) = hCurProc
        cmp     r0, r2                          ; if new process == old process
        beq     %B31                            ; then skip CeLog call

        stmfd   sp!, {r0-r1, r12, lr}           ; save regs

        CALL    CeLogThreadMigrate

        ldmfd   sp!, {r0-r1, r12, lr}           ; restore regs
        b       %B31
        ; ------- End CeLogThreadMigrate -------

  IF CELOG
        LTORG
  ENDIF




;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY KCall
        PROLOG_END

; KCall - call kernel function
;
;       KCall invokes a kernel function in a non-preemtable state by switching
; to SVC mode.  This switches to the kernel stack and inhibits rescheduling.
;
;       Entry   (r0) = ptr to function to call
;               (r1) = first function arg
;               (r2) = second fucntion arg
;               (r3) = third function arg
;       Exit    (r0) = function return value
;       Uses    r0-r3, r12

        mrs     r12, cpsr                       ; (r12) = current status
        and     r12, r12, #0x1F                 ; (r12) = current mode
        cmp     r12, #SYSTEM_MODE
        mov     r12, r0                         ; (r12) = address of function to call
        mov     r0, r1                          ; \  ;
        mov     r1, r2                          ;  | ripple args down
        mov     r2, r3                          ; /

 IF Interworking :LOR: Thumbing
        bxne    r12                             ; already non-preemptible, invoke routine directly
  ELSE
        movne   pc, r12                         ; already non-preemptible, invoke routine directly
  ENDIF

        msr     cpsr_c, #SVC_MODE
        mov     lr, pc                          ; (lr_svc) = PC+8

 IF Interworking :LOR: Thumbing
        bx      r12                             ; invoke routine in non-preemptible state
  ELSE
        mov     pc, r12                         ; invoke routine in non-preemptible state
  ENDIF

        msr     cpsr_c, #SYSTEM_MODE            ; back to preemtible state
        ldr     r3, =KData                      ; (r3) = ptr to KDataStruct
        ldrb    r12, [r3, #bResched]            ; (r12) = reschedule flag
        cmp     r12, #1
        beq     %F1
        ldrb    r12, [r3, #dwKCRes]
        cmp     r12, #1

        RETURN_NE                               ; no reschedule needed so return

1       mov     r12, lr
        mrs     r2, cpsr
 IF Interworking :LOR: Thumbing
        tst     r12, #0x01                      ; return to thumb mode ?
        orrne   r2, r2, #THUMB_STATE            ; then update the Psr
 ENDIF
        msr     cpsr_c, #SVC_MODE:OR:0x80
        stmdb   r3, {r0-r3, r12}
        mov     r1, #ID_RESCHEDULE
        b       SaveAndReschedule
        ENTRY_END KCall

VFP_ENABLE          EQU     0x40000000
VFP_FPEXC_EX_BIT    EQU     0x80000000

;
;   VFP support routines
;

;------------------------------------------------------------------------------
; void TestVFP (void);
;------------------------------------------------------------------------------
        LEAF_ENTRY TestVFP
        fmrx    r0, fpexc
        RETURN
        
        ENTRY_END TestVFP

;------------------------------------------------------------------------------
; DWORD ReadAndSetFpexc (DWORD dwNewFpExc);
;------------------------------------------------------------------------------
        LEAF_ENTRY ReadAndSetFpexc
        mov     r1, r0          ; (r1) = new value of fpexc
        fmrx    r0, fpexc       ; (r0) = return old value of fpexc
        fmxr    fpexc, r1       ; update fpexc
        RETURN
        
        ENTRY_END ReadAndSetFpexc

;------------------------------------------------------------------------------
; void SaveFloatContext (PTHREAD pth);
;------------------------------------------------------------------------------
        LEAF_ENTRY SaveFloatContext

        ; enable VFP
        mov     r1, #VFP_ENABLE
        fmxr    fpexc, r1
        
        ; save fpcsr
        fmrx    r1, fpscr
        str     r1, [r0, #TcxFpscr]

        ; save all the general purpose registers
        add     r0, r0, #TcxRegs
        fstmiad r0, {d0-d15}

        RETURN
        ENTRY_END

;------------------------------------------------------------------------------
; void RestoreFloatContext (PTHREAD pth);
;------------------------------------------------------------------------------
        LEAF_ENTRY RestoreFloatContext

        ; enable VFP
        mov     r1, #VFP_ENABLE
        fmxr    fpexc, r1

        ; restore fpscr
        ldr     r1, [r0, #TcxFpscr]
        fmxr    fpscr, r1
        
        ; restore all the general purpose registers
        add     r0, r0, #TcxRegs
        fldmiad r0, {d0-d15}

        RETURN
        ENTRY_END

;-------------------------------------------------------------------------------
; InSysCall - check if in non-preemptible state
;
;       InSysCall is called to test if code is being called in a preemptible state
; or not.  Only user mode or system mode code is preemptible.  Since this code
; cannot be called in user mode, we just check for system mode.
;
;       Entry   none
;       Return  (r0) = 0 if preemtible, !=0 not preemptible
;-------------------------------------------------------------------------------
        LEAF_ENTRY InSysCall

        mrs     r0, cpsr                        ; (r0) = current status
        and     r0, r0, #0x1F                   ; (r0) = current mode
        eor     r0, r0, #SYSTEM_MODE            ; (r0) = 0 iff in system mode

        RETURN                                  ; return to caller



;-------------------------------------------------------------------------------
; CaptureContext is invoked in kernel context on the user thread's stack to
; build a context structure to be used for exception unwinding.
;
; Note: The Psr & Pc values will be updated by ExceptionDispatch from information
;       in the excinfo struct pointed at by the THREAD.
;
;       (sp) = aligned stack pointer
;       (r0-r14), etc. - CPU state at the time of exception
;-------------------------------------------------------------------------------
        ; Fake prolog for unwinding.
        NESTED_ENTRY xxCaptureContext

        mov     r12, sp                         ; restore original sp
        mrs     r0, cpsr
        stmdb   sp!, {r0}                       ; Unwind the Psr register
        ; The following instruction is a special hint to the unwinder, giving
        ; the location of the exception PC.  See unwind.c:ArmVirtualUnwind().
        stmdb   sp!, {pc}                       ; Restore fault pc
        stmdb   sp!, {r12, lr}                  ; Restore sp, lr
        sub     sp, sp, #4                      ; Unlink past r12
        stmdb   sp!, {r4-r11}                   ; Restore non-volatiles r4-r11
        sub     sp, sp, #(4 * 5)                ; Unlink past flags, r0-r3

    PROLOG_END

        ALTERNATE_ENTRY CaptureContext
        sub     sp, sp, #CtxSizeof              ; (sp) = CONTEXT_RECORD
        stmib   sp, {r0-r15}                    ; store all registers
        mrs     r1, cpsr                        ; (r1) = current status
        mov     r0, #CONTEXT_FULL
        str     r1, [sp,#CtxPsr]
        str     r0, [sp,#CtxFlags]              ; set ContextFlags
        mov     r0, sp                          ; (r0) = Arg0 = ptr to CONTEXT_RECORD
        CALL    ExceptionDispatch
        mov     r1, sp                          ; (r1) = ptr to CONTEXT_RECORD
        ldr     r2, [r1,#CtxPsr]                ; (r0) = new Psr
        and     r0, r2, #0x1f                   ; (r2) = destination mode
        cmp     r0, #USER_MODE
        bne     %F9                             ; not returning to user mode
; Returning to user mode.
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to Supervisor Mode w/IRQs disabled
        msr     spsr, r2
        ldr     lr, [r1,#CtxPc]                 ; (lr) = address to continue at
        ldmib   r1, {r0-r14}^                   ; restore user mode registers
        nop                                     ; hazard from user mode bank switch
        movs    pc, lr                          ; return to user mode.

; Returning to a privileged mode.
;
;       (r1) = ptr to CONTEXT_RECORD
;       (r2) = new PSR

9
  IF Interworking :LOR: Thumbing

        tst     r2, #THUMB_STATE                ; returning to Thumb code ?
        beq     ARMDispatch                     ; if not then dispatch to ARM code

;
;   Switch to Thumb mode before continuing.
;   "push" r0 and PC onto the continuation stack, update the context, use
;   r0 to switch to Thumb, and then pop r0 & the continuation PC
;

ThumbDispatch

        bic     r2, r2, #THUMB_STATE            ; else update mode
        msr     cpsr, r2

        ldr     r2, [r1, #CtxR0]                ; load saved R0 value
        ldr     r3, [r1, #CtxPc]                ; and continuation PC
        ldr     r0, [r1, #CtxSp]                ; get the continuation SP
        stmdb   r0!, {r2, r3}                   ; "push" new R0 & PC values
        str     r0, [r1, #CtxSp]                ; and update new stack pointer

        add     r1, r1, #CtxR1                  ; get pointer to R1 context
        ldmia   r1, {r1-r14}                    ; load context except for R0 & PC

        orr     r0, pc, #0x01
        bx      r0                              ; and switch to Thumb code

  IF {FALSE}                                    ; CODE16 causes THUMB object type

        CODE16
        pop     {r0, pc}                        ; pop R0 and continue
        nop
        CODE32

  ENDIF

ThumbSwitch

        DCW     0xBD01                          ; pop {r0, pc}
        DCW     0x46C0                          ; nop

ARMDispatch

  ENDIF

        msr     cpsr, r2                        ; restore Psr & switch modes
        ldmib   r1, {r0-r15}                    ; restore all registers & return

        ENTRY_END xxCaptureContext


        LEAF_ENTRY RtlCaptureContext

        str     r0, [r0, #CtxR0]                ; save r0
        add     r0, r0, #CtxR1                  ; r0 = &pCtx->r1
        stmia   r0!, {r1-r15}                   ; save r1-r15, (r0) == &pCtx->Psr afterward
        mrs     r1, cpsr                        ; (r1) = current status
        str     r1, [r0]                        ; save PSR

        RETURN

        ENTRY_END RtlCaptureContext

;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExecuteHandlerForException (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN ULONG EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
;    IN PEXCEPTION_ROUTINE ExceptionRoutine,
;    IN OUT PCALLSTACK pcstk,
;    IN ULONG ExceptionMode);
;
; Routine Description:
;    This function allocates a call frame, stores the establisher frame
;    pointer in the frame, establishes an exception handler, and then calls
;    the specified exception handler as an exception handler. If a nested
;    exception occurs, then the exception handler of this function is called
;    and the establisher frame pointer is returned to the exception dispatcher
;    via the dispatcher context parameter. If control is returned to this
;    routine, then the frame is deallocated and the disposition status is
;    returned to the exception dispatcher.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of the exception handler that is to be called.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
;    ExceptionRoutine ((sp)) - supplies a pointer to the exception handler
;       that is to be called.
;
;    pcstk (4(sp)) - callstack for user-mode handler
;
;    ExceptionMode (8(sp)) - PSR value for running ExceptionRoutine
;
; Return Value:
;    The disposition value returned by the specified exception handler is
;    returned as the function value.
;--
;-------------------------------------------------------------------------------
        EXCEPTION_HANDLER RtlpExceptionHandler

        NESTED_ENTRY RtlpExecuteHandlerForException

        mov     r12, sp
        stmfd   sp!, {r3}                       ; save ptr to DispatcherContext
                                                ;       for RtlpExceptionHandler
        stmfd   sp!, {r11,r12,lr}
        sub     r11, r12, #4                    ; (r11) = frame pointer

        PROLOG_END

        ldr     r12, [r11, #12]                 ; (r12) = handler's mode
        add     lr, pc, #reheRet-(.+8)          ; (lr) = return address
        tst     r12, #0xF                       ; user mode??

  IF Interworking :LOR: Thumbing
        ldrne   r12, [r11, #4]                  ; invoke handler in system mode
        bxne    r12
  ELSE
        ldrne   pc, [r11, #4]                   ; invoke handler in system mode
  ENDIF
        ;
        ; calling handler in user mode
        ;

        ldr     r12, [r11, #8]                  ; (r12) = pcstk
        str     lr, [r12, #CstkRa]              ; pcstk->retAddr = [lr]

        ; link pcstk into pCurThread's callstack
        ldr     r3, =KData
        ldr     r3, [r3, #pCurThd]              ; (r3) = pCurThread
        str     r12, [r3, #ThPcstkTop]          ; pCurThread->pcstkTop = pcstk

        ldr     r3, [r11]                       ; restore r3

        ldr     lr, =SYSCALL_RETURN             ; setup return from trap
        ldr     r12, [r11, #4]                  ; (r12) = address to continue at

        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to SVC Mode w/IRQs disabled

        tst     r12, #0x01                      ; set continuation Psr based on
        msreq   spsr_c, #USER_MODE              ; Thumb or ARM continuation
        msrne   spsr_c, #USER_MODE | THUMB_STATE

        movs    pc, r12                         ; switch to user mode & jump to handler
        nop                                     ; NOP required to terminate unwinding

reheRet

 IF Interworking :LOR: Thumbing
        ldmdb   r11, {r11, sp, lr}
        bx      lr
  ELSE
        ldmdb   r11, {r11, sp, pc}
  ENDIF

        ENTRY_END RtlpExecuteHandlerForException



;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExceptionHandler (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN ULONG EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PDISPATCHER_CONTEXT DispatcherContext
;    )
;
; Routine Description:
;    This function is called when a nested exception occurs. Its function
;    is to retrieve the establisher frame pointer from its establisher's
;    call frame, store this information in the dispatcher context record,
;    and return a disposition value of nested exception.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of this exception handler.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
; Return Value:
;    A disposition value ExceptionNestedException is returned if an unwind
;    is not in progress. Otherwise a value of ExceptionContinueSearch is
;    returned.
;--
;-------------------------------------------------------------------------------
        LEAF_ENTRY RtlpExceptionHandler

        ldr     r12, [r0, #ErExceptionFlags]    ; (r12) = exception flags
        tst     r12, #EXCEPTION_UNWIND          ; unwind in progress?
        movne   r0, #ExceptionContinueSearch    ;  Y: set dispostion value

        RETURN_NE                               ;  Y: & return

        ldr     r12, [r1, #-4]                  ; (r12) = ptr to DispatcherContext
        ldr     r12, [r12, #DcEstablisherFrame] ; (r12) = establisher frame pointer
        str     r12, [r3, #DcEstablisherFrame]  ; copy to current dispatcher ctx
        mov     r0, #ExceptionNestedException   ; set dispostion value

        RETURN                                  ; return to caller

        ENTRY_END RtlpExeptionHandler



;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExecuteHandlerForUnwind (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN PVOID EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PVOID DispatcherContext,
;    IN PEXCEPTION_ROUTINE ExceptionRoutine,
;    IN OUT PCALLSTACK pcstk
;    )
;
; Routine Description:
;    This function allocates a call frame, stores the establisher frame
;    pointer and the context record address in the frame, establishes an
;    exception handler, and then calls the specified exception handler as
;    an unwind handler. If a collided unwind occurs, then the exception
;    handler of of this function is called and the establisher frame pointer
;    and context record address are returned to the unwind dispatcher via
;    the dispatcher context parameter. If control is returned to this routine,
;    then the frame is deallocated and the disposition status is returned to
;    the unwind dispatcher.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of the exception handler that is to be called.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
;    ExceptionRoutine ((sp)) - supplies a pointer to the exception handler
;       that is to be called.
;
;    pcstk (4(sp)) - callstack for user-mode handler
;
; Return Value:
;    The disposition value returned by the specified exception handler is
;    returned as the function value.
;--
;-------------------------------------------------------------------------------
        EXCEPTION_HANDLER RtlpUnwindHandler

        NESTED_ENTRY RtlpExecuteHandlerForUnwind

        mov     r12, sp
        stmfd   sp!, {r3}                       ; save ptr to DispatcherContext
        stmfd   sp!, {r11,r12,lr}               ;       for RtlpUnwindHandler
        sub     r11, r12, #4                    ; (r11) = frame pointer

        PROLOG_END

        ldr     r12, [r2, #CtxPsr]              ; (r12) = handler's mode
        add     lr, pc, #rehuRet-(.+8)          ; (lr) = return address
        tst     r12, #0xF                       ; user mode??
 IF Interworking :LOR: Thumbing
        ldrne   r12, [r11, #4]
        bxne    r12                             ; invoke Thumb mode handler
  ELSE
        ldrne   pc, [r11, #4]                   ; invoke handler in system mode
  ENDIF
        ;
        ; calling handler in user mode
        ;
        ldr     r12, [r11, #8]                  ; (r12) = pcstk
        str     lr, [r12, #CstkRa]              ; pcstk->retAddr = [lr]

        ; link pcstk into pCurThread's callstack
        ldr     r3, =KData
        ldr     r3, [r3, #pCurThd]              ; (r3) = pCurThread
        str     r12, [r3, #ThPcstkTop]          ; pCurThread->pcstkTop = pcstk

        ldr     r3, [r11]                       ; restore r3

        ldr     lr, =SYSCALL_RETURN             ; setup return from trap
        ldr     r12, [r11, #4]                  ; (r12) = address to continue at

        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to SVC Mode w/IRQs disabled

        tst     r12, #0x01                      ; set continuation Psr based on
        msreq   spsr_c, #USER_MODE              ; Thumb or ARM continuation
        msrne   spsr_c, #USER_MODE | THUMB_STATE

        movs    pc, r12                         ; switch to user mode & jump to handler

rehuRet

 IF Interworking :LOR: Thumbing
        ldmdb   r11, {r11, sp, lr}
        bx      lr
  ELSE
        ldmdb   r11, {r11, sp, pc}
  ENDIF

        ENTRY_END RtlpExecuteHandlerForUnwind



;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpUnwindHandler (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN PVOID EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PVOID DispatcherContext
;    )
;
; Routine Description:
;    This function is called when a collided unwind occurs. Its function
;    is to retrieve the establisher dispatcher context, copy it to the
;    current dispatcher context, and return a disposition value of nested
;    unwind.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of this exception handler.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
; Return Value:
;    A disposition value ExceptionCollidedUnwind is returned if an unwind is
;    in progress. Otherwise, a value of ExceptionContinueSearch is returned.
;--
;-------------------------------------------------------------------------------
        LEAF_ENTRY RtlpUnwindHandler

        ldr     r12, [r0, #ErExceptionFlags]    ; (r12) = exception flags
        tst     r12, #EXCEPTION_UNWIND          ; unwind in progress?
        moveq   r0, #ExceptionContinueSearch    ;  N: set dispostion value
        RETURN_EQ                               ;  N: & return

        ldr     r12, [r1, #-4]                  ; (r12) = ptr to DispatcherContext
        ldmia   r12, {r0-r2,r12}                ; copy establisher frames disp. ctx
        stmia   r3, {r0-r2,r12}                 ; to the current dispatcher context
        mov     r0, #ExceptionCollidedUnwind    ; set dispostion value

        RETURN                                  ; return to caller

        ENTRY_END RtlpUnwindHandler

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY NKIsSysIntrValid

        subs    r0, r0, #SYSINTR_DEVICES        ; (r0) = idInt - SYSINTR_DEVICES
        blt     NKIRetFalse                     ; return FALSE if (r0) < 0
        cmp     r0, #SYSINTR_MAX_DEVICES        ; (r0) < SYSINTR_MAX_DEVICES?
        
        ldrlt   r1, =KData                      ; (r1) = &KData
        addlt   r1, r1, #alpeIntrEvents         ; (r1) = &KData.alpeIntrEvents
        ldrlt   r0, [r1, r0, LSL #2]            ; (r0) = KData.alpeIntrEvents[idInt - SYSINTR_DEVICES]

        RETURN_LT

NKIRetFalse
        mov     r0, #0
        RETURN
        ENTRY_END NKIsSysIntrValid




;-------------------------------------------------------------------------------
; This code is copied to the kernel's data page so that it is accessible from
; the kernel & user code. The kernel checks if it is interrupting an interlocked
; api by range checking the PC to be between UserKPage+0x380 & UserKPage+0x400.
;-------------------------------------------------------------------------------

        LEAF_ENTRY InterlockedAPIs
ILPopList
        ldr     r12, [r0]                       ; (r12) = ptr to first item in list
        cmp     r12, #0
        ldrne   r1, [r12]                       ; (r1) = new head of list
        strne   r1, [r0]                        ; update list head
        mov     r0, r12                         ; return ptr to first item
  IF Interworking :LOR: Thumbing
        bx      lr
  ELSE
        mov     pc, lr
  ENDIF

ILPushList
        ldr     r12, [r0]                       ; (r12) = old list head
        str     r12, [r1]                       ; store linkage
        str     r1, [r0]                        ; store new list head
        mov     r0, r12                         ; return old list head
  IF Interworking :LOR: Thumbing
        bx      lr
  ELSE
        mov     pc, lr
  ENDIF

ILCmpEx
        ldr     r12, [r0]
        cmp     r12, r2
        streq   r1, [r0]
        mov     r0, r12                         ; (r0) = return original value
  IF Interworking :LOR: Thumbing
        bx      lr
  ELSE
        mov     pc, lr
  ENDIF

ILExAdd
        ldr     r12, [r0]
        add     r2, r12, r1
        str     r2, [r0]
        mov     r0, r12                         ; (r0) = return original value
  IF Interworking :LOR: Thumbing
        bx      lr
  ELSE
        mov     pc, lr
  ENDIF

ILEx
        ldr     r12, [r0]
        str     r1, [r0]
        mov     r0, r12                         ; (r0) = return original value
  IF Interworking :LOR: Thumbing
        bx      lr
  ELSE
        mov     pc, lr
  ENDIF
        END_REGION InterlockedEnd

; NullSection - empty memory section for unallocated 32mb regions
        EXPORT  NullSection[DATA]
NullSection
        OPT     2                               ; disable listing
        %       512*4                           ; 512 Words of zeros
        OPT     1                               ; reenable listing

        END

