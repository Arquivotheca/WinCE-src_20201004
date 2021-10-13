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
;***
;dllsupp.asm - Definitions of public constants
;
;	Copyright (c) Microsoft Corporation. All rights reserved.
;
;Purpose:
;	Provides definitions for public constants (absolutes) that are
;	'normally' defined in objects in the C library, but must be defined
;	here for clients of crtdll.dll & msvcrt*.dll.  These constants are:
;
;			    _except_list
;			    _fltused
;			    _ldused
;
;Revision History:
;	01-23-92  GJF	Module created.
;
;*******************************************************************************

	.686
	.model	flat,c

; offset, with respect to FS, of pointer to currently active exception handler.
; referenced by compiler generated code for SEH and by _setjmp().

	public	_except_list
_except_list	equ	0

	public	_fltused
_fltused	equ	9876h

	public	_ldused
_ldused 	equ	9876h

	end
