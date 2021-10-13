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

;
; Mark exception handlers written in C as safe so that the OS will dispatch
; to them.
;

PUBLIC __RTLSUP_MARK_HANDLERS_SAFE__
__RTLSUP_MARK_HANDLERS_SAFE__ EQU 0

ExceptionHandler PROTO
.SAFESEH ExceptionHandler

UnwindHandler PROTO
.SAFESEH UnwindHandler

END

