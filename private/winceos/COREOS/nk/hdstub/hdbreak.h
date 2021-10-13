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

const DWORD BPMax = 256;



struct BreakpointImpl : public Breakpoint
{   
    HRESULT GetState(enum BreakpointState *stat);
    HRESULT SetState(enum BreakpointState stat);
    HRESULT ReadInstruction();
    HRESULT WriteInstruction(BYTE*, BYTE*);
    HRESULT CopyPageOnWrite();
    HRESULT FreeCopiedPage();
    HRESULT Enable();
    HRESULT Disable();

    static BYTE RefCount[BPMax];
    static BreakpointImpl Breakpoints[BPMax];
    static BreakpointImpl *CurrentlyDisabledBreakpoint;
    static BOOL fDisableAllOnHalt;
    
    static BreakpointImpl *Find(MEMADDR *Address, ULONG ThreadID, BOOL SixteenBit);
    static BreakpointImpl *New(MEMADDR *Address, ULONG ThreadID, BOOL SixteenBit);
    static void Reset();
    static BOOL OnPageIn(DWORD PageAddr, DWORD NumPages, BOOL Writeable);

    // CPU Specific constants.
    // TODO: Add support for multiple breakpoint instruction types.
    // ARM supports multiple breakpoint instructions! (3 currently)
    static const BYTE CpuBreakSize;
    static const BYTE CpuBreakSize16;
    static const ULONG CpuBreak;
    static const ULONG CpuBreak16;

    static HRESULT Add(MEMADDR *Address, ULONG ThreadID, BOOL SixteenBit, Breakpoint **handle);
    static HRESULT Delete(Breakpoint *handle);
    static HRESULT EnableAll();
    static HRESULT Enable(Breakpoint *);
    static HRESULT DisableAll();
    static HRESULT Disable(Breakpoint *);
    static HRESULT WriteBackDisabled();
    static HRESULT Find(MEMADDR *, Breakpoint *, Breakpoint **);
#ifdef DEBUG
    static HRESULT DumpBreakpoint(Breakpoint *);
#endif
    static BOOL DoSanitize(void *buffer, MEMADDR const *Addr, ULONG nSize, BOOL fAlwaysCopy, BOOL fExternal);
    static void SetDisableAllOnHalt(BOOL fDisable);
    static BOOL DisableAllOnHalt();
    static HRESULT SetState(Breakpoint *bp, enum BreakpointState state);
    static HRESULT GetState(Breakpoint *bp, enum BreakpointState *state);
    static void Sanitize(void *buffer, MEMADDR const *Addr, ULONG nSize);
};



struct ROMMap
{
    DWORD dwPFN;                //physical ROM page the bp is on.  
    PPROCESS VM;                  // VM of breakpoint
    void *VA;                          // Virtual address of Page that is mapped in (important if DLL!)
    void *MappedPageAddr;         // Cloned from this page.
    DWORD dwMappedPagePFN;  // Cloned page's PFN (for convenience-this can be calculate via GetPFNOfProcess)
    PTENTRY OriginalEntry;    // Original PTENTRY.  (Easier to restore)
    ULONG RefCount;              // Number of VM Breakpoints referencing this remap.

    void AddRef();
    HRESULT PatchVM();
    void Release();
    HRESULT RestoreVM();

    static const ULONG REMAP_PAGES = 16;    // 16*4K = 64K
    static ROMMap Entries[REMAP_PAGES];

    static BOOL Init();
    static void DeInit();
    static HRESULT CopyPage(PPROCESS vm, void *va);
    static HRESULT FreePage(PPROCESS vm, void *va);
    static ROMMap *FindPhysPage(PPROCESS vm, void *va);
    static ROMMap *New(PPROCESS vm, void *va, DWORD dwPFN);
};


BOOL OnEmbeddedBreakpoint();
BOOL Currently16Bit();

