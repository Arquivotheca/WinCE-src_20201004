;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; This source code is licensed under Microsoft Shared Source License
; Version 1.0 for Windows CE.
; For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    ; DWORD GetCp15R15 ()
    ; This function returns the value of register 15 of coprocessor 15
    ; This is needed for Concan support
    ;
    LEAF_ENTRY GetCp15R15
    mrc p15, 0, r0, c15, c1, 0            ; Move to ARM Register from Coprocessor
                                          ; from Processor p0xf, register c15 to r0.  0s are undefed parameters
    RET

    ;
    ; DWORD GetwCID ()
    ; This function returns the value of the wCID register (c0) from the Concan Coprocessor.
    ;
    LEAF_ENTRY GetwCID
    mrc p1, 0, r0, c0, c0, 0              ; Move to ARM Register from Coprocessor
                                          ; from p0, reg 0, to r0. the 0s are undefed parameters
    RET


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

	LEAF_ENTRY GetConReg0
	mrc p1, 0, r0, c0, c0, 0
	RET

	LEAF_ENTRY GetConReg1
	mrc p1, 0, r0, c1, c0, 0
	RET

	LEAF_ENTRY GetConReg2
	mrc p1, 0, r0, c2, c0, 0
	RET

	LEAF_ENTRY GetConReg3
	mrc p1, 0, r0, c3, c0, 0
	RET

	LEAF_ENTRY GetConReg4
	mrc p1, 0, r0, c4, c0, 0
	RET

	LEAF_ENTRY GetConReg5
	mrc p1, 0, r0, c5, c0, 0
	RET

	LEAF_ENTRY GetConReg6
	mrc p1, 0, r0, c6, c0, 0
	RET

	LEAF_ENTRY GetConReg7
	mrc p1, 0, r0, c7, c0, 0
	RET

	LEAF_ENTRY GetConReg8
	mrc p1, 0, r0, c8, c0, 0
	RET

	LEAF_ENTRY GetConReg9
	mrc p1, 0, r0, c9, c0, 0
	RET

	LEAF_ENTRY GetConReg10
	mrc p1, 0, r0, c10, c0, 0
	RET

	LEAF_ENTRY GetConReg11
	mrc p1, 0, r0, c11, c0, 0
	RET

	LEAF_ENTRY GetConReg12
	mrc p1, 0, r0, c12, c0, 0
	RET

	LEAF_ENTRY GetConReg13
	mrc p1, 0, r0, c13, c0, 0
	RET

	LEAF_ENTRY GetConReg14
	mrc p1, 0, r0, c14, c0, 0
	RET

	LEAF_ENTRY GetConReg15
	mrc p1, 0, r0, c15, c0, 0
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

	; Coprocessor 1

	LEAF_ENTRY SetConReg0
	mcr p1, 0, r0, c0, c0, 0
	RET
	
	LEAF_ENTRY SetConReg1
	mcr p1, 0, r0, c1, c0, 0
	RET
	
	LEAF_ENTRY SetConReg2
	mcr p1, 0, r0, c2, c0, 0
	RET
	
	LEAF_ENTRY SetConReg3
	mcr p1, 0, r0, c3, c0, 0
	RET
	
	LEAF_ENTRY SetConReg4
	mcr p1, 0, r0, c4, c0, 0
	RET
	
	LEAF_ENTRY SetConReg5
	mcr p1, 0, r0, c5, c0, 0
	RET
	
	LEAF_ENTRY SetConReg6
	mcr p1, 0, r0, c6, c0, 0
	RET
	
	LEAF_ENTRY SetConReg7
	mcr p1, 0, r0, c7, c0, 0
	RET
	
	LEAF_ENTRY SetConReg8
	mcr p1, 0, r0, c8, c0, 0
	RET
	
	LEAF_ENTRY SetConReg9
	mcr p1, 0, r0, c9, c0, 0
	RET
	
	LEAF_ENTRY SetConReg10
	mcr p1, 0, r0, c10, c0, 0
	RET
	
	LEAF_ENTRY SetConReg11
	mcr p1, 0, r0, c11, c0, 0
	RET
	
	LEAF_ENTRY SetConReg12
	mcr p1, 0, r0, c12, c0, 0
	RET
	
	LEAF_ENTRY SetConReg13
	mcr p1, 0, r0, c13, c0, 0
	RET
	
	LEAF_ENTRY SetConReg14
	mcr p1, 0, r0, c14, c0, 0
	RET
	
	LEAF_ENTRY SetConReg15
	mcr p1, 0, r0, c15, c0, 0
	RET

	LEAF_ENTRY GetCp15R0
	mrc p15, 0, r0, c0, c0, 0             ; Move to ARM Register from Coprocessor
                                          ; from Processor p0xf, register c15 to r0.  0s are undefed parameters
    RET


    END
