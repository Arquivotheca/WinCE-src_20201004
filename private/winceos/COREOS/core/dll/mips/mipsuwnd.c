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
    mipsuwnd.c

Abstract:
    This module implements the unwinding of procedure call frames for
    exception processing on MIPS processors.
--*/

#if defined (UNDER_CE)
#include "kernel.h"
#include "coredll.h"

PRUNTIME_FUNCTION RtlLookupFunctionEntry (
        IN DWORD FunctionTableAddr,
        IN DWORD SizeOfFuntionTable,
        IN ULONG ControlPc,
        OUT PRUNTIME_FUNCTION prf);

#define LOOKUP_FUNCTION_ENTRY(eptr, pc, prf)                                   \
    RtlLookupFunctionEntry((eptr)->e32_vbase + (eptr)->e32_unit[EXC].rva,      \
                           (eptr)->e32_unit[EXC].size,                         \
                           (pc), (prf))
#endif

#if !defined(PRIVATE)
#define PRIVATE static
#endif

// Replace any break instruction introduced by the debugger, as
// a result of a breakpoint, with its original instruction
#if !defined(READ_CODE32)
#define READ_CODE32(dst, addr) {                                               \
    MIPS_INSTRUCTION _inst = { *(ULONG*)(addr) };                              \
    if (_inst.r_format.Function == BREAK_OP &&                                 \
        _inst.r_format.Opcode == SPEC_OP) {                                    \
        KDbgSanitize((PVOID)&_inst, (PVOID)(addr), sizeof(_inst));             \
    }                                                                          \
    (dst) = _inst.Long;                                                        \
}
#endif

#if !defined(READ_CODE16)
#define READ_CODE16(dst, addr) {                                               \
    MIPS16_INSTRUCTION _inst = { *(USHORT*)(addr) };                           \
    if (_inst.rr_format.Function == BREAK_OP16 &&                              \
        _inst.rr_format.Opcode == RR_OP16) {                                   \
        KDbgSanitize((PVOID)&_inst, (PVOID)(addr), sizeof(_inst));             \
    }                                                                          \
    (dst) = _inst.Short;                                                       \
}
#endif

#if !defined(READ_DATA)
#define READ_DATA(t, dst, addr) (dst) = *(t*)(addr)
#endif

// Maximum number of instructions encoded by block with given start and end addresses
// The +1 covers the potential delay slot of the last instruction
#define BYTES_TO_OPCOUNT(_begin, _end)  ((((_end) - (_begin)) >> (2 >> ((_end) & 1))) + 1)

// Op count of longest prologue helper function called from the current mode
#define MIPS_PROLHELPER_OPCOUNT(_pc) 14 // store 9 callee-saved regs, store RA, subtract saved-reg and arg area
                                        // size from SP, subtract local var area size from SP, return, and return
                                        // delay slot = 14 instructions.

// Address of instruction adjacent to _pc: +/- 4 for MIPS32, +/- 2 for MIPS16
#define MIPS_PREV_INSTR(_pc)  (ULONG)((_pc) - (4 >> ((_pc) & 1)))
#define MIPS_NEXT_INSTR(_pc)  (ULONG)((_pc) + (4 >> ((_pc) & 1)))

// MIPS16->MIPS32 register encoding conversion
#define FIX_MIPS16_REG(r)     ((REGNAME)(((r) < 2) ? (r)+16 : (r)))

// MIPS16 MOV32R register encoding
#define FIX_MOV32R_REG(r)     ((REGNAME)(((r) >> 2) | (((r) & 0x3) << 3)))
#define UNFIX_MOV32R_REG(r)   ((REGNAME)(((r) << 2) | (((r) & 0x18) >> 3)))

// MIPS16 instruction tests
#define IS_MIPS16_MOV32RSP(i) (((i) & 0xfff8) == ((I8_OP16 << 11) | (MOV32R_OP16 << 8) | (UNFIX_MOV32R_REG (SP) << 3)))
#define IS_MIPS16_ADDIUSP(i)  (((i) & 0xff00) == ((I8_OP16 << 11) | (ADJSP_OP16 << 8)))
#define IS_MIPS16_JR(i)       (((i) & 0xf8ff) == ((RR_OP16 << 11) | JR_OP16))
#define IS_MIPS16_JRRA(i)     ((i) == ((RR_OP16 << 11) | (0x1 << 5) | JR_OP16))
#define IS_MIPS16_RETURN(i)   (IS_MIPS16_JR(i) || IS_MIPS16_JRRA(i))

// MIPS32 instruction tests
#define IS_MIPS32_ADDIUSP(i) (((i) & 0xffff0000) == ((ADDIU_OP << 26) | (SP << 16) | (SP << 21)))
#define IS_MIPS32_ADDUSP(i) (((i) & 0xffe0ffff) == ((SPEC_OP << 26) | ADDU_OP | (SP << 11) | (SP << 21)))
#define IS_MIPS32_RETURN(i) ((i) == JUMP_RA)

// Interpreter opcodes
typedef enum Opcode {
    OP_NOP = 0,
    OP_ADD,
    OP_ADDI,
    OP_SUB,
    OP_OR,
    OP_MOVE,
    OP_IMM,
    OP_STORE,
    OP_STORE64,
    OP_FSTORE,
    OP_DSTORE,
} OPCODE;

// Register names
typedef enum RegName {
    NOREG = -1,
    ZERO = 0,
    AT,
    V0, V1,
    A0, A1, A2, A3,
    T0, T1, T2, T3, T4, T5, T6, T7,
    S0, S1, S2, S3, S4, S5, S6, S7,
    T8, T9,
    K0, K1,
    GP,
    SP,
    S8,
    RA
} REGNAME;

// Interpreter operation structure
typedef struct Operation {
    OPCODE Opcode:CHAR_BIT;
    REGNAME Rd:CHAR_BIT;
    REGNAME Rs:CHAR_BIT;
    REGNAME Rt:CHAR_BIT;
    REG_TYPE Imm;
} OPERATION, *POPERATION;

static BOOL
IsScratchReg (
    IN REGNAME regName)
/*++
Routine Description
    Returns TRUE if the given register is used as a scratch register in the
    prologue.

Arguments
    regName - register index in question.
--*/
{
    switch (regName)
    {
        case AT:
        case V0:
        case V1:
        case T0:
        case T1:
        case T2:
        case T3:
        case T4:
        case T5:
        case T6:
        case T7:
        case T8:
        case T9:
            return TRUE;
    }

    return FALSE;
}

PRIVATE POPERATION
BuildSimpleOp16 (
    IN OUT POPERATION Op,
    IN ULONG Address,
    IN ULONG PCAddress,
    IN ULONG Extend)
/*++
Routine Description
    This function converts a MIPS16 instruction at the specified address into the
    equivalent interpreter instruction.

Arguments
    Op - A pointer to an interpreter operation structure to receive the translated
        opcode.

    Address - The address of the MIPS16 instruction.

    PCAddress - The PC value at execution time (PCAddress != Address when the
        instruction is being executed in a delay slot).

    Extend - The value of a previous EXTEND opcode immediate field.  Set to -1
        if the instruction is not extended.
--*/
{
    MIPS16_INSTRUCTION I16;
    ULONG UImm8, UImm5, SImm8;

    READ_CODE16 (I16.Short, Address);
    UImm8 = I16.ri_format.Immediate << 2;
    UImm5 = I16.rri_format.Immediate << 2;
    SImm8 = I16.rsi_format.Simmediate;
    if (Extend != (ULONG) -1) {
        SImm8 = UImm8 = UImm5 = Extend | I16.rri_format.Immediate;
    }
    Op->Opcode = OP_NOP;
    Op->Rt = Op->Rs = Op->Rd = NOREG;
    switch (I16.i_format.Opcode) {
    case LI_OP16:
        Op->Opcode = OP_IMM;
        Op->Rd = FIX_MIPS16_REG (I16.ri_format.Rx);
        Op->Imm = I16.ri_format.Immediate;
        if (Extend != (ULONG) -1) {
            Op->Imm = (Extend | I16.rri_format.Immediate) & 0xFFFF;
        }
        break;
    case SW_OP16:
        Op->Opcode = OP_STORE;
        Op->Rs = FIX_MIPS16_REG (I16.rri_format.Rx);
        Op->Rt = FIX_MIPS16_REG (I16.rri_format.Ry);
        Op->Imm = UImm5;
        break;
    case SWSP_OP16:
        Op->Opcode = OP_STORE;
        Op->Rs = SP;
        Op->Rt = FIX_MIPS16_REG (I16.ri_format.Rx);
        Op->Imm = UImm8;
        break;
    case LWPC_OP16:
        Op->Opcode = OP_IMM;
        Op->Rd = FIX_MIPS16_REG (I16.ri_format.Rx);
        Op->Imm = *(PULONG) ((PCAddress & ~3) + UImm8);
        break;
    case ADDIU8_OP16:
        Op->Opcode = OP_ADDI;
        Op->Rs = Op->Rd = FIX_MIPS16_REG (I16.ri_format.Rx);
        Op->Imm = SImm8;
        break;
    case ADDIUSP_OP16:
        Op->Opcode = OP_ADDI;
        Op->Rd = FIX_MIPS16_REG (I16.ri_format.Rx);
        Op->Rs = SP;
        Op->Imm = UImm8;
        break;
    case RRR_OP16:
        Op->Rd = FIX_MIPS16_REG (I16.rrr_format.Rz);
        Op->Rs = FIX_MIPS16_REG (I16.rrr_format.Rx);
        Op->Rt = FIX_MIPS16_REG (I16.rrr_format.Ry);
        if (Op->Rs != Op->Rd) break;
        switch (I16.rrr_format.Function) {
        case SUBU_OP16:
            Op->Opcode = OP_SUB;
            break;
        case ADDU_OP16:
            Op->Opcode = OP_ADD;
            break;
        }
        break;
    case I8_OP16:
        switch (I16.i8_format.Function) {
        case SWRASP_OP16:
            Op->Opcode = OP_STORE;
            Op->Rs = SP;
            Op->Rt = RA;
            Op->Imm = UImm8;
            break;
        case ADJSP_OP16:
            Op->Opcode = OP_ADDI;
            Op->Rs = Op->Rd = SP;
            Op->Imm = SImm8;
            if (Extend == (ULONG) -1) {
                Op->Imm = SImm8 << 3;
            }
            break;
        case MOV32R_OP16:
            Op->Opcode = OP_MOVE;
            Op->Rd = FIX_MOV32R_REG (I16.mov32r_format.R32);
            Op->Rs = FIX_MIPS16_REG (I16.mov32r_format.Rz);
            break;
        case MOVR32_OP16:
            Op->Opcode = OP_MOVE;
            Op->Rd = FIX_MIPS16_REG (I16.movr32_format.Ry);
            Op->Rs = (REGNAME) I16.movr32_format.R32;
            break;
        }
        break;
    default:
        break;
    }

    return Op + (Op->Opcode != OP_NOP);
}

PRIVATE POPERATION
BuildSimpleOp32 (
    IN OUT POPERATION Op,
    IN ULONG Address,
    IN BOOLEAN NotFirst)
/*++
Routine Description
    This function converts a MIPS32 instruction at the specified address into the
    equivalent interpreter instruction.  It will attempt to compress LUI-ORI pairs
    into a single OP_IMM operation.

Arguments
    Op - A pointer to an interpreter operation structure to receive the translated
        opcode.

    Address - The address of the MIPS32 instruction.

    NotFirst - True if this is not the first Op in the list, to tell LUI-ORI
        compression to look at the previous operation at Op-1.
--*/
{
    MIPS_INSTRUCTION I32;

    READ_CODE32 (I32.Long, Address);
    Op->Rs = (REGNAME) I32.i_format.Rs;
    Op->Rt = (REGNAME) I32.i_format.Rt;
    Op->Rd = (REGNAME) I32.r_format.Rd;
    Op->Imm = I32.i_format.Simmediate;
    switch (I32.i_format.Opcode) {
    case SD_OP:
        Op->Opcode = OP_STORE64;
        goto store;
    case SW_OP:
        Op->Opcode = OP_STORE;
        goto store;
    case SWC1_OP:
        Op->Opcode = OP_FSTORE;
        goto store;
    case SDC1_OP:
        Op->Opcode = OP_DSTORE;
    store:
        if (Op->Rs == SP) {
            Op->Rd = NOREG;
            Op++;
        } else {
            // Look for a previous definition using SP
            if (NotFirst
             && ((Op-1)->Opcode == OP_ADD || (Op-1)->Opcode == OP_ADDI)
             && (Op-1)->Rd == Op->Rs
             && ((Op-1)->Rs == SP || (Op-1)->Rt == SP)) {
                Op->Rd = NOREG;
                Op++;
            }
        }
        break;
    case ADDIU_OP:
    case DADDIU_OP:
        Op->Rd = Op->Rt;
        if (Op->Rs == Op->Rt || Op->Rs == SP) {
            Op->Opcode = OP_ADDI;
            Op++;
        } else if (Op->Rs == ZERO) {
            Op->Opcode = OP_IMM;
            Op++;
        }
        break;
    case ORI_OP:
        Op->Opcode = OP_IMM;
        Op->Imm &= 0xffff;
        Op->Rd = Op->Rt;
        if (Op->Rs == Op->Rt) {
            // Look for matching LUI
            if (NotFirst && (Op-1)->Opcode == OP_IMM && (Op-1)->Rd == Op->Rd) {
                (Op-1)->Imm |= Op->Imm;
            } else {
                Op++;
            }
        } else if (Op->Rs == ZERO) {
            Op++;
        }
        break;
    case SPEC_OP:
        switch (I32.r_format.Function) {
        case OR_OP:
            Op->Opcode = OP_OR;
            goto spec;
        case ADDU_OP:
            Op->Opcode = OP_ADD;
        spec:
            if (Op->Rs == ZERO) {
                Op->Opcode = OP_MOVE;
                Op->Rs = Op->Rt;
                Op++;
            } else if (Op->Rt == ZERO) {
                Op->Opcode = OP_MOVE;
                Op++;
            } else if (Op->Rs == Op->Rd) {
                Op++;
            } else if (Op->Rt == Op->Rd) {
                Op->Rt = Op->Rs;
                Op->Rs = Op->Rd;
                Op++;
            } else if (Op->Rs == SP || Op->Rt == SP) {
                Op++;
            }
            break;
        case SUBU_OP:
        case DSUBU_OP:
            if (Op->Rs == Op->Rd) {
                Op->Opcode = OP_SUB;
                Op++;
            }
            break;
        }
        break;
    case LUI_OP:
        Op->Opcode = OP_IMM;
        Op->Imm <<= 16;
        Op->Rd = Op->Rt;
        Op++;
        break;
    }
    return Op;
}

PRIVATE INT
BuildOps (
    IN OUT POPERATION Operations,
    IN INT Max,
    IN ULONG Address,
    IN ULONG EndAddress,
    IN ULONG pcOfst)
/*++
Routine Description
    This function constructs an interpreter operation list based upon the machine
    instructions found between a start and end address.

    The code is smart enough to handle a single level of calls, this is required
    if prolog helpers are being used.

Arguments
    Operations - A pointer to an interpreter operation array to receive the translated
        instructions.

    Max - Size of the operation array.

    Address - The address of the first instruction.

    EndAddress - The address of the last instruction.

Return Value:
    The number of operations generated.
--*/
{
    MIPS16_INSTRUCTION I16;
    MIPS_INSTRUCTION I32;
    ULONG RetAddress = (ULONG) -1;
    ULONG Extend = (ULONG) -1;
    ULONG PCAddress = Address;
    POPERATION Op = Operations;
    POPERATION LastOp = Operations + Max;

    while (Address != EndAddress && Op < LastOp) {
        if (Address & 1) {
            READ_CODE16 (I16.Short, Address - 1);
            switch (I16.i_format.Opcode) {
            case EXTEND_OP16:
                Extend = (I16.ei_format.Simmediate11 << 11) | (I16.ei_format.Immediate5 << 5);
                PCAddress = Address;
                Address += 2;
                break;
            case B_OP16:
                if (Extend == (ULONG) -1) {
                    Address += (I16.b_format.Simmediate << 1) + 2;
                } else {
                    Address += (I16.rri_format.Immediate | Extend) + 4;
                }
                PCAddress = Address;
                Extend = (ULONG) -1;
                break;
            case JALX_OP16:
                Op = BuildSimpleOp16 (Op, Address + 3, Address, (ULONG) -1);
                RetAddress = Address + 6;
                Address = (I16.j_format.Target16 << 18) | (I16.j_format.Target21 << 23) | (*(PUSHORT) (Address+1) << 2) | ((Address-pcOfst+3) & 0xf0000000);
                Address += pcOfst;
                Address |= (I16.j_format.Ext == 0);
                PCAddress = Address;
                Extend = (ULONG) -1;
                break;
            case RR_OP16:
                switch (I16.rr_format.Function) {
                case JR_OP16:
                    if (I16.rr_format.Ry == 1 && I16.rr_format.Rx == 0) {
                        // If this is a "JR RA" and we've nested then decode from return address
                        if (RetAddress != (ULONG) -1) {
                            Op = BuildSimpleOp16 (Op, Address + 1, PCAddress, (ULONG) -1);
                            PCAddress = Address = RetAddress;
                            RetAddress = (ULONG) -1;
                            Extend = (ULONG) -1;
                            continue;
                        }
                    }
                    // Anything other than JR RA is not acceptable in helper functions
                    return -1;
                }
                Address += 2;
                PCAddress = Address;
                Extend = (ULONG) -1;
                break;
            default:
                Op = BuildSimpleOp16 (Op, Address - 1, PCAddress, Extend);
                Address += 2;
                PCAddress = Address;
                Extend = (ULONG) -1;
                break;
            }
        } else {
            // Get MIPS32 instruction
            READ_CODE32 (I32.Long, Address);
            switch (I32.i_format.Opcode) {
            case JAL_OP:
                Op = BuildSimpleOp32 (Op, Address + 4, (BOOLEAN)(Op != Operations));
                RetAddress = Address + 8;
                Address = ((I32.j_format.Target << 2) | ((Address - pcOfst + 4) & 0xf0000000)) + pcOfst;
                if (I32.j_format.Target >> 26) Address |= 1;
                PCAddress = Address;
                break;
            case SPEC_OP:
                if (I32.Long == JUMP_RA && RetAddress != (ULONG) -1) {
                    Op = BuildSimpleOp32 (Op, Address + 4, (BOOLEAN)(Op != Operations));
                    Address = PCAddress = RetAddress;
                    RetAddress = (ULONG) -1;
                    continue;
                }
                // Note fallthrough
            default:
                Op = BuildSimpleOp32 (Op, Address, (BOOLEAN)(Op != Operations));
                Address += 4;
                break;
            }
        }
    }
    if (Address != EndAddress) {
        return -1;
    }
    return Op - Operations;
}

PRIVATE void
FindValue (
    IN POPERATION Ops,
    IN DWORD NumOps,
    IN REGNAME RegNum,
    IN OUT REG_TYPE *Regs)
/*++
Routine Description
    This function searches backwards through the Op array trying to deduce
    the value of an expression targeting a register.  Here, instructions
    must be executed forwards.

Arguments
    Ops - A pointer to an interpreter operation array.

    NumOps - Number of operations in the array.

    RegNum - Register to search for.

    Regs - Register value array to update.
--*/
{
    if (!IsScratchReg(RegNum)) return;

    while (NumOps-- > 0) {
        if (Ops->Rd == RegNum) {
            switch (Ops->Opcode) {
                case OP_IMM:
                    Regs[RegNum] = Ops->Imm;
                    break;

                case OP_ADD:
                case OP_SUB:
                    FindValue (Ops - 1, NumOps, Ops->Rs, Regs);
                    FindValue (Ops - 1, NumOps, Ops->Rt, Regs);
                    Regs[RegNum] = (Ops->Opcode == OP_ADD) ?
                                        Regs[Ops->Rs] + Regs[Ops->Rt] :
                                        Regs[Ops->Rs] - Regs[Ops->Rt];
                    break;

                case OP_ADDI:
                    FindValue (Ops - 1, NumOps, Ops->Rs, Regs);
                    Regs[RegNum] = Regs[Ops->Rs] + Ops->Imm;
                    break;

                case OP_MOVE:
                    FindValue (Ops - 1, NumOps, Ops->Rs, Regs);
                    Regs[RegNum] = Regs[Ops->Rs];
                    break;
            }
            // stop once we've found the definition of our register
            return;
        }
        Ops--;
    }
}

PRIVATE ULONG
Reverse (
    IN POPERATION Op,
    IN DWORD NumOps,
    IN OUT PCONTEXT Context)
/*++
Routine Description
Arguments
--*/
{
    REG_TYPE * IntRegs = &Context->IntZero;
    FREG_TYPE * FpRegs = &Context->FltF0;
    ULONG NextPc;
    BOOL RestoredRa = FALSE;

    Op += NumOps - 1;
    while (NumOps-- > 0) {
        switch (Op->Opcode) {
        case OP_ADD:
            if (Op->Rd == Op->Rs) {
                FindValue (Op-1, NumOps, Op->Rt, IntRegs);
                IntRegs[Op->Rd] -= IntRegs[Op->Rt];
            }
            break;
        case OP_SUB:
            if (Op->Rd == Op->Rs) {
                FindValue (Op-1, NumOps, Op->Rt, IntRegs);
                IntRegs[Op->Rd] += IntRegs[Op->Rt];
            }
            break;
        case OP_ADDI:
            if (Op->Rd == Op->Rs) {
                IntRegs[Op->Rd] -= Op->Imm;
            }
            break;
        case OP_STORE:
            FindValue (Op-1, NumOps, Op->Rs, IntRegs);
            READ_DATA (LONG, IntRegs[Op->Rt], (LONG)(Op->Imm + IntRegs[Op->Rs]));
            if (Op->Rt == RA) {
                if (RestoredRa == FALSE) {
                    NextPc = MIPS_PREV_INSTR(IntRegs[RA]);
                    RestoredRa = TRUE;
                } else {
                    NextPc = MIPS_NEXT_INSTR(NextPc);
                }
            }
            break;
        case OP_STORE64:
            FindValue (Op-1, NumOps, Op->Rs, IntRegs);
            READ_DATA (REG_TYPE, IntRegs[Op->Rt], (LONG)(Op->Imm + IntRegs[Op->Rs]));
            if (Op->Rt == RA) {
                if (RestoredRa == FALSE) {
                    NextPc = MIPS_PREV_INSTR(IntRegs[RA]);
                    RestoredRa = TRUE;
                } else {
                    NextPc = MIPS_NEXT_INSTR(NextPc);
                }
            }
            break;
        case OP_FSTORE:
            FindValue (Op-1, NumOps, Op->Rs, IntRegs);
            READ_DATA (ULONG, FpRegs[Op->Rt], (LONG)(Op->Imm + IntRegs[Op->Rs]));
            break;
        case OP_DSTORE:
            FindValue (Op-1, NumOps, Op->Rs, IntRegs);
            READ_DATA (FREG_TYPE, FpRegs[Op->Rt], (LONG)(Op->Imm + IntRegs[Op->Rs]));
#ifndef _MIPS64
            READ_DATA (ULONG, FpRegs[Op->Rt+1], (LONG)(Op->Imm + IntRegs[Op->Rs] + 4));
#endif  //  _MIPS64
            break;
        case OP_IMM:
            IntRegs[Op->Rd] = Op->Imm;
            break;
        case OP_MOVE:
            IntRegs[Op->Rs] = IntRegs[Op->Rd];
            if (Op->Rs == RA) {
                if (RestoredRa == FALSE) {
                    NextPc = MIPS_PREV_INSTR(IntRegs[RA]);
                    RestoredRa = TRUE;
                } else {
                    NextPc = MIPS_NEXT_INSTR(NextPc);
                }
            }
            break;
        }
        Op--;
    }
    return RestoredRa ? NextPc : MIPS_PREV_INSTR(IntRegs[RA]);
}

ULONG
RtlVirtualUnwind(
    IN ULONG ControlPc,
    IN PRUNTIME_FUNCTION FunctionEntry,
    IN OUT PCONTEXT ContextRecord,
    OUT PBOOLEAN InFunction,
    OUT PULONG EstablisherFrame,
    IN e32_lite *eptr
    )
/*++
Routine Description:
    This function virtually unwinds the specfified function by executing its
    prologue code backwards.

    If the function is a leaf function, then the address where control left
    the previous frame is obtained from the context record. If the function
    is a nested function, but not an exception or interrupt frame, then the
    prologue code is executed backwards and the address where control left
    the previous frame is obtained from the updated context record.

    Otherwise, an exception or interrupt entry to the system is being unwound
    and a specially coded prologue restores the return address twice. Once
    from the fault instruction address and once from the saved return address
    register. The first restore is returned as the function value and the
    second restore is place in the updated context record.

    If a context pointers record is specified, then the address where each
    nonvolatile registers is restored from is recorded in the appropriate
    element of the context pointers record.

Arguments:
    ControlPc - Supplies the address where control left the specified
        function.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function.

    ContextRecord - Supplies the address of a context record.

    InFunction - Supplies a pointer to a variable that receives whether the
        control PC is within the current function.

    EstablisherFrame - Supplies a pointer to a variable that receives the
        the establisher frame pointer value.

    ContextPointers - Supplies an optional pointer to a context pointers
        record.

Return Value:
    The address where control left the previous frame is returned as the
    function value.
--*/
{
    RUNTIME_FUNCTION rf;
    FREG_TYPE * FloatingRegister;
    REG_TYPE * IntegerRegister;
    MIPS_INSTRUCTION I32;
    MIPS16_INSTRUCTION I16;
    ULONG Rs;
    ULONG NextPc;
    INT OpCount;
    BOOL WithinHelper;
    POPERATION Ops;
    DWORD pcOfst = 0; // ControlPc - (DWORD) ZeroPtr (ControlPc);
    DWORD BeginAddress, PrologEndAddress;

    DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: (ControlPc=%x)\r\n", ControlPc));
    DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: entry RA=%x SP=%x\r\n", ContextRecord->IntRa, ContextRecord->IntSp));

    // Set the base address of the integer and floating register arrays.
    FloatingRegister = &ContextRecord->FltF0;
    IntegerRegister = &ContextRecord->IntZero;
    WithinHelper = FALSE;
    *InFunction = FALSE;

    // If the function is a frameless leaf function, then RA holds
    // the control PC for the previous function
    if (FunctionEntry == NULL) {
        *EstablisherFrame = (ULONG)ContextRecord->IntSp;
        DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: returning due to NULL function entry RA=%x\r\n", ContextRecord->IntRa));
        return MIPS_PREV_INSTR(ContextRecord->IntRa);
    }

    // Determine if the PC is within a prolog helper.  If it is then
    // the RA register will hold the address that we left the prolog
    // within the calling function.  Get the function entry for this
    // function.
    if (FunctionEntry->ExceptionHandler == 0 && (ULONG)FunctionEntry->HandlerData == 2) {
        // Get function entry based upon ContextRecord->IntRA
        DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: in prolog helper RA=%x\r\n", ContextRecord->IntRa));
        WithinHelper = TRUE;
        FunctionEntry = LOOKUP_FUNCTION_ENTRY (eptr, (ULONG)ContextRecord->IntRa, &rf);
        if (FunctionEntry == NULL) {
            DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: returning due to NULL function entry RA=%x\r\n", ContextRecord->IntRa));
            return 0;
        }
    } else {
        // If we are currently pointing at a return, then any saved registers
        // have been restored with the possible exception of the stack pointer.
        // The control PC is considered to be in the calling function.  If the
        // instruction in the delay slot is a stack pointer restore then it is
        // executed.
        if (ControlPc & 1) {
            DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: MIPS16 code\r\n"));
            READ_CODE16 (I16.Short, ControlPc - 1);
            if (IS_MIPS16_RETURN (I16.Short)) {
                // Determine return address (RA or Rx)
                Rs = RA;
                if (IS_MIPS16_JR (I16.Short)) {
                    Rs = FIX_MIPS16_REG (I16.rr_format.Rx);
                }

                // Get MIPS16 instruction in delay slot
                READ_CODE16 (I16.Short, ControlPc + 1);

                // Handle:  MOV32R  SP, Rx
                //          ADDIU   SP, Imm
                if (IS_MIPS16_MOV32RSP (I16.Short)) {
                    IntegerRegister[SP] = IntegerRegister[FIX_MIPS16_REG (I16.mov32r_format.Rz)];
                } else if (IS_MIPS16_ADDIUSP (I16.Short)) {
                    IntegerRegister[SP] += (I16.rsi_format.Simmediate << 3);
                }

                *EstablisherFrame = (ULONG)ContextRecord->IntSp;
                return MIPS_PREV_INSTR(IntegerRegister[Rs]);
            }
        } else {
            DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: MIPS32 code\r\n"));
            READ_CODE32 (I32.Long, ControlPc);
            if (IS_MIPS32_RETURN (I32.Long)) {
                DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: at return instruction\r\n"));

                // Get MIPS32 instruction in delay slot
                READ_CODE32 (I32.Long, ControlPc + 4);

                // Handle:  ADDIU  SP, SP, Imm
                //          ADDU   SP, SP, Rt
                if (IS_MIPS32_ADDIUSP (I32.Long)) {
                    IntegerRegister[SP] += I32.i_format.Simmediate;
                } else if (IS_MIPS32_ADDUSP (I32.Long)) {
                    IntegerRegister[SP] += IntegerRegister[I32.i_format.Rt];
                }

                *EstablisherFrame = (ULONG)ContextRecord->IntSp;
                return MIPS_PREV_INSTR(ContextRecord->IntRa);
            }
        }
    }

    // If the PC is within an epilog helper then the RA register will
    // hold the address that we left the calling function.  Use this
    // to get the function entry.
    if (FunctionEntry->ExceptionHandler == 0 && (ULONG)FunctionEntry->HandlerData == 3) {
        DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: within epilog helper\r\n"));
        // Get function entry based upon ContextRecord->IntRA - 4;
        FunctionEntry = LOOKUP_FUNCTION_ENTRY (eptr, (ULONG)ContextRecord->IntRa - 4, &rf);
        if (FunctionEntry == NULL) {
            return 0;
        }

        // Reverse execute to the end of the prologue
        ControlPc = FunctionEntry->PrologEndAddress;

        // map slot 0 address to the process
        if (ControlPc < (1 << VA_SECTION))
            ControlPc += pcOfst;
    }

    BeginAddress = FunctionEntry->BeginAddress;
    PrologEndAddress = FunctionEntry->PrologEndAddress;

    // map slot 0 address to the process
    if (BeginAddress < (1 << VA_SECTION)) {
        BeginAddress += pcOfst;
        PrologEndAddress += pcOfst;
    }

    // If the address where control left the specified function is outside
    // the limits of the prologue, then the control PC is considered to be
    // within the function and the control address is set to the end of
    // the prologue. Otherwise, the control PC is not considered to be
    // within the function (i.e., it is within the prologue).
    if ((ControlPc < BeginAddress ||
         ControlPc >= PrologEndAddress) &&
        !WithinHelper) {
        *InFunction = TRUE;
        ControlPc = PrologEndAddress;
        DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: within function, new ControPc=%x\r\n", ControlPc));
    }

    // Allocate an array of OPERATION's that will hold the entire prologue.
    if (WithinHelper) {
        OpCount = BYTES_TO_OPCOUNT(BeginAddress, PrologEndAddress);
    } else {
        OpCount = BYTES_TO_OPCOUNT(BeginAddress, ControlPc);
    }
    // Note that the prologue may include the body of a helper function. If this is the case,
    // the call and return won't be stored in Ops, so we add the size of the largest
    // prologue helper, minus the call to and return from the helper.
    OpCount += MIPS_PROLHELPER_OPCOUNT(ControlPc) - 2;
    Ops = (POPERATION) _alloca(OpCount * sizeof(OPERATION));

    // Build an operation list of all instructions from the function start
    // to the control PC.
    OpCount = BuildOps (Ops, OpCount, BeginAddress, ControlPc, pcOfst);
    DEBUGMSG(DBGEH,(L"RtlVirtualUnwind: build op list with %d elements\r\n", OpCount));

    // We were unable to build the op list
    if (OpCount < 0) {
        return 0;
    }

    NextPc = Reverse (Ops, OpCount, ContextRecord);
    DEBUGMSG(DBGEH,(L"Reverse executed RA=%x SP=%x\r\n", ContextRecord->IntRa, ContextRecord->IntSp));

    *EstablisherFrame = (ULONG)ContextRecord->IntSp;
    // Make sure that integer register zero is really zero.
    ContextRecord->IntZero = 0;
    return NextPc;
}

