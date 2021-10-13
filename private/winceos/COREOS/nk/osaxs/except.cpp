//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++

Module Name:

    except.cpp

Abstract:

    OsaxsT0 exception catch.  Also includes code to bundle exception
    context into a buffer in the same format as current FlexiPTI

--*/

#include "osaxs_p.h"

BEGIN_FIELD_INFO_LIST (ContextDescArm)
    FIELD_INFO (cfiArmIntCxt, cfsArmIntCxt, L"ARM Integer Context", L"")
    FIELD_INFO (cfiArmVfpCxt, cfsArmVfpCxt, L"ARM FPU Context", L"")
END_FIELD_INFO_LIST ();
BEGIN_FIELD_INFO_LIST (ContextDescMips32)
    FIELD_INFO (cfiMips32IntCxt, cfsMips32IntCxt, L"MIPS32 Integer Context", L"")
    FIELD_INFO (cfiMips32FpCxt, cfsMips32FpCxt, L"MIPS32 FPU Context", L"")
END_FIELD_INFO_LIST ();
BEGIN_FIELD_INFO_LIST (ContextDescMips64)
    FIELD_INFO (cfiMips64IntCxt, cfsMips64IntCxt, L"MIPS64 Integer Context", L"")
    FIELD_INFO (cfiMips64FpCxt, cfsMips64FpCxt, L"MIPS64 FPU Context", L"")
END_FIELD_INFO_LIST ();
BEGIN_FIELD_INFO_LIST (ContextDescSh4)
    FIELD_INFO (cfiSHxIntCxt, cfsSHxIntCxt, L"SH Integer Context", L"")
    FIELD_INFO (cfiSH4FpCxt, cfsSH4FpCxt, L"SH4 FPU Context", L"")
END_FIELD_INFO_LIST ();
BEGIN_FIELD_INFO_LIST (ContextDescX86)
    FIELD_INFO (cfiX86IntCxt, cfsX86IntCxt, L"X86 Integer Context", L"")
    FIELD_INFO (cfiX86FpCxt, cfsX86FpCxt, L"X86 FPU Context", L"")
END_FIELD_INFO_LIST ();


// TODO: Replace this with a better means of selecting the appropriate data

#if defined (SHx) && !defined (SH4) && !defined (SH3e)
#define CFIELDS 2
#define FIELDLIST ContextDescSh3Dsp

#elif defined (SH3e)
#define CFIELDS 1
#define FIELDLIST ContextDescSh3

#elif defined (SH4)
#define CFIELDS 2
#define FIELDLIST ContextDescSh4

#elif defined (MIPS)
#ifdef MIPS_HAS_FPU
#define CFIELDS 2
#else
#define CFIELDS 1
#endif

#ifdef _MIPS64
#define FIELDLIST ContextDescMips64
#else
#define FIELDLIST ContextDescMips32
#endif

#elif defined (ARM)
#define CFIELDS 2
#define FIELDLIST ContextDescArm

#elif defined (x86)
#define CFIELDS 2
#define FIELDLIST ContextDescX86
#endif


/*++

Routine Name:

    WriteExceptionContext

Routine Description:

    This function writes out the current exception context into the specified buffer in the
    OsAxs context format documented in \public\common\oak\inc\osaxsflexi.h.

Arguments:

    riOut - [in/out] Current index to the output stream
    cbOut - [in] The size of the output stream
    pbOut - Pointer to the head of the output stream.
    fIntegerOnly - Allows the caller to specify that only integer registers information are to be generated.

--*/
HRESULT WriteExceptionContext (DWORD &riOut, const DWORD cbOut, BYTE *pbOut, BOOL fIntegerOnly)
{
    HRESULT hr = S_OK;
    CONTEXT * pExceptionCtx = DD_ExceptionState.context;
    
#if defined (ARM)
    if (SUCCEEDED (hr))
    {
        OsAxsArmIntContext ctx;

        ctx.Psr = pExceptionCtx->Psr;
        ctx.R[0] = pExceptionCtx->R0;
        ctx.R[1] = pExceptionCtx->R1;
        ctx.R[2] = pExceptionCtx->R2;
        ctx.R[3] = pExceptionCtx->R3;
        ctx.R[4] = pExceptionCtx->R4;
        ctx.R[5] = pExceptionCtx->R5;
        ctx.R[6] = pExceptionCtx->R6;
        ctx.R[7] = pExceptionCtx->R7;
        ctx.R[8] = pExceptionCtx->R8;
        ctx.R[9] = pExceptionCtx->R9;
        ctx.R[10] = pExceptionCtx->R10;
        ctx.R[11] = pExceptionCtx->R11;
        ctx.R[12] = pExceptionCtx->R12;
        ctx.Sp = pExceptionCtx->Sp;
        ctx.Lr = pExceptionCtx->Lr;
        ctx.Pc = pExceptionCtx->Pc;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
        OsAxsArmVfpContext ctx;
        DWORD i;

        ctx.Fpscr = pExceptionCtx->Fpscr;
        ctx.FpExc = pExceptionCtx->FpExc;
        for (i = 0; i < OSAXS_NUM_ARM_VFP_REGS; i++)
            ctx.S[i] = pExceptionCtx->S[i];
        for (i = 0; i < OSAXS_NUM_ARM_EXTRA_CONTROL_REGS; i++)
            ctx.FpExtra[i] = pExceptionCtx->FpExtra[i];

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }

#elif defined (MIPS)

    if (SUCCEEDED (hr))
    {
#ifdef _MIPS64
        OsAxsMips64IntContext ctx;
#else
        OsAxsMips32IntContext ctx;
#endif
        ctx.IntAt = pExceptionCtx->IntAt;

        ctx.IntV0 = pExceptionCtx->IntV0;
        ctx.IntV1 = pExceptionCtx->IntV1;

        ctx.IntA0 = pExceptionCtx->IntA0;
        ctx.IntA1 = pExceptionCtx->IntA1;
        ctx.IntA2 = pExceptionCtx->IntA2;
        ctx.IntA3 = pExceptionCtx->IntA3;

        ctx.IntT0 = pExceptionCtx->IntT0;
        ctx.IntT1 = pExceptionCtx->IntT1;
        ctx.IntT2 = pExceptionCtx->IntT2;
        ctx.IntT3 = pExceptionCtx->IntT3;
        ctx.IntT4 = pExceptionCtx->IntT4;
        ctx.IntT5 = pExceptionCtx->IntT5;
        ctx.IntT6 = pExceptionCtx->IntT6;
        ctx.IntT7 = pExceptionCtx->IntT7;

        ctx.IntS0 = pExceptionCtx->IntS0;
        ctx.IntS1 = pExceptionCtx->IntS1;
        ctx.IntS2 = pExceptionCtx->IntS2;
        ctx.IntS3 = pExceptionCtx->IntS3;
        ctx.IntS4 = pExceptionCtx->IntS4;
        ctx.IntS5 = pExceptionCtx->IntS5;
        ctx.IntS6 = pExceptionCtx->IntS6;
        ctx.IntS7 = pExceptionCtx->IntS7;

        ctx.IntT8 = pExceptionCtx->IntT8;
        ctx.IntT9 = pExceptionCtx->IntT9;

        ctx.IntK0 = pExceptionCtx->IntK0;
        ctx.IntK1 = pExceptionCtx->IntK1;

        ctx.IntGp = pExceptionCtx->IntGp;
        ctx.IntSp = pExceptionCtx->IntSp;

        ctx.IntS8 = pExceptionCtx->IntS8;

        ctx.IntRa = pExceptionCtx->IntRa;

        ctx.IntLo = pExceptionCtx->IntLo;
        ctx.IntHi = pExceptionCtx->IntHi;

        ctx.IntFir = pExceptionCtx->Fir;
        ctx.IntPsr = pExceptionCtx->Psr;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }

#ifdef MIPS_HAS_FPU
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
#ifdef _MIPS64
        OsAxsMips64FpContext ctx;
#else
        OsAxsMips32FpContext ctx;
#endif
        ctx.Fsr = pExceptionCtx->Fsr;
        ctx.FltF[0] = pExceptionCtx->FltF0;
        ctx.FltF[1] = pExceptionCtx->FltF1;
        ctx.FltF[2] = pExceptionCtx->FltF2;
        ctx.FltF[3] = pExceptionCtx->FltF3;
        ctx.FltF[4] = pExceptionCtx->FltF4;
        ctx.FltF[5] = pExceptionCtx->FltF5;
        ctx.FltF[6] = pExceptionCtx->FltF6;
        ctx.FltF[7] = pExceptionCtx->FltF7;
        ctx.FltF[8] = pExceptionCtx->FltF8;
        ctx.FltF[9] = pExceptionCtx->FltF9;
        ctx.FltF[10] = pExceptionCtx->FltF10;
        ctx.FltF[11] = pExceptionCtx->FltF11;
        ctx.FltF[12] = pExceptionCtx->FltF12;
        ctx.FltF[13] = pExceptionCtx->FltF13;
        ctx.FltF[14] = pExceptionCtx->FltF14;
        ctx.FltF[15] = pExceptionCtx->FltF15;
        ctx.FltF[16] = pExceptionCtx->FltF16;
        ctx.FltF[17] = pExceptionCtx->FltF17;
        ctx.FltF[18] = pExceptionCtx->FltF18;
        ctx.FltF[19] = pExceptionCtx->FltF19;
        ctx.FltF[20] = pExceptionCtx->FltF20;
        ctx.FltF[21] = pExceptionCtx->FltF21;
        ctx.FltF[22] = pExceptionCtx->FltF22;
        ctx.FltF[23] = pExceptionCtx->FltF23;
        ctx.FltF[24] = pExceptionCtx->FltF24;
        ctx.FltF[25] = pExceptionCtx->FltF25;
        ctx.FltF[26] = pExceptionCtx->FltF26;
        ctx.FltF[27] = pExceptionCtx->FltF27;
        ctx.FltF[28] = pExceptionCtx->FltF28;
        ctx.FltF[29] = pExceptionCtx->FltF29;
        ctx.FltF[30] = pExceptionCtx->FltF30;
        ctx.FltF[31] = pExceptionCtx->FltF31;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
#endif // MIPS_HAS_FPU

#elif defined (SHx)

    if (SUCCEEDED (hr))
    {
        OsAxsShxIntContext ctx;

        ctx.PR = pExceptionCtx->PR;
        ctx.MACH = pExceptionCtx->MACH;
        ctx.MACL = pExceptionCtx->MACL;
        ctx.GBR = pExceptionCtx->GBR;
        ctx.R[0] = pExceptionCtx->R0;
        ctx.R[1] = pExceptionCtx->R1;
        ctx.R[2] = pExceptionCtx->R2;
        ctx.R[3] = pExceptionCtx->R3;
        ctx.R[4] = pExceptionCtx->R4;
        ctx.R[5] = pExceptionCtx->R5;
        ctx.R[6] = pExceptionCtx->R6;
        ctx.R[7] = pExceptionCtx->R7;
        ctx.R[8] = pExceptionCtx->R8;
        ctx.R[9] = pExceptionCtx->R9;
        ctx.R[10] = pExceptionCtx->R10;
        ctx.R[11] = pExceptionCtx->R11;
        ctx.R[12] = pExceptionCtx->R12;
        ctx.R[13] = pExceptionCtx->R13;
        ctx.R[14] = pExceptionCtx->R14;
        ctx.R[15] = pExceptionCtx->R15;
        ctx.Fir = pExceptionCtx->Fir;
        ctx.Psr = pExceptionCtx->Psr;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }

#if defined (SH4)
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
        OsAxsSh4FpContext ctx;
        DWORD i;

        ctx.Fpscr = pExceptionCtx->Fpscr;
        ctx.Fpul = pExceptionCtx->Fpul;
        for (i = 0; i < OSAXS_NUM_SH4_F_REGS; i++)
            ctx.FRegs[i] = pExceptionCtx->FRegs[i];
        for (i = 0; i < OSAXS_NUM_SH4_XF_REGS; i++)
            ctx.xFRegs[i] = pExceptionCtx->xFRegs[i];

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
#endif

#elif defined (x86)
    if (SUCCEEDED (hr))
    {
        OsAxsX86IntContext ctx;

        ctx.Gs = pExceptionCtx->SegGs;
        ctx.Fs = pExceptionCtx->SegFs;
        ctx.Es = pExceptionCtx->SegEs;
        ctx.Ds = pExceptionCtx->SegDs;
        ctx.Edi = pExceptionCtx->Edi;
        ctx.Esi = pExceptionCtx->Esi;
        ctx.Ebx = pExceptionCtx->Ebx;
        ctx.Edx = pExceptionCtx->Edx;
        ctx.Ecx = pExceptionCtx->Ecx;
        ctx.Eax = pExceptionCtx->Eax;
        ctx.Ebp = pExceptionCtx->Ebp;
        ctx.Eip = pExceptionCtx->Eip;
        ctx.Cs = pExceptionCtx->SegCs;
        ctx.Eflags = pExceptionCtx->EFlags;
        ctx.Esp = pExceptionCtx->Esp;
        ctx.Ss = pExceptionCtx->SegSs;

        hr = Write(&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
        OsAxsX86FpContext ctx;

        ctx.ControlWord = pExceptionCtx->FloatSave.ControlWord;
        ctx.StatusWord = pExceptionCtx->FloatSave.StatusWord;
        ctx.TagWord = pExceptionCtx->FloatSave.TagWord;
        ctx.ErrorOffset = pExceptionCtx->FloatSave.ErrorOffset;
        ctx.ErrorSelector = pExceptionCtx->FloatSave.ErrorSelector;
        ctx.DataOffset = pExceptionCtx->FloatSave.DataOffset;
        ctx.DataSelector = pExceptionCtx->FloatSave.DataSelector;

        memcpy (&ctx.RegisterArea, &pExceptionCtx->FloatSave.RegisterArea, sizeof (ctx.RegisterArea));

        ctx.Cr0NpxState = pExceptionCtx->FloatSave.Cr0NpxState;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
#endif

    return hr;
}


/*++

Routine Name:

    GetExceptionContext

Routine Description:

    Regurgitate the stored registers and give them to the caller in
    a nice format.
    
--*/
HRESULT GetExceptionContext (DWORD dwFlags, DWORD *pcbBuf, BYTE *pbBuf)
{
    HRESULT hr = S_OK;
    DWORD cbBuf = 0;
    DWORD i = 0, iHeader;
    DWORD dwElementRva = 0;
    
    if (pcbBuf == NULL || pbBuf == NULL)
    {
        hr = E_INVALIDARG;
        goto exit;
    }

    cbBuf = *pcbBuf;
        
    if(dwFlags & GET_CTX_HDR)
    {
        iHeader = i;
        hr = WritePTMResponseHeader(CFIELDS, 1, dwElementRva, i, cbBuf, pbBuf);
        if(FAILED(hr))
        {
            goto exit;
        }
    }

    if(dwFlags & GET_CTX_DESC)
    {
        hr = WriteFieldInfo (FIELDLIST, CFIELDS, i, cbBuf, pbBuf);
        if(FAILED(hr))
        {
            goto exit;
        }
    }

    if(dwFlags & GET_CTX)
    {
        dwElementRva = i;
        hr = WriteExceptionContext (i, cbBuf, pbBuf, FALSE);
        if(FAILED(hr))
        {
            goto exit;
        }
    }

    if (dwFlags & GET_CTX_HDR)
    {
        hr = WritePTMResponseHeader(CFIELDS, 1, dwElementRva, iHeader, cbBuf, pbBuf);
        if(FAILED(hr))
        {
            goto exit;
        }
    }

    *pcbBuf = i;
    hr = S_OK;

exit:
    return hr;
}


