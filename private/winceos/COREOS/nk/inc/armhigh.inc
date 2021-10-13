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

; Pre-V6 versions use subpage protection bits and multiple mappings to give some
; basic stack overflow protection. This is not possible with the new MMU format,
; so this code defines a simplified layout.
; There are alternative layouts which restore the unmapped gaps between the stack
; areas; I have used the simple version as it saves memory, and it is not clear that
; a data abort in exception handling will be cleanly handled.

; High memory layout:
;       FFFD0000 - first level page table (uncached)
;       FFFD4000 - not used
;       FFFE0000 - disabled for protection
;       FFFF0000 - exception vectors
;       FFFF03E0 - exception vector jump table
;       FFFF0400 - Interrupt stack (1k)
;       FFFF0800 - FIQ stack (256 bytes)
;       FFFF0900 - Abort stack (2k - 256 bytes)
;       FFFF1000 - disabled
;       FFFF7000 - disabled
;       FFFFC000 - kernel stack
;       FFFFC800 - KDataStruct
;       FFFFCC00 - r/o for protection (2nd level page table for 0xFFF00000)

        ^ 0xFFFD0000
FirstPT         # 0x4000
                # 0x4000
                # 0x8000
                # 0x10000       ; not mapped
ExVector        # 0x0400        ; vectors and table
                # 0x0400        ; 1K interrupt stack            (ffff0400)
IntStack        # 0x0100        ; 256 byte FIQ stack            (ffff0800)
FIQStack        # 0x0700        ; 2K-256 abort stack            (ffff0900)
AbortStack      # 0xC000-0x1000 ; not mapped                    (ffff1000)
KDBase          # 0x07E0        ; 2K-32 kernel stack
KStack          # 0x0020        ; temporary register save area
KData           # 0x400         ; kernel data area


        END

