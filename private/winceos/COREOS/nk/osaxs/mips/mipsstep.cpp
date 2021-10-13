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
#include "osaxs_p.h"
#include "step.h"
#include "mipsinst.h"
#else
#include <nk\inc\mipsinst.h>
#include <nk\inc\m16inst.h>
#endif


#ifndef BUILDING_UNIT_TEST
#define ReadInstDword ReadDword
#endif

#ifdef _MIPS64
typedef signed __int64 SREG_TYPE;
#else
typedef signed long SREG_TYPE;
#endif

SREG_TYPE MIPSRegister(ULONG i)
{
    if (i == 0) return 0;
    if (i < 32) return (&DD_ExceptionState.context->IntZero)[i];
    return 0;
}

SREG_TYPE MIPS16Register(ULONG i)
{
    switch (i)
    {
    case 0: return DD_ExceptionState.context->IntS0;
    case 1: return DD_ExceptionState.context->IntS1;
    case 2: return DD_ExceptionState.context->IntV0;
    case 3: return DD_ExceptionState.context->IntV1;
    case 4: return DD_ExceptionState.context->IntA0;
    case 5: return DD_ExceptionState.context->IntA1;
    case 6: return DD_ExceptionState.context->IntA2;
    case 7: return DD_ExceptionState.context->IntA3;
    }
    return 0;
}

ULONG MIPSComputeNextOffset(BOOL *pfIsCall, BOOL *pfTarget16)
{
    MIPS_INSTRUCTION Inst;

    *pfIsCall = FALSE;

    // LSB indicates mode, and the address is always even.  Mask off
    // before reading the instruction.
    ReadInstDword(DD_ExceptionState.context->Fir & ~1, &Inst.Long);

    if (!(DD_ExceptionState.context->Fir & 1))
    { // MIPS32
        const SREG_TYPE rs = MIPSRegister(Inst.r_format.Rs);
        const SREG_TYPE rt = MIPSRegister(Inst.r_format.Rt);
        const ULONG fir = DD_ExceptionState.context->Fir;
        const ULONG delaySlot = fir + sizeof(ULONG);
        const ULONG postDelaySlot = fir + sizeof(ULONG) * 2;

        *pfTarget16 = FALSE;
        if (Inst.Long == SYSCALL_OP)
        {
            // stepping over a syscall instruction must set the breakpoint
            // at the caller's return address, not the inst after the syscall
            return (DWORD) DD_ExceptionState.context->IntRa;
        }

        switch (Inst.j_format.Opcode)
        {
        case SPEC_OP:
                if (Inst.r_format.Function == JALR_OP ||
                    Inst.r_format.Function == JR_OP)
                {
                    *pfIsCall   = (Inst.r_format.Function == JALR_OP);
                    *pfTarget16 = (rs & 1) == 1;
                    return rs & ~1;                    
                }
                return delaySlot;

        case JALX_OP:
        case JAL_OP:
        case J_OP:
			*pfTarget16 = (Inst.j_format.Opcode == JALX_OP);
			*pfIsCall = (Inst.j_format.Opcode == JALX_OP) ||
					(Inst.j_format.Opcode == JAL_OP);
            return (delaySlot & 0xF0000000UL) | (Inst.j_format.Target << 2);
 
        case BCOND_OP:
            switch (Inst.i_format.Rt)
            {
            case BLTZALL_OP:
            case BLTZAL_OP:
            case BLTZ_OP:
            case BLTZL_OP:
                if (rs < 0)
				{
				    *pfIsCall = (Inst.j_format.Opcode == BLTZALL_OP) ||
							(Inst.j_format.Opcode == BLTZAL_OP);
					return delaySlot + (Inst.i_format.Simmediate << 2);
				}
				return postDelaySlot;

            case BGEZAL_OP:
            case BGEZALL_OP:
            case BGEZ_OP:
            case BGEZL_OP:
				if (rs >= 0)
				{
				    *pfIsCall = (Inst.j_format.Opcode == BGEZAL_OP) ||
							(Inst.j_format.Opcode == BGEZALL_OP);
                    return delaySlot + (Inst.i_format.Simmediate << 2);
				}
                return postDelaySlot;

            default:
				return postDelaySlot;
            }

        case BEQ_OP:
        case BEQL_OP:
			return (rs == rt)?
                    delaySlot + (Inst.i_format.Simmediate << 2) :
					postDelaySlot;

        case BNE_OP:
        case BNEL_OP:
			return (rs != rt)?
					delaySlot + (Inst.i_format.Simmediate << 2) :
					postDelaySlot;

        case BLEZ_OP:
        case BLEZL_OP:
            return (rs <= 0)?
					delaySlot + (Inst.i_format.Simmediate << 2) :
					postDelaySlot;

        case BGTZ_OP:
        case BGTZL_OP:
            return (rs > 0)?
					delaySlot + (Inst.i_format.Simmediate << 2) :
					postDelaySlot;

        case COP1_OP:
            if (Inst.ci_format.Bcc1 == COP_BC_OP &&
					Inst.ci_format.Cc == 0)
            {
				const REG_TYPE Fsr = DD_ExceptionState.context->Fsr;
                if (Inst.ci_format.Tf == ((Fsr >> 23)&1))
                    return delaySlot + Inst.ci_format.Offset;
                return postDelaySlot;
            }
            return delaySlot;

        default:
            return delaySlot;
        }
    }
    else
    { // MIPS16
        MIPS16_INSTRUCTION Inst16;
        MIPS16_INSTRUCTION NextInst;
        
        Inst16.Short = Inst.Short[1];
        NextInst.Short = Inst.Short[0];
        
        const ULONG fir = DD_ExceptionState.context->Fir & ~1;
        const ULONG delaySlot = fir + sizeof(USHORT);
        const ULONG postDelaySlot = fir + sizeof(USHORT) * 2;

        const REG_TYPE rx = MIPS16Register(Inst16.ri_format.Rx);
        const REG_TYPE t = DD_ExceptionState.context->IntT8;

        *pfTarget16 = TRUE;

        switch (Inst16.rr_format.Opcode)
        {
        case RR_OP16:
            if (Inst16.rr_format.Function)
                return delaySlot;

            switch (Inst16.rr_format.Ry)
            {
            case JALR_OP16:
            case JRRX_OP16:
				*pfIsCall = Inst16.rr_format.Ry == JALR_OP16;
				*pfTarget16 = (rx & 1) == 1;
				return rx & ~1;

            case JRRA_OP16:
				*pfTarget16 = DD_ExceptionState.context->IntRa & 1;
				return DD_ExceptionState.context->IntRa & ~1;

            default:
				return delaySlot;
            }

        case BEQZ_OP16:
            return (rx == 0)?
					delaySlot + (((int)Inst16.ri_format.Immediate) << 1) :
					delaySlot;

        case BNEZ_OP16:
			return (rx != 0)?
					delaySlot + (((int)Inst16.ri_format.Immediate) << 1) :
					delaySlot;

        case I8_OP16:
            switch (Inst16.i8_format.Function)
            {
            case BTEQZ_OP16:
				return (t == 0)?
						delaySlot + (((int)Inst16.i8_format.Immediate) << 1) :
						delaySlot;

            case BTNEZ_OP16:
				return (t != 0)?
						delaySlot + (((int)Inst16.i8_format.Immediate) << 1) :
						delaySlot;

            default:
				return delaySlot;
            }

        case B_OP16:
            return delaySlot + (((int)Inst16.i_format.Immediate) << 1);

        case JAL_OP16:
            *pfIsCall = TRUE;
            *pfTarget16 = !Inst16.j_format.Ext;
            return (fir & 0xF0000000) |
                    (((Inst16.j_format.Target21 << 21) |
                      (Inst16.j_format.Target16 << 16) |
                      NextInst.Short) << 2);

        case EXTEND_OP16:
            switch (NextInst.ei_format.Opcode)
            {
            case BEQZ_OP16:
                if (MIPS16Register(NextInst.rri_format.Rx) == 0)
                    return postDelaySlot +
                        ((Inst16.ei_format.Simmediate11 << 12) |
                         (Inst16.ei_format.Immediate5 << 6) |
                         (NextInst.rri_format.Immediate << 1));
                else
                    return postDelaySlot;
            case BNEZ_OP16:
                if (MIPS16Register(NextInst.rri_format.Rx) != 0)
                    return postDelaySlot +
                        ((Inst16.ei_format.Simmediate11 << 12) |
                         (Inst16.ei_format.Immediate5 << 6) |
                         (NextInst.rri_format.Immediate << 1));
                else
                    return postDelaySlot;

            case I8_OP16:
                switch (NextInst.rri_format.Rx)
                {
                case BTEQZ_OP16:
                    if (t == 0)
                        return postDelaySlot +
                            ((Inst16.ei_format.Simmediate11 << 12) |
                             (Inst16.ei_format.Immediate5 << 6) |
                             (NextInst.rri_format.Immediate << 1));
                    else
                        return postDelaySlot;
                case BTNEZ_OP16:
                    if (t != 0)
                        return postDelaySlot +
                            ((Inst16.ei_format.Simmediate11 << 12) |
                             (Inst16.ei_format.Immediate5 << 6) |
                             (NextInst.rri_format.Immediate << 1));
                    else
                        return postDelaySlot;
                default:
                    return postDelaySlot;
                }

            case B_OP16:
                return postDelaySlot +
                    ((Inst16.ei_format.Simmediate11 << 12) |
                     (Inst16.ei_format.Immediate5 << 6) |
                     (NextInst.rri_format.Immediate << 1));

            default:
                return postDelaySlot;
            }

        default:
            return delaySlot;
        }
    }
}

#ifndef BUILDING_UNIT_TEST
HRESULT SingleStep::CpuSetup()
{
    HRESULT hr;

    BOOL iscall;
    BOOL is16;
    ULONG next = MIPSComputeNextOffset(&iscall, &is16);
    if (SingleStep::IsPSLCall(next))
    {
        iscall = TRUE;
    }

    next = DD_TranslateOffset(next);

    hr = SetBP((void *) next, is16, &bp);
    if (FAILED(hr))
    {
        goto Error;
    }

    if (iscall)
    {
        const BOOL SixteenBit = DD_Currently16Bit();
        const ULONG InstSize = SixteenBit? sizeof(USHORT):sizeof(ULONG);
        const ULONG Pc = DD_ExceptionState.context->Fir & CLEAR_BIT0;
        ULONG SafetyPC = Pc + InstSize * 2;

        hr = SetBP((void *) SafetyPC, SixteenBit, &safetybp);
        if (FAILED(hr))
        {
            goto Error;
        }
    }
    
    hr = S_OK;
Exit:
    return hr;
Error:
    goto Exit;
}


HRESULT SingleStep::GetNextPC(ULONG *pc)
{
    BOOL fIsCall, fIs16;
    *pc = MIPSComputeNextOffset(&fIsCall, &fIs16);
    return S_OK;
}

void SingleStep::SimulateDebugBreakExecution(void)
{
    return;
}
#endif
