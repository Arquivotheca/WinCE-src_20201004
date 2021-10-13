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

    TTL private\test\BaseOS\pqoal\cetk\MemoryPerf\Read2D.s
    CODE32

    AREA    |.drectve|, DRECTVE
    DCB "-defaultlib:LIBCMT "
    DCB "-defaultlib:OLDNAMES "


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; int iRead2D4( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead2D4@@YAHPBKHABUtStridePattern@@@Z| ; iRead2D4
    AREA    |.text|, CODE, ARM, ALIGN=3

r0iTotal        RN  r0
r5iY            RN  r5
r6iYStrideCount RN  r6
r7iRepeats      RN  r7
r8iY            RN  r8
r9iYdwDelta4    RN  r9
r10pdwBase      RN  r10
r11iXdwStride   RN  r11
lrpdwY          RN  lr


|?iRead2D4@@YAHPBKHABUtStridePattern@@@Z| PROC      ; iRead2D4

; 197  : {

    stmdb       sp!, {r4-r11,lr}
    mov         r7iRepeats,r1
    mov         r10pdwBase,r0

; 198  :     const int ciYStrideCount = ctStrideInfo.m_iYStrides;
; 199  :     const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
; 200  :     const int ciXStrideCount = ctStrideInfo.m_iXStrides;
; 201  :     const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
; 202  :     const int ciYdwDelta4    = ciYdwStride-3*ciXdwStride;
; 205  :     int iTotal = 0; 

    ldr         r11iXdwStride,[r2,#0x10]
    ldr         lr,[r2,#8]
    ldr         r6iYStrideCount,[r2,#4]
    add         r3,r11iXdwStride,r11iXdwStride,lsl #1
    sub         r9iYdwDelta4,lr,r3
    mov         r0iTotal,#0

; 206  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    cmp         r7iRepeats,#0
    ble         |$LN7@iRead2D4|
|$LL9@iRead2D4|

; 207  :     {   
; 208  :         // these pointers should be volatile but the current compiler adds three stack references per loop
; 209  :         // which defeats the purpose of this function.
; 210  :         // after building, examine the code in LibBld\Obj\Arm*\ReadLoop2D.asm
; 211  :         const DWORD* pdwY = pdwBase;
; 212  :         int iY = ciYStrideCount;
; 213  :         for ( ; iY >= 4; iY -= 4)

    mov         lrpdwY,r10pdwBase
    mov         r5iY,r6iYStrideCount
    cmp         r6iYStrideCount,#4
    blt         |$LN18@iRead2D4|
    mov         r8iY,r6iYStrideCount, lsr #2
    sub         r5iY,r6iYStrideCount,r8iY,lsl #2
    
; 214  :         {
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL6@iRead2D4|

; 216  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 217  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 218  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 219  :             iTotal += *pdwY;      pdwY += ciYdwDelta4;
; 220  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 221  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 222  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 223  :             iTotal += *pdwY;      pdwY += ciYdwDelta4;
; 224  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 225  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 226  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 227  :             iTotal += *pdwY;      pdwY += ciYdwDelta4;
; 228  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 229  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 230  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 231  :             iTotal += *pdwY;      pdwY += ciYdwDelta4;
; 239  :             iTotal += *pdwY;      pdwY += ciYdwDelta4;

    ldr         r1, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    ldr         r2, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    subs        r8iY, r8iY, #1
    ldr         r3, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r1
    ldr         r4, [lrpdwY], r9iYdwDelta4, lsl #2          ; post index
    add         r0iTotal, r0iTotal, r2
    ldr         r1, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r3
    ldr         r2, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r4
    ldr         r3, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r1
    ldr         r4, [lrpdwY], r9iYdwDelta4, lsl #2          ; post index
    add         r0iTotal, r0iTotal, r2
    ldr         r1, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r3
    ldr         r2, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r4
    ldr         r3, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r1
    ldr         r4, [lrpdwY], r9iYdwDelta4, lsl #2          ; post index
    add         r0iTotal, r0iTotal, r2
    ldr         r1, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r3
    ldr         r2, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r4
    ldr         r3, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal, r1
    ldr         r4, [lrpdwY], r9iYdwDelta4, lsl #2          ; post index
    add         r0iTotal, r0iTotal, r2
    add         r3, r3, r4
    add         r0iTotal, r0iTotal, r3
    bne         |$LL6@iRead2D4|

    cmp         r5iY,#0
    ble         |$LN18@iRead2D4|

; 232  :         }
; 233  :         for ( ; iY > 0; iY-- )
; 234  :         {
    ;;; The following loop 
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL3@iRead2D4|

; 236  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 237  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 238  :             iTotal += *pdwY;      pdwY += ciXdwStride;
; 239  :             iTotal += *pdwY;      pdwY += ciYdwDelta4;

    ldr         r1, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    ldr         r2, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    subs        r5iY, r5iY, #1
    ldr         r3, [lrpdwY], r11iXdwStride, lsl #2         ; post index
    add         r0iTotal, r0iTotal ,r1
    ldr         r4, [lrpdwY], r9iYdwDelta4, lsl #2          ; post index
    add         r0iTotal, r0iTotal, r2
    add         r3, r3, r4
    add         r0iTotal, r0iTotal, r3
    bgt         |$LL3@iRead2D4|
    

; 206  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

|$LN18@iRead2D4|
    subs        r7iRepeats, r7iRepeats, #1
    bne         |$LL9@iRead2D4|
    
; 240  :         }
; 241  :     }
; 242  :     return iTotal;

|$LN7@iRead2D4|
    ldmia       sp!, {r4-r11,pc}
; 243  : }


    ENDP  ; |?iRead2D4@@YAHPBKHABUtStridePattern@@@Z|, iRead2D4


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; int iRead2DBig( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo );

    EXPORT  |?iRead2DBig@@YAHPBKHABUtStridePattern@@@Z| ; iRead2DBig

; register assignment
r0iTotal        RN  r0
r5pdwX          RN  r5
r6iXdwStride    RN  r6
r7iXStrideCount RN  r7
r8iY            RN  r8
r9iX            RN  r9
r10pdwY         RN  r10
r11iRepeats     RN  r11
lriYdwStride    RN  lr

; [sp]      unused
; [sp+4]    ciYStrideCount
; [sp+8]    pdwBase

|?iRead2DBig@@YAHPBKHABUtStridePattern@@@Z| PROC    ; iRead2DBig

; 246  : {

    stmdb       sp!, {r4-r11,lr}
    sub         sp,sp,#0xC
    mov         r10pdwY, r0             ; pdwBase
    str         r0,[sp,#8]              ; [sp+8]    pdwBase
    mov         r11iRepeats, r1         ; ciRepeats

; 247  :     const int ciYStrideCount = ctStrideInfo.m_iYStrides;
; 248  :     const int ciYdwStride    = ctStrideInfo.m_iYdwStride;
; 249  :     const int ciXStrideCount = ctStrideInfo.m_iXStrides;
; 250  :     const int ciXdwStride    = ctStrideInfo.m_iXdwStride;
; 252  :     int iTotal = 0; 

    ldr         r8iY,[r2,#4]            ; ctStrideInfo.m_iYStrides
    ldr         lriYdwStride,[r2,#8]    ; ctStrideInfo.m_iYdwStride
    ldr         r7iXStrideCount,[r2,#0xC]
    ldr         r6iXdwStride,[r2,#0x10]
    str         r8iY,[sp,#4]            ; [sp+4]    ciYStrideCount
    mov         r0iTotal,#0
    mov         r3, #0
    mov         r4, #0

; 253  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    cmp         r11iRepeats,#0
    ble         |$LN13@iRead2DBig|
|$LL15@iRead2DBig|

; 254  :     {   
; 255  :         volatile const DWORD* pdwY = pdwBase;
; 256  :         int iY = ciYStrideCount;
; 257  :         for( ; iY > 0; iY--, pdwY += ciYdwStride )

    cmp         r8iY,#0
    ble         |$LN14@iRead2DBig|
|$LL12@iRead2DBig|

; 258  :         {
; 259  :             int iX = ciXStrideCount;
; 260  :             volatile const DWORD* pdwX = pdwY;

    mov         r5pdwX, r10pdwY

; 261  :             for ( ; iX >= 16; iX -= 16)

    cmp         r7iXStrideCount, #0x10
    blt         |$LN7@iRead2DBig|
    mov         r9iX, r7iXStrideCount, lsr #4           ; iXStrideCount Div 16
    
; 262  :             {
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL9@iRead2DBig|

; 264  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 265  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 266  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 267  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 268  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 269  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 270  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 271  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 272  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 273  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 274  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 275  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 276  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 277  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 278  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 279  :                 iTotal += *pdwX;            pdwX += ciXdwStride;

    ldr         r1, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    subs        r9iX,r9iX,#1
    ldr         r2, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r3
    ldr         r3, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r4
    ldr         r4, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r1
    ldr         r1, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r2
    ldr         r2, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r3
    ldr         r3, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r4
    ldr         r4, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r1
    ldr         r1, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r2
    ldr         r2, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r3
    ldr         r3, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r4
    ldr         r4, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r1
    ldr         r1, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r2
    ldr         r2, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r3
    ldr         r3, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r4
    ldr         r4, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r1
    add         r0iTotal,r0iTotal,r2
    bne         |$LL9@iRead2DBig|

; 280  :             }
|$LN7@iRead2DBig|

; 281  :             for ( ; iX >= 4; iX -= 4)

    and         r1, r7iXStrideCount, #0xF               ; iXStrideCount Mod 16
    cmp         r1, #4
    blt         |$LL6X@iRead2Big|
    mov         r9iX, r1, lsr #2                        ; (iXStrideCount mod 16) Div 4

; 282  :             {
    ;;; The following loop (which is the main work load of this routine)
    ;;; needs to be 8-byte aligned on some processors to pair instructions properly and predictably
    ALIGN       8
|$LL6@iRead2DBig|

; 284  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 285  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 286  :                 iTotal += *pdwX;            pdwX += ciXdwStride;
; 287  :                 iTotal += *pdwX;            pdwX += ciXdwStride;

    ldr         r1, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    subs        r9iX, r9iX, #1
    ldr         r2, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r3
    ldr         r3, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r4
    ldr         r4, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    add         r0iTotal,r0iTotal,r1
    add         r0iTotal,r0iTotal,r2
    bne         |$LL6@iRead2DBig|

|$LL6X@iRead2Big|
    ands        r9iX, r7iXStrideCount, #3               ; iXStrideCount Mod 4
    beq         |$LN28@iRead2DBig|

; 288  :             }
; 289  :             for ( ; iX > 0; iX-- )
; 290  :             {
; 291  :                 iTotal += *pdwX;            pdwX += ciXdwStride;

|$LL3@iRead2DBig|
    ldr         r1, [r5pdwX], r6iXdwStride, LSL #2      ; post index
    subs        r9iX, r9iX, #1
    add         r0iTotal, r0iTotal, r1
    bgt         |$LL3@iRead2DBig|
    
|$LN28@iRead2DBig|

; 292  :             }
; 257  :         for( ; iY > 0; iY--, pdwY += ciYdwStride )
; 260  :             volatile const DWORD* pdwX = pdwY;
    
    subs        r8iY, r8iY, #1
    add         r10pdwY, r10pdwY, lriYdwStride, lsl #2
    bgt         |$LL12@iRead2DBig|
    
; 253  :     for (int iRpt = 0; iRpt < ciRepeats; iRpt++)

    ldr         r8iY,[sp,#4]                ; [sp+4]    ciYStrideCount
    ldr         r10pdwY,[sp,#8]             ; [sp+8]    pdwBase

|$LN14@iRead2DBig|
    subs        r11iRepeats, r11iRepeats, #1
    bne         |$LL15@iRead2DBig|
|$LN13@iRead2DBig|

; 293  :         }
; 294  :     }
; 295  :     return iTotal;
; 296  : }

    add         r0iTotal, r0iTotal, r3
    add         r0iTotal, r0iTotal, r4
    add         sp, sp, #0xC
    ldmia       sp!, {r4-r11,pc}

    ENDP  ; |?iRead2DBig@@YAHPBKHABUtStridePattern@@@Z|, iRead2DBig

    END
