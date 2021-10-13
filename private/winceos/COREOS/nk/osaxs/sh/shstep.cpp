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

#ifndef BUILDING_UNIT_TEST
#include <osaxs_p.h>
#include <step.h>
#include <shxinst.h>
#endif

#ifndef BUILDING_UNIT_TEST
#define ReadInstWord ReadWord
#endif


ULONG SHRegister(ULONG i)
{
    return (&DD_ExceptionState.context->R0)[i];
}

LONG SignExtend(ULONG i, ULONG b)
{
    KDASSERT(b <= 32);
    ULONG msb = (1 << (b - 1));
    ULONG mask = (b >= 32)? -1 : (1 << b) - 1;
    if (msb & i) return ~mask | i;
    else          return i;
}

ULONG SHComputeNextOffset(BOOL *pfIsCall)
{
    SH3IW Inst;
    ULONG const fir = DD_ExceptionState.context->Fir;
    ULONG const sr = DD_ExceptionState.context->Psr;

    *pfIsCall = FALSE;
    ReadInstWord(fir, (WORD *)&Inst.instruction);

    switch (Inst.instr1.opcode)
    {
    case 0:
        switch (Inst.instr1.function)
        {
        case 0x03:      // BSRF (2#0000nnnn00100011)
        case 0x23:      // BRAF (2#0000nnnn00000011)
            *pfIsCall = (Inst.instr1.function == 0x03);
            return fir + SHRegister(Inst.instr1.reg) + sizeof(SH3IW)*2;
            
        case 0x0B:      // Check for RTS
            return (Inst.instruction == RTS)?
                    DD_ExceptionState.context->PR :
                    fir + sizeof(SH3IW);

        default:
            return fir + sizeof(SH3IW);
        }
        
    case 0x4:
        switch (Inst.instr1.function)
        {
        case 0x0B:  // JSR (2#0100nnnn00001011)
        case 0x2B:  // JMP (2#0100nnnn00101011)
            *pfIsCall = (Inst.instr1.function == 0x0B);
            return SHRegister(Inst.instr1.reg);
            
        default:
            return fir + sizeof(SH3IW);
        }
        
    case 0x8:
        switch (Inst.instr2.reg)
        {
        case 0x9:       // BT (2#10001001dddddddd)
            return (sr & 1)?
                    fir + sizeof(SH3IW)*2 +
                        (SignExtend(Inst.instr5.imm_disp, 8) << 1) :
                    fir + sizeof(SH3IW);

        case 0xB:       // BF (2#10001011dddddddd)
            return (!(sr & 1))?
                    fir + sizeof(SH3IW)*2 +
                        (SignExtend(Inst.instr5.imm_disp, 8) << 1) :
                    fir + sizeof(SH3IW);

        case 0xD:       // BT/S (2#10001101dddddddd)
            return (sr & 1)?
                    fir + sizeof(SH3IW)*2 +
                        (SignExtend(Inst.instr5.imm_disp, 8) << 1) :
                    fir + sizeof(SH3IW)*2;

        case 0xF:       // BF/S (2#10001111dddddddd)
            return (!(sr & 1))?
                    fir + sizeof(SH3IW)*2 +
                        (SignExtend(Inst.instr5.imm_disp, 8) << 1) :
                    fir + sizeof(SH3IW)*2;
        default:
            return fir + sizeof(SH3IW);
        }
        
    case 0xB: // BSR (2#1011dddddddddddd)
    case 0xA: // BRA (2#1010dddddddddddd)
        *pfIsCall = (Inst.instr1.opcode == 0xB);
        return fir + (SignExtend(Inst.instr7.disp, 12) << 1) + sizeof(SH3IW)*2;

        
    default:
        return fir + sizeof(SH3IW);
    }
}

#ifndef BUILDING_UNIT_TEST
HRESULT SingleStep::CpuSetup()
{
    HRESULT hr;
    
    BOOL fIsCall;
    ULONG next = SHComputeNextOffset(&fIsCall);
    ULONG TranslateOffset = DD_TranslateOffset(next);
    if (TranslateOffset != next)
    {
        fIsCall = TRUE;
        next = TranslateOffset;
    }

    hr = SetBP((void *)next, FALSE, &bp);
    if (FAILED(hr))
    {
        goto Error;
    }

    if (fIsCall)
    {
        ULONG SafetyPC = DD_ExceptionState.context->Fir + sizeof(USHORT)*2;
        hr = SetBP((void *) SafetyPC, FALSE, &safetybp);
        if (FAILED(hr))
        {
            goto Error;
        }
    }

    hr = S_OK;

Exit:
    return hr;

Error:
    DBGRETAILMSG(OXZONE_ALERT, (L"  SS:Cpu, failed hr=%08X\r\n", hr));
    goto Exit;
}

HRESULT SingleStep::GetNextPC(ULONG *Pc)
{
    HRESULT hr;
    BOOL IsCall;
    
    *Pc = SHComputeNextOffset(&IsCall);
    hr = S_OK;

    return hr;
}

void SingleStep::SimulateDebugBreakExecution(void)
{
    return;
}
#endif
