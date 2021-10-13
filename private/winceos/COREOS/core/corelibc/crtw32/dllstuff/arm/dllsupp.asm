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
;	Copyright (c) 1992-2001, Microsoft Corporation. All rights reserved.
;
;Purpose:
;	Provides definitions for public constants (absolutes) that are
;	'normally' defined in objects in the C library, but must be defined
;	here for clients of msvcr*.dll.  These constants are:
;
;			    _fltused
;
;Revision History:
;	Resides in source code control in the 21st century
;
;*******************************************************************************

       TTL  "Absolutele symbol definitions for CRT build used for object selection in static builds"

    EXPORT _fltused
_fltused   EQU     1

    END
