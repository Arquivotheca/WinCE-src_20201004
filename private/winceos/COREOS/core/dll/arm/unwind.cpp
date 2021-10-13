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

    unwind.cpp

Abstract:

    This module implements the unwinding of ARM and Thumb procedure call
    frames for exception processing and general callstack walking.

    Note that this file is used by both the OS (when UNDER_CE is defined)
    and the debugger.  PLEASE KEEP THIS FILE AND ALL COPIES IN SYNCH.

--*/

#if defined(UNDER_CE)

#include <kernel.h>
#include <coredll.h>
#include <stdlib.h>

//
// Force the compiler to inline _lrotr to remove a dependency on the CRT
//

#pragma intrinsic(_lrotr)

//
// Expectations are assertions on the validity of input.  They are used in
// this file to indicate valid state when code is executing properly.  If
// they fail, it indicates that bad input (code, stack, etc.) was encountered
// in the process of unwinding and that there is probably a bug in user
// code.
//
// Assertions, on the other hand, are used to validate the internal state
// of code in this file.  Assertion failures indicate bugs in this file.
//

#define EXPECT(x) ASSERT(x)


static
BOOL
ArmIsBreakpoint(
    ULONG Opcode
    )

/*++

Routine Description:

    This function returns whether the given opcode is an ARM breakpoint
    instruction.

Arguments:

    Opcode - Supplies the opcode.

Return Value:

    TRUE if the opcode is a breakpoint.  False otherwise.

--*/

{
    return (Opcode == 0xe6000010) || (Opcode == 0xe7f000f1);
}


static
BOOL
ThumbIsBreakpoint(
    USHORT Opcode
    )

/*++

Routine Description:

    This function returns whether the given opcode is a Thumb breakpoint
    instruction.

Arguments:

    Opcode - Supplies the opcode.

Return Value:

    TRUE if the opcode is a breakpoint.  False otherwise.

--*/

{
    return Opcode == 0xdefe;
}


static
FORCEINLINE
VOID
ReadCode(
    __out_bcount(Size) VOID *       Destination,
    __in_bcount(Size) VOID const *  Address,
    __in ULONG                      Size,
    __in BOOL                       IsThumb
    )

/*++

Routine Description:

    This function copies memory from code with any breakpoints that may have
    been placed there by the kernel debugger removed.

Arguments:

    Destination - Supplies the address of the destination buffer.

    Address - Supplies the address of code to be read.

    Size - Supplies the number of bytes to be copied.

    IsThumb - Supplies a flag indicating whether the buffer should be
        interpreted as containing Thumb instructions.

Return Value:

    None.

--*/

{
    BOOL BreakpointPossible = FALSE;

    if (IsThumb)
    {
        USHORT * DestinationOpcode = (USHORT *)Destination;
        USHORT const * SourceOpcode = (USHORT const *)Address;

        ASSERT((Size % sizeof(USHORT)) == 0);

        for (ULONG Index = 0; Index < (Size / sizeof(USHORT)); Index++)
        {
            USHORT Opcode = SourceOpcode[Index];

            BreakpointPossible = BreakpointPossible || ThumbIsBreakpoint(Opcode);

            DestinationOpcode[Index] = Opcode;
        }
    }
    else
    {
        ULONG * DestinationOpcode = (ULONG *)Destination;
        ULONG const * SourceOpcode = (ULONG const *)Address;

        ASSERT((Size % sizeof(ULONG)) == 0);

        for (ULONG Index = 0; Index < (Size / sizeof(ULONG)); Index++)
        {
            ULONG Opcode = SourceOpcode[Index];

            BreakpointPossible = BreakpointPossible || ArmIsBreakpoint(Opcode);

            DestinationOpcode[Index] = Opcode;
        }
    }

    //
    // If a breakpoint instruction is found, ask the kernel debugger to scrub
    // the buffer of breakpoints.  There is a chance that this may be called
    // more often than necessary if the second half of a Thumb instruction
    // happens to match the Thumb breakpoint opcode, but this should be
    // exceedingly rare and should have no impact beyond a small loss of
    // performance.
    //

    if (BreakpointPossible)
    {
        KDbgSanitize(Destination, (VOID *)Address, Size);
    }
}


static
ULONG
ReadCode32(
   __in ULONG   Address
   )

/*++

Routine Description:

    This function reads a 32-bit instruction word from memory.

Arguments:

    Address - Supplies the address of instruction to be read.

Return Value:

    The instruction word.

--*/

{
    ULONG Opcode = 0;

    EXPECT((Address & 3) == 0);

    ReadCode(&Opcode, (VOID const *)Address, sizeof(Opcode), FALSE);

    return Opcode;
}


static
USHORT
ReadCode16(
   __in ULONG   Address
   )

/*++

Routine Description:

    This function reads a 16-bit instruction word from memory.

Arguments:

    Address - Supplies the address of instruction to be read.

Return Value:

    The instruction word.

--*/

{
    USHORT Opcode = 0;

    EXPECT((Address & 1) == 0);

    ReadCode(&Opcode, (VOID const *)Address, sizeof(Opcode), TRUE);

    return Opcode;
}

//
// The following macros are used to access code and stack memory in the
// executing process during virtual unwinding.
//

#define READ_CODE(pid_, dst_, src_, cnt_, thumb_) ReadCode((dst_), (VOID const *)(src_), (cnt_), (thumb_))
#define READ_CODE_ULONG(pid_, src_) ReadCode32(src_)
#define READ_CODE_USHORT(pid_, src_) ReadCode16(src_)
#define READ_DATA(pid_, dst_, src_, cnt_) memcpy((dst_), (VOID const *)(src_), (cnt_))
#define READ_DATA_ULONG(pid_, src_) (*(ULONG const *)(src_))


static
ULONG
PreviousPc (
    __in ULONG  ControlPc
    )

/*++

Routine Description:

    This function attempts to calculate the call site address given
    a return address.  It is not guaranteed to be correct since the call
    site may be arbitrarily far away from a return address.

Arguments:

    ControlPc - Supplies the return address.

Return Value:

    Address of call instruction.

--*/

{
    USHORT Opcode = 0;

    if (ControlPc & 1)
    {
        //
        // We could probably safely back up the PC only 2 bytes for *any*
        // Thumb instruction, but we only do it when we're fairly sure it's
        // a 2-byte BLX(2) instruction since there is a known potential
        // problem with EH states in that situation.  If a state ends
        // immediately before a BLX(2), it's very important that the
        // unwound PC indicates control left at the BLX(2) and not at an
        // earlier instruction.
        //

        Opcode = READ_CODE_USHORT(ProcessId, ControlPc - 3);

        if ((Opcode & 0xff87) == 0x4780)
        {
            //
            // BLX  Rm
            //
            //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-----------+---+-+-------------+
            //   |0 1 0 0 0 1|1 1|1|   Rm  |0 0 0|
            //   +-----------+---+-+-------------+
            //
            // Indirect call.  Back up the PC by 2 bytes instead of the
            // usual 4.
            //

            return ControlPc - 2;
        }
    }

    return ControlPc - 4;
}

//
// The following macro is used to adjust the return address for a function
// call.  Windows CE unwinders back up the return address by one instruction
// to attempt to indicate the address of the call site rather than the return
// address.  This has the benefit of not requiring NOP instructions to
// follow calls on exception state boundaries.
//

#define ADJUST_NEXTPC(pc) PreviousPc(pc)

//
// LOOKUP_FUNCTION_ENTRY retrieves the PDATA corresponding to a given program
// counter.  It is necessary in some corner cases during unwind.
//

EXTERN_C
PRUNTIME_FUNCTION
RtlLookupFunctionEntry(
    __in DWORD                  FunctionTableAddr,
    __in DWORD                  SizeOfFuntionTable,
    __in ULONG                  ControlPc,
    __out RUNTIME_FUNCTION *    FunctionEntry
    );

#define LOOKUP_FUNCTION_ENTRY(pid_, pc_, prf_)                             \
    RtlLookupFunctionEntry((pid_)->e32_vbase + (pid_)->e32_unit[EXC].rva, \
                           (pid_)->e32_unit[EXC].size,                   \
                           (pc_), (prf_))

//
// The process access macros are used to indicate the region of code during
// which unwindee memory must be accessed.  They may be used to trap
// exceptions thrown by memory accesses.  Exceptions during unwind are allowed
// to propagate out of the unwinder and back to enclosing handler(s).
//

#define BEGIN_PROCESS_ACCESS
#define END_PROCESS_ACCESS

//
// A parameter of type PROCESSID_T is passed to most of the functions defined
// in this file.  For exception dispatch, it is a pointer to the image header
// structure and is used to look up PDATA.  In other contexts, it may also be
// used as a handle to indicate the current process from which to read memory.
//

typedef e32_lite * PROCESSID_T;

#endif // UNDER_CE

#undef UNREACHED
#define UNREACHED FALSE

enum
{
    REG_SP   = 13,
    REG_LR   = 14,
    REG_PC   = 15,
    REG_CPSR = 16
};

#define NFLAG(cpsr_) (((cpsr_) >> 31) & 1)
#define ZFLAG(cpsr_) (((cpsr_) >> 30) & 1)
#define CFLAG(cpsr_) (((cpsr_) >> 29) & 1)
#define VFLAG(cpsr_) (((cpsr_) >> 28) & 1)

const ULONG ITSTATE_MASK_  = 0x0600fc00;
const ULONG JFLAG_MASK     = 0x01000000;
const ULONG EFLAG_MASK     = 0x00000200;
const ULONG TFLAG_MASK     = 0x00000020;

//
// When exclusive-OR'd with a 32-bit opcode, PUSH_TO_POP_MASK will toggle the
// B(efore), I(ncrement), and L(oad) bits to convert a (V)PUSH into a (V)POP.
//

const ULONG PUSH_TO_POP_MASK = 0x01900000;

const ULONG MAX_PROLOG_INSTR = 20;
const ULONG PRO_EPILOG_DELTA = 4;
const ULONG MAX_EPILOG_INSTR = (MAX_PROLOG_INSTR + PRO_EPILOG_DELTA);
const ULONG CONDCODE_INVALID = ULONG_MAX;


static
BOOL
UnwindMillicode(
    __in ULONG                      ControlPc,
    __in RUNTIME_FUNCTION const *   FunctionEntry,
    __inout CONTEXT *               ContextRecord,
    __in PROCESSID_T                ProcessId
    )

/*++

Routine Description:

    This function unwinds a "millicode" helper function, which is used by
    Thumb(1) code to save or restore registers R8-R11.

Arguments:

    ControlPc - Supplies the address where control left the specified
        function.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function.

    ContextRecord - Supplies the address of a context record.

Return Value:

    This function returns TRUE if the function is a millicode helper and it
    is successfully unwound and FALSE otherwise.

--*/

{
    ULONG Opcode = 0;

    if (FunctionEntry->ExceptionHandler != 0)
    {
        return FALSE;
    }

    //
    // If the handler is NULL and handlerdata is 1 or 2, we started in a prologue
    // or epilogue helper funclet called from Thumb code in order to facilitate
    // using high callee-save registers (R8-R11).
    //
    // The helper is called via a 4-byte Thumb BL instruction.
    //

    switch ((DWORD) FunctionEntry->HandlerData)
    {
        case 1:

            //
            // Prologue helper
            //
            //  Offset      Instruction             Mode
            //  0           BX      PC              Thumb
            //  2           NOP                     Thumb
            //  4           PUSH    {reglist}       ARM
            //  8           BX      LR              ARM
            //
            // If ControlPc does not point to the BX LR, then the PUSH has not been
            // executed.  The return address needs to be backed up by 4 bytes so that
            // the Thumb unwind will skip the BL instruction.  Otherwise, leave the
            // return address intact as the PC for the next frame and let the Thumb
            // unwinder proceed as usual.
            //

            Opcode = READ_CODE_ULONG(ProcessId, ControlPc & ~3);

            if (Opcode != 0xe12fff1e) // BX LR
            {
                ContextRecord->Lr -= 4;
            }

            ContextRecord->Pc = ContextRecord->Lr;
            break;

        case 2:

            //
            // Epilogue helper
            //
            //  Offset      Instruction             Mode
            //  0           MOV     R3,LR           Thumb
            //  2           NOP                     Thumb
            //  4           BX      PC              Thumb
            //  6           NOP                     Thumb
            //  8           POP     {reglist,LR}    ARM
            //  12          BX      R3              ARM
            //
            // If ControlPc does not point to the BX R3, then the POP has not
            // been executed.  The return address needs to be backed up by four
            // bytes so that the Thumb epilogue walk will execute the BL
            // instruction.  Otherwise, leave the return address intact as the
            // PC for the next frame and let the Thumb walk proceed as usual.
            //

            Opcode = READ_CODE_ULONG(ProcessId, ControlPc & ~3);

            if (Opcode == 0xe12fff13) // BX R3
            {
                ContextRecord->Pc = ContextRecord->R3;
            }
            else
            {
                ContextRecord->Lr -= 4;
                ContextRecord->Pc = ContextRecord->Lr;
            }
            break;

        default:
            return FALSE;
    }

    return TRUE;
}


static
BOOL
ArmCheckConditionCode(
    __in ULONG  Cpsr,
    __in ULONG  Opcode
    )

/*++

Routine Description:

    This function checks the condition code of the instruction and the
    values of the condition flags in the current program status register
    and determines whether or not the instruction will be executed.

Arguments:

    Opcode - Supplies the opcode containing the condition code in bits
        [31:28].

Return Value:

    TRUE if the given instruction would execute under current conditions.
    FALSE otherwise.

--*/

{
    BOOL Result = FALSE;

    switch (Opcode >> 28)
    {
        // EQ: Z set
        case 0:
            Result = ZFLAG(Cpsr);
            break;

        // NE: Z clear
        case 1:
            Result = !ZFLAG(Cpsr);
            break;

        // HS/CS: C set
        case 2:
            Result = CFLAG(Cpsr);
            break;

        // LO/CC: C clear
        case 3:
            Result = !CFLAG(Cpsr);
            break;

        // MI: N set
        case 4:
            Result = NFLAG(Cpsr);
            break;

        // PL: N clear
        case 5:
            Result = !NFLAG(Cpsr);
            break;

        // VS: V set
        case 6:
            Result = VFLAG(Cpsr);
            break;

        // VC: V clear
        case 7:
            Result = !VFLAG(Cpsr);
            break;

        // HI: C set and Z clear
        case 8:
            Result = CFLAG(Cpsr) && !ZFLAG(Cpsr);
            break;

        // LS: C clear or Z set
        case 9:
            Result = !CFLAG(Cpsr) || ZFLAG(Cpsr);
            break;

        // GE: N == V
        case 0xa:
            Result = NFLAG(Cpsr) == VFLAG(Cpsr);
            break;

        // LT: N != V
        case 0xb:
            Result = NFLAG(Cpsr) != VFLAG(Cpsr);
            break;

        // GT: Z clear, and N == V
        case 0xc:
            Result = !ZFLAG(Cpsr) && (NFLAG(Cpsr) == VFLAG(Cpsr));
            break;

        // LE: Z set, or N != V
        case 0xd:
            Result = ZFLAG(Cpsr) || (NFLAG(Cpsr) != VFLAG(Cpsr));
            break;

        // AL: Always execute
        case 0xe:
            Result = TRUE;
            break;

        // Unconditional - not a valid epilogue instruction
        case 0xf:
            Result = FALSE;
            break;

        default:
            ASSERT(UNREACHED);
            break;
    }

    return Result;
}


static
ULONG
ThumbCheckConditionCode(
    __in ULONG      Cpsr,
    __out ULONG *   CurrentCondCode
    )

/*++

Routine Description:

    This function checks the ITSTATE field in CPSR and the values of the
    condition flags to determine how many, if any, of the next instructions
    could be in the epilogue.

    N.B. Conditional epilogues must be contiguous and under a single
    condition.  That is, there can be no interleaved instructions with a
    differing condition code.

Arguments:

    Cpsr - Supplies the current status register (CPSR) value.

    CurrentCondCode - Supplies a pointer to a variable that recieves the
        current condition code if the instruction is inside an IT block
        or CONDCODE_INVALID otherwise.

Return Value:

    0 if the current program counter could not reside in an epilogue or if
        the condition check fails.

    MAX_EPILOG_INSTR if the current program counter is not inside an IT block.

    The maximum number of instructions that are remaining in the current
        IT block (1 to 4) otherwise.

--*/

{
    ULONG ItState = 0;
    ULONG CondCode = 0;
    ULONG NextMask = 0;
    ULONG InstructionCount = 0;

    *CurrentCondCode = CONDCODE_INVALID;

    ItState = ((Cpsr >> 25) & 0x3) | ((Cpsr >> 8) & 0xfc);

    if (ItState == 0)
    {
        //
        // No condition; execute up to the maximum number of instructions.
        //

        return MAX_EPILOG_INSTR;
    }

    CondCode = (ItState & 0xf0) << 24;
    NextMask = ItState & 0x1f;

    //
    // Check the current instruction's condition code against the
    // condition flags.  If the check fails, assume the epilogue has not
    // begun executing.
    //

    if (!ArmCheckConditionCode(Cpsr, CondCode))
    {
        return 0;
    }

    *CurrentCondCode = CondCode;

    //
    // Since the condition check has passed, calculate the maximum number
    // of subsequent instructions to check.  Since epilogues cannot be
    // interleaved with other instructions, the number of instructions
    // to check is the number of subsequent instructions with the
    // same condition code as the current instruction.
    //

    InstructionCount = 0;

    switch (NextMask)
    {
        case 0:
        case 16:

            EXPECT(!"Invalid ITSTATE field");
            break;

        case 1:
        case 31:
            InstructionCount = 4;
            break;

        case 2:
        case 3:
        case 29:
        case 30:
            InstructionCount = 3;
            break;

        case 4:
        case 5:
        case 6:
        case 7:
        case 25:
        case 26:
        case 27:
        case 28:
            InstructionCount = 2;
            break;

        default:
            InstructionCount = 1;
            break;
    }

    return InstructionCount;
}


static
VOID
ExecuteVPopDouble(
    __in ULONG                          Opcode,
    __inout_ecount(17) ULONG            Register[17],
    __inout_ecount(NUM_VFP_REGS) ULONG  FpRegister[NUM_VFP_REGS],
    __in PROCESSID_T                    ProcessId
    )

/*++

Routine Description:

    This function executes a double-precision VPOP instruction.

Arguments:

    Opcode - Supplies the ARM or 32-bit Thumb instruction word.

    Register - Supplies a pointer to an array of the 16 general purpose
        registers in an ARM context.

    FpRegister - Supplies a pointer to the floating point portion of an
        ARM context.

Return Value:

    None.

--*/

{
    ULONG RegCount = 0;
    ULONG RegIndex = 0;

    //
    // VPOP of double registers is encoded as follows:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
    //   | cond  |1 1 0|0|1|D|1|1|  Rn   |  Vd   |1 0 1 1|     imm8      |
    //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
    //
    // Where Rn = SP(13), and imm8 is the number of consecutive double-
    // precision registers to pop beginning with the register with index D:Vd.
    //

    RegCount = Opcode & 0xff;

    //
    // Popping more than 16 double precision registers results in
    // unpredictable behavior, so warn about it here, but allow it in case
    // it is ever supported.
    //

    EXPECT(RegCount > 0);
    EXPECT(RegCount <= 32);

    //
    // The FpRegister (32-bit) index is twice the double-precision index.
    //

    RegIndex = (((Opcode >> 18) & 0x10) | ((Opcode >> 12) & 0xf)) * 2;

    //
    // Do not overflow the register file.
    //

    if ((RegIndex + RegCount) > NUM_VFP_REGS)
    {
        EXPECT(!"Invalid VPOP; too many registers");
        return;
    }

    //
    // Load the registers.
    //

    READ_DATA(ProcessId, &FpRegister[RegIndex], Register[REG_SP], 4 * RegCount);

    //
    // Write back.
    //

    Register[REG_SP] += 4 * RegCount;
}


static
VOID
ExecuteLoadMultiple(
    __in ULONG                  Opcode,
    __inout_ecount(17) ULONG *  Register,
    __in PROCESSID_T            ProcessId
    )

/*++

Routine Description:

    This function executes an ARM or 32-bit Thumb load multiple instruction.

Arguments:

    Opcode - The opcode to process.

    Register - Supplies a pointer to the register states.

    ContextPointers - Supplies an optional pointer to a context pointers
        record.

Return Value:

    None.

--*/

{
    ULONG Address = 0;
    ULONG RegisterCount = 0;
    ULONG RegIndex = 0;
    ULONG RnValue = 0;

    //
    // Load multiple instructions are encoded as follows:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +-------+-----------+-+-+-------+-------------------------------+
    //   | cond  |1 0 0 B I 0|W|1|  Rn   |         register_mask         |
    //   +-------+-----------+-+-+-------+-------------------------------+
    //
    // B = before(1) or after(0)
    // I = increment(1) or decrement(0)
    // W = writeback(1)
    //

    RnValue = Register[(Opcode >> 16) & 0xf];

    RegisterCount = 0;
    for (RegIndex = 0; RegIndex < 16; RegIndex++)
    {
        if ((Opcode & (1 << RegIndex)) != 0)
        {
            RegisterCount++;
        }
    }

    //
    // The address where the registers are stored depends on the B and I bits
    //

    switch ((Opcode >> 23) & 3)
    {
        // decrement after
        case 0:
            RnValue -= 4 * RegisterCount;
            Address = RnValue - 4;
            break;

        // increment after
        case 1:
            Address = RnValue;
            RnValue += 4 * RegisterCount;
            break;

        // decrement before
        case 2:
            RnValue -= 4 * RegisterCount;
            Address = RnValue;
            break;

        // increment before
        case 3:
            Address = RnValue + 4;
            RnValue += 4 * RegisterCount;
            break;

        default:
            ASSERT(UNREACHED);
    }

    //
    // Load the registers.
    //

    for (RegIndex = 0; RegIndex < 16; RegIndex++)
    {
        if ((Opcode & (1 << RegIndex)) != 0)
        {
            Register[RegIndex] = READ_DATA_ULONG(ProcessId, Address);
            Address += 4;
        }
    }

    //
    // Writeback.
    //

    if (((Opcode >> 21) & 1) != 0)
    {
        Register[(Opcode >> 16) & 0xf] = RnValue;
    }
}


static
BOOL
BranchIsLocal(
    __in ULONG          PrologBeginAddress,
    __in ULONG          TargetAddress,
    __in PROCESSID_T    ProcessId
    )

/*++

Routine Description:

    This function determines whether an address lies within the same PDATA
    region as the given entrypoint.

    N.B. This function does not distinguish a local branch from a recursive
        call.

Arguments:

    PrologBeginAddress - Supplies the address of the entrypoint of the given
        function.

    TargetAddress - Supplies the address to compare.

Return Value:

    TRUE if the address satisfies the conditions of being local;
    FALSE otherwise.

--*/

{
    RUNTIME_FUNCTION FunctionEntry = { 0 };

    ASSERT((PrologBeginAddress & 1) == 0);

    //
    // The target address is made odd for the lookup to be compatible with
    // Thumb PDATA, which has an odd begin address.  This is still correct
    // for ARM code since there must be at least one (4-byte) instruction
    // in any ARM PDATA region.
    //

    if (LOOKUP_FUNCTION_ENTRY(ProcessId, TargetAddress | 1, &FunctionEntry)
        == NULL)
    {
        //
        // There is no PDATA for the target address, so assume it is contained
        // by a leaf function with no PDATA external to the current function.
        //

        return FALSE;
    }

    //
    // The branch is local if it targets the current PDATA region.
    //

    return (FunctionEntry.BeginAddress & ~1) == PrologBeginAddress;
}


enum BRANCH_CLASS
{
    BranchUnknown,
    BranchLocalOrRecursiveCall,
    BranchCall,
    BranchTailCall,
    BranchReturn
};


static
BRANCH_CLASS
ClassifyIndirectBranch(
    __in ULONG          PrologBeginAddress,
    __in ULONG          TargetAddress,
    __in ULONG          CurrentLr,
    __in PROCESSID_T    ProcessId
    )

/*++

Routine Description:

    This function assigns one of four categories (local, call, tail call, or
    return) to an indirect branch.

    N.B. This function should not be called if the target of the branch is
        contained in the link register (R14), as that can be assumed to be
        a return.

    N.B. This function does not distinguish a local branch from a recursive
        call.  Recursive calls could be distinguished by examining whether
        the target of the branch is an "entrypoint," but such a distinction
        is not useful to clients in this module.

Arguments:

    PrologBeginAddress - Supplies the entrypoint address of the function
        containing the branch in question.

    TargetAddress - Supplies the target address of the branch.

    CurrentLr - Supplies the link register (R14) value at the time the branch
        is executed.

Return Value:

    BranchLocalOrRecursiveCall - The branch targets its containing function.

    BranchCall - The branch is part of a call that will return to the current
        function.

    BranchTailCall - The branch is a tail call that will return to a caller
        of the current function.

    BranchReturn - The branch is a return instruction.

--*/

{
    RUNTIME_FUNCTION TargetFunctionEntry = { 0 };
    RUNTIME_FUNCTION ReturnFunctionEntry = { 0 };
    BOOL TargetHasPdata = FALSE;

    ASSERT((PrologBeginAddress & 1) == 0);

    if (LOOKUP_FUNCTION_ENTRY(ProcessId, TargetAddress | 1, &TargetFunctionEntry) != NULL)
    {
        TargetHasPdata = TRUE;
    }

    if (TargetHasPdata &&
        ((TargetFunctionEntry.BeginAddress & ~1) == PrologBeginAddress))
    {
        //
        // The target address is within the same PDATA region as the candidate
        // branch.  Assume this is a branch local to the current function.
        //

        return BranchLocalOrRecursiveCall;
    }
    else if ((LOOKUP_FUNCTION_ENTRY(ProcessId, CurrentLr, &ReturnFunctionEntry) != NULL) &&
        ((ReturnFunctionEntry.BeginAddress & ~1) == PrologBeginAddress))
    {
        //
        // The branch target is outside the current PDATA region, and the
        // link register contains an address that is inside the current PDATA
        // region.  Assume this is a true indirect call and not part of an
        // epilogue.
        //

        return BranchCall;
    }
    else if (!TargetHasPdata ||
        ((TargetFunctionEntry.BeginAddress | 1) == (TargetAddress | 1)))
    {
        //
        // The branch targets a region without PDATA or targets the entrypoint
        // of another region with PDATA, and the link register contains an
        // address that is outside the current PDATA region.  Assume this is a
        // tail call.
        //

        return BranchTailCall;
    }

    //
    // The branch target and return address lie outside the current PDATA
    // region, and the target region has PDATA, but the target is not that
    // region's entrypoint.  Assume this is a return.
    //

    return BranchReturn;
}


static
FORCEINLINE
ULONG
ThumbExtractImmediate16(
    __in ULONG  Opcode
    )

/*++

Routine Description:

    Extracts a 16-bit immediate from a Thumb-2 MOVW/MOVT instruction.

Arguments:

    Opcode - The opcode containing the immediate.

Return Value:

    The extracted immediate value.

--*/

{
    //
    // The immediate is encoded as:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +---------+-+-----------+-------+-+-----+-------+---------------+
    //   |         |i|           | imm4  | |imm3 |       |     imm8      |
    //   +---------+-+-----------+-------+-+-----+-------+---------------+
    //
    // And concatenated as imm4:i:imm3:imm8.
    //

    return ((Opcode >>  4) & 0xf000) |  // bits[15:12] in OP0[3:0]   => Opcode[19:16]
           ((Opcode >> 15) & 0x0800) |  // bits[11]    in OP0[10]    => Opcode[26]
           ((Opcode >>  4) & 0x0700) |  // bits[10:8]  in OP1[14:12] => Opcode[14:12]
           ((Opcode >>  0) & 0x00ff);   // bits[7:0]   in OP1[7:0]   => Opcode[7:0]
}


static
FORCEINLINE
ULONG
ThumbExtractImmediate12(
    __in ULONG  Opcode
    )

/*++

Routine Description:

    Extracts a 12-bit encoded immediate from a Thumb-2 instruction.

Arguments:

    Opcode - The opcode containing the immediate.

Return Value:

    The extracted immediate value.

--*/

{
    //
    // The immediate is encoded as:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +---------+-+---------------------+-----+-------+---------------+
    //   |         |i|                     |imm3 |       |     imm8      |
    //   +---------+-+---------------------+-----+-------+---------------+
    //
    // And concatenated as i:imm3:imm8.
    //

    return ((Opcode >> 15) & 0x0800) |  // bits[11]    in OP0[10]    => Opcode[26]
           ((Opcode >>  4) & 0x0700) |  // bits[10:8]  in OP1[14:12] => Opcode[14:12]
           ((Opcode >>  0) & 0x00ff);   // bits[7:0]   in OP1[7:0]   => Opcode[7:0]
}


static
FORCEINLINE
ULONG
ThumbExtractEncodedImmediate(
    __in ULONG  Opcode
    )

/*++

Routine Description:

    Extracts a 12-bit encoded immediate from a Thumb-2 instruction.

Arguments:

    Opcode - The opcode containing the immediate.

Return Value:

    The extracted immediate value.

--*/

{
    ULONG ImmediateBits = 0;
    ULONG ControlBits = 0;
    ULONG Result = 0;

    //
    // The immediate is encoded as:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +---------+-+---------------------+-----+-------+---------------+
    //   |         |i|                     |imm3 |       |     imm8      |
    //   +---------+-+---------------------+-----+-------+---------------+
    //
    // And concatenated as i:imm3:imm8. The top 5 bits are then used to
    // control a shift/rotate behavior.
    //

    ImmediateBits = ThumbExtractImmediate12(Opcode);

    ControlBits = ImmediateBits >> 7;
    ImmediateBits &= 0xff;

    ASSERT(ControlBits < 0x1000);

    switch (ControlBits)
    {
        // low 8 bits as-is
        case 0:
        case 1:
            Result = ImmediateBits;
            break;

        // low 8 bits replicated to bits 23:16 and 7:0
        case 2:
        case 3:
            Result = ImmediateBits;
            Result |= Result << 16;
            break;

        // low 8 bits replicated to bits 31:24 and 15:8
        case 4:
        case 5:
            Result = ImmediateBits << 8;
            Result |= Result << 16;
            break;

        // low 8 bits replicated to all bytes of the word
        case 6:
        case 7:
            Result = ImmediateBits;
            Result |= Result << 8;
            Result |= Result << 16;
            break;

        // 1 + low 7 bits rotated right
        default:
            Result = 0x80 | ImmediateBits;
            Result = _lrotr(Result, ControlBits);
            break;
    }

    return Result;
}


static
FORCEINLINE
LONG
ThumbExtractBranchImmediate(
    __in ULONG  Opcode
    )

/*++

Routine Description:

    Extracts a 24-bit immediate from a Thumb-2 B/BL instruction.

Arguments:

    Opcode - The opcode containing the immediate.

Return Value:

    The extracted immediate value.

--*/

{
    ULONG Result = 0;

    //
    // The 24-bit immediate is encoded as:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +---------+-+-------------------+-+-+-+-+-+---------------------+
    //   |         |S|      imm10        | | |J| |j|       imm11         |
    //   +---------+-+-------------------+-+-+-+-+-+---------------------+
    //
    // And concatenated as S:I:i:imm10:imm11:0, where I = !(S ^ J) and
    // i = !(S ^ j). The final result is sign-extended to 32-bits.
    //

    Result = (((~Opcode >> 13) & 0x001) << 23) |
             (((~Opcode >> 11) & 0x001) << 22) |
             ((( Opcode >> 16) & 0x3ff) << 12) |
             ((( Opcode >>  0) & 0x7ff) <<  1);

    if (((Opcode >> 26) & 1) != 0)
    {
        Result ^= 0xffc00000;
    }

    return Result;
}


static
BOOL
ThumbRecoverRegisterValue(
    __in_bcount(StartingPc - PrologPc) USHORT const *   Prolog,
    __in ULONG                                          PrologPc,
    __in ULONG                                          StartingPc,
    __in ULONG                                          RegIndex,
    __out ULONG *                                       RegValue,
    __in PROCESSID_T                                    ProcessId
    )

/*++

Routine Description:

    Recover the value of a register that is loaded with a constant
    in the thumb prologue.

Arguments:

    Prolog - Supplies a pointer to a copy of the prologue opcodes.

    PrologPc - Supplies the address where the original prologue opcodes
        were located.

    StartingPc - Supplies the address where the unwinder is currently
        executing.

    Register - Supplies the index of the register we are interested in.

    RegValue - Supplies a pointer where the new register value will be
        stored.

Return Value:

    TRUE if all bits of the register were accounted for; FALSE otherwise.

--*/

{
    ULONG Address = 0;
    ULONG CurrentPc = 0;
    ULONG InstructionIndex = 0;
    ULONG Opcode = 0;
    ULONG ValidValueBits = 0;

    const ULONG ALL_BITS = 0xffffffff;

    //
    // Work forward from the start of the prologue, updating the register
    // value based on instructions we know are used to set up the constant.
    //

    *RegValue = 0;

    for (CurrentPc = PrologPc; CurrentPc < StartingPc; CurrentPc += 2)
    {
        InstructionIndex = (CurrentPc - PrologPc) / 2;

        Opcode = Prolog[InstructionIndex];

        if (Opcode < 0xe800)
        {
            //
            // 16-bit instructions.
            //

            if (((Opcode & 0xf800) == 0x4800) &&
                (((Opcode >> 8) & 7) == RegIndex))
            {
                //
                // LDR  Rt,[PC,#imm]
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-----+---------------+
                //   |0 1 0 0 1| Rt  |     imm8      |
                //   +---------+-----+---------------+
                //
                // Where Rt == RegIndex.
                //
                // Load from Literal Pool into register. This instruction is
                // used in the prologue for functions with large stack frames.
                // The stack frame size is loaded from the literal pool before
                // being used to update the stack pointer.
                //

                Address = (CurrentPc & ~3) + 4 + ((Opcode & 0xff) << 2);
                *RegValue = READ_CODE_ULONG(ProcessId, Address);
                ValidValueBits = ALL_BITS;
            }
            else if (((Opcode & 0xffc0) == 0x4240) &&
                     (((Opcode >> 3) & 7) == RegIndex) &&
                     (((Opcode >> 0) & 7) == RegIndex))
            {
                //
                // RSBS Rd,Rn,#0
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-----------+-------+-----+-----+
                //   |0 1 0 0 0 0|1 0 0 1| Rn  | Rd  |
                //   +-----------+-------+-----+-----+
                //
                // Where Rd == Rt == RegIndex.
                //
                // The instruction sequence to create a large stack frame may
                // include a negate instruction.
                //

                *RegValue = 0 - *RegValue;
            }
        }
        else
        {
            //
            // 32-bit instructions. Advance the PC and assemble the opcode
            // words to match the ARM nomenclature.
            //

            Opcode = (Opcode << 16) | Prolog[InstructionIndex + 1];

            if (((Opcode & 0xfbf08000) == 0xf2400000) &&
                (((Opcode >> 8) & 0xf) == RegIndex))
            {
                //
                // MOVW Rd,#imm16
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+---+-+-+-+-+-------+-+-----+-------+---------------+
                //   |1 1 1 1 0|i|1 0|0|1|0|0| imm4  |0|imm3 |  Rd   |     imm8      |
                //   +---------+-+---+-+-+-+-+-------+-+-----+-------+---------------+
                //
                // If the stack linking is not doable in one instruction, then
                // the compiler generates a constant into a GPR (usually R12).
                // The compiler can use a MOVW to generate the constant, or its
                // lower 16bits.
                //

                *RegValue = ThumbExtractImmediate16(Opcode);
                ValidValueBits = ALL_BITS;
            }
            else if (((Opcode & 0xfbf08000) == 0xf2c00000) &&
                     (((Opcode >> 8) & 0xf) == RegIndex))
            {
                //
                // MOVT Rd,#imm16
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+---+-+-+-+-+-------+-+-----+-------+---------------+
                //   |1 1 1 1 0|i|1 0|1|1|0|0| imm4  |0|imm3 |  Rd   |     imm8      |
                //   +---------+-+---+-+-+-+-+-------+-+-----+-------+---------------+
                //
                // If the stack linking is not doable in one instruction, then
                // the compiler generates a constant into a GPR (usually R12).
                // The compiler can use a MOVT to generate the upper 16 bits of
                // the constant.
                //

                *RegValue = (*RegValue & 0x0000ffff) |
                            (ThumbExtractImmediate16(Opcode) << 16);
                ValidValueBits |= 0xffff0000;
            }
            else if ((Opcode & 0xff7f0000) == 0xf85f0000)
            {
                //
                // LDR  Rt,[PC,#+/-imm12]
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+---+-+-+---+-+-------+-------+-----------------------+
                //   |1 1 1 1 1|0 0|0|U|1 0|1|  Rn   |  Rt   |        imm12          |
                //   +---------+---+-+-+---+-+-------+-------+-----------------------+
                //
                // Where Rt == RegIndex and Rn == PC(15).
                //
                // Load from Literal Pool into register. This instruction may
                // be used in the prologues of functions with large stack frames.
                // The stack frame size is loaded from the literal pool before
                // being used to update the stack pointer.
                //

                Address = (CurrentPc & ~3) + 4;

                if (((Opcode >> 23) & 1) == 1)
                {
                    Address += (Opcode & 0xfff);
                }
                else
                {
                    Address -= (Opcode & 0xfff);
                }

                *RegValue = READ_CODE_ULONG(ProcessId, Address);
                ValidValueBits = ALL_BITS;
            }

            CurrentPc += 2;
        }
    }

    return ValidValueBits == ALL_BITS;
}


static
BOOL
ThumbIgnoreInstruction16(
    __in ULONG Opcode
    )

/*++

Routine Description:

    This function determines whether a Thumb prologue instruction is expected
    but should have no effect on the unwind.

Arguments:

    Opcode - Supplies the (16-bit) opcode for the instruction.

Return Value:

    If the instruction should be ignored, this function returns TRUE, and
    it returns FALSE otherwise.

--*/

{
    //
    // TODO: Initialize Ignore to FALSE once we have identified all
    // instructions that the compiler might need to place in the prologue.
    //

    BOOL Ignore = TRUE;

    ASSERT((Opcode & 0xffff0000) == 0);

    if ((Opcode & 0xf800) == 0x4800)
    {
        //
        // LDR  Rt,[PC,#imm8]
        //
        //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +---------+-----+---------------+
        //   |0 1 0 0 1| Rt  |     imm8      |
        //   +---------+-----+---------------+
        //
        // Load a large immediate value.  This instruction is interpreted by
        // ThumbRecoverRegisterValue.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xffc0) == 0x4240)
    {
        //
        // RSBS  Rd,Rn,#0
        //
        //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-----------+-------+-----+-----+
        //   |0 1 0 0 0 0|1 0 0 1| Rn  | Rd  |
        //   +-----------+-------+-----+-----+
        //
        // Invert an immediate value already in a register.  This instruction
        // is interpreted by ThumbRecoverRegisterValue.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xf000) == 0x0000)
    {
        //
        // LSLS Rd,Rm
        // LSRS Rd,Rm
        //
        //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-----+---+---------+-----+-----+
        //   |0 0 0|0 R|  imm5   | Rm  | Rd  |
        //   +-----+---+---------+-----+-----+
        //
        // Align a pointer.  See BIC for more information.
        //

        Ignore = TRUE;
    }
    else if (Opcode == 0xbf00)
    {
        //
        // NOP
        //
        //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+-------+-------+-------+
        //   |1 0 1 1|1 1 1 1|0 0 0 0|0 0 0 0|
        //   +-------+-------+-------+-------+
        //
        // 16-bit NOPs may be present for code alignment purposes.  Note that
        // MOV R8,R8, which is also a NOP, is handled by ThumbUnwindProlog.
        //

        Ignore = TRUE;
    }

    return Ignore;
}


static
BOOL
ThumbIgnoreInstruction32(
    __in ULONG Opcode
    )

/*++

Routine Description:

    This function determines whether a Thumb prologue instruction is expected
    but should have no effect on the unwind.

Arguments:

    Opcode - Supplies the (32-bit) opcode for the instruction.

Return Value:

    If the instruction should be ignored, this function returns TRUE, and
    it returns FALSE otherwise.

--*/

{
    //
    // TODO: Initialize Ignore to FALSE once we have identified all
    // instructions that the compiler might need to place in the prologue.
    //

    BOOL Ignore = TRUE;

    if ((Opcode & 0xf800d000) == 0xf000c000)
    {
        //
        // BLX  __chkstk
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +---------+-+-------------------+-+-+-+-+-+---------------------+
        //   |1 1 1 1 0|S|      imm10        |1|1|J|0|j|        imm11        |
        //   +---------+-+-------------------+-+-+-+-+-+---------------------+
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xfb708000) == 0xf2400000)
    {
        //
        // MOVW Rd,#imm16
        // MOVT Rd,#imm16
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +---------+-+---+-+-+-+-+-------+-+-----+-------+---------------+
        //   |1 1 1 1 0|i|1 0|t|1|0|0| imm4  |0|imm3 |  Rd   |     imm8      |
        //   +---------+-+---+-+-+-+-+-------+-+-----+-------+---------------+
        //
        // Load a large immediate value.  These instructions are interpreted by
        // ThumbRecoverRegisterValue.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xff7f0000) == 0xf85f0000)
    {
        //
        // LDR  Rt,[PC,#+/-imm12]
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +---------+---+-+-+---+-+-------+-------+-----------------------+
        //   |1 1 1 1 1|0 0|0|U|1 0|1|  Rn   |  Rt   |        imm12          |
        //   +---------+---+-+-+---+-+-------+-------+-----------------------+
        //
        // Load a large immediate value.  This instruction is interpreted by
        // ThumbRecoverRegisterValue.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xfbf00000) == 0xf0200000)
    {
        //
        // BIC  Rd,Rn,#mask
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
        //   |1 1 1 1 0|i|0|0 0 0 1|S|  Rn   |0|imm3 |  Rd   |     imm8      |
        //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
        //
        // This instruction could be used to align a pointer value such as a
        // frame pointer.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xfffff0e0) == 0xf36f0000)
    {
        //
        // BFC  Rd,#0,#width
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +---------+-+---+-----+-+-------+-+-----+-------+---+-----------+
        //   |1 1 1 1 0|0|1 1|0 1 1|0|1 1 1 1|0|imm3 |  Rd   |im2|    msb    |
        //   +---------+-+---+-----+-+-------+-+-----+-------+---+-----------+
        //
        // Where lsb (imm3:im2) == 0
        //
        // This instruction could be used to align a pointer value such as a
        // frame pointer.
        //
    }

    return Ignore;
}


static
BOOL
ThumbUnwindProlog(
    __in_bcount(PrologBytes) USHORT const * Prolog,
    __in ULONG                              PrologBytes,
    __in ULONG                              PrologPc,
    __inout CONTEXT *                       ContextRecord,
    __in PROCESSID_T                        ProcessId
    )

/*++

Routine Description:

    This function unwinds the prologue code by executing it in the
    reverse direction.

Arguments:

    Prolog - Supplies a pointer to a copy of the prologue opcodes.

    PrologBytes - Specifies the number of bytes' worth of prologue
        to unwind.

    PrologPc - Supplies the address where the original prologue opcodes
        were located.

    ContextRecord - Supplies the address of a context record.

Return Value:

    If an unexpected intstruction is found, we return FALSE and stop
    executing. Otherwise, we return TRUE.

--*/

{
    ULONG CurrentPc = 0;
    ULONG Is2ndHalfMask = 0;
    ULONG InstructionIndex = 0;
    ULONG Opcode = 0;
    ULONG * Register = &ContextRecord->R0;
    BOOL ExplicitPcFound = FALSE;

    //
    // Reverse execute the prologue instructions, starting with the last one.
    //
    // Before we begin, we need to separate the 32-bit instructions from the
    // 16-bit instructions. We do this by setting up a bitmask with 1 bit
    // per instruction word. The bitmask is set to 1 for each instruction
    // word that corresponds to the 2nd half of a 32-bit instruction.
    //

    ASSERT(PrologBytes < (8 * sizeof(Is2ndHalfMask)) * 2);
    for (CurrentPc = PrologPc;
         CurrentPc < (PrologPc + PrologBytes);
         CurrentPc += 2)
    {
        //
        // 32-bit instructions begin with the top 5 bits >= 13. This means
        // we can do a simple compare.
        //

        InstructionIndex = (CurrentPc - PrologPc) / 2;
        Opcode = Prolog[InstructionIndex];

        if (Opcode >= 0xe800)
        {
            CurrentPc += 2;
            InstructionIndex += 1;
            Is2ndHalfMask |= 1 << InstructionIndex;
        }
    }

    //
    // We must not end up in the middle of an instruction.
    //

    if (CurrentPc != PrologPc + PrologBytes)
    {
        EXPECT(!"Instruction misalignment");
        return FALSE;
    }

    //
    // Now begin reverse execution.
    //

    for (CurrentPc = PrologPc + PrologBytes - 2;
         CurrentPc >= PrologPc;
         CurrentPc -= 2)
    {
        ULONG Address = 0;
        ULONG Opcode2 = 0;
        ULONG Immediate = 0;
        ULONG RegIndex = 0;
        ULONG RegValue = 0;
        BOOL Succeeded = FALSE;

        InstructionIndex = (CurrentPc - PrologPc) / 2;
        if ((Is2ndHalfMask & (1 << InstructionIndex)) == 0)
        {
            //
            // 16-bit instructions.
            //

            Opcode = Prolog[InstructionIndex];

            if ((Opcode & 0xfe00) == 0xb400)
            {
                //
                // PUSH <registers>
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-+---+-+---------------+
                //   |1 0 1 1|0|1 0|M| register_list |
                //   +-------+-+---+-+---------------+
                //
                // Push one or more registers in (r0-r7) and/or the Link Register.
                // The registers to push are specified in an eight bit register mask.
                // The M bit specifies that the LR is to be pushed.
                //
                // Reverse this by loading the registers from the stack and undoing
                // the effects on SP.
                //

                Address = Register[REG_SP];

                for (RegIndex = 0; RegIndex < 8; RegIndex++)
                {
                    if ((Opcode & (1 << RegIndex)) != 0)
                    {
                        Register[RegIndex] = READ_DATA_ULONG(ProcessId, Address);
                        Address += 4;
                    }
                }

                if (((Opcode >> 8) & 1) != 0)
                {
                    Register[REG_LR] = READ_DATA_ULONG(ProcessId, Address);
                    Address += 4;
                }

                Register[REG_SP] = Address;
            }
            else if ((Opcode & 0xff80) == 0xb080)
            {
                //
                // SUB  SP,#imm7
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-------+-+-------------+
                //   |1 0 1 1|0 0 0 0|1|    imm7     |
                //   +-------+-------+-+-------------+
                //
                // Reverse this by adding the scaled (x4) immediate to SP.
                //

                Register[REG_SP] += (Opcode & 0x7f) << 2;
            }
            else if ((Opcode & 0xff87) == 0x4485)
            {
                //
                // ADD  SP,Rm
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-----------+---+-+-------+-----+
                //   |0 1 0 0 0 1|0 0|1|  Rm   |1 0 1|
                //   +-----------+---+-+-------+-----+
                //
                // Walk the prologue forward to recover the value in Rm, and
                // then subtract that value from Rdn.
                //

                RegIndex = (Opcode >> 3) & 0xf;

                Succeeded = ThumbRecoverRegisterValue(Prolog, PrologPc,
                    CurrentPc, RegIndex, &RegValue, ProcessId);

                EXPECT(Succeeded);
                if (Succeeded)
                {
                    Register[REG_SP] -= RegValue;
                }
            }
            else if ((Opcode & 0xf800) == 0xa800)
            {
                //
                // ADD  Rd,SP,#imm8
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-+-----+---------------+
                //   |1 0 1 0|1| Rd  |     imm8      |
                //   +-------+-+-----+---------------+
                //
                // Reverse this by subtracting the scaled (x4) offset from Rd and
                // placing the result in SP.
                //

                RegIndex = (Opcode >> 8) & 7;
                Immediate = (Opcode & 0xff) << 2;

                Register[REG_SP] = Register[RegIndex] - Immediate;
            }
            else if ((Opcode & 0xff00) == 0x4600)
            {
                //
                // MOV  Rd,Rm
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-----------+---+-+-------+-----+
                //   |0 1 0 0 0 1|1 0|D|  Rm   | Rd  |
                //   +-----------+---+-+-------+-----+
                //
                // Reverse this by moving from the destination back to the source.
                //

                RegIndex = (((Opcode >> 7) & 1) << 3) | (Opcode & 7);
                Register[(Opcode >> 3) & 0xf] = Register[RegIndex];
            }
            else if (!ThumbIgnoreInstruction16(Opcode))
            {
                //
                // Fail to unwind if we see anything we don't expect.
                //

                EXPECT(!"Unexpected prologue instruction");
                return FALSE;
            }
        }
        else
        {
            //
            // 32-bit instructions. Back up the PC and assemble the opcode
            // words to match the ARM nomenclature.
            //

            CurrentPc -= 2;
            InstructionIndex -= 1;
            Opcode = (Prolog[InstructionIndex] << 16) | Prolog[InstructionIndex + 1];

            if ((Opcode & 0xffffa000) == 0xe92d0000)
            {
                //
                // PUSH <registers>
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+---+-----+-+-+-------+-+-+-+-------------------------+
                //   |1 1 1 0 1|0 0|1 0 0|1|0|1 1 0 1|0|M|0|     register_mask       |
                //   +---------+---+-----+-+-+-------+-+-+-+-------------------------+
                //
                //  Push two or more registers.  This instruction is encoded exactly as
                //  the corresponding ARM instruction (note that the halfwords have
                //  already been swapped).  Reverse by executing the corresponding POP.
                //

                Opcode ^= PUSH_TO_POP_MASK;
                ExecuteLoadMultiple(Opcode, Register, ProcessId);
            }
            else if ((Opcode & 0xfbff8f00) == 0xf2ad0d00)
            {
                //
                // SUB  SP,#imm12
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //   |1 1 1 1 0|i|1|0 1 0 1|0|  Rn   |0|imm3 |  Rd   |     imm8      |
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //
                // Reverse this by adding the immediate value to SP.
                //

                Immediate = ThumbExtractImmediate12(Opcode);

                Register[REG_SP] += Immediate;
            }
            else if ((Opcode & 0xfbff8f00) == 0xf1ad0d00)
            {
                //
                // SUB  SP,#enc12
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //   |1 1 1 1 0|i|0|1 1 0 1|S|  Rn   |0|imm3 |  Rd   |     imm8      |
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //
                // Reverse this by adding the immediate value to SP.
                //

                Immediate = ThumbExtractEncodedImmediate(Opcode);

                Register[REG_SP] += Immediate;
            }
            else if ((Opcode & 0xfbff8000) == 0xf20d0000)
            {
                //
                // ADD  Rd,SP,#imm12
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //   |1 1 1 1 0|i|1|0 0 0 0|0|  Rn   |0|imm3 |  Rd   |     imm8      |
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //
                // Undo this by adding the immediate value to Rd and storing the
                // result in Rn.
                //

                RegIndex = (Opcode >> 8) & 0xf;
                Immediate = ThumbExtractImmediate12(Opcode);

                Register[REG_SP] = Register[RegIndex] - Immediate;
            }
            else if ((Opcode & 0xfbff8000) == 0xf10d0000)
            {
                //
                // ADD  Rd,SP,#enc12
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //   |1 1 1 1 0|i|0|1 0 0 0|S|  Rn   |0|imm3 |  Rd   |     imm8      |
                //   +---------+-+-+-------+-+-------+-+-----+-------+---------------+
                //
                // Where S == 0.
                //
                // Undo this by adding the immediate value to Rd and storing the
                // result in Rn.
                //

                RegIndex = (Opcode >> 8) & 0xf;
                Immediate = ThumbExtractEncodedImmediate(Opcode);

                Register[REG_SP] = Register[RegIndex] - Immediate;
            }
            else if ((Opcode & 0xffbf0f00) == 0xed2d0b00)
            {
                //
                // VPUSH <registers>
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
                //   |1 1 1 0|1 1 0|0|1|D|1|1|  Rn   |  Vd   |1 0 1 1|     imm8      |
                //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
                //
                // Push one or more VFP/NEON registers onto the stack.  This
                // instruction is encoded exactly as is the corresponding ARM instruction
                // (note that the halfwords have already been swapped).  Reverse this
                // by executing the corresponding VPOP instruction.
                //

                Opcode ^= PUSH_TO_POP_MASK;
                ExecuteVPopDouble(Opcode, Register, &ContextRecord->S[0], ProcessId);
            }
            else if ((Opcode & 0xfffffff0) == 0xebad0d00)
            {
                //
                // SUB  SP,Rm
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-+-------+-+-------+-+-----+-------+---+---+-------+
                //   |1 1 1 0 1|0|1|1 1 0 1|0|  Rn   |0|0 0 0|  Rd   |0 0|0 0|  Rm   |
                //   +---------+-+-+-------+-+-------+-+-----+-------+---+---+-------+
                //
                // Walk the prologue forward to recover the value in Rm and then add
                // that to the stack pointer.
                //

                RegIndex = Opcode & 0xf;
                Succeeded = ThumbRecoverRegisterValue(Prolog, PrologPc,
                    CurrentPc, RegIndex, &RegValue, ProcessId);

                EXPECT(Succeeded);
                if (Succeeded)
                {
                    Register[REG_SP] += RegValue;
                }
            }
            else if ((Opcode & 0xf800d000) == 0xf000d000)
            {
                //
                // BL   <label>
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-------------------+-+-+-+-+-+---------------------+
                //   |1 1 1 1 0|S|      imm10        |1|1|J|1|j|        imm11        |
                //   +---------+-+-------------------+-+-+-+-+-+---------------------+
                //
                // Only calls to register save millicode routines are valid within
                // the prologue. Register save routines have the form:
                //
                //  BX      PC          ; Thumb instruction
                //  NOP.N               ; Thumb instruction
                //  PUSH    {reglist}   ; ARM instruction
                //  BX      LR          ; ARM instruction
                //
                // The following code loads the instruction at the function
                // destination address and verifies that it matches the sequence
                // above. If not, the instruction is ignored on the assumption that
                // it is another helper like __chkstk.  Otherwise, the push
                // instruction is unwound.
                //

                Immediate = ThumbExtractBranchImmediate(Opcode);
                Address = CurrentPc + 4 + Immediate;

                if ((Address & 3) != 0)
                {
                    //
                    // Millicode helpers must be 4-byte aligned.
                    //

                    continue;
                }

                Opcode2 = READ_CODE_ULONG(ProcessId, Address);

                //
                // BX   PC
                // MOV  R8,R8 (NOP)
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //                                   +-----------+---+-+-------+-----+
                //                                   |0 1 0 0 0 1|1 1|0|   Rm  |0 0 0|
                //   +-----------+---+-+-------+-----+-----------+---+-+-------+-----+
                //   |0 1 0 0 0 1|1 0|D|   Rm  |  Rd |
                //   +-----------+---+-+-------+-----+
                //
                //   Where Rm == PC(15) for BX
                //   Where Rm == D:Rd == R8 for MOV
                //
                // This sequence switches from Thumb code to ARM code so that the
                // millicode helper can be entered directly via a Thumb BL instruction.
                // We match this pattern to ensure we are entering a millicode helper
                // as opposed to another helper like __chkstk.  Technically, we could
                // (or perhaps should) look up PDATA to ensure that the helper has a
                // NULL handler with handlerdata = 1.

                if (Opcode2 != 0x46c04778)
                {
                    //
                    // Assume this is something other than a millicode helper
                    //

                    continue;
                }

                Opcode2 = READ_CODE_ULONG(ProcessId, Address + 4);

                //
                // Now we're looking for the STM opcode in the handler, which is a
                // full 32-bit ARM opcode:
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-----------+-+-+-------+-------------------------------+
                //   | cond  |1 0 0 1 0 0|1|0|  Rn   |         register_mask         |
                //   +-------+-----------+-+-+-------+-------------------------------+
                //
                // Where Rn == SP(13).
                //
                // Convert this instruction to a load multiple, with inverted
                // increment/decrement and before/after bits.
                //

                if ((Opcode2 & 0xffffe000) == 0xe92d0000)
                {
                    Opcode2 ^= PUSH_TO_POP_MASK;
                    ExecuteLoadMultiple(Opcode2, Register, ProcessId);
                }
                else
                {
                    //
                    // Pattern not recognized--fail.
                    //

                    EXPECT(UNREACHED);
                    return FALSE;
                }
            }
            else if ((Opcode & 0xffff0fff) == 0xf84d0d04)
            {
                //
                // PUSH {Rt}
                // PUSH {PC} (unpredictable encoding)
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+---+-------+-+-------+-------+-------+---------------+
                //   |1 1 1 1 1|0 0|0|0|1|0|0|1 1 0 1|   Rt  |1|1 0 1|0 0 0 0 0 1 0 0|
                //   +---------+---+-------+-+-------+-------+-------+---------------+
                //
                // Push single register (Rt).
                //
                // PUSH {PC} only exists in "fake" prologues constructed to assist the
                // unwinder across exception frames.  It is never actually executed
                // (its behavior is unpredictable according to the ARM ARM).
                //

                RegIndex = (Opcode >> 12) & 0xf;

                Register[RegIndex] = READ_DATA_ULONG(ProcessId, Register[REG_SP]);
                Register[REG_SP] += 4;
                if (RegIndex == REG_PC)
                {
                    ExplicitPcFound = TRUE;
                }
            }
            else if (!ThumbIgnoreInstruction32(Opcode))
            {
                //
                // Fail to unwind if we see anything we don't expect.
                //

                EXPECT(!"Unexpected prologue instruction");
                return FALSE;
            }
        }
    }

    if (!ExplicitPcFound)
    {
        //
        // Copy LR to PC if an explicit PC "push" wasn't found, i.e. this wasn't an
        // exception frame.
        //

        Register[REG_PC] = ADJUST_NEXTPC(Register[REG_LR]);
    }

    return TRUE;
}


static
BOOL
ThumbWalkEpilog(
    __in ULONG          PrologBeginAddress,
    __in ULONG          EpilogPc,
    __in ULONG          MaxEpilogBytes,
    __inout CONTEXT *   ContextRecord,
    __in PROCESSID_T    ProcessId
    )

/*++

Routine Description:

    This function executes the given code in the forward direction, assuming
    it is an epilogue. If it is not an epilogue, then execution is aborted and
    an error is signaled.

Arguments:

    ControlPc - Supplies the address where execution should begin.

    MaxEpilogBytes - Supplies the maximum number of bytes of code to consider
        as part of the epilogue.

    ContextRecord - Supplies the address of a context record.

Return Value:

    If an unexpected intstruction is found, we assume it is not an epilogue,
    and return FALSE. If only epilogue-proper instructions are found and all
    were executed, we return TRUE.

--*/

{
    ULONG CurrentPc = 0;
    ULONG OriginalSp = 0;
    ULONG InstructionsExecuted = 0;
    ULONG MaxInstructions = 0;
    ULONG OriginalCondCode = 0;
    ULONG * Register = &ContextRecord->R0;

    ASSERT(MaxEpilogBytes <= (4 * MAX_EPILOG_INSTR));

    //
    // Back up the SP register so it can be restored in the case of failure.
    //

    OriginalSp = Register[REG_SP];

    //
    // Thumb epilogues may be in IT blocks.  If so, they must be contiguous,
    // i.e. non-epilogue instructions (with another condition) may not be
    // interleaved among epilogue instructions.  The maximum number of
    // instructions in the epilogue is the number of remaining instructions
    // in the current IT block, if any, plus one to cover the possibility
    // of a conditional branch with its own embedded condition.  This is
    // just a heuristic at this point because we don't check to make sure
    // the "+1" instruction after an IT block is a conditional branch, so we
    // are slightly more permissive than we should be.
    //

    MaxInstructions = ThumbCheckConditionCode(Register[REG_CPSR], &OriginalCondCode);

    if ((MaxInstructions != 0) && (MaxInstructions != MAX_EPILOG_INSTR))
    {
        //
        // Allow conditional branch even if the current instruction is inside
        // an IT block.  If MaxInstructions == 0, the current instruction has
        // failed its condition check, so there is no possibility of being in
        // an epilogue.
        //

        ++MaxInstructions;
    }

    //
    // Now begin forward execution.
    //

    for (CurrentPc = EpilogPc;
         CurrentPc < (EpilogPc + MaxEpilogBytes);
         CurrentPc += 2)
    {
        ULONG Address = 0;
        ULONG Immediate = 0;
        ULONG Opcode = 0;
        ULONG Opcode2 = 0;
        ULONG RegIndex = 0;
        BRANCH_CLASS BranchClass = BranchUnknown;

        if (InstructionsExecuted == MaxInstructions)
        {
            break;
        }

        ASSERT(InstructionsExecuted < MaxInstructions);

        Opcode = READ_CODE_USHORT(ProcessId, CurrentPc);

        if (Opcode < 0xe800)
        {
            //
            // 16-bit instructions
            //

            if ((Opcode & 0xff87) == 0x4700)
            {
                //
                // BX   Rm
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-----------+---+-+-------------+
                //   |0 1 0 0 0 1|1 1|0|   Rm  |0 0 0|
                //   +-----------+---+-+-------------+
                //
                // Return if this is a return or tail call.  Ignore if this is
                // a local branch or true indirect call.
                //

                RegIndex = (Opcode >> 3) & 0xf;

                if (RegIndex != REG_LR)
                {
                    BranchClass = ClassifyIndirectBranch(PrologBeginAddress,
                        Register[RegIndex], Register[REG_LR], ProcessId);

                    if (BranchClass == BranchTailCall)
                    {
                        //
                        // Assume this is an indirect tail call with the return
                        // address in LR (R14).
                        //

                        RegIndex = REG_LR;
                    }
                    else if (BranchClass != BranchReturn)
                    {
                        //
                        // Assume this is not a return instruction and
                        // therefore not part of an epilogue.
                        //

                        break;
                    }
                }

                Register[REG_PC] = Register[RegIndex];
                goto Succeed;
            }
            else if ((Opcode & 0xfe00) == 0xbc00)
            {
                //
                // POP  {R0,...}
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-+---+-+---------------+
                //   |1 0 1 1|1|1 0|M| register_list |
                //   +-------+-+---+-+---------------+
                //
                // Pop zero or more (low) GPRs and/or the PC.
                // The GPRs to push are specified in an eight bit
                // register mask. The M bit specifies that the PC is
                // to be popped (a return instruction).
                //

                Address = Register[REG_SP];

                for (RegIndex = 0; RegIndex < 8; RegIndex++)
                {
                    if ((Opcode & (1 << RegIndex)) != 0)
                    {
                        Register[RegIndex] = READ_DATA_ULONG(ProcessId, Address);
                        Address += 4;
                    }
                }

                if (((Opcode >> 8) & 1) != 0)
                {
                    Register[REG_PC] = READ_DATA_ULONG(ProcessId, Address);
                    Register[REG_SP] = Address + 4;
                    goto Succeed;
                }

                Register[REG_SP] = Address;
            }
            else if ((Opcode & 0xff00) == 0xb000)
            {
                //
                // ADD  SP,#imm7
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-------+-+-------------+
                //   |1 0 1 1|0 0 0 0|0|    imm7     |
                //   +-------+-------+-+-------------+
                //
                // Adjust the stack pointer by adding a scaled 7-bit
                // immediate (shifted by 2 bits).
                //

                Register[REG_SP] += (Opcode & 0x7f) << 2;
            }
            else if ((Opcode & 0xff00) == 0x4600)
            {
                //
                // MOV  Rd,Rm
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-----------+---+-+-------+-----+
                //   |0 1 0 0 0 1|1 0|D|  Rm   | Rdn |
                //   +-----------+---+-+-------+-----+
                //

                RegIndex = (((Opcode >> 7) & 1) << 3) | (Opcode & 7);
                Register[RegIndex] = Register[(Opcode >> 3) & 0xf];
            }
            else if (Opcode == 0xbf00)
            {
                //
                // NOP
                //
                //   15 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-------+-------+-------+
                //   |1 0 1 1|1 1 1 1|0 0 0 0|0 0 0 0|
                //   +-------+-------+-------+-------+
                //
                // NOPs may be present for code alignment or hot patch
                // purposes.  Note that MOV R8, R8, which is also a NOP, is
                // handled above.
                //
            }
            else
            {
                //
                // Assume we are not in an epilogue if we see anything else.
                //

                break;
            }
        }
        else
        {
            //
            // 32-bit instructions.  Back up the PC and assemble the opcode
            // words to match the ARM nomenclature.
            //

            Opcode = (Opcode << 16) | READ_CODE_USHORT(ProcessId, CurrentPc + 2);

            if (((Opcode & 0xffff2000) == 0xe8bd0000) &&
                ((Opcode & 0x0000c000) != 0x000c0000))
            {
                //
                // POP  {R0,...}
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+---+-----+-+-+-------+-+-+-+-------------------------+
                //   |1 1 1 0 1|0 0|0 1 0|1|1|1 1 0 1|P|M|0|     register_mask       |
                //   +---------+---+-----+-+-+-------+-+-+-+-------------------------+
                //
                //  Where P and M not both 1
                //
                //  Pop two or more registers.  This instruction is encoded exactly as
                //  is the corresponding ARM instruction (note that the halfwords have
                //  already been swapped).  If P is 1, this is a return instruction.
                //

                ExecuteLoadMultiple(Opcode, Register, ProcessId);

                if (((Opcode >> 15) & 1) != 0)
                {
                    goto Succeed;
                }
            }
            else if ((Opcode & 0xffff0f00) == 0xf85d0b00)
            {
                //
                // POP  {Rt}
                // LDR  Rt,[SP],#imm8
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+---+-+-+---+-+-------+-------+-+-----+---------------+
                //   |1 1 1 1 1|0 0|0|0|1 0|1|  Rn   |  Rt   |1|P U W|    imm8       |
                //   +---------+---+-+-+---+-+-------+-------+-+-----+---------------+
                //
                // Where Rn == SP(13), P = 0, U = 1, and W = 1.
                //
                // Execute this instruction.  If PC is the popped register, return
                // success as this is a return instruction.
                //

                RegIndex = (Opcode >> 12) & 0xf;

                Register[RegIndex] = READ_DATA_ULONG(ProcessId, Register[REG_SP]);
                Register[REG_SP] += (Opcode & 0xff);
                if (RegIndex == REG_PC)
                {
                    goto Succeed;
                }
            }
            else if ((Opcode & 0xfff0f000) == 0xf8d0f000)
            {
                //
                // LDR  PC,[Rn,#imm12]
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+---+-+-+---+-+-------+-------+-----------------------+
                //   |1 1 1 1 1|0 0|0|1|1 0|1|  Rn   |  Rt   |         imm12         |
                //   +---------+---+-+-+---+-+-------+-------+-----------------------+
                //
                // Where Rt == PC(15).
                //
                // If the branch type is a tail call, execute a return via the link
                // register (R14).
                //
                // Note that only non-negative offsets from the base register are
                // supported.
                //

                RegIndex = (Opcode >> 16) & 0xf;
                Address = READ_DATA_ULONG(ProcessId,
                    Register[RegIndex] + (Opcode & 0xfff));

                BranchClass = ClassifyIndirectBranch(PrologBeginAddress,
                    Address, Register[REG_LR], ProcessId);

                if (BranchClass == BranchTailCall)
                {
                    //
                    // This is a tail call with the return address in LR.
                    //

                    Register[REG_PC] = Register[REG_LR];
                    goto Succeed;
                }

                //
                // Assume this instruction is not part of an epilogue.
                //

                break;
            }
            else if ((Opcode & 0xf800d000) == 0xf000d000)
            {
                //
                // BL   <label>
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-------------------+-+-+-+-+-+---------------------+
                //   |1 1 1 1 0|S|      imm10        |1|1|J|1|j|        imm11        |
                //   +---------+-+-------------------+-+-+-+-+-+---------------------+
                //
                // Only calls to register restore millicode routines are valid within
                // the epilogue. Register restore routines have the form:
                //
                //  MOV     R3,LR       ; Thumb instruction
                //  NOP.N               ; Thumb instruction
                //  BX      PC          ; Thumb instruction
                //  NOP.N               ; Thumb instruction
                //  POP     {reglist}   ; ARM instruction
                //  BX      LR          ; ARM instruction
                //
                // The following code loads the instruction at the function
                // destination address plus four and verifies that it matches the
                // BX/NOP sequence above.  If not, epilogue walking halts.  Otherwise
                // the pop is executed.
                //

                //
                // BL can only occur as the last instruction in an IT block, but since
                // more epilogue instructions must follow, this cannot be an epilogue.
                //

                if (MaxInstructions != MAX_EPILOG_INSTR)
                {
                    break;
                }

                Immediate = ThumbExtractBranchImmediate(Opcode);
                Address = CurrentPc + 4 + Immediate + 4;

                if ((Address & 3) != 0)
                {
                    //
                    // Millicode helpers must be 4-byte aligned.
                    //

                    break;
                }

                Opcode2 = READ_CODE_ULONG(ProcessId, Address);

                //
                // BX   PC
                // MOV  R8,R8 (formerly NOP)
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //                                   +-----------+---+-+-------+-----+
                //                                   |0 1 0 0 0 1|1 1|0|   Rm  |0 0 0|
                //   +-----------+---+-+-------+-----+-----------+---+-+-------+-----+
                //   |0 1 0 0 0 1|1 0|D|   Rm  |  Rd |
                //   +-----------+---+-+-------+-----+
                //
                //   Where Rm == PC(15) for BX
                //   Where Rm == D:Rd == R8 for MOV
                //
                // This sequence switches from Thumb code to ARM code so that the
                // millicode helper can be entered directly via a Thumb BL instruction.
                // We match this pattern to ensure we are entering a millicode helper
                // as opposed to another helper like __chkstk.  Technically, we could
                // (or perhaps should) look up PDATA to ensure that the helper has a
                // NULL handler with handlerdata = 1.

                if (Opcode2 != 0x46c04778)
                {
                    //
                    // Assume we aren't in an epilogue.
                    //

                    break;
                }

                Opcode2 = READ_CODE_ULONG(ProcessId, Address + 4);

                //
                // Now we're looking for the POP opcode in the handler, which is a
                // full 32-bit ARM opcode:
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-----------+-+-+-------+-------------------------------+
                //   | cond  |1 0 0 0 1 0|1|1|  Rn   |         register_mask         |
                //   +-------+-----------+-+-+-------+-------------------------------+
                //
                // Where Rn == SP(13).
                //
                // Execute the POP.
                //

                if ((Opcode2 & 0xffffa000) == 0xe8bd0000)
                {
                    ExecuteLoadMultiple(Opcode2, Register, ProcessId);
                }
                else
                {
                    //
                    // Assume we aren't in an epilogue.
                    //

                    break;
                }
            }
            else if ((Opcode & 0xf800c000) == 0xf0008000)
            {
                //
                // B    <label>
                // B<c> <label>
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +---------+-+-------------------+-+-+-+-+-+---------------------+
                //   |1 1 1 1 0|S|      imm10        |1|0|J|1|j|        imm11        |
                //   +---------+-+-------+-----------+-+-+-+-+-+---------------------+
                //   |1 1 1 1 0|S| cond  |   imm6    |1|0|J|0|j|        imm11        |
                //   +---------+-+-------+-----------+-+-+-+-+-+---------------------+
                //
                // B is a tail call if it branches to the entrypoint of another
                // function.
                //

                if (((Opcode >> 12) & 1) != 0)
                {
                    //
                    // Unconditional branch
                    //

                    Immediate = ThumbExtractBranchImmediate(Opcode);
                }
                else
                {
                    //
                    // Conditional branch
                    //

                    ULONG CondCode = (Opcode << 6) & 0xf0000000;

                    if (CondCode >= 0xe0000000)
                    {
                        //
                        // This is not a branch instruction.
                        //

                        break;
                    }

                    if (OriginalCondCode == CONDCODE_INVALID)
                    {
                        //
                        // There was no previous condition.  This instruction
                        // can only be part of an epilogue if EpilogPc points
                        // at it, i.e. it's the first instruction considered.
                        //

                        if (CurrentPc != EpilogPc)
                        {
                            break;
                        }
                    }
                    else if (CondCode != OriginalCondCode)
                    {
                        //
                        // The condition on the branch must match the condition
                        // of a previous IT block.
                        //

                        break;
                    }

                    Immediate = (((Opcode >> 13) & 0x001) << 19) |
                                (((Opcode >> 11) & 0x001) << 18) |
                                (((Opcode >> 16) & 0x03f) << 12) |
                                (((Opcode >>  0) & 0x7ff) <<  1);

                    if (((Opcode >> 26) & 1) != 0)
                    {
                        Immediate |= 0xfff00000;
                    }
                }

                Address = CurrentPc + 4 + Immediate;

                if (BranchIsLocal(PrologBeginAddress, Address, ProcessId))
                {
                    //
                    // Assume this is a branch within the current function and not
                    // part of the epilogue.
                    //

                    break;
                }

                //
                // Assume this is a tail call.
                //

                Register[REG_PC] = Register[REG_LR];
                goto Succeed;
            }
            else if ((Opcode & 0xffbf0f00) == 0xecbd0b00)
            {
                //
                // VPOP <registers>
                //
                //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
                //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
                //   |1 1 1 0|1 1 0|0|1|D|1|1|  Rn   |  Vd   |1 0 1 1|     imm8      |
                //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
                //
                // Where Rn == SP(13).
                //
                // Restore one or more VFP/NEON registers.  This instruction is encoded
                // exactly as is the corresponding ARM instruction (note that the
                // halfwords have already been swapped).
                //

                ExecuteVPopDouble(Opcode, Register, &ContextRecord->S[0], ProcessId);
            }
            else
            {
                //
                // Assume we are not in an epilogue if we see anything else.
                //

                break;
            }

            CurrentPc += 2;
        }

        InstructionsExecuted++;
    }

    //
    // Fail.  Restore SP so the prologue unwind can proceed.
    //

    Register[REG_SP] = OriginalSp;
    return FALSE;

Succeed:

    //
    // Return address is in PC.
    //

    Register[REG_PC] = ADJUST_NEXTPC(Register[REG_PC]);
    return TRUE;
}


static
VOID
ResetExecutionState(
    __inout CONTEXT *   ContextRecord
    )

/*++

Routine Description:

    This function clears all of the execution state bits except possibly
    T(humb) in CPSR.

Arguments:

    ContextRecord - Supplies the address of a context record.

Notes:

    The ABI requires J(azelle), ITSTATE, and (Big) E(ndian) bits to be clear upon
    entry to a function.  Therefore, these execution state bits should be
    cleared after unwinding one frame in case the resulting context is used
    to resume execution at a point after the unwind.  The T(humb) bit is set to
    LSb of the PC as it would be upon return via BX.

--*/

{
    ContextRecord->Psr &= ~(JFLAG_MASK | ITSTATE_MASK_ | EFLAG_MASK | TFLAG_MASK);
    if ((ContextRecord->Pc & 1) != 0)
    {
        ContextRecord->Psr |= TFLAG_MASK;
    }
}


static
BOOL
ThumbVirtualUnwind(
    __in ULONG                      ControlPc,
    __in RUNTIME_FUNCTION const *   FunctionEntry,
    __inout CONTEXT *               ContextRecord,
    __out BOOLEAN *                 InFunction,
    __in PROCESSID_T                ProcessId
    )

/*++

Routine Description:

    This function virtually unwinds the specified function by executing its
    prologue code backward or its epilogue code forward.

Arguments:

    ControlPc - Supplies the address where control left the specified
        function.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function.

    ContextRecord - Supplies the address of a context record.

    InFunction - Supplies a pointer to a variable that receives a flag
        indicating whether control has reached the function body.

Return Value:

    The address where control left the caller or the original ControlPc value
    if an error occurs.

--*/

{
    USHORT Prolog[MAX_PROLOG_INSTR];
    ULONG PrologBeginAddress = 0;
    ULONG PrologBytes = 0;
    ULONG PrologEndAddress = 0;
    ULONG * Register = NULL;
    BOOL Succeeded = FALSE;

    ASSERT((ControlPc & 1) == 1);
    ASSERT(FunctionEntry != NULL);
    ASSERT(ContextRecord != NULL);
    ASSERT(InFunction != NULL);

    ControlPc &= ~1;

    *InFunction = TRUE;

#ifndef RUNTIME_FUNCTION_NO_IS32BIT_FLAG
    if ((FunctionEntry->BeginAddress & 1) == 0)
    {
        //
        // PDATA does not indicate Thumb mode
        //

        goto Done;
    }
#endif

    PrologBeginAddress = FunctionEntry->BeginAddress & ~1;
    PrologEndAddress = FunctionEntry->PrologEndAddress & ~1;
    Register = &ContextRecord->R0;

    //
    // If no prologue, copy LR to PC and leave.
    //

    if (PrologEndAddress == PrologBeginAddress)
    {
        Register[REG_PC] = ADJUST_NEXTPC(Register[REG_LR]);
        Succeeded = TRUE;
        goto Done;
    }

    //
    // Determine the size of the prologue. Fail if the prologue is larger than
    // we can handle.
    //

    PrologBytes = PrologEndAddress - PrologBeginAddress;

    if (PrologBytes > sizeof(Prolog))
    {
        EXPECT(!"Too many prologue instructions");
        goto Done;
    }

    //
    // If we are in the prologue, then we shouldn't execute instructions past
    // our current location; otherwise, assume we are in the function proper.
    // Note that we must check against the beginning of the prologue as well
    // as the end, since a separated function may have a portion of its body
    // at a lower address than the prologue.
    //

    if ((ControlPc >= PrologBeginAddress) && (ControlPc < PrologEndAddress))
    {
        PrologBytes = ControlPc - PrologBeginAddress;
        *InFunction = FALSE;
    }
    else
    {
        //
        // Attempt to execute epilogue instructions forward.  If we fail,
        // assume we are in the function body and unwind the prologue.
        //

        Succeeded = ThumbWalkEpilog(PrologBeginAddress, ControlPc,
            PrologBytes + (4 * PRO_EPILOG_DELTA), ContextRecord, ProcessId);
    }

    if (!Succeeded)
    {
        READ_CODE(ProcessId, Prolog, PrologBeginAddress, PrologBytes, TRUE);

        //
        // Execute the prologue instructions backwards, either from the end
        // or from where we left off.
        //

        Succeeded = ThumbUnwindProlog(Prolog, PrologBytes,
            PrologBeginAddress, ContextRecord, ProcessId);
    }

Done:

    return Succeeded;
}


static
VOID
ArmExecuteDataProcess(
    __in ULONG              Opcode,
    __in_ecount(17) ULONG * Register
    )

/*++

Routine Description:

    This function executes an ARM data processing instruction using the
    current register set. It automatically updates the destination
    register.

Arguments:

    Opcode - The opcode to process.

    Register - Supplies a pointer to the register states.

Return Value:

    None.

--*/

{
    ULONG Cflag = 0;
    ULONG Operand1 = 0;
    ULONG Operand2 = 0;
    ULONG Shift = 0;

    //
    // Data processing instructions are encoded as follows:
    //
    //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
    //   +-------+---+-+-------+-+-------+-------+-------+---------------+
    //   | cond  |0 0|1|opcode |S|  Rn   |  Rd   |rotate |   immediate   |
    //   +-------+---+-+-------+-+-------+-------+-------+-+-----+-------+
    //   | cond  |0 0|0|opcode |S|  Rn   |  Rd   |   imm5  |typ|0|  Rm   |
    //   +-------+---+-+-------+-+-------+-------+-------+-+-----+-------+
    //   | cond  |0 0|0|opcode |S|  Rn   |  Rd   |  Rs   |0|typ|1|  Rm   |
    //   +-------+---+-+-------+-+-------+-------+-------+-------+-------+
    //
    // The first form is an 8-bit immediate value with a 4-bit rotation.
    // The second form is a register value with an immediate shift.
    // The third form is a register value with a register shift.
    //

    Operand1 = Register[(Opcode >> 16) & 0xf];
    Cflag = CFLAG(Register[16]);

    if (((Opcode >> 25) & 1) != 0)
    {
        //
        // Immediate case
        //

        Operand2 = _lrotr(Opcode & 0xff, ((Opcode >> 8) & 0xf) * 2);
    }
    else
    {
        //
        // Register cases
        //

        Operand2 = Register[Opcode & 0xf];
        switch ((Opcode >> 4) & 7)
        {
            // Logical left-shift immediate
            case 0:
                Shift = (Opcode >> 7) & 0x1f;
                Operand2 = Operand2 << Shift;
                break;

            // Logical left-shift register
            case 1:
                Shift = Register[(Opcode >> 8) & 0xf] & 0xff;
                Operand2 = (Shift < 32) ? ((ULONG)Operand2 << Shift) : 0;
                break;

            // Logical right-shift immediate
            case 2:
                Shift = (Opcode >> 7) & 0x1f;
                Operand2 = (Shift != 0) ? ((ULONG)Operand2 >> Shift) : 0;
                break;

            // Logical right-shift register
            case 3:
                Shift = Register[(Opcode >> 8) & 0xf] & 0xff;
                Operand2 = (Shift < 32) ? ((ULONG)Operand2 >> Shift) : 0;
                break;

            // Arithmetic right-shift immediate
            case 4:
                Shift = (Opcode >> 7) & 0x1f;
                Operand2 = (Shift != 0) ? ((LONG)Operand2 >> Shift) :
                                          ((LONG)Operand2 >> 31);
                break;

            // Arithmetic right-shift register
            case 5:
                Shift = Register[(Opcode >> 8) & 0xf] & 0xff;
                Operand2 = (Shift < 32) ? ((LONG)Operand2 >> Shift) :
                                          ((LONG)Operand2 >> 31);
                break;

            // Logical right rotate immediate
            case 6:
                Shift = (Opcode >> 7) & 0x1f;
                Operand2 = (Shift != 0) ? _lrotr(Operand2, Shift) :
                                          ((Cflag << 31) | (Operand2 >> 1));
                break;

            // Logical right rotate register
            case 7:
                Shift = Register[(Opcode >> 8) & 0xf] & 0xff;
                Operand2 = _lrotr(Operand2, Shift);
                break;
        }
    }

    // Determine the result (the new PC), based on the opcode.
    switch ((Opcode >> 21) & 0xf)
    {
        case 0: // AND
            Register[(Opcode >> 12) & 0xf] = Operand1 & Operand2;
            break;

        case 1: // EOR
            Register[(Opcode >> 12) & 0xf] = Operand1 ^ Operand2;
            break;

        case 2: // SUB
            Register[(Opcode >> 12) & 0xf] = Operand1 + ~Operand2 + 1;
            break;

        case 3: // RSB
            Register[(Opcode >> 12) & 0xf] = Operand2 + ~Operand1 + 1;
            break;

        case 4: // ADD
            Register[(Opcode >> 12) & 0xf] = Operand1 + Operand2;
            break;

        case 5: // ADC
            Register[(Opcode >> 12) & 0xf] = Operand1 + Operand2 + Cflag;
            break;

        case 6: // SBC
            Register[(Opcode >> 12) & 0xf] = Operand1 + ~Operand2 + Cflag;
            break;

        case 7: // RSC
            Register[(Opcode >> 12) & 0xf] = Operand2 + ~Operand1 + Cflag;
            break;

        case 0xc: // ORR
            Register[(Opcode >> 12) & 0xf] = Operand1 | Operand2;
            break;

        case 0xd: // MOV
            Register[(Opcode >> 12) & 0xf] = Operand2;
            break;

        case 0xe: // BIC
            Register[(Opcode >> 12) & 0xf] = Operand1 & ~Operand2;
            break;

        case 0xf: // MVN
            Register[(Opcode >> 12) & 0xf] = ~Operand2;
            break;

        case 8: // TST
        case 9: // TEQ
        case 0xa: // CMP
        case 0xb: // CMN
            // These instructions do not have a destination register.
            // There is nothing to do.
            break;

        default:
            ASSERT(UNREACHED);
    }
}


static
BOOL
ArmRecoverRegisterValue(
    __in_bcount(StartingPc - PrologPc) ULONG const *    Prolog,
    __in ULONG                                          PrologPc,
    __in ULONG                                          StartingPc,
    __in ULONG                                          RegIndex,
    __out ULONG *                                       RegValue,
    __in PROCESSID_T                                    ProcessId
    )

/*++

Routine Description:

    Recover the value of a register that is loaded with a constant
    in the ARM prologue.

Arguments:

    Prolog - Supplies a pointer to a copy of the prologue opcodes.

    PrologPc - Supplies the address where the original prologue opcodes
        were located.

    StartingPc - Supplies the address where the unwinder is currently
        executing.

    Register - Supplies the index of the register we are interested in.

    RegValue - Supplies a pointer where the new register value will be
        stored.

Return Value:

    TRUE if all bits of the register were accounted for; FALSE otherwise.

--*/

{
    ULONG Address = 0;
    ULONG CurrentPc = 0;
    ULONG InstructionIndex = 0;
    ULONG Opcode = 0;
    ULONG Immediate = 0;
    BOOL HighBitsValid = FALSE;

    //
    // Work backward from the current instruction updating the register
    // value based on instructions we know are used to set up the constant.
    //

    ASSERT(RegIndex < 16);

    for (CurrentPc = StartingPc - 4; CurrentPc >= PrologPc; CurrentPc -= 4)
    {
        InstructionIndex = (CurrentPc - PrologPc) / 4;

        Opcode = Prolog[InstructionIndex];

        if (((Opcode & 0xff7f0000) == 0xe51f0000) &&
            (((Opcode >> 12) & 0xf) == RegIndex))
        {
            //
            // LDR  Rt,[PC,#imm]
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //   | cond  |0 1 0|P|U|0|W|1|  Rn   |  Rt   |        imm12          |
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //
            // Where Rt == RegIndex, Rn == PC(15), P == 1, and W == 0.
            //
            // Load from Literal Pool into register. This instruction is
            // used in the prologue for functions with large stack frames.
            // The stack frame size is loaded from the literal pool before
            // being used to update the stack pointer.
            //
            // Compute the PC-relative address and restore R12 with its value.
            //

            Address = CurrentPc + 8;

            if (((Opcode >> 23) & 1) != 0)
            {
                Address += Opcode & 0xfff;
            }
            else
            {
                Address -= Opcode & 0xfff;
            }

            *RegValue = READ_CODE_ULONG(ProcessId, Address);
            return TRUE;
        }
        else if (((Opcode & 0xfff00000) == 0xe3000000) &&
                 (((Opcode >> 12) & 0xf) == RegIndex))
        {
            //
            // MOVW Rd,#imm16
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-------+-------+-------+-------+-----------------------+
            //   | cond  |0 0 1 1|0 0 0 0| imm4  |  Rd   |         imm12         |
            //   +-------+-------+-------+-------+-------+-----------------------+
            //
            // If the stack linking is not doable in one instruction, then
            // the compiler generates a constant into a GPR (usually R12).
            // The compiler can use a MOVW to generate the constant, or its
            // lower 16bits.
            //

            Immediate = ((Opcode >> 4) & 0xf000) | (Opcode & 0xfff);

            if (HighBitsValid)
            {
                *RegValue |= Immediate;
            }
            else
            {
                *RegValue = Immediate;
            }

            return TRUE;
        }
        else if (((Opcode & 0xfff00000) == 0xe3400000) &&
                 (((Opcode >> 12) & 0xf) == RegIndex))
        {
            //
            // MOVT Rd,#imm16
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-------+-------+-------+-------+-----------------------+
            //   | cond  |0 0 1 1|0 1 0 0| imm4  |  Rd   |         imm12         |
            //   +-------+-------+-------+-------+-------+-----------------------+
            //
            // If the stack linking is not doable in one instruction, then
            // the compiler generates a constant into a GPR (usually R12).
            // The compiler can use a MOVT to generate the upper 16 bits of
            // the constant.  MOVT should be preceded by a MOVW.
            //

            Immediate = ((Opcode >> 4) & 0xf000) | (Opcode & 0xfff);
            *RegValue = Immediate << 16;
            HighBitsValid = TRUE;
        }
    }

    *RegValue = 0;

    return FALSE;
}


static
BOOL
ArmIgnoreInstruction(
    __in ULONG Opcode
    )

/*++

Routine Description:

    This function determines whether an ARM prologue instruction is expected
    but should have no effect on the unwind.

Arguments:

    Opcode - Supplies the opcode for the instruction.

Return Value:

    If the instruction should be ignored, this function returns TRUE, and
    it returns FALSE otherwise.

--*/

{
    //
    // TODO: Initialize Ignore to FALSE once we have identified all
    // instructions that the compiler might need to place in the prologue.
    //

    BOOL Ignore = TRUE;

    if ((Opcode & 0xff7f0000) == 0xe51f0000)
    {
        //
        // LDR  Rt,[PC,#imm]
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
        //   | cond  |0 1 0|P|U|0|W|1|  Rn   |  Rt   |        imm12          |
        //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
        //
        // Load a large immediate value.  This instruction is interpreted by
        // ArmRecoverRegisterValue.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xffb00000) == 0xe3000000)
    {
        //
        // MOVW Rd,#imm16
        // MOVT Rd,#imm16
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
        //   | cond  |0 0 1 1|0 t 0 0| imm4  |  Rd   |        imm12          |
        //   +-------+-------+-------+-------+-------+-----------------------+
        //
        // Load a large immediate value.  These instructions are interpreted by
        // ArmRecoverRegisterValue.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xfff00000) == 0xe3c00000)
    {
        //
        // BIC  Rd,Rn,#const
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+---+-+-------+-+-------+-------+-----------------------+
        //   | cond  |1 0|1|1 1 1 0|S|  Rn   |  Rd   |        imm12          |
        //   +-------+---+-+-+-----+-+-------+-------+-----------------------+
        //
        // This instruction could be used to align a pointer value such as a
        // frame pointer.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xff000000) == 0xeb000000)
    {
        //
        // BL   __chkstk
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+-------+-----------------------------------------------+
        //   | cond  |1 0 1 1|                    imm24                      |
        //   +-------+-------+-----------------------------------------------+
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xfdff0000) == 0xe1a00000)
    {
        //
        // MOV  Rd,...
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+---+-+-------+-+-------+-------+-----------------------+
        //   | cond  |0 0|x|1 1 0 1|S|0 0 0 0|  Rd   |    imm12/shift/Rm     |
        //   +-------+---+-+-------+-+-------+-------+-----------------------+
        //
        // All moves not handled by ArmUnwindProlog are ignored.  These include
        // LSL and LSR, which may be used to align the stack pointer.
        //

        Ignore = TRUE;
    }
    else if (Opcode == 0xe320f000)
    {
        //
        // NOP
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+-------+-------+-------+-------+-------+---------------+
        //   | cond  |0 0 1 1|0 0 1 0|0 0 0 0|1 1 1 1|0 0 0 0|0 0 0 0 0 0 0 0|
        //   +-------+-------+-------+-------+-------+-------+---------------+
        //
        // NOPs may be present for code alignment or hot patch
        // purposes.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xfffff000) == 0xe24ca000)
    {
        //
        // SUB  R10,R12,#imm
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+---+-+-------+-+-------+-------+-----------------------+
        //   | cond  |0 0|1|0 0 1 0|S|  Rn   |  Rd   |        imm12          |
        //   +-------+---+-+-------+-+-------+-------+-----------------------+
        //
        // This instruction may be used to set up a parameter pointer in a
        // dynamically aligned frame.
        //

        Ignore = TRUE;
    }
    else if ((Opcode & 0xffeff000) == 0xe24bb000)
    {
        //
        // STR  R0,[R11,#offs]
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
        //   | cond  |0 1 0|P|U|0|W|0|  Rn   |  Rt   |         imm12         |
        //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
        //
        // This instruction may be incorrectly placed in the prologue by the
        // compiler.  Ignore for now.
        //

        Ignore = TRUE;
    }
    else if (((Opcode & 0xffe0f000) == 0xe280b000) ||
             ((Opcode & 0xffe0f000) == 0xe240b000) ||
             ((Opcode & 0xffeffff0) == 0xe1a0b000))
    {
        //
        // ADD  R11, X - stack allocation
        // SUB  R11, X
        // MOV  R11, X
        //
        //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //   +-------+---+-+-------+-+-------+-------+-----------------------+
        //   | cond  |0 0|1|opcode |S|  Rn   |  Rd   |        immediate      |
        //   +-------+---+-+-------+-+-------+-------+---------------+-------+
        //   | cond  |0 0|0|opcode |S|  Rn   |  Rd   |   shift/type  |  Rm   |
        //   +-------+---+-+-------+-+-------+-------+---------------+-------+
        //   | cond  |0 0|0|1 1 0 1|S|0 0 0 0|  Rd   |0 0 0 0 0 0 0 0|  Rm   |
        //   +-------+---+-+-------+-+-------+-------+---------------+-------+
        //
        // Where opcode == 2 (SUB) or 4 (ADD) and Rd == FP(11).
        //
        // All the supported instructions for establising the frame pointer were
        // handled by ArmUnwindProlog, any other forms are currently unsupported.
        //

        Ignore = FALSE;
    }

    return Ignore;
}


static
BOOL
ArmUnwindProlog(
    __in_bcount(PrologBytes) ULONG *    Prolog,
    __in ULONG                          PrologBytes,
    __in ULONG                          PrologPc,
    __inout CONTEXT *                   ContextRecord,
    __in PROCESSID_T                    ProcessId
    )

/*++

Routine Description:

    This function unwinds the prologue code by executing it in the
    reverse direction.

Arguments:

    Prolog - Supplies a pointer to a copy of the prologue opcodes.

    PrologBytes - Specifies the number of bytes' worth of prologue
        to unwind.

    PrologPc - Supplies the address where the original prologue opcodes
        were located.

    ContextRecord - Supplies the address of a context record.

Return Value:

    If an unexpected intstruction is found, we return FALSE and stop
    executing. Otherwise, we return TRUE.

--*/

{
    ULONG CurrentPc = 0;
    ULONG * Register = &ContextRecord->R0;
    BOOL ExplicitPcFound = FALSE;

    //
    // Reverse execute the prologue instructions, starting with the last one.
    //
    // NOTE:  When unwinding the call stack, we assume that all instructions
    // in the prologue have the condition code "Always execute."  This is not
    // necessarily true for the epilogue.
    //

    for (CurrentPc = PrologPc + PrologBytes - 4;
         CurrentPc >= PrologPc;
         CurrentPc -= 4)
    {
        ULONG Opcode = 0;
        ULONG RegIndex = 0;
        BOOL Succeeded = FALSE;

        Opcode = Prolog[(CurrentPc - PrologPc) / 4];

        if ((Opcode & 0xffff0fff) == 0xe1a0000d)
        {
            //
            // MOV  Rd,SP - save stack value
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //   | cond  |0 0|0|1 1 0 1|S|0 0 0 0|  Rd   |0 0 0 0 0 0 0 0|  Rm   |
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //
            // Where S == 0 and Rm == SP(13).
            //
            // Undo this instruction by restoring SP to the value of Rd.
            //

            Register[REG_SP] = Register[(Opcode >> 12) & 0xf];
        }
        else if (((Opcode & 0xfdfff000) == 0xe08dd000) ||
                 ((Opcode & 0xfdfff000) == 0xe04dd000))
        {
            //
            // ADD  SP,<shifter>
            // SUB  SP,<shifter>
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+---+-+-------+-+-------+-------+-----------------------+
            //   | cond  |0 0|1|opcode |S|  Rn   |  Rd   |        immediate      |
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //   | cond  |0 0|0|opcode |S|  Rn   |  Rd   |   shift/type  |  Rm   |
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //
            // Where opcode == 2 (SUB) or 4 (ADD), S == 0, and Rd == Rn == SP(13).
            //

            //
            // For a register second operand, scan backward to find the
            // instruction so that we have the correct value for Rm.
            //

            if ((Opcode & 0x02000ff0) == 0x00000000)
            {
                RegIndex = Opcode & 0xf;
                Succeeded = ArmRecoverRegisterValue(Prolog, PrologPc,
                    CurrentPc, RegIndex, &Register[RegIndex], ProcessId);
                EXPECT(Succeeded);
            }

            //
            // Change the subtract to an add, and execute the instruction. A simple
            // XOR of the opcode field with 6 will swap the two.
            //

            Opcode ^= 0x00c00000;

            ArmExecuteDataProcess(Opcode, Register);
        }
        else if (Opcode == 0xe92d8000)
        {
            //
            // STMDB SP!,{PC} - special instruction in fake prologue
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----------+-+-+-------+-------------------------------+
            //   | cond  |1 0 0 1 0 0|W|0|  Rn   |         register_mask         |
            //   +-------+-----------+-+-+-------+-------------------------------+
            //
            // Where Rn == SP(13), W == 1, and register_mask == 0x8000.
            //
            // This is an instruction that will not appear in a real prologue
            // (or probably anywhere else). It is used as a marker in a fake
            // prologue as described below.
            //
            // In the usual case, the link register gets set by a call
            // instruction so the PC value should point to the instruction
            // that sets the link register.  In an interrupt or exception
            // frame, the link register and PC value are independent.  By
            // convention, the fake prologue for these frames (see
            // CaptureContext in armtrap.s) contains the STMDB instruction
            // shown above to lead the unwinder to the faulting PC location.
            //

            ExplicitPcFound = TRUE;
            Register[REG_PC] = READ_DATA_ULONG(ProcessId, Register[REG_SP]);
            Register[REG_SP] += 4;
        }
        else if ((Opcode & 0xfe5f0000) == 0xe80d0000)
        {
            //
            // STM* SP(!),<registers> - store/push incoming registers onto the stack
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------------------------------+
            //   | cond  |1 0 0|P|U|0|W|0|  Rn   |         register_mask         |
            //   +-------+-----+-+-+-+-+-+-------+-------------------------------+
            //
            // Where Rn == SP(13).
            //
            // Convert this instruction to a load multiple, with inverted
            // increment/decrement and before/after bits.
            //

            //
            // We support all STMs for legacy reasons, but we really should only
            // ever see true PUSH instructions in the prologue.
            //
            // TODO: No evidence we have ever required support for more than
            // PUSH in the prologue.  Consider removing support for others and
            // merge this block with PUSH {PC} above.  Note that we do support
            // LDMDB in the epilogue.
            //

            EXPECT((Opcode & 0x01a00000) == 0x01200000);

            Opcode ^= PUSH_TO_POP_MASK;
            ExecuteLoadMultiple(Opcode, Register, ProcessId);
        }
        else if ((Opcode & 0xffff0fff) == 0xe52d0004)
        {
            //
            // PUSH {Rt}
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //   | cond  |0 1 0|P|U|0|W|0|  Rn   |  Rt   |         imm12         |
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //
            // Where Rn == SP(13), P == 1, U == 0, W == 1, and imm12 == 4.
            //
            // Store of the link register that updates the stack as a base
            // register must be reverse executed to restore Rt and SP
            // to their values on entry.
            //
            // Load Rt from the stack and increment SP.
            //
            RegIndex = (Opcode >> 12) & 0xf;

            Register[RegIndex] = READ_DATA_ULONG(ProcessId, Register[REG_SP]);
            Register[REG_SP] += 4;
        }
        else if (((Opcode & 0xfdfff000) == 0xe08db000) ||
                 ((Opcode & 0xfdfff000) == 0xe04db000))
        {
            //
            // ADD  R11,SP,<shifter>
            // SUB  R11,SP,<shifter>
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+---+-+-------+-+-------+-------+-----------------------+
            //   | cond  |0 0|1|opcode |S|  Rn   |  Rd   |        immediate      |
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //   | cond  |0 0|0|opcode |S|  Rn   |  Rd   |   shift/type  |  Rm   |
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //
            // Where opcode == 2 (SUB) or 4 (ADD), Rd == FP(11), Rn == SP(13), and
            // S == 0.
            //

            //
            // For a register second operand, scan backward to find the
            // instruction so that we have the correct value for Rm.
            //

            if ((Opcode & 0x02000ff0) == 0x00000000)
            {
                RegIndex = Opcode & 0xf;

                Succeeded = ArmRecoverRegisterValue(Prolog, PrologPc,
                    CurrentPc, RegIndex, &Register[RegIndex], ProcessId);
                EXPECT(Succeeded);
            }

            //
            // Swap between ADD and SUB, and swap Rn and Rd between SP and FP.
            //

            Opcode ^= 0x00c66000;
            ArmExecuteDataProcess(Opcode, Register);
        }
        else if ((Opcode & 0xffbf0f00) == 0xed2d0b00)
        {
            //
            // VPUSH <registers>
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
            //   | cond  |1 1 0|1|0|D|1|0|  Rn   |  Rd   |1 0 1 1|     imm8      |
            //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
            //
            // Where Rn == SP(13)
            //
            // Push one or more double-precision VFP registers onto the stack.
            // Reverse by inverting the Load, Increment and Before bits and
            // executing the corresponding VPOP.
            //

            Opcode ^= PUSH_TO_POP_MASK;
            ExecuteVPopDouble(Opcode, Register, &ContextRecord->S[0], ProcessId);
        }
        else if (!ArmIgnoreInstruction(Opcode))
        {
            //
            // Fail to unwind if we see anything we don't expect.
            //

            EXPECT(!"Unexpected prologue instruction");
            return FALSE;
        }
    }

    if (!ExplicitPcFound)
    {
        //
        // Copy LR to PC if an explicit PC "push" wasn't found, i.e. this wasn't an
        // exception frame.
        //

        Register[REG_PC] = ADJUST_NEXTPC(Register[REG_LR]);
    }

    return TRUE;
}


static
BOOL
ArmWalkEpilog(
    __in ULONG          PrologBeginAddress,
    __in ULONG          EpilogPc,
    __in ULONG          MaxEpilogBytes,
    __in CONTEXT *      ContextRecord,
    __in PROCESSID_T    ProcessId
    )

/*++

Routine Description:

    This function executes the given code in the forward direction, assuming
    it is an epilogue. If it is not an epilogue, then execution is aborted and
    an error is signaled.

Arguments:

    ControlPc - Supplies the address where execution should begin.

    MaxEpilogBytes - Supplies the maximum number of bytes of code to consider
        as part of the epilogue.

    ContextRecord - Supplies the address of a context record.

Return Value:

    If an unexpected intstruction is found, we assume it is not an epilogue,
    and return FALSE. If only epilogue-proper instructions are found and all
    were executed, we return TRUE.

--*/

{
    ULONG CurrentPc = 0;
    ULONG OriginalSp = 0;
    ULONG OriginalCondCode = 0;
    ULONG * Register = &ContextRecord->R0;

    ASSERT(MaxEpilogBytes <= (4 * MAX_EPILOG_INSTR));

    OriginalSp = Register[REG_SP];

    //
    // Check the condition code of the first instruction. If it won't be
    // executed, don't bother checking what type of instruction it is.
    // Note that all epilogue instructions must execute in a contiguous
    // block (no interleaved instructions) and all condition codes must
    // match.
    //

    OriginalCondCode = READ_CODE_ULONG(ProcessId, EpilogPc) & 0xf0000000;

    if (!ArmCheckConditionCode(Register[REG_CPSR], OriginalCondCode))
    {
        return FALSE;
    }

    //
    // Execute instructions in a loop, looking for specific epilogue
    // instructions. If we don't get a match, assume this isn't an epilogue and
    // return FALSE.
    //

    for (CurrentPc = EpilogPc;
         CurrentPc < (EpilogPc + MaxEpilogBytes);
         CurrentPc += 4)
    {
        ULONG Address = 0;
        ULONG Opcode = 0;
        ULONG RegIndex = 0;
        BRANCH_CLASS BranchClass = BranchUnknown;

        Opcode = READ_CODE_ULONG(ProcessId, CurrentPc);

        //
        // All epilogue condition codes must match.
        //

        if ((Opcode & 0xf0000000) != OriginalCondCode)
        {
            break;
        }

        if ((Opcode & 0x0de0f000) == 0x0080d000)
        {
            //
            // ADD  SP,<shifter> - stack unlink
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+---+-+-------+-+-------+-------+-----------------------+
            //   | cond  |0 0|R|0 1 0 0|S|  Rn   |  Rd   |        imm12          |
            //   +-------+---+-+-------+-+-------+-------+-----------------------+
            //
            // Where Rd == SP(13).
            //
            // Execute this instruction and continue to the next.
            //

            ArmExecuteDataProcess(Opcode, Register);
        }
        else if ((Opcode & 0x0ffffff0) == 0x012fff10)
        {
            //
            // BX   Rm
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+---+-+-------+-+-------+-------+-------+-------+-------+
            //   | cond  |0 0 0 1 0 0 1 0|1 1 1 1|1 1 1 1|1 1 1 1|0 0 0 1|  Rm   |
            //   +-------+---------------+-------+-------+-------+-------+-------+
            //
            // Return if this is a return or tail call.  Ignore if this is a
            // local branch or true indirect call.
            //

            RegIndex = (Opcode & 0xf);

            if (RegIndex != REG_LR)
            {
                BranchClass = ClassifyIndirectBranch(PrologBeginAddress,
                    Register[RegIndex], Register[REG_LR], ProcessId);

                if (BranchClass == BranchTailCall)
                {
                    //
                    // Assume this is an indirect tail call with the return
                    // address in LR (R14).
                    //

                    RegIndex = REG_LR;
                }
                else if (BranchClass != BranchReturn)
                {
                    //
                    // Assume this is not a return instruction and therefore
                    // not part of an epilogue.
                    //

                    break;
                }
            }

            Register[REG_PC] = Register[RegIndex];
            goto Succeed;
        }
        else if ((Opcode & 0x0fefffff) == 0x01a0f00e)
        {
            //
            // MOV  PC,LR
            // MOVS PC,LR
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+---+-+-------+-+-------+-------+---------------+-------+
            //   | cond  |0 0|0|1 1 0 1|S|0 0 0 0|  Rd   |0 0 0 0 0 0 0 0|  Rm   |
            //   +-------+---+-+-------+-+-------+-------+-------+-------+-------+
            //
            // Where Rd == PC(15) and Rm == LR(14).
            //
            // Return via the link register.
            //

            Register[REG_PC] = Register[REG_LR];
            goto Succeed;
        }
        else if ((Opcode & 0x0f000000) == 0x0a000000)
        {
            //
            // B    <label>
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-------+-----------------------------------------------+
            //   | cond  |1 0 1 0|                  immediate                    |
            //   +-------+-------+-----------------------------------------------+
            //
            // B is a tail call if it branches to the entrypoint of another
            // function.
            //

            Address = (Opcode & 0x00ffffff) << 2;

            if ((Address >> 25) != 0)
            {
                Address |= 0xfc000000;
            }

            Address += CurrentPc + 8;

            if (BranchIsLocal(PrologBeginAddress, Address, ProcessId))
            {
                //
                // Assume this is a branch within the current function and not
                // part of the epilogue.
                //

                break;
            }

            //
            // Assume this is a tail call.
            //

            Register[REG_PC] = Register[REG_LR];
            goto Succeed;
        }
        else if ((Opcode & 0x0e5f0000) == 0x081d0000)
        {
            //
            // LDM  SP,<registers> - load multiple registers
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------------------------------+
            //   | cond  |1 0 0|P|U|0|W|1|  Rn   |         register_list         |
            //   +-------+-----+-+-+-+-+-+-------+-------------------------------+
            //
            // Where Rn == SP(13).
            //
            // Note that LDM instructions that include SP in register_list are
            // deprecated by ARM.
            //
            // Execute this instruction.  If PC is included in register_list,
            // return success, as this is a return instruction.
            //

            ExecuteLoadMultiple(Opcode, Register, ProcessId);
            if ((Opcode & 0x00008000) != 0)
            {
                goto Succeed;
            }
        }
        else if ((Opcode & 0x0fff0000) == 0x091b0000)
        {
            //
            // LDMDB R11,<registers> - load multiple registers
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------------------------------+
            //   | cond  |1 0 0|P|U|0|W|1|  Rn   |         register_list         |
            //   +-------+-----+-+-+-+-+-+-------+-------------------------------+
            //
            // Where Rn == 11, P == 1, U == 0, and W == 0.
            //
            // This instruction was once used to return from functions that
            // allocate a frame pointer.  R11 is set up to point to the end of
            // the register save area, and all the registers, including SP, are
            // popped in one instruction.  The problem with this approach is that
            // it is not compatible with an LDM that is interrupted (possible
            // with ARMv6+).  Since SP is not Rn, it is not protected from update
            // in the case of interruption.  This can result in an interrupt
            // handler or other code that executes on the current stack corrupt-
            // ing memory that needs to be preserved in order to restart the LDM
            // instruction.  For this reason, the compiler no longer generates
            // this instruction.  However, it is supported for legacy applications
            // since it is technically possible for code to run correctly if
            // Low Latency Interrupt Mode is disabled such that LDMs cannot be
            // interrupted.
            //
            // Execute this instruction.  If PC is included in <registers>,
            // return success as this is a return instruction.
            //

            // TODO: EXPECT(UNREACHED);

            ExecuteLoadMultiple(Opcode, Register, ProcessId);
            if ((Opcode & 0x00008000) != 0)
            {
                goto Succeed;
            }
        }
        else if ((Opcode & 0x0fff0000) == 0x049d0000)
        {
            //
            // POP  {Rt}
            // LDR  Rt,[SP],#imm12
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //   | cond  |0 1 0|P|U|0|W|1|  Rn   |  Rt   |        imm12          |
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //
            // Where Rn == SP(13), P = 0, U = 1, and W = 0.
            //
            // Execute this instruction.  If PC is the popped register, return
            // success as this is a return instruction.
            //

            RegIndex = (Opcode >> 12) & 0xf;

            Register[RegIndex] = READ_DATA_ULONG(ProcessId, Register[REG_SP]);
            Register[REG_SP] += (Opcode & 0xfff);
            if (RegIndex == REG_PC)
            {
                goto Succeed;
            }
        }
        else if ((Opcode & 0x0ff0f000) == 0x0590f000)
        {
            //
            // LDR  PC,[Rn,#imm12]
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //   | cond  |0 1 0|P|U|0|W|1|  Rn   |  Rt   |        imm12          |
            //   +-------+-----+-+-+-+-+-+-------+-------+-----------------------+
            //
            // Where Rt == PC(15), P = 1, U = 1, and W = 0;
            //
            // If the branch type is a tail call, execute a return via the link
            // register (R14).
            //
            // Note that only non-negative offsets from the base register are
            // supported.
            //

            RegIndex = (Opcode >> 16) & 0xf;
            Address = READ_DATA_ULONG(ProcessId,
                Register[RegIndex] + (Opcode & 0xfff));

            BranchClass = ClassifyIndirectBranch(PrologBeginAddress,
                Address, Register[REG_LR], ProcessId);

            if (BranchClass == BranchTailCall)
            {
                //
                // This is a tail call with the return address in LR.
                //

                Register[REG_PC] = Register[REG_LR];
                goto Succeed;
            }

            //
            // Assume this instruction is not part of an epilogue.
            //

            break;
        }
        else if ((Opcode & 0xffbf0f00) == 0xecbd0b00)
        {
            //
            // VPOP <registers>
            //
            //   31 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
            //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
            //   |1 1 1 0|1 1 0|0|1|D|1|1|  Rn   |  Vd   |1 0 1 1|     imm8      |
            //   +-------+-----+-+-+-+-+-+-------+-------+-------+---------------+
            //
            // Where Rn == SP(13).
            //
            // Restore one or more VFP/NEON registers.
            //

            ExecuteVPopDouble(Opcode, Register, &ContextRecord->S[0], ProcessId);
        }
        else if (Opcode == 0xe320f000)
        {
            //
            // NOP
            //
            //   +-------+-------+-------+-------+-------+-------+---------------+
            //   | cond  |0 0 1 1|0 0 1 0|0 0 0 0|1 1 1 1|0 0 0 0|0 0 0 0 0 0 0 0|
            //   +-------+-------+-------+-------+-------+-------+---------------+
            //
            // Ignored.
            //
        }
        else
        {
            //
            // Assume we are not in an epilogue if we see anything else.
            //

            break;
        }
    }

    //
    // Fail.  Restore SP so the prologue unwind can proceed.
    //

    Register[REG_SP] = OriginalSp;
    return FALSE;

Succeed:

    //
    // Return address is in PC.
    //

    Register[REG_PC] = ADJUST_NEXTPC(Register[REG_PC]);
    return TRUE;
}


static
BOOL
ArmVirtualUnwind(
    __in ULONG                      ControlPc,
    __in RUNTIME_FUNCTION const *   FunctionEntry,
    __inout CONTEXT *               ContextRecord,
    __out BOOLEAN *                 InFunction,
    __in PROCESSID_T                ProcessId
    )

/*++

Routine Description:

    This function virtually unwinds the specified function by executing its
    prologue code backward or its epilogue code forward.

Arguments:

    ControlPc - Supplies the address where control left the specified
        function.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function.

    ContextRecord - Supplies the address of a context record.

    InFunction - Supplies a pointer to a variable that receives whether the
        control PC is within the current function.

Return Value:

    The address where control left the caller or the original ControlPc value
    if an error occurs.

--*/

{
    ULONG Prolog[MAX_PROLOG_INSTR];
    ULONG PrologBeginAddress = 0;
    ULONG PrologBytes = 0;
    ULONG PrologEndAddress = 0;
    ULONG * Register = NULL;
    BOOL Succeeded = FALSE;

    ASSERT((ControlPc & 1) == 0);
    ASSERT(FunctionEntry != NULL);
    ASSERT(ContextRecord != NULL);
    ASSERT(InFunction != NULL);

    *InFunction = TRUE;

#ifndef RUNTIME_FUNCTION_NO_IS32BIT_FLAG
    if ((FunctionEntry->BeginAddress & 1) == 1)
    {
        //
        // PDATA does not indicate ARM mode
        //

        goto Done;
    }
#endif

    PrologBeginAddress = FunctionEntry->BeginAddress;
    PrologEndAddress = FunctionEntry->PrologEndAddress;
    Register = &ContextRecord->R0;

    //
    // If no prologue, copy LR to PC and leave.
    //

    if (PrologEndAddress == PrologBeginAddress)
    {
        Register[REG_PC] = ADJUST_NEXTPC(Register[REG_LR]);
        Succeeded = TRUE;
        goto Done;
    }

    //
    // Determine the size of the prologue. Fail if the prologue is larger than
    // we can handle.
    //

    PrologBytes = PrologEndAddress - PrologBeginAddress;

    if (PrologBytes > sizeof(Prolog))
    {
        EXPECT(!"Too many prologue instructions");
        goto Done;
    }

    //
    // If we are in the prologue, then we shouldn't execute instructions past
    // our current location; otherwise, assume we are in the function proper.
    // Note that we must check against the beginning of the prologue as well
    // as the end, since a separated function may have a portion of its body
    // at a lower address than the prologue.
    //

    if ((ControlPc >= PrologBeginAddress) && (ControlPc < PrologEndAddress))
    {
        PrologBytes = ControlPc - PrologBeginAddress;
        *InFunction = FALSE;
    }
    else
    {
        //
        // Attempt to execute epilogue instructions forward.  If we fail,
        // assume we are in the function body and unwind the prologue.
        //

        Succeeded = ArmWalkEpilog(PrologBeginAddress, ControlPc,
            PrologBytes + (4 * PRO_EPILOG_DELTA), ContextRecord, ProcessId);
    }

    if (!Succeeded)
    {
        READ_CODE(ProcessId, Prolog, PrologBeginAddress, PrologBytes, FALSE);

        //
        // Execute the prologue instructions backwards, either from the end
        // or from where we left off.
        //

        Succeeded = ArmUnwindProlog(Prolog, PrologBytes,
            PrologBeginAddress, ContextRecord, ProcessId);
    }

Done:

    return Succeeded;
}


EXTERN_C
ULONG
RtlVirtualUnwind(
    __in ULONG              ControlPc,
    __in PRUNTIME_FUNCTION  FunctionEntry,
    __inout PCONTEXT        ContextRecord,
    __out PBOOLEAN          InFunction,
    __out PULONG            EstablisherFrame,
    __in PROCESSID_T        ProcessId
    )

/*++

Routine Description:

    This function virtually unwinds the specfified function by executing its
    prologue code backward (or its epilogue code forward).

    If the function is a leaf function, then the address where control left
    the previous frame is obtained from the context record. If the function
    is a nested function, but not an exception or interrupt frame, then the
    prologue code is executed backwards and the address where control left
    the previous frame is obtained from the updated context record.

    Otherwise, an exception or interrupt entry to the system is being unwound
    and a special instruction in the prologue indicates the fault instruction
    address.

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

Return Value:

    The address where control left the previous frame is returned as the
    function value.

--*/

{
    ULONG NextPc = ControlPc;
    ULONG OriginalPc = ContextRecord->Pc;
    ULONG OriginalSp = ContextRecord->Sp;
    BOOL Succeeded = FALSE;

    //
    //  This function is simply a passthrough which dispatches to the correct
    //  unwinder for the processor mode at the current unwind point.
    //

    BEGIN_PROCESS_ACCESS
    {
        if (UnwindMillicode(ControlPc, FunctionEntry, ContextRecord, ProcessId))
        {
            *InFunction = FALSE;

            Succeeded = TRUE;
        }
        else if ((ControlPc & 1) != 0)
        {
            //
            // Thumb mode
            //

            Succeeded = ThumbVirtualUnwind(ControlPc, FunctionEntry, ContextRecord,
                InFunction, ProcessId);
        }
        else
        {
            //
            // ARM mode
            //

            Succeeded = ArmVirtualUnwind(ControlPc, FunctionEntry, ContextRecord,
                InFunction, ProcessId);
        }
    }
    END_PROCESS_ACCESS

    if (Succeeded)
    {
        //
        // Check for no forward progress.  A better test would be that there are no
        // loops in the callstack, but that is beyond the scope of this
        // function.
        //

        if (((ControlPc & ~1) != (ContextRecord->Pc & ~1)) || (OriginalSp != ContextRecord->Sp))
        {
            //
            // Use the final SP as the establishing frame and the final PC as
            // the next frame's PC.
            //

            *EstablisherFrame = ContextRecord->Sp;
            NextPc = ContextRecord->Pc;

            //
            // Restore the original PC value because RtlUnwind expects that
            // it isn't modified.
            //

            ContextRecord->Pc = OriginalPc;

            //
            // Reset the status register per ABI call boundary requirements.
            //

            ResetExecutionState(ContextRecord);
        }
    }

    return NextPc;
}

