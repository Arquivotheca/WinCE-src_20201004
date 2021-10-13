//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifdef  DEBUG
#include "kernel.h"
#define HEADER_FILE
#define VALIDATE_STRUCTURES
#include "ksmips.h"

//  Calculate the byte offset of a field in a structure of type type.
#define FIELDOFFSET(type, field)    ((SHORT)&(((type *)0)->field))

//  Validate context field offsets.
#define VALID_CTX(CTX_FIELD,CTX_NEXT_FIELD,CONTEXT_FIELD) \
    if ((( CTX_FIELD ) != ( FIELDOFFSET ( CONTEXT, CONTEXT_FIELD ))) \
        || (( CTX_NEXT_FIELD - CTX_FIELD ) < sizeof (((PCONTEXT) 0 ) -> CONTEXT_FIELD )))    \
    {   \
        Error = * (long *) 0;   \
    }

//  Validate thread field offsets.
#define VALID_TCX(TCX_FIELD,TCX_NEXT_FIELD,THREAD_FIELD) \
    if ((( TCX_FIELD ) != ( FIELDOFFSET ( struct Thread, THREAD_FIELD ))) \
        || (( TCX_NEXT_FIELD - TCX_FIELD ) < sizeof (((struct Thread *) 0 ) -> THREAD_FIELD )))    \
    {   \
        Error = * (long *) 0;   \
    }

//  Validate the thread field values.

void
ValidateStructures (
    )
{
    long Error;

    //  Validate the context fields.
    VALID_CTX ( CxFltF0,  CxFltF1,  FltF0 );
    VALID_CTX ( CxFltF1,  CxFltF2,  FltF1 );
    VALID_CTX ( CxFltF2,  CxFltF3,  FltF2 );
    VALID_CTX ( CxFltF3,  CxFltF4,  FltF3 );
    VALID_CTX ( CxFltF4,  CxFltF5,  FltF4 );
    VALID_CTX ( CxFltF5,  CxFltF6,  FltF5 );
    VALID_CTX ( CxFltF6,  CxFltF7,  FltF6 );
    VALID_CTX ( CxFltF7,  CxFltF8,  FltF7 );
    VALID_CTX ( CxFltF8,  CxFltF9,  FltF8 );
    VALID_CTX ( CxFltF9,  CxFltF10, FltF9 );
    VALID_CTX ( CxFltF10, CxFltF11, FltF10 );
    VALID_CTX ( CxFltF11, CxFltF12, FltF11 );
    VALID_CTX ( CxFltF12, CxFltF13, FltF12 );
    VALID_CTX ( CxFltF13, CxFltF14, FltF13 );
    VALID_CTX ( CxFltF14, CxFltF15, FltF14 );
    VALID_CTX ( CxFltF15, CxFltF16, FltF15 );
    VALID_CTX ( CxFltF16, CxFltF17, FltF16 );
    VALID_CTX ( CxFltF17, CxFltF18, FltF17 );
    VALID_CTX ( CxFltF18, CxFltF19, FltF18 );
    VALID_CTX ( CxFltF19, CxFltF20, FltF19 );
    VALID_CTX ( CxFltF20, CxFltF21, FltF20 );
    VALID_CTX ( CxFltF21, CxFltF22, FltF21 );
    VALID_CTX ( CxFltF22, CxFltF23, FltF22 );
    VALID_CTX ( CxFltF23, CxFltF24, FltF23 );
    VALID_CTX ( CxFltF24, CxFltF25, FltF24 );
    VALID_CTX ( CxFltF25, CxFltF26, FltF25 );
    VALID_CTX ( CxFltF26, CxFltF27, FltF26 );
    VALID_CTX ( CxFltF27, CxFltF28, FltF27 );
    VALID_CTX ( CxFltF28, CxFltF29, FltF28 );
    VALID_CTX ( CxFltF29, CxFltF30, FltF29 );
    VALID_CTX ( CxFltF30, CxFltF31, FltF30 );
    VALID_CTX ( CxFltF31, CxIntZero, FltF31 );

    VALID_CTX ( CxIntZero, CxIntAt, IntZero );
    VALID_CTX ( CxIntAt, CxIntV0, IntAt );
    VALID_CTX ( CxIntV0, CxIntV1, IntV0 );
    VALID_CTX ( CxIntV1, CxIntA0, IntV1 );
    VALID_CTX ( CxIntA0, CxIntA1, IntA0 );
    VALID_CTX ( CxIntA1, CxIntA2, IntA1 );
    VALID_CTX ( CxIntA2, CxIntA3, IntA2 );
    VALID_CTX ( CxIntA3, CxIntT0, IntA3 );
    VALID_CTX ( CxIntT0, CxIntT1, IntT0 );
    VALID_CTX ( CxIntT1, CxIntT2, IntT1 );
    VALID_CTX ( CxIntT2, CxIntT3, IntT2 );
    VALID_CTX ( CxIntT3, CxIntT4, IntT3 );
    VALID_CTX ( CxIntT4, CxIntT5, IntT4 );
    VALID_CTX ( CxIntT5, CxIntT6, IntT5 );
    VALID_CTX ( CxIntT6, CxIntT7, IntT6 );
    VALID_CTX ( CxIntT7, CxIntS0, IntT7 );
    VALID_CTX ( CxIntS0, CxIntS1, IntS0 );
    VALID_CTX ( CxIntS1, CxIntS2, IntS1 );
    VALID_CTX ( CxIntS2, CxIntS3, IntS2 );
    VALID_CTX ( CxIntS3, CxIntS4, IntS3 );
    VALID_CTX ( CxIntS4, CxIntS5, IntS4 );
    VALID_CTX ( CxIntS5, CxIntS6, IntS5 );
    VALID_CTX ( CxIntS6, CxIntS7, IntS6 );
    VALID_CTX ( CxIntS7, CxIntT8, IntS7 );
    VALID_CTX ( CxIntT8, CxIntT9, IntT8 );
    VALID_CTX ( CxIntT9, CxIntK0, IntT9 );
    VALID_CTX ( CxIntK0, CxIntK1, IntK0 );
    VALID_CTX ( CxIntK1, CxIntGp, IntK1 );
    VALID_CTX ( CxIntGp, CxIntSp, IntGp );
    VALID_CTX ( CxIntSp, CxIntS8, IntSp );
    VALID_CTX ( CxIntS8, CxIntRa, IntS8 );
    VALID_CTX ( CxIntRa, CxIntLo, IntRa );
    VALID_CTX ( CxIntLo, CxIntHi, IntLo );
    VALID_CTX ( CxIntHi, CxFsr, IntHi );
    VALID_CTX ( CxFsr, CxFir, Fsr );
    VALID_CTX ( CxFir, CxPsr, Fir );
    VALID_CTX ( CxPsr, CxContextFlags, Psr );
    VALID_CTX ( CxContextFlags, CxHProc, ContextFlags );
    VALID_CTX ( CxHProc, CxAkyCur, Fill [ 0 ]);
    VALID_CTX ( CxAkyCur, ContextFrameLength, Fill [ 1 ]);

    //  Validate the thread context fields.
    VALID_TCX ( TcxBadVAddr, TcxIntAt, ctx.BadVAddr );
    VALID_TCX ( TcxIntAt, TcxIntV0, ctx.IntAt );
    VALID_TCX ( TcxIntV0, TcxIntV1, ctx.IntV0 );
    VALID_TCX ( TcxIntV1, TcxIntA0, ctx.IntV1 );
    VALID_TCX ( TcxIntA0, TcxIntA1, ctx.IntA0 );
    VALID_TCX ( TcxIntA1, TcxIntA2, ctx.IntA1 );
    VALID_TCX ( TcxIntA2, TcxIntA3, ctx.IntA2 );
    VALID_TCX ( TcxIntA3, TcxIntT0, ctx.IntA3 );
    VALID_TCX ( TcxIntT0, TcxIntT1, ctx.IntT0 );
    VALID_TCX ( TcxIntT1, TcxIntT2, ctx.IntT1 );
    VALID_TCX ( TcxIntT2, TcxIntT3, ctx.IntT2 );
    VALID_TCX ( TcxIntT3, TcxIntT4, ctx.IntT3 );
    VALID_TCX ( TcxIntT4, TcxIntT5, ctx.IntT4 );
    VALID_TCX ( TcxIntT5, TcxIntT6, ctx.IntT5 );
    VALID_TCX ( TcxIntT6, TcxIntT7, ctx.IntT6 );
    VALID_TCX ( TcxIntT7, TcxIntS0, ctx.IntT7 );
    VALID_TCX ( TcxIntS0, TcxIntS1, ctx.IntS0 );
    VALID_TCX ( TcxIntS1, TcxIntS2, ctx.IntS1 );
    VALID_TCX ( TcxIntS2, TcxIntS3, ctx.IntS2 );
    VALID_TCX ( TcxIntS3, TcxIntS4, ctx.IntS3 );
    VALID_TCX ( TcxIntS4, TcxIntS5, ctx.IntS4 );
    VALID_TCX ( TcxIntS5, TcxIntS6, ctx.IntS5 );
    VALID_TCX ( TcxIntS6, TcxIntS7, ctx.IntS6 );
    VALID_TCX ( TcxIntS7, TcxIntT8, ctx.IntS7 );
    VALID_TCX ( TcxIntT8, TcxIntT9, ctx.IntT8 );
    VALID_TCX ( TcxIntT9, TcxIntK0, ctx.IntT9 );
    VALID_TCX ( TcxIntK0, TcxIntK1, ctx.IntK0 );
    VALID_TCX ( TcxIntK1, TcxIntGp, ctx.IntK1 );
    VALID_TCX ( TcxIntGp, TcxIntSp, ctx.IntGp );
    VALID_TCX ( TcxIntSp, TcxIntS8, ctx.IntSp );
    VALID_TCX ( TcxIntS8, TcxIntRa, ctx.IntS8 );
    VALID_TCX ( TcxIntRa, TcxIntLo, ctx.IntRa );
    VALID_TCX ( TcxIntLo, TcxIntHi, ctx.IntLo );
    VALID_TCX ( TcxIntHi, TcxFsr, ctx.IntHi );
    VALID_TCX ( TcxFsr, TcxFir, ctx.Fsr );
    VALID_TCX ( TcxFir, TcxPsr, ctx.Fir );
    VALID_TCX ( TcxPsr, TcxContextFlags, ctx.Psr );
#ifndef MIPS_HAS_FPU
    VALID_TCX ( TcxContextFlags, TcxSizeof, ctx.ContextFlags );
#else   //  MIPS_HAS_FPU
    VALID_TCX ( TcxFltF0,  TcxFltF1,  ctx.FltF0 );
    VALID_TCX ( TcxFltF1,  TcxFltF2,  ctx.FltF1 );
    VALID_TCX ( TcxFltF2,  TcxFltF3,  ctx.FltF2 );
    VALID_TCX ( TcxFltF3,  TcxFltF4,  ctx.FltF3 );
    VALID_TCX ( TcxFltF4,  TcxFltF5,  ctx.FltF4 );
    VALID_TCX ( TcxFltF5,  TcxFltF6,  ctx.FltF5 );
    VALID_TCX ( TcxFltF6,  TcxFltF7,  ctx.FltF6 );
    VALID_TCX ( TcxFltF7,  TcxFltF8,  ctx.FltF7 );
    VALID_TCX ( TcxFltF8,  TcxFltF9,  ctx.FltF8 );
    VALID_TCX ( TcxFltF9,  TcxFltF10, ctx.FltF9 );
    VALID_TCX ( TcxFltF10, TcxFltF11, ctx.FltF10 );
    VALID_TCX ( TcxFltF11, TcxFltF12, ctx.FltF11 );
    VALID_TCX ( TcxFltF12, TcxFltF13, ctx.FltF12 );
    VALID_TCX ( TcxFltF13, TcxFltF14, ctx.FltF13 );
    VALID_TCX ( TcxFltF14, TcxFltF15, ctx.FltF14 );
    VALID_TCX ( TcxFltF15, TcxFltF16, ctx.FltF15 );
    VALID_TCX ( TcxFltF16, TcxFltF17, ctx.FltF16 );
    VALID_TCX ( TcxFltF17, TcxFltF18, ctx.FltF17 );
    VALID_TCX ( TcxFltF18, TcxFltF19, ctx.FltF18 );
    VALID_TCX ( TcxFltF19, TcxFltF20, ctx.FltF19 );
    VALID_TCX ( TcxFltF20, TcxFltF21, ctx.FltF20 );
    VALID_TCX ( TcxFltF21, TcxFltF22, ctx.FltF21 );
    VALID_TCX ( TcxFltF22, TcxFltF23, ctx.FltF22 );
    VALID_TCX ( TcxFltF23, TcxFltF24, ctx.FltF23 );
    VALID_TCX ( TcxFltF24, TcxFltF25, ctx.FltF24 );
    VALID_TCX ( TcxFltF25, TcxFltF26, ctx.FltF25 );
    VALID_TCX ( TcxFltF26, TcxFltF27, ctx.FltF26 );
    VALID_TCX ( TcxFltF27, TcxFltF28, ctx.FltF27 );
    VALID_TCX ( TcxFltF28, TcxFltF29, ctx.FltF28 );
    VALID_TCX ( TcxFltF29, TcxFltF30, ctx.FltF29 );
    VALID_TCX ( TcxFltF30, TcxFltF31, ctx.FltF30 );
    VALID_TCX ( TcxFltF31, TcxSizeof, ctx.FltF31 );
#endif  //  MIPS_HAS_FPU
}
#endif  //  DEBUG
