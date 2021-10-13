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
        page    ,132
        title   enable - enable/disable interrupts
;***
;enable.asm - contains _enable() and _disable() routines
;
;       Copyright (c) Microsoft Corporation. All rights reserved.
;
;Purpose:
;
;Revision History:
;       06-15-93  GJF   Module created.
;       10-25-93  GJF   Added ifndef to ensure this is not part of the NT
;                       SDK build.
;
;*******************************************************************************

ifndef  _NTSDK

        .xlist
        include cruntime.inc
        .list

page
;***
;void _enable(void)  - enables interrupts
;void _disable(void) - disables interrupts
;
;Purpose:
;       The _enable() functions executes a "sti" instruction. The _disable()
;       function executes a "cli" instruction.
;
;Entry:
;
;Exit:
;
;Uses:
;
;Exceptions:
;
;*******************************************************************************


        CODESEG

        public  _enable, _disable

_enable proc

        sti
        ret

_enable endp

        align   4

_disable proc

        cli
        ret

_disable endp

endif   ; _NTSDK

        end
