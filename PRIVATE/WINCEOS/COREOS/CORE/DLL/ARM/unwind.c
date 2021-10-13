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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++

Module Name:
    unwind.c

Abstract:
    This module implements the unwinding of procedure call frames for
    exception processing.
--*/
#if defined(UNDER_CE)
#include <kernel.h>
#include <coredll.h>
#include <arminst.h>
#include <stdlib.h>

// force the compiler to inline these to remove a dependency on the CRT
#pragma intrinsic(_lrotl, _lrotr)
#endif

#if !defined(PRIVATE)
#define PRIVATE static
#endif

#if !defined(READ_CODE)
static BOOL
READ_CODE(
    void *src,
    void *dest,
    int length
    )
{
    BOOL fRet = TRUE;

    __try {
        memcpy(dest, src, length);
        KDbgSanitize(dest, src, length);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }

    return fRet;
}
#endif

#if !defined(READ_DATA)
static BOOL
READ_DATA(
    void *src,
    void *dest,
    int length
    )
{
    BOOL fRet = TRUE;

    __try {
        memcpy(dest, src, length);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }

    return fRet;
}
#endif

#if !defined(ADJUST_NEXTPC)
#define ADJUST_NEXTPC(pc) ((pc) - 4)
#endif

//------------------------------------------------------------------------------
//
//  Routine Description:
//      This function executes an ARM data processing instruction using the
//      current register set. It automatically updates the destination register.
//
//  Arguments:
//      instr      The ARM 32-bit instruction
//
//      Register   Pointer to the ARM integer registers.
//
//  Return Value:
//      None.
//
//------------------------------------------------------------------------------
static VOID
UnwindDataProcess(
    ARMI instr,
    PULONG Register
    )
{
    ULONG Op1, Op2;
    ULONG Cflag = (Register[16] & 0x20000000L) == 0x20000000L; // CPSR
    ULONG shift;

    // We are checking for all addressing modes and op codes, even though
    // the prolog and epilog don't use them all right now.  In the future,
    // more instructions and addressing modes may be added.
    //
    // Figure out the addressing mode (there are 11 of them), and get the
    // operands.
    Op1 = Register[instr.dataproc.rn];
    if (instr.dataproc.bits == 0x1) {
        // Immediate addressing - Type 1
        Op2 = _lrotr(instr.dpi.immediate, instr.dpi.rotate * 2);

    } else {
        // Register addressing - start by getting the value of Rm.
        Op2 = Register[instr.dpshi.rm];
        if (instr.dprre.bits == 0x6) {
            // Rotate right with extended - Type 11
            Op2 = (Cflag << 31) | (Op2 >> 1);

        } else if (instr.dataproc.operand2 & 0x10) {
            // Register shifts. Types 4, 6, 8, and 10
            //
            // Get the shift value from the least-significant byte of the
            // shift register.
            shift = Register[instr.dpshr.rs] & 0xFF;
            switch(instr.dpshr.bits) {
            case 0x1: //  4 Logical shift left by register
                if (shift >= 32)
                    Op2 = 0;
                else
                    Op2 = Op2 << shift;
                break;

            case 0x3: //  6 Logical shift right by register
                if (shift >= 32)
                    Op2 = 0;
                else
                    Op2 = Op2 >> shift;
                break;

            case 0x5: //  8 Arithmetic shift right by register
                if (shift >= 32) {
                    if (Op2 & 0x80000000)
                        Op2 = 0xffffffff;
                    else
                        Op2 = 0;
                } else
                    Op2 = (LONG)Op2 >> shift;
                break;

            case 0x7: // 10 Rotate right by register
                if ((shift & 0xf) != 0)
                    Op2 = _lrotl(Op2, shift);
            default:
                break;
            }

        } else {
            // Immediate shifts. Types 2, 3, 5, 7, and 9
            //
            // Get the shift value from the instruction.
            shift = instr.dpshi.shift;
            switch(instr.dpshi.bits) {
            case 0x0: // 2,3 Register, Logical shift left by immediate
                if (shift != 0)
                    Op2 = Op2 << shift;
                break;

            case 0x2: // 5 Logical shift right by immediate
                if (shift == 0)
                    Op2 = 0;
                else
                    Op2 = Op2 >> shift;
                break;

            case 0x4: // 7 Arithmetic shift right by immediate
                if (shift == 0)
                    Op2 = 0;
                else
                    Op2 = (LONG)Op2 >> shift;
                break;

            case 0x6: // 9 Rotate right by immediate
                Op2 = _lrotl(Op2, shift);
                break;

            default:
                break;
            }
        }
    }

    // Determine the result (the new PC), based on the opcode.
    switch(instr.dataproc.opcode) {
    case OP_AND:
        Register[instr.dataproc.rd] = Op1 & Op2;
        break;

    case OP_EOR:
        Register[instr.dataproc.rd] = Op1 ^ Op2;
        break;

    case OP_SUB:
        Register[instr.dataproc.rd] = Op1 - Op2;
        break;

    case OP_RSB:
        Register[instr.dataproc.rd] = Op2 - Op1;
        break;

    case OP_ADD:
        Register[instr.dataproc.rd] = Op1 + Op2;
        break;

    case OP_ADC:
        Register[instr.dataproc.rd] = (Op1 + Op2) + Cflag;
        break;

    case OP_SBC:
        Register[instr.dataproc.rd] = (Op1 - Op2) - ~Cflag;
        break;

    case OP_RSC:
        Register[instr.dataproc.rd] = (Op2 - Op1) - ~Cflag;
        break;

    case OP_ORR:
        Register[instr.dataproc.rd] = Op1 | Op2;
        break;

    case OP_MOV:
        Register[instr.dataproc.rd] = Op2;
        break;

    case OP_BIC:
        Register[instr.dataproc.rd] = Op1 & ~Op2;
        break;

    case OP_MVN:
        Register[instr.dataproc.rd] = ~Op2;
        break;

    case OP_TST:
    case OP_TEQ:
    case OP_CMP:
    case OP_CMN:
    default:
        // These instructions do not have a destination register.
        // There is nothing to do.
        break;
    }
}



//------------------------------------------------------------------------------
//
//  Routine Description:
//      This function executes an ARM load multiple instruction.
//
//  Arguments:
//      instr           The ARM 32-bit instruction
//
//      Register        Pointer to the ARM integer registers.
//
//  Return Value:
//      TRUE if successful, FALSE otherwise.
//
//------------------------------------------------------------------------------
PRIVATE BOOL
UnwindLoadMultiple(
    ARMI instr,
    PULONG Register
    )
{
    LONG i;
    ULONG RegList;
    PULONG Rn;

    // Load multiple with the PC bit set.  We are currently checking for all
    // four addressing modes, even though decrement before and increment
    // after are the only modes currently used in the epilog and prolog.
    //
    // Rn is the address at which to begin, and RegList is the bit map of which
    // registers to read.
    Rn = (PULONG)Register[instr.ldm.rn];
    RegList = instr.ldm.reglist;
    if (instr.ldm.p) {
        if (instr.ldm.u) {
            // Increment before
            for(i = 0; i <= 15; i++) {
                if (RegList & 0x1) {
                    Rn++;
                    if (!READ_DATA(Rn, &Register[i], 4))
                        return FALSE;
                }
                RegList = RegList >> 1;
            }
        } else {
            // Decrement before
            for(i = 15; i >= 0; i--) {
                if (RegList & 0x8000) {
                    Rn--;
                    if (!READ_DATA(Rn, &Register[i], 4))
                        return FALSE;
                }
                RegList = RegList << 1;
            }
        }
    } else {
        if (instr.ldm.u) {
            // Increment after
            for(i = 0; i <= 15; i++) {
                if (RegList & 0x1) {
                    if (!READ_DATA(Rn, &Register[i], 4))
                        return FALSE;
                    Rn++;
                }
                RegList = RegList >> 1;
            }
        } else {
            // Decrement after
            for(i = 15; i >= 0; i--) {
                if (RegList & 0x8000) {
                    if (!READ_DATA(Rn, &Register[i], 4))
                        return FALSE;
                    Rn--;
                }
                RegList = RegList << 1;
            }
        }
    }

    if (instr.ldm.w)
        Register[instr.ldm.rn] = (ULONG)Rn; // Update the base register.
    return TRUE;
}



//------------------------------------------------------------------------------
//
//  Routine Description:
//      This function executes an ARM load instruction with an immediat offset.
//
//  Arguments:
//      instr           The ARM 32-bit instruction
//
//      Register        Pointer to the ARM integer registers.
//
//  Return Value:
//      TRUE if successful, FALSE otherwise.
//
//------------------------------------------------------------------------------
PRIVATE BOOL
UnwindLoadI(
    ARMI instr,
    PULONG Register
    )
{
    LONG offset;
    LONG size;
    PULONG Rn;

    Rn = (PULONG)Register[instr.ldr.rn];
    offset = instr.ldr.offset;
    if (instr.ldr.u == 0)
        offset = -offset;
    if (instr.ldr.b == 0)
        size = 4;
    else
        size = 1;

    if (instr.ldm.p) { // add offset before transfer
        if (!READ_DATA(Rn + offset, &Register[instr.ldr.rd], size))
            return FALSE;
        if (instr.ldr.w)
            Register[instr.ldr.rn] += offset;
    } else {
        if (!READ_DATA(Rn, &Register[instr.ldr.rd], size))
            return FALSE;
        if (instr.ldr.w)
            Register[instr.ldr.rn] += offset;
    }

    return TRUE;
}



//------------------------------------------------------------------------------
//
//  Routine Description:
//      Checks the condition codes of the instruction and the values of the
//      condition flags in the current program status register, and determines
//      whether or not the instruction will be executed.
//
//  Arguments:
//      CPSR    -   The value of the Current Program Status Register.
//      instr   -   The instruction to analyze.
//
//  Return Value:
//      TRUE if the instruction will be executed, FALSE otherwise.
//
//------------------------------------------------------------------------------
static BOOL
CheckConditionCodes(
    ULONG CPSR,
    DWORD instr
    )
{
    BOOL Execute = FALSE;
    BOOL Nset = (CPSR & 0x80000000L) == 0x80000000L;
    BOOL Zset = (CPSR & 0x40000000L) == 0x40000000L;
    BOOL Cset = (CPSR & 0x20000000L) == 0x20000000L;
    BOOL Vset = (CPSR & 0x10000000L) == 0x10000000L;

    switch(instr & COND_MASK) {
    case COND_EQ:   // Z set
        if (Zset) Execute = TRUE;
        break;

    case COND_NE:   // Z clear
        if (!Zset) Execute = TRUE;
        break;

    case COND_CS:   // C set
        if (Cset) Execute = TRUE;
        break;

    case COND_CC:   // C clear
        if (!Cset) Execute = TRUE;
        break;

    case COND_MI:   // N set
        if (Nset) Execute = TRUE;
        break;

    case COND_PL:   // N clear
        if (!Nset) Execute = TRUE;
        break;

    case COND_VS:   // V set
        if (Vset) Execute = TRUE;
        break;

    case COND_VC:   // V clear
        if (!Vset) Execute = TRUE;
        break;

    case COND_HI:   // C set and Z clear
        if (Cset && !Zset) Execute = TRUE;
        break;

    case COND_LS:   // C clear or Z set
        if (!Cset || Zset) Execute = TRUE;
        break;

    case COND_GE:   // N == V
        if ((Nset && Vset) || (!Nset && !Vset)) Execute = TRUE;
        break;

    case COND_LT:   // N != V
        if ((Nset && !Vset) || (!Nset && Vset)) Execute = TRUE;
        break;

    case COND_GT:   // Z clear, and N == V
        if (!Zset &&
          ((Nset && Vset) || (!Nset && !Vset))) Execute = TRUE;
        break;

    case COND_LE:   // Z set, or N != V
        if (Zset ||
          ((Nset && !Vset) || (!Nset && Vset))) Execute = TRUE;
        break;

    case COND_AL:   // Always execute
        Execute = TRUE;
        break;

    default:
    case COND_NV:   // Never - undefined.
        Execute = FALSE;
        break;
    }
    return Execute;
}


//------------------------------------------------------------------------------
//  Routine Description:
//
//      This is the Thumb specific implementation of RtlVirtualUnwind
//
//  Arguments:
//
//      See RtlVirtualUnwind
//
//  Return Value:
//
//      See RtlVirtualUnwind
//------------------------------------------------------------------------------
PRIVATE ULONG
ThumbVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT Context,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame
    )
{
    LONG    cb;
    THUMBI  Prolog[20];               // 20 instructions max. in prolog
    THUMBI  instr;
    ARMI    instrA;
    PULONG  RegPtr;
    PULONG  SrcPtr, DestPtr;
    ULONG   SpAdjReg = 0, SpAdjVal;
    ULONG   BlOffs = 0;
    ULONG   EndAddress;
    ULONG   BeginAddress;
    ULONG   RegBit;
    ULONG   Data;
    ULONG   OrigPc;
    ULONG   pcOfst;

    DEBUGMSG(DBGEH, (TEXT("Unwind: Pc=%8.8x Begin=%8.8x PrologEnd=%8.8x End=%8.8x\r\n"),
        ControlPc, FunctionEntry->BeginAddress, FunctionEntry->PrologEndAddress,
        FunctionEntry->EndAddress));

    *InFunction = FALSE;

    // Clear Thumb bits and remember original PC for error return
    OrigPc = ControlPc;
    ControlPc &= ~0x01;

    pcOfst = 0; // ControlPc - (ULONG) ZeroPtr (ControlPc);

    BeginAddress = FunctionEntry->BeginAddress & ~0x01;
    EndAddress = FunctionEntry->PrologEndAddress & ~0x01;

    // map slot 0 address to the process
    if (BeginAddress < (1 << VA_SECTION)) {
        BeginAddress += pcOfst;
        EndAddress += pcOfst;
    }

    //
    // Calculate size of prolog
    //
    cb = EndAddress - BeginAddress;
    if (cb <= 0) {
        // No prolog.
        *EstablisherFrame = Context->Sp;
        return ADJUST_NEXTPC(Context->Lr);
    }

    //
    // Check size of prolog out of range
    //
    if (cb >= sizeof Prolog) {
        ASSERT(FALSE);
        return OrigPc;
    }

    //
    // Check to see if we're in the function body
    //
    if (ControlPc >= EndAddress) {
        *InFunction = TRUE;
        ControlPc = EndAddress;
    }

    //
    // Read the prolog and return error if unable
    //
    if (!READ_CODE((PVOID)BeginAddress, (PVOID)Prolog, cb)) {
        return OrigPc;
    }

    //
    // The current "faulting" instruction has not been executed, so don't
    // reverse execute it.
    //
    ControlPc -= 2;

    //
    // Reverse execute starting with the last instruction in the prolog
    // that has been executed.
    //
    SpAdjVal = 0;                                       // No SP Sequence
    while (ControlPc >= BeginAddress) {
        //
        // Get instruction from sanitized prolog
        //
        instr = Prolog[(ControlPc - BeginAddress) / sizeof Prolog[0]];

        if (ThumbPushInst(instr.instruction)) {
            //
            //  Push zero or more GPRs or the Link Register. The GPRs to
            //  push are specified in an eight bit register mask. The L bit
            //  specifies that the LR is to be pushed.
            //
            RegPtr = &Context->R0;
            for (RegBit = 0x01; RegBit < 0x100; RegBit <<= 1) {
                if (instr.push.reglist & RegBit) {
                    if (!READ_DATA((PVOID)Context->Sp, RegPtr, 4)) {
                        return OrigPc;
                    }
                    Context->Sp += 4;

                    DEBUGMSG(DBGEH, (TEXT("Push R%d = 0x%x. Sp=0x%x\r\n"),
                                     RegPtr-&Context->R0, *RegPtr, Context->Sp));
                }
                RegPtr++;
            }

            if (instr.push.r == 1) {
                if (!READ_DATA((PVOID)Context->Sp, &Context->Lr, 4)) {
                    return OrigPc;
                }
                Context->Sp += 4;
                DEBUGMSG(DBGEH, (TEXT("Push LR = 0x%x. Sp=0x%x\r\n"),
                                 Context->Lr, Context->Sp));

            }

        } else if (ThumbAdjSPInst(instr.instruction)) {
            //
            //  Adjust the stack pointer by adding or subtracting a scaled
            //  7-bit immediate (immediate is shifted by 2 bits)
            //
            if (instr.adjsp.s) {
                Context->Sp += instr.adjsp.immed << 2;
            } else {
                Context->Sp -= instr.adjsp.immed << 2;
            }

            DEBUGMSG(DBGEH, (TEXT("SP += 0x%02x == 0x%x\r\n"),
                             instr.adjsp.immed << 2, Context->Sp));

        } else if (ThumbAddSPInst(instr.instruction)) {
            //
            //  If the stack frame size exceeds 508 bytes a sequence of
            //  instructions is used to load the stack frame size into a
            //  register and then update the stack pointer.
            //  If an Add Hi instruction is found which updates the stack
            //  pointer, record the source register until the unwind reaches
            //  the load instruction which sets the source register.
            //

            SpAdjReg = instr.spcdataproc.rm;
            SpAdjVal = 1;

            DEBUGMSG(DBGEH, (TEXT("Add Sp Instruction: Source Reg = R%d\r\n"),
                             instr.spcdataproc.rm));

        } else if (ThumbMovHiInst(instr.instruction)) {
            //
            //  Move Register Rm -> Rd.
            //
            //  One or both registers are high registers, as specified
            //  by H1 (Rd) and H2 (Rm)
            //
            RegPtr = &Context->R0;
            SrcPtr = &RegPtr[instr.spcdataproc.rm + (instr.spcdataproc.H2 * 8)];
            DestPtr = &RegPtr[instr.spcdataproc.rd + (instr.spcdataproc.H1*8)];

            DEBUGMSG(DBGEH, (TEXT("Move Hi: R%d (0x%08x) -> R%d (0x%08x)\r\n"),
                             DestPtr-RegPtr, *DestPtr,
                                SrcPtr-RegPtr, *SrcPtr));

            *SrcPtr = *DestPtr;

       } else if (ThumbLdrPCInst(instr.instruction)) {
            //
            //  Load from Literal Pool into register. This instruction is
            //  used in the prologue for functions with large stack frames.
            //  The stack frame size is loaded from the literal pool before
            //  being used to update the stack pointer.
            //
            if (SpAdjVal != 0 && (instr.ldrpc.rd == SpAdjReg)) {
                SrcPtr = (PULONG)((ControlPc & ~0x03) + 4 +
                                  (instr.ldrpc.offset * 4));

                if (!READ_DATA((PVOID)SrcPtr, &Data, 4)) {
                    return OrigPc;
                }

                Context->Sp -= (Data * SpAdjVal);   // Reverse Execute Add inst.
                SpAdjVal = 0;                       // No SP adjust sequence
            }

            DEBUGMSG(DBGEH, (TEXT("Load Literal: Addr=0x%08x  Val=0x%x\r\n"),
                             SrcPtr, Data));

        } else if (ThumbNegInst(instr.instruction)) {
            //
            //  The instruction sequence to create a large stack frame may
            //  include a negate instruction. If detected, update the SpAdjVal
            //  variable:
            //
            if (SpAdjVal != 0 && (instr.dataproc.rd == SpAdjReg)) {
                SpAdjVal = -1;
            }

            DEBUGMSG(DBGEH, (TEXT("Negate Instruction: rm=R%d rd=%d\r\n"),
                             instr.dataproc.rm, instr.dataproc.rd));

        } else if (ThumbBlPrefInst(instr.instruction)) {
            //
            // The first instruction of a BL sequence has been found. update
            // the BlOffs value which was initialized in the ThumbBlInst case.
            // Only calls to register save millicode routines are valid within
            // the prologue. Register save routines have the form:
            //
            //  bx  pc              ; Thumb instruction
            //  nop                 ; Thumb instruction
            //  stm sp!, {reglist}  ; ARM instruction
            //  bx  lr              ; ARM instruction
            //
            // The following code loads the instruction at the function
            // destination address plus four bytes and verifies that a store
            // multiple instruction exists at that location. If not, this is
            // a bad function call and unwinding stops. Otherwise the store
            // multiple instruction is unwound
            //

            BlOffs += (instr.bl.offset << 12) + 4;
            if (BlOffs & (0x01 << 22)) {
                BlOffs |= 0xFF800000;               // sign extend the offset
            }

            DEBUGMSG(DBGEH, (TEXT("BL Instruction: Destination=0x%08x\r\n"),
                             ControlPc+BlOffs));

            // verify stmdb sp!, {reglist}:

            BlOffs += 4;
            if (!READ_CODE((PVOID)(ControlPc+BlOffs), &instrA, 4) ||
                (instrA.instruction & 0xFFFF0000) != 0xE92D0000 ){
                return OrigPc;
            }

            DEBUGMSG(DBGEH, (TEXT("Register Save Instruction @%08x = 0x%08x\r\n"),
                             ControlPc+BlOffs, instrA.instruction));

            RegPtr = &Context->R0;
            for (RegBit = 0x01; RegBit < 0x10000; RegBit <<= 1) {
                if (instrA.ldm.reglist & RegBit) {
                    if (!READ_DATA((PVOID)Context->Sp, RegPtr, 4)) {
                        return OrigPc;
                    }
                    Context->Sp += 4;

                    DEBUGMSG(DBGEH, (TEXT(" Save R%d = 0x%x. Sp=0x%x\r\n"),
                                     RegPtr-&Context->R0, *RegPtr, Context->Sp));
                }
                RegPtr++;
            }

        } else if (ThumbBlInst(instr.instruction)) {
            BlOffs = instr.bl.offset << 1;

            DEBUGMSG(DBGEH, (TEXT("BL Instruction: offset=0x%03x\r\n"),
                             instr.bl.offset));

        } else if (ThumbBxInst(instr.instruction)) {
            DEBUGMSG(DBGEH, (TEXT("BX Instruction: ControlPc=0x%x  instr=0x%x\r\n"),
                             ControlPc, instr.instruction));
        } else {
            DEBUGMSG(DBGEH, (TEXT("Unknown Prolog Instruction Ignored. ControlPc=0x%08x  Inst=0x%04x\r\n"),
                             ControlPc, instr.instruction));
        }

        ControlPc -= sizeof instr;
    }

    *EstablisherFrame = Context->Sp;
    return ADJUST_NEXTPC(Context->Lr);                 // Address of calling instruction
}



//------------------------------------------------------------------------------
//
//  Routine Description:
//
//      This is the ARM specific implementation of RtlVirtualUnwind
//
//  Arguments:
//
//      See RtlVirtualUnwind
//
//  Return Value:
//
//      See RtlVirtualUnwind
//
//------------------------------------------------------------------------------
PRIVATE ULONG
ArmVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT Context,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame
    )
{
    ULONG   Address;
    LONG    cb;
    DWORD   NextPc = 0;
    BOOLEAN HaveExceptionPc;
    BOOLEAN ExecutingEpilog;
    LONG    i,j;
    ARMI    instr, instr2;
    ARMI    Prolog[15];
    PULONG  Register;
    // BUGBUG: FIX THIS LATER
    ULONG   pcOfst = 0; // ControlPc - (ULONG) ZeroPtr (ControlPc);
    ULONG   BeginAddress = FunctionEntry->BeginAddress;
    ULONG   PrologEndAddress = FunctionEntry->PrologEndAddress;


    // NOTE:  When unwinding the call stack, we assume that all instructions
    // in the prolog have the condition code "Always execute."  This is not
    // necessarily true for the epilog.

    DEBUGMSG(DBGEH, (TEXT("Unwind: Pc=%8.8x Begin=%8.8x PrologEnd=%8.8x End=%8.8x\r\n"),
        ControlPc, BeginAddress, PrologEndAddress,
        FunctionEntry->EndAddress));

    Register = &Context->R0;
    *InFunction = FALSE;

    if (PrologEndAddress == BeginAddress) {
        // No prolog.
        *EstablisherFrame = Register[13];
        return ADJUST_NEXTPC(Register[14]);
    }

    // map slot 0 address to the process
    if (BeginAddress < (1 << VA_SECTION)) {
        BeginAddress += pcOfst;
        PrologEndAddress += pcOfst;
    }

    // First check to see if we are in the epilog.  If so, forward execution
    // through the end of the epilog is required.  Epilogs are composed of the
    // following:
    //
    // An ldmdb which uses the frame pointer, R11, as the base and updates
    // the PC.
    //
    // -or-
    //
    // A stack unlink (ADD R13, x) if necessary, followed by an ldmia which
    // updates the PC or a single mov instruction to copy the link register
    // to the PC
    //
    // -or-
    //
    // An ldmia which updates the link register, followed by a regular
    // branch instruction.  (This is an optimization when the last instruction
    // before a return is a call.)
    //
    // A routine may also have an empty epilog.  (The last instruction is to
    // branch to another routine, and it doesn't modify any permanent
    // registers.)  But, in this case, we would also have an empty prolog.
    //
    // If we are in an epilog, and the condition codes dictate that the
    // instructions should not be executed, treat this as not an epilog at all.

    if (ControlPc >= PrologEndAddress) {
        DWORD   EpilogPc = ControlPc;
        DEBUGMSG(DBGEH, (TEXT("Checking epilog @%8.8x\r\n"), EpilogPc));
        // Check the condition code of the first instruction. If it won't be
        // executed, don't bother checking what type of instruction it is.
        if (!READ_CODE((PVOID)EpilogPc, &instr, 4))
            return ControlPc;

        ExecutingEpilog = CheckConditionCodes(Register[16], instr.instruction);
        while (ExecutingEpilog) {
            if (!READ_CODE((PVOID)EpilogPc, &instr, 4))
                return ControlPc;

            DEBUGMSG(DBGEH, (TEXT("executing epilog @%8.8x\r\n"), EpilogPc));

            // Test for these instructions:
            //      ADD R13, X - stack unlink
            //
            //      MOV PC, LR - return
            //
            //      LDMIA or LDMDB including PC - update registers and return
            //      (SP)   (FP)
            //
            //      LDMIA including LR, followed by a branch
            //          Update registers and branch. (In our case, return.)
            //
            //      A branch preceded by an LDMIA including LR
            //          Copy the LR to the PC to get the call stack

            if ((instr.instruction & ADD_SP_MASK) == ADD_SP_INSTR) {
                UnwindDataProcess(instr, Register);

            } else if ((instr.instruction & MOV_PC_LR_MASK) == MOV_PC_LR) {
                instr.dataproc.rd = 14; // don't damage Pc value for Unwinder
                UnwindDataProcess(instr, Register);
                *EstablisherFrame = Register[13];
                return ADJUST_NEXTPC(Register[14]);

            } else if ((instr.instruction & LDM_PC_MASK) == LDM_PC_INSTR) {
                ulong SavePc = Register[15];
                if (!UnwindLoadMultiple(instr, Register))
                    return ControlPc;
                ControlPc = Register[15];
                Register[15] = SavePc;
                *EstablisherFrame = Register[13];
                return ADJUST_NEXTPC(ControlPc);

            } else if ((instr.instruction & LDM_LR_MASK) == LDM_LR_INSTR) {
                if (!READ_CODE((PVOID)(EpilogPc+4), &instr2, 4))
                    return ControlPc;
                if ((instr2.instruction & B_BL_MASK) == B_INSTR ||
                    (instr2.instruction & BX_MASK) == BX_INSTR) {

                    if (!UnwindLoadMultiple(instr, Register))
                        return ControlPc;
                    *EstablisherFrame = Register[13];
                    return ADJUST_NEXTPC(Register[14]);
                } else
                    ExecutingEpilog = FALSE;
            } else
                ExecutingEpilog = FALSE;

            EpilogPc += 4;
        }
    }

    // We were not in the epilog. Load in the prolog, and reverse execute it.
    cb = PrologEndAddress - BeginAddress;
    if (cb > sizeof(Prolog)) {
        ASSERT(FALSE);    // prolog too large
        return ControlPc;
    }

    //
    //  Thumb assembly functions which switch to ARM via "bx pc" will have
    //  the low bit of the BeginAddress set. Align the BeginAddress
    //
    Address = BeginAddress & ~0x03;

    //  Read the prolog and restore debugger inserted break instructions to
    //  real instruction
    DEBUGMSG(DBGEH, (TEXT("Reading Prolog @%8.8x\r\n"), Address));
    if (!READ_CODE((PVOID)Address, (PVOID)Prolog, cb))
        return ControlPc;

    // Check to see if we're already in the prolog.
    if (ControlPc < PrologEndAddress)
        cb = ControlPc - BeginAddress;
    else
        *InFunction = TRUE;
    *EstablisherFrame = Register[13];

    HaveExceptionPc = FALSE;

    // Reverse execute starting with the last instruction in the prolog
    // that has been executed.
    for (i = cb/4 - 1 ; i >= 0 ; --i) {

        // Test for these instructions:
        //
        //      SUB SP, X - stack allocation
        //
        //      MOV X, SP - save stack value
        //
        //      STMDB R13, {PC} - special instruction in fake prolog
        //
        //      STMDB - save incoming registers
        //
        //      MOV R11, X or
        //      ADD R11, X or
        //      SUB R11, X    - FP (frame pointer) setup
        //

        if ((Prolog[i].instruction & MOV_SP_MASK) == MOV_SP_INSTR) {

            Register[13] = Register[Prolog[i].dataproc.rd];

        } else if ((Prolog[i].instruction & SUB_SP_MASK) == SUB_SP_INSTR) {
            //
            // ldr       r12, [pc, #Immed]
            // sub       sp, sp, r12
            //
            // This is a stack link.  Unlink the stack.

            DEBUGMSG(DBGEH, (TEXT("%d: stack link instr.\r\n"), i));
            if (Prolog[i].dataproc.bits != 0x1 && Prolog[i].dpshi.rm == 0xc) {

                // Look for an LDR instruction above this one.
                j = i - 1;
                while (j >= 0 && (Prolog[j].instruction & LDR_MASK) != LDR_PC_INSTR)
                    j--;

                if (j < 0) {
                    ASSERT(FALSE);  // This should never happen
                    return ControlPc;
                }

                // Get the address of the ldr instruction + 8 + the offset
                Address = j*4 + BeginAddress + Prolog[j].ldr.offset + 8;

                // R12 is the value at that location.
                if (!READ_DATA((PVOID)Address, (PVOID)&Register[12], 4))
                    return ControlPc;
            }

            // Change the subtract to an add, and execute the instruction
            Prolog[i].dataproc.opcode = OP_ADD;
            UnwindDataProcess(Prolog[i], Register);

        } else if ((Prolog[i].instruction & ADD_SP_MASK) == ADD_SP_INSTR) {
            //
            // ldr       r12, [pc, #Immed]
            // add       sp, sp, r12
            //
            // This is a stack link.  Unlink the stack.

            DEBUGMSG(DBGEH, (TEXT("%d: stack link instr.\r\n"), i));
            if (Prolog[i].dataproc.bits != 0x1 && Prolog[i].dpshi.rm == 0xc) {
                // Look for an LDR instruction above this one.
                j = i - 1;
                while (j >= 0 && (Prolog[j].instruction & LDR_MASK) != LDR_PC_INSTR)
                    j--;

                if (j < 0) {
                    ASSERT(FALSE);  // This should never happen
                    return ControlPc;
                }

                // Get the address of the ldr instruction + 8 + the offset
                Address = j*4 + BeginAddress + Prolog[j].ldr.offset + 8;

                // R12 is the value at that location.
                if (!READ_DATA((PVOID)Address, (PVOID)&Register[12], 4))
                    return ControlPc;
            }

            // Change the subtract to an add, and execute the instruction
            Prolog[i].dataproc.opcode = OP_SUB;
            UnwindDataProcess(Prolog[i], Register);

        } else if ((Prolog[i].instruction & STM_PC_MASK) == STM_PC_INSTR) {
            //
            // STMDB R13!, { PC }
            //
            // This is an instruction that will not appear in a real prolog
            // (or probably anywhere else).  It is used as a marker in a fake
            // prolog as described below.
            //
            // In the usual case, the link register gets set by a call
            // instruction so the PC value should point to the instruction
            // that sets the link register.  In an interrupt or exception
            // frame, the link register and PC value are independent.  By
            // convention, the fake prolog for these frames (see
            // CaptureContext in armtrap.s) contains the STMDB instruction
            // shown above to lead the unwinder to the faulting PC location.
            //

            if (!READ_DATA((PULONG)Register[13], &NextPc, 4))
                return ControlPc;
            Register[13] += 4;
            HaveExceptionPc = TRUE;
        }
        else if ((Prolog[i].instruction & STM_MASK) == STM_INSTR) {
            //
            //  Invert the Load, Increment and Before bits:
            //
            Prolog[i].instruction ^= 0x01900000;
            if (!UnwindLoadMultiple(Prolog[i], Register))
                return ControlPc;

        } else if ((Prolog[i].instruction & STRI_LR_SPU_MASK)
                                                        == STRI_LR_SPU_INSTR) {

            // Store of the link register that updates the stack as a base
            // register must be reverse executed to restore LR and SP
            // to their values on entry. This type of prolog is generated
            // for finally funclets.
            ///NKDbgPrintfW(L"%d: STRI for  mask=%4.4x.\r\n", i, Prolog[i].ldr.offset);
            //
            //  Invert the Load, Increment and Before bits:
            //

            Prolog[i].instruction ^= 0x01900000;
            if (!UnwindLoadI(Prolog[i], Register))
                return ControlPc;

        } else if ((Prolog[i].instruction & ARM_MRS_MASK) == ARM_MRS_INSTR) {

            //
            // An "MRS rd, cpsr" instruction exists in CaptureContext to
            //  preserve the Psr register when unwinding out of system calls.
            //

            Context->Psr = Register[Prolog[i].mrs.rd];

        } else if ((Prolog[i].instruction & ADD_FP_MASK) == ADD_FP_INSTR
                   || (Prolog[i].instruction & SUB_FP_MASK) == SUB_FP_INSTR
                   || (Prolog[i].instruction & MOV_FP_MASK) == MOV_FP_INSTR) {

            // We know that as of this instruction, r11 is a valid frame
            // pointer.  Therefore, it contains the stack value immediately
            // before the STM that set up the stack frame.  So, we scan
            // backward to find the STM, pretend it was a STM using r11 as the
            // base register with no update, and reverse-execute it.

            j = i - 1;
            while (j >= 0 && (Prolog[j].instruction & STM_MASK) != STM_INSTR)
                j--;

            if ( j >= 0 ){

                // Tweak the instruction so it will be reverse-executed
                // correctly.  A convenient side-effect of this is that it
                // prevents it from being recognized as a STM when we continue
                // reverse-executing, which is good because we don't want to do it
                // again.

                Prolog[j].ldm.l = TRUE;
                Prolog[j].ldm.rn = 11;
                Prolog[j].ldm.w = FALSE;
                if (!UnwindLoadMultiple(Prolog[j], Register))
                    return ControlPc;
            }

        }
    }

    // The continuation address is contained in the link register.

    *EstablisherFrame = Register[13];
    if (!HaveExceptionPc)
        NextPc = ADJUST_NEXTPC(Register[14]);
    return NextPc;
}


//------------------------------------------------------------------------------
//
//  Routine Description:
//      This function virtually unwinds the specfified function by executing its
//      prologue code backwards (or its epilog forward).
//
//      If the function is a leaf function, then the address where control left
//      the previous frame is obtained from the context record. If the function
//      is a nested function, but not an exception or interrupt frame, then the
//      prologue code is executed backwards and the address where control left
//      the previous frame is obtained from the updated context record.
//
//      Otherwise, an exception or interrupt entry to the system is being unwound
//      and a special instruction in the prologue indicates the fault instruction
//      address.
//
//  Arguments:
//      ControlPc - Supplies the address where control left the specified
//          function.
//
//      FunctionEntry - Supplies the address of the function table entry for the
//          specified function.
//
//      ContextRecord - Supplies the address of a context record.
//
//      InFunction - Supplies a pointer to a variable that receives whether the
//          control PC is within the current function.
//
//      EstablisherFrame - Supplies a pointer to a variable that receives the
//          the establisher frame pointer value.
//
//  Return Value:
//      The address where control left the previous frame is returned as the
//      function value.
//
//------------------------------------------------------------------------------
ULONG
RtlVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT Context,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN e32_lite* eptr
    )
{
    //
    //  This function is simply a passthrough which dispatches to the correct
    //  unwinder for the processor mode at the current unwind point.
    //

    if (ControlPc & 0x01) {                    // Thumb Mode
        return ThumbVirtualUnwind(ControlPc,
                                  FunctionEntry,
                                  Context,
                                  InFunction,
                                  EstablisherFrame
                                  );

    }

    return ArmVirtualUnwind(ControlPc,
                            FunctionEntry,
                            Context,
                            InFunction,
                            EstablisherFrame
                            );
}



