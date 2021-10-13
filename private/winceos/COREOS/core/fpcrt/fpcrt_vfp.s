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


        OPT     2       ; disable listing
        INCLUDE kxarm.h
        OPT     1       ; reenable listing


        TEXTAREA


        ; This global is non-zero if we have VFP hardware available
        ;
        IMPORT g_haveVfpHardware

        
;-------------------------------------------------------------------------------
;
; unsigned int _vfpGetFPSCR()
;
    LEAF_ENTRY _vfpGetFPSCR

        fmrx    r0, fpscr
        bx      lr

    ENTRY_END
    
    
;-------------------------------------------------------------------------------
;
; void _vfpSetFPSCR(unsigned int fpscr, unsigned int mask)
;
    LEAF_ENTRY _vfpSetFPSCR

        and     r0, r0, r1
        fmrx    r2, fpscr
        bic     r2, r2, r1
        orr     r0, r0, r2
        fmxr    fpscr, r0
        bx      lr

    ENTRY_END


;-------------------------------------------------------------------------------
;
; unsigned int _vfpGetFPSID()
;
    LEAF_ENTRY _vfpGetFPSID

        fmrx    r0, fpsid
        bx      lr
        
    ENTRY_END _vfpGetFPSID


;-------------------------------------------------------------------------------
;
; FMDRR/FMRRD are not supported by our current assembler, so define them as macros
;
        MACRO
        _fmdrr   $d, $r0, $r1
        mcrr p11, 1, $r0, $r1, $d
        MEND
        
        MACRO
        _fmrrd  $r0, $r1, $d
        mrrc p11, 1, $r0, $r1, $d
        MEND


;-------------------------------------------------------------------------------
;
; VFP_LEAF_ENTRY macro, used as a smart "header" for exports that have special
;   VFP execution paths (based on a run time check of g_haveVfpHardware)
;
        MACRO
        VFP_LEAF_ENTRY $lab

        IMPORT $lab

        LEAF_ENTRY vfp_$lab
        
        ldr     r12, =g_haveVfpHardware
        ldr     r12, [r12]
        cmp     r12, #0
        beq     $lab

        MEND


;-------------------------------------------------------------------------------
;
; VFP_TRANS_ENTRY macro, the equivalent of VFP_LEAF_ENTRY for trancedental functions
;
        MACRO
        VFP_TRANS_ENTRY $lab

        IMPORT $lab
        IMPORT $lab_vfp

        LEAF_ENTRY vfp_$lab
        
        ldr     r12, =g_haveVfpHardware
        ldr     r12, [r12]
        cmp     r12, #0
        beq     $lab
        b       $lab_vfp

        ENTRY_END
        MEND
        
        
;-------------------------------------------------------------------------------
;
; Transcedental functions
;
        VFP_TRANS_ENTRY acos
        VFP_TRANS_ENTRY asin
        VFP_TRANS_ENTRY atan
        VFP_TRANS_ENTRY atan2
        VFP_TRANS_ENTRY ceil
        VFP_TRANS_ENTRY ceilf
        VFP_TRANS_ENTRY cos
        VFP_TRANS_ENTRY cosh
        VFP_TRANS_ENTRY exp
        VFP_TRANS_ENTRY floor
        VFP_TRANS_ENTRY floorf
        VFP_TRANS_ENTRY fmod
        VFP_TRANS_ENTRY fmodf
        VFP_TRANS_ENTRY frexp
        VFP_TRANS_ENTRY ldexp
        VFP_TRANS_ENTRY log
        VFP_TRANS_ENTRY log10
        VFP_TRANS_ENTRY modf
        VFP_TRANS_ENTRY pow
        VFP_TRANS_ENTRY sin
        VFP_TRANS_ENTRY sinh
        VFP_TRANS_ENTRY tan
        VFP_TRANS_ENTRY tanh
        

;-------------------------------------------------------------------------------
;
; Binary arithmetic [single]
;
        MACRO
        SingleArith $lab, $op

        VFP_LEAF_ENTRY $lab

        fmsr    s0, r0
        fmsr    s1, r1
        $op     s0, s0, s1
        fmrs    r0, s0
        bx      lr

        ENTRY_END
        MEND

        ; float __adds( float _X, float _Y );       // add single
        ;
        SingleArith __adds, fadds

        ; float __subs( float _X, float _Y );       // subtract single
        ;
        SingleArith __subs, fsubs
        
        ; float __divs( float _X, float _Y );       // divide single
        ;
        SingleArith __divs, fdivs
        
        ; float __muls( float _X, float _Y );       // multiply float
        ;
        SingleArith __muls, fmuls


;-------------------------------------------------------------------------------
;
; Binary arithmetic [double]
;
        MACRO
        DoubleArith $lab, $op

        VFP_LEAF_ENTRY $lab

        _fmdrr  c0, r0, r1
        _fmdrr  c1, r2, r3
        $op     d0, d0, d1
        _fmrrd  r0, r1, c0
        bx      lr

        ENTRY_END
        MEND

        ; double __addd( double _X, double _Y );     // add double
        ;
        DoubleArith __addd, faddd
        
        ; double __subd( double _X, double _Y );     // subtract double
        ;
        DoubleArith __subd, fsubd
        
        ; double __muld( double _X, double _Y );     // multiply double
        ;
        DoubleArith __muld, fmuld
        
        ; double __divd( double _X, double _Y );     // divide double
        ;
        DoubleArith __divd, fdivd
        

;-------------------------------------------------------------------------------
;
; Conversion operations [single<->double]
;

        ; double __stod( float _X );                 // convert single to double
        ;
        VFP_LEAF_ENTRY __stod
        
        fmsr    s0, r0
        fcvtds  d0, s0
        _fmrrd  r0, r1, c0
        bx      lr
        
        ENTRY_END
        
        ; float __dtos( double _X );                // convert double to single
        ;
        VFP_LEAF_ENTRY __dtos
        
        _fmdrr  c0, r0, r1
        fcvtsd  s0, d0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END
        

;-------------------------------------------------------------------------------
;
; Conversion operations [32bit integer<->fp]
;

        ; float __itos( int _X );                   // convert int to single
        ;
        VFP_LEAF_ENTRY __itos
        
        fmsr    s0, r0
        fsitos  s0, s0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END

        ; float __utos( unsigned int _X );          // convert uint to single
        ;
        VFP_LEAF_ENTRY __utos
        
        fmsr    s0, r0
        fuitos  s0, s0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END
        
        ; double __itod( int _X );                   // convert int to double
        ;
        VFP_LEAF_ENTRY __itod
        
        fmsr    s0, r0
        fsitod  d0, s0
        _fmrrd  r0, r1, c0
        bx      lr
        
        ENTRY_END
        
        ; double __utod( unsigned int _X );          // convert unsigned to dbl
        ;
        VFP_LEAF_ENTRY __utod
        
        fmsr    s0, r0
        fuitod  d0, s0
        _fmrrd  r0, r1, c0
        bx      lr
        
        ENTRY_END
        
        ; int __stoi( float _X );                 // convert single to int
        ;
        VFP_LEAF_ENTRY __stoi
        
        fmsr    s0, r0
        ftosizs s0, s0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END
        
        ; unsigned int __stou( float _X );        // convert single to uint
        ;
        VFP_LEAF_ENTRY __stou
        
        fmsr    s0, r0
        ftouizs s0, s0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END
        
        ; int __dtoi( double _X );                // convert double to int
        ;
        VFP_LEAF_ENTRY __dtoi
        
        _fmdrr  c0, r0, r1
        ftosizd s0, d0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END
        
        ; unsigned int __dtou( double _X );      // convert double to unsign
        ;
        VFP_LEAF_ENTRY __dtou
        
        _fmdrr  c0, r0, r1
        ftouizd s0, d0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END
        

;-------------------------------------------------------------------------------
;
; Float negate
;
; NOTE: we're using ARM integer instructions since the cost of VFP<->ARM
;   register transfer may be be significantly larger than the VFP instruction
;

        ; float __negs( float _X );                 // negate single
        ;
        LEAF_ENTRY vfp___negs
        
        eor     r0, r0, #0x80000000
        bx      lr
        
        ENTRY_END
        
        ; double __negd( double _X );                // negate double
        ;
        LEAF_ENTRY vfp___negd

        eor     r1, r1, #0x80000000
        bx      lr
        
        ENTRY_END
        

;-------------------------------------------------------------------------------
;
; Compare operations [single]
;

        MACRO
        SingleCmp $lab, $cc, $fcmp

        ASSERT "$fcmp"="fcmps" :LOR: "$fcmp"="fcmpes"
        
        VFP_LEAF_ENTRY $lab

        fmsr    s0, r0
        fmsr    s1, r1
        $fcmp   s0, s1
        fmstat
        mov     r0, #0
        mov$cc  r0, #1
        bx      lr

        ENTRY_END
        MEND
        
        ; int __eqs( float _X, float _Y );        // compare single for EQ
        ;
        SingleCmp __eqs, EQ, fcmps
        
        ; int __nes( float _X, float _Y );        // compare single for NE
        ;
        SingleCmp __nes, NE, fcmps
        
        ; int __lts( float _X, float _Y );        // compare single for LO
        ;
        SingleCmp __lts, LO, fcmpes
        
        ; int __les( float _X, float _Y );        // compare single for LS
        ;
        SingleCmp __les, LS, fcmpes
        
        ; int __gts( float _X, float _Y );        // compare single for GT
        ;
        SingleCmp __gts, GT, fcmpes
        
        ; int __ges( float _X, float _Y );        // compare single for GE
        ;
        SingleCmp __ges, GE, fcmpes


;-------------------------------------------------------------------------------
;
; Compare operations [double]
;

        MACRO
        DoubleCmp $lab, $cc, $fcmp
        
        ASSERT "$fcmp"="fcmpd" :LOR: "$fcmp"="fcmped"
        
        VFP_LEAF_ENTRY $lab

        _fmdrr  c0, r0, r1
        _fmdrr  c1, r2, r3
        $fcmp   d0, d1
        fmstat
        mov     r0, #0
        mov$cc  r0, #1
        bx      lr

        ENTRY_END
        MEND
        
        ; int __eqd( double _X, double _Y );      // compare double for EQ
        ;
        DoubleCmp __eqd, EQ, fcmpd
        
        ; int __ned( double _X, double _Y );      // compare double for NE
        ;
        DoubleCmp __ned, NE, fcmpd
        
        ; int __ltd( double _X, double _Y );      // compare double for LO
        ;
        DoubleCmp __ltd, LO, fcmped
        
        ; int __led( double _X, double _Y );      // compare double for LS
        ;
        DoubleCmp __led, LS, fcmped
        
        ; int __gtd( double _X, double _Y );      // compare double for GT
        ;
        DoubleCmp __gtd, GT, fcmped

        ; int __ged( double _X, double _Y );      // compare double for GE
        ;
        DoubleCmp __ged, GE, fcmped


;-------------------------------------------------------------------------------
;
; Square root
;

        ; float sqrtf(float x)
        ;
        VFP_LEAF_ENTRY sqrtf
        
        fmsr    s0, r0
        fsqrts  s0, s0
        fmrs    r0, s0
        bx      lr
        
        ENTRY_END

        ; double sqrt(double x)
        ;
        VFP_LEAF_ENTRY sqrt
        
        _fmdrr  c0, r0, r1
        fsqrtd  d0, d0
        _fmrrd  r0, r1, c0
        bx      lr
        
        ENTRY_END


;-------------------------------------------------------------------------------
;
; fabs
;
; NOTE: we're using ARM integer instructions since the cost of VFP<->ARM
;   register transfer may be be significantly larger than the VFP instruction
;

        ; float fabsf(float x)
        ;
        LEAF_ENTRY vfp_fabsf
        
        bic     r0, r0, #0x80000000
        bx      lr
        
        ENTRY_END

        ; double fabs(double x)
        ;
        LEAF_ENTRY vfp_fabs
        
        bic     r1, r1, #0x80000000
        bx      lr
        
        ENTRY_END


;------------------------------------------------------------------------------

        END



