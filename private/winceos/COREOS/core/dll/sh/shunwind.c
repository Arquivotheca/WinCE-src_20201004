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
//------------------------------------------------------------------------------
//
//
//------------------------------------------------------------------------------
//
//  Module Name:
//      shunwind.c
//
//  Abstract:
//      This module implements the unwinding of procedure call frames for
//      exception processing on Hitachi SH-3 & SH-4.
//
//------------------------------------------------------------------------------
#if defined(UNDER_CE)
#include <kernel.h>
#include <coredll.h>
#include <shxinst.h>
#endif

#if !defined(PRIVATE)
#define PRIVATE static
#endif

#if !defined(READ_CODE)
// Sanitize TRAPA #1
#define READ_CODE(inst, addr) {                                                \
    inst.instruction = *(USHORT*)(addr);                                       \
    if ((inst.instr5.opcode == 0xc3) &&                                        \
        (inst.instr5.imm_disp == 0x01)) {                                      \
        KDbgSanitize((PVOID)&inst, (PVOID)addr, sizeof(inst));                 \
    }                                                                          \
}
#endif

#if !defined(READ_DATA)
#define READ_DATA(t, dst, addr) (dst) = *(t*)(addr)
#endif

#if !defined(ADJUST_NEXTPC)
#define ADJUST_NEXTPC(pc) ((pc) - 2)
#endif

#define SP      0xF                    /* register 15 */

// The saved registers are the permanent general registers (ie. those
// that get restored in the epilog)
#define SAVED_INTEGER_MASK 0x0000ff00  /* saved integer registers */
#define IS_REGISTER_SAVED(Register) ((SAVED_INTEGER_MASK >> Register) & 1L)

#if SH4
#define SAVED_FLOATING_MASK 0x0000f000 /* saved floating pt registers */
#define IS_FLOAT_REGISTER_SAVED(Register) \
            ((SAVED_FLOATING_MASK >> Register) & 1L)
#define FR_MASK 0x00200000             /* masks FR bit in fpscr */
#define SZ_MASK 0x00100000             /* masks SZ bit in fpscr */
#define PR_MASK 0x00080000             /* masks PR bit in fpscr */
#endif


//------------------------------------------------------------------------------
//  Routine Description:
//      This function virtually unwinds the specfified function by executing its
//      prologue code backwards.
//
//      If the function is a leaf function, then the address where control left
//      the previous frame is obtained from the context record. If the function
//      is a nested function, but not an exception or interrupt frame, then the
//      prologue code is executed backwards and the address where control left
//      the previous frame is obtained from the updated context record.
//
//      Otherwise, an exception or interrupt entry to the system is being unwound
//      and a specially coded prologue restores the return address twice. Once
//      from the fault instruction address and once from the saved return address
//      register. The first restore is returned as the function value and the
//      second restore is place in the updated context record.
//
//      If a context pointers record is specified, then the address where each
//      nonvolatile registers is restored from is recorded in the appropriate
//      element of the context pointers record.
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
//      ContextPointers - Supplies an optional pointer to a context pointers
//          record.
//
//  Return Value:
//      The address where control left the previous frame is returned as the
//      function value.
//------------------------------------------------------------------------------
ULONG
RtlVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN e32_lite* eptr
    )
{
    ULONG            Address;
    ULONG            TemporaryValue = 0;
    ULONG            TemporaryRegister;
    SH3IW            Instruction;
    SH3IW            instrProlog;
    PULONG           IntegerRegister;
#if SH4
    PULONG           FloatingRegister;
#endif
    ULONG            NextPc;
    ULONG            Offset;
    ULONG            Rm;
    ULONG            Rn;

    LONG             PendingValue;
    BOOL             PendingAdd = FALSE;
    ULONG            PendingSource;
    ULONG            PendingTarget = 0;
    ULONG            pcOfst = 0; // ControlPc - (ULONG) ZeroPtr (ControlPc);
    ULONG            BeginAddress = FunctionEntry->BeginAddress;
    ULONG            PrologEndAddress = FunctionEntry->PrologEndAddress;

    IntegerRegister = &ContextRecord->R0;
    NextPc = ADJUST_NEXTPC(ContextRecord->PR);
    *InFunction = FALSE;

    // map slot 0 address to the process
    if (BeginAddress < (1 << VA_SECTION)) {
        BeginAddress += pcOfst;
        PrologEndAddress += pcOfst;
    }

#if SH4
    // We assume FR bit always 0 in prolog.
    FloatingRegister = ContextRecord->FRegs;
#endif

    // If we are at the end of the function, and the delay slot instruction
    // is one of the following, then just execute the instruction...
    //      MOV   Rm,SP
    //      ADD   Rm,SP
    //      ADD   #imm,SP
    //      MOV.L @SP+,Rn
    // For SH4:
    //      FSCHG
    //      FMOV.S @SP+,FRn
    // There is no need reverse execute the prolog, because the epilog has
    // already taken care of restoring all the other permanent registers.
    READ_CODE(instrProlog, ControlPc);
    if (instrProlog.instruction == RTS) {
        ControlPc += 2;
        READ_CODE(Instruction, ControlPc);

        // MOV Rm,SP
        if ((Instruction.instr3.opcode   == 0x6) &&
            (Instruction.instr3.ext_disp == 0x3)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            if (Rn == SP)
                IntegerRegister[SP] = IntegerRegister[Rm];
        }

        // ADD Rm,SP
        else if ((Instruction.instr3.opcode   == 0x3) &&
                 (Instruction.instr3.ext_disp == 0xc)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            if (Rn == SP)
                IntegerRegister[SP] += IntegerRegister[Rm];
        }

        // ADD #imm,SP
        //
        // Note: the offset added to SP will always be positive...
        else if (Instruction.instr2.opcode == 0x7) {
            Rn     = Instruction.instr2.reg;
            Offset = Instruction.instr2.imm_disp;
            if (Rn == SP)
                IntegerRegister[SP] += Offset;
        }

        // MOV.L @SP+,Rn
        else if ((Instruction.instr3.opcode   == 0x6) &&
                 (Instruction.instr3.ext_disp == 0x6)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            if ((Rm == SP) && IS_REGISTER_SAVED(Rn)) {
                READ_DATA(ULONG, IntegerRegister[Rn], IntegerRegister[SP]);
                IntegerRegister[SP] += 4;
            }
        }

#if SH4
        // FMOV.S @SP+, FRn
        else if ((Instruction.instr3.opcode   == 0xf) &&
                 (Instruction.instr3.ext_disp == 0x9) &&
                 (Instruction.instr3.regM     == SP)) {
            Rn = Instruction.instr3.regN;
            if (IS_FLOAT_REGISTER_SAVED(Rn)) {
                READ_DATA(ULONG, FloatingRegister[Rn], IntegerRegister[SP]);
                IntegerRegister[SP] += 4;
            }
        }

        // FSCHG
        else if (Instruction.instr6.opcode == 0xf3fd) {
            ContextRecord->Fpscr ^= SZ_MASK;    // toggle the SZ bit
        }
#endif
        *EstablisherFrame = ContextRecord->R15;
        return NextPc;
    }

#if SH4
    // Always expect FR and PR bits to be 0 throughout the prolog
    ContextRecord->Fpscr &= ~(FR_MASK | PR_MASK);
#endif

    // If the address where control left the specified function is outside
    // the limits of the prologue, then the control PC is considered to be
    // within the function and the control address is set to the end of
    // the prologue. Otherwise, the control PC is not considered to be
    // within the function (i.e., it is within the prologue).
    if ((ControlPc < BeginAddress) ||
        (ControlPc >= PrologEndAddress)) {
        *InFunction = TRUE;
        ControlPc = PrologEndAddress;

#if SH4
        // Always expect SZ bit to be 0 at end of prolog
        ContextRecord->Fpscr &= ~(SZ_MASK);
#endif
    }

    // Scan backward through the prologue and reload callee registers that
    // were stored.
    //
    // Cannot initialize holding registers to 0 because that is a valid
    // register value...
    PendingSource = (ULONG)-1;
    TemporaryRegister = (ULONG)-1;
    while (ControlPc > BeginAddress) {
        ControlPc -= 2;
        READ_CODE(Instruction, ControlPc);

        // MOV #imm,Rn
        if (Instruction.instr2.opcode == 0xe) {
            Rn = Instruction.instr2.reg;
            if (Rn == PendingSource) {
                PendingValue = (signed char)Instruction.instr2.imm_disp;
                if (PendingAdd) {
                    PendingValue = -PendingValue;
                }
                IntegerRegister[PendingTarget] += PendingValue;
            }
        }

        // MOV Rm,Rn
        else if ((Instruction.instr3.opcode   == 0x6) &&
                 (Instruction.instr3.ext_disp == 0x3)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            IntegerRegister[Rm] = IntegerRegister[Rn];
        }

        // MOV.L Rm,@-SP
        //
        // Used to push permanent registers to stack
        else if ((Instruction.instr3.opcode   == 0x2) &&
                 (Instruction.instr3.ext_disp == 0x6) &&
                 (Instruction.instr3.regN     == SP )) {
            Rm = Instruction.instr3.regM;
            READ_DATA(ULONG, IntegerRegister[Rm], IntegerRegister[SP]);
            IntegerRegister[SP] += 4;
        }

#if SH4
        // FMOV FRm,FRn (or any combination of DRm,DRn,XDm,XDn)
        else if ((Instruction.instr3.opcode   == 0xf) &&
                 (Instruction.instr3.ext_disp == 0xc)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            if (ContextRecord->Fpscr & SZ_MASK) {
                Rm += (Rm & 0x1)*15; // DRm or XDm
                Rn += (Rn & 0x1)*15; // DRn or XDn
                // the other 32 bits of the double precision move
                FloatingRegister[Rm+1] = FloatingRegister[Rn+1];
            }
            FloatingRegister[Rm] = FloatingRegister[Rn];
        }

        // FMOV(.S) FRm,@-SP (must check for SP)
        //
        // Used to push permanent fp registers to stack
        else if ((Instruction.instr3.opcode   == 0xf) &&
                 (Instruction.instr3.ext_disp == 0xb) &&
                 (Instruction.instr3.regN     == SP)) {
            Rm = Instruction.instr3.regM;
            if (ContextRecord->Fpscr & SZ_MASK) {
                // low order bits in FR(m+1), and high order in FRm
                READ_DATA(ULONG, FloatingRegister[Rm+1], IntegerRegister[SP]);
                IntegerRegister[SP] += 4;
            }
            READ_DATA(ULONG, FloatingRegister[Rm], IntegerRegister[SP]);
            IntegerRegister[SP] += 4;
        }

        // FSCHG
        else if (Instruction.instr6.opcode == 0xf3fd) {
            ContextRecord->Fpscr ^= SZ_MASK;    // toggle the SZ bit
        }
#endif

        // MOV.L @(disp,pc),Rn
        //
        // Used to load stack decrement value into a register
        else if (Instruction.instr2.opcode == 0xD) {
            Rn = Instruction.instr2.reg;
            if (Rn == PendingSource) {
                // Address = 4-byte aligned current instruction address +
                //           4 (SH3 pc points 2 instructions downstream) +
                //           4-byte scaled displacement (from instruction)
                Address = (ControlPc & 0xfffffffc) + 4 +
                          (Instruction.instr2.imm_disp << 2);
                READ_DATA(LONG, PendingValue, Address);
                if (PendingAdd) {
                    PendingValue = -PendingValue;
                }
                IntegerRegister[PendingTarget] += PendingValue;
            }
        }

        // MOV.W @(disp,pc),Rn
        //
        // Used to load stack decrement value into a register
        else if (Instruction.instr2.opcode == 0x9) {
            Rn = Instruction.instr2.reg;
            if (Rn == PendingSource) {
                // Address = current instruction address +
                //           4 (SH3 pc points 2 instructions downstream) +
                //           2-byte scaled displacement (from instruction)
                Address = ControlPc + 4 + (Instruction.instr2.imm_disp << 1);
                READ_DATA(SHORT, PendingValue, Address);
                if (PendingAdd) {
                    PendingValue = -PendingValue;
                }
                IntegerRegister[PendingTarget] += PendingValue;
            }
        }

        // ADD #imm,Rn
        //
        // Used to allocate space on the stack (#imm < 0)
        else if (Instruction.instr2.opcode == 0x7) {
            Rn = Instruction.instr2.reg;
            Offset = (signed char)Instruction.instr2.imm_disp;
///            if (Offset & 0x80) {
///                // Offset is negative, so add 2's-complement
///                Offset = (~Offset + 1) & 0xff;
///                IntegerRegister[Rn] += Offset;
///            } else {
                // Offset is positive, so subtract it
            IntegerRegister[Rn] -= Offset;
///            }
        }

        // SUB Rm,Rn
        //
        // Used to allocate space on the stack
        else if ((Instruction.instr3.opcode   == 0x3) &&
                 (Instruction.instr3.ext_disp == 0x8)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            PendingSource = Rm;
            PendingTarget = Rn;
            PendingAdd = FALSE;
        }

        // ADD Rm,Rn
        //
        // Used to allocate space on the stack
        else if ((Instruction.instr3.opcode   == 0x3) &&
                 (Instruction.instr3.ext_disp == 0xc)) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            PendingSource = Rm;
            PendingTarget = Rn;
            PendingAdd = TRUE;
        }


        // MOV.L Rm,@(disp,Rn)
        //
        // Used to store register values to stack
        else if (Instruction.instr3.opcode == 0x1) {
            Rm = Instruction.instr3.regM;
            Rn = Instruction.instr3.regN;
            TemporaryRegister = Rm;
            Address = IntegerRegister[Rn] + (Instruction.instr3.ext_disp << 2);
            READ_DATA(ULONG, TemporaryValue, Address);
            // Added for unwinding parameter values
            IntegerRegister[Rm] = TemporaryValue;
        }

        // STS mach,Rn
        //
        // (special prolog only)
        else if ((Instruction.instr1.opcode   == 0x0) &&
                 (Instruction.instr1.function == 0x0a)) {
            Rn = Instruction.instr1.reg;
            if (Rn == TemporaryRegister) {
                ContextRecord->MACH = TemporaryValue;
            }
        }

        // STS macl,Rn
        //
        // (special prolog only)
        else if ((Instruction.instr1.opcode   == 0x0) &&
                 (Instruction.instr1.function == 0x1a)) {
            Rn = Instruction.instr1.reg;
            if (Rn == TemporaryRegister) {
                ContextRecord->MACL = TemporaryValue;
            }
        }

        // STS pr,Rn
        //
        // (special prolog only)
        //
        // NOTE: since this instruction is only in the prolog it cannot
        //       be the NextPc...only STC spc,Rn can
        else if ((Instruction.instr1.opcode   == 0x0) &&
                 (Instruction.instr1.function == 0x2a)) {
            Rn = Instruction.instr1.reg;
            if (Rn == TemporaryRegister) {
                ContextRecord->PR = TemporaryValue;
            }
        }

        // STC gbr,Rn
        //
        // (special prolog only)
        else if ((Instruction.instr1.opcode   == 0x0) &&
                 (Instruction.instr1.function == 0x12)) {
            Rn = Instruction.instr1.reg;
            if (Rn == TemporaryRegister) {
                ContextRecord->GBR = TemporaryValue;
            }
        }

        // STC ssr,Rn
        //
        // (special prolog only)
        else if ((Instruction.instr1.opcode   == 0x0) &&
                 (Instruction.instr1.function == 0x32)) {
            Rn = Instruction.instr1.reg;
            if (Rn == TemporaryRegister) {
                ContextRecord->Psr = TemporaryValue;
            }
        }

        // STC spc,Rn
        //
        // (special prolog only)
        else if ((Instruction.instr1.opcode   == 0x0) &&
                 (Instruction.instr1.function == 0x42)) {
            Rn = Instruction.instr1.reg;
            if (Rn == TemporaryRegister) {
                NextPc = TemporaryValue;
            }
        }

#if SH4
        // STS.L fpscr,@-Rn
        //
        // (special prolog only)
        else if ((Instruction.instr1.opcode   == 0x4) &&
                 (Instruction.instr1.function == 0x62)) {
            Rn = Instruction.instr1.reg;
            READ_DATA(ULONG, ContextRecord->Fpscr, IntegerRegister[Rn]);
            IntegerRegister[Rn] += 4;
        }
#endif

        // STS.L pr,@-Rn
        else if ((Instruction.instr1.opcode   == 0x4) &&
                 (Instruction.instr1.function == 0x22)) {
            Rn = Instruction.instr1.reg;
            READ_DATA(ULONG, ContextRecord->PR, IntegerRegister[Rn]);
            IntegerRegister[Rn] += 4;
            NextPc = ADJUST_NEXTPC(ContextRecord->PR);
        } else {
            DEBUGMSG(DBGEH, (TEXT("RtlVirtualUnwind: unknown instruction in prolog @%8.8lx=%4.4x\r\n"),
                             ControlPc, Instruction));
        }
    }
    *EstablisherFrame = ContextRecord->R15;
    return NextPc;
}

