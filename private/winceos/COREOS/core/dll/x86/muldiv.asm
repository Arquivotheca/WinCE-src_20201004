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
        title   "Multiply and Divide"
;++
;
;  Module Name:
;
;     muldiv.asm
;
;  Abstract:
;
;     This module implements the "Multiply and Divide" operation
;
;  Environment:
;
;     Any mode.
;
;--

.486p

_TEXT   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page , 132
        subttl  "Multiply and Divide"
;++
;
; LONG
; __cdecl
; MulDiv(
;    IN LONG nNumber,
;    IN LONG nNumerator,
;    IN LONG nDenominator
;    )
;
; Routine Description:
;
;    This function multiples two 32-bit numbers forming a 64-bit product.
;    The 64-bit product is rounded and then divided by a 32-bit divisor
;    yielding a 32-bit result.
;
; Arguments:
;
;    nNumber - Supplies the multiplier.
;
;    nNumerator - Supplies the multiplicand.
;
;    nDenominator - Supplies the divisor.
;
; Return Value:
;
;    If the divisor is zero or an overflow occurs, then a value of -1 is
;    returned as the function value. Otherwise, the rounded quotient is
;    returned as the function value.
;
;--

nNumber      equ [esp + 4]
nNumerator   equ [esp + 8]
nDenominator equ DWORD PTR [esp + 12]

_MulDiv PROC PUBLIC
.FPO ( 3, 0, 0, 0, 0, 0 )

        mov     eax, nNumber            ; get multiplier absolute value
        or      eax, eax                ;
        js      short MD32_First        ; if s, multiplier is negative

;
; The multiplier is positive.
;

        mov     edx, nNumerator         ; get multiplicand absolute value
        or      edx, edx                ;
        js      MD32_Second             ; if s, multiplicand is negative

;
; The multiplicand is positive.
;

        mul     edx                     ; compute 64-bit product
        mov     ecx, nDenominator       ; get denominator absolute value
        or      ecx, ecx                ;
        js      MD32_Third              ; if s, divisor is negative

;
; The divisor is positive.
;

        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        cmp     edx, nDenominator       ; check for overflow
        jae     short MD32_error        ; if ae, overflow or divide by 0
        div     nDenominator            ; compute quotient

;
; The result is postive.
;

        or      eax, eax                ; check for overflow
        js      short MD32_error        ; if s, overlfow has occured

        ret     0

MD32_error:
        xor     eax, eax                ; set return value to - 1
        dec     eax                     ;

        ret     0

;
; The multiplier is negative.
;

MD32_First:                             ;
        neg     eax                     ; negate multiplier
        mov     edx, nNumerator         ; get multiplicand absolute value
        or      edx, edx                ;
        js      short MD32_First10      ; if s, multiplicand is negative

;
; The multiplicand is positive.
;

        mul     edx                     ; compute 64-bit product
        mov     ecx, nDenominator       ; get denominator absolute value
        or      ecx, ecx                ;
        js      short MD32_First20      ; if s, divisor is negative

;
; The divisor is positive.
;

        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        cmp     edx, nDenominator       ; check for overflow
        jae     short MD32_error10      ; if ae, overflow or divide by 0
        div     nDenominator            ; compute quotient

;
; The result is negative.
;

        neg     eax                     ; negate result
        jg      short MD32_error10      ; if g, overlfow has occured

        ret     0

;
; The multiplier is negative and the multiplicand is negative.
;

MD32_First10:                           ;
        neg     edx                     ; negate multiplicand
        mul     edx                     ; compute 64-bit product
        mov     ecx, nDenominator       ; get denominator absolute value
        or      ecx, ecx                ;
        js      short MD32_First30      ; if s, divisor is negative

;
; The divisor is positive.
;

        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        cmp     edx, nDenominator       ; check for overflow
        jae     short MD32_error10      ; if ae, overflow or divide by 0
        div     nDenominator            ; compute quotient

;
; The result is positive.
;

        or      eax, eax                ; check for overflow
        js      short MD32_error10      ; if s, overlfow has occured

        ret     0

MD32_error10:                           ;
        xor     eax, eax                ; set return value to - 1
        dec     eax                     ;

        ret     0


;
; The multiplier is negative, the multiplicand is positive, and the
; divisor is negative.
;

MD32_First20:                           ;
        neg     ecx                     ; negate divisor
        push    ecx                     ; save absolute value of divisor
        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        pop     ecx                     ; restore divisor
        cmp     edx, ecx                ; check for overflow
        jae     short MD32_error10      ; if ae, overflow or divide by 0
        div     ecx                     ; compute quotient

;
; The result is postive.
;

        or      eax, eax                ; check for overflow
        js      short MD32_error10      ; if s, overlfow has occured

        ret     0

;
; The multiplier is negative, the multiplier is negative, and the divisor
; is negative.
;

MD32_First30:                           ;
        neg     ecx                     ; negate divisor
        push    ecx                     ; save absolute value of divisor
        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        pop     ecx                     ; restore divisor
        cmp     edx, ecx                ; check for overflow
        jae     short MD32_error10      ; if ae, overflow or divide by 0
        div     ecx                     ; compute quotient

;
; The result is negative.
;

        neg     eax                     ; negate result
        jg      short MD32_error10      ; if g, overlfow has occured

        ret     0

;
; The multiplier is positive and the multiplicand is negative.
;

MD32_Second:                            ;
        neg     edx                     ; negate multiplicand
        mul     edx                     ; compute 64-bit product
        mov     ecx, nDenominator       ; get denominator absolute value
        or      ecx, ecx                ;
        js      short MD32_Second10     ; if s, divisor is negative

;
; The divisor is positive.
;

        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        cmp     edx, nDenominator       ; check for overflow
        jae     short MD32_error20      ; if ae, overflow or divide by 0
        div     nDenominator            ; compute quotient

;
; The result is negative.
;

        neg     eax                     ; check for overflow
        jg      short MD32_error20      ; if g, overlfow has occured

        ret     0

MD32_error20:                           ;
        xor     eax, eax                ; set return value to - 1
        dec     eax                     ;

        ret     0

;
; The multiplier is positive, the multiplicand is negative, and the divisor
; is negative.
;

MD32_Second10:                          ;
        neg     ecx                     ; negate divisor
        push    ecx                     ; save absolute value of divisor
        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        pop     ecx                     ; restore divisor
        cmp     edx, ecx                ; check for overflow
        jae     short MD32_error20      ; if ae, overflow or divide by 0
        div     ecx                     ; compute quotient

;
; The result is positive.
;

        or      eax, eax                ; check for overflow
        js      short MD32_error10      ; if s, overlfow has occured

        ret     0

;
; The multiplier is positive, the multiplicand is positive, the divisor
; is negative.
;

MD32_Third:                             ;
        neg     ecx                     ; negate divisor
        push    ecx                     ; save absolute value of divisor
        sar     ecx, 1                  ; compute rounding value
        add     eax, ecx                ; round the 64-bit produce by the
        adc     edx, 0                  ; divisor / 2
        pop     ecx                     ; restore divisor
        cmp     edx, ecx                ; check for overflow
        jae     short MD32_error20      ; if ae, overflow or divide by 0
        div     ecx                     ; compute quotient

;
; The result is negative.
;

        neg     eax                     ; negate result
        jg      short MD32_error20      ; if g, overflow has occured

        ret     0

_MulDiv ENDP

_TEXT   ENDS
        END
