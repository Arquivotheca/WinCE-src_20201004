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

    armstep.cpp

Abstract:

    ARM single step logic

Environment:

    OsaxsT, OsaxsH

--*/

#ifndef BUILDING_UNIT_TEST
#include "osaxs_p.h"
#include "step.h"
#include "arminst.h"

#define ReadEADword ReadDword
#define ReadInstDword ReadDword
#else
#include <nk\inc\arminst.h>
#endif


extern BOOL ThumbComputeNextOffset(ULONG *, ULONG *, BOOL *, BOOL *);

//
// From CRT, removing dependency on needing a full crt installed.
//
static unsigned dbg_rotl (unsigned val, int shift) {
    register unsigned hibit;    /* non-zero means hi bit set */
    register unsigned num = val;    /* number to rotate */
    shift &= 0x1f;      /* modulo 32 -- this will also make negative shifts work */
    while (shift--) {
        hibit = num & 0x80000000;  /* get high bit */
        num <<= 1;      /* shift left one bit */
        if (hibit)
            num |= 1;   /* set lo bit if hi bit was set */
    }

    return num;
}

static unsigned long dbg_lrotl(unsigned long val,	int shift) {
    return( (unsigned long) dbg_rotl((unsigned) val, shift) );
}

static unsigned dbg_rotr(unsigned val, int shift) {
    register unsigned lobit;    /* non-zero means lo bit set */
    register unsigned num = val;    /* number to rotate */
    shift &= 0x1f;          /* modulo 32 -- this will also make
                                negative shifts work */
    while (shift--) {
        lobit = num & 1;    /* get high bit */
        num >>= 1;      /* shift right one bit */
        if (lobit)
            num |= 0x80000000;  /* set hi bit if lo bit was set */
    }
    return num;
}

static unsigned long dbg_lrotr (unsigned long val, int shift) {
    return( (unsigned long) dbg_rotr((unsigned) val, shift));
}


const DWORD NFlag = 0x80000000ul;
const DWORD ZFlag = 0x40000000ul;
const DWORD CFlag = 0x20000000ul;
const DWORD VFlag = 0x10000000ul;

BOOL ARMCheckConditionCodes(DWORD instr)
{
    BOOL const N = !!(DD_ExceptionState.context->Psr & NFlag);
    BOOL const Z = !!(DD_ExceptionState.context->Psr & ZFlag);
    BOOL const C = !!(DD_ExceptionState.context->Psr & CFlag);
    BOOL const V = !!(DD_ExceptionState.context->Psr & VFlag);

    switch (instr & COND_MASK)
    {
    case COND_EQ: return Z;
    case COND_NE: return !Z;
    case COND_CS: return C;
    case COND_CC: return !C;
    case COND_MI: return N;
    case COND_PL: return !N;
    case COND_VS: return V;
    case COND_VC: return !V;
    case COND_HI: return C && !Z;
    case COND_LS: return !C || Z;
    case COND_GE: return N == V;
    case COND_LT: return N != V;
    case COND_GT: return !Z && N == V;
    case COND_LE: return Z && N != V;
    case COND_AL:
    case COND_NV: return TRUE;
    default:
        KDASSERT(!"Invalid condition");
        return TRUE;
    }
}


DWORD ARMRegister(DWORD i)
{
    DWORD RegVal;
    if (i < 16)
         RegVal = (&(DD_ExceptionState.context->R0))[i];
     else
        RegVal = 0;
    return RegVal;
}


BOOL ARMComputeNextOffset(ULONG *pNextOffset, ULONG *pInstrSize, BOOL *pfIsCall, BOOL *pfIsThumb)
{
    ARMI instr;
    BOOL fResult = FALSE;

    if (DD_Currently16Bit())
    {
        // Currently T bit is set.  Go to the Thumb single step computation code.
        fResult = ThumbComputeNextOffset(pNextOffset, pInstrSize, pfIsCall, pfIsThumb);
        goto exit;
    }
    
    *pfIsCall   = FALSE;
    *pfIsThumb  = FALSE;
    *pInstrSize = sizeof(ARMI);

    fResult = ReadInstDword(DD_ExceptionState.context->Pc, &instr.instruction);
    if (!fResult)
    {
        goto exit;
    }

    ULONG const Cflag = !!(DD_ExceptionState.context->Psr & CFlag);
    if (!ARMCheckConditionCodes(instr.instruction))
    {
        *pNextOffset = DD_ExceptionState.context->Pc + sizeof (ARMI);
    }
    else if(((instr.instruction & BX_MASK ) == BX_INSTR) ||
                ((instr.instruction & BLX2_MASK) == BLX2_INSTR))
    {
        // The contents of bx.rn holds the target.
        // It may have the low bit set for a THUMB branch.
        *pNextOffset = ARMRegister(instr.bx.rn);
        *pfIsThumb   = *pNextOffset & 1;
        *pNextOffset&= ~1;
    }
    else if ((instr.instruction & DATA_PROC_MASK ) == DP_PC_INSTR)
    {
        // We are not making sure that data processing instructions are not the
        // multiply instructions, because we are checking to make sure that the
        // PC is the destination register. The PC is not a legal destination
        // register on multiply instructions.
        //
        // Currently only the MOV instruction (returns, branches) and the ADDLS
        // instruction (switch statement) are used.  Both of these instructions
        // use the addressing mode "Register, Logical shift left by immediate."
        // I'm leaving the other cases in case they are used in the future.

        // Figure out the addressing mode (there are 11 of them), and get the
        // operands.

        DWORD Op1 = ARMRegister(instr.dataproc.rn) + ((instr.dataproc.rn == 15)? 8 : 0);
        DWORD Op2;
        if (instr.dataproc.bits == 0x1)
        {
            // Immediate addressing - Type 1
            Op2 = dbg_lrotr( instr.dpi.immediate, instr.dpi.rotate * 2 );
        }
        else
        {
            Op2 = ARMRegister(instr.dpshi.rm) + ((instr.dpshi.rm == 15)? 8 : 0);
            if( instr.dprre.bits == 0x6 )
            {
                // Rotate right with extended - Type 11
                Op2 = ( Cflag << 31 ) | ( Op2 >> 1 );
            }
            else if ( instr.dataproc.operand2 & 0x10 )
            {
                // Register shifts. Types 4, 6, 8, and 10
                DWORD const shift = ARMRegister(instr.dpshr.rs) & 0xff;

                switch( instr.dpshr.bits )
                {
                case 0x1: //  4 Logical shift left by register
                    if (shift >= 32) Op2 = 0;
                    else             Op2 <<= shift;
                    break;
                case 0x3: //  6 Logical shift right by register
                    if (shift >= 32) Op2 = 0;
                    else             Op2 >>= shift;
                    break;
                case 0x5: //  8 Arithmetic shift right by register
                    Op2 = static_cast<int>(Op2) >> shift;
                    break;
                case 0x7: // 10 Rotate right by register
                    if( !( shift == 0 ) && !(( shift & 0xf ) == 0 ) )
                    {
                        Op2 = dbg_lrotl( Op2, shift );
                    }
                default:
                    break;
                }
            }
            else
            {
                // Immediate shifts. Types 2, 3, 5, 7, and 9

                // Get the shift value from the instruction.
                DWORD const shift = instr.dpshi.shift;
                switch (instr.dpshi.bits)
                {
                case 0x0: // 2,3 Register, Logical shift left by immediate
                    if( shift != 0 )
                        Op2 = Op2 << shift;
                    break;
                case 0x2: // 5 Logical shift right by immediate
                    if (shift == 0) Op2 = 0;
                    else            Op2 >>= shift;
                    break;
                case 0x4: // 7 Arithmetic shift right by immediate
                    if( shift == 0 ) Op2 = 0;
                    else             Op2 = static_cast<int>(Op2) >> shift;
                    break;
                case 0x6: // 9 Rotate right by immediate
                    Op2 = dbg_lrotl( Op2, shift );
                    break;
                default:
                    break;
                }
            }
        }

        // Determine the result (the new PC), based on the opcode.
        switch ( instr.dataproc.opcode )
        {
        case OP_AND:    *pNextOffset = Op1 & Op2;           break;
        case OP_EOR:    *pNextOffset = Op1 ^ Op2;           break;
        case OP_SUB:    *pNextOffset = Op1 - Op2;           break;
        case OP_RSB:    *pNextOffset = Op2 - Op1;           break;
        case OP_ADD:    *pNextOffset = Op1 + Op2;           break;
        case OP_ADC:    *pNextOffset = (Op1 + Op2) + Cflag; break;
        case OP_SBC:    *pNextOffset = (Op1 - Op2) - ~Cflag;break;
        case OP_RSC:    *pNextOffset = (Op2 - Op1) - ~Cflag;break;
        case OP_ORR:    *pNextOffset = Op1 | Op2;           break;
        case OP_MOV:    *pNextOffset = Op2;
                        *pfIsCall  = (instr.dataproc.operand2 != 0xE);
                        break;
        case OP_BIC:    *pNextOffset = Op1 & ~Op2;          break;
        case OP_MVN:    *pNextOffset = ~Op2;                break;

        case OP_TST:
        case OP_TEQ:
        case OP_CMP:
        case OP_CMN:
        default:        *pNextOffset = ARMRegister(15) + sizeof(ARMI);
                        break;
        }
    }
    else if (( instr.instruction & LDM_PC_MASK ) == LDM_PC_INSTR )
    {
        // ie: ldmia     sp!, {pc}
        // Load multiple with the PC bit set.  We don't need to check the
        // step over flag in this case, because a load multiple is never a
        // call, only a return.
        DWORD Rn = ARMRegister(instr.ldm.rn);
        if (instr.ldm.u)
        {
            // Increment the address. Check to see how many other registers
            // are to be read.
            DWORD RegList = instr.ldm.reglist;
            DWORD count = 0;
            for (DWORD i = 0; i < 15; i++)
            {
                count += RegList & 0x1;
                RegList = RegList >> 1;
            }

            // Check the p bit to see how big to make the offset to the PC.
            count = (count + instr.ldm.p) * sizeof(ARMI);
            Rn += count;
        }
        else
        {
            // Decrement the address.  If we decrement before, we need to
            // subract the instruction size now.  Otherwise, do nothing.
            Rn -= instr.ldm.p * sizeof(ARMI);
        }
        
        fResult = ReadEADword(Rn, pNextOffset);
        if (!fResult)
        {
            goto exit;
        }

    }
    else if ((instr.instruction & 0xFE000000) == 0xFA000000)
    {
        // ARM BLX imm
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------------+-+-----------------------------------------------+
        //   |1 1 1 1 1 0 1|H|                  imm24                        |
        //   +-------------+-+-----------------------------------------------+
        //
        *pfIsCall = TRUE;
        *pfIsThumb = TRUE;
        LONG imm32 = ((instr.instruction & 0x00FFFFFF) << 2) |
                     ((instr.instruction >> 23) & 0x2);
        if (imm32 & 0x02000000)
        {
            imm32 |= 0xFC000000;
        }
        *pNextOffset = DD_ExceptionState.context->Pc + 8 + imm32;
    }
    else if ((( instr.instruction & B_BL_MASK ) == B_INSTR ) ||
              (( instr.instruction & B_BL_MASK ) == BL_INSTR ))
    {
        *pfIsCall = instr.bl.link;

        // To calculate the branch target:
        //      Shift the 24-bit offset left 2 bits
        //      Sign-extend it to 32 bits
        //  Add it to the contents of the PC
        //      (Which would be the current address + 8);
        LONG BranchOffset = instr.bl.offset;
        BranchOffset <<= 2;
        if (BranchOffset & 0x2000000)
        {
            BranchOffset |= 0xfc000000;
        }
        *pNextOffset = DD_ExceptionState.context->Pc + 8 +  BranchOffset;
    }
    else if ( instr.instruction == LDR_THUNK_2 )
    {
        // Need to handle import DLL thunk type branches to destination func Foo
        //
        // 0001ACA0: ldr    r12, [pc]   ;  pc+8+0 = 0x0001ACA8
        // 0001ACA4: ldr    pc, [r12]
        // 0001ACA8: DCD    0x0001C020  ; This memory location holds the address, of the address, of Foo
        //
        // 0001C020: DCD    Foo         ; This memory location holds the address of Foo

        // Get the address of Foo from Rn.
        // simple register indirect. no offsets, no scaling, no indexing addressing mode
        DWORD addr = ARMRegister(instr.ldr.rn);
        fResult = ReadEADword(addr, pNextOffset);
        if (!fResult)
        {
            goto exit;
        }
    }
    else if ((instr.instruction & LDR_PC_MASK) == LDR_PC_RET_INSTR)
    {
        // LDR PC, bleh
        // Need to handle all modes, because it can change.
        // At the moment, the compiler should be only generating LDR PC, [SP], #4
        ULONG Rn = ARMRegister(instr.ldr.rn);
        if (instr.ldr.p)
        {
            // If the index is going to be preindexed, then we should calculate what
            // the adjusted address is going to be.  Otherwise, just use the value
            // from the register rn directly.
            if (!instr.ldr.i)
            {
                if (instr.ldr.u) Rn += instr.ldr.offset;
                else             Rn -= instr.ldr.offset;
            }
            else
            {
                DWORD Rm = ARMRegister(instr.ldrreg.rm);
                switch (instr.ldrreg.shift)
                {
                case 0x0: // LSL
                    Rm = Rm << instr.ldrreg.shift_imm;
                    break;
                case 0x1: // LSR
                    Rm = Rm >> instr.ldrreg.shift_imm;
                    break;
                case 0x2: // ASR
                    if (instr.ldrreg.shift_imm == 0)
                    {
                        // special case #32
                        if (Rm & 0x80000000L)
                            Rm = 0xFFFFFFFF;
                        else
                            Rm = 0;
                    }
                    else
                    {
                        ULONG OrigRm = Rm;
                        Rm = Rm >> instr.ldrreg.shift_imm;
                        if (OrigRm & 0x80000000L)
                        {
                            Rm = Rm | ~(0xFFFFFFFF >> instr.ldrreg.shift_imm);
                        }
                    }
                    break;

                case 0x3: // ROR or RRX
                    if (instr.ldrreg.shift_imm == 0)
                    {
                        Rm = (Rm >> 1) | ((Cflag)? 0x80000000ul : 0);
                    }
                    else
                    {
                        ULONG WrapAround = Rm & ~(0xFFFFFFFF << instr.ldrreg.shift_imm);
                        Rm = (WrapAround << (32 - instr.ldrreg.shift_imm)) | (Rm >> instr.ldrreg.shift_imm);
                    }
                    break;
                }

                if (instr.ldrreg.u) Rn += Rm;
                else                Rn -= Rm;
            }
        }

        // At this point, Rn now contains the address to access the data from.
        KDASSERT(!instr.ldr.b);
        fResult = ReadEADword(Rn, pNextOffset);
        if (!fResult)
        {
            goto exit;
        }
    }
    else
    {
        *pNextOffset = DD_ExceptionState.context->Pc + sizeof (ARMI);
    }

    fResult = TRUE;

exit:
    return fResult;
}


#ifndef BUILDING_UNIT_TEST
HRESULT SingleStep::CpuSetup()
{
    DEBUGGERMSG(OXZONE_STEP, (L"+SS:arm\r\n"));

    HRESULT hr;
    ULONG InstSize, NextPC;
    BOOL fIsCall;
    BOOL fIsTarget16Bit;
    BOOL fResult;
               
    fResult = ARMComputeNextOffset(&NextPC, &InstSize, &fIsCall, &fIsTarget16Bit);
    if (!fResult)
    {
        hr = E_FAIL;
        goto Exit;
    }

    DEBUGGERMSG(OXZONE_STEP, (L" SS:arm, nextPC=%08X, Thumb=%d, Call=%d\r\n", NextPC, fIsTarget16Bit, fIsCall));

    // Handle PSL call / return
    ULONG TranslatePC = DD_TranslateOffset(NextPC);
    if (TranslatePC != NextPC)
    {
        fIsCall = TRUE;
        NextPC = TranslatePC;
    }
    NextPC &= CLEAR_BIT0;

    if(IsPCInIlockAddr(NextPC))
    {
        //disallow bp and set on ret address.
        //Set NextPC == lr
        NextPC = DD_ExceptionState.context->Lr;
    }        

    // Set the breakpoint for single step.
    hr = SetBP((void *) NextPC, fIsTarget16Bit, &bp);
    if (FAILED(hr))
    {
        DEBUGGERMSG(OXZONE_ALERT, (L" SS:arm, Unable to set bp on next pc, hr=%08X\r\n", hr));
        if(!fIsCall)
        {
            goto Error;
        }
    }

    // Set the safety breakpoint 
    if (fIsCall)
    {
        ULONG SafetyPC       = DD_ExceptionState.context->Pc + InstSize;

        BOOL fCurrently16Bit = DD_Currently16Bit();
        SafetyPC &= CLEAR_BIT0;

        DEBUGGERMSG(OXZONE_STEP, (L" SS:arm, safetyPC=%08X, fThumb=%d\r\n", SafetyPC, fCurrently16Bit));
        hr = SetBP((void *) SafetyPC, fCurrently16Bit, &safetybp); 
        if (FAILED(hr))
        {
            DEBUGGERMSG(OXZONE_ALERT, (L" SS:arm, Unable to set safetybp, hr=%08X\r\n", hr));
        }
    }
    
    hr = S_OK;

Exit:
    DEBUGGERMSG(OXZONE_STEP, (L"-SS:arm, hr=%08X\r\n", hr));
    return hr;

Error:
    DEBUGGERMSG(OXZONE_ALERT, (L" SS:arm Failed, %08X\r\n", hr));
    goto Exit;
    
}

HRESULT SingleStep::GetNextPC(ULONG *Pc)
{
    BOOL fIsCall, fIsThumb, fResult;
    ULONG Size;

    fResult = ARMComputeNextOffset(Pc, &Size, &fIsCall, &fIsThumb);
    return fResult? S_OK : E_FAIL;
}

#define THUMB2_IT_ORIGINAL_TRANSFER_BIT 0x04000000
#define THUMB2_IT_SHIFTED_TRANSFER_BIT 0x00000400;
#define THUMB2_IT_HIGH_BITS 0x06000000
#define THUMB2_IT_LOW_BITS 0x00001C00
void SingleStep::SimulateDebugBreakExecution(void)
{
    
    // Simulate executing the debug break in the IT bits of the CPSR
    BOOL bSetTransferBit = FALSE;
    DWORD dwPSR = DD_ExceptionState.context->Psr;
    
    if((dwPSR & THUMB2_IT_ORIGINAL_TRANSFER_BIT) != 0)
    {
        bSetTransferBit = TRUE;
    }

    dwPSR = (dwPSR & ~THUMB2_IT_HIGH_BITS) | (((dwPSR & THUMB2_IT_HIGH_BITS)  << 1) & THUMB2_IT_HIGH_BITS);
    dwPSR = (dwPSR & ~THUMB2_IT_LOW_BITS) | (((dwPSR & THUMB2_IT_LOW_BITS) << 1) & THUMB2_IT_LOW_BITS);

    if(bSetTransferBit == TRUE)
    {
        dwPSR |= THUMB2_IT_SHIFTED_TRANSFER_BIT;
    }

    DD_ExceptionState.context->Psr = dwPSR;
}

#endif
