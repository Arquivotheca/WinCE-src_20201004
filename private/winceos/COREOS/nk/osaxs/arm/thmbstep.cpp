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

#include <osaxs_p.h>
#include <step.h>
#include <arminst.h>

static BOOL ReadThunk(ULONG Addr, ULONG Size, void *Thunk)
{
    return ReadMem(Addr, Thunk, Size);
}

#define ReadInstWord ReadWord
#define ReadEADword ReadDword
#define ReadInstDword ReadDword

#else

#define SET_BIT0 0x1
#define CLEAR_BIT0 (0xFFFFFFFE)

#endif

#define T2_BRANCH_S_SHIFT   22
#define T2_BRANCH_I1_SHIFT  14
#define T2_BRANCH_I2_SHIFT  12
#define T2_BRANCH_OFFSET_S_POS  24
#define T2_BRANCH_OFFSET_I1_POS 23
#define T2_BRANCH_OFFSET_I2_POS 22
#define T2_BRANCH_CLEAR_S_MASK  0x003FF000
#define T2_BRANCH_CLEAR_I_MASK  0x00000FFE
#define T2_BRANCH_SIGN_EXTEND_MASK   0xFF000000
#define T2_B_FINAL_MASK              0xFFFFFFFC
#define T2_BCOND_S_SHIFT        20
#define T2_BCOND_I2_SHIFT       19
#define T2_BCOND_I1_SHIFT       18
#define T2_B_COND_CODE_MASK 0x003C0000
#define T2_B_COND_CODE_SHIFT    18
#define T2_B_CLEAR_COND_CODE    0x0003F000
#define T2_BCOND_SIGN_EXTEND_MASK   0xFFF00000

#define T2_LDR_ADD_BIT_MASK 0x0200
#define T2_LDR_OFFSET_MASK  0x00FF

#define T2_LDR_OFFSET_REG_MASK  0x000F
#define T2_LDR_LEFT_SHIFT_MASK  0x0030
#define T2_LDR_LEFT_SHIFT_SHIFT 0x0004
#define T2_LDR_BASE_REG_MASK    0x000F

#define T2_CZB_N_SHIFT      11
#define T2_CZB_LOW_OFFSET_MASK  0x00F8
#define T2_CZB_LOW_OFFSET_SHIFT 2
#define T2_CZB_HI_OFFSET_MASK   0x0200
#define T2_CZB_HI_OFFSET_SHIFT  3

#define T2_IT_FIRST_PARAM_SHIFT     4
#define T2_IT_CURRENT_PARAM_MASK    0x10
#define T2_IT_REMAINING_PARAM_MASK  0x0F
#define T2_IT_INST_LENGTH 2

#define T2_PSR_COND_CODE_MASK   0x0000F000
#define T2_PSR_COND_CODE_SHIFT  12
#define T2_PSR_LOW_PARAM_MASK   0x06000000
#define T2_PSR_LOW_PARAM_SHIFT  25
#define T2_PSR_HI_PARAM_MASK    0x00001C00
#define T2_PSR_HI_PARAM_SHIFT   8

#define T2_INST_LEN_PREFIX_MASK 0x0000E000
#define T2_INST_LEN_PREFIX_VAL  0x0000E000
#define T2_INST_LEN_SUFFIX_MASK 0x00001800

#define T2_REG_PC   0x0F

extern DWORD ARMRegister(DWORD i);
static BOOL ThumbCheckConditionCodes(DWORD, DWORD);


DWORD const ARM_IREG_LR = 14; //(offsetof(CONTEXT, Lr) - offsetof(CONTEXT, R0)) / sizeof(ULONG);
DWORD const ARM_IREG_PC = 15; //(offsetof(CONTEXT, Pc) - offsetof(CONTEXT, R0)) / sizeof(ULONG);

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
    };
    union {
        DWORD Aux2;
        DWORD Rs;
        DWORD TargetLow;
        DWORD PushLR;
        DWORD PopPC;
        DWORD BX_SBZ;

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

static DIList dilistThumb[] = {

#if 0
#define DI_BREAKPOINT    1
    {BP_OPCODE_16,0xFFFF,DI_BREAKPOINT,2, 0x0,0,0x0, 0x0, 0,0x0},    // Breakpoint
#endif

#define DI_BUNCOND        2
    {0xE000,0xF800,DI_BUNCOND,    2, 0x07FF,1,0xFFFFFC00, 0x0000, 0,0x00000000},    // B uncond
#define DI_BRANCH        3
    {0xD000,0xF000,DI_BRANCH,    2, 0x00FF,1,0xFFFFFF00, 0x0F00,-8,0x00000000},    //B conditional

#define DI_PUSH            4
#define DI_POP            5
    {0xB400,0xFE00,DI_PUSH,        2, 0x00FF,0,0x00000000, 0x0100,-8,0x00000000},    //PUSH
    {0xBC00,0xFE00,DI_POP,        2, 0x00FF,0,0x00000000, 0x0100,-8,0x00000000},    //POP

#define DI_DECSP        6
#define DI_INCSP        7
    {0xB080,0xFF80,DI_DECSP,    2, 0x007F,2,0x00000000, 0x0000,0,0x00000000},    //DecSP
    {0xB000,0xFF80,DI_INCSP,    2, 0x007F,2,0x00000000, 0x0000,0,0x00000000},    //IncSP

#define DI_MOVHI        8
#define DI_ADDHI        9
    {0x4600,0xFF00,DI_MOVHI,    2, 0x0007,0,0x00000000, 0x0078,-3,0x00000000},    //MovHiRegs
    {0x4400,0xFF00,DI_ADDHI,    2, 0x0007,0,0x00000000, 0x0078,-3,0x00000000},    //AddHiRegs

#define DI_T2_LDR_IMM       22
#define DI_T2_LDR_NEGIMM    23
#define DI_T2_LDR_INDEXED        24
#define DI_T2_LDR_REG_OFFSET     25
#define DI_T2_LDR_PC_RELATIVE   26
    {0xF000F85F, 0xF000FF7F, DI_T2_LDR_PC_RELATIVE, 4, 0x00000080, 0, 0, 0x0FFF0000, -16, 0}, // LDR from PC-relative offset (Thumb-2)
    {0xF000F8D0, 0xF000FFF0, DI_T2_LDR_IMM, 4, 0x00000000F, 0, 0, 0x0FFF0000, -16, 0}, // LDR positive, immediate offset (Thumb-2)
    {0xFC00F850, 0xFF00FFF0, DI_T2_LDR_NEGIMM, 4, 0x0000000F, 0, 0, 0x00FF0000, -16, 0}, // LDR negative, immediate offset (Thumb-2)
    {0xF900F850, 0xF900FFF0, DI_T2_LDR_INDEXED, 4, 0x0000000F, 0, 0, 0x02FF0000, -16, 0}, // LDR positive/negative offset, post- or pre-indexed (Thumb-2)
    {0xF000F850, 0xF000FFF0, DI_T2_LDR_REG_OFFSET, 4, 0x0000000F, 0, 0, 0x003F0000, -16, 0}, // LDR from a register offset (Thumb-2)

#define DI_T2_SUBS  27
    {0x8F00F3DE, 0xDF00FFFF, DI_T2_SUBS, 4, 0x0000000F, 0, 0, 0x00FF0000, -16, 0}, // Subtract an immediate offset (Thumb-2)

// Val, Mask, InstNum, InstLen, RdMask, RdShift, RdSignExtend, RsMask, RsShift, RsSignExtend
#define DI_T2_BLX       18
#define DI_T2_B         19  
#define DI_T2_BL        20
#define DI_T2_BCOND     21
    {0xC000F000, 0xD001F000, DI_T2_BLX, 4, 0x000007FF, 12, 0, 0x3FFF0000, -15, 0}, // BLX (Thumb-2)
    {0x9000F000, 0xD001F000, DI_T2_B, 4, 0x000007FF, 12, 0, 0x3FFF0000, -15, 0}, // Uncoditional B (Thumb-2)
    {0xD000F000, 0xD001F000, DI_T2_BL, 4, 0x000007FF, 12, 0, 0x3FFF0000, -15, 0}, //  BL (Thumb-2)
    {0x8000F000, 0xD001F000, DI_T2_BCOND, 4, 0x000007FF, 12, 0, 0x3FFF0000, -15, 0}, // Conditional Branch (Thumb-2)
    
#define DI_T2_TBB   28
#define DI_T2_TBH   29
    {0x0000E8D0, 0x00F0FFF0, DI_T2_TBB, 4, 0x0000000F, 0, 0, 0x000F0000, -16, 0}, // Table branch - single byte offsets (Thumb-2)
    {0x0010E8D0, 0x00F0FFF0, DI_T2_TBH, 4, 0x0000000F, 0, 0, 0x000F0000, -16, 0}, // Table branch - halfword offsets (Thumb-2)

#define DI_T2_CZB   30
    {0xB100, 0xF500, DI_T2_CZB, 2, 0x0FF8, 0, 0, 0x0007, 0, 0},

#define DI_T2_IT    31
    {0xBF00, 0xFF00, DI_T2_IT, 2, 0x00F0, -4, 0, 0x000F, 0, 0},

#define DI_BLPAIR        10
#define DI_BLBLX1       15
    // Hit this one first, so we don't look at BL.
    {0xF800F000,0xF800F800,DI_BLPAIR , 4, 0x000007FF,12,0xFFC00000, 0x07FF0000,-15,0x00000000},    //BL pair
    {0xE800F000,0xF800F800,DI_BLBLX1,  4, 0x000007FF,12,0xFFC00000, 0x07FF0000,-15,0x00000000}, //BL, BLX(1) (THUMBv2)

#define DI_BLPFX        11
#define DI_BL            12
#define DI_BLX1            16
    {0xF000,0xF800,DI_BLPFX,    2, 0x07FF,12,0xFFC00000, 0x0000,0,0x00000000},    //BL prefix
    {0xF800,0xF800,DI_BL,        2, 0x07FF, 1,0xFFFF8000, 0x0000,0,0x00000000},    //BL
    {0xE800,0xF800,DI_BLX1,     2, 0x07FF, 1,0xFFFF8000, 0x0000,0,0x00000000},  //BLX(1) (THUMBv2)

#define DI_BLX2            17
    {0x4780,0xFF87,DI_BLX2,     2, 0x0078,-3,0x00000000, 0x0007,0,0x00000000},  //BLX(2) (THUMBv2)

#define DI_BX            13
    {0x4700,0xFF87,DI_BX,        2, 0x0078,-3,0x00000000, 0x0007,0,0x00000000},    //BX

#define DI_UNDEF        14
    {0xE800,0xF800,DI_UNDEF,    2, 0x0000,0,0x00000000, 0x0000,0,0x00000000},    // Undefined

    {0x0000,0x0000,0x00,        0, 0x0000,0,0x00000000, 0x0000,0,0x00000000}    //End of list
};

DWORD ThumbGetInstructionLength(DWORD dwInstruction)
{
    if(((dwInstruction & T2_INST_LEN_PREFIX_MASK) == T2_INST_LEN_PREFIX_VAL) 
            && ((dwInstruction & T2_INST_LEN_SUFFIX_MASK) != 0))
    {
        return 4;   // 32-bit thumb2 instruction
    }
    return 2; // default thumb1 or thumb2 instruction 
}

static int ThumbDecipherInstruction(DWORD inst, DcfInst *DI)
{
    int i;
    DIList *dl = dilistThumb;
    int InstLen = 0;

    if (!DI) return 0;
    memset(DI,0,sizeof(DcfInst));

    InstLen = ThumbGetInstructionLength(inst);

    for(i=0;dl[i].Mask!=0 && DI->InstNum==0; i++) {
        // Because thumb2 has variable length instructions, we need to
        // match not only on the mask, but on the length of the instruction
        if(((inst&dl[i].Mask)==dl[i].Val) && (dl[i].InstLen == InstLen)) {

            DI->InstNum = dl[i].InstNum;

            // Extract, shift, and sign extend the first auxiliary field
            DI->Rd = (inst&dl[i].RdMask);
            if(DI->Rd) {
                if(dl[i].RdShift) {
                    if(dl[i].RdShift>0)    DI->Rd <<=   dl[i].RdShift;
                    else                DI->Rd >>= (-dl[i].RdShift);
                }

                if(DI->Rd & dl[i].RdSignExtend) DI->Rd |= dl[i].RdSignExtend;
            }

            // Extract, shift, and sign extend the second auxiliary field
            DI->Rs = (inst&dl[i].RsMask);
            if(DI->Rs) {
                if(dl[i].RsShift) {
                    if(dl[i].RsShift>0)    DI->Rs <<=   dl[i].RsShift;
                    else                DI->Rs >>= (-dl[i].RsShift);
                }

                if(DI->Rs & dl[i].RsSignExtend) DI->Rs |= dl[i].RsSignExtend;
            }

            // Special case to handle MovHiRegs and AddHiRegs.
            if(DI->InstNum==DI_MOVHI || DI->InstNum==DI_ADDHI ) {
                DI->Rd |= ((inst&0x0080)>>4);
            }
        }
    }
    
    return InstLen;
}


// InstructionStatus holds the validity of
// interpreted instructions.
enum InstStatus {
    isNotInterpreted,
    isValid,
    isInvalid
};

enum CondStatus {
    csAlwaysFalse,        // Never execute this instruction
    csAlwaysTrue,        // Always execute this instruction
    csConditionFalse,
    csConditionTrue,
    csConditionCannotBeEvaluated
};

// Some targets are fixed, and can
// be interpreted "off-line."  Others
// are variable, and we have to wait
// until the branch is the next instruction
// before we can evaluate the branch target.
enum TargSpec {
    tsTargetUnknown,
    tsTargetFixed,
    tsTargetVariable,
    tsTargetCannotBeEvaluated
};

typedef struct _structThumbInstrInterp {
    DWORD Instruction;
    union {
        USHORT Flags;
        struct {
            enum InstStatus InstructionStatus:2;


            // All jumps, branches, and calls result
            // in IsBranch = TRUE;
            BOOL    IsBranch:1;
            BOOL    IsCall:1;
            // Just BL sets this one
            BOOL    IsLinkedBranch:1;

            enum CondStatus ConditionStatus:3;

            BOOL IsBreakpoint:1;

            enum TargSpec TargetSpecification:2;

            // These instructions are useful for
            // locating RETURN instructions.
            USHORT PopPc:1;
            USHORT LR2PC:1;
            USHORT BXLR:1;

            BOOL IsReturn:1;
            DWORD Pad:2;
        };
    };
    // The TargetAddr is the address that the
    // branch points to.  The SequentialAddr is
    // the address of the next instruction in
    // sequence, ignoring the branch.
    ULONG TargetAddr;
    BOOL TargetIsThumb;
    ULONG SequentialAddr;

} ThumbInstInterp;

static BOOL DetermineIfCall(ULONG,ThumbInstInterp*);

DWORD Thumb2FindNextITInstruction(DWORD dwCurrentPc, DWORD dwITParameters, BOOL fCurrentInstructionExecuted)
{
        BOOL    bNextInstructionFound = FALSE;
        DWORD dwTotalLengthOfInstructions = 0;
        DWORD dwOriginalParameter = dwITParameters & T2_IT_CURRENT_PARAM_MASK;

        DWORD dwTargetParameter = 0;
        if(fCurrentInstructionExecuted == TRUE)
        {
            dwTargetParameter = dwOriginalParameter;
        }
        else
        {
            dwTargetParameter = dwOriginalParameter ^ T2_IT_CURRENT_PARAM_MASK;
        }

        
        while((dwITParameters & T2_IT_REMAINING_PARAM_MASK) != 0 && bNextInstructionFound==FALSE)
        {
            DWORD dwNextInstruction; 
            DWORD dwNextInstructionLength;
            
            ReadEADword(((dwCurrentPc + dwTotalLengthOfInstructions) & CLEAR_BIT0), &dwNextInstruction);
      
            dwNextInstructionLength = ThumbGetInstructionLength(dwNextInstruction);            
            
            if(!fCurrentInstructionExecuted && ((dwITParameters & T2_IT_CURRENT_PARAM_MASK) == dwTargetParameter))
            {
                bNextInstructionFound = TRUE;
            }
            else
            {
                dwTotalLengthOfInstructions += dwNextInstructionLength;                
                dwITParameters = dwITParameters << 1;
                fCurrentInstructionExecuted = FALSE;
            }
        }
        
    return dwTotalLengthOfInstructions;
}

static int
ThumbInterpretInstruction
    (
    ULONG    InstAddr,
    BYTE*    pMemory,
    ThumbInstInterp* pII
    )
{
    DWORD    dwNextPc = 0;
    DWORD    dwSequentialOffset = 0;
    DWORD    dwCurrentPc = InstAddr;
    DWORD    dwCurrentPsr = DD_ExceptionState.context->Psr;
    int InstructionLength;
    ULONG ul;

    DWORD   dwInstruction;
    DcfInst  DI;

    // If we have a memory buffer, use it to get the
    // instruction.  Otherwise, just read it in ourselves.
    if(pMemory) {
        memcpy(&dwInstruction,pMemory,sizeof(DWORD));
    } else {
        ReadInstDword((InstAddr & CLEAR_BIT0), &dwInstruction);
    }

    pII->Instruction = dwInstruction;
    pII->Flags = 0;
    pII->InstructionStatus = isValid;
    pII->ConditionStatus = csAlwaysTrue;

    InstructionLength=ThumbDecipherInstruction(dwInstruction,&DI);
    dwSequentialOffset = dwCurrentPc + InstructionLength;

    DWORD dwITParameters = (dwCurrentPsr & T2_PSR_LOW_PARAM_MASK) >> T2_PSR_LOW_PARAM_SHIFT;
    dwITParameters |= ((dwCurrentPsr & T2_PSR_HI_PARAM_MASK) >> T2_PSR_HI_PARAM_SHIFT);

    // The PSR condition code field will be zero if no IT block is present
    if(dwITParameters != 0)
    {
        DWORD dwTotalLengthOfInstructions = Thumb2FindNextITInstruction(dwCurrentPc, dwITParameters, TRUE);
        dwNextPc = dwCurrentPc + dwTotalLengthOfInstructions;
        pII->IsBranch = TRUE;
        pII->TargetSpecification = tsTargetFixed;
        pII->ConditionStatus = csConditionTrue;
    }
    
    pII->TargetIsThumb = TRUE;
    
    switch(DI.InstNum) {

        case DI_UNDEF:
            pII->InstructionStatus = isInvalid;
            break;

#if 0
        case DI_BREAKPOINT:
            pII->IsBreakpoint = TRUE;
            break;
#endif

        case DI_BUNCOND:
            pII->IsBranch=TRUE;
            dwNextPc = dwCurrentPc + 4 + (long)DI.BranchTarget;

            pII->TargetSpecification=tsTargetFixed;
            break;

        case DI_BRANCH:
            pII->IsBranch = TRUE;
            dwNextPc = dwCurrentPc + 4 + DI.BranchTarget;

            pII->TargetSpecification=tsTargetFixed;
            if(ThumbCheckConditionCodes(DI.Aux2, dwCurrentPsr))
                pII->ConditionStatus = csConditionTrue;
            else
                pII->ConditionStatus = csConditionFalse;

            break;

        case DI_BLPAIR:
        case DI_BLBLX1:
            pII->IsBranch = TRUE;
            pII->IsLinkedBranch = TRUE;
            pII->TargetSpecification = tsTargetFixed;
            dwNextPc = dwCurrentPc + 4 + (long)DI.Auxil + (long)DI.Aux2;
            if (DI.InstNum == DI_BLBLX1) pII->TargetIsThumb = FALSE; // (THUMBv2)
            break;

        case DI_BL:
        case DI_BLX1:
            // We may encounter an isolated BL (or BLX) if the user
            // intentionally put a bp on the second half of
            // a BL pair, or if the BL prefix was replaced
            // with another instruction.
            pII->IsBranch = TRUE;
            pII->IsLinkedBranch = TRUE;

            // Half of the target specification is in the
            // LR register, so this counts as a variable
            // target.
            pII->TargetSpecification = tsTargetVariable;

            dwNextPc = DD_ExceptionState.context->Lr + (long)DI.TargetLow;

            if (DI.InstNum == DI_BLX1) pII->TargetIsThumb = FALSE; // (THUMBv2)

            break;

        case DI_POP:
            if(DI.PopPC!=0) {
                ULONG StackAddr = 0;;

                pII->IsBranch = TRUE;
                pII->PopPc = TRUE;
                pII->TargetSpecification = tsTargetVariable;
                // Compute the stored location of pc:
                StackAddr = DD_ExceptionState.context->Sp;
                // Add 4 bytes for every on-bit in the
                // register list.
                for(ul = 0x1; ul < 0x100; ul <<= 1) {
                    if(DI.PopList & ul) StackAddr += 4;
                }
                // Now we know where the PC is, so go read it.
                ReadEADword(StackAddr, &dwNextPc);

                // TODO: this is different behavior between THUMBv1 and THUMBv2!!!
                if ((dwNextPc & 0x01L) == 0x00L) pII->TargetIsThumb = FALSE;
            }
            break;

        case DI_BLX2:
            // Similar to DI_BX below
            pII->IsBranch = TRUE;
            pII->IsLinkedBranch = TRUE;
            pII->TargetSpecification=tsTargetVariable;
            dwNextPc = ARMRegister(DI.BX_Register);
            // Not sure what happens if DI.BX_Register == ARM_REG_LR
            if ((dwNextPc & 0x01L) == 0x00L) pII->TargetIsThumb = FALSE;
            break;

        case DI_BX:
            pII->IsBranch = TRUE;
            pII->TargetSpecification=tsTargetVariable;
            // It's ok to leave bit0 as it is.
            // All the Find/Get/Write BP functions
            // will strip it, and it might be useful
            // in the future to have it left in.
            dwNextPc = ARMRegister(DI.BX_Register);
            if (DI.BX_Register == ARM_IREG_LR) pII->BXLR = TRUE;
            if ((dwNextPc & 0x01L) == 0x00L) pII->TargetIsThumb = FALSE;
            break;

        case DI_MOVHI:
            if(DI.Rd==ARM_IREG_PC) {
                pII->IsBranch=TRUE;
                pII->TargetSpecification = tsTargetVariable;
                dwNextPc = ARMRegister(DI.Rs);
                if(DI.Rs==ARM_IREG_LR) pII->LR2PC = TRUE;
            }
            break;

        case DI_ADDHI:
            if(DI.Rd==ARM_IREG_PC) {
                pII->IsBranch=TRUE;
                pII->TargetSpecification = tsTargetVariable;
                dwNextPc = DD_ExceptionState.context->Pc + ARMRegister(DI.Rs) + 4;
            }
            break;

        case DI_T2_BLX:
        case DI_T2_B:
        case DI_T2_BL:
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetFixed;

            // Only set linked flag for BL and BLX
            if(DI.InstNum != DI_T2_B)
            {
                pII->IsLinkedBranch = TRUE;
            }
            
            DWORD dwS = ((DI.Auxil >> T2_BRANCH_S_SHIFT) & 0x01); // S = DI->Auxil[22]
            DWORD dwI1 = ((DI.Aux2 >> T2_BRANCH_I1_SHIFT) & 0x01); // I1 = DI->Aux2[14]
            DWORD dwI2 = ((DI.Aux2 >> T2_BRANCH_I2_SHIFT) & 0x01); // I2 = DI->Aux2[12]

            DWORD dwOffset = 0x00000000;        
            dwOffset |= (dwS << T2_BRANCH_OFFSET_S_POS); // dwOffset[24] = S
            dwOffset |= ((!(dwI1 ^ dwS)) << T2_BRANCH_OFFSET_I1_POS); // dwOffset[23] = !(I1 ^ S)
            dwOffset |= ((!(dwI2 ^ dwS)) << T2_BRANCH_OFFSET_I2_POS); // dwOffset[22] = !(I2 ^ S)

            DI.Auxil &= T2_BRANCH_CLEAR_S_MASK; // DI->Auxil[22] = 0 (clearing S bit)
            dwOffset |= DI.Auxil; // dwOffset[21:12] = DI->Auxil[21:12]

            DI.Aux2 &= T2_BRANCH_CLEAR_I_MASK; // Clear I1, I2 (DI->Auxil[14] = DI->Auxil[12] = 0)
            dwOffset |= DI.Aux2; // dwOffset[11:2] = DI->Auxil[11:2]

            // Sign extend offset
            if(dwS != 0)
            {
                dwOffset |= T2_BRANCH_SIGN_EXTEND_MASK;
            }

            if(DI.InstNum == DI_T2_BLX)
            {
                dwNextPc = ((dwCurrentPc + 4) & T2_B_FINAL_MASK) + dwOffset;
            }
            else
            {
                dwNextPc = dwCurrentPc + 4 + dwOffset;
            }
            break;
        }
        case DI_T2_BCOND:
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetFixed;

            DWORD dwS = ((DI.Auxil >> T2_BRANCH_S_SHIFT) & 0x01); // S = DI->Auxil[22]
            DWORD dwJ1 = ((DI.Aux2 >> T2_BRANCH_I1_SHIFT) & 0x01); // I1 = DI->Aux2[14]
            DWORD dwJ2 = ((DI.Aux2 >> T2_BRANCH_I2_SHIFT) & 0x01); // I2 = DI->Aux2[12]
            DWORD dwConditionCodes = 0;
            DWORD dwOffset = 0;        

            dwOffset |= (dwS << T2_BCOND_S_SHIFT); 
            dwOffset |= (dwJ2 << T2_BCOND_I2_SHIFT);
            dwOffset |= (dwJ1 << T2_BCOND_I1_SHIFT);
            
            DI.Auxil &= T2_BRANCH_CLEAR_S_MASK; // (clearing S bit)

            // Store condition codes, then clear from auxil               
            dwConditionCodes = ((DI.Auxil & T2_B_COND_CODE_MASK) >> T2_B_COND_CODE_SHIFT);
            DI.Auxil &= T2_B_CLEAR_COND_CODE;

            dwOffset |= DI.Auxil; // dwOffset[17:12] = DI->Auxil[17:12]

            DI.Aux2 &= T2_BRANCH_CLEAR_I_MASK; // DI->Auxil[14] = DI->Auxil[12] = 0
            dwOffset |= DI.Aux2; // dwOffset[11:2] = DI->Auxil[11:2]

            // Sign extend offset
            if(dwS != 0)
            {
                dwOffset |= T2_BCOND_SIGN_EXTEND_MASK;
            }

            dwNextPc = dwCurrentPc + 4 + dwOffset;

            BOOL fConditionSatisfied = ThumbCheckConditionCodes(dwConditionCodes, dwCurrentPsr);
            if (fConditionSatisfied == TRUE)
            {
                pII->ConditionStatus = csConditionTrue;
            }
            else
            {
                pII->ConditionStatus = csConditionFalse;
            }
            
            break;
        }

        case DI_T2_LDR_IMM:
        case DI_T2_LDR_NEGIMM:
        case DI_T2_LDR_INDEXED:       
        case DI_T2_SUBS:            
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetVariable;
            DWORD dwOffset = DI.Aux2;
            DWORD dwBase = 0;
            DWORD dwAddress = 0;

            dwBase = ARMRegister(DI.Auxil);

            // The next PC is computed differently based on the load variation
            if(DI.InstNum == DI_T2_LDR_IMM)
            {
                dwAddress = dwBase + dwOffset; 
            }
            // Subtracting from LR and loading a negative immediate offset have the 
            // same mask, so we handle both here.
            else if(DI.InstNum == DI_T2_LDR_NEGIMM || DI.InstNum == DI_T2_SUBS)
            {
                dwAddress = dwBase - dwOffset;
            }
            else if(DI.InstNum == DI_T2_LDR_INDEXED)
            {
                DWORD dwAddBit = (dwOffset & T2_LDR_ADD_BIT_MASK); 
                dwOffset &= T2_LDR_OFFSET_MASK; 
                
                if(dwAddBit == 0)   // AddBit = 0 is subtract
                {
                    dwAddress = dwBase - dwOffset;
                }
                else                      // AddBit = 1 is add
                {
                    dwAddress = dwBase + dwOffset;
                }
            }

            ReadEADword(dwAddress, &dwNextPc);
            if ((dwNextPc & 0x01L) == 0x00L) 
            {
                pII->TargetIsThumb = FALSE;
            }
            break;
        }
        
        case DI_T2_LDR_REG_OFFSET:
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetVariable;

            DWORD dwOffsetRegister = (DI.Aux2 & T2_LDR_OFFSET_REG_MASK);
            DWORD dwLeftShift = (DI.Aux2 & T2_LDR_LEFT_SHIFT_MASK) >> T2_LDR_LEFT_SHIFT_SHIFT;
            DWORD dwBaseRegister = (DI.Auxil & T2_LDR_BASE_REG_MASK);

            DWORD dwBase = 0;
            DWORD dwOffset = 0;
            
            dwBase = ARMRegister(dwBaseRegister);            
            dwOffset = ARMRegister(dwOffsetRegister);

            dwOffset = (dwOffset << dwLeftShift);
            
            DWORD dwAddress = dwBase + dwOffset;            

            ReadEADword(dwAddress, &dwNextPc);
            if ((dwNextPc & 0x01L) == 0x00L) 
            {
                pII->TargetIsThumb = FALSE;
            }
            
            break;
        }

        case DI_T2_LDR_PC_RELATIVE:
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetVariable;

            DWORD dwAddBit = DI.Auxil;
            DWORD dwOffset = DI.Aux2; 
            DWORD dwAddress = 0;

            if(dwAddBit==0) // Subtract
            {
                dwAddress = dwCurrentPc - dwOffset;
            }
            else
            {
                dwAddress = dwCurrentPc + dwOffset;
            }

            ReadEADword(dwAddress, &dwNextPc);
            if ((dwNextPc & 0x01L) == 0x00L) 
            {
                pII->TargetIsThumb = FALSE;
            }
            break;
        }        

        case DI_T2_TBB:
        case DI_T2_TBH:
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetVariable;

            DWORD dwTableBaseReg = DI.Auxil;
            DWORD dwTableIndexReg = DI.Aux2;            
        
            DWORD dwTableAddress;
            DWORD dwTableIndex;
            DWORD dwOffset;

            dwTableAddress = ARMRegister(dwTableBaseReg);
            dwTableIndex = ARMRegister(dwTableIndexReg);

            // If dwTableBaseReg is the PC (R15), the branch must add 4
            if(dwTableBaseReg == T2_REG_PC)
            {
                dwTableAddress &= CLEAR_BIT0; 
                dwTableAddress += 4;
            }

            if(DI.InstNum == DI_T2_TBB)
            {
                dwTableAddress += dwTableIndex; // Index is in single bytes
            }
            else
            {
                dwTableAddress += (dwTableIndex*2); // For TBH, index is in halfwords
            }

            ReadEADword(dwTableAddress, &dwOffset);

            // We only want one byte offsets for TBB
            if(DI.InstNum == DI_T2_TBB)
            {
                dwOffset &= 0xF;
            }

            // The most we want is two bytes
            dwOffset &= 0xFF;
            
            // The branch is defined as always being twice the offset listed in the table
            dwOffset *= 2;
            dwNextPc = dwCurrentPc + dwOffset + 4;
            break;
        }

        case DI_T2_CZB:
        {
            pII->IsBranch = TRUE;
            pII->TargetSpecification = tsTargetFixed;

            DWORD dwRegisterContents;
            DWORD dwRegisterNumber;
            DWORD dwN = (DI.Auxil >> T2_CZB_N_SHIFT);

            DWORD dwOffset = ((DI.Auxil & T2_CZB_LOW_OFFSET_MASK) >> T2_CZB_LOW_OFFSET_SHIFT); 
            dwOffset |= ((DI.Auxil & T2_CZB_HI_OFFSET_MASK) >> T2_CZB_HI_OFFSET_SHIFT);           

            dwRegisterNumber = DI.Aux2;

            dwRegisterContents = ARMRegister(dwRegisterNumber);

            // CZB is always based off of PC + 4
            dwNextPc = dwCurrentPc + dwOffset + 4;

            if((dwN == 0 && dwRegisterContents==0) || (dwN!=0 && dwRegisterContents!=0))
            {
                pII->ConditionStatus = csConditionTrue;
            }                
            else
            {
                pII->ConditionStatus = csConditionFalse;
            }            
            break;                
        }
        case DI_T2_IT:
        {
            DWORD dwCondition = DI.Auxil;
            DWORD dwParameters = DI.Aux2;
            BOOL fConditionSatisfied = ThumbCheckConditionCodes(dwCondition, dwCurrentPsr);

            // The first instruction after an IT always corresponds to 
            // the "true" case of the condition specified.  If the condition
            // is false, we need to find the nearest instruction corresponding
            // to the negative case.
            if(fConditionSatisfied == FALSE)
            {
                // Lowest bit of the condition is the highest bit of the parameter field
                DWORD dwITParameters = (((dwCondition & 0x1) << T2_IT_FIRST_PARAM_SHIFT) | dwParameters); 
                DWORD dwTotalLengthOfInstructions = T2_IT_INST_LENGTH;
                dwTotalLengthOfInstructions += Thumb2FindNextITInstruction((dwCurrentPc + dwTotalLengthOfInstructions), dwITParameters, FALSE);
                dwNextPc = dwCurrentPc + dwTotalLengthOfInstructions;      
                pII->IsBranch=TRUE;
                pII->TargetSpecification = tsTargetFixed;
                pII->ConditionStatus = csConditionTrue;
            }
            break;
        }

    }

    // Determine whether this instruction is a return
    pII->IsReturn = pII->PopPc || pII->LR2PC || pII->BXLR;

    // The target offset computed above is the address within_the_
    // process'_memory_space.  The O/S sees it at a different
    // location, but that location is not immediately obvious
    // from the computed address.  The address may point within
    // the same .exe, or it may point into a .dll, which the O/S
    // sees at a different range of addresses.  Let the kd-stub
    // figure out where the O/S sees the address.

    
    if(dwNextPc != 0) 
    {
        // If the new dwNextPc is odd then the target is Thumb mode
        if (dwNextPc & SET_BIT0)
        {
            pII->TargetIsThumb = TRUE;
            dwNextPc &= CLEAR_BIT0;
            
            // Now that the TargetAddr is all figured out, let's see if we're making
            // a call or a branch.
            // The IsCall computation is a bit long-winded.
            pII->IsCall = DetermineIfCall(InstAddr,pII);
        }
        else
        {
            pII->TargetIsThumb = FALSE;

            // If we're in thumb, and the target is ARM, 
            // it must be a call
            pII->IsCall = TRUE;
        }
    }
    else
    {
        pII->IsCall = FALSE;
    }
    
    pII->SequentialAddr = dwSequentialOffset;
    pII->TargetAddr     = dwNextPc;
    
    return (InstructionLength);
}


// Sometimes, Thumb code needs to make a really long branch
// that a normal branch instruction can't perform (i.e., the
// branch is more that 4096 bytes).  To perform it, the code
// will BL to a section of code that reads the target from a
// literal pool and then jump to that target.  This routine
// determines if the code starting at a certain address is one
// of these "long-branch thunks."
#define LBT_CINST    8
#define LBT_TARGET_POS 12    // Target address is 12 bytes from start of thunk.

static WORD
ThumbLBTSignature[LBT_CINST] = {
    0x46E6,        // mov lr,r12
    0xB408,        // push {r3}
    0x4B01,        // ldr r3,[pc+#4]
    0x469C,        // mov r12,r3
    0xBC08,        // pop {r3},
    0x4760,        // bx r12
    0x0000,        // DCD LabOld
    0x0000
};
static WORD
ThumbLBTMask[LBT_CINST] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x0000,0x0000};

static BOOL
ThumbIsLongBranchThunk
    (
    ULONG    StartAddr,
    ULONG    *lpTargetAddr
    )
{
    int InstIndex;
    WORD ThunkCandidate[LBT_CINST];
    DWORD    LBT_TargetOffset;

    ULONG    LbtLength = LBT_CINST * sizeof(WORD);

    *lpTargetAddr = 0;
    if (!ReadThunk(StartAddr, LbtLength, ThunkCandidate))
    {
        return FALSE;
    }

    for(InstIndex=0;InstIndex<LBT_CINST;InstIndex++) {
        if((ThunkCandidate[InstIndex] & ThumbLBTMask[InstIndex]) != ThumbLBTSignature[InstIndex])
            return FALSE;
    }

    // If we get this far, we're in an LBT.

    // If the calling function passed in a pointer for the target address,
    // let's extract the LBT target and fill in the target address.
    if(lpTargetAddr) {
        LBT_TargetOffset = *(DWORD*)((BYTE*)ThunkCandidate + LBT_TARGET_POS);
        *lpTargetAddr = LBT_TargetOffset;
    }

    return TRUE;

}


// It is not always obvious if an instruction is a call.
// We need to examine the BL instruction carefully to
// determine if it's intended as a call, or merely as
// a long branch.

static BOOL
DetermineIfCall
    (
    ULONG               InstAddr,
    ThumbInstInterp*    pII
    )
{
    BOOL fRet = FALSE;
    DWORD CurrOff, TargetOff;
    BOOL IsNextInstruction;
    ULONG ThunkTargetAddr;

    // If it's not a branch, it's surely not a call.
    if (pII->IsBranch)
    {
        CurrOff   = InstAddr;
        TargetOff = pII->TargetAddr;

        if (CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context) == CurrOff)
        {
            IsNextInstruction = TRUE;
        }
        else
        {
            IsNextInstruction = FALSE;
        }

        // If we make a call through anything but a BL,
        // then LR must be filled with the address of the
        // next instruction.  Let's check:
        if(!(pII->IsLinkedBranch))
        {
            if(IsNextInstruction)
            {
                // If the branch is the next instruction, we can
                // cheat a little, and look at the LR to see if
                // it's already set up.
                if(DD_ExceptionState.context->Lr == CurrOff + 2)
                {
                    fRet = TRUE;
                }
                else
                {
                    fRet = FALSE;
                }
            }
            else
            {
                // Otherwise, let's look at the previous instruction.
                // If it's MOV LR,PC, then we're looking at a call.
                WORD PrevInstr;
                ULONG PrevAddr = InstAddr;
                PrevAddr -= 2;

                ReadInstWord(PrevAddr, &PrevInstr);
                if((PrevInstr & 0xFFFF)==0x46FE) // MOV LR,PC
                {
                    fRet = TRUE;
                }
                else
                {
                    fRet = FALSE;
                }
            }
            goto Exit;
        }

        // The other possible call is the BL instruction.  This
        // instruction might be used just to do a long branch, so
        // we need to check if it's really a call.

        // Let's examine the instructions at the target of the BL,
        // to see if there's a long-branch thunk there.  If there
        // is, then we're not making a call, just a branch.

        if(ThumbIsLongBranchThunk(pII->TargetAddr, &ThunkTargetAddr))
        {
            fRet = FALSE;
        }
        else
        {
            // Now, we need to make the best guess that we can.
            // If we performed a BL, but we didn't need to do it for
            // the 22-bits of offset, then we must have done it for
            // the linking, so it's a call.
            DWORD Dist = CurrOff - TargetOff;
            if (Dist>>12 == 0)
            {
                fRet = TRUE;
            }
            else
            {
                // If we get here, we really have no good way
                // of telling for sure.  To be safe, we won't
                // let the user step over.
                fRet = FALSE;
            }
        }
    }
Exit:
    return fRet;
}



BOOL ThumbComputeNextOffset(DWORD *pNextOffset, DWORD *pInstrSize, BOOL *pfIsCall, BOOL *pfIsThumb)
{
    ThumbInstInterp II = { 0 };

    // Default to thumb.   Stuck in thumb.
    *pInstrSize  = ThumbInterpretInstruction(DD_ExceptionState.context->Pc, NULL,&II);
    *pfIsCall    = FALSE;
    *pfIsThumb = TRUE;
    *pNextOffset = II.SequentialAddr;

    if (II.IsBranch && 
        (II.ConditionStatus == csAlwaysTrue || II.ConditionStatus == csConditionTrue))
    {
        // Unconditional branch or conditional branch whose condition matches.
        *pfIsCall    = II.IsCall;
        *pNextOffset = II.TargetAddr;
        *pfIsThumb   = II.TargetIsThumb;
    }

    return TRUE;
}

BOOL ThumbCheckConditionCodes (DWORD dwCond, DWORD dwPsrReg)
{
    BOOL Execute = FALSE;
    BOOL Nset = (dwPsrReg & 0x80000000L) == 0x80000000L;
    BOOL Zset = (dwPsrReg & 0x40000000L) == 0x40000000L;
    BOOL Cset = (dwPsrReg & 0x20000000L) == 0x20000000L;
    BOOL Vset = (dwPsrReg & 0x10000000L) == 0x10000000L;
    switch (dwCond >> 1)
    {
        case 0: Execute = Zset; break;
        case 1:    Execute = Cset; break;
        case 2:    Execute = Nset; break;
        case 3: Execute = Vset;    break;
        case 4: Execute = (Cset && !Zset);break;
        case 5: Execute = (Nset==Vset);    break;
        case 6: Execute = (Nset==Vset && !Zset); break;
        // The always / never condition codes are not legal in
        // Thumb.
        case 7:    return FALSE; break;
    }
    if (dwCond & 0x01) Execute = !Execute;

    return Execute;
}

