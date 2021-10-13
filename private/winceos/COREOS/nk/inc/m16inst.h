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

    m16inst.h

Abstract:

    MIPS-16 instruction and floating constant definitions.

--*/

#ifndef _MP16INST_
#define _MP16INST_

//
// Define MIPS-16 instruction format structures.
//

typedef union _MIPS16_INSTRUCTION {
    USHORT Short;
    UCHAR Byte[2];

    struct
    {
        USHORT Immediate : 11;
        USHORT Opcode : 5;
    } i_format;

    struct
    {
        SHORT Simmediate : 11;
        USHORT Opcode : 5;
    } b_format;

    struct
    {
        USHORT Immediate : 8;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } ri_format;
      
    struct
    {
        SHORT Simmediate : 8;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } rsi_format;
      
    struct
    {
        USHORT Function : 5;
        USHORT Ry : 3;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } rr_format;

    struct
    {
        USHORT Immediate : 5;
        USHORT Ry : 3;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } rri_format;

    struct
    {
        USHORT Function : 2;
        USHORT Rz : 3;
        USHORT Ry : 3;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } rrr_format;

    struct
    {
        USHORT Immediate : 4;
        USHORT Function : 1;
        USHORT Ry : 3;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } rria_format;

    struct
    {
        USHORT Function : 2;
        USHORT Shift : 3;
        USHORT Ry : 3;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } shift_format;

    struct
    {
        USHORT Immediate : 8;
        USHORT Function : 3;
        USHORT Opcode : 5;
    } i8_format;

    struct
    {
        USHORT R32 : 5;
        USHORT Ry : 3;
        USHORT Function : 3;
        USHORT Opcode : 5;
    } movr32_format;
      
    struct
    {
        USHORT Rz : 3;
        USHORT R32 : 5;            // mangled
        USHORT Function : 3;
        USHORT Opcode : 5;
    } mov32r_format;

    struct
    {
        USHORT Immediate : 8;
        USHORT Function : 3;
        USHORT Opcode : 5;
    } i64_format;

    struct
    {
        USHORT Immediate : 5;
        USHORT Ry : 3;
        USHORT Function : 3;
        USHORT Opcode : 5;
    } ri64_format;

    struct
    {
        USHORT Target21 : 5;
        USHORT Target16 : 5;
        USHORT Ext : 1;
        USHORT Opcode : 5;
    } j_format;

    struct
    {
        USHORT Function : 5;
        USHORT Ry : 3;
        USHORT Shift : 3;
        USHORT Opcode : 5;
    } shift64_format;

    struct
    {
        USHORT Zero1 : 5;
        USHORT Function : 3;
        USHORT Rx : 3;
        USHORT Opcode : 5;
    } jr_format;

    struct
    {
        USHORT Break : 5;
        USHORT Code : 6;
        USHORT Opcode : 5;
    } break_format;

// EXTEND formats

    struct
    {
        SHORT Simmediate11 : 5;
        USHORT Immediate5 : 6;
        USHORT Opcode : 5;
    } ei_format;

    struct
    {
        USHORT Immediate11 : 4;
        USHORT Immediate4 : 7;
        USHORT Opcode : 5;
    } erria_format;

    struct
    {
        USHORT Zero : 5;
        USHORT Shift5 : 1;
        USHORT Shift0 : 5;
        USHORT Opcode : 5;
    } eshift_format;

} MIPS16_INSTRUCTION, *PMIPS16_INSTRUCTION;

//
// Define MIPS-16 instruction major opcode values.
//

#define I8_OP16             0x0c         // I8-type - use minor opcode in bits [10:8]
#define RRR_OP16            0x1c         // RRR-type - use minor opcode in bits [1:0]
#define RR_OP16             0x1d         // RR-type - use minor opcode in bits [4:0]

#define ADDIUSP_OP16        0x00         // add immediate to sp 3-op
#define B_OP16              0x02         // unconditional branch
#define JALX_OP16           0x03         // JAL & JALX
#define ADDIU8_OP16         0x09         // add 8-bit immediate
#define LI_OP16             0x0d         // load 8-bit unsigned immediate 
#define LWPC_OP16           0x16         // load word base pc
#define SWSP_OP16           0x1a         // store word base sp
#define SW_OP16             0x1b         // store word base integer register
#define EXTEND_OP16         0x1e         // extend immediate of next instruction

//
// Define I8-type minor opcode values (bits [10:8]).
//

#define SWRASP_OP16         0x02         // store word in ra, base sp
#define ADJSP_OP16          0x03         // add immediate unsigned integer to sp
#define MOV32R_OP16         0x05         // move general register to any of 32 registers
#define MOVR32_OP16         0x07         // move any of 32 registers to general register

//
// Define RRR-type minor opcode values (bits [1:0]).
//

#define ADDU_OP16           0x01         // add unsigned integer
#define SUBU_OP16           0x03         // subtract unsigned integer

//
// Define RR-type minor opcode values (bits [4:0]).
//

#define JR_OP16             0x00         // JR and JALR
#define BREAK_OP16          0x05         // BREAK

// MIPS16 ops
#define ADDIUSP_OP16 0x00
#define ADDIUPC_OP16 0x01
#define B_OP16       0x02
#define JAL_OP16     0x03
#define BEQZ_OP16    0x04
#define BNEZ_OP16    0x05
#define SHIFT_OP16   0x06

#define RRIA_OP16    0x08
#define ADDIU8_OP16  0x09
#define SLTI_OP16    0x0A
#define SLTIU_OP16   0x0B
#define CMPI_OP16    0x0E

#define LB_OP16      0x10
#define LH_OP16      0x11
#define LWSP_OP16    0x12
#define LW_OP16      0x13
#define LBU_OP16     0x14
#define LHU_OP16     0x15
#define LWPC_OP16    0x16

#define SB_OP16      0x18
#define SH_OP16      0x19

// RR Function encodings
#define JRRX_OP16    0x0
#define JRRA_OP16    0x1
#define JALR_OP16    0x2

// I8 Function encodings
#define BTEQZ_OP16   0x0
#define BTNEZ_OP16   0x1

#endif // MP16INST





