;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of the Microsoft shared
; source or premium shared source license agreement under which you licensed
; this source code. If you did not accept the terms of the license agreement,
; you are not authorized to use this source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the SOURCE.RTF on your install media or the root of your tools installation.
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
        INCLUDE armmacros.s
        OPT     1       ; reenable listing

        INCLUDE armhigh.inc


        ;-----------------------------------------------------------------------
        ; .KDATA area is used to reserve physical memory for the above structures.
        AREA |.KDATA|,DATA,NOINIT
        
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


        ;-----------------------------------------------------------------------
        ; Code (Read-only TEXT section)
        TEXTAREA

        IMPORT  ARMInit

        MACRO
        mtc15   $cpureg, $cp15reg
        mcr     p15,0,$cpureg,$cp15reg,c0,0
        MEND

        MACRO
        mfc15   $cpureg, $cp15reg
        mrc     p15,0,$cpureg,$cp15reg,c0,0
        MEND

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
        blo     %F102                           ; go to next entry if not

        ldr     r2, [r1, #8]                    ; (r2) = size in MB
        add     r2, r3, r2, LSL #BANK_SHIFT     ; (r2) = OEMAddressTable[i].vaddr + region size
        cmp     r12, r2                         ; Is (VA < OEMAddressTable[i].vaddr + region size)?
        blo     %F103                           ; Found if it's true

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
        blo     %F202                           ; go to next entry if not

        add     r2, r2, r3, LSL #BANK_SHIFT     ; (r2) = OEMAddressTable[i].paddr + region size
        cmp     r12, r2                         ; Is (PA < OEMAddressTable[i].paddr + region size)?
        blo     %F203                           ; Found if it's true

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

        LEAF_ENTRY GetCpuId
        mrc     p15, 0, r0, c0, c0, 0           ; r0 = CP15:r0
        mov     pc, lr
        ENTRY_END

        ;-------------------------------------------------------------------------------
        ; KernelStart - kernel loader entry point
        ;
        ;       The OEM layer will setup any platform or CPU specific configuration that is
        ; required for the kernel to have access to ROM and DRAM and jump here to start up
        ; the system. Any processor specific cache or MMU initialization should be completed.
        ; The MMU and caches should not enabled.
        ;
        ;       This routine will initialize the first-level page table based up the contents of
        ; the OEMAddressTable array and enable the MMU and caches.
        ;
        ; NOTE: Until the MMU is enabled, kernel symbolic addresses are not valid and must be
        ;       translated via the OEMAddressTable array to find the correct physical address.
        ;
        ;       Entry   (r0) = pointer to OEMAddressTable (physical)
        ;
        ; The role of KernelStart is to perform minimal initialzation, setup minimal mappings for MMU,
        ; turn on MMU with everything running uncached, initialize globals of kernel.dll, then jump to
        ; the entry point of kernel.dll to perform the rest of the initialization.
        ;
        ; We'll setups 1st level page table for high area (0xFFF00000, and the cached statically
        ;   mapped area (0x80000000 - 0x9fffffff), both with C=B=0. The uncached area will be setup 
        ;   when we actually get to kernel's entry point, and attributes of "cached statically mapped area"
        ;   will be modified based on OEM settings. Interrupt vectors, PSL mappings will also be
        ;   setup after kernel started (jumped to entry of kernel.dll).
        ;
        ;-------------------------------------------------------------------------------
        LEAF_ENTRY KernelStart

        mov     r11, r0                         ; (r11) = &OEMAddressTable (save pointer)

        ; skip the 1st entry if we're using the new table format
        ldr     r2, [r11]
        ldr     r3, =CE_NEW_MAPPING_TABLE
        cmp     r2, r3
        bne     BootContinue
        add     r11, r11, #12

BootContinue

        ; (r0)  = physical address of &OEMAddressTable[0]
        ; (r11) = physical address of &OEMAddressTable[0] iff old table format
        ;         physical address of &OEMAddressTabel[1] iff new table format
        ;
        ; figure out the virtual address of OEMAddressTable
        mov     r1, r11                         ; (r1) = &OEMAddressTable (2nd argument to VaFromPa)
        bl      VaFromPa
        mov     r6, r0                          ; (r6) = VA of OEMAddressTable

        ; convert base of PTs to Physical address
        ldr     r4, =PTs                        ; (r4) = virtual address of FirstPT
        mov     r0, r4                          ; (r0) = virtual address of FirstPT
        mov     r1, r11                         ; (r1) = &OEMAddressTable (2nd argument to PaFromVa)
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

        ; read the architecture information
        bl      GetCpuId
        mov     r5, r0 LSR #16                  ; r5 >>= 16
        and     r5, r5, #0x0000000f             ; r5 &= 0x0000000f == architecture id
        
;       Setup 2nd level page table to map the high memory area which contains the
; first level page table, 2nd level page tables, kernel data page, etc.
;       (r5) = architecture id

        add     r4, r10, #HighPT-PTs            ; (r4) = ptr to high page table

        cmp     r5, #ARMv6                      ; v6 or later?
; ARMV6_MMU
        orrge   r0, r10, #PTL2_KRW + PTL2_SMALL_PAGE + ARMV6_MMU_PTL2_SMALL_XN
                                                ; (r0) = PTE for 4K, kr/w u-/- page, uncached unbuffered, nonexecutable
; PRE ARMV6_MMU
        orrlt   r0, r10, #PTL2_KRW + (PTL2_KRW << 2) + (PTL2_KRW << 4) + (PTL2_KRW << 6)
                                                ; Need to replicate AP bits into all 4 fields
        orrlt   r0, r0,  #PTL2_SMALL_PAGE + PREARMV6_MMU_PTL2_SMALL_XN
                                                ; (r0) = PTE for 4K, kr/w u-/- page, uncached unbuffered, nonexecutable
        str     r0, [r4, #0xD0*4]               ; store the entry into 4 slots to map 16K of primary page table
        add     r0, r0, #0x1000                 ; step on the physical address
        str     r0, [r4, #0xD1*4]
        add     r0, r0, #0x1000                 ; step on the physical address
        str     r0, [r4, #0xD2*4]
        add     r0, r0, #0x1000                 ; step on the physical address
        str     r0, [r4, #0xD3*4]

        add     r8, r10, #ExceptionVectors-PTs  ; (r8) = ptr to vector page
        orr     r0, r8, #PTL2_SMALL_PAGE        ; construct the PTE (C=B=0)

;; The exception stacks and the vectors are mapped as a single kr/w page.
;; Any alternative will use more physical memory.
;; Multiple mappings don't provide any real protection: if the vectors were in a r/o page,
;; they could still be corrupted via the kr/w setting required for the stacks.
        cmp     r5, #ARMv6                      ; v6 or later?
; ARMV6_MMU 
        orrge   r0, r0, #PTL2_KRW
; PRE ARMV6_MMU
        orrlt   r0, r0, #PTL2_KRW + (PTL2_KRW << 2) + (PTL2_KRW << 4) + (PTL2_KRW << 6)
                                                ; Need to replicate AP bits into all 4 fields for pre-V6 MMU

        str     r0, [r4, #0xF0*4]               ; store entry for exception stacks and vectors
                                                ; other 3 entries now unused

        add     r9, r10, #KPage-PTs             ; (r9) = ptr to kdata page
        orr     r0, r9, #PTL2_SMALL_PAGE        ; (r0)=PTE for 4K (C=B=0)
        
; ARMV6_MMU (condition codes still set)
        orrge   r0, r0, #PTL2_KRW_URO           ; No subpage access control, so we must set this all to kr/w+ur/o
; PRE ARMV6_MMU
        orrlt   r0, r0, #(PTL2_KRW << 0) + (PTL2_KRW << 2) + (PTL2_KRW_URO << 4) + (PTL2_KRW_URO << 6)
                                                ; (r0) = set perms kr/w kr/w kr/w+ur/o r/o
        str     r0, [r4, #0xFC*4]               ; store entry for kernel data page
        orr     r0, r4, #PTL1_2Y_TABLE          ; (r0) = 1st level PTE for high memory section
        add     r1, r10, #0x4000
        str     r0, [r1, #-4]                   ; store PTE in last slot of 1st level table

;       Fill in first level page table entries to create "statically mapped" regions
; from the contents of the OEMAddressTable array.
;
;       (r5) = architecture id
;       (r9) = ptr to KData page
;       (r10) = ptr to 1st level page table
;       (r11) = ptr to OEMAddressTable array

        add     r10, r10, #0x2000               ; (r10) = ptr to 1st PTE for "unmapped space"

        mov     r0, #PTL1_SECTION
        orr     r0, r0, #PTL1_KRW               ; (r0)=PTE for 0: 1MB (C=B=0, kernel r/w)
20      mov     r1, r11                         ; (r1) = ptr to OEMAddressTable array (physical)


25      ldr     r2, [r1], #4                    ; (r2) = virtual address to map Bank at
        ldr     r3, [r1], #4                    ; (r3) = physical address to map from
        ldr     r4, [r1], #4                    ; (r4) = num MB to map

        cmp     r4, #0                          ; End of table?
        beq     %F29

        ldr     r12, =0x1FF00000
        and     r2, r2, r12                      ; VA needs 512MB, 1MB aligned.

        ldr     r12, =0xFFF00000
        and     r3, r3, r12                      ; PA needs 4GB, 1MB aligned.

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
        sub     r10, r10, #0x2000               ; (r10) = restore address of 1st level page table

        ; The minimal page mappings are setup. Initialize the MMU and turn it on.

        ; there are some CPUs with pipeline issues that requires identity mapping before turning on MMU.
        ; We'll create an identity mapping for the address we'll jump to when turning on MMU on and remove
        ; the mapping after we turn on MMU and running on Virtual address.
        

        ldr     r12, =0xFFF00000                ; (r12) = mask for section bits
        and     r1, pc, r12                     ; physical address of where we are 
                                                ; NOTE: we assume that the KernelStart function never spam across 1M boundary.
        orr     r0, r1, #PTL1_SECTION
        orr     r0, r0, #PTL1_KRW               ; (r0) = PTE for 1M for current physical address, C=B=0, kernel r/w
        add     r7, r10, r1, LSR #18            ; (r7) = 1st level PT entry for the identity map
        ldr     r8, [r7]                        ; (r8) = saved content of the 1st-level PT
        str     r0, [r7]                        ; create the identity map

        mov     r1, #1
        mtc15   r1, c3                          ; Setup access to domain 0 and clear other
        mtc15   r10, c2                         ; setup translation base (physical of 1st level PT)

        mov     r0, #0
        mcr     p15, 0, r0, c8, c7, 0           ; Flush the I&D TLBs

        mfc15   r1, c1
        orr     r1, r1, #0x007F                 ; changed to read-mod-write for ARM920 Enable: MMU, Align, DCache, WriteBuffer

        ldr     r0, VirtualStart                ; (r0) = Virtual address to start after enabling MMU

        cmp     r5, #ARMv6                      ; check CPU architecture
        blt     %F30
        
        ; ARMV6 or later
        cmp     r0, #0                          ; make sure no stall on "mov pc,r0" below
        mov     r2, #0
        mcr     p15, 0, r2, c7, c10, 4          ; DSB (guarantee the I&D TLB flush complete before we enable MMU)
        orr     r1, r1, #0x3000                 ; vector adjust, ICache
        orr     r1, r1, #1<<23                  ; V6-format page tables
        orr     r1, r1, #ARMV6_U_BIT            ; V6-set U bit, let A bit control unalignment support
        
        mtc15   r1, c1                          ; enable the MMU & Caches
        mcr     p15, 0, r2, c7, c5, 4           ; flush prefetch buffer (ISB)
        mcr     p15, 0, r2, c7, c5, 6           ; flush BTAC
        mcr     p15, 0, r2, c7, c10, 4          ; DSB (guarantee completion of cache enable)
        mcr     p15, 0, r2, c7, c5, 4           ; flush prefetch buffer (ISB), guarantee BTAC flush visible to next instruction
        mov     pc, r0                          ;  & jump to new virtual address
        nop

30
        ; PRE ARMV6
        cmp     r0, #0                          ; make sure no stall on "mov pc,r0" below
        orr     r1, r1, #0x3200                 ; vector adjust, ICache, ROM protection
        mtc15   r1, c1                          ; enable the MMU & Caches
        mov     pc, r0                          ;  & jump to new virtual address
        nop

; MMU & caches now enabled.
;
;       (r10) = physcial address of 1st level page table
;       (r7)  = entry in 1st level PT for identity map
;       (r8)  = saved 1st level PT save at (r7)
VStart  ldr     r2, =FirstPT                    ; (r2) = VA of 1st level PT
        sub     r7, r7, r10                     ; (r7) = offset into 1st-level PT
        str     r8, [r2, r7]                    ; restore the temporary identity map
        mcr     p15, 0, r0, c8, c7, 0           ; Flush the I&D TLBs

;
; setup stack for each modes: current mode = supervisor mode
;
        ldr     sp, =KStack
        add     r4, sp, #KData-KStack           ; (r4) = ptr to KDataStruct

        ; setup ABORT stack
        mov     r1, #ABORT_MODE:OR:0xC0
        msr     cpsr_c, r1                      ; switch to Abort Mode w/IRQs disabled
        add     sp, r4, #AbortStack-KData

        ; setup IRQ stack
        mov     r2, #IRQ_MODE:OR:0xC0
        msr     cpsr_c, r2                      ; switch to IRQ Mode w/IRQs disabled
        add     sp, r4, #IntStack-KData

        ; setup FIQ stack
        mov     r3, #FIQ_MODE:OR:0xC0
        msr     cpsr_c, r3                      ; switch to FIQ Mode w/IRQs disabled
        add     sp, r4, #FIQStack-KData

        ; setup UNDEF stack
        mov     r3,  #UNDEF_MODE:OR:0xC0
        msr     cpsr_c, r3                      ; switch to Undefined Mode w/IRQs disabled
        mov     sp, r4                          ; (sp_undef) = &KData

        ; switch back to Supervisor mode
        mov     r0, #SVC_MODE:OR:0xC0
        msr     cpsr_c, r0                      ; switch to Supervisor Mode w/IRQs disabled
        ldr     sp, =KStack

        ; continue initialization in C
        add     r0, sp, #KData-KStack           ; (r0) = ptr to KDataStruct
        str     r6, [r0, #pAddrMap]             ; store VA of OEMAddressTable in KData
        bl      ARMInit                         ; call C function to perform the rest of initializations
        ; upon return, (r0) = entry point of kernel.dll

        mov     r12, r0
        ldr     r0, =KData
        mov     pc, r12                          ; jump to entry of kernel.dll

VirtualStart DCD VStart

        ENTRY_END KernelStart

        END

