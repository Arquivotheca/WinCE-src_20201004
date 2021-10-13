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
 
;****************************************************************************************
; these functions are provided in assembly to provide a controlled 
; instruction loop structure to access memory a fixed number of times per loop.
; be wary of the optimizing compiler changing its behavour over time.
; for example, adding stack references when it thinks spilling registers to
; the stack will be an efficent trick
;****************************************************************************************
; This .s was originally produced by the compiler as a .asm output and hand editted
; some changes were made to the compilers code generation - use of post index rather than
; pre-index and some instruction re-ordering to get perhaps slightly better pairing.
; It has also been cleaned-up for readability.
;****************************************************************************************
;
;****************************************************************************************
;****************************************************************************************
;****************************************************************************************
;
; Warning - if you change this file, you must enable VERIFY_OPTIMIZED and run 
;   s memoryperfexe -d -c -o
; without getting a "Warning: iReadLoop2D mismatch" in the log file
;
; Do not check-in with VERIFY_OPTIMIZED defined.
;
;****************************************************************************************
;****************************************************************************************
;****************************************************************************************

    TTL private\test\BaseOS\pqoal\cetk\MemoryPerf\Read1DASM.s
    CODE32

    AREA    |.drectve|, DRECTVE
    DCB "-defaultlib:LIBCMT "
    DCB "-defaultlib:OLDNAMES "


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1D1( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1D1@@YAHPBKHABUtStridePattern@@@Z| ; iRead1D1
    AREA    |.text|, CODE, ARM, ALIGN=3

r0iTotal        RN  r0
r5iRptMod       RN  r5
r6iRptDiv       RN  r6
r7iTotal        RN  r7
lrpdwT          RN  lr


|?iRead1D1@@YAHPBKHABUtStridePattern@@@Z| PROC      ; iRead1D1

; 466  : {

|$LN20@iRead1D1|
    stmdb       sp!, {r4-r7,lr}

; 467  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;
; 470  :     int iTotal = 0; 
    mov         lrpdwT, r0
    mov         r0iTotal, #0
    mov         r7iTotal, #0
    mov         r3, #0
    mov         r4, #0

; 471  :     int iRpt = ciRepeats;
; 472  :     for ( ; iRpt >= 16; iRpt -= 16)
    cmp         r1, #0x10
    blt         |$LN7@iRead1D1|
    mov         r6iRptDiv, r1, lsr #4
    sub         r5iRptMod, r1, r6iRptDiv, lsl #4
    
; 473  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL9@iRead1D1|
; 475  :         volatile const DWORD* pdwT = pdwBase;
; 476  :         iTotal += *pdwT;
; 477  :         iTotal += *pdwT;
; 478  :         iTotal += *pdwT;
; 479  :         iTotal += *pdwT;
; 480  :         iTotal += *pdwT;
; 481  :         iTotal += *pdwT;
; 482  :         iTotal += *pdwT;
; 483  :         iTotal += *pdwT;
; 484  :         iTotal += *pdwT;
; 485  :         iTotal += *pdwT;
; 486  :         iTotal += *pdwT;
; 487  :         iTotal += *pdwT;
; 488  :         iTotal += *pdwT;
; 489  :         iTotal += *pdwT;
; 490  :         iTotal += *pdwT;
; 491  :         iTotal += *pdwT;

    ldr         r1,[lrpdwT]
    subs        r6iRptDiv, r6iRptDiv, #1
    ldr         r2,[lrpdwT]
    add         r0iTotal, r0iTotal, r3
    ldr         r3,[lrpdwT]
    add         r7iTotal, r7iTotal, r4
    ldr         r4,[lrpdwT]
    add         r0iTotal, r0iTotal, r1
    ldr         r1,[lrpdwT]
    add         r7iTotal, r7iTotal, r2
    ldr         r2,[lrpdwT]
    add         r0iTotal, r0iTotal, r3
    ldr         r3,[lrpdwT]
    add         r7iTotal, r7iTotal, r4
    ldr         r4,[lrpdwT]
    add         r0iTotal, r0iTotal, r1
    ldr         r1,[lrpdwT]
    add         r7iTotal, r7iTotal, r2
    ldr         r2,[lrpdwT]
    add         r0iTotal, r0iTotal, r3
    ldr         r3,[lrpdwT]
    add         r7iTotal, r7iTotal, r4
    ldr         r4,[lrpdwT]
    add         r0iTotal, r0iTotal, r1
    ldr         r1,[lrpdwT]
    add         r7iTotal, r7iTotal, r2
    ldr         r2,[lrpdwT]
    add         r0iTotal, r0iTotal, r3
    ldr         r3,[lrpdwT]
    add         r7iTotal, r7iTotal, r4
    ldr         r4,[lrpdwT]
    add         r0iTotal, r0iTotal, r1
    add         r7iTotal, r7iTotal, r2
    bne         |$LL9@iRead1D1|
    
; 492  :     }

|$LN7@iRead1D1|
; 493  :     for ( ; iRpt >= 4; iRpt -= 4)

    cmp         r5iRptMod,#4
    blt         |$LN4@iRead1D1|
    mov         r6iRptDiv, r5iRptMod, lsr #2
    sub         r5iRptMod, r5iRptMod, r6iRptDiv, lsl #2
    
; 494  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL6@iRead1D1|

; 495  :         volatile const DWORD* pdwT = pdwBase;
; 496  :         iTotal += *pdwT;
; 497  :         iTotal += *pdwT;
; 498  :         iTotal += *pdwT;
; 499  :         iTotal += *pdwT;

    ldr         r1,[lrpdwT]
    subs        r6iRptDiv, r6iRptDiv, #1
    ldr         r2,[lrpdwT]
    add         r0iTotal, r0iTotal, r3
    ldr         r3,[lrpdwT]
    add         r7iTotal, r7iTotal, r4
    ldr         r4,[lrpdwT]
    add         r0iTotal, r0iTotal, r1
    add         r7iTotal, r7iTotal, r2
    bne         |$LL6@iRead1D1|
; 500  :     }

|$LN4@iRead1D1|

; 501  :     for ( ; iRpt >= 0; iRpt--)

    cmp         r5iRptMod,#0
    bmi         |$LN1@iRead1D1|

; 502  :     {   
; 503  :         volatile const DWORD* pdwT = pdwBase;
; 504  :         iTotal += *pdwT;

|$LL3@iRead1D1|
    ldr         r1,[lrpdwT]
    subs        r5iRptMod, r5iRptMod, #1
    add         r0iTotal, r0iTotal, r1
    bpl         |$LL3@iRead1D1|
    

; 505  :     }
; 506  :     return iTotal;
; 507  : }

|$LN1@iRead1D1|

    add         r0iTotal, r0iTotal, r3
    add         r7iTotal, r7iTotal, r4
    add         r0iTotal, r0iTotal, r7iTotal
    ldmia       sp!, {r4-r7,pc}

    ENDP  ; |?iRead1D1@@YAHPBKHABUtStridePattern@@@Z|, iRead1D1


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1D2( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1D2@@YAHPBKHABUtStridePattern@@@Z| ; iRead1D2

r0iTotal        RN  r0
r3pdwT          RN  r3
r4iRptMod       RN  r4
r5dwYStride     RN  r5
r6iRptDiv       RN  r6
r9iTotal        RN  r9
lrpdwT1         RN  lr

|?iRead1D2@@YAHPBKHABUtStridePattern@@@Z| PROC      ; iRead1D2

; 512  : {

    stmdb       sp!,{r4-r9,lr}

; 513  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;
; 515  :     int iTotal = 0; 

    mov         r4iRptMod, r1
    ldr         r5dwYStride,[r2,#8]
    mov         r0iTotal,#0
    mov         r9iTotal,#0
    mov         r7, #0
    mov         r8, #0
    
; 516  :     int iRpt = ciRepeats;
; 517  :     for ( ; iRpt >= 4; iRpt -= 4)

    cmp         r4iRptMod, #4
    blt         |$LN4@iRead1D2|
    mov         r6iRptDiv, r4iRptMod, lsr #2
    sub         r4iRptMod, r4iRptMod, r6iRptDiv, lsl #2
    add         lrpdwT1, r3pdwT, r5dwYStride, lsl #2
    
; 518  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL6@iRead1D2|

; 520  :         volatile const DWORD* pdwT = pdwBase;
; 521  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 522  :         iTotal += *pdwT;                pdwT = pdwBase;
; 523  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 524  :         iTotal += *pdwT;                pdwT = pdwBase;
; 525  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 526  :         iTotal += *pdwT;                pdwT = pdwBase;
; 527  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 528  :         iTotal += *pdwT;            }

    ldr         r1, [r3pdwT]
    subs        r6iRptDiv,r6iRptDiv,#1
    ldr         r2, [lrpdwT1]
    add         r0iTotal, r0iTotal, r7
    ldr         r7, [r3pdwT]
    add         r9iTotal, r9iTotal, r8
    ldr         r8, [lrpdwT1]
    add         r0iTotal, r0iTotal, r1
    ldr         r1, [r3pdwT]
    add         r9iTotal, r9iTotal, r2
    ldr         r2, [lrpdwT1]
    add         r0iTotal, r0iTotal, r7
    ldr         r7, [r3pdwT]
    add         r9iTotal, r9iTotal, r8
    ldr         r8, [lrpdwT1]
    add         r0iTotal, r0iTotal, r1
    add         r9iTotal, r9iTotal, r2
    bne         |$LL6@iRead1D2|
|$LN4@iRead1D2|

; 529  :     for ( ; iRpt > 0; iRpt-- )

    cmp         r4iRptMod,#0
    ble         |$LN1@iRead1D2|


; 530  :     {   
    ALIGN       8
|$LL3@iRead1D2|

; 531  :         volatile const DWORD* pdwT = pdwBase;
; 532  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 533  :         iTotal += *pdwT;

    ldr         r1, [r3pdwT]
    subs        r4iRptMod,r4iRptMod,#1
    ldr         r2, [lrpdwT1]
    add         r0iTotal, r0iTotal, r1
    add         r9iTotal, r9iTotal, r2
    bgt         |$LL3@iRead1D2|
|$LN1@iRead1D2|

; 534  :     }
; 535  :     return iTotal;
; 536  : }

    add         r0iTotal, r0iTotal, r7
    add         r9iTotal, r9iTotal, r8
    add         r0iTotal, r0iTotal, r9iTotal
    ldmia       sp!, {r4-r9,pc}

    ENDP  ; |?iRead1D2@@YAHPBKHABUtStridePattern@@@Z|, iRead1D2


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1D4( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1D4@@YAHPBKHABUtStridePattern@@@Z| ; iRead1D4

r0iTotal        RN  r0
r5iRptmod4      RN  r5
r6iDWDelta      RN  r6
r7iDWStride     RN  r7
r8pdwT          RN  r8
r9iRepeat       RN  r9
r10iTotal       RN  r10

|?iRead1D4@@YAHPBKHABUtStridePattern@@@Z| PROC      ; iRead1D4

; 576  : {

|$LN15@iRead1D4|
    stmdb       sp!,{r4-r10,lr}
    mov         r9iRepeat, r1
    mov         r5iRptmod4, r1
    mov         r8pdwT, r0

; 577  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;
; 579  :     int iTotal = 0; 
; 580  :     int iRpt = ciRepeats;
;            const int ciDWDelta     = 3*ciDWStride

    ldr         r7iDWStride,[r2,#8]
    mov         r0iTotal, #0
    mov         r10iTotal,#0
    mov         r3, #0
    mov         r4, #0
    add         r6iDWDelta, r7iDWStride, r7iDWStride, lsl #1

; 581  :     for ( ; iRpt >= 4; iRpt -= 4)

    cmp         r9iRepeat, #4
    blt         |$LN4@iRead1D4|
    
    mov         r9iRepeat, r9iRepeat, lsr #2
    sub         r5iRptmod4, r5iRptmod4, r9iRepeat, lsl #2

; 582  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL6@iRead1D4|

; 584  :         volatile const DWORD* pdwT = pdwBase;
; 585  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 586  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 587  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 588  :         iTotal += *pdwT;                pdwT = pdwBase;
; 589  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 590  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 591  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 592  :         iTotal += *pdwT;                pdwT = pdwBase;
; 593  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 594  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 595  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 596  :         iTotal += *pdwT;                pdwT = pdwBase;
; 597  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 598  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 599  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 600  :         iTotal += *pdwT;

    ldr         r1, [r8pdwT], r7iDWStride, lsl #2
    subs        r9,r9,#1
    ldr         r2, [r8pdwT], r7iDWStride, lsl #2
    add         r0iTotal,  r0iTotal,  r3
    ldr         r3, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r4
    ldr         r4, [r8pdwT], -r6iDWDelta, lsl #2
    add         r0iTotal,  r0iTotal,  r1
    ldr         r1, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r2
    ldr         r2, [r8pdwT], r7iDWStride, lsl #2
    add         r0iTotal,  r0iTotal,  r3
    ldr         r3, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r4
    ldr         r4, [r8pdwT], -r6iDWDelta, lsl #2
    add         r0iTotal,  r0iTotal,  r1
    ldr         r1, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r2
    ldr         r2, [r8pdwT], r7iDWStride, lsl #2
    add         r0iTotal,  r0iTotal,  r3
    ldr         r3, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r4
    ldr         r4, [r8pdwT], -r6iDWDelta, lsl #2
    add         r0iTotal,  r0iTotal,  r1
    ldr         r1, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r2
    ldr         r2, [r8pdwT], r7iDWStride, lsl #2
    add         r0iTotal,  r0iTotal,  r3
    ldr         r3, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r4
    ldr         r4, [r8pdwT], -r6iDWDelta, lsl #2
    add         r0iTotal,  r0iTotal,  r1
    add         r10iTotal, r10iTotal, r2
    bne         |$LL6@iRead1D4|

; 580  :     int iRpt = ciRepeats;
; 581  :     for ( ; iRpt >= 4; iRpt -= 4)

|$LN4@iRead1D4|

; 601  :     }
; 602  :     for ( ; iRpt > 0; iRpt-- )

    cmp         r5iRptmod4,#0
    ble         |$LN1@iRead1D4|

; 603  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL3@iRead1D4|

; 604  :         volatile const DWORD* pdwT = pdwBase;
; 605  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 606  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 607  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 608  :         iTotal += *pdwT;

    ldr         r1, [r8pdwT], r7iDWStride, lsl #2
    subs        r5iRptmod4, r5iRptmod4, #1
    ldr         r2, [r8pdwT], r7iDWStride, lsl #2
    add         r0iTotal,  r0iTotal,  r3
    ldr         r3, [r8pdwT], r7iDWStride, lsl #2
    add         r10iTotal, r10iTotal, r4
    ldr         r4, [r8pdwT], -r6iDWDelta, lsl #2
    add         r0iTotal,  r0iTotal,  r1
    add         r10iTotal, r10iTotal, r2
    bgt         |$LL3@iRead1D4|
|$LN1@iRead1D4|

; 609  :     }
; 610  :     return iTotal;
; 611  : }

    add         r0iTotal,  r0iTotal,  r3
    add         r10iTotal, r10iTotal, r4
    add         r0iTotal, r0iTotal, r10iTotal
    ldmia       sp!, {r4-r10,pc}
|$M81292|

    ENDP  ; |?iRead1D4@@YAHPBKHABUtStridePattern@@@Z|, iRead1D4


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1D8( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1D8@@YAHPBKHABUtStridePattern@@@Z| ; iRead1D8

r0iTotal        RN  r0
r5iRepeat       RN  r5
r6iYdwStride    RN  r6
R7pdwBase       RN  r7
r8iTotal        RN  r8
lrpdwT          RN  lr

|?iRead1D8@@YAHPBKHABUtStridePattern@@@Z| PROC      ; iRead1D8

; 677  : {

|$LN10@iRead1D8|
    stmdb       sp!, {r4-r8,lr}
|$M81389|
    mov         r5iRepeat,r1
    mov         R7pdwBase,r0
    mov         lrpdwT, r0

; 678  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;
; 680  :     int iTotal = 0; 

    ldr         r6iYdwStride,[r2,#8]
    mov         r0iTotal,#0
    mov         r8iTotal,#0
    mov         r3, #0
    mov         r4, #0

; 681  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    cmp         r5iRepeat,#0
    ble         |$LN1@iRead1D8|

; 682  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL3@iRead1D8|

; 684  :         volatile const DWORD* pdwT = pdwBase;
; 685  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 686  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 687  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 688  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 689  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 690  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 691  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 692  :         iTotal += *pdwT;

    ldr         r1, [lrpdwT], r6iYdwStride, lsl #2
    subs        r5iRepeat, r5iRepeat, #1
    ldr         r2, [lrpdwT], r6iYdwStride, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT], r6iYdwStride, lsl #2
    add         r8iTotal, r8iTotal, r4
    ldr         r4, [lrpdwT], r6iYdwStride, lsl #2
    add         r0iTotal, r0iTotal, r1
    ldr         r1, [lrpdwT], r6iYdwStride, lsl #2
    add         r8iTotal, r8iTotal, r2
    ldr         r2, [lrpdwT], r6iYdwStride, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT], r6iYdwStride, lsl #2
    add         r8iTotal, r8iTotal, r4
    ldr         r4, [lrpdwT]
    add         r0iTotal, r0iTotal, r1
    mov         lrpdwT, R7pdwBase
    add         r8iTotal, r8iTotal, r2
    bne         |$LL3@iRead1D8|
|$LN1@iRead1D8|

; 693  :     }
; 694  :     return iTotal;
; 695  : }

    add         r0iTotal, r0iTotal, r3
    add         r8iTotal, r8iTotal, r4
    add         r0iTotal, r0iTotal, r8iTotal
    ldmia       sp!, {r4-r8,pc}
|$M81390|

    ENDP  ; |?iRead1D8@@YAHPBKHABUtStridePattern@@@Z|, iRead1D8


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1D12( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1D12@@YAHPBKHABUtStridePattern@@@Z| ; iRead1D12

r0iTotal        RN  r0
r7iTotal        RN  r7
r8iRepeats      RN  r8
r10pdwBase      RN  r10
R11iDWStride    RN  r11
lrpdwT          RN  lr

|?iRead1D12@@YAHPBKHABUtStridePattern@@@Z| PROC     ; iRead1D12

; 774  : {

    stmdb       sp!, {r4-r11,lr}
    mov         r8iRepeats,r1
    mov         r10pdwBase, r0

; 775  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;
; 777  :     int iTotal = 0; 

    ldr         R11iDWStride,[r2,#8]
    mov         r0iTotal, #0
    mov         r7iTotal, #0
    mov         r4, #0
    mov         r5, #0
    mov         r6, #0

; 778  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    cmp         r8iRepeats, #0
    mov         lrpdwT, r10pdwBase
    ble         |$LN1@iRead1D12|

; 779  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL3@iRead1D12|

; 781  :         volatile const DWORD* pdwT = pdwBase;
; 782  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 783  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 784  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 785  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 786  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 787  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 788  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 789  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 790  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 791  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 792  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 793  :         iTotal += *pdwT;

    ldr         r1, [lrpdwT], R11iDWStride, lsl #2
    subs        r8iRepeats,r8iRepeats,#1
    ldr         r2, [lrpdwT], R11iDWStride, lsl #2
    add         r0iTotal, r0iTotal, r4
    ldr         r3, [lrpdwT], R11iDWStride, lsl #2
    add         r7iTotal, r7iTotal, r5
    ldr         r4, [lrpdwT], R11iDWStride, lsl #2
    add         r0iTotal, r0iTotal, r6
    ldr         r5, [lrpdwT], R11iDWStride, lsl #2
    add         r7iTotal, r7iTotal, r1
    ldr         r6, [lrpdwT], R11iDWStride, lsl #2
    add         r0iTotal, r0iTotal, r2
    ldr         r1, [lrpdwT], R11iDWStride, lsl #2
    add         r7iTotal, r7iTotal, r3
    ldr         r2, [lrpdwT], R11iDWStride, lsl #2
    add         r0iTotal, r0iTotal, r4
    ldr         r3, [lrpdwT], R11iDWStride, lsl #2
    add         r7iTotal, r7iTotal, r5
    ldr         r4, [lrpdwT], R11iDWStride, lsl #2
    add         r0iTotal, r0iTotal, r6
    ldr         r5, [lrpdwT], R11iDWStride, lsl #2
    add         r7iTotal, r7iTotal, r1
    ldr         r6, [lrpdwT]
    add         r0iTotal, r0iTotal, r2
    add         r7iTotal, r7iTotal, r3
    mov         lrpdwT, r10pdwBase
    bne         |$LL3@iRead1D12|
|$LN1@iRead1D12|

; 794  :     }
; 795  :     return iTotal;
; 796  : }

    add         r7iTotal, r7iTotal, r4
    add         r0iTotal, r0iTotal, r5
    add         r7iTotal, r7iTotal, r6
    add         r0iTotal, r0iTotal, r7iTotal
    ldmia       sp!, {r4-r11,pc}
|$M81512|

    ENDP  ; |?iRead1D12@@YAHPBKHABUtStridePattern@@@Z|, iRead1D12


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1D16( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1D16@@YAHPBKHABUtStridePattern@@@Z| ; iRead1D16

r0iTotal        RN  r0
r5iRepeats      RN  r5
r6iDWStride     RN  r6
r7pdwBase       RN  r7
lrpdwT          RN  lr

|?iRead1D16@@YAHPBKHABUtStridePattern@@@Z| PROC     ; iRead1D16

; 887  : {

|$LN10@iRead1D16|
    stmdb       sp!, {r4-r7,lr}
|$M81657|
    mov         r5iRepeats, r1
    mov         r7pdwBase, r0

; 888  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;
; 890  :     int iTotal = 0; 

    ldr         r6iDWStride,[r2,#8]
    mov         r0,#0

; 891  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    cmp         r5iRepeats,#0
    mov         lrpdwT, r7pdwBase
    ble         |$LN1@iRead1D16|

; 892  :     {   
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL3@iRead1D16|

; 894  :         volatile const DWORD* pdwT = pdwBase;
; 895  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 896  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 897  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 898  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 899  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 900  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 901  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 902  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 903  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 904  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 905  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 906  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 907  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 908  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 909  :         iTotal += *pdwT;                pdwT += ciDWStride;
; 910  :         iTotal += *pdwT;

    ldr         r1, [lrpdwT], r6iDWStride, lsl #2
    ldr         r2, [lrpdwT], r6iDWStride, lsl #2
    subs        r5iRepeats, r5iRepeats, #1
    ldr         r3, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r1
    ldr         r4, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r2
    ldr         r1, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r3
    ldr         r2, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r4
    ldr         r3, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r1
    ldr         r4, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r2
    ldr         r1, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r3
    ldr         r2, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r4
    ldr         r3, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r1
    ldr         r4, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r2
    ldr         r1, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r3
    ldr         r2, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r4
    ldr         r3, [lrpdwT], r6iDWStride, lsl #2
    add         r0iTotal,r0iTotal,r1
    ldr         r4, [lrpdwT], r6iDWStride, lsl #2
    mov         lrpdwT, r7pdwBase
    add         r0iTotal,r0iTotal,r2
    add         r3, r3, r4
    add         r0iTotal,r0iTotal,r3
    bne         |$LL3@iRead1D16|
|$LN1@iRead1D16|

; 911  :     }
; 912  :     return iTotal;
; 913  : }

    ldmia       sp!, {r4-r7,pc}
|$M81658|

    ENDP  ; |?iRead1D16@@YAHPBKHABUtStridePattern@@@Z|, iRead1D16


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; extern int iRead1DBig( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead1DBig@@YAHPBKHABUtStridePattern@@@Z| ; iRead1DBig

r0iTotal        RN  r0
r5iStrideCount  RN  r5
r6iDWStride     RN  r6
r6iDWStride2    RN  r6
r7iTotal        RN  r7
r8iRepeats      RN  r8
r9i             RN  r9
r10pdwBase      RN  r10
r11pdwT2        RN  r11
lrpdwT          RN  lr

|?iRead1DBig@@YAHPBKHABUtStridePattern@@@Z| PROC    ; iRead1DBig

; 917  : {

|$LN26@iRead1DBig|
    stmdb       sp!, {r4-r11,lr}
    mov         r8iRepeats,r1
    mov         r10pdwBase,r0

; 918  :     const int ciStrideCount = ctStrideInfo.m_iYStrides;
; 919  :     const int ciDWStride    = ctStrideInfo.m_iYdwStride;

    ldr         r5iStrideCount, [r2,#4]
    ldr         r6iDWStride,    [r2,#8]

; 921  :     int iTotal = 0; 
; 922  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    cmp         r8iRepeats,#0
    mov         r0iTotal,#0
    mov         r7iTotal,#0
    ; make the stride double for the use of two alternating pointers striding double below
    mov         r6iDWStride2, r6iDWStride, lsl #1
    mov         r3, #0
    mov         r4, #0
    ble         |$LN10@iRead1DBig|

; 923  :     {   
|$LL12@iRead1DBig|

; 924  :         volatile const DWORD* pdwT = pdwBase;
; 925  :         int i = ciStrideCount;
; 926  :         for ( ; i >= 16; i -= 16)

    ; second pointer to address every other access to avoid pipeline stalls
    mov         lrpdwT, r10pdwBase
    add         r11pdwT2, r10pdwBase, r6iDWStride2, lsl #1      ; single stride forward
    
    cmp         r5iStrideCount, #0x10
    mov         r9i, r5iStrideCount, lsr #4                     ; iStrideCount div 16
    blt         |$LN7@iRead1DBig|

; 927  :         {
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL9@iRead1DBig|

; 929  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 930  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 931  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 932  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 933  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 934  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 935  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 936  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 937  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 938  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 939  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 940  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 941  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 942  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 943  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 944  :             iTotal += *pdwT;            pdwT += ciDWStride;

    ldr         r1, [lrpdwT],   r6iDWStride2, lsl #2
    subs        r9i,r9i,#1
    ldr         r2, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r4
    ldr         r4, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r1
    ldr         r1, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r2
    ldr         r2, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r4
    ldr         r4, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r1
    ldr         r1, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r2
    ldr         r2, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r4
    ldr         r4, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r1
    ldr         r1, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r2
    ldr         r2, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r4
    ldr         r4, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r1
    add         r7iTotal, r7iTotal, r2
    bne         |$LL9@iRead1DBig|

; 945  :         }
; 946  :         for ( ; i >= 4; i -= 4)

|$LN7@iRead1DBig|
    and         r9i, r5iStrideCount, #0xF           ; iStrideCount mod 16
    cmp         r9i, #4
    mov         r9i, r9i, lsr #2                    ; (iStrideCount mod 16) div 4
    blt         |$LN23@iRead1DBig|

; 947  :         {
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL6@iRead1DBig|

; 949  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 950  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 951  :             iTotal += *pdwT;            pdwT += ciDWStride;
; 952  :             iTotal += *pdwT;            pdwT += ciDWStride;

    ldr         r1, [lrpdwT],   r6iDWStride2, lsl #2
    subs        r9i, r9i, #1
    ldr         r2, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r3
    ldr         r3, [lrpdwT],   r6iDWStride2, lsl #2
    add         r7iTotal, r7iTotal, r4
    ldr         r4, [r11pdwT2], r6iDWStride2, lsl #2
    add         r0iTotal, r0iTotal, r1
    add         r7iTotal, r7iTotal, r2
    bne         |$LL6@iRead1DBig|

    ands        r9i, r5iStrideCount, #3             ; iStrideCount mod 4
    beq         |$LLX@iRead1DBig|

; 953  :         }
; 954  :         for ( ; i > 0; i-- )
; 955  :         {
; 956  :             iTotal += *pdwT;            pdwT += ciDWStride;

|$LL3@iRead1DBig|
    ldr         r1, [lrpdwT], r6iDWStride2, lsl #1
    subs        r9i, r9i, #1
    add         r0iTotal, r0iTotal, r1
    bgt         |$LL3@iRead1DBig|
    
; 922  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

|$LLX@iRead1DBig|
    subs        r8iRepeats,r8iRepeats,#1
    bne         |$LL12@iRead1DBig|
    
; 957  :         }
; 958  :     }
; 959  :     return iTotal;
; 960  : }

|$LN10@iRead1DBig|
    add         r7iTotal, r7iTotal, r4
    add         r0iTotal, r0iTotal, r3
    add         r0iTotal, r0iTotal, r7iTotal
    ldmia       sp!, {r4-r11,pc}
    
    ; put this cold path outside the main flow
|$LN23@iRead1DBig|
    ands        r9i, r5iStrideCount, #3             ; iStrideCount mod 4
    bne         |$LL3@iRead1DBig|
    
    subs        r8iRepeats,r8iRepeats,#1
    bne         |$LL12@iRead1DBig|
    
    add         r7iTotal, r7iTotal, r4
    add         r0iTotal, r0iTotal, r3
    add         r0iTotal, r0iTotal, r7iTotal
    ldmia       sp!, {r4-r11,pc}

    ENDP  ; |?iRead1DBig@@YAHPBKHABUtStridePattern@@@Z|, iRead1DBig


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; some optimization notes
; On the Scorpion, there was no difference in L1 
    END
