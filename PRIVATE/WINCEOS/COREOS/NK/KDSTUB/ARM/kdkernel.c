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

    kdkernel.c

Abstract:

    This module contains code that somewhat emulates KiDispatchException

--*/

#include "kdp.h"

// Functions to grab WMMX registers
extern ULONGLONG GetWr0();
extern ULONGLONG GetWr1();
extern ULONGLONG GetWr2();
extern ULONGLONG GetWr3();
extern ULONGLONG GetWr4();
extern ULONGLONG GetWr5();
extern ULONGLONG GetWr6();
extern ULONGLONG GetWr7();
extern ULONGLONG GetWr8();
extern ULONGLONG GetWr9();
extern ULONGLONG GetWr10();
extern ULONGLONG GetWr11();
extern ULONGLONG GetWr12();
extern ULONGLONG GetWr13();
extern ULONGLONG GetWr14();
extern ULONGLONG GetWr15();

// Function to set the WMMX registers
extern void SetWr0(ULONGLONG);
extern void SetWr1(ULONGLONG);
extern void SetWr2(ULONGLONG);
extern void SetWr3(ULONGLONG);
extern void SetWr4(ULONGLONG);
extern void SetWr5(ULONGLONG);
extern void SetWr6(ULONGLONG);
extern void SetWr7(ULONGLONG);
extern void SetWr8(ULONGLONG);
extern void SetWr9(ULONGLONG);
extern void SetWr10(ULONGLONG);
extern void SetWr11(ULONGLONG);
extern void SetWr12(ULONGLONG);
extern void SetWr13(ULONGLONG);
extern void SetWr14(ULONGLONG);
extern void SetWr15(ULONGLONG);

// Functions to grab WMMX registers
extern DWORD GetConReg0();
extern DWORD GetConReg1();
extern DWORD GetConReg2();
extern DWORD GetConReg3();
extern DWORD GetConReg4();
extern DWORD GetConReg5();
extern DWORD GetConReg6();
extern DWORD GetConReg7();
extern DWORD GetConReg8();
extern DWORD GetConReg9();
extern DWORD GetConReg10();
extern DWORD GetConReg11();
extern DWORD GetConReg12();
extern DWORD GetConReg13();
extern DWORD GetConReg14();
extern DWORD GetConReg15();

// Function to set the WMMX registers
extern void SetConReg0(DWORD);
extern void SetConReg1(DWORD);
extern void SetConReg2(DWORD);
extern void SetConReg3(DWORD);
extern void SetConReg4(DWORD);
extern void SetConReg5(DWORD);
extern void SetConReg6(DWORD);
extern void SetConReg7(DWORD);
extern void SetConReg8(DWORD);
extern void SetConReg9(DWORD);
extern void SetConReg10(DWORD);
extern void SetConReg11(DWORD);
extern void SetConReg12(DWORD);
extern void SetConReg13(DWORD);
extern void SetConReg14(DWORD);
extern void SetConReg15(DWORD);


void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx)
{
    pCtx->R0 = pCpuCtx->R0;
    pCtx->R1 = pCpuCtx->R1;
    pCtx->R2 = pCpuCtx->R2;
    pCtx->R3 = pCpuCtx->R3;
    pCtx->R4 = pCpuCtx->R4;
    pCtx->R5 = pCpuCtx->R5;
    pCtx->R6 = pCpuCtx->R6;
    pCtx->R7 = pCpuCtx->R7;
    pCtx->R8 = pCpuCtx->R8;
    pCtx->R9 = pCpuCtx->R9;
    pCtx->R10 = pCpuCtx->R10;
    pCtx->R11 = pCpuCtx->R11;
    pCtx->R12 = pCpuCtx->R12;
    pCtx->Sp = pCpuCtx->Sp;
    pCtx->Lr = pCpuCtx->Lr;
    pCtx->Pc = pCpuCtx->Pc;
    pCtx->Psr = pCpuCtx->Psr;
    pCtx->ContextFlags = CONTEXT_FULL;
}


// An escape mechanism to allow an user to override the autodetection below
BOOL g_fOverrideWMMXDetection = FALSE;
BOOL g_fWMMXPresent = FALSE;

BOOL HasWMMX()
{
    BOOL fRet;
    fRet = FALSE;
    
    if(g_fOverrideWMMXDetection)
        fRet = g_fWMMXPresent;
    else
        fRet = KdpIsProcessorFeaturePresent(PF_ARM_INTEL_WMMX);

    DEBUGGERMSG(KDZONE_INTELWMMX, (L"--HasWMMX: %d\r\n", fRet));
    return fRet;
}


// Call a bunch of assembler stub functions to copy registers to the concan_regs structure
void GetWMMXRegisters (CONCAN_REGS *pRegs)
{
    pRegs->Wregs[0] = GetWr0();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR0 -> %I64x\r\n", pRegs->Wregs[0]));
    pRegs->Wregs[1] = GetWr1();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR1 -> %I64x\r\n", pRegs->Wregs[1]));
    pRegs->Wregs[2] = GetWr2();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR2 -> %I64x\r\n", pRegs->Wregs[2]));
    pRegs->Wregs[3] = GetWr3();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR3 -> %I64x\r\n", pRegs->Wregs[3]));
    pRegs->Wregs[4] = GetWr4();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR4 -> %I64x\r\n", pRegs->Wregs[4]));
    pRegs->Wregs[5] = GetWr5();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR5 -> %I64x\r\n", pRegs->Wregs[5]));
    pRegs->Wregs[6] = GetWr6();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR6 -> %I64x\r\n", pRegs->Wregs[6]));
    pRegs->Wregs[7] = GetWr7();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR7 -> %I64x\r\n", pRegs->Wregs[7]));
    pRegs->Wregs[8] = GetWr8();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR8 -> %I64x\r\n", pRegs->Wregs[8]));
    pRegs->Wregs[9] = GetWr9();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR9 -> %I64x\r\n", pRegs->Wregs[9]));
    pRegs->Wregs[10] = GetWr10();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR10 -> %I64x\r\n", pRegs->Wregs[10]));
    pRegs->Wregs[11] = GetWr11();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR11 -> %I64x\r\n", pRegs->Wregs[11]));
    pRegs->Wregs[12] = GetWr12();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR12 -> %I64x\r\n", pRegs->Wregs[12]));
    pRegs->Wregs[13] = GetWr13();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR13 -> %I64x\r\n", pRegs->Wregs[13]));
    pRegs->Wregs[14] = GetWr14();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR14 -> %I64x\r\n", pRegs->Wregs[14]));
    pRegs->Wregs[15] = GetWr15();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR15 -> %I64x\r\n", pRegs->Wregs[15]));

    pRegs->ConRegs[0] = GetConReg0();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCID -> %x\r\n", pRegs->ConRegs[0]));
    pRegs->ConRegs[1] = GetConReg1();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCon -> %x\r\n", pRegs->ConRegs[1]));
    pRegs->ConRegs[2] = GetConReg2();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCSSF -> %x\r\n", pRegs->ConRegs[2]));
    pRegs->ConRegs[3] = GetConReg3();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCASF -> %x\r\n", pRegs->ConRegs[3]));

    // Reserved registers - do nothing with them
#if 0
    pRegs->ConRegs[4] = GetConReg4();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved4 -> %x\r\n", pRegs->ConRegs[4]));
    pRegs->ConRegs[5] = GetConReg5();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved5 -> %x\r\n", pRegs->ConRegs[5]));
    pRegs->ConRegs[6] = GetConReg6();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved6 -> %x\r\n", pRegs->ConRegs[6]));
    pRegs->ConRegs[7] = GetConReg7();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved7 -> %x\r\n", pRegs->ConRegs[7]));
#endif

    pRegs->ConRegs[8] = GetConReg8();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR0 -> %x\r\n", pRegs->ConRegs[8]));
    pRegs->ConRegs[9] = GetConReg9();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR1 -> %x\r\n", pRegs->ConRegs[9]));
    pRegs->ConRegs[10] = GetConReg10();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR2 -> %x\r\n", pRegs->ConRegs[10]));
    pRegs->ConRegs[11] = GetConReg11();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR3 -> %x\r\n", pRegs->ConRegs[11]));

    // Reserved registers - do nothing with them.
#if 0
    pRegs->ConRegs[12] = GetConReg12();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved12 -> %x\r\n", pRegs->ConRegs[12]));
    pRegs->ConRegs[13] = GetConReg13();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved13 -> %x\r\n", pRegs->ConRegs[13]));
    pRegs->ConRegs[14] = GetConReg14();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved14 -> %x\r\n", pRegs->ConRegs[14]));
    pRegs->ConRegs[15] = GetConReg15();
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved15 -> %x\r\n", pRegs->ConRegs[15]));
#endif
}


// Call a bunch of assembler stub functions to copy concan_regs to the actual registers.
void SetWMMXRegisters (CONCAN_REGS *pRegs)
{
    SetWr0(pRegs->Wregs[0]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR0 <- %I64x\r\n", pRegs->Wregs[0]));
    SetWr1(pRegs->Wregs[1]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR1 <- %I64x\r\n", pRegs->Wregs[1]));
    SetWr2(pRegs->Wregs[2]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR2 <- %I64x\r\n", pRegs->Wregs[2]));
    SetWr3(pRegs->Wregs[3]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR3 <- %I64x\r\n", pRegs->Wregs[3]));
    SetWr4(pRegs->Wregs[4]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR4 <- %I64x\r\n", pRegs->Wregs[4]));
    SetWr5(pRegs->Wregs[5]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR5 <- %I64x\r\n", pRegs->Wregs[5]));
    SetWr6(pRegs->Wregs[6]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR6 <- %I64x\r\n", pRegs->Wregs[6]));
    SetWr7(pRegs->Wregs[7]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR7 <- %I64x\r\n", pRegs->Wregs[7]));
    SetWr8(pRegs->Wregs[8]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR8 <- %I64x\r\n", pRegs->Wregs[8]));
    SetWr9(pRegs->Wregs[9]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR9 <- %I64x\r\n", pRegs->Wregs[9]));
    SetWr10(pRegs->Wregs[10]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR10 <- %I64x\r\n", pRegs->Wregs[10]));
    SetWr11(pRegs->Wregs[11]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR11 <- %I64x\r\n", pRegs->Wregs[11]));
    SetWr12(pRegs->Wregs[12]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR12 <- %I64x\r\n", pRegs->Wregs[12]));
    SetWr13(pRegs->Wregs[13]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR13 <- %I64x\r\n", pRegs->Wregs[13]));
    SetWr14(pRegs->Wregs[14]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR14 <- %I64x\r\n", pRegs->Wregs[14]));
    SetWr15(pRegs->Wregs[15]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wR15 <- %I64x\r\n", pRegs->Wregs[15]));

    // the ID register is read only.  Do not bother to set it at all.
#if 0
    SetConReg0(pRegs->ConRegs[0]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCID <- %x\r\n", pRegs->ConRegs[0]));
#endif
    SetConReg1(pRegs->ConRegs[1]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCon <- %x\r\n", pRegs->ConRegs[1]));
    SetConReg2(pRegs->ConRegs[2]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCSSF <- %x\r\n", pRegs->ConRegs[2]));

    // Arithmetic flags really shouldn't be written to.  Consider them read only.
#if 0
    SetConReg3(pRegs->ConRegs[3]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCASF <- %x\r\n", pRegs->ConRegs[3]));
#endif

    // Do nothing with the reserved registers
#if 0
    SetConReg4(pRegs->ConRegs[4]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved4 <- %x\r\n", pRegs->ConRegs[4]));
    SetConReg5(pRegs->ConRegs[5]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved5 <- %x\r\n", pRegs->ConRegs[5]));
    SetConReg6(pRegs->ConRegs[6]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved6 <- %x\r\n", pRegs->ConRegs[6]));
    SetConReg7(pRegs->ConRegs[7]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved7 <- %x\r\n", pRegs->ConRegs[7]));
#endif

    SetConReg8(pRegs->ConRegs[8]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR0 <- %x\r\n", pRegs->ConRegs[8]));
    SetConReg9(pRegs->ConRegs[9]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR1 <- %x\r\n", pRegs->ConRegs[9]));
    SetConReg10(pRegs->ConRegs[10]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR2 <- %x\r\n", pRegs->ConRegs[10]));
    SetConReg11(pRegs->ConRegs[11]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"wCGR3 <- %x\r\n", pRegs->ConRegs[11]));

    // Do nothing with the reserved registers
#if 0
    SetConReg12(pRegs->ConRegs[12]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved12 <- %x\r\n", pRegs->ConRegs[12]));
    SetConReg13(pRegs->ConRegs[13]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved13 <- %x\r\n", pRegs->ConRegs[13]));
    SetConReg14(pRegs->ConRegs[14]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved14 <- %x\r\n", pRegs->ConRegs[14]));
    SetConReg15(pRegs->ConRegs[15]);
    DEBUGGERMSG(KDZONE_INTELWMMX, (L"Reserved15 <- %x\r\n", pRegs->ConRegs[15]));
#endif
}
