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

#include "osaxs_p.h"
#include <armintr.h>

// Functions to grab WMMX registers

extern "C" {

ULONGLONG GetWr0();
ULONGLONG GetWr1();
ULONGLONG GetWr2();
ULONGLONG GetWr3();
ULONGLONG GetWr4();
ULONGLONG GetWr5();
ULONGLONG GetWr6();
ULONGLONG GetWr7();
ULONGLONG GetWr8();
ULONGLONG GetWr9();
ULONGLONG GetWr10();
ULONGLONG GetWr11();
ULONGLONG GetWr12();
ULONGLONG GetWr13();
ULONGLONG GetWr14();
ULONGLONG GetWr15();

// Function to set the WMMX registers
void SetWr0(ULONGLONG);
void SetWr1(ULONGLONG);
void SetWr2(ULONGLONG);
void SetWr3(ULONGLONG);
void SetWr4(ULONGLONG);
void SetWr5(ULONGLONG);
void SetWr6(ULONGLONG);
void SetWr7(ULONGLONG);
void SetWr8(ULONGLONG);
void SetWr9(ULONGLONG);
void SetWr10(ULONGLONG);
void SetWr11(ULONGLONG);
void SetWr12(ULONGLONG);
void SetWr13(ULONGLONG);
void SetWr14(ULONGLONG);
void SetWr15(ULONGLONG);
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
        fRet = IsProcessorFeaturePresent(PF_ARM_INTEL_WMMX);

    return fRet;
}


// Call a bunch of assembler stub functions to copy registers to the concan_regs structure
void GetWMMXRegisters (CONCAN_REGS *pRegs)
{
    pRegs->Wregs[0] = GetWr0();
    pRegs->Wregs[1] = GetWr1();
    pRegs->Wregs[2] = GetWr2();
    pRegs->Wregs[3] = GetWr3();
    pRegs->Wregs[4] = GetWr4();
    pRegs->Wregs[5] = GetWr5();
    pRegs->Wregs[6] = GetWr6();
    pRegs->Wregs[7] = GetWr7();
    pRegs->Wregs[8] = GetWr8();
    pRegs->Wregs[9] = GetWr9();
    pRegs->Wregs[10] = GetWr10();
    pRegs->Wregs[11] = GetWr11();
    pRegs->Wregs[12] = GetWr12();
    pRegs->Wregs[13] = GetWr13();
    pRegs->Wregs[14] = GetWr14();
    pRegs->Wregs[15] = GetWr15();

    pRegs->ConRegs[0] = _MoveFromCoprocessor(1, 0, 0, 0, 0);
    pRegs->ConRegs[1] = _MoveFromCoprocessor(1, 0, 1, 0, 0);
    pRegs->ConRegs[2] = _MoveFromCoprocessor(1, 0, 2, 0, 0);
    pRegs->ConRegs[3] = _MoveFromCoprocessor(1, 0, 3, 0, 0);

    // 4 - 7 reserved.

    pRegs->ConRegs[8] = _MoveFromCoprocessor(1, 0, 8, 0, 0);
    pRegs->ConRegs[9] = _MoveFromCoprocessor(1, 0, 9, 0, 0);
    pRegs->ConRegs[10] = _MoveFromCoprocessor(1, 0, 10, 0, 0);
    pRegs->ConRegs[11] = _MoveFromCoprocessor(1, 0, 11, 0, 0);

    // 12 - 15 reserved.
}


// Call a bunch of assembler stub functions to copy concan_regs to the actual registers.
void SetWMMXRegisters (CONCAN_REGS *pRegs)
{
    SetWr0(pRegs->Wregs[0]);
    SetWr1(pRegs->Wregs[1]);
    SetWr2(pRegs->Wregs[2]);
    SetWr3(pRegs->Wregs[3]);
    SetWr4(pRegs->Wregs[4]);
    SetWr5(pRegs->Wregs[5]);
    SetWr6(pRegs->Wregs[6]);
    SetWr7(pRegs->Wregs[7]);
    SetWr8(pRegs->Wregs[8]);
    SetWr9(pRegs->Wregs[9]);
    SetWr10(pRegs->Wregs[10]);
    SetWr11(pRegs->Wregs[11]);
    SetWr12(pRegs->Wregs[12]);
    SetWr13(pRegs->Wregs[13]);
    SetWr14(pRegs->Wregs[14]);
    SetWr15(pRegs->Wregs[15]);

    // 0 (id) is readonly

    _MoveToCoprocessor(pRegs->ConRegs[1], 1, 0, 1, 0, 0);
    _MoveToCoprocessor(pRegs->ConRegs[2], 1, 0, 2, 0, 0);

    // 3 (flags) readonly

    // 4 - 7 reserved
    _MoveToCoprocessor(pRegs->ConRegs[8], 1, 0, 8, 0, 0);
    _MoveToCoprocessor(pRegs->ConRegs[9], 1, 0, 9, 0, 0);
    _MoveToCoprocessor(pRegs->ConRegs[10], 1, 0, 10, 0, 0);
    _MoveToCoprocessor(pRegs->ConRegs[11], 1, 0, 11, 0, 0);

    // 12 - 15 reserved
}
