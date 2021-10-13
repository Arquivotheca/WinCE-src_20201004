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

#pragma once

struct SingleStep
{
    DWORD thrid;                // Thread affinity!
    Breakpoint *bp;             // Just in case step uses bp.
    Breakpoint *safetybp;       // Just in case bp doesn't match.
    DWORD quantum;              // ...
    BYTE bprio;                 // ...
    BYTE cprio;                 // ...
    BYTE inuse : 1;         // in use.
    BYTE atomic : 1;        // Remember to thaw
    BYTE run : 1;           // continue after step finishes.

    Breakpoint *restorebp;      // For "soar"ing        
    DWORD dwDBPHandle;
    DWORD dwAddress;
    PPROCESS pVM;        

private:
    HRESULT CpuSetup();
    void DumpState();

    HRESULT New(BOOL);     
    void Delete();

    HRESULT AddCBP(BOOL fRun);
    HRESULT AddDBP(BOOL fRun);
    void RestoreCBP();
    void RestoreDBP();

    PPROCESS OwnerProc();

    HRESULT SetBP(void *addr, BOOL f16Bit, Breakpoint **ppbp);
    
private:
    static HRESULT AllocSingleStep(DWORD *dwIndex);

public:
    static const DWORD MAX_STEP = 10;
    static SingleStep stepStates[MAX_STEP];

    static void SimulateDebugBreakExecution(void);
    static HRESULT GetNextPC(ULONG *);
    static BOOL IsPSLCall(ULONG Offset);
    static void DeleteAll ();
    static SingleStep *Find (DWORD dwThreadID);

    static HRESULT StepThread();
    static HRESULT StepCBP(BOOL fRun);
    static HRESULT StepDBP(BOOL fRun);

    static BOOL Trap(EXCEPTION_RECORD *, CONTEXT *, BOOL, BOOL *);
    static BOOL ModuleUnload(DWORD dwModuleHandle);
};


BOOL ReadByte(ULONG, BYTE*);
BOOL ReadWord(ULONG, WORD*);
BOOL ReadDword(ULONG, DWORD *);
BOOL ReadMem(ULONG, void *, ULONG);