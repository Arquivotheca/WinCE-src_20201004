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
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

        INCLUDE armhigh.inc

; must match the value in kernel.h
PERFORMCALLBACK         EQU     -30     ; (iMethod of PerformCallBack)
SECURESTK_RESERVE       EQU     112     ; (SIZE_PRETLS + SIZE_CRTSTORAGE + 16)

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

        ;-----------------------------------------------------------------------
        ; Globals
        AREA |.data|, DATA

        
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

  IF NKPROF
        IMPORT  CELOG_Interrupt
  ENDIF

        LEAF_ENTRY GetCpuId
        mrc     p15, 0, r0, c0, c0, 0           ; r0 = CP15:r0
        mov     pc, lr
        ENTRY_END

;-------------------------------------------------------------------------------
; KernelStart - called after NKStartup finished setting up to startup kernel,
;-------------------------------------------------------------------------------
        LEAF_ENTRY KernelStart

        ldr     r4, =KData                      ; (r4) = ptr to KDataStruct
        ldr     r0, =APIRet
        str     r0, [r4, #pAPIReturn]           ; set API return address

        mov     r1, #SVC_MODE 
        msr     cpsr_c, r1                      ; switch to Supervisor Mode w/IRQs enabled
        CALL    KernelInit                      ; initialize scheduler, etc.
        mov     r0, #0                          ; no current thread
        mov     r1, #ID_RESCHEDULE
        b       FirstSchedule

        ENTRY_END

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        NESTED_ENTRY    UndefException

        stmdb   sp, {r0-r3, lr}
        mrs     r0, spsr
        tst     r0, #THUMB_STATE                ; Entered from Thumb Mode ?
        subne   lr, lr, #2                      ; Update return address
        subeq   lr, lr, #4                      ;   accordingly
        str     lr, [sp, #-4]                   ; update lr on stack

        PROLOG_END

        mov     r1, #ID_UNDEF_INSTR
        b       CommonHandler
        ENTRY_END UndefException

;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
        LEAF_ENTRY SWIHandler
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
;
; NKPerformCallBack - calling back to user process
;
;-------------------------------------------------------------------------------
        NESTED_ENTRY NKPerformCallBack
        mov     r12, sp                         ; (r12) = original sp
        
        stmfd   sp!, {r0-r3}                    ; save arguments {r0-r3}
        stmfd   sp!, {r4-r11}                   ; save volatile registers
        sub     sp, sp, #cstk_regs              ; make room of callstack
        ;
        ; NOTE: the above instructions decrement sp by (cstk_sizeof)
        ;

        ; (sp) = pcstk
        str     lr,  [sp, #cstk_retAddr]        ; save return address
        str     r12, [sp, #cstk_dwPrevSP]       ; save original sp
        PROLOG_END

        mov     r0, sp
        bl      NKPrepareCallback

        ; (r0) = function to call
        ldr     r12, [sp, #cstk_dwNewSP]        ; (r12) = pcstk->dwNewSP

        cmp     r12, #0                         ; (r12) == 0 if kernel mode

        ; (r12) != 0 if user mode
        movne   r6, r12                         ; (r6) = new SP
        movne   r12, r0                         ; (r12) = function to call
        bne     CallUModeFunc                   ; jump to common code to call a u-mode funciton

        ; kernel-mode, call direct
        mov     r12, r0                         ; (r12) = function to call
        ldr     lr, [sp, #cstk_retAddr]         ; restore lr
        add     sp, sp, #cstk_sizeof-16         ; get rid of cstk struture, less r0-r3 save area
        ldmfd   sp!, {r0-r3}                    ; restore r0-r3
        bx      r12                             ; jump to the function

        ENTRY_END NKPerformCallBack

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
        b       CallUModeFunc
        
        ENTRY_END MDSwitchToUserCode

;-------------------------------------------------------------------------------
;++
; The following code is never executed. Its purpose is to support unwinding
; through the calls to API dispatch and return routines.
;--
;-------------------------------------------------------------------------------

        NESTED_ENTRY    PrefetchAbortEH
;BUGBUG: TBD
;
; We should've never need to unwind through PSL calls. The is probably no longer needed
;
;        stmfd   sp!, {r0-r3}
;        stmfd   sp!, {lr}
;        sub     sp, sp, #apiRa
        PROLOG_END

        ALTERNATE_ENTRY PrefetchAbort

        sub     lr, lr, #4                      ; repair continuation address
        stmfd   sp!, {r0-r3, r12, lr}           ; save r0 - r3, r12, lr, on ABORT stack
                                                ; NOTE: MUST RETRIEVE INFO BEFORE TURNING INT ON
                                                ;       IF IT'S AN API Call
                                                
        sub     r12, lr, #0xF1000000
        cmp     r12, #MAX_METHOD_NUM-0xF1000000         ; 0x20400 is the max value of API encoding
        bhs     ProcessPrefAbort

        ; (r12) = API encoding - 0xF1000000
        sub     r3, r12, #FIRST_METHOD-0xF1000000       ; (r3) = API encoding - FIRST_METHOD
        mov     r3, r3, ASR #2                          ; (r3) = iMethod
        ldr     r2, =KData                              ; (r2) = KData
        ldr     r2, [r2, #pCurThd]                      ; (r2) = pCurThread
        ldr     r1, [r2, #ThPcstkTop]                   ; (r1) = pCurThread->pcstkTop

        cmp     r3, #-1
        bne     NotTrapRtn

        ; possible trap return, make sure pCurThread->pcstkTop != NULL

        cmp     r1, #0                          ; (pCurThread->pcstkTop == NULL)?
        bne     TrapRtn                         ; process trap return if non NULL


NotTrapRtn
        ; STILL IN ABORT MODE, original r0-r3, r12, and lr on abort stack, INT disabled
        ; (r3) = iMethod
        ; (r2) = pCurThread
        ; (r1) = pCurThread->pcstkTop

        ; figure out which mode we're calling from, switch stack if necessary
        mrs     r12, spsr                       ; (r12) = spsr
        and     r12, r12, #MODE_MASK            ; (r12) = caller's mode
        cmp     r12, #USER_MODE
        ldr     r0, [r2, #ThTlsSecure]          ; (r0)  = pCurThread->tlsPtr 

        bne     KPSLCall

        ; caller was in user mode, switch stack
        str     r0, [r2, #ThTlsPtr]             ;       = pCurTheread->tlsSecure
        ldr     r12, =KData                     ; KData.lpvTls 
        str     r0, [r12, #lpvTls]              ;       = pCurTheread->tlsSecure
        
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
        ldmfd   sp!, {r4-r7, r12, lr}           ; (r12, lr_abt no longer used, but just restore for symmatric save/restore)

        ;
        ; switch to system mode, and enable interrupts
        ; NOTE: Can't turn on INT before updating SP, or we're in trouble if interrupted
        ;
        msr     cpsr_c, #SYSTEM_MODE:OR:0x80    ; switch to System Mode, interrupts disabled
        ; (sp) - sp at the point of call
        ; (r0) - pCurThread->tlsSecure - SECURESTK_RESERVE (ptr to the saved r4-r7)
        ; (r1) - sp for secure stack
        ; (r2) - mode
        ; (r3) - iMethod
        ; (r4-r7) - API call arg 0-3

        ; switch stack, and reserve space on secure stack
        mov     r12, sp                         ; (r12) = sp at the point of call
        sub     sp, r1, #cstk_sizeof            ; (sp) = sp on secure stack, with callstack structure reserved (pcstk)
        
        msr     cpsr_c, #SYSTEM_MODE            ; switch to System Mode, and enable interrupts

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
        cmp     r6, #0

        bne     CallUModeFunc
        
        ; k-mode server, call direct

        mov     r4, sp                          ; (r4) = pcstk

        ; ARM calling convention, r0-r3 for arg0-arg3, no room on stack for r0-r3.
        ldmfd   sp!, {r0-r3}                    ; retrieve arg0 - arg3

        ; invoke the function
        mov     lr, pc
        bx      r12
        
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
        cmp     r3, #0                          ; which mode to return to (0 == kmode)
        
        ldreq   sp, [sp, #cstk_dwPrevSP]        ; restore SP if return to k-mode
        bxeq    r12                             ; jump to return address if return to k-mode

        ; returning to user mode
        msr     cpsr_c, #SYSTEM_MODE:OR:0x80    ; disable interrupts
        ldr     sp, [sp, #cstk_dwPrevSP]        ; restore SP

RtnToUMode
        ; returning to user mode caller, INTERRUPTS DISABLED (sp pointing to user stack)
        ;  (r12) = address to continue
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to SVC Mode w/IRQs disabled

        tst     r12, #0x01                      ; returning to Thumb mode ??
        msrne   spsr_c, #USER_MODE | THUMB_STATE
        msreq   spsr_c, #USER_MODE              ; set the Psr according to returning state

        movs    pc, r12                         ; switch to User Mode and return


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
        msr     cpsr_c, #SYSTEM_MODE:OR:0x80    ; switch to System Mode w/IRQs disabled
        mov     r1, sp                          ; target SP (r1) == sp
        msr     cpsr_c, #ABORT_MODE:OR:0x80     ; switch back to ABORT Mode w/IRQs disabled
        
        cmp     r3, #PERFORMCALLBACK            ; is this a callback?
        
        movne   r2, #0                          ; (r2) = mode == 0
        bne     PSLCallCommon                   ; jump to common code

        ; callback, 
        ; (1) restore everything,
        ; (2) switch back to SYSTEM_MODE, enable int, and
        ; (3) jump to NKPerformCallBack
        
        ldmfd   sp!, {r0-r3, r12, lr}           ; restore registers
        msr     cpsr_c, #SYSTEM_MODE            ; switch to System Mode, and enable interrupts
        b       NKPerformCallBack
        


CallUModeFunc
        ;
        ; calling user-mode server
        ;   (r6)  = new SP, with arg0 - arg3 on stack
        ;   (r12) = function to call

        ; update tlsptr
        ldr     r3, =KData                      ; (r3) = &KData
        ldr     r2, [r3, #pCurThd]              ; (r2) = pCurThread
        ldr     r1, [r2, #ThTlsNonSecure]       ; pCurThread->tlsPtr
        str     r1, [r2, #ThTlsPtr]             ;       = pCurThread->tlsNonSecure
        str     r1, [r3, #lpvTls]               ; g_pKData->lpvTls = = pCurThread->tlsNonSecure

        ; update return address
        ldr     lr, =SYSCALL_RETURN             ; return via a trap

        ;
        ; BUGBUG: need to think more about this. We know we don't hold
        ;         any CS here, but can we except here?
        ; NOTE: accessing stack of user-mode server in the following code.
        ;
        ; ARM calling convention, r0-r3 for arg0-arg3, no room on stack for r0-r3.
        ldmfd   r6!, {r0-r3}

        
        msr     cpsr_c, #SYSTEM_MODE:OR:0x80    ; disable interrupts
        mov     sp, r6                          ; update sp (points to user stack)

        ; continue in user mode at (r12)
        b       RtnToUMode

TrapRtn
        ; STILL IN ABORT MODE, original r0-r3, r12, and lr on abort stack, INT disabled
        ;
        ; returning from callback/user-mode server
        ;
        ;   (r0) = return value
        ;   (r1) = pCurThread->pcstkTop
        ;   (r2) = pCurThread
        ;

        ; discard everything on stack_abt
        add     sp, sp, #24                     ; 24 == sizeof (r0-r3, r12, lr)

        ; switch to system mode, interrupts disabled
        msr     cpsr_c, #SYSTEM_MODE:OR:0x80    ; switch to System Mode, interrupts disabled
        
        mov     sp, r1                          ; SP set to secure stack

        ; update TLS
        ldr     r3, =KData                      ; (r3) = g_pKData
        ldr     r1, [r2, #ThTlsSecure]          ; pCurThread->tlsPtr
        str     r1, [r2, #ThTlsPtr]             ;       = pCurThread->tlsSecure
        str     r1, [r3, #lpvTls]               ; g_pKData->lpvTls = = pCurThread->tlsNonSecure

        msr     cpsr_c, #SYSTEM_MODE            ; switch to System Mode, and enable interrupts

        ; jump to common code
        ; (r0) = return value
        ; (sp) = pcstk
        mov     r4, sp
        b       APIRet

        ENTRY_END PrefetchAbortEH

; Process a prefetch abort.

ProcessPrefAbort
        ;
        ; r0-r3, r12, and lr already pushed onto stack (lr already dec by 4)
        ;
        mov     r0, lr                          ; (r0) = faulting address
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
        tst     r2, #THUMB_STATE                ; returning to Thumb code ?
        addne   r1, r0, #THREAD_CONTEXT_OFFSET  ; create pointer to context
        bne     ThumbDispatch                   ; and branch to thumb dispatch code

        msr     cpsr, r2                        ; switch to target mode
        add     r0, r0, #TcxR0
        ldmia   r0, {r0-r15}                    ; reload all registers & return

; Save registers for fault from a non-preemptible state.

50      sub     sp, sp, #TcxSizeof              ; allocate space for temp. thread structure
        cmp     r0, #SVC_MODE
        add     r0, sp, #TcxR4                  ; (r0) = ptr to r4 save area
        bne     %F55                            ; must mode switch to save state
        stmia   r0, {r4-r14}                    ; save SVC state registers
        add     r4, sp, #TcxSizeof              ; (r4) = old SVC stack pointer
        str     r4, [r0, #TcxSp-TcxR4]          ; update stack pointer value
        b       %B10

55
;
; we're pretty much hosed when we get here (faulted in abort/irq mode)
; but still, we'd like to get some exception print out if possible
;
;   (r0) = &pth->ctx.R4
;   (r2) = mode where exception coming from
;
        msr     cpsr, r2                        ; and switch to mode exception came from
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

        ;
        ; Test interlocked API status.
        ;
        sub     r0, lr, #INTERLOCKED_START
        cmp     r0, #INTERLOCKED_END-INTERLOCKED_START
        bllo    CheckInterlockedRestart
        
  IF NKPROF
        ;
        ; On profiling builds, log the ISR entry event to CeLog
        ;
        mov     r0,#0x80000000                  ; mark as ISR entry

        CALL    CELOG_Interrupt
        
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
        ldr     r0, [sp, #FrameLR]              ; (r0) = IRQ LR (parameter to OEMInterruptHandler)
        stmfd   sp!, {r1}                       ; save SPSR onto the IRQ stack

        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to supervisor mode w/IRQs disabled
        stmfd   sp!, {lr}                       ; save LR onto the SVC stack

        ;
        ; Now we call the OEM's interrupt handler code. It is up to them to
        ; enable interrupts if they so desire. We can't do it for them since
        ; there's only on interrupt and they haven't yet defined their nesting.
        ;

        CALL    OEMInterruptHandler

        ; (r0) = SYSINTR returned
        
        ; notify OEM an interrupt had occurred
        CALL    OEMNotifyIntrOccurs

        ldmfd   sp!, {lr}                       ; restore SVC LR from the SVC stack
        msr     cpsr_c, #IRQ_MODE:OR:0x80       ; switch back to IRQ mode w/IRQs disabled

        ;
        ; Restore the saved program status register from the stack.
        ;
        ldmfd   sp!, {r1}                       ; restore IRQ SPSR from the IRQ stack
        msr     spsr, r1                        ; (r1) = saved status reg

  IF NKPROF
        ;
        ; On profiling builds, log the ISR exit event to CeLog
        ;
        
        ; call CELOG_Interrupt with cNest << 16 | SYSINTR
        stmfd   sp!, {r0}
        
        ldr     lr, =KData
        ldrsb   r1, [lr,#cNest]                 ; (r1) = cNest value (1, 2, etc)
        sub     r1, r1, #1                      ; make one level less
        orr     r0, r0, r1, LSL #16             ; (r0) = cNest value << 16 + SYSINTR
        strb    r1, [lr,#cNest]        
        
        CALL    CELOG_Interrupt
        
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

        cmp     r0, #32                         ; (r0 >= 32)?
        subge   r0, r0, #32                     ; r0 -= 32 if true
        ldrlt   r2, [lr, #PendEvents1]          ; (r2) = pending interrupt event mask (lower 32)
        ldrge   r2, [lr, #PendEvents2]          ; (r2) = pending interrupt event mask (upper 32)
        mov     r1, #1
        orr     r2, r2, r1, LSL r0              ; (r2) = new pending mask
        strlt   r2, [lr, #PendEvents1]          ; save it
        strge   r2, [lr, #PendEvents2]          ; save it

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

        add     r0, r0, #INTERLOCKED_START      ; (r0) = interrupted PC
        ldr     r1, [r0]                        ; (r1) = interrupted instruction
        ldr     r2, =0xE1A0                     ; (r2) = "mov rx, ry" opcode
        cmp     r2, r1, LSR #16                 ; is it a MOV instruction?

        ldrne   r2, =0xE12FFF1                  ; or is it a BX instruction ?
        cmpne   r2, r1, LSR #4

        moveq   pc, lr                          ; Y: return to caller
        ldr     r2, =0xE590C000                 ; (r2) = "ldr r12, [r0]" instruction
        cmp     r1, r2                          ; at start of routine?
        moveq   pc, lr                          ;   Y: return to caller
10      ldr     r1, [r0, #-4]!                  ; (r1) = previous instruction
        cmp     r1, r2                          ; found start of routine?
        bne     %B10                            ;   N: keep backing up
        str     r0, [sp, #FrameLR]              ; update return address in stack frame

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
; DWORD MDCallKernelHAPI (FARPROC pfnAPI, DWORD cArgs, LPVOID phObj, REGTYPE *pArgs)
;   - calling a k-mode handle-based API
;-------------------------------------------------------------------------------
        NESTED_ENTRY MDCallKernelHAPI
        stmfd   sp!, {r4-r8, lr}
        sub     sp, sp, #MAX_PSL_ARG            ; room for arguments
        PROLOG_END

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

        mov     lr, pc
        bx      r12

        add     sp, sp, #MAX_PSL_ARG
        ldmfd   sp!, {r4-r8, lr}

        RETURN
        ENTRY_END MDCallKernelHAPI

;-------------------------------------------------------------------------------
; DWORD MDCallUserHAPI (PHDATA phd, PDHCALL_STRUCT phc)
;   - calling a u-mode handle-based API
;-------------------------------------------------------------------------------
        NESTED_ENTRY MDCallUserHAPI
        mov     r12, sp                         ; (r12) = old SP
        sub     sp, sp, #cstk_sizeof            ; make room for callstack struture
        str     lr, [sp, #cstk_retAddr]         ; save return address
        PROLOG_END

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
        ; save pcstk at (r4)
        mov     r4, sp

        ; ARM calling convention, r0-r3 for arg0-arg3, no room on stack for r0-r3.
        ldmfd   sp!, {r0-r3}                    ; retrieve arg0 - arg3

        ; call k-mode function directly
        mov     lr, pc
        bx      r12

        ; (r0) = return value
        ; (r4) = pcstk
        b       APIRet

        ENTRY_END MDCallUserHAPI


;-------------------------------------------------------------------------------
; UpdateTranslationBase - update the translation base register, called in KCall
;       context during process switch.
; Argument: (r0) = physical address of the page directory
; NOTE: Assuming caller had flushed TLB
;-------------------------------------------------------------------------------
        LEAF_ENTRY UpdateTranslationBase
        mtc15   r0, c2                          ; setup translation base (physical of 1st level PT)
        mov     pc, lr
        ENTRY_END UpdateTranslationBase

;-------------------------------------------------------------------------------
; GetTranslationBase - Get the translation base register, called in KCall
;       context during process switch.
; Argument: None
;-------------------------------------------------------------------------------
        LEAF_ENTRY GetTranslationBase
        mfc15   r0, c2                          ; (r0) = translation base
        bic     r0, r0, #TTBR_CTRL_BIT_MASK     ; clear the control bits
        mov     pc, lr
        ENTRY_END GetTranslationBase

;-------------------------------------------------------------------------------
; SetupTTBR1 - update TTBR1/TTBCR for V6 or later
; Argument: (r0) = physical address of kernel's page directory | g_pKData->dwTTBRCtrlBits
;-------------------------------------------------------------------------------
        LEAF_ENTRY SetupTTBR1
        ; called only for v6 or later
        mov     r2, #1                          ; set N=1, for TTBCR
        mov     r3, #0
        mcr     p15, 0, r0, c2, c0, 1           ; Write Translation Table Base Register 1 (TTBR1)
        mcr     p15, 0, r2, c2, c0, 2           ; Write Translation Table Base Control Register (set N=1)
        mcr     p15, 0, r3, c7, c5, 4           ; flush prefetch buffer (ISB)
        mov     pc, lr
        ENTRY_END SetupTTBR1

;-------------------------------------------------------------------------------
; UpdateAsidAndTranslationBase - update current ASID and TTBR0
; Argument: (r0) = new ASID (INVALID_ASID for ARMv4/v5)
;           (r1) = new value for TTBR0 (already or'd with g_pKData->dwTTBRCtrlBits)
;-------------------------------------------------------------------------------
        LEAF_ENTRY UpdateAsidAndTranslationBase
        
        cmp     r0, #INVALID_ASID               ; v6 or later?
        beq     PreV6
        ;
        ; V6 or later, update ASID and TTBR0
        ;
        mov     r2, #INVALID_ASID

        ; save current cpsr and disable interrupts
        mrs     r12, cpsr                       ; (r12) = current status
        orr     r3, r12, #0x80                  ; set interrupt disable bit
        msr     cpsr, r3                        ; update status register

        ; set ASID to invalid so that prefecth/branch prediction won't cause trouble
        ; while we're updating TTBR0
        mov     r3, #0
        mcr     p15, 0, r2, c13, c0, 1          ; set ASID to an invalid ASID
        mcr     p15, 0, r3, c7, c5, 4           ; flush prefetch buffer (ISB)

        ; update ttbr0
        mcr     p15, 0, r1, c2, c0, 0           ; update TTBR0
        mcr     p15, 0, r3, c7, c5, 4           ; flush prefetch buffer (ISB)
        
        ; now update ASID to real value
        mcr     p15, 0, r0, c13, c0, 1          ; set ASID to the desired ASID
        mcr     p15, 0, r3, c7, c5, 6           ; Flush Entire Branch Target Cache (BTAC). 
        mcr     p15, 0, r3, c7, c5, 4           ; flush prefetch buffer (ISB)

        ; restore cpsr, saved in r12
        msr     cpsr, r12                       ; update status register

        bx      lr
PreV6
        ;
        ; Pre-V6, just update TTBR and done.
        ;
        mcr     p15, 0, r1, c2, c0, 0           ; update TTBR0
        bx      lr
        ENTRY_END  UpdateAsidAndTranslationBase

;-------------------------------------------------------------------------------
; ARMSetASID - update current ASID
; Argument: (r0) = new ASID
;-------------------------------------------------------------------------------
        LEAF_ENTRY ARMSetASID

        mov     r1, #0
        mcr     p15, 0, r0, c13, c0, 1          ; Write Context ID Register
        ;
        ; ARM Errata: require flushing BTAC after changing ASID
        ;
        mcr     p15, 0, r1, c7, c5, 6           ; Flush Entire Branch Target Cache (BTAC). 
        mov     pc, lr
        ENTRY_END ARMSetASID



;-------------------------------------------------------------------------------
; BOOL UnalignedAccessEnable (BOOL fEnable)
; V6 or later only - enable/disable unaligned access, return the current state
;-------------------------------------------------------------------------------
        LEAF_ENTRY UnalignedAccessEnable
        cmp     r0, #0                          ; enable?
        mfc15   r1, c1                          ; (r1) = current Control Register configuration data
        
        orreq   r2, r1, #ARMV6_A_BIT            ; (disable) => set A bit
        bicne   r2, r1, #ARMV6_A_BIT            ; (enable) => clear A bit

        mtc15   r2, c1                          ; Write Control Register configuration data with new settings
        mov     pc, lr

        ENTRY_END UnalignedAccessEnable


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

        bxne    r12                             ; already non-preemptible, invoke routine directly

        msr     cpsr_c, #SVC_MODE
        mov     lr, pc                          ; (lr_svc) = PC+8

        bx      r12                             ; invoke routine in non-preemptible state

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

        tst     r12, #0x01                      ; return to thumb mode ?
        orrne   r2, r2, #THUMB_STATE            ; then update the Psr

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
;
;       CaptureContext is a LEAF function now, for we never have to unwind through
;       this funciton
;
;-------------------------------------------------------------------------------

        LEAF_ENTRY CaptureContext
        
        sub     sp, sp, #CtxSizeof+ExrSizeof    ; (sp) = ptr to Context record
        stmib   sp, {r0-r15}                    ; store all registers
        mrs     r1, cpsr                        ; (r1) = current status
        mov     r0, #CONTEXT_FULL
        str     r1, [sp,#CtxPsr]
        str     r0, [sp,#CtxFlags]              ; set ContextFlags
        
        mov     r1, sp                          ; (r1) = Arg1 = ptr to Context record
        add     r0, r1, #CtxSizeof              ; (r0) = Arg0 = ptr to Exception record
        CALL    ExceptionDispatch
        mov     r1, sp                          ; (r1) = pCtx
        ldr     r2, [r1,#CtxPsr]                ; (r2) = new Psr
        and     r3, r2, #0x1f                   ; (r3) = destination mode
        
        ; Run SEH?
        cmp     r0, #0
        bne     RunSEH
        
        cmp     r3, #USER_MODE
        bne     %F9                             ; not returning to user mode
; Returning to user mode.
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to Supervisor Mode w/IRQs disabled
        msr     spsr, r2
        ldr     lr, [r1,#CtxPc]                 ; (lr) = address to continue at
        ldmib   r1, {r0-r14}^                   ; restore user mode registers
        nop                                     ; hazard from user mode bank switch
        movs    pc, lr                          ; return to user mode.

; Run SEH handler
; Note: RtlDispathException never returns, we're free to use any register here
;       (r1) = pCtx
;       (r2) = new PSR
;       (r3) = target mode
RunSEH
        bic     r2, r2, #THUMB_STATE            ; clear THUMB state
        cmp     r3, #USER_MODE
        mov     r4, r1                          ; (r4) = pCtx
        ldreq   r12, =g_pfnUsrRtlDispExcp       ; user mode,   (r12) = &g_pfnUsrRtlDispExcp
        ldrne   r12, =g_pfnKrnRtlDispExcp       ; kernel mode, (r12) = &g_pfnKrnRtlDispExcp
        ldreq   r1, [r4, #CtxSp]                ; user mode,   target stack (r1) = pCtx->Sp
        ldr     r12, [r12]                      ; (r12) = RtlDispatchException (depending on mode)

        add     r0, r1, #CtxSizeof              ; (r0) = pExr =>points to stack top of current stack

        msrne   cpsr, r2                        ; restore cpsr if in kernel mode
        movne   pc, r12                         ; jump to RtlDispatchException if in kernel mode

; Returning to user mode.
; (r12) = function to call
; (r1)  = target stack
; (r2)  = new PSR
        msr     cpsr_c, #SYSTEM_MODE:OR:0x80    ; disable interrupts
        mov     sp, r1                          ; update sp
        
        msr     cpsr_c, #SVC_MODE:OR:0x80       ; switch to Supervisor Mode w/IRQs disabled
        msr     spsr, r2                        ; spsr = target psr
        movs    pc, r12                         ; jump to r3 with PSR restored


; Not running SEH, returning to privileged mode.
;
;       (r1) = ptr to CONTEXT_RECORD
;       (r2) = new PSR

9
        tst     r2, #THUMB_STATE                ; returning to Thumb code ?

        beq     ARMDispatch

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

ThumbSwitch

        DCW     0xBD01                          ; pop {r0, pc}
        DCW     0x46C0                          ; nop


ARMDispatch
        msr     cpsr, r2                        ; (ARM MODE) restore Psr & switch modes
        ldmib   r1, {r0-r15}                    ; (ARM MODE) restore all registers & return

        ENTRY_END CaptureContext


        LEAF_ENTRY RtlCaptureContext

        str     r0, [r0, #CtxR0]                ; save r0
        add     r0, r0, #CtxR1                  ; r0 = &pCtx->r1
        stmia   r0!, {r1-r15}                   ; save r1-r15, (r0) == &pCtx->Psr afterward
        mrs     r1, cpsr                        ; (r1) = current status
        str     r1, [r0]                        ; save PSR

        RETURN

        ENTRY_END RtlCaptureContext

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
        bx      lr

ILPushList
        ldr     r12, [r0]                       ; (r12) = old list head
        str     r12, [r1]                       ; store linkage
        str     r1, [r0]                        ; store new list head
        mov     r0, r12                         ; return old list head
        bx      lr

ILCmpEx
        ldr     r12, [r0]
        cmp     r12, r2
        streq   r1, [r0]
        mov     r0, r12                         ; (r0) = return original value
        bx      lr

ILExAdd
        ldr     r12, [r0]
        add     r2, r12, r1
        str     r2, [r0]
        mov     r0, r12                         ; (r0) = return original value
        bx      lr

ILEx
        ldr     r12, [r0]
        str     r1, [r0]
        mov     r0, r12                         ; (r0) = return original value
        bx      lr

        END_REGION InterlockedEnd


        END

