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
;cruntime.inc - multi-model assembly macros for interfacing to HLLs
;
;	Copyright (c) Microsoft Corporation.  All rights reserved.
;
;Purpose:
;	This file defines the current memory model being used.
;
;Revision History:
;	08-04-88  SJM	Initial version to handle all four memory models
;			in 16-bit mode and small model in 32-bit mode
;	08-08-88  JCR	Added CPDIST, ?WIN, PCS, ISHIFT/LSHIFT, OS2,
;			DNPTR/DFPTR, DFLOAT/DDOUBLE/DLDOUBLE
;	08-16-88  PHG	Added FPES, LFPES, CBI, ZXAL, ZXBL, ZXCL, ZXDL
;	08-17-88  JCR	Added CAXDX, modified FPSIZE
;	08-20-88  PHG	Added diagnostic messages, removed 386 16-bit support
;			and 386 large code/data support, added mucho comments,
;			PSS now defined as es: only if SS_NEQ_GROUP defined
;	08-24-88  JCR	Added RBXSAVE and RBXONLY for use in 'proc uses'
;	08-25-88  JCR	Added savereg macro, removed rbxsave/rbxonly...
;	08-26-88  GJF	Added codeseg (text) macro
;	09-15-88  JCR	Corrected savelist/reglist macro to go with new MASM
;	09-21-88  WAJ	Added JS*, static*, global*, and label*, and lab macros
;	09-22-88  JCR	Change 'plm' to 'pascal' in label macro
;	09-26-88  WAJ	Added PUSH16 which will do a 16 bit push in a USE32 seg.
;	09-28-88  WAJ	Added CPWORD and DPWORD
;	09-29-88  WAJ	Added JMPFAR16 macro
;	10-12-88  JCR	Made PCS evaluate to 'cs:' for 16/32 stub testbed
;	04-24-89  JCR	Added 'assume seg:flat' for 386 to avoid masm/link bug
;	05-25-89  GJF	Added APIEXT, a macro that expands to the proper extrn
;			declaration for an API function
;	06-13-89  JCR	Added 'flat:' to DCPTR and DDPTR
;	09-15-89  JCR	Added DCPTR? and DDPTR?, always use "FLAT" not "flat"
;	09-06-94  CFW	Replace MTHREAD with _MT.
;	01-11-95  SKS	Remove MASM 5.X support
;       02-27-10  Intel Add ISA_AVAILABLE constants.
;
;*******************************************************************************

;==============================================================================
;
;Use the following defines to control processor/segment model
;
;   default is -DI86 -Dmem_S
;
;==============================================================================
;
;The following variables are defined by this file:
;   cpu 			86, 286, or 386
;   mmodel			english name of the memory model, i.e. "Medium"
;   ISIZE, LSIZE, NSIZE 	size of ints, longs, shorts
;   FLTSIZE, DBLSIZE, LDBLSIZE	size of float, double, long double
;
;The following macros allow easy writing of combined 16/32 bit code:
;
; 16/32 bit registers:
;   rax, rbx, rcx, rdx, 	expand to native registers (rax = eax or ax)
;   rsi, rdi, rsp, rbp
;   CBI 			convert byte to int (al to rax)
; Numeric type instructions:
;   IWORD, LWORD, SWORD 	data type of int, long, short
;   DFLOAT, DDOUBLE, DLDOUBLE	define float, double, long double
;
;The following utility macros are provided:
;   codeseg			define/declare code segment
;   error <msg> 		stop assembly with message
;   display <msg>		display a message, unless QUIET defined
;   _if cond <instruction>	assemble instruction only if cond is TRUE
;   _ife cond <instruction>	assemble instruction only if cond is FALSE
;   _ifd symbol <instruction>	assemble instruction only if symbol defined
;   _ifnd symbol <instruction>	assemble instruction only if symbol not defined
;
;   lab  LabelName		assembles to "LabelName:" If DEBUG is defined
;				LabelName is made public
;
;   JS* (ex. JSE,JSZ,JSB ...)	assemble to "je short","jz short","jb short"
;
;   Cmacro look alikes
;   static* Name, InitialValue, Repeat	 defines a static variable of type *
;   global* Name, InitialValue, Repeat	 defines a global variable of type *
;   label*  Name, {PUBLIC,PASCAL,C}	 defines a label of type *
;
;==============================================================================

; error <msg>	-    Output message and generate error

error	MACRO	msg
if2			;; only on pass 2 can we generate errors
	%out	**********************************************************
	%out	*** E r r o r  --  msg
	%out	**********************************************************
	.err
endif
        ENDM

; display msg	-    Output message unless QUIET defined

display MACRO	msg
ifndef QUIET		;; only when quiet flag not set
if1			;; and on pass 1, then display message
	%out msg
endif
endif
	ENDM

; One line conditionals:
;   here we create the capability of writing code lines like
;
; _if sizeD   <push ds>   as opposed to    if sizeD
;					       push  ds
;					   endif

_if	MACRO	cond,text
    if	cond
	text
    endif
	ENDM

_ife	MACRO	cond,text
    ife cond
	text
    endif
	ENDM

_ifd	MACRO	cond,text
    ifdef   cond
	text
    endif
	ENDM

_ifnd	MACRO	cond,text
    ifndef  cond
	text
    endif
	ENDM

; Process processor arguments

	.686

;  Set memory model

	.model	flat, C

; Define registers:
; Instead of using the "word" registers directly, we will use a set of
; text equates.  This will allow you to use the native word size instead of
; hard coded to 16 bit words.  We also have some instruction equates for
; instruction with the register type hard coded in.

    rax equ <eax>
    rbx equ <ebx>
    rcx equ <ecx>
    rdx equ <edx>
    rdi equ <edi>
    rsi equ <esi>
    rbp equ <ebp>
    rsp equ <esp>

    CBI   equ <movsx eax, al>	 ; convert byte to int (al to rax)

; The next set of equates deals with the size of SHORTS, INTS, LONGS, and
; pointers.

    ; parameters and locals
    IWORD   equ <dword>

    ; sizes for fixing SP, stepping through tables, etc.
    ISIZE   equ 4

; Float/double definitions
; (currently the same for 16- and 32-bit segments)

FLTSIZE  equ	4	; float
DBLSIZE  equ	8	; double
LDBLSIZE equ	10	; long double

DFLOAT	 equ	<dd>
DDOUBLE  equ	<dq>
DLDOUBLE equ	<dt>

; codeseg - Define/declare the standard code segment. Maps to the proper
; form of the .code directive.
;
; Input:
;
; Output:
;	.code _TEXT	; for large code models
;	.code		; for small code models
;	assume	cs:FLAT ; for 386
;	assume	ds:FLAT ; for 386
;	assume	es:FLAT ; for 386
;	assume	ss:FLAT ; for 386
;

codeseg MACRO

	.code

	assume	ds:FLAT
	assume	es:FLAT
	assume	ss:FLAT

	ENDM

; Define named constants for ISA levels.

__ISA_AVAILABLE_X86     equ 0
__ISA_AVAILABLE_SSE2    equ 1
__ISA_AVAILABLE_SSE42   equ 2
__ISA_AVAILABLE_AVX     equ 3

; Define named constants for favor

__FAVOR_ATOM           equ 0
__FAVOR_ENFSTRG        equ 1

;***************************************************************
;*
;*  Debug lab macro
;*
;***************************************************************

lab	macro name
ifdef	DEBUG
    public  pascal name	    ;; define label public for Symdeb
endif
name:
	endm


;***************************************************************
;*
;*  Conditional jump short macros
;*
;***************************************************************


	irp	x,<Z,NZ,E,NE,P,PE,AE,BE,G>
JS&x	equ   <j&x short>
	endm


;***************************************************************
;*
;*  Global data definition macros
;*
;*  Usage:
;*	globalI   Name, InitialValue, Repeat
;*
;***************************************************************


MakeGlobal  macro   suffix, DataType	    ;; makes all of the global* macros

global&suffix  macro   name, data, rep
public	name
ifb	<rep>
    _repeat = 1
else
    _repeat = (rep)
endif

name	&DataType  _repeat dup( data )
	endm

	endm


    MakeGlobal	T, dt			; globalT
    MakeGlobal	Q, dq			; globalQ
    MakeGlobal	D, dd			; globalD
    MakeGlobal	W, dw			; globalW
    MakeGlobal	B, db			; globalB

;***************************************************************
;*
;*  Static data definition macros
;*
;*  Usage:
;*	staticI   Name, InitialValue, Repeat
;*
;***************************************************************


MakeStatic  macro   suffix, DataType	    ;; makes all of the static* macros

static&suffix  macro   name, data, rep

ifdef  DEBUG
    public  pascal name			    ;; make statics public if DEBUG
endif

ifb	<rep>
    _repeat = 1
else
    _repeat = (rep)
endif

name	&DataType  _repeat dup( data )
	endm

	endm


    MakeStatic	T, dt			; staticT
    MakeStatic	Q, dq			; staticQ
    MakeStatic	D, dd			; staticD
    MakeStatic	W, dw			; staticW
    MakeStatic	B, db			; staticB

;***************************************************************
;*
;*  Label definition macros
;*
;*  Usage:
;*	labelI	 Name, {PUBLIC, PASCAL, C}
;*
;***************************************************************

__MakePublic	macro	name, option	;; decides if a label should be
ifidni	<option>, <PUBLIC>		;; made public
    public  name
elseifidni  <option>, <PASCAL>
    public  pascal name
elseifidni  <option>, <C>
    public  C name
elseifb  <option>
    ifdef  DEBUG
	public	pascal name	;; make public if DEBUG
    endif
endif
		endm


MakeLabel   macro suffix, LabelType	;; makes all of the label* macros

%@CatStr(<label>,<suffix>)	macro	name, option
	__MakePublic	<name>,<option>
name	label	&LabelType
	endm

	endm


	MakeLabel   T, tbyte	; make labelT
	MakeLabel   Q, qword	; make labelQ
	MakeLabel   D, dword	; make labelD
	MakeLabel   W, word	; make labelW
	MakeLabel   B, byte	; make labelB

	MakeLabel   P, proc	; make labelP
	MakeLabel   FP, far	; make labelFP
	MakeLabel   NP, near	; make labelNP
