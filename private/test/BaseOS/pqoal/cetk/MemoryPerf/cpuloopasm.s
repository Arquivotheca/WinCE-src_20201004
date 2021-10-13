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
 
;**********************************************************************
; this function provides a controlled 16-add half-dependent loop so two
; execution units should be able to run the code faster than one.
;**********************************************************************

;
;  File: CPILoopASM.s
;

    TTL E:\WM7BSP\developr\SilS\src\MemoryPerf\CPULoopASM.s
    CODE32

    AREA  |.drectve|, DRECTVE
    DCB "-defaultlib:LIBCMT "
    DCB "-defaultlib:OLDNAMES "

    EXPORT  |?iCPULoop@@YAHH@Z|    ; iCPULoop

; Function compile flags: /Ogtpy
; File e:\wm7bsp\developr\sils\src\memoryperf\cpuloop.cpp

    AREA  |.text|, CODE, ARM, ALIGN=3

|?iCPULoop@@YAHH@Z| PROC     ; iCPULoop

; 26   : {

    stmdb       sp!, {r4, r5, r6, r7, lr}
    sub         sp, sp, #4

    mov         r4, r0

; 27   : 
; 28   :     // create a dependent calculation which the compiler can't over-optimize and the CPU can retire more than 1 instruction per cycle
; 29   :     // perhaps the only retiring 1 instruction per cycle is too optimistic.
; 30   : 
; 31   :         volatile int iTotal = 0; 

    mov         r3, #0
    mov         r5, r0

; 34   :         int iRpt = ciRepeats;
; 35   :         for ( ; iRpt >= 16; iRpt -= 16)
    mov         r1, #0
    mov         r6, #0
    cmp         r4, #0x10
    mov         r7, #0
    blt         |$LN7@iCPULoop|
    mov         r2, r4, lsr #4
    sub         r4, r4, r2, lsl #4
    
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
    
|$LL9@iCPULoop|

; 36   :         {   
; 37   :             iTotal += iX1+iX2;
    subs        r2, r2, #1
    add         r3, r3, r0
    add         r1, r1, r5
; 38   :             iTotal += iX1+iX2;
    add         r6, r6, r0
    add         r7, r7, r5
; 39   :             iTotal += iX1+iX2
    add         r3, r3, r0
    add         r1, r1, r5
; 40   :             iTotal += iX1+iX2;
    add         r6, r6, r0
    add         r7, r7, r5
; 41   :             iTotal += iX1+iX2;
    add         r3, r3, r0
    add         r1, r1, r5
; 42   :             iTotal += iX1+iX2;
    add         r6, r6, r0
    add         r7, r7, r5
; 43   :             iTotal += iX1+iX2;
    add         r3, r3, r0
    add         r1, r1, r5
; 44   :             iTotal += iX1+iX2;
    add         r6, r6, r0
    add         r7, r7, r5
    bne         |$LL9@iCPULoop|
    
|$LN7@iCPULoop|
; 45   :         }
; 46   :         for ( ; iRpt >= 2; iRpt -= 2)

    cmp         r4, #2
    blt         |$LN4@iCPULoop|
    mov         r2, r4, lsr #1
    sub         r4, r4, r2, lsl #1
    
|$LL6@iCPULoop|
; 47   :         {   
; 48   :             iTotal += iX1+iX2;
    subs        r2, r2, #1
    add         r3, r3, r0
    add         r1, r1, r5
    bne         |$LL6@iCPULoop|
    
|$LN4@iCPULoop|
; 49   :         }
; 50   :         for ( ; iRpt >= 0; iRpt--)
    cmp         r4, #0
    bmi         |$LN18@iCPULoop|
    
|$LL3@iCPULoop|
; 51   :         {   
; 52   :             iTotal += iX1;
    subs        r4, r4, #1
    add         r3, r3, r5
    bpl         |$LL3@iCPULoop|
    add         r3, r3, r1
    add         r6, r6, r7
    add         r3, r3, r6
    
|$LN18@iCPULoop|
; 53   :         }
; 54   :         return iTotal;

    mov         r0, r3
; 55   : }
    add         sp, sp, #4
    ldmia       sp!, {r4, r5, r6, r7, pc}

    ENDP  ; |?iCPULoop@@YAHH@Z|, iCPULoop

;;DWORD dwCallerAligned8(const DWORD *pdw)
;;{
;;    if ( NULL!=pdw ) return 0;
;;    return ( _ReturnAddress()&4 ? 4 : 1 );
;;}

    EXPORT  |?dwCallerAligned8@@YAKPBK@Z|   ; dwCallerAligned8

|?dwCallerAligned8@@YAKPBK@Z| PROC          ; dwCallerAligned8

    teq         r0, #0                      ; only test the return address if pointer is NULL         
    andeq       r0, lr, #4                  ; Mask Align8 bit of return address 
    orreq       r0, r0, #1                  ; return 1 for aligned and 5 for unaligned.
    movne       r0, #0                      ; but return non-zero if pointer is NULL
    bx          lr

    ENDP  ; |?dwCallerAligned8@@YAKPBK@Z|, dwCallerAligned8

    END
