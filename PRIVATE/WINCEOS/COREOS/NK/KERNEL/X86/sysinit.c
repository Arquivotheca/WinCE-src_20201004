//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <kernel.h>

extern void KernelFindMemory(void);
extern void InitializePageTables(void);

// This table is NOT const data since it is edited to initialize the TSS entries.
unsigned __int64 g_aGlobalDescriptorTable[] = {
    0,
    0x00CF9A000000FFFF,         // Ring 0 code, Limit = 4G
    0x00CF92000000FFFF,         // Ring 0 data, Limit = 4G
    0x00CFBA000000FFFF,         // Ring 1 code, Limit = 4G
    0x00CFB2000000FFFF,         // Ring 1 data, Limit = 4G
    0x00CFDA000000FFFF,         // Ring 2 code, Limit = 4G
    0x00CFD2000000FFFF,         // Ring 2 data, Limit = 4G
    0x00CFFA000000FFFF,         // Ring 3 code, Limit = 4G
    0x00CFF2000000FFFF,         // Ring 3 data, Limit = 4G
    0,                          // Will be main TSS
    0,                          // Will be NMI TSS
    0,                          // Will be Double Fault TSS
    0x0040F20000000000+FS_LIMIT,    // PCR selector
    0x00CFBE000000FFFF,         // Ring 1 (conforming) code, Limit = 4G
};

#pragma pack(push, 1)           // We want this structure packed exactly as declared!

typedef struct {
    BYTE        PushEax;
    BYTE        Pushad;
    BYTE        MovEsi;
    FARPROC     pfnHandler;
    BYTE        JmpNear;
    DWORD       JmpOffset;
    DWORD       Padding;
} INTHOOKSTUB, * PINTHOOKSTUB;

INTHOOKSTUB g_aIntHookStubs[32] = {0};

typedef struct {
    USHORT Size;
    PVOID Base;
} FWORDPtr;

const FWORDPtr g_KernelGDTBase = {sizeof(g_aGlobalDescriptorTable)-1, &g_aGlobalDescriptorTable };


KIDTENTRY g_aIntDescTable[MAXIMUM_IDTVECTOR+1];

const FWORDPtr g_IDTBase = {sizeof(g_aIntDescTable), &g_aIntDescTable };

#pragma pack(pop)

KTSS MainTSS;
KTSS DoubleTSS;
KTSS NMITSS;

BYTE TASKStack[0x800];      

char SyscallStack[128];     // temporary stack for syscall trap

void Int1Fault(void);
void Int2Fault(void);
void Int3Fault(void);
void GeneralFault(void);
void PageFault(void);
void InvalidOpcode(void);
void ZeroDivide(void);
void CommonIntDispatch(void);
void OEMNMIHandler(void);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DumpTSS(
    KTSS *ptss
    ) 
{
    NKDbgPrintfW(L"TSS=%8.8lx EIP=%8.8lx Flags=%8.8lx\r\n",
            ptss, ptss->Eip, ptss->Eflags);
    NKDbgPrintfW(L"Eax=%8.8lx Ebx=%8.8lx Ecx=%8.8lx Edx=%8.8lx\r\n",
            ptss->Eax, ptss->Ebx, ptss->Ecx, ptss->Edx);
    NKDbgPrintfW(L"Esi=%8.8lx Edi=%8.8lx Ebp=%8.8lx Esp=%8.8lx\r\n",
            ptss->Esi, ptss->Edi, ptss->Ebp, ptss->Esp);
    NKDbgPrintfW(L"CS=%4.4lx DS=%4.4lx ES=%4.4lx SS=%4.4lx FS=%4.4lx GS=%4.4lx\r\n",
            ptss->Cs, ptss->Ds, ptss->Es, ptss->Ss, ptss->Fs, ptss->Gs);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
NonMaskableInterrupt(void) 
{
    KData.cNest = -1;
    NKDbgPrintfW(L"\r\nNMI -- backlink=%4.4x\r\n", NMITSS.Backlink);
    DumpTSS(&MainTSS);
    OEMNMIHandler();
    __asm {
        cli
        hlt
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoubleFault(void) 
{
    KData.cNest = -1;
    NKDbgPrintfW(L"\r\nDouble Fault -- backlink=%4.4x\r\n", DoubleTSS.Backlink);
    DumpTSS(&MainTSS);
    __asm {
        cli
        hlt
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitTSSSelector(
    ULONG uSelector,
    PKTSS pTSS
    ) 
{
    PKGDTENTRY pTSSSel = (PKGDTENTRY)(((ULONG)&g_aGlobalDescriptorTable[0]) + uSelector);
    pTSSSel->LimitLow = sizeof(KTSS)-1;
    pTSSSel->BaseLow = (USHORT)((DWORD)pTSS) & 0xFFFF;
    pTSSSel->HighWord.Bytes.BaseMid = (UCHAR)(((DWORD)pTSS) >> 16) & 0xFF;
    pTSSSel->HighWord.Bytes.BaseHi = (UCHAR)(((DWORD)pTSS) >> 24) & 0xFF;
    pTSSSel->HighWord.Bits.Type = TYPE_TSS;
    pTSSSel->HighWord.Bits.Pres = 1;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitializeTaskStates(void) 
{
    ulong pageDir;
    __asm {
        mov eax, CR3
        mov pageDir, eax
    }
    InitTSSSelector(KGDT_MAIN_TSS, &MainTSS);

    MainTSS.Esp0 = 0xB00B00;
    MainTSS.Ss0 = KGDT_R0_DATA;
    MainTSS.Esp1 = (ulong)(SyscallStack+sizeof(SyscallStack));
    MainTSS.Ss1 = KGDT_R1_DATA | 1;

    InitTSSSelector(KGDT_NMI_TSS, &NMITSS);
    NMITSS.Eip = (ULONG)&NonMaskableInterrupt;
    NMITSS.Cs = KGDT_R0_CODE;
    NMITSS.Ss = KGDT_R0_DATA;
    NMITSS.Ds = KGDT_R0_DATA;
    NMITSS.Es = KGDT_R0_DATA;
    NMITSS.Ss0 = KGDT_R0_DATA;
    NMITSS.Esp = NMITSS.Esp0 = ((ULONG)&TASKStack) + sizeof(TASKStack);
    NMITSS.CR3 = pageDir;

    InitTSSSelector(KGDT_DOUBLE_TSS, &DoubleTSS);
    DoubleTSS.Eip = (ULONG)&DoubleFault;
    DoubleTSS.Cs = KGDT_R0_CODE;
    DoubleTSS.Ss = KGDT_R0_DATA;
    DoubleTSS.Ds = KGDT_R0_DATA;
    DoubleTSS.Es = KGDT_R0_DATA;
    DoubleTSS.Ss0 = KGDT_R0_DATA;
    DoubleTSS.Esp = DoubleTSS.Esp0 = ((ULONG)&TASKStack) + sizeof(TASKStack);
    DoubleTSS.CR3 = pageDir;
 
    // Change the KGDT_EMX87 entry in the global descriptor table to be Ring 3 conforming
    // if KFLAG_NOTALLKMODE set (not all running in K-mode), else keep at Ring 1
    if (pTOC->ulKernelFlags & KFLAG_NOTALLKMODE)
    {
        PKGDTENTRY pTSSSel = (PKGDTENTRY)(((ULONG)&g_aGlobalDescriptorTable[0]) + KGDT_EMX87);
        pTSSSel->HighWord.Bits.Dpl = 3;
    }

    _asm {
        mov     eax, KGDT_MAIN_TSS
        ltr     ax
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitIDTEntry(
    int i,
    USHORT usSelector,
    PVOID pFaultHandler,
    USHORT usGateType
    ) 
{
    PKIDTENTRY pCur = &g_aIntDescTable[i];
    pCur->Offset = LOWORD((DWORD)pFaultHandler);
    pCur->ExtendedOffset = HIWORD((DWORD)pFaultHandler);
    pCur->Selector = usSelector;
    pCur->Access = usGateType;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitializeIDT(void) 
{
    InitIDTEntry(0x00, KGDT_R0_CODE, ZeroDivide, INTERRUPT_GATE);
    InitIDTEntry(0x01, KGDT_R0_CODE, Int1Fault, RING3_INT_GATE);
    InitIDTEntry(0x02, KGDT_NMI_TSS, 0, TASK_GATE);
    InitIDTEntry(0x02, KGDT_R0_CODE, Int2Fault, INTERRUPT_GATE);
    InitIDTEntry(0x03, KGDT_R0_CODE, Int3Fault, RING3_INT_GATE);
    InitIDTEntry(0x06, KGDT_R0_CODE, InvalidOpcode, INTERRUPT_GATE);
    InitIDTEntry(0x08, KGDT_DOUBLE_TSS, 0, TASK_GATE);
    InitIDTEntry(0x0D, KGDT_R0_CODE, GeneralFault, INTERRUPT_GATE);
    InitIDTEntry(0x0E, KGDT_R0_CODE, PageFault, INTERRUPT_GATE);
    _asm { lidt [g_IDTBase] };
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    if (hwInterruptNumber < ARRAY_SIZE(g_aIntDescTable)) {
        PKIDTENTRY pIDTEntry = &g_aIntDescTable[hwInterruptNumber];
        if (pIDTEntry->Offset == 0 && pIDTEntry->Selector == 0) {
            PINTHOOKSTUB pStub = &g_aIntHookStubs[0];
            for (pStub = &g_aIntHookStubs[0]; pStub <= LAST_ELEMENT(g_aIntHookStubs); pStub++) {
                if (pStub->pfnHandler == NULL) {
                    pStub->PushEax   = 0x50;
                    pStub->Pushad    = 0x60;
                    pStub->MovEsi    = 0xBE;
                    pStub->pfnHandler= pfnHandler;
                    pStub->JmpNear   = 0xE9;
                    pStub->JmpOffset = ((DWORD)&CommonIntDispatch) - ((DWORD)(&pStub->JmpOffset) + 4);
                    InitIDTEntry(hwInterruptNumber, KGDT_R0_CODE, pStub, INTERRUPT_GATE);
                    return(TRUE);
                }
            }
        }
    }
    return(FALSE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UnhookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    if (hwInterruptNumber < ARRAY_SIZE(g_aIntDescTable)) {
        PKIDTENTRY pIDTEntry = &g_aIntDescTable[hwInterruptNumber];
        PINTHOOKSTUB pStub = (PINTHOOKSTUB)(pIDTEntry->Offset + (pIDTEntry->ExtendedOffset << 16));
        if (pStub >= &g_aIntHookStubs[0] && pStub <= LAST_ELEMENT(g_aIntHookStubs)) {
            InitIDTEntry(hwInterruptNumber, 0, 0, 0);
            pStub->pfnHandler = NULL;
            return(TRUE);
        }
    }
    return(FALSE);
}

PPTE        g_pOEMAddressTable;
PPAGETABLE  g_pPageDir;

DWORD       g_dwRAMOfstVA2PA = 0x80000000;

static int FindRAMEntry (PPTE pOEMAddressTable)
{
    int i;
    DWORD dwRAMAddr = pTOC->ulRAMFree;
    for (i = 0; pOEMAddressTable[i].dwSize; i ++) {
        if ((dwRAMAddr >= pOEMAddressTable[i].dwVA)
            && (dwRAMAddr < (pOEMAddressTable[i].dwVA + pOEMAddressTable[i].dwSize))) {
            g_dwRAMOfstVA2PA = pOEMAddressTable[i].dwPA - pOEMAddressTable[i].dwVA;
            break;
        }
    }
    return i;
}

//
// mapping extra ram using 4M pages
//
static void MapExtraRAM4M (DWORD dwRamStart, DWORD dwRamEnd)
{
    DWORD idxPDE   = dwRamStart >> 22;                // index to 1st cached PDE entry
    DWORD idxUCPDE = idxPDE + 0x80;                   // index to 1st uncached PDE entry
    DWORD nEntries = (dwRamEnd - dwRamStart) >> 22;   // # of PDE entries needed

    DEBUGCHK (idxPDE > 0);

    for ( ; nEntries --; idxPDE ++, idxUCPDE ++) {
        g_pPageDir->PTE[idxPDE] = g_pPageDir->PTE[idxPDE-1] + 0x400000;     // cached entry
        g_pPageDir->PTE[idxUCPDE] = g_pPageDir->PTE[idxUCPDE-1] + 0x400000; // uncached entry
    }
}

//
// mapping extra ram using 4K pages
//
static void MapExtraRAM4K (DWORD dwRamStart, DWORD dwRamEnd)
{
    DWORD idxPDE   = dwRamStart >> 22;                // index to 1st cached PDE entry
    DWORD idxUCPDE = idxPDE + 0x80;                   // index to 1st uncached PDE entry
    DWORD nEntries = (dwRamEnd - dwRamStart) >> 22;   // # of PDE entries needed
    PPAGETABLE ppte = (PPAGETABLE) (pTOC->ulRAMFree + MemForPT);    // 2nd Level Page table for cached address
    PPAGETABLE ppteUC = ppte+1;                         // 2nd level page table for uncached address
    PPAGETABLE pptePrev = (PPAGETABLE) Phys2Virt (GetPFN (g_pPageDir->PTE[idxPDE-1] & -PAGE_SIZE));     // previous PDE for cached address
    PPAGETABLE ppteUCPrev = (PPAGETABLE) Phys2Virt (GetPFN (g_pPageDir->PTE[idxUCPDE-1] & -PAGE_SIZE)); // previous PDE for uncached address
    const DWORD pdeAttrib = g_pPageDir->PTE[idxPDE-1] & (PAGE_SIZE-1);      // attribute bits for cached PDE
    const DWORD pdeUCAttrib = g_pPageDir->PTE[idxUCPDE-1] & (PAGE_SIZE-1);  // attribute bits for uncached PDE
    int i;

    // reserver memroy for 2nd level page tables (*2 == 1 for cached and 1 for uncached)
    MemForPT += nEntries * PAGE_SIZE * 2;

    // fill PDE/PTE in 4M chunks.
    for ( ; nEntries --; idxPDE ++, idxUCPDE ++) {

        // file the page table entries. 
        for (i = 0; i < 1024; i ++) {
            ppte->PTE[i] = pptePrev->PTE[i] + 0x400000;
            ppteUC->PTE[i] = ppteUCPrev->PTE[i] + 0x400000;
        }

        // update page directory entries
        g_pPageDir->PTE[idxPDE] = LIN_TO_PHYS (ppte) | pdeAttrib;
        g_pPageDir->PTE[idxUCPDE] = LIN_TO_PHYS (ppteUC) | pdeUCAttrib;

        // update 'prev' page table pointers.
        ppteUCPrev = ppteUC;
        pptePrev = ppte;

        // advance to the next page table.
        // NOTE: we use 2 pages at a time (1 cached, 1 uncached)
        ppte += 2;
        ppteUC += 2;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SystemInitialization (PPTE pOEMAddressTable, LPDWORD pSysPages) 
{
    int i, idxRam;
    DWORD dwCpuCap;
    DWORD dwMappedRamEnd;

    /* Zero out kernel data page. */
    /* NOTE: need to preserve the number of pages used by Page Tables */
    i = KData.nMemForPT;                // save MemForPT
    dwCpuCap = KData.dwCpuCap;          // save dwCpuCap
    memset(&KData, 0, sizeof(KData));
    KData.nMemForPT = i;                // resotre MemForPT
    KData.dwCpuCap = dwCpuCap;          // restore dwCpuCap
    KData.handleBase = 0x80000000;

    /* Initialize SectionTable in KPage */
    for (i = 1 ; i <= SECTION_MASK ; ++i)
        SectionTable[i] = NULL_SECTION;

    /* Copy kernel data to RAM & zero out BSS */
    KernelRelocate(pTOC);

    /* save the address of OEMAddressTable */
    g_pOEMAddressTable = pOEMAddressTable;
    g_pPageDir = (PPAGETABLE) pSysPages;

    idxRam = FindRAMEntry (pOEMAddressTable);
    dwMappedRamEnd = pOEMAddressTable[idxRam].dwVA + pOEMAddressTable[idxRam].dwSize;
    
    OEMInitDebugSerial();           // initialize serial port

    OEMWriteDebugString(TEXT("\r\nSysInit: "));

    // if config.bib specify more RAM than OEMAddressTable, map extra ram and keep a 
    // copy of the OEMAddressTable, with adjusted size.
    // NOTE: OEMAddress is considered to be in ROM and we cannot modify it directly.

    if (dwMappedRamEnd && (dwMappedRamEnd < pTOC->ulRAMEnd)) {
        // we have more RAM than we've mapped.

        // Round up end address to next 4M. 
        DWORD dwEndAddr = (pTOC->ulRAMEnd + 0x3fffff) & ~0x3fffff;

        NKDbgPrintfW(L"Mapping Extra RAM from 0x%8.8lx to 0x%8.8lx\r\n", dwMappedRamEnd, dwEndAddr);
        // make a copy of OEMAddressTable. we have a full page reserved for this purpose,
        // which can hold more than 300 entries. 
        g_pOEMAddressTable = (PPTE) ((DWORD)pSysPages + PAGE_SIZE);
        for (i = 0; (g_pOEMAddressTable[i] = pOEMAddressTable[i]).dwSize; i ++) {
            // empty loop body
        }

        // update the size of RAM in OEMAddressTable
        g_pOEMAddressTable[idxRam].dwSize = dwEndAddr - g_pOEMAddressTable[idxRam].dwVA;

#ifdef DEBUG
        for (i = 0; g_pOEMAddressTable[i].dwSize; i ++) {
            // empty loop body
            NKDbgPrintfW(L"%d: %8.8lx %8.8lx %8.8lx\r\n", i, 
                g_pOEMAddressTable[i].dwVA, g_pOEMAddressTable[i].dwPA, g_pOEMAddressTable[i].dwSize);
        }
#endif
        // map extra ram.
        if (dwCpuCap & CPUID_PSE) {
            // map extra RAM using 4M pages
            MapExtraRAM4M (dwMappedRamEnd, dwEndAddr); 
        } else {
            // map extra ram using 4K pages
            MapExtraRAM4K (dwMappedRamEnd, dwEndAddr); 
        }
    }
    
    NKDbgPrintfW(L"GDTBase=%8.8lx IDTBase=%8.8lx KData=%8.8lx\r\n", &g_aGlobalDescriptorTable, &g_aIntDescTable, &KData);

    /* Switch to our GDT now that kernel relocation is done */
    __asm {
        lgdt    [g_KernelGDTBase]
        push    KGDT_R0_CODE
        push    offset OurCSIsLoadedNow
        retf
OurCSIsLoadedNow:
        mov     eax, KGDT_R0_DATA
        mov     ss, ax
        mov     eax, KGDT_R3_DATA
        mov     ds, ax
        mov     es, ax
    }

    // Initialize the interrupt dispatch tables
    InitializeTaskStates();
    InitializeIDT();

    OEMWriteDebugString(TEXT("Windows CE Kernel for i486 Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) TEXT("\r\n"));

    //  Initialize the page tables and the floating point unit
    InitializePageTables();
    OEMInit();          // initialize firmware
    KernelFindMemory();
    NKDbgPrintfW (TEXT("X86Init done, OEMAddressTable = %8.8lx.\r\n"), g_pOEMAddressTable);
}

