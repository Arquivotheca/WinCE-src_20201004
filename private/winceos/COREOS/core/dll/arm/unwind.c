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

    unwind.c

Abstract:

    This module implements the unwinding of ARM and Thumb procedure call
    frames for exception processing and general callstack walking.

    Note that this file is used by both the OS (when UNDER_CE is defined)
    and the debugger.  PLEASE KEEP THIS FILE AND ALL COPIES IN SYNCH.

--*/

#include <kernel.h>
#include <coredll.h>
#include <stdlib.h>

#define ARGUMENT_PRESENT(ArgumentPointer)   (ArgumentPointer)
typedef struct _KNONVOLATILE_CONTEXT_POINTERS {  
  
    PULONG R4;  
    PULONG R5;  
    PULONG R6;  
    PULONG R7;  
    PULONG R8;  
    PULONG R9;  
    PULONG R10;  
    PULONG R11;  
    PULONG Lr;  
  
    PULONGLONG D8;  
    PULONGLONG D9;  
    PULONGLONG D10;  
    PULONGLONG D11;  
    PULONGLONG D12;  
    PULONGLONG D13;  
    PULONGLONG D14;  
    PULONGLONG D15;  
  
} KNONVOLATILE_CONTEXT_POINTERS, *PKNONVOLATILE_CONTEXT_POINTERS;  
  

#ifndef INLINE
#define INLINE __inline
#endif

//
// Parameters describing the unwind codes.
//

#define STATUS_UNWIND_UNSUPPORTED_VERSION   STATUS_UNSUCCESSFUL
#define STATUS_UNWIND_NOT_IN_FUNCTION       STATUS_UNSUCCESSFUL

//
// Internal Microsoft-specific opcodes. (CE only use MSFT_OP_CONTEXT)
//

#define MSFT_OP_TRAP_FRAME_OLD      0
#define MSFT_OP_MACHINE_FRAME       1
#define MSFT_OP_CONTEXT             2
#define MSFT_OP_TRAP_FRAME          3
#define MSFT_OP_REDZONE_FRAME       4


//
// Macro for accessing an integer register by index.
//

#define CONTEXT_REGISTER(ctx, idx)    ((&(ctx)->R0)[idx])


//
// Macros for accessing memory. These can be overridden if other code
// (in particular the debugger) needs to use them.
//

#define MEMORY_READ_BYTE(params, addr)       (*(const BYTE *)(addr))
#define MEMORY_READ_DWORD(params, addr)      (*(const DWORD *)(addr))
#define MEMORY_READ_QWORD(params, addr)      (*(const UINT64 *)(addr))


//
// ARM_UNWIND_PARAMS definition. This is the kernel-specific definition,
// and contains information on the original PC, the stack bounds, and
// a pointer to the non-volatile context pointer array. Any usage of
// these fields must be wrapped in a macro so that the debugger can take
// a direct drop of this code and use it.
//

typedef struct _ARM_UNWIND_PARAMS
{
    ULONG           ControlPc;
    PULONG          LowLimit;
    PULONG          HighLimit;
    PKNONVOLATILE_CONTEXT_POINTERS ContextPointers;
} ARM_UNWIND_PARAMS, *PARM_UNWIND_PARAMS;

#define UNWIND_PARAMS_SET_TRAP_FRAME(Params, Address, Size)

#define UPDATE_CONTEXT_POINTERS(Params, RegisterNumber, Address)                \
do {                                                                            \
    PKNONVOLATILE_CONTEXT_POINTERS ContextPointers = (Params)->ContextPointers; \
    if (ARGUMENT_PRESENT(ContextPointers)) {                                    \
        if (RegisterNumber >=  4 && RegisterNumber <= 11) {                     \
            (&ContextPointers->R4)[RegisterNumber - 4] = (PULONG)Address;       \
        } else if (RegisterNumber == 14) {                                      \
            ContextPointers->Lr = (PULONG)Address;                              \
        }                                                                       \
    }                                                                           \
} while (0)

#define UPDATE_FP_CONTEXT_POINTERS(Params, RegisterNumber, Address)             \
do {                                                                            \
    PKNONVOLATILE_CONTEXT_POINTERS ContextPointers = (Params)->ContextPointers; \
    if (ARGUMENT_PRESENT(ContextPointers) &&                                    \
        (RegisterNumber >=  8) &&                                               \
        (RegisterNumber <= 15)) {                                               \
                                                                                \
        (&ContextPointers->D8)[RegisterNumber - 8] = (PULONGLONG)Address;       \
    }                                                                           \
} while (0)

#define VALIDATE_STACK_ADDRESS_EX(Params, Context, Address, DataSize, Alignment, OutStatus) \
do {                                                                            \
    if (ARGUMENT_PRESENT((Params)->LowLimit) &&                                 \
        (((ULONG)(Address) < *(Params)->LowLimit) ||                            \
         ((ULONG)(Address) > *(Params)->HighLimit - DataSize))) {               \
                                                                                \
        *OutStatus = STATUS_BAD_STACK;                                          \
    } else {                                                                    \
        *OutStatus = STATUS_SUCCESS;                                            \
    }                                                                           \
} while (0)

#define VALIDATE_STACK_ADDRESS(Params, Context, DataSize, Alignment, OutStatus) \
    VALIDATE_STACK_ADDRESS_EX(Params, Context, (Context)->Sp, DataSize, Alignment, OutStatus)


//
// This table describes the size of each unwind code, in bytes (lower nibble), 
// along with the size of the corresponding machine code, in halfwords
// (upper nibble).
//

static const BYTE UnwindOpTable[256] =
{
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,

    0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,  0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,
    0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,  0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,
    0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,  0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,
    0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,  0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
    0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,  0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21,
    0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21,  0x22,0x22,0x22,0x22,0x12,0x12,0x02,0x22,
    0x01,0x01,0x01,0x01,0x01,0x22,0x22,0x13,  0x14,0x23,0x24,0x11,0x21,0x10,0x20,0x00
};


//
// This table provides the register mask described by the given C/L/R/Reg bit
// combinations in the compact pdata format, along with the number of VFP
// registers to save in bits 16-19.
//

static const ULONG RegisterMaskLookup[1 << 6] =
{                // C L R Reg
    0x00010,     // 0 0 0 000
    0x00030,     // 0 0 0 001
    0x00070,     // 0 0 0 010
    0x000f0,     // 0 0 0 011
    0x001f0,     // 0 0 0 100
    0x003f0,     // 0 0 0 101
    0x007f0,     // 0 0 0 110
    0x00ff0,     // 0 0 0 111
    
    0x10000,     // 0 0 1 000
    0x20000,     // 0 0 1 001
    0x30000,     // 0 0 1 010
    0x40000,     // 0 0 1 011
    0x50000,     // 0 0 1 100
    0x60000,     // 0 0 1 101
    0x70000,     // 0 0 1 110
    0x00000,     // 0 0 1 111
    
    0x04010,     // 0 1 0 000
    0x04030,     // 0 1 0 001
    0x04070,     // 0 1 0 010
    0x040f0,     // 0 1 0 011
    0x041f0,     // 0 1 0 100
    0x043f0,     // 0 1 0 101
    0x047f0,     // 0 1 0 110
    0x04ff0,     // 0 1 0 111
    
    0x14000,     // 0 1 1 000
    0x24000,     // 0 1 1 001
    0x34000,     // 0 1 1 010
    0x44000,     // 0 1 1 011
    0x54000,     // 0 1 1 100
    0x64000,     // 0 1 1 101
    0x74000,     // 0 1 1 110
    0x04000,     // 0 1 1 111
    
    0x00810,     // 1 0 0 000
    0x00830,     // 1 0 0 001
    0x00870,     // 1 0 0 010
    0x008f0,     // 1 0 0 011
    0x009f0,     // 1 0 0 100
    0x00bf0,     // 1 0 0 101
    0x00ff0,     // 1 0 0 110
    0x0ffff,     // 1 0 0 111
    
    0x1ffff,     // 1 0 1 000
    0x2ffff,     // 1 0 1 001
    0x3ffff,     // 1 0 1 010
    0x4ffff,     // 1 0 1 011
    0x5ffff,     // 1 0 1 100
    0x6ffff,     // 1 0 1 101
    0x7ffff,     // 1 0 1 110
    0x0ffff,     // 1 0 1 111
    
    0x04810,     // 1 1 0 000
    0x04830,     // 1 1 0 001
    0x04870,     // 1 1 0 010
    0x048f0,     // 1 1 0 011
    0x049f0,     // 1 1 0 100
    0x04bf0,     // 1 1 0 101
    0x04ff0,     // 1 1 0 110
    0x0ffff,     // 1 1 0 111
    
    0x14800,     // 1 1 1 000
    0x24800,     // 1 1 1 001
    0x34800,     // 1 1 1 010
    0x44800,     // 1 1 1 011
    0x54800,     // 1 1 1 100
    0x64800,     // 1 1 1 101
    0x74800,     // 1 1 1 110
    0x04800      // 1 1 1 111
};
    

//
// The ConditionTable is used to look up the state of a condition
// based on the CPSR flags N,Z,C,V, which reside in the upper 4
// bits. To use this table, take the condition you are interested
// in and use it as the index to look up the UINT16 from the table.
// Then right-shift that value by the upper 4 bits of the CPSR,
// and the low bit will be the result.
//
// The bits in the CPSR are ordered (MSB to LSB): N,Z,C,V. Taken
// together, this is called the CpsrFlags.
//
// The macros below are defined such that:
//
//    N = (NSET_MASK >> CpsrFlags) & 1
//    Z = (ZSET_MASK >> CpsrFlags) & 1
//    C = (CSET_MASK >> CpsrFlags) & 1
//    V = (VSET_MASK >> CpsrFlags) & 1
//
// Also:
//
//    (N == V) = (NEQUALV_MASK >> CpsrFlags) & 1
//

#define NSET_MASK        (0xff00)
#define ZSET_MASK        (0xf0f0)
#define CSET_MASK        (0xcccc)
#define VSET_MASK        (0xaaaa)

#define NEQUALV_MASK     ((NSET_MASK & VSET_MASK) | (~NSET_MASK & ~VSET_MASK))

static const UINT16 ConditionTable[16] =
{
    ZSET_MASK,                     // EQ: Z
    ~ZSET_MASK,                    // NE: !Z
    CSET_MASK,                     // CS: C
    ~CSET_MASK,                    // CC: !C
    NSET_MASK,                     // MI: N
    ~NSET_MASK,                    // PL: !N
    VSET_MASK,                     // VS: V
    ~VSET_MASK,                    // VC: !V
    CSET_MASK & ~ZSET_MASK,        // HI: C & !Z
    ~CSET_MASK | ZSET_MASK,        // LO: !C | Z
    NEQUALV_MASK,                  // GE: N == V
    ~NEQUALV_MASK,                 // LT: N != V
    NEQUALV_MASK & ~ZSET_MASK,     // GT: (N == V) & !Z
    ~NEQUALV_MASK | ZSET_MASK,     // LE: (N != V) | Z
    0xffff,                        // AL: always
    0x0000                         // NV: never
};


//
// Prototypes for local functions:
//

NTSTATUS
RtlpxVirtualUnwind (
    __in ULONG ImageBase,
    __in ULONG ControlPc,
    __in PRUNTIME_FUNCTION FunctionEntry,
    __inout PCONTEXT ContextRecord,
    __out PVOID *HandlerData,
    __out PULONG EstablisherFrame,
    __inout_opt PKNONVOLATILE_CONTEXT_POINTERS ContextPointers,
    __in_opt PULONG LowLimit,
    __in_opt PULONG HighLimit,
    __deref_opt_out_opt PEXCEPTION_ROUTINE *HandlerRoutine
    );


FORCEINLINE
BOOLEAN
RtlpCheckCondition(
    __in PCONTEXT ContextRecord,
    __in ULONG Condition
    )

/*++

Routine Description:

    Checks the condition codes against the provided condition, and determines
    whether or not the instruction will be executed.

Arguments:

    ContextRecord - Supplies the address of a context record.

    Condition - The condition to test (only low 4 bits matter).

Return Value:

    TRUE if the condition is met; FALSE otherwise.

--*/

{
    return (ConditionTable[Condition & 0xf] >> (ContextRecord->Psr >> 28)) & 1;
}

FORCEINLINE
WORD
RtlpRangeToMask(
    __in ULONG Start,
    __in ULONG Stop,
    __in ULONG Lr
    )

/*++

Routine Description:

    Generate a register mask from a start/stop range, plus a flag
    indicating whether or not to include LR in the list.

Arguments:

    Start - Supplies the index of the first register in the range.

    Stop - Supplies the index of the last register in the range.
    
    Lr - Supplies a value which, if non-zero, indicates that the LR
        register is to be included in the mask.

Return Value:

    A WORD value containing a bitmask of the registers.

--*/

{
    WORD Result;
    
    Result = 0;
    if (Start <= Stop) {
        Result |= ((1 << (Stop + 1)) - 1) - ((1 << Start) - 1);
    }
    return Result | ((Lr != 0) ? (1 << 14) : 0);
}

NTSTATUS
RtlpPopRegisterMask(
    __inout PCONTEXT ContextRecord,
    __in WORD RegMask,
    __in PARM_UNWIND_PARAMS UnwindParams
    )

/*++

Routine Description:

    Pops a series of integer registers based on a provided register mask.

Arguments:

    ContextRecord - Supplies the address of a context record.

    RegMask - Specifies a 16-bit mask of registers to pop.

    UnwindParams - Additional parameters shared with caller.

Return Value:

    An NTSTATUS indicating either STATUS_SUCCESS if everything went ok, or
    another status code if there were problems.

--*/

{
    ULONG RegCount;
    ULONG RegIndex;
    NTSTATUS Status;
    
    //
    // Count and validate the number of registers.
    //
    
    RegCount = 0;
    for (RegIndex = 0; RegIndex < 15; RegIndex++) {
        RegCount += (RegMask >> RegIndex) & 1;
    }

    Status = STATUS_SUCCESS;
    VALIDATE_STACK_ADDRESS(UnwindParams, ContextRecord, 4 * RegCount, 4, &Status);
    if (Status != STATUS_SUCCESS) {
        return Status;
    }

    //
    // Then pop each register in sequence.
    //
    
    for (RegIndex = 0; RegIndex < 15; RegIndex++) {
        if ((RegMask & (1 << RegIndex)) != 0) {
            UPDATE_CONTEXT_POINTERS(UnwindParams, RegIndex, ContextRecord->Sp);
            CONTEXT_REGISTER(ContextRecord, RegIndex) = 
                    MEMORY_READ_DWORD(UnwindParams, ContextRecord->Sp);
            ContextRecord->Sp += 4;
        }
    }

    //
    // If we popped LR, move it to the PC.
    //

    if ((RegMask & 0x4000) != 0) {
        ContextRecord->Pc = ContextRecord->Lr;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS
RtlpPopVfpRegisterRange(
    __inout PCONTEXT ContextRecord,
    __in ULONG RegStart,
    __in ULONG RegStop,
    __in PARM_UNWIND_PARAMS UnwindParams
    )

/*++

Routine Description:

    Pops a series of floating-point registers in the provided inclusive range.

Arguments:

    ContextRecord - Supplies the address of a context record.

    RegStart - Specifies the index of the first register to pop.

    RegStop - Specifies the index of the final register to pop.

    UnwindParams - Additional parameters shared with caller.

Return Value:

    An NTSTATUS indicating either STATUS_SUCCESS if everything went ok, or
    another status code if there were problems.

--*/

{
    ULONG RegCount;
    ULONG RegIndex;
    NTSTATUS Status;
    
    //
    // Count and validate the number of registers.
    //
    
    RegCount = RegStop + 1 - RegStart;
    Status = STATUS_SUCCESS;
    VALIDATE_STACK_ADDRESS(UnwindParams, ContextRecord, 8 * RegCount, 4, &Status);
    if (Status != STATUS_SUCCESS) {
        return Status;
    }

    //
    // Then pop each register in sequence.
    //
    
    for (RegIndex = RegStart; RegIndex <= RegStop; RegIndex++) {
        UPDATE_FP_CONTEXT_POINTERS(UnwindParams, RegIndex, ContextRecord->Sp);
        ContextRecord->D[RegIndex] = MEMORY_READ_QWORD(UnwindParams, ContextRecord->Sp);
        ContextRecord->Sp += 8;
    }
    
    return STATUS_SUCCESS;
}

ULONG
RtlpComputeScopeSize(
    __in ULONG UnwindCodePtr,
    __in ULONG UnwindCodesEndPtr,
    __in BOOLEAN IsEpilog,
    __in PARM_UNWIND_PARAMS UnwindParams
    )

/*++

Routine Description:

    Computes the size of an prolog or epilog

Arguments:

    UnwindCodePtr - Supplies a pointer to the start of the unwind
        code sequence.
    
    UnwindCodesEndPtr - Supplies a pointer to the byte immediately
        following the unwind code table, as described by the header.
    
    IsEpilog - Specifies TRUE if the scope describes an epilog,
        or FALSE if it describes a prolog.

    UnwindParams - Additional parameters shared with caller.

Return Value:

    The size of the scope described by the unwind codes, in halfword units.

--*/

{
    ULONG ScopeSize;
    BYTE TableValue;
    BYTE Opcode;

    //
    // Iterate through the unwind codes until we hit an end marker.
    // While iterating, accumulate the total scope size.
    //
    
    ScopeSize = 0;
    Opcode = 0;
    while (UnwindCodePtr < UnwindCodesEndPtr && (Opcode = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr)) < 0xfd) {
        TableValue = UnwindOpTable[Opcode];
        ScopeSize += TableValue >> 4;
        UnwindCodePtr += TableValue & 0xf;
    }
    
    //
    // Handle the special epilog-only end codes.
    //
    
    if (Opcode >= 0xfd && Opcode <= 0xfe && IsEpilog) {
        ScopeSize += Opcode - 0xfc;
    }
    return ScopeSize;
}

NTSTATUS
RtlpUnwindCustom(
    __inout PCONTEXT ContextRecord,
    __in BYTE Opcode,
    __in PARM_UNWIND_PARAMS UnwindParams
    )

/*++

Routine Description:

    Handles custom unwinding operations involving machine-specific
    frames.

Arguments:

    ContextRecord - Supplies the address of a context record.

    Opcode - The opcode to decode.

    UnwindParams - Additional parameters shared with caller.

Return Value:

    An NTSTATUS indicating either STATUS_SUCCESS if everything went ok, or
    another status code if there were problems.

--*/

{
    ULONG RegIndex;
    ULONG SourceAddress;
    ULONG StartingSp;
    NTSTATUS Status;
    
    StartingSp = ContextRecord->Sp;
    Status = STATUS_SUCCESS;

    //
    // The opcode describes the special-case stack
    //

    switch (Opcode)
    {
    //
    // Context case
    //
    
    case MSFT_OP_CONTEXT:
    
        //
        // Ensure there is enough valid space for the full CONTEXT structure
        //
        
        VALIDATE_STACK_ADDRESS(UnwindParams, ContextRecord, sizeof(CONTEXT), 8, &Status);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        //
        // Restore R0-R11, and D0-D31
        //
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, R0);
        for (RegIndex = 0; RegIndex < 13; RegIndex++) {
            UPDATE_CONTEXT_POINTERS(UnwindParams, RegIndex, SourceAddress);
            CONTEXT_REGISTER(ContextRecord, RegIndex) = MEMORY_READ_DWORD(UnwindParams, SourceAddress);
            SourceAddress += sizeof(ULONG);
        }
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, D);
        for (RegIndex = 0; RegIndex < 32; RegIndex++) {
            UPDATE_FP_CONTEXT_POINTERS(UnwindParams, RegIndex, SourceAddress);
            ContextRecord->D[RegIndex] = MEMORY_READ_QWORD(UnwindParams, SourceAddress);
            SourceAddress += sizeof(ULONGLONG);
        }

        //
        // Restore SP, LR, PC, and the status registers
        //
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, Sp);
        ContextRecord->Sp = MEMORY_READ_DWORD(UnwindParams, SourceAddress);
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, Lr);
        ContextRecord->Lr = MEMORY_READ_DWORD(UnwindParams, SourceAddress);
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, Pc);
        ContextRecord->Pc = MEMORY_READ_DWORD(UnwindParams, SourceAddress);
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, Psr);
        ContextRecord->Psr = MEMORY_READ_DWORD(UnwindParams, SourceAddress);
        
        SourceAddress = StartingSp + FIELD_OFFSET(CONTEXT, Fpscr);
        ContextRecord->Fpscr = MEMORY_READ_DWORD(UnwindParams, SourceAddress);

        break;


    default:
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}


NTSTATUS 
RtlpUnwindFunctionCompact(
    __in ULONG ControlPcRva,
    __in PRUNTIME_FUNCTION FunctionEntry,
    __inout PCONTEXT ContextRecord,
    __out PULONG EstablisherFrame,
    __deref_opt_out_opt PEXCEPTION_ROUTINE *HandlerRoutine,
    __out PVOID *HandlerData,
    __in PARM_UNWIND_PARAMS UnwindParams
    )

/*++

Routine Description:

    This function virtually unwinds the specified function by parsing the
    compact .pdata record to determine where in the function the provided 
    ControlPc is, and then executing a standard, well-defined set of
    operations.

    If a context pointers record is specified (in the UnwindParams), then 
    the address where each nonvolatile register is restored from is recorded 
    in the appropriate element of the context pointers record.

Arguments:

    ControlPcRva - Supplies the address where control left the specified
        function, as an offset relative to the IamgeBase.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function. If appropriate, this should have already been
        probed.

    ContextRecord - Supplies the address of a context record.

    EstablisherFrame - Supplies a pointer to a variable that receives the
        the establisher frame pointer value.

    HandlerRoutine - Supplies an optional pointer to a variable that receives
        the handler routine address.  If control did not leave the specified
        function in either the prolog or an epilog and a handler of the
        proper type is associated with the function, then the address of the
        language specific exception handler is returned. Otherwise, NULL is
        returned.

    HandlerData - Supplies a pointer to a variable that receives a pointer
        the the language handler data.

    UnwindParams - Additional parameters shared with caller.

Return Value:

    STATUS_SUCCESS if the unwind could be completed, a failure status otherwise.
    Unwind can only fail when validation bounds are specified.

--*/

{
    ULONG CBit;
    ULONG ComputeFramePointerLength;
    ULONG CurrentOffset;
    ULONG EpilogLength;
    ULONG FunctionLength;
    ULONG HBit;
    ULONG OffsetInFunction;
    ULONG OffsetInScope;
    ULONG PopMask;
    ULONG PrologLength;
    ULONG PushMask;
    ULONG PushPopParamsLength;
    ULONG PushPopFloatingPointLength;
    ULONG PushPopIntegerLength;
    ULONG RetBits;
    ULONG ReturnLength;
    ULONG ScopeStart;
    ULONG StackAdjustLength;
    ULONG StackAdjust;
    NTSTATUS Status;
    ULONG UnwindData;
    ULONG VfpSaveCount;

    UnwindData = FunctionEntry->UnwindData;
    Status = STATUS_SUCCESS;
    
#if 0    // it's always unwound_to_call for CE
    // Compact records always describe an unwind to a call.
    //

    ContextRecord->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
#endif
    //
    // Extract the basic information about how to do a full unwind.
    //

    FunctionLength = (UnwindData >> 2) & 0x7ff;
    RetBits = (UnwindData >> 13) & 3;
    HBit = (UnwindData >> 15) & 1;
    CBit = (UnwindData >> 21) & 1;
    StackAdjust = (UnwindData >> 22) & 0x3ff;

    //
    // Determine push/pop masks based on this information. This comes
    // from a mix of the C, L, R, and Reg fields.
    //
    
    VfpSaveCount = RegisterMaskLookup[(UnwindData >> 16) & 0x3f];
    PushMask = PopMask = VfpSaveCount & 0xffff;
    VfpSaveCount >>= 16;
    
    //
    // Move LR->PC for the pop case if the Ret field is 0. This must be
    // accurate so that the opcode size computation below is correct.
    //
    
    if (RetBits == 0) {
        ASSERT((PopMask & 0x4000) != 0);
        PopMask = (PopMask & ~0x4000) | 0x8000;
    }
    
    //
    // If the stack adjustment is folded into the push/pop, encode this
    // by setting one of the low 4 bits of the push/pop mask and recovering
    // the actual stack adjustment.
    //

    if (StackAdjust >= 0x3f4) {
        PushMask |= StackAdjust & 4;
        PopMask |= StackAdjust & 8;
        StackAdjust = (StackAdjust & 3) + 1;
    }
    
    //
    // If we're near the start of the function (within 9 halfwords), 
    // see if we are within the prolog.
    //
    // N.B. If the low 2 bits of the UnwindData are 2, then we have
    // no prolog.
    //
    
    OffsetInFunction = (ControlPcRva - (FunctionEntry->BeginAddress & ~1)) / 2;
    OffsetInScope = 0;
    if (OffsetInFunction < 9 && (UnwindData & 3) != 2) {
    
        //
        // Compute sizes for each opcode in the prolog.
        //

        PushPopParamsLength = (HBit != 0) ? 1 : 0;
        PushPopIntegerLength = (PushMask == 0) ? 0 :
                               ((PushMask & 0xbf00) == 0) ? 1 : 2;
        ComputeFramePointerLength = (CBit == 0) ? 0 :
                                    ((PushMask & ~0x4800) == 0) ? 1 : 2;
        PushPopFloatingPointLength = (VfpSaveCount != 0) ? 2 : 0;
        StackAdjustLength = (StackAdjust == 0 || (PushMask & 4) != 0) ? 0 :
                            (StackAdjust < 0x80) ? 1 : 2;
    
        //
        // Compute the total prolog length and determine if we are within
        // its scope.
        //
        // N.B. We must execute prolog operations backwards to unwind, so
        // our final scope offset in this case is the distance from the end.
        //

        PrologLength = PushPopParamsLength +
                       PushPopIntegerLength +
                       ComputeFramePointerLength +
                       PushPopFloatingPointLength +
                       StackAdjustLength;

        if (OffsetInFunction < PrologLength) {
            OffsetInScope = PrologLength - OffsetInFunction;
        }
    }
    
    //
    // If we're near the end of the function (within 8 halfwords), see if 
    // we are within the epilog.
    //
    // N.B. If Ret == 3, then we have no epilog.
    //
    
    if (OffsetInScope == 0 && OffsetInFunction + 8 >= FunctionLength && RetBits != 3) {
    
        //
        // Compute sizes for each opcode in the epilog.
        //

        StackAdjustLength = (StackAdjust == 0 || (PopMask & 8) != 0) ? 0 :
                            (StackAdjust < 0x80) ? 1 : 2;
        PushPopFloatingPointLength = (VfpSaveCount != 0) ? 2 : 0;
        ComputeFramePointerLength = 0;
        PushPopIntegerLength = (PopMask == 0 || (HBit != 0 && RetBits == 0 && PopMask == 0x8000)) ? 0 :
                               ((PopMask & 0x7f00) == 0) ? 1 : 2;
        PushPopParamsLength = (HBit == 0) ? 0 : (RetBits == 0) ? 2 : 1;
        ReturnLength = RetBits;

        //
        // Compute the total epilog length and determine if we are within
        // its scope.
        //

        EpilogLength = StackAdjustLength +
                       PushPopFloatingPointLength +
                       PushPopIntegerLength +
                       PushPopParamsLength +
                       ReturnLength;

        ScopeStart = FunctionLength - EpilogLength;
        if (OffsetInFunction > ScopeStart) {
            OffsetInScope = OffsetInFunction - ScopeStart;
            PushMask = PopMask & 0x1fff;
            if (HBit == 0) {
                PushMask |= ((PopMask >> 1) & 0x4000);
            }
        }
    }
    
    //
    // Process operations backwards, in the order: stack deallocation,
    // VFP register popping, integer register popping, parameter home 
    // area recovery.
    //
    // First case is simple: we process everything with no regard for
    // the current offset within the scope.
    //
    
    if (OffsetInScope == 0) {

        ContextRecord->Sp += 4 * StackAdjust;
        if (VfpSaveCount != 0) {
            Status = RtlpPopVfpRegisterRange(ContextRecord, 8, 8 + VfpSaveCount - 1, UnwindParams);
        }
        PushMask &= 0xfff0;
        if (PushMask != 0) {
            Status = RtlpPopRegisterMask(ContextRecord, (WORD)PushMask, UnwindParams);
        }
        if (HBit != 0) {
            ContextRecord->Sp += 4 * 4;
        }
    }
    
    //
    // Second case is more complex: we must step along each operation
    // to ensure it should be executed.
    //
    
    else {

        CurrentOffset = 0;
        if (CurrentOffset >= OffsetInScope && StackAdjustLength != 0) {
            ContextRecord->Sp += 4 * StackAdjust;
        }
        CurrentOffset += StackAdjustLength;

        if (CurrentOffset >= OffsetInScope && PushPopFloatingPointLength != 0) {
            Status = RtlpPopVfpRegisterRange(ContextRecord, 8, 8 + VfpSaveCount - 1, UnwindParams);
        }
        CurrentOffset += PushPopFloatingPointLength;

        //
        // N.B. We don't need to undo any side effects of frame pointer linkage
        //
        
        CurrentOffset += ComputeFramePointerLength;
        
        //
        // N.B. In the epilog case above, we copied PopMask to PushMask
        //
        
        if (CurrentOffset >= OffsetInScope && PushPopIntegerLength != 0) {
            if (StackAdjustLength == 0) {
                ContextRecord->Sp += 4 * StackAdjust;
            }
            PushMask &= 0xfff0;
            Status = RtlpPopRegisterMask(ContextRecord, (WORD)PushMask, UnwindParams);
        }
        CurrentOffset += PushPopIntegerLength;
        
        //
        // N.B. In the epilog case, we also need to pop the return address
        //
        
        if (CurrentOffset >= OffsetInScope && PushPopParamsLength != 0) {
            if (PushPopParamsLength == 2) {
                Status = RtlpPopRegisterMask(ContextRecord, 1 << 14, UnwindParams);
            }
            ContextRecord->Sp += 4 * 4;
        }
    }

    //
    // If we succeeded, post-process the results a bit
    //
    
    if (Status == STATUS_SUCCESS) {

        //
        // Since we always POP to the LR, recover the final PC from there.
        // Also set the establisher frame equal to the final stack pointer.
        //
        
        ContextRecord->Pc = ContextRecord->Lr;
        *EstablisherFrame = ContextRecord->Sp;

        if (ARGUMENT_PRESENT(HandlerRoutine)) {
            *HandlerRoutine = NULL;
        }
        *HandlerData = NULL;
    }

    return Status;
}

NTSTATUS
RtlpUnwindFunctionFull(
    __in ULONG ControlPcRva,
    __in ULONG ImageBase,
    __in PRUNTIME_FUNCTION FunctionEntry,
    __inout PCONTEXT ContextRecord,
    __out PULONG EstablisherFrame,
    __deref_opt_out_opt PEXCEPTION_ROUTINE *HandlerRoutine,
    __out PVOID *HandlerData,
    __in PARM_UNWIND_PARAMS UnwindParams
    )

/*++

Routine Description:

    This function virtually unwinds the specified function by parsing the
    .xdata record to determine where in the function the provided ControlPc
    is, and then executing unwind codes that map to the function's prolog
    or epilog behavior.

    If a context pointers record is specified (in the UnwindParams), then 
    the address where each nonvolatile register is restored from is recorded 
    in the appropriate element of the context pointers record.

Arguments:

    ControlPcRva - Supplies the address where control left the specified
        function, as an offset relative to the IamgeBase.

    ImageBase - Supplies the base address of the image that contains the
        function being unwound.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function. If appropriate, this should have already been
        probed.

    ContextRecord - Supplies the address of a context record.

    EstablisherFrame - Supplies a pointer to a variable that receives the
        the establisher frame pointer value.

    HandlerRoutine - Supplies an optional pointer to a variable that receives
        the handler routine address.  If control did not leave the specified
        function in either the prolog or an epilog and a handler of the
        proper type is associated with the function, then the address of the
        language specific exception handler is returned. Otherwise, NULL is
        returned.

    HandlerData - Supplies a pointer to a variable that receives a pointer
        the the language handler data.

    UnwindParams - Additional parameters shared with caller.

Return Value:

    STATUS_SUCCESS if the unwind could be completed, a failure status otherwise.
    Unwind can only fail when validation bounds are specified.

--*/

{
    ULONG CurCode;
    ULONG EpilogScopeCount;
    PEXCEPTION_ROUTINE ExceptionHandler;
    PVOID ExceptionHandlerData;
    BOOLEAN FinalPcFromLr;
    ULONG FunctionLength;
    ULONG HeaderWord;
    ULONG OffsetInFunction;
    ULONG Param;
    ULONG ScopeNum;
    ULONG ScopeSize;
    ULONG ScopeStart;
    LONG SkipHalfwords;
    NTSTATUS Status;
    BYTE TableValue;
    ULONG UnwindCodePtr;
    ULONG UnwindCodesEndPtr;
    ULONG UnwindDataPtr;
    ULONG UnwindIndex;
    ULONG UnwindWords;

#if 0    // it's always unwound_to_call for CE
    //
    // Unless a special frame is enountered, assume that any unwinding
    // will return us to the return address of a call and set the flag
    // appropriately (it will be cleared again if the special cases apply).
    //

    ContextRecord->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
#endif 
    //
    // By default, unwinding is done by popping to the LR, then copying
    // that LR to the PC. However, some special opcodes require different
    // behavior.
    //
    
    FinalPcFromLr = TRUE;

    //
    // Fetch the header word from the .xdata blob
    //
    
    UnwindDataPtr = ImageBase + FunctionEntry->UnwindData;
    HeaderWord = MEMORY_READ_DWORD(UnwindParams, UnwindDataPtr);
    UnwindDataPtr += 4;
    
    //
    // Verify the version before we do anything else
    //
    
    if (((HeaderWord >> 18) & 3) != 0) {
        return STATUS_UNWIND_UNSUPPORTED_VERSION;
    }

    FunctionLength = HeaderWord & 0x3ffff;
    OffsetInFunction = (ControlPcRva - (FunctionEntry->BeginAddress & ~1)) / 2;

    if (OffsetInFunction >= FunctionLength) {
        return STATUS_UNWIND_NOT_IN_FUNCTION;
    }
    
    //
    // Determine the number of epilog scope records and the maximum number
    // of unwind codes.
    //

    UnwindWords = (HeaderWord >> 28) & 15;
    EpilogScopeCount = (HeaderWord >> 23) & 31;
    if (EpilogScopeCount == 0 && UnwindWords == 0) {
        EpilogScopeCount = MEMORY_READ_DWORD(UnwindParams, UnwindDataPtr);
        UnwindDataPtr += 4;
        UnwindWords = (EpilogScopeCount >> 16) & 0xff;
        EpilogScopeCount &= 0xffff;
    }
    if ((HeaderWord & (1 << 21)) != 0) {
        UnwindIndex = EpilogScopeCount;
        EpilogScopeCount = 0;
    }
    
    //
    // If exception data is present, extract it now.
    //

    ExceptionHandler = NULL;
    ExceptionHandlerData = NULL;
    if ((HeaderWord & (1 << 20)) != 0) {
        ExceptionHandler = (PEXCEPTION_ROUTINE)(ImageBase + 
                        MEMORY_READ_DWORD(UnwindParams, UnwindDataPtr + 4 * (EpilogScopeCount + UnwindWords)));
        ExceptionHandlerData = (PVOID)(UnwindDataPtr + 4 * (EpilogScopeCount + UnwindWords + 1));
    }
    
    //
    // Unless we are in a prolog/epilog, we execute the unwind codes
    // that immediately follow the epilog scope list.
    //

    UnwindCodePtr = UnwindDataPtr + 4 * EpilogScopeCount;
    UnwindCodesEndPtr = UnwindCodePtr + 4 * UnwindWords;
    SkipHalfwords = 0;
    
    //
    // If we're near the start of the function, and this function has a prolog,
    // compute the size of the prolog from the unwind codes. If we're in the
    // midst of it, we still execute starting at unwind code index 0, but we may
    // need to skip some to account for partial execution of the prolog.
    //
    // N.B. As an optimization here, note that each byte of unwind codes can
    //      describe at most one 32-bit (2 halfwords) instruction. Thus, the
    //      largest prologue that could possibly be described by UnwindWords
    //      (which is 4 * the number of unwind code bytes) is 8 * UnwindWords
    //      halfwords. If OffsetInFunction is larger than this value, it is
    //      guaranteed to be in the body of the function.
    //

    if (OffsetInFunction < 8 * UnwindWords && ((HeaderWord & (1 << 22)) == 0)) {
        ScopeSize = RtlpComputeScopeSize(UnwindCodePtr, UnwindCodesEndPtr, FALSE, UnwindParams);

        if (OffsetInFunction < ScopeSize) {
            SkipHalfwords = ScopeSize - OffsetInFunction;
            ExceptionHandler = NULL;
            ExceptionHandlerData = NULL;
            goto ExecuteCodes;
        }
    }
    
    //
    // We're not in the prolog, now check to see if we are in the epilog.
    // In the simple case, the 'E' bit is set indicating there is a single
    // epilog that lives at the end of the function. If we're near the end
    // of the function, compute the actual size of the epilog from the
    // unwind codes. If we're in the midst of it, adjust the unwind code
    // pointer to the start of the codes and determine how many we need to skip.
    //
    // N.B. Similar to the prolog case above, the maximum number of halfwords
    //      that an epilog can cover is limited by UnwindWords. In the epilog
    //      case, however, the starting index within the unwind code table is
    //      non-zero, and so the maximum number of unwind codes that can pertain
    //      to an epilog is (UnwindWords * 4 - UnwindIndex), thus further
    //      constraining the bounds of the epilog.
    //
    
    if ((HeaderWord & (1 << 21)) != 0) {
        if (OffsetInFunction + (8 * UnwindWords - 2 * UnwindIndex) >= FunctionLength) {
            ScopeSize = RtlpComputeScopeSize(UnwindCodePtr + UnwindIndex, UnwindCodesEndPtr, TRUE, UnwindParams);
            ScopeStart = FunctionLength - ScopeSize;
            
            if (OffsetInFunction >= ScopeStart) {
                UnwindCodePtr += UnwindIndex;
                SkipHalfwords = OffsetInFunction - ScopeStart;
                ExceptionHandler = NULL;
                ExceptionHandlerData = NULL;
            }
        }
    }
    
    //
    // In the multiple-epilog case, we scan forward to see if we are within
    // shooting distance of any of the epilogs. If we are, we compute the
    // actual size of the epilog from the unwind codes and proceed like the
    // simple case above.
    //

    else {
        for (ScopeNum = 0; ScopeNum < EpilogScopeCount; ScopeNum++) {
            HeaderWord = MEMORY_READ_DWORD(UnwindParams, UnwindDataPtr);
            UnwindDataPtr += 4;

            //
            // The scope records are stored in order. If we hit a record that
            // starts after our current position, we must not be in an epilog.
            //

            ScopeStart = HeaderWord & 0x3ffff;
            if (OffsetInFunction < ScopeStart) {
                break;
            }
            
            UnwindIndex = HeaderWord >> 24;
            if (OffsetInFunction < ScopeStart + (8 * UnwindWords - 2 * UnwindIndex)) {
                ScopeSize = RtlpComputeScopeSize(UnwindCodePtr + UnwindIndex, UnwindCodesEndPtr, TRUE, UnwindParams);

                if (RtlpCheckCondition(ContextRecord, HeaderWord >> 20) && 
                    OffsetInFunction < ScopeStart + ScopeSize) {
                    
                    UnwindCodePtr += UnwindIndex;
                    SkipHalfwords = OffsetInFunction - ScopeStart;
                    ExceptionHandler = NULL;
                    ExceptionHandlerData = NULL;
                    break;
                }
            }
        }
    }

ExecuteCodes:

    //
    // Skip over unwind codes until we account for the number of halfwords
    // to skip.
    //

    while (UnwindCodePtr < UnwindCodesEndPtr && SkipHalfwords > 0) {
        CurCode = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
        if (CurCode >= 0xfd) {
            break;
        }
        TableValue = UnwindOpTable[CurCode];
        SkipHalfwords -= TableValue >> 4;
        UnwindCodePtr += TableValue & 0xf;
    }
    
    //
    // Now execute codes until we hit the end.
    //

    Status = STATUS_SUCCESS;
    while (UnwindCodePtr < UnwindCodesEndPtr && Status == STATUS_SUCCESS) {

        CurCode = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
        UnwindCodePtr++;
        
        //
        // 0x00-0x7f: 2-byte stack adjust ... add sp, sp, #0xval
        //
        
        if (CurCode < 0x80) {
            ContextRecord->Sp += (CurCode & 0x7f) * 4;
        }
        
        //
        // 0x80-0xbf: 4-byte bitmasked pop ... pop {r0-r12, lr}
        //
        
        else if (CurCode < 0xc0) {
            if (UnwindCodePtr >= UnwindCodesEndPtr) {
                Status = STATUS_BAD_FUNCTION_TABLE;
            } else {
                Param = ((CurCode & 0x20) << 9) |
                        ((CurCode & 0x1f) << 8) | 
                        MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                UnwindCodePtr++;
                Status = RtlpPopRegisterMask(ContextRecord, (WORD)Param, UnwindParams);
            }
        }
        
        //
        // 0xc0-0xcf: 2-byte stack restore ... mov sp, rX
        //
        
        else if (CurCode < 0xd0) {
            ContextRecord->Sp = CONTEXT_REGISTER(ContextRecord, CurCode & 0x0f);
        }
        
        else {
            switch (CurCode) {
            
            //
            // 0xd0-0xd7: 2-byte range pop ... pop {r4-r7, lr}
            //

            case 0xd0:  case 0xd1:  case 0xd2:  case 0xd3:
            case 0xd4:  case 0xd5:  case 0xd6:  case 0xd7:
                Status = RtlpPopRegisterMask(ContextRecord,
                                             RtlpRangeToMask(4, 4 + (CurCode & 3), CurCode & 4),
                                             UnwindParams);
                break;
            
            //
            // 0xd8-0xdf: 4-byte range pop ... pop {r4-r11, lr}
            //

            case 0xd8:  case 0xd9:  case 0xda:  case 0xdb:
            case 0xdc:  case 0xdd:  case 0xde:  case 0xdf:
                Status = RtlpPopRegisterMask(ContextRecord,
                                             RtlpRangeToMask(4, 8 + (CurCode & 3), CurCode & 4),
                                             UnwindParams);
                break;
            
            //
            // 0xe0-0xe7: 4-byte range vpop ... vpop {d8-d15}
            //

            case 0xe0:  case 0xe1:  case 0xe2:  case 0xe3:
            case 0xe4:  case 0xe5:  case 0xe6:  case 0xe7:
                Status = RtlpPopVfpRegisterRange(ContextRecord,
                                                 8, 8 + (CurCode & 0x07),
                                                 UnwindParams);
                break;
            
            //
            // 0xe8-0xeb: 4-byte stack adjust ... addw sp, sp, #0xval
            //

            case 0xe8:  case 0xe9:  case 0xea:  case 0xeb:
                if (UnwindCodePtr >= UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                ContextRecord->Sp += 4 * 256 * (CurCode & 3);
                ContextRecord->Sp += 4 * MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                UnwindCodePtr++;
                break;
            
            //
            // 0xec-0xed: 2-byte bitmasked pop ... pop {r0-r7,lr}
            //

            case 0xec:  case 0xed:
                if (UnwindCodePtr >= UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                Status = RtlpPopRegisterMask(ContextRecord,
                                             MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr) 
                                                    | ((CurCode << 14) & 0x4000), 
                                             UnwindParams);
                UnwindCodePtr++;
                break;
            
            
            //
            // 0xee: 0-byte custom opcode
            //

            case 0xee:
                if (UnwindCodePtr >= UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                Param = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                UnwindCodePtr++;
                if ((Param & 0xf0) == 0x00) {
                    Status = RtlpUnwindCustom(ContextRecord, 
                                              Param & 0x0f,
                                              UnwindParams);
                    FinalPcFromLr = FALSE;
                } else {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                }
                break;
            
            //
            // 0xef: 4-byte stack restore with post-increment ... ldr pc, [sp], #X
            //

            case 0xef:
                if (UnwindCodePtr >= UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                Param = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                UnwindCodePtr++;
                if ((Param & 0xf0) == 0x00) {
                    Status = RtlpPopRegisterMask(ContextRecord, 
                                                 0x4000,
                                                 UnwindParams);
                    ContextRecord->Sp += ((Param & 15) - 1) * 4;
                } else {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                }
                break;
            
            //
            // 0xf5: 4-byte range vpop ... vpop {d0-d15}
            //

            case 0xf5:
                if (UnwindCodePtr >= UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                Param = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                UnwindCodePtr++;
                Status = RtlpPopVfpRegisterRange(ContextRecord,
                                                 Param >> 4, Param & 0x0f,
                                                 UnwindParams);
                break;
            
            //
            // 0xf6: 4-byte range vpop ... vpop {d16-d31}
            //

            case 0xf6:
                if (UnwindCodePtr >= UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                Param = MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                UnwindCodePtr++;
                Status = RtlpPopVfpRegisterRange(ContextRecord,
                                                 16 + (Param >> 4), 16 + (Param & 0x0f),
                                                 UnwindParams);
                break;
            
            //
            // 0xf7: 2-byte stack adjust ... add sp, sp, <reg>
            // 0xf9: 4-byte stack adjust ... add sp, sp, <reg>
            //
            
            case 0xf7:
            case 0xf9:
                if (UnwindCodePtr + 2 > UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                ContextRecord->Sp += 4 * 256 * MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                ContextRecord->Sp += 4 * MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr + 1);
                UnwindCodePtr += 2;
                break;
            
            //
            // 0xf8: 2-byte stack adjust ... add sp, sp, <reg>
            // 0xfa: 4-byte stack adjust ... add sp, sp, <reg>
            //
            
            case 0xf8:
            case 0xfa:
                if (UnwindCodePtr + 3 > UnwindCodesEndPtr) {
                    Status = STATUS_BAD_FUNCTION_TABLE;
                    break;
                }
                ContextRecord->Sp += 4 * 256 * 256 * MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr);
                ContextRecord->Sp += 4 * 256 * MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr + 1);
                ContextRecord->Sp += 4 * MEMORY_READ_BYTE(UnwindParams, UnwindCodePtr + 2);
                UnwindCodePtr += 3;
                break;
            
            //
            // 0xfb: 2-byte no-op/misc instruction
            // 0xfc: 4-byte no-op/misc instruction
            //
            
            case 0xfb:
            case 0xfc:
                break;
            
            //
            // 0xfd: 2-byte end (epilog)
            // 0xfe: 4-byte end (epilog)
            // 0xff: generic end
            //
            
            case 0xfd:
            case 0xfe:
            case 0xff:
                goto finished;
            
            default:
                Status = STATUS_BAD_FUNCTION_TABLE;
                break;
            }
        }
    }

    //
    // If we succeeded, post-process the results a bit
    //
finished:
    if (Status == STATUS_SUCCESS) {

        //
        // Since we always POP to the LR, recover the final PC from there, unless
        // it was overwritten due to a special case custom unwinding operation.
        // Also set the establisher frame equal to the final stack pointer.
        //
        
        if (FinalPcFromLr) {
            ContextRecord->Pc = ContextRecord->Lr;
        }
        *EstablisherFrame = ContextRecord->Sp;

        if (ARGUMENT_PRESENT(HandlerRoutine)) {
            *HandlerRoutine = ExceptionHandler;
        }
        *HandlerData = ExceptionHandlerData;
    }
    
    return Status;
}


PEXCEPTION_ROUTINE
RtlVirtualUnwind (
    __in ULONG ImageBase,
    __in ULONG ControlPc,
    __in PRUNTIME_FUNCTION FunctionEntry,
    __inout PCONTEXT ContextRecord,
    __out PVOID *HandlerData,
    __out PULONG EstablisherFrame,
    __inout_opt PKNONVOLATILE_CONTEXT_POINTERS ContextPointers
    )

/*++

Routine Description:

    See RtlpxVirtualUnwind.

Arguments:

    See RtlpxVirtualUnwind.

Return Value:

    If control did not leave the specified function in either the prolog
    or an epilog and a handler of the proper type is associated with the
    function, then the address of the language specific exception handler
    is returned. Otherwise, NULL is returned.

    N.B. This value is not probed.

--*/

{

    PEXCEPTION_ROUTINE HandlerRoutine;
    NTSTATUS Status;

    Status = RtlpxVirtualUnwind(ImageBase,
                                ControlPc,
                                FunctionEntry,
                                ContextRecord,
                                HandlerData,
                                EstablisherFrame,
                                ContextPointers,
                                NULL,
                                NULL,
                                &HandlerRoutine);

    //
    // If we fail the unwind, clear the PC to 0. This is recognized by
    // many callers as a failure, given that RtlVirtualUnwind does not
    // return a status code.
    //

    if (!NT_SUCCESS(Status)) {
        ContextRecord->Pc = 0;
    }

    return HandlerRoutine;
}

FORCEINLINE
NTSTATUS
RtlpxVirtualUnwind (
    __in ULONG ImageBase,
    __in ULONG ControlPc,
    __in PRUNTIME_FUNCTION FunctionEntry,
    __inout PCONTEXT ContextRecord,
    __out PVOID *HandlerData,
    __out PULONG EstablisherFrame,
    __inout_opt PKNONVOLATILE_CONTEXT_POINTERS ContextPointers,
    __in_opt PULONG LowLimit,
    __in_opt PULONG HighLimit,
    __deref_opt_out_opt PEXCEPTION_ROUTINE *HandlerRoutine
    )

/*++

Routine Description:

    This function virtually unwinds the specified function by executing its
    prolog code backward or its epilog code forward.

    If a context pointers record is specified, then the address where each
    nonvolatile registers is restored from is recorded in the appropriate
    element of the context pointers record.

Arguments:

    HandlerType - Supplies the handler type expected for the virtual unwind.
        This may be either an exception or an unwind handler. A flag may
        optionally be supplied to indicate that the unwind should assume
        that the instruction at the PC is the one we are interested in
        (versus the PC being a return address).

    ImageBase - Supplies the base address of the image that contains the
        function being unwound.

    ControlPc - Supplies the address where control left the specified
        function.

    FunctionEntry - Supplies the address of the function table entry for the
        specified function. If appropriate, this should have already been
        probed.

    ContextRecord - Supplies the address of a context record.

    HandlerData - Supplies a pointer to a variable that receives a pointer
        the the language handler data.

    EstablisherFrame - Supplies a pointer to a variable that receives the
        the establisher frame pointer value.

    ContextPointers - Supplies an optional pointer to a context pointers
        record.

    LowLimit - Supplies an optional low limit used to bound the establisher
        frame. This must be supplied in conjunction with a high limit.

    HighLimit - Supplies an optional high limit used to bound the establisher
        frame. This must be supplied in conjunction with a low limit.

    HandlerRoutine - Supplies an optional pointer to a variable that receives
        the handler routine address.  If control did not leave the specified
        function in either the prolog or an epilog and a handler of the
        proper type is associated with the function, then the address of the
        language specific exception handler is returned. Otherwise, NULL is
        returned.

Return Value:

    STATUS_SUCCESS if the unwind could be completed, a failure status otherwise.
    Unwind can only fail when validation bounds are specified.

--*/

{
    NTSTATUS Status;
    ARM_UNWIND_PARAMS UnwindParams;
    
    //
    // Make sure out-of-bound stack accesses don't send us into an infinite
    // unwinding loop.
    //
    
    __try
    {

        //
        // Build an UnwindParams structure containing the starting PC, stack
        // limits, and context pointers.
        //

        UnwindParams.ControlPc = ControlPc & ~1;
        UnwindParams.LowLimit = LowLimit;
        UnwindParams.HighLimit = HighLimit;
        UnwindParams.ContextPointers = ContextPointers;

        //
        // Identify the compact .pdata format versus the full .pdata+.xdata format.
        //
        
        if ((FunctionEntry->UnwindData & 3) != 0) {

            Status = RtlpUnwindFunctionCompact(ControlPc - ImageBase,
                                               FunctionEntry,
                                               ContextRecord,
                                               EstablisherFrame,
                                               HandlerRoutine,
                                               HandlerData,
                                               &UnwindParams);
        } else {

            Status = RtlpUnwindFunctionFull(ControlPc - ImageBase,
                                            ImageBase,
                                            FunctionEntry,
                                            ContextRecord,
                                            EstablisherFrame,
                                            HandlerRoutine,
                                            HandlerData,
                                            &UnwindParams);
        }
    }
    
    //
    // If we do take an exception here, fetch the exception code as the status
    // and do not propagate the exception. Since the exception handler also
    // uses this function, propagating it will most likely generate the same
    // exception at the same point in the unwind, and continuing will typically
    // overflow the kernel stack.
    //

    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = GetExceptionCode();
    }
    
    return Status;
}

