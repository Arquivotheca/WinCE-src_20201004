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
/*++ BUILD Version: 0001    // Increment this if a change has global effects


Module Name:

    arminst.h

Abstract:

    ARM instruction definitions.

--*/

#ifndef _ARMINST_
#define _ARMINST_

//
// Define ARM instruction format structures.
//

#define COND_EQ         0x00000000L // Z set
#define COND_NE         0x10000000L // Z clear
#define COND_CS         0x20000000L // C set    // aka HS
#define COND_CC         0x30000000L // C clear  // aka LO
#define COND_MI         0x40000000L // N set
#define COND_PL         0x50000000L // N clear
#define COND_VS         0x60000000L // V set
#define COND_VC         0x70000000L // V clear
#define COND_HI         0x80000000L // C set and Z clear
#define COND_LS         0x90000000L // C clear or Z set
#define COND_GE         0xa0000000L // N == V
#define COND_LT         0xb0000000L // N != V
#define COND_GT         0xc0000000L // Z clear, and N == V
#define COND_LE         0xd0000000L // Z set, or N != V
#define COND_AL         0xe0000000L // Always execute
#define COND_NV         0xf0000000L // Never - undefined
#define COND_MASK       COND_NV

#define OP_AND  0x0 // 0000
#define OP_EOR  0x1 // 0001
#define OP_SUB  0x2 // 0010
#define OP_RSB  0x3 // 0011
#define OP_ADD  0x4 // 0100
#define OP_ADC  0x5 // 0101
#define OP_SBC  0x6 // 0110
#define OP_RSC  0x7 // 0111
#define OP_TST  0x8 // 1000
#define OP_TEQ  0x9 // 1001
#define OP_CMP  0xa // 1010
#define OP_CMN  0xb // 1011
#define OP_ORR  0xc // 1100
#define OP_MOV  0xd // 1101
#define OP_BIC  0xe // 1110
#define OP_MVN  0xf // 1111

#define MOV_PC_LR_MASK  0x0de0f00eL     // All types of mov - i, is, rs
#define MOV_PC_LR       0x01a0f00eL
#define MOV_PC_X_MASK   0x0de0f000L
#define MOV_PC_X        0x01a0f000L

#define DATA_PROC_MASK  0x0c00f000L
#define DP_PC_INSTR     0x0000f000L     // Data process instr w/PC as destination
#define DP_R11_INSTR    0x0000b000L     // Data process instr w/R11 as destination

#define ADD_SP_MASK     0x0de0f000L
#define ADD_SP_INSTR    0x0080d000L     // Add instr with SP as destination

#define SUB_SP_MASK     0x0de0f000L
#define SUB_SP_INSTR    0x0040d000L     // Sub instr with SP as destination

#define MOV_SP_MASK     0x0fff0fffL     // Register Mov instr with SP as source
#define MOV_SP_INSTR    0x01A0000dL

#define ADD_FP_SP_MASK  0x0feff000L
#define ADD_FP_SP_INSTR 0x028db000L     // ADD FP, SP, #immed

#define SUB_FP_SP_MASK  0x0feff000L
#define SUB_FP_SP_INSTR 0x024db000L     // SUB FP, SP, #immed

#define SUB_FP_FP_MASK  0x0feff000L
#define SUB_FP_FP_INSTR 0x024bb000L     // SUB FP, FP, #immed

#define ADD_FP_MASK     0x0de0f000L
#define ADD_FP_INSTR    0x0080b000L     // Add instr with FP as destination

#define SUB_FP_MASK     0x0de0f000L
#define SUB_FP_INSTR    0x0040b000L     // Sub instr with FP as destination

#define MOV_FP_MASK     0x0ffffff0L     // Register Mov instr with FP as DEST!
#define MOV_FP_INSTR    0x01A0b000L

#define BX_MASK         0x0ffffff0L     // Branch and exchange instr sets
#define BX_INSTR        0x012fff10L     // return (LR) or call (Rn != LR)

#define BLX2_MASK       0x0ffffff0L // Branch with Link and Exchange (ARMv5)
#define BLX2_INSTR      0x012fff30L // register op

#define LDM_PC_MASK     0x0e108000L
#define LDM_PC_INSTR    0x08108000L     // Load multiple with PC bit set

#define LDM_LR_MASK     0x0e104000L
#define LDM_LR_INSTR    0x08104000L     // Load multiple with LR bit set

#define STRI_LR_SPU_MASK    0x073de000L // Load or Store of LR with stack update
#define STRI_LR_SPU_INSTR   0x052de000L // Store LR (with immediate offset, update SP)

#define LDR_PC_MASK     0x0C50F000L
#define LDR_PC_RET_INSTR 0x0410F000L // Load PC Register from memory

#define STM_MASK        0x0e100000L
#define STM_INSTR       0x08000000L     // Store multiple instruction

#define STM_PC_MASK     0x0fff8000L
#define STM_PC_INSTR    0x092d8000L     // STMDB R13!, { pc }

#define FLDMD_SP_MASK   0x0fff0f00L
#define FLDMD_SP_INSTR  0x0cbd0b00L     // FLDMIA{D|X} SP!, { ... }

#define FSTMD_SP_MASK   0x0fff0f00L
#define FSTMD_SP_INSTR  0x0d2d0b00L     // FSTMDB{D|X} SP!, { ... }

#define B_BL_MASK       0x0f000000L     // Regular branches
#define B_INSTR         0x0a000000L
#define BL_INSTR        0x0b000000L

#define LDR_MASK        0x0f3ff000L
#define LDR_PC_INSTR    0x051fc000L     // Load an address from PC + offset to R12
#define LDR_THUNK_1     0xe59fc000L     // ldr r12, [pc]
#define LDR_THUNK_2     0xe59cf000L     // ldr pc, [r12]

#define ARM_MRS_MASK    0xffff0fffL
#define ARM_MRS_INSTR   0xE10f0000L     // mrs rd, cpsr

typedef union _ARMI {

    // This is incomplete.  There are other instruction forms, I just
    // don't need to check for them in the dm or imagehlp.

    struct {
        ULONG operand2  : 12;
        ULONG rd        : 4;
        ULONG rn        : 4;
        ULONG s         : 1;
        ULONG opcode    : 4;
        ULONG bits      : 3; // Specifies immediate (001) or register (000)
        ULONG cond      : 4;
    } dataproc; // Data processing, PSR transfer

    struct {
        //
        // Type 1 - Immediate
        //
        ULONG immediate : 8;
        ULONG rotate    : 4;
        ULONG dpbits    : 20;
    } dpi;

    struct {
        //
        //  Form: Shift or rotate by immediate
        //
        //  Type    bits    Name
        //
        //  2       (000)   Register (Shift is 0)
        //  3       (000)   Logical shift left by immediate
        //  5       (010)   Logical shift right by immediate
        //  7       (100)   Arithmetic shift right by immediate
        //  9       (110)   Rotate right by immediate
        //
        ULONG rm        : 4;
        ULONG bits      : 3;
        ULONG shift     : 5;
        ULONG dpbits    : 20;
    } dpshi;

    struct {
        //
        //  Form: Shift or rotate by register
        //
        //  Type    bits    Name
        //  4       (0001)  Logical shift left by register
        //  6       (0011)  Logical shift right by register
        //  8       (0101)  Arithmetic shift right by register
        //  10      (0111)  Rotate right by register
        //
        ULONG rm        : 4;
        ULONG bits      : 4;
        ULONG rs        : 4;
        ULONG dpbits    : 20;
    } dpshr;

    struct {
        //
        //  Type 11 - Rotate right with extended
        //
        ULONG rm        : 4;
        ULONG bits      : 8;    // (00000110)
        ULONG dpbits    : 20;
    } dprre;

    struct {
        ULONG rn        : 4;
        ULONG bits      : 24;
        ULONG cond      : 4;
    } bx;  // Branch and exchange instruction sets

    struct {
        ULONG rm        : 4; // Offset register
        ULONG reserved  : 1; // ZERO!
        ULONG shift     : 2; // Type of shift
        ULONG shift_imm : 5; // Value to shift by
        ULONG rd        : 4; // Destination register
        ULONG rn        : 4; // Base register
        ULONG l         : 1; // {
        ULONG w         : 1; // :
        ULONG b         : 1; // :
        ULONG u         : 1; // same as ldr, below
        ULONG p         : 1; // :
        ULONG i         : 1; // :
        ULONG bits      : 2; // :
        ULONG cond      : 4; // }
    } ldrreg;

    struct {
        ULONG offset    : 12;
        ULONG rd        : 4;
        ULONG rn        : 4;
        ULONG l         : 1;    // load = 1, store = 0
        ULONG w         : 1;    // update base register bit
        ULONG b         : 1;    // unsigned byte = 1, word = 0;
        ULONG u         : 1;    // increment = 1, decrement = 0
        ULONG p         : 1;    // pre-indexing = 1, post-indexing = 0
        ULONG i         : 1;    // immediate = 1, register = 0
        ULONG bits      : 2;
        ULONG cond      : 4;
    } ldr;  // Load register

    struct {
        ULONG reglist   : 16;
        ULONG rn        : 4;
        ULONG l         : 1;    // load = 1, store = 0
        ULONG w         : 1;    // update base register after transfer
        ULONG s         : 1;
        ULONG u         : 1;    // increment = 1, decrement = 0
        ULONG p         : 1;    // before = 1, after = 0
        ULONG bits      : 3;
        ULONG cond      : 4;
    } ldm;    // Load multiple

    struct {
        ULONG offset    : 8;
        ULONG cpnum     : 4;
        ULONG fd        : 4;
        ULONG rn        : 4;
        ULONG l         : 1;    // load = 1, store = 0
        ULONG w         : 1;    // update base register after transfer
        ULONG d         : 1;
        ULONG u         : 1;    // increment = 1, decrement = 0
        ULONG p         : 1;
        ULONG bits      : 3;
        ULONG cond      : 4;
    } fldm;   // VFP Load multiple
    
    struct {
        ULONG offset    : 24;
        ULONG link      : 1;
        ULONG bits      : 3;
        ULONG cond      : 4;
    } bl;   // Branch, Branch and link

    struct {
        ULONG sbz       : 12;
        ULONG rd        : 4;
        ULONG sbo       : 4;
        ULONG bits      : 8;        // 0x10 = cpsr, 0x14 = spsr
        ULONG cond      : 4;
    } mrs;

    ULONG instruction;

} ARMI, *PARMI;

//
// Define Thumb instruction format structures.
//

#define THM_DP_MASK     0xFC00              // Data Processing Instructions
#define THM_DP_INSTR    0x4000

#define THM_SPCDP_MASK  0xFC00              // Special Data Processing Inst
#define THM_SPCDP_INSTR 0x4400

#define THM_LDRPC_MASK  0xF800              // Load Register PC relative
#define THM_LDRPC_INSTR 0x4800

#define THM_BLPFX_MASK  0xF800              // BL instruction prefix
#define THM_BLPFX_INSTR 0xF000

#define THM_BL_MASK     0xF800              // BL instruction
#define THM_BL_INSTR    0xF800

#define THM_PUSH_MASK   0xFE00
#define THM_PUSH_INSTR  0xB400

#define THM_POP_MASK    0xFE00
#define THM_POP_INSTR   0xBC00

#define THM_ADDHI_MASK  0xFF00              // Add to High Register
#define THM_ADDHI_INSTR 0x4400

#define THM_MOVHI_MASK  0xFF00              // Move High Registers
#define THM_MOVHI_INSTR 0x4600

#define THM_ADJSP_MASK  0xFF00              // Adjust Stack Pointer
#define THM_ADJSP_INSTR 0xB000

#define THM_BX_MASK     0xFF80              // BX instruction
#define THM_BX_INSTR    0x4700

#define THM_NEG_MASK    0xFFC0              // Negate Instruction
#define THM_NEG_INSTR   0x4240

#define THM_ADDSP_MASK  0xFF87              // Add register to SP
#define THM_ADDSP_INSTR 0x4485

#define THM_BLX_MASK   0xF800               // First instruction will be a BL instruction prefix,
#define THM_BLX_INSTR  0xE800               // This will be the second instruction

#define THM_BLX2_MASK   0xFF80
#define THM_BLX2_INSTR  0x4780

#define ThumbDataProcInst(i)    (((i) & THM_DP_MASK) == THM_DP_INSTR)
#define ThumbSpecDPInst(i)      (((i) & THM_SPCDP_MASK) == THM_DP_INSTR)
#define ThumbLdrPCInst(i)       (((i) & THM_LDRPC_MASK) == THM_LDRPC_INSTR)
#define ThumbBlPrefInst(i)      (((i) & THM_BLPFX_MASK) == THM_BLPFX_INSTR)
#define ThumbBlInst(i)          (((i) & THM_BL_MASK) == THM_BL_INSTR)
#define Thumb1PushInst(i)       (((i) & THM_PUSH_MASK) == THM_PUSH_INSTR)
#define ThumbPopInst(i)         (((i) & THM_POP_MASK) == THM_POP_INSTR)
#define ThumbAddHiInst(i)       (((i) & THM_ADDHI_MASK) == THM_ADDHI_INSTR)
#define ThumbMovHiInst(i)       (((i) & THM_MOVHI_MASK) == THM_MOVHI_INSTR)
#define Thumb1AdjSPInst(i)      (((i) & THM_ADJSP_MASK) == THM_ADJSP_INSTR)
#define Thumb1AddSPInst(i)      (((i) & THM_ADDSP_MASK) == THM_ADDSP_INSTR)
#define ThumbBxInst(i)          (((i) & THM_BX_MASK) == THM_BX_INSTR)
#define ThumbNegInst(i)         (((i) & THM_NEG_MASK) == THM_NEG_INSTR)
#define ThumbBlxInst(i)         (((i) & THM_BLX_MASK) == THM_BLX_INSTR)
#define ThumbBlx2Inst(i)        (((i) & THM_BLX2_MASK) == THM_BLX2_INSTR)



typedef union _THUMBI {

    //
    //  This is not a complete definition of Thumb instructions. Only
    //  instruction formats which are decoded by the unwinder are included
    //  here.
    //

    USHORT  instruction;                    // The full 16-bit instruction

    struct {                                // Shift by immediate instruction
        USHORT rd       : 3;
        USHORT rm       : 3;
        USHORT immed    : 5;
        USHORT opcode   : 2;
        USHORT bits     : 3;                // == 000b
    } dpshi;

    struct {                                // Data processing instruction
        USHORT rd       : 3;
        USHORT rm       : 3;
        USHORT opcode   : 4;
        USHORT bits     : 6;                // == 010000b
    } dataproc;

    struct {                                // Special data processing inst
        USHORT rd       : 3;
        USHORT rm       : 3;
        USHORT H2       : 1;
        USHORT H1       : 1;
        USHORT opcode   : 2;
        USHORT bits     : 6;                // == 010001b
    } spcdataproc;

    struct {                                // Load from literal pool (PC rel)
        USHORT offset   : 8;
        USHORT rd       : 3;
        USHORT bits     : 5;                // == 01001b
    } ldrpc;

    struct {                                // Adjust Stack Pointer
        USHORT immed    : 7;
        USHORT s        : 1;                // subtract = 1, add = 0
        USHORT sbz      : 4;
        USHORT bits     : 4;                // == 1011b
    } adjsp;

    struct {                                // Push/Pop instruction
        USHORT reglist  : 8;
        USHORT r        : 1;                // Push/Pop PC if r == 1
        USHORT sbz      : 1;
        USHORT one      : 1;
        USHORT l        : 1;                // pop = 1, push = 0
        USHORT bits     : 4;                // == 1011b
    } push;
        
    struct {                                // Load/Store multiple
        USHORT reglist  : 8;
        USHORT rn       : 3;
        USHORT l        : 1;                // load = 1, store = 0
        USHORT bits     : 4;                // == 1100b
    } ldm;

    struct {                                // BL instruction Prefix
        USHORT offset   : 11;
        USHORT bits     : 5;                // == 11110b
    } blpref;

    struct {                                // BL instruction
        USHORT offset   : 11;
        USHORT bits     : 5;                // == 11111b
    } bl;

    struct {
        USHORT target:8;        // sign-extend
        USHORT cond:4;
        USHORT opcode:4;
    } bcond;

} THUMBI, *PTHUMBI;


typedef union _THUMBI32 {

    //
    //  This is not a complete definition of Thumb-2 instructions. Only
    //  instruction formats which are decoded by the unwinder are included
    //  here.
    //

    ULONG  instruction;                     // The full 32-bit instruction

    struct {                                // STM
// --------  HW1
        USHORT rn         : 4;
        USHORT unused1    : 1;
        USHORT w          : 1;
        USHORT unused2    : 1;
        USHORT vu         : 2;
        USHORT unused3    : 7;
// --------  HW2
        USHORT reglist    : 13;
        USHORT unused4    : 1;
        USHORT lr         : 1;
        USHORT unused5    : 1;
    } stm;


    struct {                                // ADD/SUB plain 12bit immediate
// --------  HW1
        USHORT rn         : 4;
        USHORT opcode     : 4;
        USHORT unused1    : 2;
        USHORT imm_11     : 1;
        USHORT unused2    : 5;
// --------  HW2
        USHORT imm_7_0    : 8;
        USHORT rd         : 4;
        USHORT imm_10_8   : 3;
        USHORT unused3    : 1;
    } addsubwide;

    struct {                                // ADD/SUB with modconst12 immediate
// --------  HW1
        USHORT rn         : 4;
        USHORT s          : 1;
        USHORT opcode     : 4;
        USHORT unused1    : 1;
        USHORT imm_11     : 1;
        USHORT unused2    : 5;
// --------  HW2
        USHORT imm_7_0    : 8;
        USHORT rd         : 4;
        USHORT imm_10_8   : 3;
        USHORT unused3    : 1;
    } addsubmc12;

    struct {                                // MOVT/MOVW
// --------  HW1
        USHORT imm_15_12  : 4;
        USHORT opcode     : 4;
        USHORT unused1    : 2;
        USHORT imm_11     : 1;
        USHORT unused2    : 5;
// --------  HW2
        USHORT imm_7_0    : 8;
        USHORT rd         : 4;
        USHORT imm_10_8   : 3;
        USHORT unused3    : 1;
    } movwt;


    struct {                                // VFP FSTM instruction
        USHORT rn       : 4;
        USHORT unused2  : 12;
        USHORT offset   : 8;
        USHORT unused1  : 4;
        USHORT dd       : 4;
    } fstm;

} THUMBI32, *PTHUMBI32;


#define T2_STM_MASK                 0xA000FE50
#define T2_STM_INSTR                0x0000E800
#define Thumb2PushInst(i)           (((i) & T2_STM_MASK) == T2_STM_INSTR)

#define T2_FSTM_MASK                0x0F00FE5F
#define T2_FSTM_INSTR               0x0B00EC0D
#define Thumb2FstmInst(i)           (((i) & T2_FSTM_MASK) == T2_FSTM_INSTR)


#define T2_ADDSUB_WIDE_MASK         0x8F00FB4F
#define T2_ADDSUB_WIDE_ISNTR        0x0D00F20D

#define T2_MOVT_MASK                0x8000FBC0
#define T2_MOVT_INSTR               0x0000F2C0

#define T2_MOVW_MASK                0x8000FB40
#define T2_MOVW_INSTR               0x0000F240

#define T2_ADDSUB_MC12_MASK         0x8F00FA00
#define T2_ADDSUB_MC12_INSTR        0x0D00F000


#endif // ARMINST

