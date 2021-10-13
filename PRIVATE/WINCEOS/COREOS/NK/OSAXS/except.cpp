//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    
#if defined (ARM)
    if (SUCCEEDED (hr))
    {
        OsAxsArmIntContext ctx;

        ctx.Psr = g_pExceptionCtx->Psr;
        ctx.R[0] = g_pExceptionCtx->R0;
        ctx.R[1] = g_pExceptionCtx->R1;
        ctx.R[2] = g_pExceptionCtx->R2;
        ctx.R[3] = g_pExceptionCtx->R3;
        ctx.R[4] = g_pExceptionCtx->R4;
        ctx.R[5] = g_pExceptionCtx->R5;
        ctx.R[6] = g_pExceptionCtx->R6;
        ctx.R[7] = g_pExceptionCtx->R7;
        ctx.R[8] = g_pExceptionCtx->R8;
        ctx.R[9] = g_pExceptionCtx->R9;
        ctx.R[10] = g_pExceptionCtx->R10;
        ctx.R[11] = g_pExceptionCtx->R11;
        ctx.R[12] = g_pExceptionCtx->R12;
        ctx.Sp = g_pExceptionCtx->Sp;
        ctx.Lr = g_pExceptionCtx->Lr;
        ctx.Pc = g_pExceptionCtx->Pc;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
        OsAxsArmVfpContext ctx;
        DWORD i;

        ctx.Fpscr = g_pExceptionCtx->Fpscr;
        ctx.FpExc = g_pExceptionCtx->FpExc;
        for (i = 0; i <= OSAXS_NUM_ARM_VFP_REGS; i++)
            ctx.S[i] = g_pExceptionCtx->S[i];
        for (i = 0; i < OSAXS_NUM_ARM_EXTRA_CONTROL_REGS; i++)
            ctx.FpExtra[i] = g_pExceptionCtx->FpExtra[i];

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
        ctx.IntAt = g_pExceptionCtx->IntAt;

        ctx.IntV0 = g_pExceptionCtx->IntV0;
        ctx.IntV1 = g_pExceptionCtx->IntV1;

        ctx.IntA0 = g_pExceptionCtx->IntA0;
        ctx.IntA1 = g_pExceptionCtx->IntA1;
        ctx.IntA2 = g_pExceptionCtx->IntA2;
        ctx.IntA3 = g_pExceptionCtx->IntA3;

        ctx.IntT0 = g_pExceptionCtx->IntT0;
        ctx.IntT1 = g_pExceptionCtx->IntT1;
        ctx.IntT2 = g_pExceptionCtx->IntT2;
        ctx.IntT3 = g_pExceptionCtx->IntT3;
        ctx.IntT4 = g_pExceptionCtx->IntT4;
        ctx.IntT5 = g_pExceptionCtx->IntT5;
        ctx.IntT6 = g_pExceptionCtx->IntT6;
        ctx.IntT7 = g_pExceptionCtx->IntT7;

        ctx.IntS0 = g_pExceptionCtx->IntS0;
        ctx.IntS1 = g_pExceptionCtx->IntS1;
        ctx.IntS2 = g_pExceptionCtx->IntS2;
        ctx.IntS3 = g_pExceptionCtx->IntS3;
        ctx.IntS4 = g_pExceptionCtx->IntS4;
        ctx.IntS5 = g_pExceptionCtx->IntS5;
        ctx.IntS6 = g_pExceptionCtx->IntS6;
        ctx.IntS7 = g_pExceptionCtx->IntS7;

        ctx.IntT8 = g_pExceptionCtx->IntT8;
        ctx.IntT9 = g_pExceptionCtx->IntT9;

        ctx.IntK0 = g_pExceptionCtx->IntK0;
        ctx.IntK1 = g_pExceptionCtx->IntK1;

        ctx.IntGp = g_pExceptionCtx->IntGp;
        ctx.IntSp = g_pExceptionCtx->IntSp;

        ctx.IntS8 = g_pExceptionCtx->IntS8;

        ctx.IntRa = g_pExceptionCtx->IntRa;

        ctx.IntLo = g_pExceptionCtx->IntLo;
        ctx.IntHi = g_pExceptionCtx->IntHi;

        ctx.IntFir = g_pExceptionCtx->Fir;
        ctx.IntPsr = g_pExceptionCtx->Psr;

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
        ctx.Fsr = g_pExceptionCtx->Fsr;
        ctx.FltF[0] = g_pExceptionCtx->FltF0;
        ctx.FltF[1] = g_pExceptionCtx->FltF1;
        ctx.FltF[2] = g_pExceptionCtx->FltF2;
        ctx.FltF[3] = g_pExceptionCtx->FltF3;
        ctx.FltF[4] = g_pExceptionCtx->FltF4;
        ctx.FltF[5] = g_pExceptionCtx->FltF5;
        ctx.FltF[6] = g_pExceptionCtx->FltF6;
        ctx.FltF[7] = g_pExceptionCtx->FltF7;
        ctx.FltF[8] = g_pExceptionCtx->FltF8;
        ctx.FltF[9] = g_pExceptionCtx->FltF9;
        ctx.FltF[10] = g_pExceptionCtx->FltF10;
        ctx.FltF[11] = g_pExceptionCtx->FltF11;
        ctx.FltF[12] = g_pExceptionCtx->FltF12;
        ctx.FltF[13] = g_pExceptionCtx->FltF13;
        ctx.FltF[14] = g_pExceptionCtx->FltF14;
        ctx.FltF[15] = g_pExceptionCtx->FltF15;
        ctx.FltF[16] = g_pExceptionCtx->FltF16;
        ctx.FltF[17] = g_pExceptionCtx->FltF17;
        ctx.FltF[18] = g_pExceptionCtx->FltF18;
        ctx.FltF[19] = g_pExceptionCtx->FltF19;
        ctx.FltF[20] = g_pExceptionCtx->FltF20;
        ctx.FltF[21] = g_pExceptionCtx->FltF21;
        ctx.FltF[22] = g_pExceptionCtx->FltF22;
        ctx.FltF[23] = g_pExceptionCtx->FltF23;
        ctx.FltF[24] = g_pExceptionCtx->FltF24;
        ctx.FltF[25] = g_pExceptionCtx->FltF25;
        ctx.FltF[26] = g_pExceptionCtx->FltF26;
        ctx.FltF[27] = g_pExceptionCtx->FltF27;
        ctx.FltF[28] = g_pExceptionCtx->FltF28;
        ctx.FltF[29] = g_pExceptionCtx->FltF29;
        ctx.FltF[30] = g_pExceptionCtx->FltF30;
        ctx.FltF[31] = g_pExceptionCtx->FltF31;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
#endif // MIPS_HAS_FPU

#elif defined (SHx)

    if (SUCCEEDED (hr))
    {
        OsAxsShxIntContext ctx;

        ctx.PR = g_pExceptionCtx->PR;
        ctx.MACH = g_pExceptionCtx->MACH;
        ctx.MACL = g_pExceptionCtx->MACL;
        ctx.GBR = g_pExceptionCtx->GBR;
        ctx.R[0] = g_pExceptionCtx->R0;
        ctx.R[1] = g_pExceptionCtx->R1;
        ctx.R[2] = g_pExceptionCtx->R2;
        ctx.R[3] = g_pExceptionCtx->R3;
        ctx.R[4] = g_pExceptionCtx->R4;
        ctx.R[5] = g_pExceptionCtx->R5;
        ctx.R[6] = g_pExceptionCtx->R6;
        ctx.R[7] = g_pExceptionCtx->R7;
        ctx.R[8] = g_pExceptionCtx->R8;
        ctx.R[9] = g_pExceptionCtx->R9;
        ctx.R[10] = g_pExceptionCtx->R10;
        ctx.R[11] = g_pExceptionCtx->R11;
        ctx.R[12] = g_pExceptionCtx->R12;
        ctx.R[13] = g_pExceptionCtx->R13;
        ctx.R[14] = g_pExceptionCtx->R14;
        ctx.R[15] = g_pExceptionCtx->R15;
        ctx.Fir = g_pExceptionCtx->Fir;
        ctx.Psr = g_pExceptionCtx->Psr;

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }

#if defined (SH4)
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
        OsAxsSh4FpContext ctx;
        DWORD i;

        ctx.Fpscr = g_pExceptionCtx->Fpscr;
        ctx.Fpul = g_pExceptionCtx->Fpul;
        for (i = 0; i < OSAXS_NUM_SH4_F_REGS; i++)
            ctx.FRegs[i] = g_pExceptionCtx->FRegs[i];
        for (i = 0; i < OSAXS_NUM_SH4_XF_REGS; i++)
            ctx.xFRegs[i] = g_pExceptionCtx->xFRegs[i];

        hr = Write (&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
#endif

#elif defined (x86)
    if (SUCCEEDED (hr))
    {
        OsAxsX86IntContext ctx;

        ctx.Gs = g_pExceptionCtx->SegGs;
        ctx.Fs = g_pExceptionCtx->SegFs;
        ctx.Es = g_pExceptionCtx->SegEs;
        ctx.Ds = g_pExceptionCtx->SegDs;
        ctx.Edi = g_pExceptionCtx->Edi;
        ctx.Esi = g_pExceptionCtx->Esi;
        ctx.Ebx = g_pExceptionCtx->Ebx;
        ctx.Edx = g_pExceptionCtx->Edx;
        ctx.Ecx = g_pExceptionCtx->Ecx;
        ctx.Eax = g_pExceptionCtx->Eax;
        ctx.Ebp = g_pExceptionCtx->Ebp;
        ctx.Eip = g_pExceptionCtx->Eip;
        ctx.Cs = g_pExceptionCtx->SegCs;
        ctx.Eflags = g_pExceptionCtx->EFlags;
        ctx.Esp = g_pExceptionCtx->Esp;
        ctx.Ss = g_pExceptionCtx->SegSs;

        hr = Write(&ctx, sizeof (ctx), riOut, cbOut, pbOut);
    }
    if (SUCCEEDED (hr) && !fIntegerOnly)
    {
        OsAxsX86FpContext ctx;

        ctx.ControlWord = g_pExceptionCtx->FloatSave.ControlWord;
        ctx.StatusWord = g_pExceptionCtx->FloatSave.StatusWord;
        ctx.TagWord = g_pExceptionCtx->FloatSave.TagWord;
        ctx.ErrorOffset = g_pExceptionCtx->FloatSave.ErrorOffset;
        ctx.ErrorSelector = g_pExceptionCtx->FloatSave.ErrorSelector;
        ctx.DataOffset = g_pExceptionCtx->FloatSave.DataOffset;
        ctx.DataSelector = g_pExceptionCtx->FloatSave.DataSelector;

        memcpy (&ctx.RegisterArea, &g_pExceptionCtx->FloatSave.RegisterArea, sizeof (ctx.RegisterArea));

        ctx.Cr0NpxState = g_pExceptionCtx->FloatSave.Cr0NpxState;

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
    DWORD cbBuf;
    DWORD i = 0, iHeader;
    DWORD dwElementRva = 0;
    
    if (pcbBuf == NULL || pbBuf == NULL)
        hr = E_INVALIDARG;

    if (SUCCEEDED (hr))
        cbBuf = *pcbBuf;
        
    if (SUCCEEDED (hr) && (dwFlags & GET_CTX_HDR))
    {
        iHeader = i;
        hr = WritePTMResponseHeader(CFIELDS, 1, dwElementRva, i, cbBuf, pbBuf);
    }

    if (SUCCEEDED (hr) && (dwFlags & GET_CTX_DESC))
        hr = WriteFieldInfo (FIELDLIST, CFIELDS, i, cbBuf, pbBuf);

    if (SUCCEEDED (hr) && (dwFlags & GET_CTX))
    {
        dwElementRva = i;
        hr = WriteExceptionContext (i, cbBuf, pbBuf, FALSE);
    }

    if (SUCCEEDED (hr) && (dwFlags & GET_CTX_HDR))
        hr = WritePTMResponseHeader(CFIELDS, 1, dwElementRva, iHeader, cbBuf, pbBuf);

    if (SUCCEEDED (hr))
        *pcbBuf = i;

    return hr;
}


