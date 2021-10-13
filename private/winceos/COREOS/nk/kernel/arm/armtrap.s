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
; Environment:
;
;    Kernel mode only.
;
;--
;-------------------------------------------------------------------------------
        OPT     2       ; disable listing
        INCLUDE ksarm.h
        OPT     1       ; reenable listing

        INCLUDE armhigh.inc

; must match the value in kernel.h
PERFORMCALLBACK         EQU     -30     ; (iMethod of PerformCallBack)
SECURESTK_RESERVE       EQU     120     ; (SIZE_PRETLS + SIZE_RUNTIMESTORAGE + 16)

MAX_PSL_ARG             EQU     56      ; 14 args max

; Resister save frame for Code Aborts, Data Aborts and IRQs.
FrameR0                 EQU     0
FrameR1                 EQU     4
FrameR2                 EQU     8
FrameR3                 EQU     12
FrameR12                EQU     16
FrameLR                 EQU     20

        MACRO
        mtc15   $cpureg, $cp15reg
        mcr     p15,0,$cpureg,$cp15reg,c0,0
        MEND

        MACRO
        mfc15   $cpureg, $cp15reg
        mrc     p15,0,$cpureg,$cp15reg,c0,0
        MEND

        MACRO
        GetPCB  $rslt
        mrc     p15, 0, $rslt, c13, c0, 3           ; ($rslt) = ppcb if #Cpus > 1
        MEND


        ;-----------------------------------------------------------------------
        ; Globals
        AREA |.data|, DATA

idCpu   DCD     1
        
        ;-----------------------------------------------------------------------
        ; Code (Read-only TEXT section)
        TEXTAREA
        
        IMPORT  ObjectCall
        IMPORT  ServerCallReturn
        IMPORT  NKPrepareCallback
        IMPORT  LoadPageTable
        IMPORT  HandleException
        IMPORT  ExceptionDispatch
        IMPORT  OEMInterruptHandler
        IMPORT  OEMInterruptHandlerFIQ
        IMPORT  OEMNotifyIntrOccurs
        IMPORT  SetupCallToUserServer
        IMPORT  KernelInit
        IMPORT  g_pfnUsrRtlDispExcp
        IMPORT  g_pfnKrnRtlDispExcp
        IMPORT  g_fStartMP
        IMPORT  MpStartContinue
        IMPORT  HandleIpi
        IMPORT  CELOG_Interrupt
        IMPORT  g_dwIpiCommand
        IMPORT  g_dwServerCallReturnAddr

        EXPORT  idCpu
        EXPORT  InterlockedEnd

        LEAF_ENTRY GetCpuId
        mrc     p15, 0, r0, c0, c0, 0           ; r0 = CP15:r0
        bx      lr
        LEAF_END

;-------------------------------------------------------------------------------
; KernelStart - called after NKStartup finished setting up to startup kernel,
;-------------------------------------------------------------------------------
        LEAF_ENTRY KernelStart

        ;
        ; callstack unwinding need the address of ServerCallReturn to be able to unwind a suspended thread
        ; 
        ldr     r0, =SvrRtn                     ; (r0) = address right after ServerCallReturn
        ldr     r1, =g_dwServerCallReturnAddr   ; (r1) = address of dwServerCallReturnAddr
        str     r0, [r1]                        ; save the address 
        
        ldr     r4, =KData                      ; (r4) = ptr to KDataStruct
        ldr     r0, =APIRet
        str     r0, [r4, #pAPIReturn]           ; set API return address

        ; update "thread id" register to point to our ppcb
        mcr     p15, 0, r4, c13, c0, 3          ; ppcb of master is KData

        ; always enabel unaligned access
        mfc15   r2, c1                          ; (r1) = current Control Register configuration data
        bic     r2, r2, #ARMV6_A_BIT            ; (enable) => clear A bit
        mtc15   r2, c1                          ; Write Control Register configuration data with new settings

        mov     r1, #0
        isb                                     ; flush prefetch buffer (ISB)
        cpsid   i, #SVC_MODE                    ; switch to Supervisor Mode w/IRQs disabled

        bl      KernelInit                      ; initialize scheduler, etc.

        ; enable interrutps
        cpsie   if

        movs    r0, #0                          ; no current thread
        movs    r1, #ID_RESCHEDULE
        b       Reschedule

        LEAF_END

;-------------------------------------------------------------------------------
; NKMpStart - kernel entry point for MP startup,
;-------------------------------------------------------------------------------
        LEAF_ENTRY NKMpStart

        ; switch to supervisor mode with interrupts disabled
        cpsid   if, #SVC_MODE                   ; switch to Supervisor Mode w/IRQs disabled

        ; spin till g_fStartMP is set
        ldr     r2, =g_fStartMP
Spin
        ldr     r1, [r2]
        cmp     r1, #0
        beq     Spin

        cmp     r1, #1
        bne     GotCpuId

        ;
        ; calculate (r1) = InterlockedIncrement (&idCpu)
        ;
        ldr     r2, =idCpu
Retry
        ldrex   r1, [r2]
        add     r1, r1, #1
        strex   r3, r1, [r2]
        cmp     r3, #0
        bne     Retry

GotCpuId
        ; (r1) = our CPU id
        
        sub     r0, r1, #2
        ldr     r2, =MP_PCB_BASE
        add     r2, r2, r0, LSL #13

        ; (r2) = pApPage 
        add     r0, r2, #0x1800                 ; (r0) = ppcb

        ; setup stacks for each mode, in SVC mode at this point
        ;   (r0) = ppcb 
        ;   (r2) = pApPage
        
        ; SVC stack
        mov     sp, r0
        sub     sp, sp, #REGISTER_SAVE_AREA

        ; UNDEF stack
        cpsid   if, #UNDEF_MODE                 ; switch to UNDEF_MODE Mode w/IRQs disabled
        mov     sp, r0                          ; NOTE: UNDEF stack overlapped with supervisor stack

        ; FIQ stack
        cpsid   if, #FIQ_MODE                   ; switch to FIQ Mode w/IRQs disabled
        add     r2, r2, #FIQ_STACK_SIZE
        mov     sp, r2

        ; IRQ stack
        cpsid   if, #IRQ_MODE                   ; switch to IRQ_MODE Mode w/IRQs disabled
        add     r2, r2, #IRQ_STACK_SIZE
        mov     sp, r2

        ; ABORT stack
        cpsid   if, #ABORT_MODE                 ; switch to ABORT_MODE Mode w/IRQs disabled
        add     r2, r2, #ABORT_STACK_SIZE
        mov     sp, r2


        ; done setting up stacks, back to supervisor mode
        cpsid   if, #SVC_MODE                   ; switch to Supervisor Mode w/IRQs disabled

        ; update "thread id" register to point to our ppcb
        mcr     p15, 0, r0, c13, c0, 3

        ; always enabel unaligned access
        mfc15   r2, c1                          ; (r1) = current Control Register configuration data
        bic     r2, r2, #ARMV6_A_BIT            ; (enable) => clear A bit
        mtc15   r2, c1                          ; Write Control Register configuration data with new settings

        ; stack setup, continue initialization in C
        ; (r0) = ppcb
        bl      MpStartContinue

        ; enable interrutps
        cpsie   if

        movs    r0, #0                          ; no current thread
        movs    r1, #ID_RESCHEDULE
        b       Reschedule
        LEAF_END

;-------------------------------------------------------------------------------
; SetPCBRegister - update per-CPU ppcb register (Thread id register)
;   (r0) = ppcb
;-------------------------------------------------------------------------------
        LEAF_ENTRY SetPCBRegister
        mcr     p15, 0, r0, c13, c0, 3
        bx      lr
        LEAF_END

        LEAF_ENTRY ARMFlushTLB
        movs    r0, #0
        mcr     p15, 0, r0, c8, c7, 0           ; Flush the I&D TLBs
        bx      lr
        LEAF_END

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY    UndefException
        sub     lr, lr, #2                      ; default to THUMB mode
        stmdb   sp, {r0-r3, lr}                 ; NOTE: DO NOT USE "push", as SP IS NOT SUPPOSED TO CHANGE
        
        movs    r1, #ID_UNDEF_INSTR
        mrs     r0, spsr
        tst     r0, #THUMB_STATE                ; Entered from ARM Mode ?
        bne     CommonHandler

        ; was in ARM mode
        sub     lr, lr, #2                      ; 
        str     lr, [sp, #-4]                   ; update lr on stack
        b       CommonHandler
        LEAF_END UndefException

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY SWIHandler
        movs    pc, lr
        LEAF_END SWIHandler


;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY FIQHandler
        PROLOG_NOP      sub lr, lr, #4              ; fix return address
        PROLOG_PUSH     {r0-r3, r12, lr}

        bl  OEMInterruptHandlerFIQ

        EPILOG_POP      {r0-r3, r12, lr}            ; restore regs & return for NOP
        movs            pc, lr
        NESTED_END FIQHandler

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------

        NESTED_ENTRY PrefetchAbort
        
        PROLOG_NOP  sub lr, lr, #4                  ; repair continuation address
        PROLOG_PUSH {r0-r3, r12, lr}                ; save r0 - r3, r12, lr, on ABORT stack
                                                    ; NOTE: MUST RETRIEVE INFO BEFORE TURNING INT ON
                                                    ;       IF IT'S AN API Call
                                                    
        dsb                                         ; errata 775420
        GetPCB  r12                                 ; (r12) = ppcb
        ldr     r3, [r12, #dwPslTrapSeed]           ; (r3)  = ppcb->dwPslTrapSeed
        eor     r3, r3, lr                          ; (r3)  = API encoding
        
        sub     r2, r3, #0xF1000000
        cmp     r2, #MAX_METHOD_NUM-0xF1000000  ; 0x20400 is the max value of API encoding
        blo     APICall

        ; Process a prefetch abort.

        mov     r0, lr                          ; (r0) = faulting address
        bl      LoadPageTable                   ; (r0) = !0 if entry loaded

        cbz     r0, %F10            ; go to pagefault handler if failed

        ; succeeded
        EPILOG_POP  {r0-r3, r12, lr}            ; restore regs & continue
        movs    pc, lr

10
        ; LoadPageTable failed, go to common handler
        GetPCB  lr                              ; (lr) = ppcb
        sub     lr, lr, #4                      ; (lr) = &((char *)ppcb)[-4]
        pop     {r0-r3, r12}                    ; restore r0-r3, r12
        stmdb   lr, {r0-r3}                     ; save r0-r3 on register save area
        pop     {r0}                            ; (r0) = resume address
        str     r0, [lr]                        ; save resume address
        movs    r1, #ID_PREFETCH_ABORT          ; (r1) = exception ID
        b       CommonHandler

        NESTED_END  PrefetchAbort

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY    DataAbortHandler
        PROLOG_NOP      sub lr, lr, #8                      ; repair continuation address
        PROLOG_PUSH     {r0-r3, r12, lr}

        dsb                                     ; errata 775420
        mfc15   r0, c6                          ; (r0) = FAR
        mfc15   r1, c5                          ; (r1) = FSR

;; V6 architectures introduce a new bit, and a new reason code.
;; This code will also work with all previous architectures, so should be
;; used for all builds (so an otherwise pre-V6 image will run properly on
;; a V6 system).
;; Two comments:
;; - bit 10 of the FSR is part of the reason code in V6, so the code would ideally
;;   check that it is zero. BUT the value of bit 10 in older architectures is defined
;;   as UNPREDICTABLE, so it is possible that some implementations will set this. We will
;;   call LoadPageTable for 3 possible new reason codes: none of these are defined in the
;;   V6 architecture specification (but could conceivably be used in implementation-specific
;;   extensions). It is preferable to call LoadPageTable and then generate an abort on extra
;;   (and very unlikely) reason codes, rather than not call LoadPageTable on old implementations
;;   that spuriously set bit 10.
;; - Because the memory is paged in with 1st-level page table entries, any page translation aborts
;;   will never cause memory to be mapped in, so only two reason codes actually need to be checked
;;   below.

        and     r3, r1, #0xF                    ; type of data abort
        cmp     r3, #0x4                        ; Instruction cache maintenance fault?
        cmpne   r3, #0x5                        ; Section translation error?
        cmpne   r3, #0x7                        ; Page translation error?

;; Reach here with EQ set if we should try to page in the FAR address
;; Reach here with NE set if we should just signal a fault.

        bne     %F10
        
        bl      LoadPageTable                   ; (r0) = !0 if entry loaded

        cbz     r0, %F10
        
        ; LoadPageTable succeed, restore registers and continue
        EPILOG_POP  {r0-r3, r12, lr}
        movs    pc, lr

10
        ; LoadPageTable failed, go to common handler
        GetPCB  lr                              ; (lr) = ppcb
        sub     lr, lr, #4                      ; (lr) = &((char *)ppcb)[-4]
        pop     {r0-r3, r12}                    ; restore r0-r3, r12
        stmdb   lr, {r0-r3}                     ; save r0-r3 on kstack
        pop     {r0}                            ; (r0) = resume address
        str     r0, [lr]                        ; save resume address
        mov     r1, #ID_DATA_ABORT              ; (r1) = exception ID
        b       CommonHandler

        NESTED_END DataAbortHandler

;-------------------------------------------------------------------------------
;
; NKPerformCallBack - calling back to user process
;
;-------------------------------------------------------------------------------
        NESTED_ENTRY NKPerformCallBack
        ;
        ; fake prolog, never executed
        ; the only purpose for this is to provide unwinding information in case we
        ; faulted/broke inside NKPrepareCallback. The only register that matters
        ; is the return address.
        ;
        PROLOG_PUSH         {r0-r3}
        ; we need to save r11, but don't want to side-effect of using it as a frame pointer.
        ; so we use PROLOG_NOP to save it.
        PROLOG_NOP          push {r11}
        PROLOG_PUSH         {r4-r10}                    ;
        PROLOG_STACK_ALLOC  cstk_regs-cstk_dwPrevSP
        PROLOG_PUSH         {lr}                        ; save return address
        PROLOG_STACK_ALLOC  cstk_retAddr                ; allocate top part of the CALLSTACK strcutre

        ;
        ; NOTE: the above instructions decrement sp by (cstk_sizeof)
        ;

        ; (sp) = pcstk
        add     r12, sp, #cstk_sizeof                   ; (r12) = original sp
        str     r12, [sp, #cstk_dwPrevSP]               ; save original sp
        
        mov     r0, sp
        bl      NKPrepareCallback

        ; (r0) = function to call
        ldr     r2, [sp, #cstk_dwNewSP]                 ; (r2) = pcstk->dwNewSP

        cbz     r2, KModeCallback                       ; (r2) == 0 if kernel mode

        ; callback to user mode
        mov     r6, r2                                  ; (r6) = new SP
        mov     r12, r0                                 ; (r12) = function to call
        b       CallUModeFunc                           ; jump to common code to call a u-mode funciton

KModeCallback
        ; kernel-mode, call direct
        mov     r12, r0                                 ; (r12) = function to call

        ldr     lr, [sp, #cstk_retAddr]                 ; restore lr
        add     sp, sp, #cstk_sizeof-16                 ; get rid of cstk struture, less r0-r3 save area
        pop     {r0-r3}                                 ; restore r0-r3
        bx      r12                                     ; jump to the function

        NESTED_END NKPerformCallBack




        NESTED_ENTRY    APICallEH
        ;-------------------------------------------------------------------------------
        ;++
        ; The following code is never executed. Its purpose is to support unwinding
        ; through the calls to API dispatch and return routines.
        ;--
        ;-------------------------------------------------------------------------------
        PROLOG_PUSH         {r0-r3}
        PROLOG_PUSH         {r4-r11}
        PROLOG_STACK_ALLOC  cstk_regs-cstk_dwPrevSP
        PROLOG_PUSH         {lr}
        PROLOG_STACK_ALLOC  cstk_retAddr

        ALTERNATE_ENTRY APICall

        ; API call handling
        ;
        ; in ABORT MODE, interrupts disabled
        ; origianl {r0-13, r12, lr} save on ABORT stack
        ; (r12) = ppcb
        ; (r2) = API encoding - 0xF1000000
        sub     r3, r2, #FIRST_METHOD-0xF1000000 ; (r3) = API encoding - FIRST_METHOD
        mov     r3, r3, ASR #2                  ; (r3) = iMethod
        ldr     r2, [r12, #pCurThd]             ; (r2) = pCurThread
        ldr     r1, [r2, #ThPcstkTop]           ; (r1) = pCurThread->pcstkTop

        cbz     r1, NotTrapRtn                  ; not trap return if pCurThread->pcstkTop = NULL

        cmp     r3, #-1
        bne     NotTrapRtn

TrapRtn
        ; STILL IN ABORT MODE, original r0-r3, r12, and lr on abort stack, INT disabled
        ;
        ; returning from callback/user-mode server
        ;
        ;   (r0) = return value
        ;   (r1) = pCurThread->pcstkTop
        ;   (r2) = pCurThread
        ;   (r12) = ppcb
        ;

        ; discard everything on stack_abt
        add     sp, sp, #24                     ; 24 == sizeof (r0-r3, r12, lr)

        ; update TLS
        ldr     r3, [r2, #ThTlsSecure]          ; pCurThread->tlsPtr
        str     r3, [r2, #ThTlsPtr]             ;       = pCurThread->tlsSecure
        str     r3, [r12, #lpvTls]              ; ppcb->lpvTls = pCurThread->tlsSecure

        ; switch to system mode, interrupts disabled
        cpsid   i, #SYSTEM_MODE                 ; switch to System Mode, interrupts disabled
        
        mov     sp, r1                          ; SP set to secure stack

        cpsie   i                               ; switch to System Mode, and enable interrupts

        ; jump to common code
        ; (r0) = return value
        ; (sp) = pcstk
        mov     r4, sp
        b       APIRet

NotTrapRtn
        ; STILL IN ABORT MODE, original r0-r3, r12, and lr on abort stack, INT disabled
        ; (r3) = iMethod
        ; (r2) = pCurThread
        ; (r1) = pCurThread->pcstkTop

        ; figure out which mode we're calling from, switch stack if necessary
        mrs     r12, spsr                       ; (r12) = spsr
        and     r12, r12, #MODE_MASK            ; (r12) = caller's mode
        cmp     r12, #USER_MODE
        ldr     r0, [r2, #ThTlsSecure]          ; (r0)  = pCurThread->tlsSecure

        bne     KPSLCall

        ; caller was in user mode, switch stack
        str     r0, [r2, #ThTlsPtr]             ; pCurThread->tlsPtr = pCurTheread->tlsSecure
        
        cmp     r1, #0                          ; first trip into kernel? (pcstkTop == NULL)?

        mov     r2, #CST_MODE_FROM_USER         ; (r2) = mode
        subeq   r1, r0, #SECURESTK_RESERVE      ; (r1) = SP == pCurThread->tlsSecure - SECURESTK_RESERVE,
                                                ;       if 1st trip into kernel
PSLCallCommon

        ; STILL IN ABORT MODE, original r0-r3, r12, and lr on abort stack, INT disabled
        ; (sp_system) - sp at the point of call
        ; (r0) - pCurThread->tlsSecure
        ; (r1) - sp for secure stack
        ; (r2) - mode
        ; (r3) - iMethod

        ; We need a few registers to work with. However, we cannot take a fault at this point.
        ; The only safe place to save registers is in the secure stack reserve area, where we
        ; have space for 4 registers.
        sub     r0, r0, #SECURESTK_RESERVE      ; (r0) = ptr to the 4 register space above tls
        stmia   r0, {r4-r7}                     ; save r4, r5, r6, and r7

        ; restore registers (use r4-r7 to keep original r0-r3)
        pop     {r4-r7, r12, lr}                ; (r12, lr_abt no longer used, but just restore for symmatric save/restore)

        ;
        ; switch to system mode, and enable interrupts
        ; NOTE: Can't turn on INT before updating SP, or we're in trouble if interrupted
        ;
        cpsid   i, #SYSTEM_MODE                 ; switch to System Mode, interrupts disabled
        ; (sp) - sp at the point of call
        ; (r0) - pCurThread->tlsSecure - SECURESTK_RESERVE (ptr to the saved r4-r7)
        ; (r1) - sp for secure stack
        ; (r2) - mode
        ; (r3) - iMethod
        ; (r4-r7) - API call arg 0-3

        ; switch stack, and reserve space on secure stack
        mov     r12, sp                         ; (r12) = sp at the point of call
        mov     sp, r1                          ; switch stack to kernel stack
        sub     sp, sp, #cstk_sizeof            ; (sp) = make room for callstack structure (pcstk)

        cpsie   i                               ; enable interrupts

        ;
        ; In SYSTEM mode, and interrupt enabled from here on
        ;

        ; setup pcstk
        ; (sp) - pcstk
        ; (r0) - pCurThread->tlsSecure - SECURESTK_RESERVE (ptr to the saved r4-r7)
        ; (r2) - mode
        ; (r3) - iMethod
        ; (r4-r7) - API call arg 0-3
        ; (r12) - old stack
        ; (lr) - return address
        stmia   sp, {r4-r7}                     ; save arg 0-3 on callstack structure
        add     r1, sp, #cstk_regs              ; (r1) = &pcstk->regs[0]
        ldmia   r0, {r4-r7}                     ; restore r4-r7
        str     r12, [sp, #cstk_dwPrevSP]       ; pcstk->dwPrevSP = caller's SP
        str     r2, [sp, #cstk_dwPrcInfo]       ; pcstk->dwPrcInfo = mode
        str     r3, [sp, #cstk_iMethod]         ; pcstk->iMethod = iMethod
        str     lr, [sp, #cstk_retAddr]         ; pcstk->retAddr = lr
        stmia   r1, {r4-r11}                    ; save all callee saved registers in pcstk->regs

        mov     r0, sp                          ; arg0 to ObjectCall == pcstk

        bl      ObjectCall                      ; call ObjectCall
        mov     r12, r0
        
        ; (r12) = function to call
        ; (sp)  = pcstk

        ldr     r6, [sp, #cstk_dwNewSP]         ; (r6) = new SP if user mode server, 0 if k-mode serer

        cbnz    r6, CallUModeFunc
        
        ; k-mode server, call direct

CallKModeFunc
        mov     r4, sp                          ; (r4) = pcstk

        ; ARM calling convention, r0-r3 for arg0-arg3, no room on stack for r0-r3.
        pop     {r0-r3}                         ; retrieve arg0 - arg3

        ; invoke the function
        blx     r12
        
APIRet
        ;
        ; (r0) = return value
        ; (r4) = pcstk
        ;
        mov     sp, r4

        ; save return value
        mov     r5, r0

        ; setup API return
        bl      ServerCallReturn
SvrRtn
        ; (r5) = return value
        mov     r0, r5                          ; restore return value
        ldr     r12, [sp, #cstk_retAddr]        ; (r12) = return address

        ; restore all non-volatile registers
        add     r1, sp, #cstk_regs              ; (r1) = &pcstk->regs[REG_R4]
        ldmia   r1, {r4-r11}                    ; restore r4-r11

        ; return to caller based on mode
        ; (r0) = return value
        ; (sp) = pcstk
        ; (r12) = return address

        ldr     r3, [sp, #cstk_dwPrcInfo]       ; (r3) = pcstk->dwPrcInfo

        cbnz    r3, APIRetUser                  ; which mode to return to (0 == kmode)
        
        ; returning to k-mode caller
        ldr     sp,  [sp, #cstk_dwPrevSP]       ; restore SP if return to k-mode
        bx      r12                             ; jump to return address if return to k-mode

APIRetUser
        ; returning to user mode
        cpsid   i                               ; disable interrupts
        ldr     sp, [sp, #cstk_dwPrevSP]        ; restore SP

        ; returning to user mode caller, INTERRUPTS DISABLED (sp pointing to user stack)
        ;  (r12) = address to continue
        cpsid   i, #SVC_MODE                    ; switch to SVC Mode w/IRQs disabled

        tst     r12, #1                         ; returning to THUMB mode?
        moveq   r2, #USER_MODE                  ; (r2) = USER_MODE if returning to ARM mode
        movne   r2, #USER_MODE:OR:THUMB_STATE   ; (r2) = USER_MODE|THUMB_STATE if returning to THUMB mode
        msr     spsr_cxsf, r2

        mov     lr, r12
        movs    pc, lr                          ; switch to User Mode and return


KPSLCall
        ; STILL IN ABORT MODE, original r0-r3, r12, and lr on abort stack, INT disabled
        ; In kernel mode already - must be defining WINCEMACRO.
        ;
        ;   (r3) = iMethod
        ;
        ; In order to get to the original stack pointer, we need to swtich back
        ; to system mode. ARM v6 provide instructions to get registers of other
        ; mode, but we can't use it for we need to support pre-V6 architectures.
        ;
        ;
        cpsid   i, #SYSTEM_MODE                 ; switch to System Mode w/IRQs disabled
        mov     r1, sp                          ; target SP (r1) == sp
        cpsid   i, #ABORT_MODE                  ; switch back to ABORT Mode w/IRQs disabled
        
        cmp     r3, #PERFORMCALLBACK            ; is this a callback?
        
        movne   r2, #0                          ; (r2) = mode == 0
        bne     PSLCallCommon                   ; jump to common code

        ; callback, 
        ; (1) restore everything,
        ; (2) switch back to SYSTEM_MODE, enable int, and
        ; (3) jump to NKPerformCallBack
        
        pop     {r0-r3, r12, lr}                ; restore registers
        cpsie   i, #SYSTEM_MODE                 ; switch to System Mode, and enable interrupts
        b       NKPerformCallBack
        
CallUModeFunc
        ;
        ; calling user-mode server
        ;   (r6)  = new SP, with arg0 - arg3 on stack
        ;   (r12) = function to call

        ; update return address
        ldr     r3, =KData
        ldr     lr, [r3, #dwSyscallReturnTrap]  ; return via a trap

CallUModeFuncWithLrSet
        ;
        ; NOTE: accessing stack of user-mode server in the following code.
        ;
        ; ARM calling convention, r0-r3 for arg0-arg3, no room on stack for r0-r3.
        ldmfd   r6!, {r0-r3}

        cpsid   i, #SYSTEM_MODE                 ; disable interrupts
        mov     sp, r6                          ; update sp (points to user stack)

        ;  (r12) = address to continue
        cpsid   i, #SVC_MODE                    ; switch to SVC Mode w/IRQs disabled

        ; update tlsptr
        GetPCB  r7                              ; (r7) = ppcb
        ldr     r8, [r7, #pCurThd]              ; (r8) = pCurThread
        ldr     r9, [r8, #ThTlsNonSecure]       ; pCurThread->tlsPtr
        str     r9, [r8, #ThTlsPtr]             ;       = pCurThread->tlsNonSecure
        str     r9, [r7, #lpvTls]               ; ppcb->lpvTls = = pCurThread->tlsNonSecure

        ; always return to THUMB mode, as all API a supposed to be calling from our own DLLs
        movs    r8, #USER_MODE:OR:THUMB_STATE   ; (r8) = USER_MODE|THUMB_STATE
        msr     spsr_cxsf, r8

        mov     lr, r12
        movs    pc, lr                          ; switch to User Mode and return

        NESTED_END APICallEH

        ;-------------------------------------------------------------------------------
        ;
        ; MDSwitchToUserCode - final step of process creation,
        ; (r0) = target IP
        ; (r1) = arg list
        ;
        ;-------------------------------------------------------------------------------
        LEAF_ENTRY MDSwitchToUserCode

        mov     r6, r1
        mov     r12, r0
        mov     lr, #0                          ; should never return
        b       CallUModeFuncWithLrSet
        
        LEAF_END MDSwitchToUserCode


;-------------------------------------------------------------------------------
; Common fault handler code.
;
;       The fault handlers all jump to this block of code to process an exception
; which cannot be handled in the fault handler or to reschedule.
;
;       (r1) = exception ID
;       original r0-r3 & lr saved at (ppcb - 0x14).
;-------------------------------------------------------------------------------
        LEAF_ENTRY CommonHandler
        GetPCB  r3                              ; (r3) = ppcb
        mrs     r2, spsr                        ; (r2) = spsr
        cpsid   i, #SVC_MODE                    ; switch to Supervisor mode w/IRQs disabled

        and     r0, r2, #0x1f                   ; (r0) = previous mode
        cmp     r0, #USER_MODE                  ; 'Z' set if from user mode
        cmpne   r0, #SYSTEM_MODE                ; 'Z' set if from System mode
        bne     %F50                            ; reentering kernel, save state on stack

; Save the processor state into a thread structure. If the previous state was
; User or System and the kernel isn't busy, then save the state into the current
; thread. Otherwise, create a temporary thread structure on the kernel stack.
;
;       (r1) = exception ID
;       (r2) = SPSR
;       (r3) = ppcb
;       Interrupted r0-r3, and Pc saved at (r3-0x14)
;       In Supervisor Mode with interrupt disabled

SaveAndReschedule

        ldr     r0, [r3,#pCurThd]               ; (r0) = ptr to current thread
        add     r0, r0, #TcxR4                  ; (r0) = ptr to r4 save
        stmia   r0, {r4-r12}                    ; save non-banked registers
        cpsid   i, #SYSTEM_MODE                 ; switch to system mode with interrupts disabled
        str     sp, [r0, #TcxSp-TcxR4]          ; save user bank sp
        str     lr, [r0, #TcxLr-TcxR4]          ; save user bank lr
        cpsid   i, #SVC_MODE                    ; switch to Supervisor mode w/IRQs disabled
10      ldmdb   r3, {r3-r7}                     ; load saved r0-r3 & Pc
        stmdb   r0!, {r2-r6}                    ; save Psr, r0-r3
        sub     r0, r0, #THREAD_CONTEXT_OFFSET  ; (r0) = ptr to Thread struct
        str     r7, [r0,#TcxPc]                 ; save Pc

        mfc15   r2, c6                          ; (r2) = fault address
        mfc15   r3, c5                          ; (r3) = fault status

; Process an exception or reschedule request.

Reschedule
        ;
        ; (r0) = pth (can be NULL if ID_RESCHEDULE)
        ; (r1) = id
        ; (r2) = FAR (ignored if ID_RESCHEDULE)
        ; (r3) = FSR (ignored if ID_RESCHEDULE)
        ;
        cpsie   i, #SVC_MODE                    ; switch to SVC mode and enable interrupts
        
        bl      HandleException
        ldr     r2, [r0, #TcxPsr]               ; (r2) = target status
        and     r1, r2, #0x1f                   ; (r1) = target mode
        cmp     r1, #USER_MODE
        cmpne   r1, #SYSTEM_MODE
        bne     %F30                            ; not going back to user or system mode?

        ; going back to user/system mode
        cpsid   i, #SYSTEM_MODE                 ; switch to system mode with interrupt disabled

        GetPCB  r1                              ; (r1) = ppcb
        ldr     r3, [r1, #ownspinlock]          ; (r3) = ppcb->ownspinlock

        cbnz    r3, ResumeExecution             ; resume execution if own a spinlock

        ldrb    r1, [r1, #bResched]             ; (r1) = nest level + reschedule flag
        cmp     r1, #1
        mov     r1, #ID_RESCHEDULE
        beq     Reschedule                      ; interrupted, reschedule again

ResumeExecution
        ;
        ; in SYSTEM mode, with interupts disabled
        ; (r0) = thread to resume execution
        ; (r2) = target mode
        ;

        ; reload user bank sp and lr
        ldr     sp, [r0, #TcxSp]                ; sp = pth->CtxSp
        ldr     lr, [r0, #TcxLr]                ; lr = pth->CtxLr

        cpsid   i, #SVC_MODE                    ; switch to SVC mode and disble interrupts
        
        msr     spsr_cxsf, r2                   ; spsr = pth->CtxPsr
        ldr     lr, [r0, #TcxPc]                ; (lr) = pth->CtxPc
        add     r0, r0, #TcxR0                  ; (r0) = &pth->CtxR0
        ldmia   r0, {r0-r12}                    ; reload r0-r12
        movs    pc, lr


30
        ; Return to a non-preemptible privileged mode.
        ;
        ; in SVC mode, interrupts enabled
        ;
        ; (r0) = ptr to THREAD structure
        ; (r2) = target mode
        
        orr     r2, r2, #THUMB_STATE            ; (r2) = psr to restore, with 'T' bit set (we're in THUMB mode now)
        msr     cpsr_cxsf, r2                   ; restore cpsr, except thumb state
                                                ; NOTE: THUMB state will be restored when we restore PC
                                                ;       as LSB of PC is set iff we're in THUMB mode
        add     r0, r0, #TcxR2                  ; r0 = &pth->CtxR2
        ldmia   r0, {r2-r12}                    ; reload {r2-r12}
        ldr     sp, [r0, #TcxSp-TcxR2]          ; reload sp
        ldr     lr, [r0, #TcxLr-TcxR2]          ; reload lr
        ldr     r1, [r0, #TcxPc-TcxR2]          ; (r1) keeps pc
        push    {r1}                            ; save pc on stack
        ldmdb   r0, {r0, r1}                    ; restore {r0, r1}
        pop     {pc}                            ; return (possible mode switch)

;
; Save registers for fault from a non-preemptible state.

50      sub     sp, sp, #TcxSizeof              ; allocate space for temp. thread structure
        cmp     r0, #SVC_MODE
        add     r0, sp, #TcxR4                  ; (r0) = ptr to r4 save area
        stmia   r0, {r4-r12}                    ; save {r4-r12}
        bne     %F55                            ; must mode switch to save sp and lr if not in SVC mode
        str     lr, [r0, #TcxLr-TcxR4]          ; save lr
        add     r4, sp, #TcxSizeof              ; (r4) = old SVC stack pointer
        str     r4, [r0, #TcxSp-TcxR4]          ; save sp
        b       %B10

55
;
; we're pretty much hosed when we get here (faulted in abort/irq mode)
; but still, we'd like to get some exception print out if possible
;
;   (r0) = &pth->ctx.R4
;   (r2) = mode where exception coming from
;   r4-r12 already saved in thread structure
;

        orr     r4, r2, #THUMB_STATE:OR:0x80    ; (r4) = psr to restore + with 'T' bit, int disabled

        msr     cpsr_c, r4                      ; and switch to mode exception came from
        str     sp, [r0, #TcxSp-TcxR4]          ; save banked sp
        str     lr, [r0, #TcxLr-TcxR4]          ; save banked lr
        
        cpsid   i, #SVC_MODE                    ; back to supervisor mode, IRQ disabled
        b       %B10                            ; go save remaining state
        LEAF_END    CommonHandler

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY IRQHandler
        PROLOG_NOP   sub lr, lr, #4                      ; fix return address
        PROLOG_PUSH  {r0-r3, r12, lr}

        ;
        ; Test interlocked API status.
        ;
        ; if (IsInInterlockedRegion (lr)
        ;     && (r3 != 0)
        ;     && (r3 > lr)) {
        ;     lr &= ~0x1e;
        ; }
        ;
        sub     r0, lr, #INTERLOCKED_START
        cmp     r0, #INTERLOCKED_END-INTERLOCKED_START
        bcs     IRQH_NoILock

        ; lr is in interlocked region
        cbz     r3, IRQH_NoILock

        cmp     r3, lr
        bls     IRQH_NoILock
        
        bic     lr, lr, #0x1e
        str     lr, [sp, #FrameLR]              ; update return address in stack frame

IRQH_NoILock

        ;
        ; CAREFUL! The stack frame is being altered here. It's ok since
        ; the only routine relying on this was the Interlock Check. 
        ;
        mrs     r1, spsr                        ; (r1) = saved status reg
        ldr     r0, [sp, #FrameLR]              ; (r0) = Interrupted PC
        push    {r1}                            ; save SPSR onto the IRQ stack

        cpsid   i, #SVC_MODE                    ; switch to supervisor mode w/IRQs disabled
        push    {r4-r6, lr}                     ; save r4-r6, lr onto the SVC stack

        GetPCB  r4                              ; (r4) = ppcb
        ldr     r5, =KData                      ; (r5) = g_pKData
        mov     r6, r0                          ; (r6) = interrupted PC

        ;====================================================================
        ;
        ; Log the ISR entry event to CeLog
        ;
        
        ; Skip logging if KInfoTable[KINX_CELOGSTATUS] == 0
        ldr     r1, [r5, #CeLogStatus]          ; (r1) = KInfoTable[KINX_CELOGSTATUS]

        cbz     r1, IRQH_NoCeLogISREntry
        
        mov     r1, r6                          ; (r1) = Interrupted PC
        mov     r0, #0x80000000                 ; (r0) = mark as ISR entry

        bl      CELOG_Interrupt
        
        ; mark that we are going a level deeper
        ldrsb   r0, [r4,#cNest]                 ; (r0) = kernel nest level
        add     r0, r0, #1                      ; in a bit deeper (0 if first entry)
        strb    r0, [r4,#cNest]                 ; save new nest level
        
IRQH_NoCeLogISREntry
        
        ;====================================================================

        ;
        ; Now we call the OEM's interrupt handler code. It is up to them to
        ; enable interrupts if they so desire. We can't do it for them since
        ; there's only on interrupt and they haven't yet defined their nesting.
        ;
        mov     r0, r6                          ; (r0) = interrupted PC
        bl      OEMInterruptHandler

        ; (r0) = SYSINTR returned
        cmp     r0, #SYSINTR_IPI
        bne     IRQH_NotIPI

        ; IPI - special handling if the command is IPI_STOP_CPU.
        ldr     r1, =g_dwIpiCommand             ; (r1) = &g_dwIpiCommand
        ldr     r1, [r1]                        ; (r1) = g_dwIpiCommand
        cmp     r1, #IPI_STOP_CPU
        beq     IRQH_SaveContextForDebugger     ; if it's stopping all CPUs, save interrupted context
IRQH_HandleIPI        
        bl      HandleIpi
IRQH_NotIPI
        
        ; notify OEM an interrupt had occurred
        bl      OEMNotifyIntrOccurs
        mov     r6, r0                          ; (r6) = SYSINTR returned

        ;====================================================================
        ;
        ; Log the ISR exit event to CeLog
        ;
        
        ; Skip logging if KInfoTable[KINX_CELOGSTATUS] == 0
        ldr     r2, [r5, #CeLogStatus]          ; (r2) = KInfoTable[KINX_CELOGSTATUS]

        cbz     r2, IRQH_NoCeLogISRExit
        
        ; call CELOG_Interrupt with cNest << 16 | SYSINTR
        ldrsb   r1, [r4,#cNest]                 ; (r1) = cNest value (1, 2, etc)
        sub     r1, r1, #1                      ; make one level less
        orr     r0, r0, r1, LSL #16             ; (r0) = cNest value << 16 + SYSINTR
        strb    r1, [r4,#cNest]        
        mov     r1, #0                          ; (r1) = 0 (RA param to CELOG_Interrupt)
        
        bl      CELOG_Interrupt

IRQH_NoCeLogISRExit

        ;====================================================================


        cpsid   i                               ; disable interrupts


        cmp     r6, #SYSINTR_RESCHED
        beq     %F10
        sub     r6, r6, #SYSINTR_DEVICES
        cmp     r6, #SYSINTR_MAX_DEVICES
        ;
        ; If not a device request (and not SYSINTR_RESCHED)
        ;        
        ldrb    r0, [r4, #bResched]             ; (r0) = reschedule flag
        bhs     %F20                            ; not a device request

        ;
        ; Update PendEvent1/PendEvent2 (in KData, instead of PCB)
        ; NOTE: we're not holding scheduler lock here. Therefore, it's necessary to use interlocked operation
        ;       to update PendEvents(1/2) if we have more than 1 CPU.
        ;
        ldr     r2, [r5, #ADDR_NUM_CPUS-USER_KPAGE] ; (r2) = #Cpus
        add     r3, r5, #PendEvents1            ; (r3) = &g_pKData->aPend1

        cmp     r6, #32                         ; (r6 >= 32)?
        blo     %F4
        
        sub     r6, r6, #32                     ; r6 -= 32 if r6 >= 32
        add     r3, r3, #PendEvents2-PendEvents1 ; (r3) = &g_pKData->aPend2, if (r6 >= 32)

4
        movs    r1, #1                          ; (r1) = 1
        mov     r1, r1 LSL r6                   ; (r1) = 1 << r6
        cmp     r2, #1                          ; (#Cpus == 1)?
        beq     %F7

        ;
        ; interlocked update PendEvent(1/2)
        ;
5
        ldrex   r2, [r3]                        ; (r2) = PendEvent(1/2)
        orr     r2, r2, r1                      ; (r2) |= (1 << r0)
        strex   r12, r2, [r3]                   ; (r2) = new value of PendEvent(1/2)
        cmp     r12, #0                         ; store successful?
        beq     %F10
        b       %B5                             ; retry if failed

        ;
        ; single CPU update PendEvent(1/2)
        ;
7
        ldr     r2, [r3]                        ; (r2) = PendEvent(1/2)
        orr     r2, r2, r1                      ; (r2) |= (1 << r0)
        str     r2, [r3]                        ; (r2) = new value of PendEvent(1/2)
        

        ;
        ; mark reschedule needed
        ;
10      ldrb    r0, [r4, #bResched]             ; (r0) = reschedule flag
        orr     r0, r0, #1                      ; set "reschedule needed bit"
        strb    r0, [r4, #bResched]             ; update flag

20      ldr     r2, [r4, #ownspinlock]          ; (r2) = ppcb->ownspinlock

        ; restore r4-r6, lr we saved on SVC stack
        pop     {r4-r6, lr}                     ; restore registers we saved

        cpsid   i, #IRQ_MODE                    ; switch back to IRQ mode w/IRQs disabled
        ;
        ; Restore the saved program status register from the IRQ stack.
        ;
        pop     {r3}                            ; restore IRQ SPSR from the IRQ stack
        msr     spsr_cxsf, r3                   ; (r3) = saved status reg
        and     r1, r3, #MODE_MASK              ; (r1) = interrupted mode
        
        cmp     r1, #USER_MODE                  ; previously in user mode?
        cmpne   r1, #SYSTEM_MODE                ; if not, was it system mode?
        cmpeq   r2, #0                          ; user or system: test ppcb->ownspinlock
        cmpeq   r0, #1                          ; user or system, and not own a spinlock: is resched == 1

        beq     NeedReschedule
        
        pop     {r0-r3, r12, lr}                ; can't reschedule right now so return
        movs    pc, lr
        
NeedReschedule
        GetPCB  lr
        sub     lr, lr, #4                      ; (lr) = &((char *)ppcb)[-4]
        pop     {r0-r3, r12}                    ; restore r0-r3, r12
        stmdb   lr, {r0-r3}                     ; save r0-r3 on kstack
        pop     {r0}                            ; (r0) = resume address
        str     r0, [lr]                        ; save resume address
        mov     r1, #ID_RESCHEDULE              ; (r1) = exception ID

        b       CommonHandler

IRQH_SaveContextForDebugger
        ;
        ; in SVC mode, interrupts disabled, saving context for kernel debugger
        ;
        ; all registers except r1-r3 needs to be preserved.
        ;

        cpsid   i, #SYSTEM_MODE                 ; switch back to SYSTEM mode w/IRQs disabled
        GetPCB  r2                              ; (r2) = ppcb
        ldr     r3, [r2, #pSavedContext]        ; (r3) = ppcb->pSavedContext
        add     r1, r3, #CtxR4                  ; (r1) = &ppcb->pSavedContext->CtxR4
        stmia   r1, {r4-r12}                    ; save non-bank registers r4-r12
        str     sp, [r3, #CtxSp]                ; save user sp
        str     lr, [r3, #CtxLr]                ; save user lr

        cpsid   i, #IRQ_MODE                    ; switch back to IRQ mode w/IRQs disabled
        ldmia   sp, {r4-r10}                    ; {r4-r10} to keep {spsr, r0-r3, r12, lr}

        stmia   r3, {r4-r8}                     ; save spsr, r0-r3 onto save context
        str     r9, [r3, #CtxR12]               ; save r12 (r9 has the original value of r12)
        str     r10, [r3, #CtxPc]               ; save PC (r10 has the original value of interrupted PC)

        ldmia   r1, {r4-r10}                    ; restore r4-r10

        cpsid   i, #SVC_MODE                    ; switch to supervisor mode w/IRQs disabled

        ; original r4-r6, lr are save on SVC stack

        ; update r4-r6 in saved context with the original
        ldr     r1, [sp]
        str     r1, [r3, #CtxR4]
        ldr     r1, [sp, #4]
        str     r1, [r3, #CtxR5]
        ldr     r1, [sp, #8]
        str     r1, [r3, #CtxR6]

        ; if we're interrupt in SVC mode, need to fixup lr and sp with SVC stack value
        ldr     r1, [r3]                        ; (r1) = spsr
        and     r1, r1, #0x1f                   ; (r0) = previous mode
        cmp     r1, #USER_MODE                  ; 'Z' set if from user mode
        cmpne   r1, #SYSTEM_MODE                ; 'Z' set if from System mode
        beq     IRQH_HandleIPI
        
        ; interrupted in SVC mode
        ldr     r1, [sp, #12]                   ; (r1) = SVC mode LR while interrupted
        add     r2, sp, #16                     ; (r2) = SVC mode SP while interrupted
        str     r1, [r3, #CtxLr]                ; update Lr
        str     r2, [r3, #CtxSp]                ; update Sp

        b       IRQH_HandleIPI
        
        NESTED_END IRQHandler


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

        bx      lr                              ; return to caller

        LEAF_END

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY GetHighPos

        adr     r1, PosTable
        mov     r2, #-1
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        cbnz    r3, done

        mov     r0, r0, lsr #8
        add     r2, r2, #8
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        cbnz    r3, done

        mov     r0, r0, lsr #8
        add     r2, r2, #8
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        cbnz    r3, done

        mov     r0, r0, lsr #8
        add     r2, r2, #8
        and     r3, r0, #0xff
        ldrb    r3, [r1, +r3]
        cbnz    r3, done

        add     r3, r3, #9
done
        add     r0, r3, r2
        bx      lr                          ; return to caller


PosTable
        DCB 0,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 8,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
        DCB 6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1,5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1
       
        LEAF_END

;-------------------------------------------------------------------------------
; V6ClearExclusive - Clear exclusive bit on context switch
;       (r0) - a dummy location where we can write to
;-------------------------------------------------------------------------------
        LEAF_ENTRY V6ClearExclusive
        clrex
        bx      lr
        LEAF_END V6ClearExclusive

;-------------------------------------------------------------------------------
; INTERRUPTS_ENABLE - enable/disable interrupts based on arguemnt and return current status
;-------------------------------------------------------------------------------
        LEAF_ENTRY INTERRUPTS_ENABLE

        mov     r2, r0

        ; setup return value
        mrs     r0, cpsr                        ; (r0) = current status
        and     r0, r0, #0x80                   ; (r0) = interrupt disabled bit
        eor     r0, r0, #0x80                   ; (r0) = non-zero if interupts was enabled

        cbz     r2, INTERRUPTS_OFF              ; (r2) == 0 ==> disable interrupts

        ; enable interupts
        cpsie   i
        bx      lr
        LEAF_END

;-------------------------------------------------------------------------------
; INTERRUPTS_ON - enable interrupts
;-------------------------------------------------------------------------------
        LEAF_ENTRY INTERRUPTS_ON
        cpsie   i
        bx      lr
        LEAF_END

;-------------------------------------------------------------------------------
; INTERRUPTS_OFF - disable interrupts
;-------------------------------------------------------------------------------
        LEAF_ENTRY INTERRUPTS_OFF
        cpsid   i
        bx      lr
        LEAF_END
        
;-------------------------------------------------------------------------------
; DWORD MDCallKernelHAPI (FARPROC pfnAPI, DWORD cArgs, LPVOID phObj, REGTYPE *pArgs)
;   - calling a k-mode handle-based API
;-------------------------------------------------------------------------------
        NESTED_ENTRY MDCallKernelHAPI
        PROLOG_PUSH         {r4-r8, lr}
        PROLOG_STACK_ALLOC  MAX_PSL_ARG         ; room for arguments

        add     r12, r3, #12                    ; (r12) = &arg[4] == source (pArgs == &arg[1])
        mov     r4, sp                          ; (r4)  = sp == dest

        ; copy argument (4 - cArgs) from source (&arg[4]) to top of stack
nextcopy
        subs    r1, r1, #4                      ; nargs > 4?
        blt     docall

        ldmia   r12!, {r5-r8}                   ; next 4 args in r5-r8
        stmia   r4!, {r5-r8}

        b       nextcopy
        
docall

        mov     r12, r0                         ; (r12) = pfnAPI
        mov     r4, r3                          ; (r4)  = pArgs
        mov     r0, r2                          ; (r0)  = phObj (setup arg 0)
        ldmia   r4!, {r1-r3}                    ; setup arg1-3

        blx     r12

        EPILOG_STACK_FREE   MAX_PSL_ARG
        EPILOG_POP          {r4-r8, pc}

        NESTED_END MDCallKernelHAPI


;-------------------------------------------------------------------------------
; DWORD MDCallUserHAPI (PHDATA phd, PDHCALL_STRUCT phc)
;   - calling a u-mode handle-based API
;-------------------------------------------------------------------------------
        NESTED_ENTRY MDCallUserHAPI
        PROLOG_NOP              mov r12, sp                 ; (r12) = old SP
        PROLOG_STACK_ALLOC      cstk_sizeof-cstk_dwPrevSP   ; allocate bottom portion of CALLSTACK
        PROLOG_PUSH             {lr}                        ; save return address
        PROLOG_STACK_ALLOC      cstk_retAddr                ; allocate remainder of CALLSTACK

        mov     r2, sp                          ; (r2) = arg2 = pcstk
        str     r12, [sp, #cstk_dwPrevSP]       ; save old SP on pcstk
        add     r3, r2, #cstk_regs              ; (r3) = &pcstk->regs[0]
        stmia   r3, {r4-r11}                    ; save all non-volatile registers on pcstk

        ; (r0) = phd
        ; (r1) = phc
        ; (r2) = pcstk
        bl      SetupCallToUserServer           ; setup call to user

        ; (r0) = function to call
        mov     r12, r0                         ; (r12) = function to call
        ldr     r6, [sp, #cstk_dwNewSP]         ; (r6)  = new SP

        cmp     r6, #0                          ; (r6) = 0 if k-mode (error, in this case)
        ; call user mode server function
        bne     CallUModeFunc

        ; error case (r12 == ErrorReturn/CaptureContext)
        b       CallKModeFunc

        NESTED_END MDCallUserHAPI

;
; TTBR bit format
;
; | 31 --------------------- (14-N) |(13-N) - 6| 5 |4 - 3|2|1|0 |
; |      Translation Base           |   SBZ    |C1 | RGN |P|S|C0|
;
;
;   [4:3] RGN - Outer cachable attributes for page table walking:
;           b00 = Outer Noncachable
;           b01 = Outer Cachable Write-Back cached, Write Allocate
;           b10 = Outer Cachable Write-Through, No Allocate on Write
;           b11 = Outer Cachable Write-Back, No Allocate on Write.
;
;   [2] P - If ECC is supported, indicates to the memory controller whether it is 
;       enabled (P=1) or disabled (P=0).
;
;   [1] S - Indicates whether the page table walk is to Shared or Non-Shared memory.
;           0 = Non-Shared. This is the reset value.
;           1 = Shared.
;   C0:C1 (bit0:bit5) - same as RGN, except describing inner cacheability
;


;-------------------------------------------------------------------------------
; GetTranslationBase - Get the translation base register, called in KCall
;       context during process switch.
; Argument: None
;-------------------------------------------------------------------------------
        LEAF_ENTRY GetTranslationBase
        mfc15   r0, c2                          ; (r0) = translation base
        bic     r0, r0, #TTBR_CTRL_BIT_MASK     ; clear the control bits
        bx      lr
        LEAF_END GetTranslationBase

;-------------------------------------------------------------------------------
; SetupTTBR1 - update TTBR1/TTBCR for V6 or later
; Argument: (r0) = physical address of kernel's page directory | g_pKData->dwTTBRCtrlBits
;-------------------------------------------------------------------------------
        LEAF_ENTRY SetupTTBR1
        ; called only for v6 or later
        movs    r2, #1                          ; set N=1, for TTBCR
        movs    r3, #0
        mcr     p15, 0, r0, c2, c0, 1           ; Write Translation Table Base Register 1 (TTBR1)
        mcr     p15, 0, r2, c2, c0, 2           ; Write Translation Table Base Control Register (set N=1)
        isb                                     ; flush prefetch buffer (ISB)
        dsb
        bx      lr
        LEAF_END SetupTTBR1

;-------------------------------------------------------------------------------
; UpdateAsidAndTranslationBase - update current ASID and TTBR0
; Argument: (r0) = new ASID
;           (r1) = new value for TTBR0 (already or'd with g_pKData->dwTTBRCtrlBits)
;-------------------------------------------------------------------------------
        LEAF_ENTRY UpdateAsidAndTranslationBase

        ;
        ; V6 or later, update ASID and TTBR0
        ;
        movs    r2, #INVALID_ASID

        ; save current cpsr and disable interrupts
        mrs     r12, cpsr                       ; (r12) = current status
        cpsid   i                               ; disable interrupts
        
        
        ; set ASID to invalid so that prefecth/branch prediction won't cause trouble
        ; while we're updating TTBR0
        movs    r3, #0
        dsb                                     ; work around Cortex-A9 errata 754322
        mcr     p15, 0, r2, c13, c0, 1          ; set ASID to an invalid ASID
        isb                                     ; flush prefetch buffer (ISB)

        ; update ttbr0
        mcr     p15, 0, r1, c2, c0, 0           ; update TTBR0
        isb                                     ; flush prefetch buffer (ISB)
        
        dsb                                     ; work around Cortex-A9 errata 754322
        
        ; now update ASID to real value
        mcr     p15, 0, r0, c13, c0, 1          ; set ASID to the desired ASID
        mcr     p15, 0, r3, c7, c5, 6           ; Flush Entire Branch Target Cache (BTAC). 
        isb                                     ; flush prefetch buffer (ISB)

        ; restore cpsr, saved in r12
        msr     cpsr_c, r12                     ; restore status register

        bx      lr
        LEAF_END  UpdateAsidAndTranslationBase


;-------------------------------------------------------------------------------
; BOOL UnalignedAccessEnable (BOOL fEnable)
;
;       always enable as the new compiler assumes this
;-------------------------------------------------------------------------------
        LEAF_ENTRY UnalignedAccessEnable

        ; nop
        movs    r0, #0
        bx      lr
        
        LEAF_END UnalignedAccessEnable


;-------------------------------------------------------------------------------
; release a spinlock and reschedule
;
; NOTE: Thread context must be saved before releasing the spinlock. For other CPU
;       can pickup the thread and start running it once spinlock is released.
;
; (r0) = spinlock to release
;-------------------------------------------------------------------------------
        LEAF_ENTRY MDReleaseSpinlockAndReschedule

        ;
        ; DEBUGCHK (!InPrivilegeCall () && (1 == GetPCB ()->ownspinlock));
        ;
        ; cpsr/lr are banked registers, we need to get it before switch to supervisor mode
        mrs     r2, cpsr                        ; (r2) = psr
        orr     r2, r2, #THUMB_STATE

        ; get pcb
        GetPCB  r12

        ; (r12) = ppcb
        ldr     r1, [r12, #pCurThd]             ; (r1) = pCurThread
        

        ; save psr/lr in thread strucutre
        str     sp, [r1, #TcxSp]                ; save SP
        str     r2, [r1, #TcxPsr]               ; save PSR
        str     lr, [r1, #TcxLr]                ; save lr (probably not needed)
        str     lr, [r1, #TcxPc]                ; save lr as pc so the thread will return 
                                                ; from this function upon resume

        ; save non-volatile registers in thread structure
        add     r1, r1, #TcxR4                  ; (r1) = ptr to r4 save
        stmia   r1, {r4-r11}                    ; save non-volatile registers


        ; switch to supervisor mode
        cps     #SVC_MODE

        ; no reference to pCurThread from this point
        movs    r1, #0

        ;
        ; thread context saved, ready to release spinlock
        ;
        ; release spinklock (psplock->owner_cpu = psplock->next_owner; ppcb->ownspinlock = 0;)
        ; (r12) = ppcb
        ; (r0)  = psplock
        ; (r1)  = 0 == ID_RESCHEDULE

        ;
        ; issue DSB if more than 1 CPUs.
        ;
        ldr     r3, =KData
        ldr     r3, [r3, #ADDR_NUM_CPUS-USER_KPAGE]
        cmp     r3, #1

        bne     MultiCore

        ; single core
        ldr     r2, [r0, #next_owner]           ; (r2) = psplock->next_owner
        str     r2, [r0, #owner_cpu]            ; psplock->owner_cpu = psplock->next_owner
        str     r1, [r12, #ownspinlock]         ; clear ownspinlock field

        movs    r0, #0                          ; no current thread
        b       Reschedule

MultiCore
        ; multi core
        DMB
        
        ldr     r2, [r0, #next_owner]           ; (r2) = psplock->next_owner
        str     r2, [r0, #owner_cpu]            ; psplock->owner_cpu = psplock->next_owner

        DSB
        SEV
        
        str     r1, [r12, #ownspinlock]         ; clear ownspinlock field

        movs    r0, #0                          ; no current thread
        b       Reschedule

        LEAF_END MDReleaseSpinlockAndReschedule


;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY KCall

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

        bxne    r12                             ; already non-preemptible, invoke routine directly

        ; not nested. switch to SVC mode to perform KCall
        cps     #SVC_MODE

        blx     r12

        ;
        ; ppcb->ownspinlock and ppcb->dwKCRes are synchronous, retrieve them here
        ;
        GetPCB  r12                             ; (r12) = ppcb
        ldr     r2, [r12, #ownspinlock]         ; (r2)  = ppcb->ownspinlock
        ldr     r3, [r12, #dwKCRes]             ; (r3)  = ppcb->dwKCRes

        cps     #SYSTEM_MODE                    ; back to preemtible state

        ; can't reschedule if we own spinlock
        cmp     r2, #0                          ; ppcb->ownspinlock?
        bxne    lr                              ; return if we own a spinlock
        
        ;
        ; NOTE: It's possible that we can be preempted and start running on another CPU, but
        ;       it's okay because the worst case is that we ddo a redundent reschedule.
        ;
        cbnz    r3, %F10
        
        ldrb    r2, [r12, #bResched]            ; (r2) = reschedule flag
        cbnz    r2, %F10

        bx      lr                              ; no reschedule needed so return

10
        ;
        ; Need to reschedule
        ; 
        mov     r12, lr
        mrs     r2, cpsr
        orr     r2, r2, #THUMB_STATE

        cpsid   i, #SVC_MODE
        ;
        ; NOTE: (1) can't use the preivous ppcb value we got, for we can be running on a different CPU.
        ;       (2) we only need to save r0 and lr (kept in r12), but we saved r0-r3 and lr for simplicity
        ;
        GetPCB  r3                                  ; (r3) = ppcb
        stmdb   r3, {r0-r3, r12}
        mov     r1, #ID_RESCHEDULE
        b       SaveAndReschedule
        LEAF_END KCall

VFP_ENABLE          EQU     0x40000000
VFP_FPEXC_EX_BIT    EQU     0x80000000

;
;   VFP support routines
;

;------------------------------------------------------------------------------
; DWORD ReadAndSetFpexc (DWORD dwNewFpExc);
;------------------------------------------------------------------------------
        LEAF_ENTRY ReadAndSetFpexc
        
        mov     r1, r0          ; (r1) = new value of fpexc
        vmrs    r0, fpexc       ; (r0) = return old value of fpexc
        vmsr    fpexc, r1       ; update fpexc
        bx      lr
        
        LEAF_END ReadAndSetFpexc

;------------------------------------------------------------------------------
; void SaveFloatContext(PTHREAD pth, BOOL fPreempted, BOOL fExtendedVfp)
;------------------------------------------------------------------------------
        LEAF_ENTRY SaveFloatContext

        ; enable VFP
        mov     r12, #VFP_ENABLE
        vmsr    fpexc, r12
        
        ; save FPSCR
        vmrs    r12, fpscr
        str     r12, [r0, #TcxFpscr]

        ; save the general purpose registers
        add     r0, r0, #TcxRegs
        vstmia  r0!, {d0-d15}

        ; do we have an extended VFP/Neon register bank?
        cmp     r2, #0
        bxeq    lr
        
        vstmia  r0, {d16-d31}
        
        bx      lr
        LEAF_END

;------------------------------------------------------------------------------
; void RestoreFloatContext (PTHREAD pth, BOOL fExtendedVfp)
;------------------------------------------------------------------------------
        LEAF_ENTRY RestoreFloatContext

        ; enable VFP
        mov     r12, #VFP_ENABLE
        vmsr    fpexc, r12

        ; restore FPSCR
        ldr     r12, [r0, #TcxFpscr]
        vmsr    fpscr, r12
        
        ; restore all the general purpose registers
        add     r0, r0, #TcxRegs
        vldmia  r0!, {d0-d15}

        ; do we have an extended VFP/Neon register bank?
        cmp     r1, #0
        bxeq    lr
        
        vldmia  r0, {d16-d31}
        
        bx      lr
        LEAF_END

;-------------------------------------------------------------------------------
; InPrivilegeCall - check if in non-preemptible "privilege" state
;
; InPrivilegeCall is called to test if code is being called in a privilege state
; or not.
;
;       Entry   none
;       Return  (r0) = 0 if not in privilege state, !=0 in privilege state
;-------------------------------------------------------------------------------
        LEAF_ENTRY InPrivilegeCall

        mrs     r0, cpsr                        ; (r0) = current status
        and     r0, r0, #0x1F                   ; (r0) = current mode
        eor     r0, r0, #SYSTEM_MODE            ; (r0) = 0 iff in system mode

        bx      lr
        LEAF_END InPrivilegeCall

;-------------------------------------------------------------------------------
; CaptureContext is invoked in kernel context on the user thread's stack to
; build a context structure to be used for exception unwinding.
;
; Note: The Psr & Pc values will be updated by ExceptionDispatch from information
;       in the excinfo struct pointed at by the THREAD.
;
;       (sp) = aligned stack pointer
;       (r0-r14), etc. - CPU state at the time of exception
;
;-------------------------------------------------------------------------------

        NESTED_ENTRY CaptureContextEH
        ;-------------------------------------------------------------------------------
        ;++
        ; The following code is never executed. Its purpose is to support unwinding
        ; through the calls to API dispatch and return routines.
        ;--
        ;-------------------------------------------------------------------------------

        PROLOG_NOP          mov r12, sp
        PROLOG_PUSH         {lr}
        PROLOG_PUSH         {r12, lr}
        PROLOG_PUSH         {r0-r12}
        PROLOG_STACK_ALLOC  4

        ALTERNATE_ENTRY CaptureContext
        
        sub     sp, sp, #CtxSizeof+ExrSizeof    ; (sp) = ptr to Context record

        str     lr, [sp, #CtxLr]                ; save lr (R14)
        add     lr, sp, #CtxR0                  ; lr = &pCtx->CtxR0
        stmia   lr, {r0-r12}                    ; save r0-r12

        ; sp/pc will be set in ExceptionDispatch
        
        mrs     r1, cpsr                        ; (r1) = current status
        str     r1, [sp,#CtxPsr]

        mov     r0, #CONTEXT_SEH
        str     r0, [sp,#CtxFlags]              ; set ContextFlags
        
        mov     r1, sp                          ; (r1) = Arg1 = ptr to Context record
        add     r0, r1, #CtxSizeof              ; (r0) = Arg0 = ptr to Exception record
        bl      ExceptionDispatch
        mov     r1, sp                          ; (r1) = pCtx
        ldr     r2, [r1,#CtxPsr]                ; (r2) = new Psr
        and     r3, r2, #MODE_MASK              ; (r3) = destination mode
        
        ; Run SEH?
        cbnz    r0, RunSEH

        ; not running SEH
        
        cmp     r3, #USER_MODE
        cmpne   r3, #SYSTEM_MODE
        bne     ResumeSvcMode                   ; NE if not returning to user/system mode.
        
        ; Returning to user/system mode
        ; (r1) = pCtx
        cpsid   i                               ; disable interrupts, still in system mode

        ; restore sp and lr before switching to SVC mode
        ldr     sp, [r1, #CtxSp]!               ; reload sp, r1 = &pCtx->Sp (R13)
        ldr     lr, [r1, #CtxLr-CtxSp]          ; reload lr

        ; switch to SVC mode for final dispatch
        cpsid   i, #SVC_MODE                    ; switch to Supervisor Mode w/IRQs disabled
        msr     spsr_cxsf, r2
        ldr     lr, [r1,#CtxPc-CtxSp]           ; (svc_lr) = address to continue at
        ldmdb   r1, {r0-r12}                    ; reload r0-r12
        movs    pc, lr                          ; return to user mode.

ResumeSvcMode
        ; resume in privileged mode (probably already hosed when getting here)
        ; (r1) = pCtx
        ; (r2) = pCtx->Psr

        orr     r2, r2, #THUMB_STATE            ; (r2) = psr to restore, with 'T' bit set
        
        msr     cpsr_cxsf, r2                   ; restore cpsr, except thumb state
                                                ; NOTE: THUMB state will be restored when we restore PC
                                                ;       as LSB of PC is set iff we're in THUMB mode

        add     r0, r1, #CtxR2                  ; r0 = &pCtx->R2
        ldmia   r0, {r2-r12}                    ; reload {r2-r12}
        ldr     sp, [r0, #CtxSp-CtxR2]          ; reload sp
        ldr     lr, [r0, #CtxLr-CtxR2]          ; reload lr
        ldr     r1, [r0, #CtxPc-CtxR2]          ; (r1) keeps pc
        push    {r1}                            ; save pc on stack
        ldmdb   r0, {r0, r1}                    ; restore {r0, r1}
        pop     {pc}                            ; return (possible mode switch)

        

; Run SEH handler
; Note: RtlDispathException never returns, we're free to use any register here
;       (r1) = pCtx
;       (r2) = new PSR
;       (r3) = target mode
RunSEH
        bic     r2, r2, #EXEC_STATE_MASK3       ; clear JAZELLE state, ITSTATE[1:0], ...
        bic     r2, r2, #EXEC_STATE_MASK1       ;  ... BIG_ENDIAN state, ITSTATE[7:2], and ...
        orr     r2, r2, #THUMB_STATE            ; always set THUMB state
        
        ldr     r12, =g_pfnKrnRtlDispExcp       ; (r12) = &g_pfnKrnRtlDispExcp
        cmp     r3, #USER_MODE
        bne     RunRtlDispatch
        
        ; usre mode
        ldr     r12, =g_pfnUsrRtlDispExcp       ; user mode,   (r12) = &g_pfnUsrRtlDispExcp
        ldr     r1, [r1, #CtxSp]                ; user mode,   target stack (r1) = pCtx->Sp

RunRtlDispatch
        add     r0, r1, #CtxSizeof              ; (r0) = pExr

        cpsid   i, #SYSTEM_MODE                 ; switch to SYSTEM_MODE with IRQ disabled
        mov     sp, r1                          ; update sp

        cpsid   i, #SVC_MODE                    ; switch to SVC mode with IRQ disabled

        ldr     lr, [r12]                       ; (lr) = RtlDispatchException (depending on mode)
        msr     spsr_cxsf, r2                   ; spsr = target psr

        movs    pc, lr

        NESTED_END CaptureContext


        LEAF_ENTRY RtlCaptureContext

        str     r0, [r0, #CtxR0]                ; save r0
        add     r0, r0, #CtxR1                  ; r0 = &pCtx->r1
        stmia   r0!, {r1-r12}                   ; save r1-r12, (r0) == &pCtx->Sp afterward
        mov     r1, sp
        mov     r2, lr
        mov     r3, pc
        mrs     r12, cpsr                       ; (r12) = current status
        stmia   r0, {r1-r3, r12}                ; save sp, lr, pc, and psr

        bx      lr
        
        LEAF_END RtlCaptureContext



;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
    ; void HwTrap(struct _HDSTUB_EVENT2*)
        LEAF_ENTRY HwTrap
        EMIT_BREAKPOINT                         ; CE debug break
        bx  lr                                  ; Return function
        DCD 0, 0, 0                             ; 4 words
        DCD 0, 0, 0, 0                          ; 8 words
        DCD 0, 0, 0, 0                          ; 12 words
        DCD 0, 0, 0, 0                          ; 16 words
        LEAF_END HwTrap
        
        

;-------------------------------------------------------------------------------
; This code is copied to the kernel's data page so that it is accessible from
; the kernel & user code. The kernel checks if it is interrupting an interlocked
; api by range checking the PC to be between UserKPage+0x380 & UserKPage+0x400.
;
; NOTE: 
;   (1) Before jumping to these functions, (r3) MUST BE 0.
;   (2) Each API must use EXACTLY 32 bytes (8 instructions), and entry is 32-byte aligned.
;   (3) (r3) keeps the "finish" address of the interlocked API while in progress, and reset to
;       0 once completed
;
;-------------------------------------------------------------------------------
        ALIGN 32

        LEAF_ENTRY InterlockedAPIs
;; PCBGetDWORD -  USERKDATA+0x380 == 0xffffcb80
;; (r0) == offset
        adr     r3, Done0                       ; (r3) = finish address == instruction at "mov r3, #0"
        mrc     p15, 0, r1, c13, c0, 3          ; (r1) = ppcb
        ldr     r0, [r0, r1]                    ; (r0) = ((LPDWORD)ppcb)[offset]
Done0   mov     r3, #0                          ; done, no need to restart once reached here
        bx      lr                              ; 

        ALIGN 32
;; ILEx -  USERKDATA+0x3a0 == 0xffffcba0
        adr     r3, Done1                       ; (r3) = finish address == instruction at "mov r3, #0"
        ldr     r12, [r0]                       ; (r12) = original
        str     r1, [r0]                        ; store value
Done1   mov     r3, #0                          ; done, no need to restart once reached here
        mov     r0, r12                         ; (r0) = return original value
        bx      lr                              ; 

        ALIGN 32
;; - ILCmpEx  - USERKDATA+0x3c0 == 0xffffcbc0
        adr     r3, Done2                       ; (r3) = finish address == instruction at "mov r3, #0"
        ldr     r12, [r0]                       ; (r12) = original value
        cmp     r12, r2                         ; compare with comparand
        streq   r1, [r0]                        ; store value if equal
Done2   mov     r3, #0                          ; done, no need to restart once reached here
        mov     r0, r12                         ; (r0) = return original value
        bx      lr                              ; 

        ALIGN 32
;; ILExAdd - USERKDATA+0x3e0 == 0xffffcbe0
        adr     r3, Done3                       ; (r3) = finish address == instruction at "mov r3, #0"
        ldr     r12, [r0]                       ; (r12) = original value
        add     r12, r12, r1                    ; (r12) = original value + addent
        str     r12, [r0]                       ; store result
Done3   mov     r3, #0                          ; done, no need to restart once reached here
        sub     r0, r12, r2                     ; (r0) = return value = (original) + (addent) - (r2)
        bx      lr                              ; 
        ALIGN 32
InterlockedEnd

        LEAF_END InterlockedAPIs

        END

