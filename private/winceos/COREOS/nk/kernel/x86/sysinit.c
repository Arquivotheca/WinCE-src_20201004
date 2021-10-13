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
#include <kernel.h>

void KernelFindMemory (void);
void Reschedule (void);
void SafeIdentifyCpu (void);
void KernelInit (void);

static BOOL sg_fResumeFromSnapshot = FALSE;


//
// x86 specfic NK Globals
//
static ULONGLONG BspGDT[] = {
    0,                          // 0x00
    0x00CF9A000000FFFF,         // 0x08: Ring 0 code, Limit = 4G
    0x00CF92000000FFFF,         // 0x10: Ring 0 data, Limit = 4G
    0x00CFBA000000FFFF,         // 0x18: Ring 1 code, Limit = 4G
    0x00CFB2000000FFFF,         // 0x20: Ring 1 data, Limit = 4G
    0x00CFDA000000FFFF,         // 0x28: Ring 2 code, Limit = 4G
    0x00CFD2000000FFFF,         // 0x30: Ring 2 data, Limit = 4G
    0x00CFFA000000FFFF,         // 0x38: Ring 3 code, Limit = 4G
    0x00CFF2000000FFFF,         // 0x40: Ring 3 data, Limit = 4G
    0,                          // 0x48: Will be main TSS
    0,                          // 0x50: Will be NMI TSS
    0,                          // 0x58: Will be Double Fault TSS
    0x0040F20000000000+FS_LIMIT,// 0x60: PCR selector
    0x00CFBE000000FFFF,         // 0x68: Ring 1 (conforming) code, Limit = 4G
    0x0040F00000000000+sizeof(PCB), // 0x70: Ring 3 r/o Data, PCB selector
    0x0040B20000000000+sizeof(PCB), // 0x78: Ring 1 Data, PCB selector
};
const FWORDPtr BspGDTBase = {sizeof(BspGDT)-1, &BspGDT };

#define Naked void __declspec(naked)

#pragma pack(push, 1)           // We want this structure packed exactly as declared!
typedef struct {
    BYTE        PushEax;
    BYTE        Pushad;
    BYTE        PushDS;
    BYTE        PushES;
    WORD        PushFS;
    WORD        PushGS;
    BYTE        MovEsi;
    PFNVOID     pfnHandler;
    BYTE        JmpNear;
    DWORD       JmpOffset;
    DWORD       Padding;
} INTHOOKSTUB, * PINTHOOKSTUB;
#pragma pack(pop)

INTHOOKSTUB g_aIntHookStubs[MAXIMUM_IDTVECTOR+1] = {0};

KIDTENTRY BspIDT[MAXIMUM_IDTVECTOR+1];      // interrupt vector for main CPU
KIDTENTRY ApIDT[MAXIMUM_IDTVECTOR+1];       // interrupt vector for the rest of CPUs

const WORD cbIDTLimit = sizeof (BspIDT) - 1;    // IDT limit is constant, regardless of CPUs

#define MAX_GDT             64
#define SYSCALL_STACK_SIZE  128

typedef struct _APPage {
    BYTE        SyscallStack[SYSCALL_STACK_SIZE];
    KGDTENTRY   GDT[MAX_GDT];
    KTSS        TSS;
} APPage, *PAPPage;

ERRFALSE (sizeof (APPage) <= 0x800);
ERRFALSE (sizeof (PCB) <= 0x800);

__inline PPCB ApPCBFromApPage (PAPPage pPage)
{
    return (PPCB) ((DWORD) pPage + 0x800);
}

static KTSS BspTSS;
static KTSS DoubleTSS;
static KTSS NMITSS;

BYTE TASKStack[0x800];      // task stack. proabaly don't need to be this big, but allocate 2k to be safe

// increasing the stack size as floating point emulation ops
// are now done on syscallstack always (ring1 operation).
BYTE BspSyscallStack[1024];     // temporary stack for syscall trap

void Int1Fault(void);
void Int2Fault(void);
void Int3Fault(void);
void GeneralFault(void);
void PageFault(void);
void InvalidOpcode(void);
void ZeroDivide(void);
void CommonIntDispatch(void);
void Int22KCallHandler(void);
void Int24Reschedule (void);
PFNVOID Int20SyscallHandler(void);


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
    GetPCB ()->cNest = -1;
    NKDbgPrintfW(L"\r\nNMI -- backlink=%4.4x\r\n", NMITSS.Backlink);
    DumpTSS(&BspTSS);
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
    GetPCB ()->cNest = -1;
    NKDbgPrintfW(L"\r\nDouble Fault -- backlink=%4.4x\r\n", DoubleTSS.Backlink);
    DumpTSS(&BspTSS);
    __asm {
        cli
        hlt
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitTSSSelector(
    PKGDTENTRY pGDT,
    ULONG uSelector,
    PKTSS pTSS
    ) 
{
    PKGDTENTRY pTSSSel = (PKGDTENTRY)(((ULONG)&pGDT[0]) + uSelector);
    pTSSSel->LimitLow = sizeof(KTSS)-1;
    pTSSSel->BaseLow = (USHORT)((DWORD)pTSS) & 0xFFFF;
    pTSSSel->HighWord.Bytes.BaseMid = (UCHAR)(((DWORD)pTSS) >> 16) & 0xFF;
    pTSSSel->HighWord.Bytes.BaseHi = (UCHAR)(((DWORD)pTSS) >> 24) & 0xFF;
    pTSSSel->HighWord.Bits.Type = TYPE_TSS;
    pTSSSel->HighWord.Bits.Pres = 1;
}

void InitTSS (PKGDTENTRY pGDT, ULONG tssSelector, PKTSS ptss, ULONG esp0, ULONG eip)
{
    ulong       pageDir;

    __asm {
        mov eax, CR3
        mov pageDir, eax
    }
    InitTSSSelector(pGDT, tssSelector, ptss);
    ptss->Eip   = (ULONG)eip;
    ptss->Cs    = KGDT_R0_CODE;
    ptss->Ss    = KGDT_R0_DATA;
    ptss->Ss0   = KGDT_R0_DATA;
    ptss->Ds    = KGDT_R0_DATA;
    ptss->Es    = KGDT_R0_DATA;
    ptss->Esp   = ptss->Esp0 = esp0;
    ptss->CR3   = pageDir;
    ptss->Gs    = KGDT_KPCB;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static
void InitTasks (PPCB ppcb, LPBYTE pSyscallStack) 
{
    PKGDTENTRY  pGDT     = ppcb->pGDT;
    PKTSS       pMainTss = ppcb->pTSS;
    PKGDTENTRY  pEmx87Sel = (PKGDTENTRY)(((ULONG)&pGDT[0]) + KGDT_EMX87);

    InitTSS (pGDT, KGDT_MAIN_TSS, pMainTss, (DWORD) ppcb->pKStack, 0);
    pMainTss->Esp1 = (ulong)pSyscallStack+SYSCALL_STACK_SIZE;
    pMainTss->Ss1  = KGDT_R1_DATA | 1;

    InitTSS (pGDT, KGDT_NMI_TSS,    &NMITSS,    ((ULONG)&TASKStack[0]) + sizeof(TASKStack), (ULONG) NonMaskableInterrupt);
    InitTSS (pGDT, KGDT_DOUBLE_TSS, &DoubleTSS, ((ULONG)&TASKStack[0]) + sizeof(TASKStack), (ULONG) DoubleFault);

    // Change the KGDT_EMX87 entry in the global descriptor table to be Ring 3 conforming
    pEmx87Sel->HighWord.Bits.Dpl = 3;

    _asm {
        mov     eax, KGDT_MAIN_TSS
        ltr     ax
    }
}

void SetGS (void)
{
    _asm {
        mov eax, KGDT_KPCB
        mov gs, ax
    }
}

static void UpdateSelectorBase (PKGDTENTRY pTSSSel, DWORD dwBase)
{
    pTSSSel->BaseLow = (USHORT) (dwBase & 0xFFFF);
    pTSSSel->HighWord.Bytes.BaseMid = (UCHAR)((dwBase) >> 16) & 0xFF;
    pTSSSel->HighWord.Bytes.BaseHi  = (UCHAR)((dwBase) >> 24) & 0xFF;
}


static void UpdatePCBSelectorBase (PPCB ppcb)
{
    UpdateSelectorBase ((PKGDTENTRY)(((ULONG)ppcb->pGDT) + KGDT_KPCB), (DWORD) ppcb);
    UpdateSelectorBase ((PKGDTENTRY)(((ULONG)ppcb->pGDT) + KGDT_UPCB), (DWORD) PUserKData + (ppcb->dwCpuId-1)*VM_PAGE_SIZE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static 
void InitIDTEntry(
    PKIDTENTRY pIDT,
    int i,
    USHORT usSelector,
    PFNVOID pFaultHandler,
    USHORT usGateType
    ) 
{
    PKIDTENTRY pCur = &pIDT[i];
    pCur->Offset = LOWORD((DWORD)pFaultHandler);
    pCur->ExtendedOffset = HIWORD((DWORD)pFaultHandler);
    pCur->Selector = usSelector;
    pCur->Access = usGateType;
}


void DoHookInterrupt (PKIDTENTRY pIDT, int hwInterruptNumber, PFNVOID pfnHandler, USHORT usGateType)
{
    PINTHOOKSTUB pStub = &g_aIntHookStubs[hwInterruptNumber];
    pStub->PushEax   = 0x50;
    pStub->Pushad    = 0x60;
    pStub->PushDS    = 0x1E;
    pStub->PushES    = 0x06;
    pStub->PushFS    = 0xa00f;
    pStub->PushGS    = 0xa80f;
    pStub->MovEsi    = 0xBE;
    pStub->pfnHandler= pfnHandler;
    pStub->JmpNear   = 0xE9;
    pStub->JmpOffset = ((DWORD)&CommonIntDispatch) - ((DWORD)(&pStub->JmpOffset) + 4);
    InitIDTEntry (pIDT, hwInterruptNumber, KGDT_R0_CODE, (PFNVOID) (DWORD) pStub, usGateType);
}

DWORD g_nBadInts;

void __declspec(naked) BadInt(void) 
{  
    _asm {
        inc     dword ptr [g_nBadInts]
        iretd
    } 
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
InitIDT (PKIDTENTRY pIDT) 
{
    FWORDPtr IDTBase = { cbIDTLimit, pIDT };

    InitIDTEntry(pIDT, ID_DIVIDE_BY_ZERO,       KGDT_R0_CODE,     ZeroDivide,   INTERRUPT_GATE);
    InitIDTEntry(pIDT, ID_SINGLE_STEP,          KGDT_R0_CODE,     Int1Fault,    RING3_INT_GATE);
    InitIDTEntry(pIDT, ID_THREAD_BREAK_POINT,   KGDT_R0_CODE,     Int2Fault,    INTERRUPT_GATE);
    // InitIDTEntry(pIDT, 0x02, KGDT_NMI_TSS, 0, TASK_GATE);
    InitIDTEntry(pIDT, ID_BREAK_POINT,          KGDT_R0_CODE,     Int3Fault,    RING3_INT_GATE);
    InitIDTEntry(pIDT, ID_ILLEGAL_INSTRUCTION,  KGDT_R0_CODE,     InvalidOpcode,INTERRUPT_GATE);
    InitIDTEntry(pIDT, ID_DOUBLE_FAULT,         KGDT_DOUBLE_TSS,  NULL,         TASK_GATE);
    InitIDTEntry(pIDT, ID_PROTECTION_FAULT,     KGDT_R0_CODE,     GeneralFault, INTERRUPT_GATE);
    InitIDTEntry(pIDT, ID_PAGE_FAULT,           KGDT_R0_CODE,     PageFault,    INTERRUPT_GATE);
    InitIDTEntry(pIDT, SYSCALL_INT,             KGDT_R1_CODE | 1, Int20SyscallHandler(), RING3_INT_GATE);
    InitIDTEntry(pIDT, KCALL_INT,               KGDT_R0_CODE,     Int22KCallHandler,     RING1_INT_GATE);
    InitIDTEntry(pIDT, RESCHED_INT,             KGDT_R0_CODE,     Int24Reschedule,       RING1_INT_GATE);

    _asm { lidt [IDTBase] };


}

void FPUNotPresentException (void);
void FPUException (void);

void InitFPUIDTs (FARPROC NPXNPHandler)
{
    if (NPXNPHandler) {
        // use emulation
        if (IsKModeAddr ((DWORD)NPXNPHandler)) {
            InitIDTEntry(BspIDT, ID_FPU_NOT_PRESENT, KGDT_R1_CODE, NPXNPHandler, RING1_TRAP_GATE);
            InitIDTEntry(ApIDT, ID_FPU_NOT_PRESENT, KGDT_R1_CODE, NPXNPHandler, RING1_TRAP_GATE);
            
        } else {
            InitIDTEntry(BspIDT, ID_FPU_NOT_PRESENT, KGDT_R3_CODE, NPXNPHandler, RING3_TRAP_GATE);
            InitIDTEntry(ApIDT, ID_FPU_NOT_PRESENT, KGDT_R3_CODE, NPXNPHandler, RING3_TRAP_GATE);
        }
        
    } else {
        InitIDTEntry(BspIDT, ID_FPU_NOT_PRESENT, KGDT_R0_CODE, FPUNotPresentException, INTERRUPT_GATE);
        InitIDTEntry(BspIDT, ID_FPU_EXCEPTION,   KGDT_R0_CODE, FPUException, INTERRUPT_GATE);
        InitIDTEntry(ApIDT,  ID_FPU_NOT_PRESENT, KGDT_R0_CODE, FPUNotPresentException, INTERRUPT_GATE);
        InitIDTEntry(ApIDT,  ID_FPU_EXCEPTION,   KGDT_R0_CODE, FPUException, INTERRUPT_GATE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    PKIDTENTRY pIDTEntry = &BspIDT[hwInterruptNumber];
    BOOL fRet = ((DWORD) hwInterruptNumber <= MAXIMUM_IDTVECTOR)
             && !pIDTEntry->Offset
             && !pIDTEntry->Selector;

    if (fRet) {
        DoHookInterrupt (BspIDT, hwInterruptNumber, (PFNVOID) pfnHandler, INTERRUPT_GATE);
    }
    return (fRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UnhookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    if ((DWORD) hwInterruptNumber < ARRAY_SIZE(BspIDT)) {
        PKIDTENTRY pIDTEntry = &BspIDT[hwInterruptNumber];
        PINTHOOKSTUB pStub = (PINTHOOKSTUB)(pIDTEntry->Offset + (pIDTEntry->ExtendedOffset << 16));
        if (pStub >= &g_aIntHookStubs[0] && pStub <= LAST_ELEMENT(g_aIntHookStubs)) {
            InitIDTEntry (BspIDT, hwInterruptNumber, 0, 0, 0);
            pStub->pfnHandler = NULL;
            return(TRUE);
        }
    }
    return(FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
HookIpi(
    int hwInterruptNumber,
    FARPROC pfnHandler
    )
{
    PKIDTENTRY pIDTEntry = &BspIDT[hwInterruptNumber];
    BOOL fRet = ((DWORD) hwInterruptNumber <= MAXIMUM_IDTVECTOR)
             && !pIDTEntry->Offset
             && !pIDTEntry->Selector;

    if (fRet) {
        DoHookInterrupt (BspIDT, hwInterruptNumber, (PFNVOID) pfnHandler, INTERRUPT_GATE);
        DoHookInterrupt (ApIDT, hwInterruptNumber, (PFNVOID) pfnHandler, INTERRUPT_GATE);
    }
    return (fRet);
}

extern volatile LONG g_nCpuStopped;
extern volatile LONG g_nCpuReady;

volatile DWORD g_dwPerMPMemoryBase;
extern volatile LONG g_fStartMP;

// setup User access to PCB
void MDSetupUserKPage (PPAGETABLE pptbl)
{
    DWORD idx;
    DWORD dwPfn;

    // No more than 8 CPU for now.
    DEBUGCHK (g_pKData->nCpus <= 8);
    
    for (idx = 0; idx < g_pKData->nCpus; idx ++) {
        DEBUGCHK (g_ppcbs[idx]);
        dwPfn = GetPFN ((LPCVOID) ((DWORD)g_ppcbs[idx] & ~VM_PAGE_OFST_MASK));
        DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);
        pptbl->pte[VM_KPAGE_IDX+idx] = dwPfn + PG_VALID_MASK + PG_CACHE + PG_USER_MASK;
    }
}

// setup User access to PCB
void MDClearUserKPage (PPAGETABLE pptbl)
{
    DWORD  idx;

    // No more than 8 CPU for now.
    DEBUGCHK (g_pKData->nCpus <= 8);
    
    for (idx = 0; idx < g_pKData->nCpus; idx ++) {
        pptbl->pte[idx + VM_KPAGE_IDX] = 0;
    }
}


void 
ZeroPage(
    void *pvPage
    );

void ApStartContinue (PAPPage pApPage)
{
    PPCB ppcb = ApPCBFromApPage (pApPage);

    ppcb->pVMPrc      = ppcb->pCurPrc
                      = g_pprcNK;
    ppcb->dwCpuState  = sg_fResumeFromSnapshot? CE_PROCESSOR_STATE_POWERED_OFF : CE_PROCESSOR_STATE_POWERED_ON;
    ppcb->ActvProcId  = g_pprcNK->dwId;
    ppcb->pCurThd     = &dummyThread;
    ppcb->CurThId     = DUMMY_THREAD_ID;
    ppcb->fIdle       = FALSE;
    ppcb->OwnerProcId = g_pprcNK->dwId;

    UpdatePCBSelectorBase (ppcb);
    SetGS ();
    InitTasks (ppcb, &pApPage->SyscallStack[0]);
    InitIDT (ApIDT);

    // call per CPU initialization function
    ppcb->dwHardwareCPUId = g_pOemGlobal->pfnMpPerCPUInit ();

    ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = GETCURRTICK ();
    SetReschedule (ppcb);

    DEBUGMSG (1, (L"CPU %d started, ready to reschedule\r\n", ppcb->dwCpuId));
    
    InterlockedIncrement (&g_nCpuReady);

    while (g_nCpuStopped) {
    }

    __asm {
        // for multi-core, we always assume that FXSAVE/FXRESTORE is available
        MOV_EDX_CR4
        or      edx, CR4_FXSR
        MOV_CR4_EDX

        xor edi, edi
        mov ebx, dword ptr [ppcb]
        jmp Reschedule
    }
}

void InitAPPages (PAPPage pApPage, DWORD idx)
{
    PPCB ppcb = ApPCBFromApPage (pApPage);

    ZeroPage (pApPage);

    g_ppcbs[idx]    = ppcb;
    ppcb->dwCpuId   = idx + 1;
    ppcb->pGDT      = pApPage->GDT;
    ppcb->pIDT      = ApIDT;
    ppcb->pTSS      = &pApPage->TSS;
    ppcb->pSelf     = ppcb;
    ppcb->pKStack   = ((LPBYTE) pApPage) - 4;
    ppcb->fIdle     = FALSE;
    ppcb->dwSyscallReturnTrap = SYSCALL_RETURN_RAW;

    // initialize GDT for AP
    memcpy (ppcb->pGDT, BspGDT, sizeof (BspGDT));
    
    // use the 1st 6 bytes to store FWORDPtr to GDT
    ((FWORDPtr *) pApPage)->Base = pApPage->GDT;
    ((FWORDPtr *) pApPage)->Size = sizeof (BspGDT)-1;
}

static DWORD dwIdxApPage;


//
// NKMpStart - start AP running. 
//
// NOTE: WE DO NOT HAVE A STACK AT THE ENTRANCE. NOT CALL TO C FUNCTIONS UNTIL STACK IS SET UP.
//
Naked NKMpStart (void)
{
    _asm {
            // spin till g_pApPages is setup
    spin:   mov         esi, dword ptr [g_fStartMP]
            cmp         esi, 0
            jz          short spin

            mov         edx, dword ptr [g_dwPerMPMemoryBase]
            lea         eax, [esi-2]

            cmp         esi, 1
            jne         gotcpuid

            // get our CPU ID
            mov         eax, 1
            lock xadd   dword ptr [dwIdxApPage], eax
            
    gotcpuid:
            // calcluate the offset to g_pApPages for the CPU
            //  -- every CPU get 2 pages where  2nd page is the AP Page
            // (eax) = idx to AP page
            // (edx) = g_dwPerMPMemoryBase
            shl         eax, (VM_PAGE_SHIFT+1)
            add         eax, VM_PAGE_SIZE
            lea         edi, [eax+edx]          // (edi) = pAPPage

            // switch GDT to our own (pApPage->GDT)
            // (edi) = pAPPage, ptr to FWORDPtr for our GDT at this point (setup by master CPU)
            // load our GDT
            lgdt        [edi]

            lea         esp, [edi-4]            // setup esp

            // perform a far return to load CS
            push        KGDT_R0_CODE
            push        offset CSLoaded
            retf

    CSLoaded:
            wbinvd

            // our GDT is loaded, update segment registers
            mov         eax, KGDT_R0_DATA
            mov         ss, ax
            mov         eax, KGDT_R3_DATA
            mov         ds, ax
            mov         es, ax

            push        edi         // argument - pAPPage
            call        near ApStartContinue

            // should never return
            cli
            hlt
    }
}


void InitAllOtherCPUs (void)
{

    if (g_pOemGlobal->fMPEnable) {
        DWORD nCpus;
        DWORD idx;

        // Hook dummy handler (AP still got interrupted on AMD...)
        for (idx = 0; idx <= MAXIMUM_IDTVECTOR; idx ++) {
            InitIDTEntry (ApIDT, idx, KGDT_R0_CODE, BadInt, INTERRUPT_GATE);
        }

        // call to OAL to perform MP Init
        if (g_pOemGlobal->pfnStartAllCpus ((PLONG) &nCpus, (FARPROC) NKMpStart) && (nCpus > 1)) {
            // MP detected. allocate memory for PCB/stacks for the CPUs
            DEBUGMSG (1, (L"MP Detected, # of CPUs = %8.8lx\r\n", nCpus));

            g_pKData->nCpus = nCpus;

            g_dwPerMPMemoryBase = pTOC->ulRAMFree + MemForPT;
            // each core got 2 pages for PCB, stacks, etc.
            MemForPT += (nCpus - 1) * 2 * VM_PAGE_SIZE;

            // initialize AP Pages
            for (idx = 1; idx < nCpus; idx ++) {
                InitAPPages ((PAPPage) (g_dwPerMPMemoryBase + idx * VM_PAGE_SIZE * 2 - VM_PAGE_SIZE), idx);
            }

            KCall ((PKFN)OEMCacheRangeFlush, 0, 0, CACHE_SYNC_ALL);
        }
    }
}

void StartAllCPUs (void)
{
    if (g_pKData->nCpus > 1) {

        // all CPUs should be blocked in NKMpStart, spinning waiting for g_pApPages to be set.
        DEBUGMSG (1, (L"Resuming all CPUs, g_pKData->nCpus = %d\r\n", g_pKData->nCpus));

        INTERRUPTS_OFF ();
        g_fStartMP = 1;

        while (g_pKData->nCpus != (DWORD) g_nCpuReady) {
            ;
        }
        // setup user mode access to pcb
        MDSetupUserKPage (GetPageTable (g_ppdirNK, 0));
        if(sg_fResumeFromSnapshot){
            g_nCpuReady = 1;
        }
        g_nCpuStopped = 0;
        INTERRUPTS_ON ();


        KCall ((PKFN) OEMCacheRangeFlush, 0, 0, CACHE_SYNC_ALL);

    } else {
        g_nCpuStopped = 0;
    }

    DEBUGMSG (1, (L"All CPUs resumed, g_nCpuReady = %d\r\n", g_nCpuReady));

    
}

DWORD CalcMaxDeviceMapping (void);
void MapDeviceTable (void);

static void SetupKnownUncachedAddress (void)
{
    if (g_pKData->pAddrMap != g_pOEMAddressTable) {
        // new table format, 0xa0000000 is not mapped.
        // use the 1st entry of the syscall page to map 
        // to uncached address at physical 0
        PPAGETABLE pptbl = GetPageTable (g_ppdirNK, 0x3FF);
        pptbl->pte[0] = (PG_VALID_MASK | PG_DIRTY_MASK | PG_NOCACHE | PG_WRITE_MASK);
        g_pNKGlobal->dwKnownUncachedAddress = 0xFFC00000;
    }
}

static void DoNKStartup (struct KDataStruct *pKData)
{
    PFN_OEMInitGlobals pfnInitGlob;
    PFN_DllMain pfnKitlEntry;
    PPCB ppcb = (PPCB) pKData;


    g_pKData   = pKData;
    g_ppcbs[0] = ppcb;

    // initialize Bsp 'processor control block'
    g_pKData->nCpus = 1;
    ppcb->pGDT      = (PKGDTENTRY) &BspGDT[0];
    ppcb->pIDT      = &BspIDT[0];
    ppcb->pTSS      = &BspTSS;
    ppcb->dwCpuId   = 1;
    ppcb->pSelf     = ppcb;
    ppcb->pKStack   = (LPBYTE) pKData - 4;
    ppcb->pVMPrc    = ppcb->pCurPrc = g_pprcNK;
    ppcb->CurThId   = 2;        // faked current thread id so ECS won't put 0 in OwnerThread
    ppcb->dwCpuState = CE_PROCESSOR_STATE_POWERED_ON;
    ppcb->dwSyscallReturnTrap = SYSCALL_RETURN_RAW;

    // update base of KGDT_KPCB
    UpdatePCBSelectorBase (ppcb);

    SetGS ();

    // pickup arguments from the nk loader
    pTOC       = (const ROMHDR *) pKData->dwTOCAddr;

    // initialize nk globals
    FirstROM.pTOC = (ROMHDR *) pTOC;
    FirstROM.pNext = 0;
    ROMChain = &FirstROM;
    KInfoTable[KINX_PTOC] = (long)pTOC;

    // o Page directory at pTOC->ulRamFree
    g_ppdirNK = (PPAGEDIRECTORY) pTOC->ulRAMFree;
    g_pOEMAddressTable = pKData->pAddrMap;
    g_pKData->pNk  = g_pNKGlobal;
    g_pprcNK->ppdir = g_ppdirNK;
    g_pKData->dwKVMStart = g_pprcNK->vaFree = CalcMaxDeviceMapping ();

    // setup known uncached address for cache flushing
    SetupKnownUncachedAddress ();

    pfnInitGlob = (PFN_OEMInitGlobals) pKData->dwOEMInitGlobalsAddr;

    // no checking here, if OAL entry point doesn't exist, we can't continue
    g_pOemGlobal = pfnInitGlob (g_pNKGlobal);
    g_pKData->pOem = g_pOemGlobal;
    g_pOemGlobal->dwMainMemoryEndAddress = pTOC->ulRAMEnd;
    
    // initialize Process information
    CEProcessorType = PROCESSOR_INTEL_486;
    CEInstructionSet = PROCESSOR_X86_32BIT_INSTRUCTION;

    g_pNKGlobal->pfnWriteDebugString = g_pOemGlobal->pfnWriteDebugString;

    MapDeviceTable ();

    // try to load KITL if exist
    pfnKitlEntry = (PFN_DllMain) g_pOemGlobal->pfnKITLGlobalInit;

    if (!pfnKitlEntry) {
        pfnKitlEntry = (PFN_DllMain) FindROMDllEntry (pTOC, KITLDLL);
    }
    if (pfnKitlEntry) {
        (* pfnKitlEntry) (NULL, DLL_PROCESS_ATTACH, (DWORD) NKKernelLibIoControl);
    }

#ifdef DEBUG
    dwLastKCTime = CurMSec = ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif

    OEMInitDebugSerial ();

    NKDbgPrintfW(L"\r\nSysInit: GDTBase=%8.8lx IDTBase=%8.8lx KData=%8.8lx\r\n", BspGDT, BspIDT, g_pKData);

    // Initialize the interrupt dispatch tables
    InitTasks (ppcb, BspSyscallStack);
    InitIDT (ppcb->pIDT);

    OEMWriteDebugString (TEXT("Windows CE Kernel for i486") 
#ifdef DEBUG
                         TEXT(" Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__)
#endif // DEBUG
                         TEXT("\r\n"));


    OEMInit ();
#ifndef SHIP_BUILD            
    OEMWriteDebugString (TEXT("OEMInit Complete\r\n"));
#endif
    
    InitAllOtherCPUs ();
#ifndef SHIP_BUILD        
    OEMWriteDebugString (TEXT("InitAllOtherCPUs Complete\r\n"));
#endif

    SafeIdentifyCpu ();
    

#ifndef SHIP_BUILD
    NKDbgPrintfW (L"X86Init done, OEMAddressTable = %8.8lx, RAM mapped = %8.8lx.\r\n",
        g_pOEMAddressTable, g_pOEMAddressTable[0].dwSize);
#endif

    KernelInit ();

#ifndef SHIP_BUILD    
    OEMWriteDebugString (TEXT("KernelInit Complete\r\n"));     
#endif
        


    DEBUGLOG (ZONE_SCHEDULE, g_pKData->nCpus);
    DEBUGLOG (ZONE_SCHEDULE, &g_schedLock);
    DEBUGLOG (ZONE_SCHEDULE, &g_physLock);
    
    // initialization done. schedule the 1st thread
    __asm {
        xor edi, edi
        mov ebx, dword ptr [ppcb]
        jmp Reschedule
    }
}

void __declspec(naked) NKStartup (struct KDataStruct * pKData)
{
    /* Switch to the real GDT */
    __asm {
        lgdt    [BspGDTBase]
        push    KGDT_R0_CODE
        push    offset OurCSIsLoadedNow
        retf
OurCSIsLoadedNow:
        mov     eax, KGDT_R0_DATA
        mov     ss, ax
        mov     eax, KGDT_R3_DATA
        mov     ds, ax
        mov     es, ax
        // flush TLB
        mov     eax, cr3
        mov     cr3, eax
        jmp     DoNKStartup
    }
}

DWORD NKSnapshotSMPStart()
{
    DWORD nCpus = 1;
    if ( g_pOemGlobal->fMPEnable ) {
        dwIdxApPage = 0;
        g_fStartMP = 0;
        sg_fResumeFromSnapshot = TRUE;
        g_nCpuStopped = 1;
        // call to OAL to perform MP Init
        if (g_pOemGlobal->pfnStartAllCpus ((PLONG) &nCpus, (FARPROC) NKMpStart) && (nCpus > 1)) {
            g_pKData->nCpus = nCpus;
        }
        StartAllCPUs();
    }
    return nCpus;
}


