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
    TTL    Interlock memory operations
;++
;
; Module Name:
;    armpcb.s
;
; Abstract:
;    This module implements functions to access PCB.
;
; Environment:
;    Kernel mode or User mode.
;
;--
        OPT     2           ; disable listing
        INCLUDE ksarm.h
        OPT     1           ; reenable listing


;-----------------------------------------------------------------------
; Globals (static)
        AREA |.data|, DATA

PCBFunctions
                DCD         SingleCoreGetPCB
                DCD         SingleCorePCBGetDwordAtOffset
                DCD         SingleCorePCBSetDwordAtOffset
                DCD         SingleCorePCBAddAtOffset

GETPCB          EQU         0
PCBGETDWORD     EQU         4
PCBSETDWORD     EQU         8
PCBADD          EQU         12

;
; Depenedeing on the number of CPUs, PCB operations are implemented differently.
; - if there are more than 1 CPUs, we load it directly from KData.
; - if there is only 1 CPU, we always jump to know address, where kernel can restart instruction when interrupted.
;   NOTE: (r3) must be 0 before the jump - it'll be used as restart indicator.
;

        TEXTAREA

;;
;;--------------------------------------------------------------------------------------
;; Retrieve PCB related information for single core (private)
;;--------------------------------------------------------------------------------------
;;
        LEAF_ENTRY PCBFunctionArea

SingleCorePCBGetDwordAtOffset
        ldr     r12, =USER_KPAGE
        ldr     r0, [r12, r0]
        bx      lr


;;
;;--------------------------------------------------------------------------------------
;;
SingleCorePCBSetDwordAtOffset
        ldr     r12, =USER_KPAGE
        str     r0, [r12, r1]
        bx      lr

;;
;;--------------------------------------------------------------------------------------
;;
SingleCorePCBAddAtOffset
        ldr     r12, =USER_KPAGE
        ldr     r2, [r12, r1]               ; (r2) = original value
        add     r2, r2, r0                  ; (r2) += addent
        str     r2, [r12, r1]               ; store to kdata
        bx      lr

;;
;;--------------------------------------------------------------------------------------
;;
SingleCoreGetPCB
        ldr     r0, =USER_KPAGE
        bx      lr




;;--------------------------------------------------------------------------------------
;; Retrieve PCB related information for multi core (private)
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY MultiCorePCBGetDwordAtOffset
        ldr     r12, =USER_KPAGE
        mov     r3, #0
        add     pc, r12, #PCB_GETDWORD-USER_KPAGE

;;
;;--------------------------------------------------------------------------------------
;;
MultiCorePCBSetDwordAtOffset

        mrs     r3, cpsr                        ; (r3) = current status
        orr     r2, r3, #0x80                    
        msr     cpsr, r2                        ; INT off

        mrc     p15, 0, r2, c13, c0, 3          ; 0x444: (r2) = ppcb
        str     r0, [r1, r2]                    ; 0x448: ((LPDWORD)ppcb)[offset] = r1

        msr     cpsr, r3                        ; restore status
        bx      lr                              ; 0x450:

;;
;;--------------------------------------------------------------------------------------
;;
MultiCorePCBAddAtOffset

        mrs     r3, cpsr                        ; (r3) = current status
        orr     r2, r3, #0x80                    
        msr     cpsr, r2                        ; INT off

        mrc     p15, 0, r2, c13, c0, 3          ; 0x464: (r2) = ppcb
        ldr     r12, [r1, r2]                   ; 0x468: (r12) = ((LPDWORD)ppcb)[offset]
        add     r12, r12, r0                    ; 0x46c: (r12) += (r0)
        str     r12, [r1, r2]                   ; 0x470: ((LPDWORD)ppcb)[offset] = (r12)

        msr     cpsr, r3                        ; restore status
        bx      lr                              ; 0x478:

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY MultiCoreGetPCB
        mrc     p15, 0, r0, c13, c0, 3              ; (r0) = ppcb
        bx      lr



;;
;;--------------------------------------------------------------------------------------
;;
;;  Exported PCB functions
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY __PCBGetDwordAtOffset

        ldr     r12, =PCBFunctions
        ldr     pc, [r12, #PCBGETDWORD]

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY PCBSetDwordAtOffset

        ldr     r12, =PCBFunctions
        ldr     pc, [r12, #PCBSETDWORD]

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY PCBAddAtOffset

        ldr     r12, =PCBFunctions
        ldr     pc, [r12, #PCBADD]


;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY __GetPCB
        
        ldr     r12, =PCBFunctions
        ldr     pc, [r12, #GETPCB]


;;
;;--------------------------------------------------------------------------------------
;;
;;
;;
        ALTERNATE_ENTRY InitPCBFunctions

        ldr     r12, =USER_KPAGE
        ldr     r3, [r12, #ADDR_NUM_CPUS-USER_KPAGE]
        cmp     r3, #1
        bxeq    lr

        ;
        ; more than one core
        ;
        ldr     r12, =PCBFunctions
        ldr     r0,  =MultiCoreGetPCB
        ldr     r1,  =MultiCorePCBGetDwordAtOffset
        ldr     r2,  =MultiCorePCBSetDwordAtOffset
        ldr     r3,  =MultiCorePCBAddAtOffset

        str     r0, [r12, #GETPCB]
        str     r1, [r12, #PCBGETDWORD]
        str     r2, [r12, #PCBSETDWORD]
        str     r3, [r12, #PCBADD]

        bx      lr

        ENTRY_END


        END

