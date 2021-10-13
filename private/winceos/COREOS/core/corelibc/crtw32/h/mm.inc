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
;mm.inc - macros to write memory model dependent code
;
;	Copyright (c) Microsoft Corporation.  All rights reserved.
;
;Purpose:
;	This file contains definitions of a number of macros which
;	make the writing of memory model dependent code for the
;	386 a little easier and more portable.
;
;Revision History:
;	05-18-89  SKS	Removed ES references from pointer macros -- DS is used
;	09-01-89  GJF	Fixed copyright date.
;	11-19-93  SMK	Modified for 32 bit hosting.
;
;*******************************************************************************

; Big/Little Endian Definitions for Long Integers

LOWORD	equ	[0]
HIWORD	equ	[4]
