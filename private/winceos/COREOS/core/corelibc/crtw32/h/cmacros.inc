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
comment $

	SCCSID = "@(#)cmacros.mas:1.12"

cmacros - assembly macros for interfacing to hhls

Copyright (C) Microsoft Corporation. All rights reserved.

$

;; Revision History
;;
;;	1.00	05/03/84	Initial Release
;;
;;	1.01	05/06/84	Greg Whitten
;;				Added defgrp and changed cMerge to Microsoft C
;;				Added copyright message and changed to 1.01
;;				Changes should have no affect on working programs
;;
;;	1.02	07/10/84	Steve Wood
;;				Added labelx macros
;;
;;	1.03	07/14/84	Greg Whitten
;;				Added defines for ?pu, ?adj, ?lblpu
;;				  (removes undefined errors)
;;				Changes should have no affect on working programs
;;
;;	1.04	07/18/84	Greg Whitten
;;				Added local control from PL/M or C conventions
;;				  except for cCall macro
;;
;;	1.05	08/06/84	Steve Wood
;;				Made ?PLM and ?WIN be the defaults
;;
;;	1.06	01/02/85	Steve Wood
;;				Changed createSeg and defgrp to automatically
;;				  define the ln_assumes macro and the lnoffset
;;				  and lnbase equates for each logical segment
;;				  name.
;;
;;	1.07	02/19/85	Walt Moore
;;				Added farptr macro for defining a far pointer
;;				to be used in a cCall.	Folded regptr into
;;				farptr.  Space compaction in macros.  Changed
;;				?pp to be smaller.  Moved ?ia out of ?al1 into
;;				?aloc.	Merged cProc and ?pd into one macro.
;;				Changed some %outs to use the error macro so
;;				an error would be generated.  Added makeframe
;;				and parmR to cProc.  Changed error to also put
;;                              the error message in the listing.
;;				Deleted the smashes macro.
;;
;;	1.08	03/18/85	Steve Wood
;;				Added NODATA support.
;;
;;	1.09	03/27/85	Steve Wood
;;				Added ?definitions
;;
;;	2.00	04/01/85	Steve Wood
;;				April fools
;;
;;	2.01	06/17/85	Steve Wood
;;				Changed NODATA to always generate POP DS
;;				  for return address patching
;;
;;	2.02	02/11/86	Steve Wood
;;				Added ATOMIC keyword to cProc macro
;;				Changed far epilog to use LEA SP,BP-2
;;				Changed error macro to ??error to avoid
;;				  conflict
;;
;;	2.03	03/06/86	Steve Wood
;;				Fixed bug with ATOMIC and locals in far proc
;;				Added DOS5 switch to disable INC/DEC BP
;;				  instructions in special far prologs/epilogs
;;
;;	2.04	08/07/86	Scott Randell
;;				Fixed bug with ATOMIC and ?TF
;;				  (was doing unnecessary MOV SP,BP)
;;				Added pcode profile ?PROFILE
;;
;;	2.05	08/12/86	Walt Moore
;;				Changed _TEXT alignment to word.
;;				Added/corrected some comments.
;;				Removed redundant init of ?pc in cProc
;;				Made ATOMIC require NODATA
;;				Moved definition of non-underscored 'C' label
;;				  from the cProc to the cBegin macro
;;				Minor clean-up of code
;;
;;	2.06	09/11/86	Walt Moore
;;				Added private stack checking
;;				Put local control for PL/M or C into cCall
;;
;;
;;	2.07	09/19/86	Steve Wood
;;				Added ?SMALL, ?MEDIUM, etc.  symbols
;;				Added forceframe keyword to cProc macro.
;;				Interpret ?TF for all epilogs.
;;
;;	3.xx.a	02/26/87	Walt Moore
;;				Massive rework.  Documentation coming.
;;
;;	3.xx.b	04/08/87	NeilK
;;				Added parmH, which is like parmW except
;;				that it reserves 4 bytes on frame.
;;
;;	3.xx.c	05/11/87	Walt Moore
;;				Added <> to ?ap so that arg <DataOFFSET foo>
;;				can be used.
;;
;;	3.01	07/03/87	Walt Moore
;;				parm_bytes_&procname is now generated for
;;				all cProcs, and is the number of bytes of
;;				parameters to the cProc.
;;
;;				NO_BP added as a keyword to cProc which
;;				causes all equates to be generated without
;;				reference to BP for the procedure.  All type
;;				info is still generated, but the user must
;;				supply any segment and base register.
;;				?NO_BP, if defined, makes this the default.
;;
;;				USE_BP can be specified on the cProc line
;;				to force the generation of BP in equates.
;;
;;				Moved definition of xxxxBASE.  The equ was to
;;				a forward reference.
;;
;;				Don't generate a warning for a nogen if only
;;				parameters are present.
;;
;;				Keywords for cProc, cBegin, cEnd, and registers
;;				are now allowed to be either all upper case or
;;				all lower case.
;;
;;				Only generate warnings on pass 2.
;;
;;	3.02	07/06/87	Walt Moore
;;				Boy did I mess up <nogen>  If the text is
;;				recognized as nogen, then process it as such,
;;				else ignore it and generate frame as needed.
;;
;;	3.03	07/14/87	Walt Moore
;;				Added <partial> keyword to the cBegin macro
;;				to only allocate locals and save registers.
;;				Used in conjunction with dispatching in the
;;				Winthorn engine and drivers.
;;
;;				Added cleanframe macro which will take the
;;				frame down, but not generate the RET statement.
;;				Used in conjunction with dispatching in the
;;				Winthorn engine and drivers.
;;
;;	3.04	07/16/87	Walt Moore
;;				globalD was generating off and seg name
;;				without word ptr override, giving them a
;;				dword attribute
;;
;;	3.05	07/17/87	Walt Moore
;;				.xcref cleanframe
;;
;;	3.06    07/24/87	Hans Spiller
;;				32 bit small model (?SMALL32):
;;				  new entry exit sequences using pseudoregs
;;				  mach independant register names (IAX, etc)
;;				  parmI/localI (int size variables)
;;				  mpush/mpop uses mach independant names
;;				  IPUSHF, IPOPF, IPUSHA,IPOPA,IIRET
;;				  
;;				case sensitivity bugfix. the compiler
;;				  generates "CODE", not "Code"
;;
;;				32 bit issues as yet undone:  allocation
;;			        macros for dealing with 32 bit mode far
;;				pointers, globalI, staticI, parmR, saving
;;				si,di vs. esi,edi,ebx
;;
;;	3.06a   09/29/87	Mark Roberts
;;				32 bit small model (?SMALL32):
;;				  fix a few bugs and add staticI
;;
;;	3.06b	07/20/87	Scott Randell
;;				Fix up for ?COW compatibility, added ?NOATOMIC
;;				to 3.xx version.
;;
;;	3.06c	04/29/88	Jim Schaad
;;				Put in ?DFDATA to force data segments even if
;;				?NODATA is set.
;;
;;	3.06d	05/02/88	Andy Padawer
;;				Bug fixes for 3.06b (cEnd), 3.06c (general).
;;
;;	3.06e	08/31/88	Andy Padawer
;;				use "if memS32" not "ifdef ?SMALL32".
;;
;;	3.06f   05/12/89	Mark Roberts
;;			  	fix staticI, globalDP and add globalI
;;
;;	3.06g   12/07/89	Mark Roberts
;;			  	add externI
;;
;;	3.06h	01/25/90	Jan de Rie
;;				add ?DFCODE to allow suppressing code segments
;;
;; Assembly macros for interfacing to C
;;
;;      User settable conditional assembly flags used within the cmacros
;;
;;	Memory model flags.  Set only one of the following.  memS is the
;;	default.  The symbols with ? are for defining from the command line
;;      and the memx symbols are numeric symbols that can be set in your source
;;      file prior to including this file.
;;
;;	?DF	Define flag.  If this flag is 0, then defines default segment
;;		and group definitions based on the compiler flag.  If this
;;              flag is 1, then does not define any segments or groups.
;;
;;	?TF	Tight flag.  If this flag is 0, then use longer epilog
;;		sequence that safely cleans up a stack frame.  If this flag is
;;              1, then use more efficient epilog that assumes the stack is
;;		valid (SP)
;;
;;	?PLM	Calling convention flag.  If this flag is 0, then the
;;		calling convention used is that of C.  If this flag
;;		is 1, then the PL/M calling convention is used.
;;		The default value is 1.  The PL/M calling convention
;;		is used by pascal, fortran, basic, and cobol.
;;
;;		In the C calling convention, arguments are passed
;;              in reverse order; arg0 is the last pushed, argn is the
;;              first pushed.  also, it is the callers responsibility
;;              to remove the arguments from the stack upon a return
;;              from a call.
;;
;;		In the PL/M calling comvention, arguments are passed
;;              as encountered; arg0 is the first pushed, argn is the
;;              last pushed.  also, it is the called procedure's
;;              responsibility to remove parameters from the stack
;;		before returning (using the RET n instruction)
;;
;;	?QUIET	  If defined, then only error messages will be issued to
;;		  the console.	If undefined, then certain cmacro text will
;;		  be generated to the console.
;;
;;	IAX, ICX, IDX, IBX, ISP, IBP, ISI, IDI
;;		  these pseudo registers expand to either ax..., or eax...
;;		  depending upon 32bit mode being enabled.  they should be
;;		  used whenever a pointer or integer is being used in order
;;		  to make source code machine independant



.xcref					;;Get rid of a lot of symbols


;	??_out	- output given message to the console unless ?QUIET has
;	been specified.
;
;	usage:
;		??_out	<t>
;
;	where:
;		<t> is the message to output

.xcref	??_out
??_out macro t
  ifndef ?QUIET
    %out t
  endif
endm



;	outif - output msg if name is non-zero.  if name is undefined,
;	set name = 0, else set name to the default value.
;
;	usage:
;		outif	name,defval,onmsg,offmsg
;	where:
;		name	name of symbol
;		defval	default value to give symbol if not defined
;			if blank, then 0 will be used
;		onmsg	text to display if symbol is non-zero
;		offmsg	test to be displayed if symbol is zero


outif macro name,defval,onmsg,offmsg
  ifndef name
    ifb <defval>
      name=0
    else
      name=defval
    endif
  endif
  if name
    name=1
    ifnb <onmsg>
      ??_out <! onmsg>
    endif
  else
    ifnb <offmsg>
      ??_out <! offmsg>
    endif
  endif
endm



;	??error - output msg and generate an assembly time error
;		  on regardess of assembler pass
;	usage:
;		??error <t>
;	where:
;		t	is the text to be output


.xcref	??error
??error macro	msg
  %out e r r o r ----- msg		;;to console
  .err e r r o r ----- msg		;;forced error by assembler
endm


;	??error2 - output msg and generate an assembly time error
;		   on pass 2 only
;	usage:
;		??error2 <t>
;	where:
;		t	is the text to be output


.xcref	??error2
??error2 macro	 msg
  if2
  %out e r r o r ----- msg		;;to console
  .err e r r o r ----- msg		;;forced error by assembler
  endif
endm


.xcref sizeI,wordI

  ??_out  <cMacros Version 3.06h - 01/25/90>
  ??_out  <Copyright (C)Microsoft Corporation. All rights reserved.>

    sizeI = 4				; size of a push
    wordI equ <dword>
    asmdI equ <dd>

  outif ?DF,0,<No segments or groups will be defined>
  outif ?TF,0,<Epilog sequences assume valid SP>
  outif ?PLM,1,<PL/M calling convention>

;; define pseudo registers and instructions for 386/8086 independance

  .686
  IAX equ <eax>
  ICX equ <ecx>
  IDX equ <edx>
  IBX equ <ebx>
  ISP equ <esp>
  IBP equ <ebp>
  ISI equ <esi>
  IDI equ <edi>

;;	Initialize all symbols used in the macros.  Theses symbols will not be
;;      included in any cross reference listing.

	.xcref	?n,?ax,?ah,?al,?bx,?bh
	.xcref	?bl,?cx,?ch,?cl,?dx,?dh
	.xcref	?dl,?si,?di,?es,?ds,?bp
	.xcref	?sp,?ss,?cs
	.xcref	?n,?AX,?AH,?AL,?BX,?BH
	.xcref	?BL,?CX,?CH,?CL,?DX,?DH
	.xcref	?DL,?SI,?DI,?ES,?DS,?BP
	.xcref	?SP,?SS,?CS
	.xcref  ?EAX,?EBX,?ECX,?EDX,?ESI,?EDI,?ESP,?EBP
	.xcref  ?eax,?ebx,?ecx,?edx,?esi,?edi,?esp,?ebp
	.xcref  ?IAX,?IBX,?ICX,?IDX,?ISI,?IDI,?ISP,?IBP

	.xcref	?rsl,?cpd,?argl,?argc,?ba
	.xcref	?acb,???,?po
	.xcref	?pas

	.xcref	uconcat,mpush,mpop
	.xcref	?ri,?pp,?pp1,?al1
	.xcref	?ad,?ap,?atal,?dd,?dd1,?dd2
	.xcref	?pg,?pg1,?aloc
	.xcref	?DF,?TF,?PLM,?ia,?pu,?adj
	.xcref	?uf,?rp
	.xcref	arg,cProc,assumes
	.xcref	createSeg
	.xcref	save,outif
	.xcref	?PLMPrevParm,?gcc
	.xcref	?pcc
	.xcref	?cbe,?pcbe



;;      conditionals set by the macros
;;
;;	?pc	Procedure class.  If this is set to 1, then the procedure
;;              is a far procedure, else it is a near procedure.
;;
;;	?ia	Interface adjustment count for far procedures.	The
;;              interface adjustment defines the number of bytes of
;;		storage allocated between BP and the first frame variable
;;              allocated on the stack.
;;
;;		Normally zero, it will be adjusted for both far windows
;;              procedures and by register parameters.
;;
;;	?cpd	Current procedure defined.  This is set to a non-zero
;;		value if a procedure is being defined (i.e a cProc has
;;		been encountered, and cBegin has not).
;;
;;	?ba	Begin active.  This is set to a non-zero value if a
;;		cBegin is active (i.e. a cBegin has been encountered,
;;		and cEnd has not).
;;
;;	?pcc	procedure calling conventing.  Calling convention for
;;		this procedure.  May be different than the default set
;;		via ?PLM
;;
;;
;;	Other variables that are defined once so that the .xcref command
;;      doesn't get too upset if they show up missing!

?rsl    =       0                       ;;0 = no register to save
?cpd    =       0                       ;;<> 0 if in a procedure definition
?argl   =       0                       ;;length of arguments pushed on stack
?argc   =       0                       ;;# of arguments so far
?ba     =       0                       ;;<>0 if in a procedure (xbegin)
?acb    =       0                       ;;number of arguments to a call
???     =       0                       ;;byte count of local storage
?po     =       0                       ;;byte count of parameters
?pas    =       0                       ;;autosave value for procedure
?ia     =       0                       ;;no adjustment
?pu     =       0                       ;;public flag for some macros
?adj    =       0                       ;;initial define for .xcref
?rp     =       0                       ;;count of register parameters
?uf     =       0                       ;;user's frame code specified
?dd2	=	0			;;used for globalx and staticx
?pcc	=	?PLM			;;procedure calling convention
?PLMPrevParm =	0			;;Used in parameter processing
?cbe	=	0			;;cbegin/cEnd keyword flag

	.xcref	?casen
if1					;;only define ?casen on pass 1
?casen	=	0			;;case sensitive assembly if <> 0
endif



?n      =       0000000000000000b       ;;register none
?ax	=	0000000000000011b	;;register ax
?ah	=	0000000000000001b	;;register ah
?al	=	0000000000000010b	;;register al
?bx	=	0000000000001100b	;;register bx
?bh	=	0000000000000100b	;;register bh
?bl	=	0000000000001000b	;;register bl
?cx	=	0000000000110000b	;;register cx
?ch	=	0000000000010000b	;;register ch
?cl	=	0000000000100000b	;;register cl
?dx	=	0000000011000000b	;;register dx
?dh	=	0000000001000000b	;;register dh
?dl	=	0000000010000000b	;;register dl
?si	=	0000000100000000b	;;register si
?di	=	0000001000000000b	;;register di
?es	=	0000010000000000b	;;register es
?ds	=	0000100000000000b	;;register ds
?bp	=	0001000000000000b	;;register bp
?sp	=	0010000000000000b	;;register sp
?ss	=	0100000000000000b	;;register ss
?cs	=	1000000000000000b	;;register cs
					;;Incase we're case sensitive
?AX	=	0000000000000011b	;;register ax
?AH	=	0000000000000001b	;;register ah
?AL	=	0000000000000010b	;;register al
?BX	=	0000000000001100b	;;register bx
?BH	=	0000000000000100b	;;register bh
?BL	=	0000000000001000b	;;register bl
?CX	=	0000000000110000b	;;register cx
?CH	=	0000000000010000b	;;register ch
?CL	=	0000000000100000b	;;register cl
?DX	=	0000000011000000b	;;register dx
?DH	=	0000000001000000b	;;register dh
?DL	=	0000000010000000b	;;register dl
?SI	=	0000000100000000b	;;register si
?DI	=	0000001000000000b	;;register di
?ES	=	0000010000000000b	;;register es
?DS	=	0000100000000000b	;;register ds
?BP	=	0001000000000000b	;;register bp
?SP	=	0010000000000000b	;;register sp
?SS	=	0100000000000000b	;;register ss
?CS	=	1000000000000000b	;;register cs

?EAX	=	0000000000000011b	;;register ax
?EBX	=	0000000000001100b	;;register bx
?ECX	=	0000000000110000b	;;register cx
?EDX	=	0000000011000000b	;;register dx
?ESI	=	0000000100000000b	;;register si
?EDI	=	0000001000000000b	;;register di
?EBP	=	0001000000000000b	;;register bp
?ESP	=	0010000000000000b	;;register sp

?eax	=	0000000000000011b	;;register ax
?ebx	=	0000000000001100b	;;register bx
?ecx	=	0000000000110000b	;;register cx
?edx	=	0000000011000000b	;;register dx
?esi	=	0000000100000000b	;;register si
?edi	=	0000001000000000b	;;register di
?ebp	=	0001000000000000b	;;register bp
?esp	=	0010000000000000b	;;register sp

?IAX	=	0000000000000011b	;;register ax
?IBX	=	0000000000001100b	;;register bx
?ICX	=	0000000000110000b	;;register cx
?IDX	=	0000000011000000b	;;register dx
?ISI	=	0000000100000000b	;;register si
?IDI	=	0000001000000000b	;;register di
?IBP	=	0001000000000000b	;;register bp
?ISP	=	0010000000000000b	;;register sp

        .cref



;;      uconcat - unconditionally generate a statement from a field
;;      of given parameters
;;
;;      usage:
;;		uconcat  a,b,c,d,e,f,g
;;
;;      where:
;;              a,b   are concatenated for field 1
;;              c,d   are concatenated for field 2
;;		e,f,g are concatenated for field 3

uconcat macro a,b,c,d,e,f,g
  a&b c&d e&f&g
endm



;;      mpush pushes multiple registers onto the stack according to
;;      a register specification.
;;
;;      format:
;;              mpush   r
;;
;;      where:
;;              r       is a numeric expression returned from ?ri
;;                      or any other valid register expression

mpush macro r
  irp x,<IAX,IBX,ICX,IDX,ISI,IDI,es,ds,IBP,ISP,ss,cs>
    if (r and ?&&x)
      push x								      ;@
    endif
  endm
endm



;;      mpop pops multiple registers from the stack according to
;;      a register specification.
;;
;;      format:
;;              mpop    r
;;
;;      where:
;;              r       is a numeric expression returned from ?ri
;;                      or any other valid register expression

mpop macro r
  irp x,<cs,ss,ISP,IBP,ds,es,IDI,ISI,IDX,ICX,IBX,IAX>
    if (r and ?&&x)
      pop x								      ;@
    endif
  endm
endm


;;      save - flag that the indicated registers are to be saved/restored
;;
;;	A flag is created which indicates which registers are to be saved
;;	when the cCall macro is invoked, and then restored after the call.
;;
;;      usage:
;;              save    <r>
;;
;;      where  r  is the list of registers to save, which may be:
;;
;;		register     saves
;;		   AX	      AX
;;		   AH	      AX
;;		   AL	      AX
;;		   BX	      BX
;;		   BH	      BX
;;		   BL	      BX
;;		   CX	      CX
;;		   CH	      CX
;;		   CL	      CX
;;		   DX	      DX
;;		   DH	      DX
;;		   DL	      DX
;;		   SI	      SI
;;		   DI	      DI
;;		   ES	      ES
;;		   DS	      DS
;;		   BP	      BP
;;
;;		  none	    nothing
;;
;;      the macro generates a value for the variable ?rsl

save macro r
  ?rsl=0				;;initialize save list
  ?ri ?rsl,<r>				;;generate magic number
endm



;;      ?ri - or register indexes to variable
;;
;;      ?ri is a macro that examines the passed argument list and computes
;;      a register index variable.
;;
;;	The values ORed with the variable are:
;;
;;              ?n       equ     0000000000000000b;
;;		?AX	 equ	 0000000000000011b;
;;		?AH	 equ	 0000000000000001b;
;;		?AL	 equ	 0000000000000010b;
;;		?BX	 equ	 0000000000001100b;
;;		?BH	 equ	 0000000000000100b;
;;		?BL	 equ	 0000000000001000b;
;;		?CX	 equ	 0000000000110000b;
;;		?CH	 equ	 0000000000010000b;
;;		?CL	 equ	 0000000000100000b;
;;		?DX	 equ	 0000000011000000b;
;;		?DH	 equ	 0000000001000000b;
;;		?DL	 equ	 0000000010000000b;
;;		?SI	 equ	 0000000100000000b;
;;		?DI	 equ	 0000001000000000b;
;;		?ES	 equ	 0000010000000000b;
;;		?DS	 equ	 0000100000000000b;
;;		?BP	 equ	 0001000000000000b;
;;		?SP	 equ	 0010000000000000b;
;;		?SS	 equ	 0100000000000000b;
;;		?CS	 equ	 1000000000000000b;
;;      usage:
;;		?ri n,<rl>
;s mach independant names;      where:
;;              n       is the variable to contain the new index value
;;		r	is the register list

?ri macro n,r
  irp x,<r>
    ifdef ?&&x				;;if defined, then add to list
      n=n or ?&&x
    else
      ??error2 <unknown register x>
      .err
    endif
  endm
endm



;;      parmx - generate reference to parameter(s) on the stack
;;
;;	An equate is generated for addressing a paramter(s)
;;      on the stack for the current procedural frame.
;;
;;	An error message is generated if there isn't a current frame.
;;
;;      usage:
;;              parmX   n
;;      where:
;;              X       is the type of the argument(s)  B=byte, W=word, D=dword
;;			I = machine independant int size
;;              n       is the name(s) to be given the parameter(s).
;;
;;	Bytes are considered to be two bytes long for alignment.
;;
;;	The parmd form of the macro generates three equates:
;;
;;              name       -    for accessing the parameter as a double word
;;              off_name   -    for accessing the offset  (lsw) of the parameter
;;              seg_name   -    for accessing the segment (msw) of the parameter

.xcref
.xcref parmDP
.cref

parmDP macro n
  ?pp <n>,wordI,sizeI,sizeI
endm



;;      ?pp is the generalized parameter definition macro
;;
;;      usage:
;;              ?pp m,t,l,s
;;
;;      where:
;;              n       is the name(s) of the parameters
;;              t       is the type (word, dword)
;;              l       is the length to update parameter byte count by
;;              s       is the internal typing size


?pp macro n,t,l,s			;;process parameter
  if ?cpd				;;must be in a procedure definition
    .xcref
    irp x,<n>
      .xcref ?t&&x			;;don't want this in xref
      ?t&&x=s				;;save size info
      ife ?pcc				;;if C calling convention
	?pp1 x,<t>,,,%(?po+?adj)
	?po=?po+l			;;update parameter offset
      else				;;else assemble for PL/M
	?PLMPrevParm=?PLMPrevParm+1	;;Show next parameter
	?po=?po+l			;;update parameter offset
	?pp1 x,<t>,%?po,%?adj,,%?PLMPrevParm,%(?PLMPrevParm-1)
      endif
    endm
    .cref
  else
    ??error2 <parm(s) "&n" declared outside proc def>
  endif
endm



;;	?pp1 is the macro that generates the text equate for the
;;	parameter.  Two options exist, one for the C calling
;;      convention where the last parameter was the first pushed onto
;;	the stack ('C' convention), and one for the PL/M calling
;;      convention where the first parameter was the first
;;      pushed (also the same as ms-pascal).
;;
;;	The text generated will be of one of two forms:
;;
;;		name equ (type ptr [bp+(adj+offset)]) for C
;;           or
;;		name equ (type ptr [bp+adj+?po-offset]) for PL/M
;;
;;
;;	For C, since parameters are pushed first last, the offset
;;      plus the adjust will point to the correct parameter.
;;
;;	For PL/M, since parameters are pushed first first, the offset
;;	of a parameter is much more complicated.  A known portion of
;;      the offset can be computed when the text equate is generated.
;;
;;	What is known is the number of garbage bytes between BP and
;;      the nearest parameter (in this case the last parameter), and
;;      also how many bytes of parameters have preceeded this parameter.
;;
;;	What is unknown is how many total bytes of parameters there will
;;      be, which affects all the generated text equates since the offset
;;      from bp must be determined at some point.
;;
;;	Well, the offset from BP can be computed with one variable if
;;      the following is remembered:
;;
;;          the offset of any parameter from the first parameter is always
;;          the current parameter offset (?po).
;;
;;	With this in mind, you just have to figure out where the first
;;      parameter is, which is:
;;
;;              bp + garbage adjustment + distance to first parameter
;;         or
;;              bp + ?adj + ?po
;;
;;	This implies that any parameter can be defined as:
;;
;;              bp + ?adj + ?po -%?po
;;
;;	Make any sense?
;;
;;	For PL/M, a chain of self-purging macros will be generated
;;	which will pass the evaluated ?po to any previous incarnation
;;	of the macro.  This will allow the text equate to be generated
;;	with the actual offset instead of the symbolic ?po.
;;
;;
;;      usage:
;;		?pp1	n,t,o,a,b,cpc,ppc
;;
;;      where:
;;              n    is the name to be given the equate
;;              t    is the type (byte, word, dword)
;;              o    is the offset from the first parameter
;;              a    is the adjustment
;;              b    is the adjustment plus the offset from the first parameter
;;		cpc  is the number of parameters so far
;;		ppc  is cpc - 1


?pp1 macro n,t,o,a,b,cpc,ppc
  ife ?pcc				;;if to generate for C
      n equ (t ptr [IBP][+b])
  else					;;else generate for PL/M
    .xcref
    .xcref ?PLMParm&cpc
    .cref
      ?PLMParm&cpc &macro po
	uconcat <n>,,<equ>,,<(t ptr [IBP][+>,%(a+po-o),<])>
	?PLMParm&ppc po
	purge ?PLMParm&cpc
      &endm
  endif
endm



;;      localx - generate reference to a local variable on the stack
;;
;;	An equate is generated for addressing a local variable
;;      on the stack for the current procedural frame.
;;
;;      usage:
;;              localx    n
;;      where:
;;              x       is the type b=byte, w=word, d=dword, v=variable size
;;              n       is the name(s) to be given the variable(s).
;;
;;	Bytes are considered to be two bytes long for alignment reasons
;;
;;	The locald form of the macro generates three equates:
;;
;;              name       -    for accessing the variable as a double word
;;              off_name   -    for accessing the offset  (lsw) of the variable
;;              seg_name   -    for accessing the segment (msw) of the variable


.xcref
.xcref localV
.cref

localV macro n,a
  ?aloc <n>,,%(a),0,1			;; word aligned
endm


;;      ?aloc is the macro that actually allocates local storage.
;;      it is only invoked by the localx macros.
;;
;;      usage:
;;              ?aloc  n,t,l,s,a
;;      where:
;;              n      is a list of names of local variable of the
;;                      given type.
;;              t       is the text string for the given variable
;;                      and is one of:
;;                          word  ptr
;;                          dword ptr
;;                          byte  ptr
;;                      or alternatively left blank for variable size
;;                      allocations (no implicit type).
;;              l       is the size of the variable in bytes
;;              s       is the internal type flag (size), and is one of:
;;                              word     - 2
;;                              dword    - 4
;;                              byte     - 1
;;                              variable - 0
;;              a       is a flag indicating that word alignment is to be
;;                      forced for this type of item.
;;
;;	NOTE:	It is assumed that the stack is already aligned on a word
;;		boundary when the cProc is invoked.  The macros will guarantee
;;		to allocate an even number of bytes on the stack to maintain
;;              word alignment.


?aloc macro n,t,l,s,a
  if ?cpd				;;must be in a proc def
    .xcref
    irp x,<n>				;;generate symbol equates
      ???=???+l 			;;update length of locals
      if a				;;if align, then force word alignment
	if l GT 2
	  ???=((??? + 3) and 0fffffffch) ;; dword alignment
	else
	  ???=((??? + 1) and 0fffeh)
	endif
      endif
      ?al1 x,<t>,%(???+?ia)		;;?ia will always be valid (0 or 2)
      .xcref ?t&&x
      ?t&&x=s				;;save size info
    endm
    .cref
  else
    ??error2 <locals "&n" declared outside procedure def>
  endif
endm



;;      ?al1 - allocate local, continued.
;;
;;      ?al1 actually generates the text equate for the local variable.
;;	The form of the text equate generated is more or less:
;;
;;		name equ (type ptr [bp-?ia-nn])
;;          or
;;		name equ ([bp-?ia-nn])
;;
;;      where:
;;              ?ia   is defined to be either zero, or is defined to be
;;		      the number of bytes between the saved BP and the first
;;		      local.  ?ia is only applicable if the current cProc is
;;		      a windows far procedure or if parmRs have been
;;		      encountered.  If not, the ?ia will be zero.  since ?ia
;;		      is determinable prior to invoking this macro, it will be
;;		      added into the  offset ("nn") passed to this macro
;;
;;      usage:
;;              ?al1 n,t,o
;;      where:
;;              n    is the name for the text equate
;;              t    is the type of the equate
;;              o    is the offset of the equate


?al1 macro n,t,o
    n equ (t [IBP][-o])
endm


;;	?gcc - get calling convention
;;
;;	?gcv sets the given symbol to the calling convention
;;	to be used.
;;
;;	usage:
;;		?gcc s,i,cc
;;
;;	where:
;;		s  is the symbol to return the convention in
;;		   s = 0 if 'C' calling convention
;;		   s = 1 if PL/M (PASCAL) calling convention
;;		i  is the initial value for s
;;		cc is the calling convention override, and may be one of
;;		   C	  use 'C' convention
;;		   PLM	  use PL/M calling convention
;;		   PASCAL use PL/M calling convention

?gcc macro s,i,cc
  s = i 				;;Set default calling convention
  ifnb <cc>
    ifidn <cc>,<C>			;;If overriding default
      s=0				;;  'C' calling convention
    endif
    ifidn <cc>,<PLM>
      s=1				;;  PL/M calling convention
    endif
    ifidn <cc>,<PASCAL>
      s=1				;;  PL/M calling convention
    endif
  endif
endm



;;      ?dd is the generalized data definition macro.
;;
;;      format:
;;		?dd n,p,t,d,i,s,c
;;      where:
;;              n       is the name of the procedure
;;              p       is the public flag
;;              t       is the assembler type (byte, word, dword)
;;              d       is the assembler directive (db,dw or dd)
;;              i       is the initial value
;;              s       is a duplication factor
;;		c	is the convention, C for C, PLM or PSACAL for PL/M.
;;			The default (?PLM flag) will be used if not specified.


?dd macro n,p,t,d,i,s,c
  ?gcc ?dd2,%?PLM,<c>			;;Set calling convention
  ife ?dd2				;;If 'C'
    n label t
    ?dd1 _&n,p,<d>,<i>,<s>		;;Microsoft C uses leading underscores
  else
    ?dd1 n,p,<d>,<i>,<s>		;;If PL/M
  endif
endm



;;      ?dd1 is the generalized data definition macro.
;;
;;      format:
;;              ?dd1 n,p,d,i,s
;;      where:
;;              n       is the name of the procedure
;;              p       is the public flag
;;              d       is the assembler directive (db,dw or dd)
;;              i       is the initial value
;;              s       is a duplication factor


?dd1 macro n,p,d,i,s
  if p
    public n
  endif
  ifb <s>
    n d i
  else
    ifb <i>
      n d s dup (?)
    else
      n d s dup (i)
    endif
  endif
endm



;	?ad is the macro which creates a definition for the given
;	symbol
;
;	usage:
;		?ad <n>,s
;	where:
;		n is a list of names to define
;		s is the size info (1,2,4,8,10)


?ad macro n,s
  irp x,<n>
    .xcref
    .xcref ?t&&x
    .cref
    ?t&&x=s				;;save size info
  endm
endm



;;      arg - declare argument
;;
;;	The given argument(s) is added to the argument list structure
;;
;;      format:
;;              arg     a
;;
;;      where:
;;              a       is any valid argument to push.
;;
;;	If any element in arglist has not been defined or isn't a 16-bit
;;      register, then a complete specification must have been given in a
;;      text equate and a defx also given (if not, you'll pay the penalty!)


arg macro a
  irp x,<a>
    ?argc=?argc+1			;;increment the arg count
    ?atal <x>,%?argc			;;generate argument
  endm
endm



;;      ?atal (add to argument list) generates a macro that will cause
;;	the given argument to be processed when invoked.  It is used by
;;      the arg macro only.


?atal macro n,i
  .xcref
  .xcref ?ali&i
  .cref
  ?ali&i &macro
    ?ap <n>
  &endm
endm



;;      ?ap - process arguments and place onto stack
;;
;;	The given argument is processed (type checking) and place on
;;	the stack for a pending call.  There must be a type definition
;;	for all arguments (except words).  This can be done by using
;;      text equates and the defx macro.
;;
;;      format:
;;              ?ap n
;;      where:
;;              n       is the name of the argument to be pushed
;;
;;	The variable  ?argl  is updated by the length of the arguments
;;      pushed so that the stack can be cleaned up after the call.


?ap macro n
  ?argl=?argl+2 			;;assume one word is pushed
  ifdef ?t&n
    ife ?t&n-1				;;byte type
      push word ptr (n) 						      ;@
      exitm
    endif

    ife ?t&n-2				;;word type
      push n								      ;@
      exitm
    endif

    ife ?t&n-4				;;dword type
      push word ptr (n)[2]						      ;@
      push word ptr (n) 						      ;@
      ?argl=?argl+2
      exitm
    endif

    ife ?t&n-8				;;qword type
      push word ptr (n)[6]						      ;@
      push word ptr (n)[4]						      ;@
      push word ptr (n)[2]						      ;@
      push word ptr (n) 						      ;@
      ?argl=?argl+6
      exitm
    endif

    if ?t&n and 80h			;;far pointer type
      n
      ?argl=?argl+2
      exitm
    endif

    ife ?t&n				;;variable storage
      push word ptr (n) 						      ;@
      exitm
    endif
  endif

  push n				;;unknown or register or immediate    ;@
endm



;;	cProc - define a 'c' procedure
;;
;;	cProc is the procedure definition for procedures.
;;
;;      format:
;;		cProc n,cf,a
;;      where:
;;              n       is the name of the procedure
;;
;;		cf	controls certain definitions, and may be:
;;			    NEAR       proc is to be a near label
;;			    FAR        proc is to be a far  label
;;			    PUBLIC     proc is to be defined as public
;;			    SMALL      call makeframe procedure
;;			    NODATA     dont create prolog code to setup DS
;;			    ATOMIC     don't link stack if not needed
;;				       NODATA must be specified for ATOMIC
;;			    FORCEFRAME Force generation of a frame
;;			    C	       proc is to be a C    procedure
;;			    PLM        proc is to be a PL/M procedure
;;			    PASCAL     proc is to be a PL/M procedure
;;			    WIN        proc is to be a windows procedure
;;			    NONWIN     proc isn't to be a windows procedure
;;			    NO_BP      don't generate BP in text equates
;;			    BP	       generate BP in text equates
;;
;;              a       is a list of registers that are to be saved whenever
;;                      the procedure is invoked.
;;
;;			makeframe procedure:   If small is specified, then
;;                      the "makeframe procedure" is invoked instead of
;;                      generating normal prologues/epilogues
;;
;;			A call is performed to the makeframe procedure.  The
;;                      call is followed by two bytes.  the first byte is the
;;                      number of locals to allocate for the frame, the second
;;			is the number of bytes of parameters.  The makeframe
;;			procedure will in turn call the cProc routine at the
;;			address following the data bytes.  When the cProc is
;;                      finished, it will do a near return to the makeframe
;;                      procedure to clean up the frame and exit.
;;
;;			Note that register parameters and makeframe are
;;                      incompatible and cannot be used together.
;;
;;			The makeframe procedure will save SI, DI, and also
;;			DS if a far procedure.	These registers will be
;;                      removed from the autosave list if specified.


cProc macro n,cf,a
  if ?cpd
    ?utpe				;;generate unterminated proc error
  endif

  ?cpd=1				;;a procdef is active now
  ???=0 				;;no locals are defined yet
  ?argc=0				;;no arguments are defined
  ?ba=0 				;;not in a procedure
  ?po=0 				;;initial parameter offset
  ?pu=0 				;;initial public setting
  ?ia=0 				;;no special prolog/epilog
  ?adj=2*sizeI				;;parameter adjustment (near ret+bp)
  ?rp=0 				;;no register parameters
  ?uf=0 				;;don't use makeframe
  ?pas=0				;;process register save list
  ?pcc=?PLM				;;calling convention (C or PL/M)

  ifnb <a>				;;register save list
    ?ri ?pas,<a>
  endif

  irp x,<cf>
    ifdef ??_cproc_&&x
      ??_cproc_&&x
    else
      ??error2 <e r r o r - unknown keyword x>
      .err
    endif
  endm

  if ?pcc				;;If PLM
    ?PLMPrevParm=0			;;  show no previous parameter
    .xcref
    .xcref ?PLMParm0
    .cref
    ?PLMParm0 &macro			;;Null macro to terminate
    purge ?PLMParm0
    &endm
  endif

  .xcref
  .xcref ?CC&n
  .cref
  ?CC&n=?pcc				;;Save procedure type

  ?pas = ?pas and (not (?sp+?cs+?ss))	;;make no sense to save these

  if ?uf				;;don't save these if user frame
    ?pas = ?pas and (not (?bp+?si+?di))
  endif

  ife ?pcc
    ?pg  <_&n>,%?pu,%?pas,<n>,%?pcc
  else
    ?pg  <n>,%?pu,%?pas,<n>,%?pcc
  endif
endm




;;      ?pg - generate begin and nested macros for current procedure
;;
;;      format:
;;		?pg n,p,a,w,nnu,cc
;;      where:
;;              n       is the name of the procedure
;;              p       is the public flag
;;              a       is an enumerated list of registers to save
;;                      at entry and restore at exit
;;		nnu	procedure name without any underscore
;;		cc	calling convention (C or PL/M)
;;
;;
;;      local stack allocation will be forced to an even byte count to
;;      maintain stack word alignment.


?pg macro n,p,a,nnu,cc
  .xcref
  if ?uf				;;if user frame
      ??error2 <NODATA encountered in &n - user frame ignored>
      ?uf=0
  endif

  .xcref cBegin
  cBegin &macro g			;;generate cBegin macro
    .xcref
    if cc				;;Finish definition of parameters
      uconcat <?PLMParm>,%?PLMPrevParm,%?po
    endif

    if ?uf				;;if user frame
      if ?rp				;;If register parameters
	??error2 <parmR encountered in &n - user frame ignored>
	?uf=0
      endif
    endif
    ?pg1 <n>,a,%?po,%?uf,%?rp,cc,<nnu>,%??? ;;generate cEnd macro
    ?cpd=0				;;terminate current proc def
    ?argc=0				;;no arguments are defined yet
    ?ba=1				;;have reached a begin
    ???=(???+1) and 0fffeh		;;word align local storage

    if p				;;If to be public
      public n
    endif

      n proc

    ife cc				;;if 'C' calling convention
      nnu equ n 			;;  generate label without underscore
    endif

    ?cbe = 0				;;Assume no command on cBegin line
    ifnb <g>
      ?pcbe <g>,<nnu>,<cBegin>
    endif
    if ?cbe eq 1			;;if nogen, then cannot have locals
      if ???+a+?rp			;;  saved regs, or parmRs
	if2
	  ??_out <cBegin - possibly invalid use of nogen>
	endif
      endif
    else				;;else must generate a frame
	ife ?cbe			;;If full frame to be set up
	    if ???+?po+?rp		;;if any locals or parameters
	      push IBP			;;  then must set frame pointer       ;@
	      mov IBP,ISP 		;;  to be able to access them	      ;@
	    endif
	  if ?rp			;;if parmR's, push them before
	    ?uf=0			;;  allocating locals and saving
	    rept ?rp			;;  the autosave registers
	      uconcat mpush,,?rp,%?uf
	      ?uf=?uf+1
	    endm
	  endif
	  if ???			;;if locals to allocate
	      sub   ISP,???		;; allocate any local storage	      ;@
	  endif
	else				;;partial frame, only set locals
	  if ?rp			;;If parmRs, error
	    ??error2 <cBegin - parmRs encountered with partial frame>
	  else
	    if ???			;;Only realloc frame if locals
	      lea ISP,[IBP][-???-?ia]	;;?ia will adjust for saved BP	      ;@
	    endif
	  endif
	endif
	mpush a 			;;save autosave registers
    endif

    .cref
    purge cBegin			;;remove the macro
  &endm 				;;end of cBegin macro

  .xcref ?utpe
  ?utpe &macro
    ??error2 <unterminated procedure definition: "&n">
  &endm
  .cref
endm					;;end of ?pg macro




;;      ?pg1 - generate end macro for current procedure
;;
;;      format:
;;		?pg1 n,a,o,f,d,r,cc,nnu,lc
;;      where:
;;              n       is the name of the procedure
;;              a       is an enumerated list of registers to save
;;                      at entry and restore at exit
;;              o       is the number of bytes of paramteres to remove at exit
;;              f       is 1 if to use the user's makeframe procedure
;;              r       number of register parameters
;;		cc	calling convention (C or PL/M)
;;		nnu	procedure name without any underscore
;;		lc	locals byte count


?pg1 macro n,a,o,f,r,cc,nnu,lc
  .xcref
  .xcref cEnd

  parm_bytes_&nnu = o			;;Define number of parameter bytes

  cEnd &macro g 			;;start of cEnd macro
    .xcref
    ?ba=0				;;no longer in a procedure
    ?cbe = 0				;;Assume no command on cBegin line
    ifnb <g>
      ?pcbe <g>,<nnu>,<cEnd>
    endif
    if ?cbe eq 1			;;if nogen, then cannot have parmRs
      if a+r+lc 			;;  locals, or saved registers
	if2
	  ??_out <cEnd - possibly invalid use of nogen>
	endif
      endif
    else				;;else must remove the frame
      mpop a				;;restore autosaved registers
      if f				;;if to use the "makeframe" procedure
	db 0c3h 			;;  near return to user's makeframe    @
      else
	  if ???+?po+r			;;if any parameters
	    if (?TF eq 0) or (???+r)	;;if cannot assume valid SP
	      mov ISP,IBP 		;;  or locals or parmR's, get valid SP;@
	    endif
	    pop IBP							      ;@
	  endif
	if ?cbe ne 4			;;Don't generate ret if ??cleanframe??   e
	  ife cc			;;if C calling convention
	    ret 			;;  return			      ;@
	  else				;;else
	    ret o			;;  return and remove paramteres      ;@
	  endif
	endif
      endif
    endif
    if ?cbe ne 4			;;Don't delete macro if ??cleanframe??   e
      n endp				;;end of process
      .cref
      purge cEnd			;;remove the macro
    else
      .cref
    endif
  &endm
  .cref
endm


;; The following macros are used to speed up the processing of the
;; command allowed on a cProc command line.  They simply set or clear
;; the correct flag as needed.

.xcref
.xcref ??_cproc_PUBLIC,??_cproc_C

.xcref ??_cproc_public,??_cproc_c
.cref

??_cproc_PUBLIC macro
  ?pu=1
endm

??_cproc_C macro
  ?pcc=0
endm

??_cproc_public macro
  ?pu=1
endm

??_cproc_c macro
  ?pcc=0
endm


;	?pcbe is the macro which processes the text on cBegin/cEnd
;	The text is allowed to be:
;
;		NOGEN	nogen
;		PARTIAL partial
;		the procedure name as given on the cProc line
;
;	usage:
;		?pcbe <g>,<nnu>,<mt>
;	where:
;		g	is the text on the cBegin/cEnd line
;		nnu	is the procedure name without any underscores
;		mt	macro type (cBegin/cEnd)
;
;	The variable ?cbe is set to the following value
;
;		0 = invalid (defualt action>
;		1 = nogen
;		2 = partial frame
;		4 = clean frame


?pcbe macro g,nnu,mt
  ifidn <g>,<NOGEN>
    ?cbe = 1
  else
    ifidn <g>,<nogen>
      ?cbe = 1
    else
      ifidn <mt>,<cBegin>
	ifidn <g>,<PARTIAL>
	  ?cbe = 2
	else
	  ifidn <g>,<partial>
	    ?cbe = 2
	  else
	    ifdif <g>,<nnu>
	      ??error2 <mt - unknown keyword g>
	    endif
	  endif
	endif
      else
	  ifdif <g>,<nnu>
	    ??error2 <mt - unknown keyword g>
	  endif
      endif
    endif
  endif
endm



;	assumes is a macro that will set up the assumes for a segment
;	or group created with the createSeg macro.  If the assumed
;	value passed in isn't known, then a normal assume is made.
;
;       usage:
;               assumes s,g
;
;       where:
;               s       is the register to make the assumption about
;		g	is the value to assume is in it
;
; as a hack, substitute FLAT for g


assumes macro   s,ln
    assume s:FLAT
endm



;	createSeg is a macro that sets up a segment definition and
;	a logical name for that segment.  The logical name can be
;       used to enter the segment, but it cannot be used for anything
;       else.
;
;       usage:
;		createSeg n,ln,a,co,cl
;       where:
;               n       is the physical name of the segment
;               ln      is the name it is to be invoked by
;               a       is the alignment, and is optional
;               co      is the combine type, and is optional
;               cl      is the class, and is optional


createSeg macro n,ln,a,co,cl
  ifnb <cl>
    n segment a co '&cl'
  else
    n segment a co
  endif
  n ends
    ln&OFFSET equ offset n:
    ln&BASE equ n
endm


;	sBegin is the macro that opens up the definition of a segment.
;	The segment must have already been defined with the createSeg
;       macro.
;
;       usage:
;		sBegin ln
;
;       where:
;               ln      is the logical name given to the segment when
;                       it was declared.

BeginDATA macro
    .data
endm

BeginCODE macro
    .code
endm

EndDATA macro
endm

EndCODE macro
endm

ife ?DF

  ;	Define all segments that will be used.	This will allow the
  ;	assume and groups to be set up at one given place, and also
  ;	allow quick changes to be made
  ;
  ;	createSeg name,logname,align,combine,class

createSeg _TEXT,CODE,%wordI,public,CODE

endif

  .model flat
