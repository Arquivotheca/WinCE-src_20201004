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


//  thumb2instructions.h
//
//  This file contains the the data structures required to decipher thumb 2 instructions 
//  This code is shared between SysDbgDm.dll, eXdiHlp.dll and Osaxst1.dll (OS) 
//  IMPORTANT NOTE :
//  If you are changing anything in this file. Please modify tools\ide\debugger\include\thumb2instructions.h too. 
// 

#ifndef THUMB2_INSTRUCTIONS_H
#define THUMB2_INSTRUCTIONS_H

// These are the structures that we use to decipher the
// instruction.  We get an instruction number, and two
// auxiliary pieces of information, which go by many
// different names.
typedef struct _DcfInst {
    int InstNum;
    union {
        DWORD Auxil;
        DWORD Rd;
        DWORD TargetHigh;
        DWORD RegList;
        DWORD BX_Register;
        DWORD BranchTarget;
        DWORD PopList;
        DWORD PushList;
        DWORD Pop_Register; 
    };
    union {
        DWORD Aux2;
        DWORD Rs;
        DWORD TargetLow;
        DWORD PushLR;
        DWORD PopPC;
        DWORD BX_SBZ;
        DWORD Condition; 
   };
} DcfInst;


// A list contains a list of parameters to match an instruction, and then
// extract the two auxiliary pieces of information.
typedef struct _DIList {
    DWORD Val,Mask;        // These fields are used for matching instruction.

    int InstNum;
    int InstLen;

    DWORD    RdMask;
    int        RdShift;
    DWORD    RdSignExtend;

    DWORD    RsMask;
    int        RsShift;
    DWORD    RsSignExtend;

} DIList;

enum Thumb2Instructions
{
    DI_BREAKPOINT = 1, 
    DI_BUNCOND,
    DI_BRANCH,
    DI_POP,
    DI_POP_PC,
    DI_DECSP,
    DI_INCSP,
    DI_MOVHI,
    DI_ADDHI,
    DI_BLPAIR,
    DI_BLPFX,
    DI_BL,
    DI_BX,
    DI_UNDEF,
    DI_BLBLX1,
    DI_BLX1,
    DI_BLX2,
    DI_T2_BLX,
    DI_T2_B,
    DI_T2_BL,
    DI_T2_BCOND,
    DI_T2_LDR_IMM,
    DI_T2_LDR_INDEXED,
    DI_T2_LDR_REG_OFFSET,
    DI_T2_LDR_PC_RELATIVE,
    DI_T2_SUBS,
    DI_T2_TBB,
    DI_T2_TBH,
    DI_T2_CBZ,
    DI_T2_IT,
    DI_T2_LDM,
    DI_T2_LDMDB,
}; 

static DIList dilistThumb[] = {

    // Val         Mask          InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
#if 0
    {BP_OPCODE_16, 0xFFFF,       DI_BREAKPOINT,          2,       0x00000000, 0,       0x00000000,   0x00000000, 0,       0x00000000}, // Breakpoint
#endif
    {0x0000E000, 0x0000F800,     DI_BUNCOND,             2,       0x000007FF, 1,       0xFFFFFC00,   0x00000000, 0,       0x00000000}, // B uncond
    {0x0000D000, 0x0000F000,     DI_BRANCH,              2,       0x000000FF, 1,       0xFFFFFF00,   0x00000F00, -8,      0x00000000}, // B conditional

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x0000BC00, 0x0000FE00,     DI_POP,                 2,       0x000000FF, 0,       0x00000000,   0x00000100, -8,      0x00000000}, //POP
    {0x0000E8BD, 0x2000FFFF,     DI_POP,                 4,       0x7FFF0000, -16,     0x00000000,   0x80000000, -31,     0x00000000}, //POP Thumb2 32-bit
    {0x0B04F85D, 0x0FFFFFFF,     DI_POP_PC,              4,       0xF0000000, -28,     0x00000000,   0x00000000, 0,       0x00000000}, //POP PC Thumb2 32-bit

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x0000B080, 0x0000FF80,     DI_DECSP,               2,       0x0000007F, 2,       0x00000000,   0x00000000, 0,       0x00000000}, //DecSP
    {0x0000B000, 0x0000FF80,     DI_INCSP,               2,       0x0000007F, 2,       0x00000000,   0x00000000, 0,       0x00000000}, //IncSP

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x00004600, 0xFF00,         DI_MOVHI,               2,       0x00000007, 0,       0x00000000,   0x00000078, -3,      0x00000000}, //MovHiRegs
    {0x00004400, 0xFF00,         DI_ADDHI,               2,       0x00000007, 0,       0x00000000,   0x00000078, -3,      0x00000000}, //AddHiRegs

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0xF000F85F, 0xF000FF7F,     DI_T2_LDR_PC_RELATIVE,  4,       0x00000080, 0,       0x00000000,   0x0FFF0000, -16,     0x00000000}, // LDR from PC-relative offset (Thumb-2)
    {0xF000F8D0, 0xF000FFF0,     DI_T2_LDR_IMM,          4,       0x0000000F, 0,       0x00000000,   0x0FFF0000, -16,     0x00000000}, // LDR positive, immediate offset (Thumb-2)
    {0xF800F850, 0xF800FFF0,     DI_T2_LDR_INDEXED,      4,       0x0000000F, 0,       0x00000000,   0x0FFF0000, -16,     0x00000000}, // LDR positive/negative offset, post- or pre-indexed (Thumb-2)
    {0xF000F850, 0xFFC0FFF0,     DI_T2_LDR_REG_OFFSET,   4,       0x0000000F, 0,       0x00000000,   0x003F0000, -16,     0x00000000}, // LDR from a register offset (Thumb-2)

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x8F00F3DE, 0xDF00FFFF,     DI_T2_SUBS,             4,       0x0000000F, 0,       0x00000000,   0x00FF0000, -16,     0x00000000}, // Subtract an immediate offset (Thumb-2)

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0xC000F000, 0xD001F800,     DI_T2_BLX,              4,       0x000007FF, 12,      0x00000000,   0x3FFE0000, -15,     0x00000000}, // BLX (Thumb-2)
    {0x9000F000, 0xD000F800,     DI_T2_B,                4,       0x000007FF, 12,      0x00000000,   0x3FFF0000, -15,     0x00000000}, // Uncoditional B (Thumb-2)
    {0xD000F000, 0xD000F800,     DI_T2_BL,               4,       0x000007FF, 12,      0x00000000,   0x3FFF0000, -15,     0x00000000}, // BL (Thumb-2)
    {0x8000F000, 0xD000F800,     DI_T2_BCOND,            4,       0x000007FF, 12,      0x00000000,   0x3FFF0000, -15,     0x00000000}, // Conditional Branch (Thumb-2)
    
    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0xF000E8D0, 0xFFF0FFF0,     DI_T2_TBB,              4,       0x0000000F, 0,       0x00000000,   0x000F0000, -16,     0x00000000}, // Table branch - single byte offsets (Thumb-2)
    {0xF010E8D0, 0xFFF0FFF0,     DI_T2_TBH,              4,       0x0000000F, 0,       0x00000000,   0x000F0000, -16,     0x00000000}, // Table branch - halfword offsets (Thumb-2)

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x0000B100, 0x0000F500,     DI_T2_CBZ,              2,       0x00000FF8, 0,       0x00000000,   0x00000007, 0,       0x00000000},

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x0000BF00, 0x0000FF00,     DI_T2_IT,               2,       0x000000F0, -4,      0x00000000,   0x0000000F, 0,       0x00000000},

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x8000E890, 0x8000FFD0,     DI_T2_LDM,              4,       0x7FFF0000, -16,     0x00000000,   0x0000000F, 0,       0x00000000},
    {0x8000E910, 0x8000FFD0,     DI_T2_LDMDB,            4,       0x7FFF0000, -16,     0x00000000,   0x0000000F, 0,       0x00000000},

    // Hit this one first, so we don't look at BL.
    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0xF800F000,0xF800F800,      DI_BLPAIR,              4,       0x000007FF, 12,      0xFFC00000,   0x07FF0000, -15,     0x00000000}, // BL pair
    {0xE800F000,0xF800F800,      DI_BLBLX1,              4,       0x000007FF, 12,      0xFFC00000,   0x07FF0000, -15,     0x00000000}, // BL, BLX(1) (THUMBv2)

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x0000F000, 0x0000F800,     DI_BLPFX,               2,       0x000007FF, 12,      0xFFC00000,   0x00000000, 0,       0x00000000}, // BL prefix
    {0x0000F800, 0x0000F800,     DI_BL,                  2,       0x000007FF, 1,       0xFFFF8000,   0x00000000, 0,       0x00000000}, // BL
    {0x0000E800, 0x0000F800,     DI_BLX1,                2,       0x000007FF, 1,       0xFFFF8000,   0x00000000, 0,       0x00000000}, // BLX(1) (THUMBv2)

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x00004780, 0x0000FF87,     DI_BLX2,                2,       0x00000078, -3,      0x00000000,   0x00000000, 0,       0x00000000}, // BLX(2) (THUMBv2)

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x00004700, 0x0000FF87,     DI_BX,                  2,       0x00000078, -3,      0x00000000,   0x00000000, 0,       0x00000000}, // BX

    // Val       Mask            InstNum                 InstLen  RdMask      RdShift  RdSignExtend  RsMask      RsShift  RsSignExtend
    {0x0000E800, 0x0000F800,     DI_UNDEF,               2,       0x00000000, 0,       0x00000000,   0x00000000, 0,       0x00000000}, // Undefined
    {0x00000000, 0x00000000,     0x00,                   0,       0x00000000, 0,       0x00000000,   0x00000000, 0,       0x00000000}  //End of list
};

#endif 
