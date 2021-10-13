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
    OPT 2   ; disable listing
    INCLUDE ksarm.h
    OPT 1   ; reenable listing
    OPT 128 ; disable listing of macro expansions


 IF Interworking :LOR: Thumbing

	MACRO
	RET
	bx lr
	MEND

 ELSE

	MACRO
	RET
	mov pc, lr
	MEND

 ENDIF

    TEXTAREA

    LEAF_ENTRY ThreadStopFunc
    DCD 0xE6000011  ; special undefined instruction
    mov pc, lr


	;
	; Code to extract register information
	;

	LEAF_ENTRY GetWr0
	mrrc p0, 0, r0, r1, c0
	RET

	LEAF_ENTRY GetWr1
	mrrc p0, 0, r0, r1, c1
	RET

	LEAF_ENTRY GetWr2
	mrrc p0, 0, r0, r1, c2
	RET

	LEAF_ENTRY GetWr3
	mrrc p0, 0, r0, r1, c3
	RET

	LEAF_ENTRY GetWr4
	mrrc p0, 0, r0, r1, c4
	RET

	LEAF_ENTRY GetWr5
	mrrc p0, 0, r0, r1, c5
	RET

	LEAF_ENTRY GetWr6
	mrrc p0, 0, r0, r1, c6
	RET

	LEAF_ENTRY GetWr7
	mrrc p0, 0, r0, r1, c7
	RET

	LEAF_ENTRY GetWr8
	mrrc p0, 0, r0, r1, c8
	RET

	LEAF_ENTRY GetWr9
	mrrc p0, 0, r0, r1, c9
	RET

	LEAF_ENTRY GetWr10
	mrrc p0, 0, r0, r1, c10
	RET

	LEAF_ENTRY GetWr11
	mrrc p0, 0, r0, r1, c11
	RET

	LEAF_ENTRY GetWr12
	mrrc p0, 0, r0, r1, c12
	RET

	LEAF_ENTRY GetWr13
	mrrc p0, 0, r0, r1, c13
	RET

	LEAF_ENTRY GetWr14
	mrrc p0, 0, r0, r1, c14
	RET

	LEAF_ENTRY GetWr15
	mrrc p0, 0, r0, r1, c15
	RET

	;
	; Mini snippets of code to set the concan registers
	;

	LEAF_ENTRY SetWr0
	mcrr p0, 0, r0, r1, c0
	RET

	LEAF_ENTRY SetWr1
	mcrr p0, 0, r0, r1, c1
	RET

	LEAF_ENTRY SetWr2
	mcrr p0, 0, r0, r1, c2
	RET

	LEAF_ENTRY SetWr3
	mcrr p0, 0, r0, r1, c3
	RET

	LEAF_ENTRY SetWr4
	mcrr p0, 0, r0, r1, c4
	RET

	LEAF_ENTRY SetWr5
	mcrr p0, 0, r0, r1, c5
	RET

	LEAF_ENTRY SetWr6
	mcrr p0, 0, r0, r1, c6
	RET

	LEAF_ENTRY SetWr7
	mcrr p0, 0, r0, r1, c7
	RET

	LEAF_ENTRY SetWr8
	mcrr p0, 0, r0, r1, c8
	RET

	LEAF_ENTRY SetWr9
	mcrr p0, 0, r0, r1, c9
	RET

	LEAF_ENTRY SetWr10
	mcrr p0, 0, r0, r1, c10
	RET

	LEAF_ENTRY SetWr11
	mcrr p0, 0, r0, r1, c11
	RET

	LEAF_ENTRY SetWr12
	mcrr p0, 0, r0, r1, c12
	RET

	LEAF_ENTRY SetWr13
	mcrr p0, 0, r0, r1, c13
	RET

	LEAF_ENTRY SetWr14
	mcrr p0, 0, r0, r1, c14
	RET

	LEAF_ENTRY SetWr15
	mcrr p0, 0, r0, r1, c15
	RET

    END
