//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "kernel.h"

#ifdef THUMBSUPPORT
#define THUMB_SUPPORT_STR TEXT("(Thumb Enabled)")
#else
#define THUMB_SUPPORT_STR TEXT(" ")
#endif

#ifdef THUMB
const wchar_t NKCpuType [] = TEXT("THUMB");
const wchar_t NKSignon[] = TEXT("Windows CE Kernel for THUMB ") THUMB_SUPPORT_STR TEXT(" Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) TEXT("\r\n");
#else
const wchar_t NKCpuType [] = TEXT("ARM");
const wchar_t NKSignon[] = TEXT("Windows CE Kernel for ARM ") THUMB_SUPPORT_STR TEXT(" Built on ") TEXT(__DATE__) TEXT(" at ") TEXT(__TIME__) TEXT("\r\n");
#endif

extern void (*lpNKHaltSystem)(void);
extern void FakeNKHaltSystem (void);

DWORD CEProcessorType;
WORD ProcessorLevel = 4;
WORD ProcessorRevision;
#if defined(ARMV4T)
DWORD CEInstructionSet = PROCESSOR_ARM_V4T_INSTRUCTION;
#elif defined(ARMV4I)
DWORD CEInstructionSet = PROCESSOR_ARM_V4I_INSTRUCTION;
#elif defined(ARMV4)
DWORD CEInstructionSet = PROCESSOR_ARM_V4_INSTRUCTION;
#endif

DWORD CurMSec;

// Fault Status register fields:
#define FSR_DOMAIN      0xF0
#define FSR_STATUS      0x0D
#define FSR_PAGE_ERROR  0x02

#define FSR_ALIGNMENT       0x01
#define FSR_TRANSLATION     0x05
#define FSR_DOMAIN_ERROR    0x09
#define FSR_PERMISSION      0x0D

#define BREAKPOINT 0x06000010   // special undefined instruction
#define THM_BREAKPOINT  0xDEFE              // Thumb equivalent

const LPCSTR IdStrings[] = {
    "RaiseException", "Reschedule", "Undefined Instruction", "SWI",
    "Prefetch Abort", "Data Abort", "IRQ", "<Invalid>", "<Invalid>", "[Stack fault]", "[HW Break]",
};


#define VFP_ENABLE_BIT  0x40000000
#define VFP_EX_BIT      0x80000000

DWORD vfpStat = VFP_NOT_EXIST;
extern BOOL TestVFP (void);
extern DWORD ReadAndSetFpexc (DWORD dwNewFpExc);

// NOTE: Save/Restore FloatContext do not Save/Restore fpexc
//       it needs to be handled differently
extern void SaveFloatContext(PTHREAD);
extern void RestoreFloatContext(PTHREAD);


// OEM defined VFP save/restore for implementation defined control registers
typedef void (* PFN_OEMSaveRestoreVFPCtrlRegs) (LPDWORD lpExtra, int nMaxRegs);
typedef BOOL (* PFN_OEMHandleVFPException) (EXCEPTION_RECORD *er, PCONTEXT pctx);

static void FakedSaveVFPExtra (LPDWORD lpExtra, int nMaxRegs)
{
}
static void FakedRestoreVFPExtra (LPDWORD lpExtra, int nMaxRegs)
{
}

// this is a stub for now
static BOOL FakedHandleVFPException (EXCEPTION_RECORD *er, PCONTEXT pctx)
{
    return FALSE;
}

PFN_OEMSaveRestoreVFPCtrlRegs pOEMSaveVFPCtrlRegs    = FakedSaveVFPExtra;
PFN_OEMSaveRestoreVFPCtrlRegs pOEMRestoreVFPCtrlRegs = FakedRestoreVFPExtra;
PFN_OEMHandleVFPException     pOEMHandleVFPException = FakedHandleVFPException;

// flag to indicate if VFP is touched. NOTE: Flushing VFP is NOT considered
// touching VFP
BOOL g_fVFPTouched;

#define PG_COARSE_TBL_BIT       0x01

extern CRITICAL_SECTION VAcs;
LPBYTE GrabFirstPhysPage(DWORD dwCount);

// hardware pagetable for the 2nd gig address and secure section
ulong gHwPTBL2G[32*HARDWARE_PT_PER_PROC];    // 32 == shared section + mapping address from 0x42000000 -> 0x7fffffff

DWORD gdwSlotTouched, gfObjStoreTouched;
PPTE g_pOEMAddressTable;

extern void _SetCPUASID (PTHREAD);

#define MAPPER_SLOT_TO_PTBL(slot)   (gHwPTBL2G + HARDWARE_PT_PER_PROC * ((slot) - 32))

BOOL MDValidateRomChain (ROMChain_t *pROMChain)
{
    PPTE ppte;
    DWORD dwEnd;
    
    for ( ; pROMChain; pROMChain = pROMChain->pNext) {
        for (ppte = g_pOEMAddressTable; ppte->dwSize; ppte ++) {
            dwEnd = ppte->dwVA + (ppte->dwSize << 20);
            if (IsInRange (pROMChain->pTOC->physfirst, ppte->dwVA, dwEnd)) {
                if (IsInRange (pROMChain->pTOC->physlast, ppte->dwVA, dwEnd)) {
                    // good XIP, break inner loop and go on to the next region
                    break;
                }
                // bad
                NKDbgPrintfW (L"MDValidateRomChain: XIP (%8.8lx -> %8.8lx) span accross multiple memory region\r\n", 
                    pROMChain->pTOC->physfirst, pROMChain->pTOC->physlast);
                return FALSE;
            }
        }
        if (!ppte->dwSize) {
            NKDbgPrintfW (L"MDValidateRomChain: XIP (%8.8lx -> %8.8lx) doesn't exist in OEMAddressTable \r\n",
                        pROMChain->pTOC->physfirst, pROMChain->pTOC->physlast);
            return FALSE;
        }
    }
    return TRUE;
}



MEMBLOCK *MDAllocMemBlock (DWORD dwBase, DWORD ixBlock)
{
    LPDWORD pPtbls;                         // which hardware PT to use
    DWORD   ixTbl  = ixBlock >> 6;          // ixTbl == which 4M block
    MEMBLOCK *pmb;

    DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: dwBase = %8.8lx, ixBlock = %8.8lx\r\n", dwBase, ixBlock));
    DEBUGCHK (!IsKernelVa (dwBase));
    DEBUGCHK (!VAcs.hCrit || (VAcs.OwnerThread == hCurThread));

    switch (dwBase) {
    case SECURE_VMBASE:
        // secure section, use NK's page table
        pPtbls = ProcArray[0].pPTBL;
        break;
        
    case MODULE_BASE_ADDRESS:
        // module section (slot 1), use 1st 8 entries of gHwPTBL2G
        pPtbls = gHwPTBL2G;
        break;
        
    case 0:
        // current process
        pPtbls = pCurProc->pPTBL;
        break;
        
    default:
        // either a sloted process address or 2nd gig
        if (dwBase < FIRST_MAPPER_ADDRESS) {
            // process address
            pPtbls = ProcArray[(dwBase >> VA_SECTION) - 1].pPTBL;
        } else {
            // in the 2nd gig
            pPtbls = MAPPER_SLOT_TO_PTBL (dwBase >> VA_SECTION);
        }
        break;
    }

    //
    // allocate a page if not allocate yet
    //
    if (!pPtbls[ixTbl]) {

        // allocate a new page for page table
        DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: pPtbls = %8.8lx ixTbl = %8.8lx\r\n", pPtbls, ixTbl));
        if (!(pPtbls[ixTbl] = (DWORD) KCall((PKFN)GrabFirstPhysPage,1))) {
            // Out of memory
            return NULL;
        }

        // we're relying on the fact that the page we just got has already been zero'd
        // or we need to memset it to 0 here.
        
        pPtbls[ixTbl] |= 0x20000000;    // uncached access only

        DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: Allocated a new page for Page Table va = %8.8lx\r\n",
            pPtbls[ixTbl]));

    }

    DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: Allocating MemBlock\r\n"));
    // allocate MEM_BLOCK
    if (pmb = AllocMem (HEAP_MEMBLOCK)) {

        // initialize memblock
        memset (pmb, 0, sizeof(MEMBLOCK));

        // pagetable access needs to be uncached.
        pmb->aPages = (LPDWORD) (pPtbls[ixTbl] + ((ixBlock << 6) & (PAGE_SIZE-1)));

    }

    DEBUGMSG (ZONE_VIRTMEM, (L"MDAllocMemBlock: returning %8.8lx, aPages = %8.8lx\r\n", pmb, pmb->aPages));
    return pmb;
}

void FreeHardwarePT (DWORD dwVMBase)
{
    LPDWORD pPtbls;                             // which hardware PT to use
    int i;
    // cannot free current process, shared section, or secure section
    DEBUGCHK ((dwVMBase != pCurProc->dwVMBase) && ((int) dwVMBase >= (2 << VA_SECTION)));
    DEBUGCHK (!VAcs.hCrit || (VAcs.OwnerThread == hCurThread));

    // clear first-level page table
    memset (&FirstPT[(dwVMBase >> 20)], 0, HARDWARE_PT_PER_PROC * 4 * sizeof (ULONG));

    if (dwVMBase < FIRST_MAPPER_ADDRESS) {
        pPtbls = ProcArray [(dwVMBase >> VA_SECTION) - 1].pPTBL;
        // per process
    } else {
        // 2nd gig
        DEBUGCHK ((dwVMBase >= FIRST_MAPPER_ADDRESS) && (dwVMBase < LAST_MAPPER_ADDRESS));

        pPtbls = MAPPER_SLOT_TO_PTBL (dwVMBase >> VA_SECTION);
    }
    for (i = 0; i < HARDWARE_PT_PER_PROC; i ++) {
        if (pPtbls[i]) {
            FreePhysPage (GetPFN (pPtbls[i]));
            pPtbls[i] = 0;
        }
    }
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
}

void MDFreeMemBlock (MEMBLOCK *pmb)
{
    DEBUGCHK ((NULL_BLOCK != pmb) && (RESERVED_BLOCK != pmb));
    DEBUGCHK (!VAcs.hCrit || (VAcs.OwnerThread == hCurThread));
#ifdef DEBUG
    {
        int i;
        for (i = 0; i < PAGES_PER_BLOCK; i ++) {
            DEBUGCHK (!pmb->aPages[i] || (pmb->aPages[i] == BAD_PAGE));
        }
    }
#endif
    // need to zero it out incase there're bad pages.
    memset (pmb->aPages, 0, PAGES_PER_BLOCK * sizeof (ulong));
    FreeMem (pmb, HEAP_MEMBLOCK);
}

_inline void ClearOneSlot (DWORD dwVMBase)
{
    //DEBUGMSG (ZONE_VIRTMEM, (L"ClearProcessSlot: dwVMBase = %8.8lx, PT entry = %8.8lx\r\n", dwVMBase, FirstPT + (dwVMBase >> 20)));
    memset (FirstPT + (dwVMBase >> 20), 0, 32 * sizeof (DWORD));
}


void ClearSlots (void)
{
    DWORD dwSlotRst;
    BOOL fCleared = FALSE;
    KCALLPROFON (0);
    if (dwSlotRst = gdwSlotTouched & ~CurAKey) {
        int i;
        for (i = 1; dwSlotRst; i ++) {
            if (dwSlotRst & (1 << i)) {
                dwSlotRst &= ~(1 << i);
                ClearOneSlot (ProcArray[i].dwVMBase);
            }
        }
        fCleared = TRUE;
        gdwSlotTouched &= CurAKey;
    }
    if (gfObjStoreTouched && !(pFileSysProc->aky & CurAKey)) {
        DWORD dwBits = g_ObjStoreSlotBits;
        int i;
        for (i = 0; dwBits; i ++) {
            if (dwBits & (1 << i)) {
                dwBits &= ~(1 << i);
                ClearOneSlot ((FIRST_MAPPER_SLOT + i) << VA_SECTION);
            }
        }
        
        fCleared = TRUE;
        gfObjStoreTouched = FALSE;
    }
    if (fCleared) {
        OEMCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
    }

    KCALLPROFOFF (0);
}

void SetCPUASID (PTHREAD pth)
{
    _SetCPUASID (pth);
    KCall ((FARPROC) ClearSlots);
}

void DetectVFP (void)
{
    DEBUGMSG (1, (L"Detecting VFP..."));
    vfpStat = VFP_TESTING | VFP_EXIST;
    TestVFP ();    // would generate an undefined-instruction exception if FPU not exist
                   // and ExceptionDispatch will unset the VFP_EXIST bit
    vfpStat &= ~VFP_TESTING;
    if (vfpStat)
        CEInstructionSet |= PROCESSOR_FEATURE_FP;
    DEBUGMSG (1, (L" VFP %s!\n", vfpStat? L"Found" : L"Not Found"));
}

BOOL IsVFPInstruction (DWORD dwInst)
{
    DWORD dwCoProc = (dwInst >> 8) & 0xf;
    return (VFP_EXIST == vfpStat) && ((dwCoProc == 10) || (dwCoProc == 11));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL SwitchFPUOwner (PCONTEXT pctx, BOOL *fFirstAccess) 
{
    BOOL fRet;
    KCALLPROFON(61);

    *fFirstAccess = !g_fVFPTouched;
    if (g_CurFPUOwner != pCurThread) {
        // DEBUGCHK (!g_fVFPTouched);
        if (g_CurFPUOwner) {
            SaveFloatContext(g_CurFPUOwner);
        }
        g_CurFPUOwner = pCurThread;
        RestoreFloatContext(pCurThread);
        fRet = (pCurThread->ctx.FpExc & VFP_EX_BIT);

        // restore the control registers
        pOEMRestoreVFPCtrlRegs (pCurThread->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);

    } else {
        if (g_fVFPTouched) {
            // we hit an exception, save fpexc and contrl registers
            pOEMSaveVFPCtrlRegs (pCurThread->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
            pCurThread->ctx.FpExc = ReadAndSetFpexc (VFP_ENABLE_BIT);
            DEBUGCHK (pCurThread->ctx.FpExc & VFP_EX_BIT);
            
        } else {
            // we're preempted and switched back before anyone touching VFP
            // do nothing (resotre fpexc, which will be done on the next statement)
        }
        if (fRet = (pCurThread->ctx.FpExc & VFP_EX_BIT))
            SaveFloatContext(pCurThread);
    }

    g_fVFPTouched = TRUE;

    ReadAndSetFpexc (pCurThread->ctx.FpExc = (pCurThread->ctx.FpExc | VFP_ENABLE_BIT) & ~VFP_EX_BIT);

    KCALLPROFOFF(61);

    return fRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FPUFlushContext(void) 
{
    if (VFP_EXIST == vfpStat) {
        if (!InSysCall ())
            KCall ((FARPROC) FPUFlushContext);
        else if (g_CurFPUOwner) {

            // if VFP has been touched, we need to save the control 
            // registers. Otherwise, the control registers have already been
            // saved and we CANNOT save it again.
            if (g_fVFPTouched) {
                DEBUGCHK (pCurThread == g_CurFPUOwner);
                pOEMSaveVFPCtrlRegs (g_CurFPUOwner->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
                g_CurFPUOwner->ctx.FpExc = ReadAndSetFpexc (VFP_ENABLE_BIT);
            }
            // save the general purpose registers
            SaveFloatContext(g_CurFPUOwner);
            
            // disable VFP so we'll reload the control registers
            // on the nexe VFP instruction (SaveFloatContext is destructive
            // to control registers).
            ReadAndSetFpexc (0);
            g_fVFPTouched = FALSE;
            g_CurFPUOwner = 0;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DumpDwords(
    PDWORD pdw,
    int len
    ) 
{
    int lc;
    lc = 0;
    NKDbgPrintfW(L"Dumping %d dwords", len);
    for (lc = 0 ; len ; ++pdw, ++lc, --len) {
        if (!(lc & 3))
            NKDbgPrintfW(L"\r\n%8.8lx -", pdw);
        NKDbgPrintfW(L" %8.8lx", *pdw);
    }
    NKDbgPrintfW(L"\r\n");
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DumpFrame(
    PTHREAD pth,
    PCPUCONTEXT pctx,
    int id,
    ulong addr,
    int level
    ) 
{
    NKDbgPrintfW(L"Exception '%a' Thread=%8.8lx AKY=%8.8lx PC=%8.8lx BVA=%8.8lx\r\n",
            IdStrings[id+1], pth, pCurThread->aky, pctx->Pc, addr);
    NKDbgPrintfW(L" R0=%8.8lx  R1=%8.8lx  R2=%8.8lx  R3=%8.8lx\r\n",
            pctx->R0, pctx->R1, pctx->R2, pctx->R3);
    NKDbgPrintfW(L" R4=%8.8lx  R5=%8.8lx  R6=%8.8lx  R7=%8.8lx\r\n",
            pctx->R4, pctx->R5, pctx->R6, pctx->R7);
    NKDbgPrintfW(L" R8=%8.8lx  R9=%8.8lx R10=%8.8lx R11=%8.8lx\r\n",
            pctx->R8, pctx->R9, pctx->R10, pctx->R11);
    NKDbgPrintfW(L"R12=%8.8lx  SP=%8.8lx  Lr=%8.8lx Psr=%8.8lx\r\n",
            pctx->R12, pctx->Sp, pctx->Lr, pctx->Psr);
}

void NextThread(void);
void KCNextThread(void);
void OEMIdle(void);
ulong OEMInterruptHandler(void);

extern char InterlockedAPIs[], InterlockedEnd[];


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ARMInit(
    int cpuType,
    int abSp,
    int iSp,
    int uSp,
    PPTE pOEMAddressTable
    ) 
{
    int ix;
    /* Initialize SectionTable in KPage */
    for (ix = 1 ; ix <= SECTION_MASK ; ++ix)
        SectionTable[ix] = NULL_SECTION;
    /* Copy kernel data to RAM & zero out BSS */
    KernelRelocate(pTOC);

    /* update g_pOEMAddressTable */
    g_pOEMAddressTable = pOEMAddressTable;
    
    OEMInitDebugSerial();           // initialize serial port
    OEMWriteDebugString((PWSTR)NKSignon);
    /* Copy interlocked api code into the kpage */
    DEBUGCHK(sizeof(KData) <= FIRST_INTERLOCK);
    DEBUGCHK((InterlockedEnd-InterlockedAPIs)+FIRST_INTERLOCK <= 0x400);
    memcpy((char *)&KData+FIRST_INTERLOCK, InterlockedAPIs, InterlockedEnd-InterlockedAPIs);
    /* setup processor version information */
    CEProcessorType = cpuType >> 4 & 0xFFF;
    ProcessorRevision = cpuType & 0x0f;
    NKDbgPrintfW(L"ProcessorType=%4.4x  Revision=%d\r\n", CEProcessorType, ProcessorRevision);
    NKDbgPrintfW(L"sp_abt=%8.8x sp_irq=%8.8x sp_undef=%8.8x OEMAddressTable = %8.8lx\r\n", abSp, iSp, uSp, g_pOEMAddressTable);
    OEMInit();          // initialize firmware
    KernelFindMemory();
    NKDbgPrintfW(L"Sp=%8.8x\r\n", &cpuType);
#ifdef DEBUG
    OEMWriteDebugString(TEXT("ARMInit done.\r\n"));
#endif
}

typedef struct ExcInfo {
    DWORD   linkage;
    ULONG   oldPc;
    UINT    oldMode0;
    char    id;
    BYTE    lowSpBits;
    ushort  fsr;
    ULONG   addr;
} EXCINFO;
typedef EXCINFO *PEXCINFO;

ERRFALSE(sizeof(EXCINFO) <= sizeof(CALLSTACK));
ERRFALSE(offsetof(EXCINFO,linkage) == offsetof(CALLSTACK,pcstkNext));
ERRFALSE(offsetof(EXCINFO,oldPc) == offsetof(CALLSTACK,retAddr));
//ERRFALSE(offsetof(EXCINFO,oldMode) == offsetof(CALLSTACK,pprcLast));
ERRFALSE(64 >= sizeof(CALLSTACK));
ERRFALSE (THUMB_STATE == CST_THUMB_MODE);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PTHREAD 
HandleException(
    PTHREAD pth,
    int id,
    ulong addr,
    ushort info
    ) 
{
    PEXCINFO pexi;
    DWORD stackaddr;
    if (id != ID_RESCHEDULE) {
#if 0
        NKDbgPrintfW(L"%a: Thread=%8.8lx Proc=%8.8lx AKY=%8.8lx\r\n",
            IdStrings[id+1], pth, pCurProc, pCurThread->aky);
        NKDbgPrintfW(L"PC=%8.8lx Lr=%8.8lx Sp=%8.8lx Psr=%4.4x\r\n",
            pth->ctx.Pc, pth->ctx.Lr, pth->ctx.Sp, pth->ctx.Psr);
        if (id == ID_DATA_ABORT)
            NKDbgPrintfW(L"FAR=%8.8lx FSR=%4.4x\r\n", addr, info);
#endif
        
        KCALLPROFON(0);

        pexi = (struct ExcInfo *)((pth->ctx.Sp & ~63) - sizeof(CALLSTACK));

        // before we touch pexi, we need to commit stack or we'll fault while
        // accessing it.
        switch (DemandCommit ((DWORD) pexi, pth)) {
        case DCMT_FAILED:
            // fatal stack error
            NKDbgPrintfW (L"Fatal Stack Error, Terminating thread %8.8lx, pexi = %8.8lx\r\n", pth, pexi);
            DumpFrame(pth, &pth->ctx, id, addr, 10);
            pth->ctx.R0 = STATUS_STACK_OVERFLOW;
            pth->ctx.R1 = pth->ctx.Pc;
            pth->ctx.Sp = (DWORD) pth->tlsPtr - SIZE_PRETLS - 512;  // arbitrary safe address
            pth->ctx.Pc = (DWORD) pExcpExitThread;
            KCALLPROFOFF(0);
            return pth;
        case DCMT_NEW:
            // commited a new page. check if we hit the last page.
            // generate stack overflow exception if yes.
            stackaddr = (DWORD)pexi & ~(PAGE_SIZE-1);
            if ((stackaddr >= KSTKBOUND(pth))
                || ((KSTKBOUND(pth) = stackaddr) >= (KSTKBASE(pth) + MIN_STACK_RESERVE))
                || TEST_STACKFAULT(pth)) {
                KCALLPROFOFF(0);
                return pth; // restart instruction
            }
            SET_STACKFAULT(pth);
            id = ID_STACK_FAULT;   // stack fault exception code
            addr = (DWORD)pexi;
            break;
        case DCMT_OLD:
            // already commited. do nothing
            break;
        default:
            DEBUGCHK (0);
        }

        if (pth->ctx.Pc != (ulong)CaptureContext+4) {
            pexi->id = id;
            pexi->lowSpBits = (uchar)pth->ctx.Sp & 63;
            pexi->oldPc = pth->ctx.Pc;
            //pexi->oldMode = pth->ctx.Psr & 0xFF;
            ((PCALLSTACK) pexi)->dwPrcInfo = CST_IN_KERNEL | ((KERNEL_MODE == GetThreadMode(pth))? 0 : CST_MODE_FROM_USER) | (pth->ctx.Psr & THUMB_STATE);
            pexi->addr = addr;
            pexi->fsr = info;
            pexi->linkage = (DWORD)pth->pcstkTop | 1;
            pth->pcstkTop = (PCALLSTACK)pexi;
            pth->ctx.Sp = (DWORD)pexi;
            if (GetThreadMode(pth) == USER_MODE)
                pth->ctx.Psr = (pth->ctx.Psr & ~0xFF) | SYSTEM_MODE;
            else
                pth->ctx.Psr &= ~THUMB_STATE;
            pth->ctx.Pc = (ULONG)CaptureContext;
            KCALLPROFOFF(0);
            return pth;         // continue execution
        }
        DumpFrame(pth, &pth->ctx, id, addr, 10);
        RETAILMSG(1, (TEXT("Halting thread %8.8lx\r\n"), pth));
        SurrenderCritSecs();
        SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
        RunList.pth = 0;
        SetReschedule();
        KCALLPROFOFF(0);
        return 0;
    }

reschedTop:
    if (ReschedFlag) {
        ReschedFlag = 0;
        NextThread();
    }
    if (KCResched) {
        KCResched = 0;
        KCNextThread();
    }
    if (KCResched)
        goto reschedTop;

    if (!RunList.pth) {
        INTERRUPTS_OFF();
        if (!ReschedFlag && !KCResched) {
            OEMIdle();
            INTERRUPTS_ON();
            ReschedFlag = 1;
            goto reschedTop;
        } else {
            INTERRUPTS_ON();
            goto reschedTop;
        }
    }

    // save the exception state of the thread being preempted if fpu has been touched.
    if (g_fVFPTouched) {
        // read and clear is guaranteed not to generate an exception
        pOEMSaveVFPCtrlRegs (pCurThread->ctx.FpExtra, NUM_EXTRA_CONTROL_REGS);
        pCurThread->ctx.FpExc = ReadAndSetFpexc (0);
        g_fVFPTouched = FALSE;
    }

    _SetCPUASID(RunList.pth);
    hCurThread = RunList.pth->hTh;
    pCurThread = RunList.pth;
    KPlpvTls = RunList.pth->tlsPtr;
    ClearSlots ();

    return RunList.pth;
}



#define DATA_ABORT_HANDLER_OFFSET 0x3F0
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PFNVOID
NKSetDataAbortHandler(
    PFNVOID pfnDataAbortHandler
    ) 
{
    extern DWORD ExceptionVectors[];
    LPDWORD pAddrAbortHandler = &ExceptionVectors[DATA_ABORT_HANDLER_OFFSET/sizeof(DWORD)];
    PFNVOID pfnOriginalHandler;
    
    pfnOriginalHandler = (PFNVOID) *pAddrAbortHandler;
    *pAddrAbortHandler = (DWORD) pfnDataAbortHandler;

    return pfnOriginalHandler;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID
NKCreateStaticMapping(
    DWORD dwPhysBase,
    DWORD dwSize
    ) 
{
    DWORD dwStart, dwEnd;

    dwPhysBase <<= 8;   // ARM only supports 32-bit physical address.

    if (dwStart = (DWORD) Phys2Virt(dwPhysBase)) {
        
        dwEnd = (DWORD) Phys2Virt(dwPhysBase + dwSize - 1);
            
        if (dwStart + dwSize - 1 != dwEnd) {
            //
            // Start of the requested region was already mapped, but size is too big
            //
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return NULL;
        }

        //
        // UNCACHED virtual access only.
        //
        return (LPVOID) (dwStart | 0x20000000);
    }
    return NULL;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID
SC_CreateStaticMapping(
    DWORD dwPhysBase,
    DWORD dwSize
    ) 
{
    TRUSTED_API (L"NKCreateStaticMapping", NULL);

    return NKCreateStaticMapping(dwPhysBase, dwSize);
}

DWORD dwNKARMFirstLevelSetBits;

//------------------------------------------------------------------------------
// LoadPageTable - load entries into page tables from kernel structures
//
//  LoadPageTable is called for prefetch and data aborts to copy page table
// entries from the kernel virtual memory tables to the ARM h/w page tables.
//------------------------------------------------------------------------------
BOOL LoadPageTable(ulong addr)
{
    DWORD   dwSlot, aky = 0;

    if (!IsKernelVa(addr)       // not kernel address
        && (((dwSlot = addr >> VA_SECTION) > MAX_PROCESSES)         // if it's a process slot
            || ((aky = 1 << (dwSlot-1)) & CurAKey))) {              // we need to have access to the slot

        LPDWORD pPtbls;
        DWORD   ix1M, entry;

        DEBUGCHK (dwSlot >= 1);
        switch (dwSlot) {
        case SECURE_SECTION:
            // secure slot, use NK's page table
            pPtbls = ProcArray[0].pPTBL;
            break;
        case MODULE_SECTION:
            // module section (slot 1), use 1st 8 entries of gHwPTBL2G
            pPtbls = gHwPTBL2G;
            break;
        default:
            // either a sloted process address or 2nd gig
            if (dwSlot <= MAX_PROCESSES) {
                DEBUGCHK (dwSlot > 1);
                // process address
                pPtbls = ProcArray[dwSlot-1].pPTBL;
                gdwSlotTouched |= aky;
            } else {
                // in the 2nd gig
                pPtbls = MAPPER_SLOT_TO_PTBL (dwSlot);

                // check object store
                if (IsSlotInObjStore (dwSlot)) {
                    if (!(CurAKey & pFileSysProc->aky))
                        return FALSE;           // don't have access

                    gfObjStoreTouched = TRUE;
                }
            }
            break;
        }

        if ((entry = pPtbls[(addr >> 22) & 7])              // has an entry on the page table
            && !(FirstPT[ix1M = ((addr >> 22) << 2)])) {    // no entry on the hardware pagetable

            int i;

            entry = GetPFN (entry) | PG_COARSE_TBL_BIT | dwNKARMFirstLevelSetBits;     // entry is now the physical
            
            // fill all 4 entries on the 1st level page table
            for (i = 0; i < 4; i ++) {
                FirstPT[ix1M+i] = entry + (i << 10);
            }

            return TRUE;
        }
    }
    return FALSE;
}



void InvalidateRange (PVOID pvAddr, ULONG ulSize)
{
    DWORD dwAddr = (DWORD) pvAddr;

    // work on the sloted address
    if (dwAddr < (1 << VA_SECTION))
        dwAddr += pCurProc->dwVMBase;

    OEMCacheRangeFlush ((LPVOID) dwAddr, ulSize, CACHE_SYNC_FLUSH_TLB);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ExceptionDispatch(
    PCONTEXT pctx
    ) 
{
    EXCEPTION_RECORD er;
    ULONG FaultAddr;
    ULONG addr;
    int id;
    BOOL ThumbMode, fInKMode;
    PTHREAD pth; 
    PEXCINFO pexi;
    PCALLSTACK pcstk = pCurThread->pcstkTop;
    uint fsr;
    DWORD dwThrdInfo = KTHRDINFO (pCurThread);  // need to save it since it might get changed during exception handling
    
    pth = pCurThread;
    pexi = (PEXCINFO)pcstk;
    DEBUGMSG(ZONE_SEH, (TEXT("ExceptionDispatch: pexi=%8.8lx Pc=%8.8lx\r\n"), pexi, pexi->oldPc));
    pctx->Pc = pexi->oldPc;
    SetContextMode(pctx, ((pcstk->dwPrcInfo & CST_MODE_FROM_USER)? USER_MODE : KERNEL_MODE) | (pcstk->dwPrcInfo & THUMB_STATE));
    pctx->Sp = (ULONG)pctx + sizeof(CONTEXT);
    memset(&er, 0, sizeof(er));
    er.ExceptionAddress = (PVOID)pctx->Pc;
#if defined(THUMBSUPPORT)
    ThumbMode = (pctx->Psr & THUMB_STATE) != 0;
#else
    ThumbMode = FALSE;
#endif
    
    // Check for RaiseException call versus a CPU detected exception.
    // RaiseException just becomes a call to CaptureContext as a KPSL.
    // HandleExcepion sets the LSB of the callstack linkage but ObjectCall
    // does not.
    if (!(pexi->linkage & 1)) {
        


        DEBUGMSG(ZONE_SEH, (TEXT("Raising exception %x flags=%x args=%d pexi=%8.8lx\r\n"),
                pctx->R0, pctx->R1, pctx->R2, pexi));
        er.ExceptionCode = pctx->R0;
        er.ExceptionFlags = pctx->R1;
        if (pctx->R3 && pctx->R2) {
            if (pctx->R2 > EXCEPTION_MAXIMUM_PARAMETERS) {
                er.ExceptionCode = STATUS_INVALID_PARAMETER;
                er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            } else {
                memcpy(er.ExceptionInformation, (void*)pctx->R3, pctx->R2*sizeof(DWORD));
                er.NumberParameters = pctx->R2;
            }
        }
        id = -1;
    } else {
        // CPU detected exception. Extract some additional information about
        // the cause of the exception from the EXCINFO (CALLSTACK) structure.

        //
        //  The exception handling code uses bit 0 to indicate Thumb vs. ARM 
        //  execution. This bit is not set by the hardware on exceptions. Set
        //  the bit here:
        //
        if ( ThumbMode ){
            pctx->Pc |= 0x01;
        }
        
        addr = pexi->addr;
        id = pexi->id;
        pctx->Sp += pexi->lowSpBits + sizeof(CALLSTACK);
        fsr = pexi->fsr;
        DEBUGMSG(ZONE_SEH, (TEXT("addr %x, id %x, sp %8.8lx, fsr=%8.8lx, BVA = %8.8lx\r\n"), addr, id, pctx->Sp, fsr, pctx->Pc));
        if (((id == ID_DATA_ABORT) && ((fsr & FSR_STATUS) == FSR_TRANSLATION)) && AutoCommit(addr)) {
            pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
            goto continueExecution;
        }
        switch (id) {
            case ID_UNDEF_INSTR:
                if (VFP_TESTING & vfpStat) {
                    // we get an exception testing if there is a VFP
                    vfpStat = VFP_NOT_EXIST;
                    pctx->Pc += 4;  // skip over the fmrx (must be in ARM mode)
                    pctx->Psr &= ~THUMB_STATE;
                    pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
                    goto continueExecution;
                }
                er.ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
                if ( ThumbMode ){
                    FaultAddr = pctx->Pc & ~0x01;
                    if ( *(PUSHORT)FaultAddr == THM_BREAKPOINT ){
                        er.ExceptionCode = STATUS_BREAKPOINT;
                        er.ExceptionInformation[0] = *(PUSHORT)FaultAddr;
                    }
                } else if ( (*(PULONG)pctx->Pc & 0x0fffffff) == BREAKPOINT ){
                    er.ExceptionCode = STATUS_BREAKPOINT;
                    er.ExceptionInformation[0] = *(PULONG)pctx->Pc;
                } else if (IsVFPInstruction (*(PULONG)pctx->Pc)) {
                    BOOL fFirstAccess, fExcept;
                    if (!(fExcept = KCall((PKFN)SwitchFPUOwner,pctx, &fFirstAccess))
                        && fFirstAccess) {
                        // first time accessing FPU, and not in exception state
                        pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
                        goto continueExecution;
                    }

                    // if we're not in exception state, must be an previlege violation
                    if (!fExcept) {
                        break;
                    }
                    pctx->Fpscr = pth->ctx.Fpscr;
                    pctx->FpExc = pth->ctx.FpExc;
                    memcpy (pctx->S, pth->ctx.S, sizeof (pctx->S));
                    memcpy (pctx->FpExtra, pth->ctx.FpExtra, sizeof (pctx->FpExtra));

                    // floating point exception
                    if (pOEMHandleVFPException (&er, pctx)) {
                        // exception handled, restore thread context
                        pth->ctx.Fpscr = pctx->Fpscr;
                        memcpy (pth->ctx.S, pctx->S, sizeof (pctx->S));
                        memcpy (pth->ctx.FpExtra, pctx->FpExtra, sizeof (pctx->FpExtra));
                        pth->ctx.FpExc = VFP_ENABLE_BIT;
                        RestoreFloatContext (pth);
                        
                        pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
                        goto continueExecution;
                    }
                    // EX bit is already cleared here.
                }
                break;
            case ID_SWI_INSTR:
                er.ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
                break;
            case ID_DATA_ABORT:     // Data page fault
                //
                // Determine load vs. store depending on whether ARM or Thumb
                // mode:
                if ( ThumbMode ){
                    ULONG Instr;
                    FaultAddr = pctx->Pc & ~0x01;               // Clear Thumb bit
                    Instr = *(PUSHORT)FaultAddr;
                    if ((Instr & 0xFE00) == 0x5600){
                        // Discontinuity in the Thumb instruction set:
                        //  LDRSB instruction does not have bit 11 set.
                        er.ExceptionInformation[0] = 0;
                    } else {
                        //  All other load instructions have bit 11 set. The
                        //  corresponding store instructions have bit 11 clear
                        er.ExceptionInformation[0] = !(Instr & (1<<11));
                    }
                } else {
                    FaultAddr = pctx->Pc;
                    er.ExceptionInformation[0] = !((*(PULONG)FaultAddr) & (1<<20));
                }
                switch(fsr & FSR_STATUS) {
                    case FSR_TRANSLATION:
                    case FSR_DOMAIN_ERROR:
                    case FSR_PERMISSION:
        
                        DEBUGMSG(ZONE_PAGING, (L"ExD: ID_DATA_ABORT\r\n"));

                        goto doPageFault;
                    case FSR_ALIGNMENT:
                        er.ExceptionCode = STATUS_DATATYPE_MISALIGNMENT;
                        break;
                    default:
                        er.ExceptionCode = 0xdfff0123;  
                        break;
                }
                break;
            case ID_PREFETCH_ABORT:     // Code page fault
                addr = pctx->Pc;
                
                DEBUGMSG(ZONE_PAGING, (L"ExD: ID_PREFETCH_ABORT\r\n"));

    doPageFault:
                DEBUGMSG(ZONE_PAGING, (L"ExD: addr = %8.8lx\r\n",addr));
                fInKMode = (GetContextMode(pctx) != USER_MODE);

                // er.ExceptionInformation[0] == read or write (1 for write)
                if (!InSysCall () 
                    && (fInKMode || !(addr & 0x80000000))
                    // writing to shared section require KMode access
                    && (!IsInSharedSection(addr) || !er.ExceptionInformation[0] || fInKMode)
                    && ProcessPageFault(er.ExceptionInformation[0], addr)) {

                    pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);

                    DEBUGMSG(ZONE_PAGING, (L"ExD: continuing.  lr = %8.8lx\r\n", pctx->Lr));
                    
                    goto continueExecution;
                }
                er.ExceptionCode = STATUS_ACCESS_VIOLATION;
                er.ExceptionInformation[1] = addr;
                er.NumberParameters = 2;
                break;
            case ID_STACK_FAULT:    // Stack overflow
                er.ExceptionCode = STATUS_STACK_OVERFLOW;
                er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
                break;
        }
    }
    if ((er.ExceptionCode != STATUS_BREAKPOINT) && !IsNoFaultMsgSet ()) {
        // if we faulted in DllMain doing PROCESS_ATTACH, the process name will
        // be pointing to other process's (the creator) address space. Make sure
        // we don't fault on displaying process name.
        LPWSTR pProcName = pCurProc->lpszProcName? pCurProc->lpszProcName : L"";
        LPCWSTR pszNamePC, pszNameRA;
        DWORD dwOfstPC = pctx->Pc, dwOfstRA = pctx->Lr;
        pszNamePC = FindModuleNameAndOffset (dwOfstPC, &dwOfstPC);
        pszNameRA = FindModuleNameAndOffset (dwOfstRA, &dwOfstRA);
        
        if (!((DWORD) pProcName & 0x80000000)
            && (((DWORD) pProcName >> VA_SECTION) != (DWORD) (pCurProc->procnum+1)))
            pProcName = L"";
        NKDbgPrintfW(L"%a: Thread=%8.8lx Proc=%8.8lx '%s'\r\n",IdStrings[id+1], pth, pCurProc, pProcName);
        NKDbgPrintfW(L"AKY=%8.8lx PC=%8.8lx(%s+0x%8.8lx) RA=%8.8lx(%s+0x%8.8lx) BVA=%8.8lx FSR=%8.8lx\r\n", 
            pCurThread->aky, pctx->Pc, pszNamePC, dwOfstPC, pctx->Lr, pszNameRA, dwOfstRA, addr, fsr);
        if (IsNoFaultSet ()) {
            NKDbgPrintfW(L"TLSKERN_NOFAULT set... bypassing kernel debugger.\r\n");
        }
    }
    // Unlink the EXCINFO structure from the threads call stack chain.
    pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);

    if (!(pexi->linkage & 1) && IsValidKPtr (pexi)) {
        // from RaiseException, free the callstack structure
        FreeMem (pexi, HEAP_CALLSTACK);
    }
    // Invoke the kernel debugger to attempt to debug the exception before
    // letting the program resolve the condition via SEH.
    if (!UserDbgTrap(&er, pctx, FALSE) && (IsNoFaultSet () || !HDException(&er, pctx, FALSE))) {
        BOOL bHandled = FALSE;
        // don't pass a break point exception to NKDispatchException
        if (er.ExceptionCode != STATUS_BREAKPOINT) {
            // to prevent recursive exception due to user messed-up TLS
            KTHRDINFO (pth) &= ~UTLS_INKMODE;
            bHandled = NKDispatchException(pth, &er, pctx);
        }
        if (!bHandled && !UserDbgTrap(&er, pctx, TRUE) && !HDException(&er, pctx, TRUE)) {
            if (er.ExceptionCode == STATUS_BREAKPOINT) {
                if (!pvHDNotifyExdi || pctx->Pc < (DWORD)pvHDNotifyExdi || pctx->Pc > ((DWORD)pvHDNotifyExdi + HD_NOTIFY_MARGIN))
                    RETAILMSG(1, (TEXT("DEBUG_BREAK @%8.8lx MD=%2x Ignored.\r\n"), pctx->Pc,
                            pctx->Psr & 0xFF));
                pctx->Pc += ThumbMode ? 2 : 4;  // skip over the BREAK instruction
            } else {
                // Terminate the process.
                RETAILMSG(1, (TEXT("\r\nUnhandled exception %8.8lx:\r\n"), er.ExceptionCode));
                if (InSysCall()) {
                    DumpFrame(pth, (PCPUCONTEXT)&pctx->Psr, id, addr, 0);
                    lpNKHaltSystem ();
                    FakeNKHaltSystem ();
                } else {
                    if (!GET_DEAD(pth)) {
                        pcstk = pth->pcstkTop;
                        while (pcstk && !pcstk->akyLast) {
                            pth->pcstkTop = (PCALLSTACK) ((DWORD) pcstk->pcstkNext & ~1);
                            if (IsValidKPtr (pcstk)) {
                                FreeMem (pcstk, HEAP_CALLSTACK);
                            }
                            pcstk = pth->pcstkTop;
                        }
                        DEBUGCHK (!pcstk);   // should this happen, we have a fault in callback that wasn't handled
                                             // by PSL
                        // clean up all the temporary callstack
                        if (er.ExceptionCode == STATUS_STACK_OVERFLOW) {
                            // stack overflow, not much we can do. Make sure we have enough room to run
                            // pExcpExitThread
                            // randomly picked a valid SP
                            pctx->Sp = (DWORD) pth->tlsPtr - SECURESTK_RESERVE - (PAGE_SIZE >> 1);
                        }
                        SET_DEAD(pth);
                        //pth->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_TRYINGTODIE;
                        pctx->Pc = (ULONG)pExcpExitThread;
                        pctx->R0 = er.ExceptionCode;    // argument: exception code
                        pctx->R1 = (ULONG)er.ExceptionAddress; // argument: exception address
                        RETAILMSG(1, (TEXT("Terminating thread %8.8lx\r\n"), pth));
                    } else {
                        DumpFrame(pth, (PCPUCONTEXT)&pctx->Psr, id, addr, 0);
                        RETAILMSG(1, (TEXT("Can't terminate thread %8.8lx, sleeping forever\r\n"), pth));
                        SurrenderCritSecs();
                        Sleep(INFINITE);
                        DEBUGCHK(0);    // should never get here
                    }
                }
            }
        }
    }
    if ((id == ID_DATA_ABORT) || (id == ID_PREFETCH_ABORT))
        GuardCommit(addr);
continueExecution:

#if defined(THUMBSUPPORT)
    //
    // Update PSR based on ARM/Thumb continuation:
    //
    if ( pctx->Pc & 0x01 ){
        pctx->Psr |= THUMB_STATE;
    } else {
        pctx->Psr &= ~THUMB_STATE;
    }
#endif
    
    // restore ThrdInfo
    KTHRDINFO (pth) = dwThrdInfo;

    // If returning from handling a stack overflow, reset the thread's stack overflow
    // flag. It would be good to free the tail of the stack at this time
    // so that the thread will stack fault again if the stack gets too big. But we
    // are currently using that stack page.
    if (id == ID_STACK_FAULT)
        CLEAR_STACKFAULT(pth);
    if (GET_DYING(pth) && !GET_DEAD(pth) && (pCurProc == pth->pOwnerProc)) {
        SET_DEAD(pth);
        CLEAR_USERBLOCK(pth);
        CLEAR_DEBUGWAIT(pth);
        pctx->Pc = (ULONG)pExcpExitThread;
        pctx->R0 = er.ExceptionCode;    // argument: exception code
        pctx->R1 = (ULONG)er.ExceptionAddress; // argument: exception address
    }   
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PVOID 
Phys2Virt(
    DWORD pfn
    ) 
{
    int i = 0;
    DWORD va;       // Virtual Base Address of section
    DWORD pa;       // Physical Base Address of section
    DWORD pau;      // Physical Address Upper Bound of section
    DWORD pfnmb;    // PFN rounded down to 1MB
    //
    // The end of the table is marked by an entry with a ZERO size.
    //
    while(g_pOEMAddressTable[i].dwSize) {
        va = g_pOEMAddressTable[i].dwVA & 0x1FF00000;
        pa = g_pOEMAddressTable[i].dwPA & 0xFFF00000;
        pau = pa + (g_pOEMAddressTable[i].dwSize << 20) - 1;
        pfnmb = pfn & 0xfff00000;
        if ((pfnmb >= pa) && (pfnmb <= pau))
            return ((PVOID) ((pfn - pa) + va + 0x80000000));
        i++;
    }
    DEBUGMSG(ZONE_PHYSMEM, (TEXT("Phys2Virt() : PFN (0x%08X) not found!\r\n"), pfn));
    return NULL;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID MDValidateKVA (DWORD dwAddr)
{
    // kernel data and page table territory...
    if (   ((dwAddr >= 0xFFFD0000) && (dwAddr < 0xFFFD4000))
        || ((dwAddr >= 0xFFFF0000) && (dwAddr < 0xFFFF1000))
        || ((dwAddr >= 0xFFFF2000) && (dwAddr < 0xFFFF3000))
        || ((dwAddr >= 0xFFFF4000) && (dwAddr < 0xFFFF5000))
        || ((dwAddr >= 0xFFFFC000) && (dwAddr < 0xFFFFD000)))
        return (LPVOID) dwAddr;

    if (dwAddr < 0xc0000000) {
        int i;
        DWORD dwCachedAddr = dwAddr & ~0x20000000;  // use cached address to do comparision
        
        // check the OEM address map to see if the address we've been 
        // handed is indeed mapped...
        for (i = 0; g_pOEMAddressTable[i].dwSize; i ++) {
            if((g_pOEMAddressTable[i].dwVA <= dwCachedAddr)
                && (dwCachedAddr < (g_pOEMAddressTable[i].dwVA + (g_pOEMAddressTable[i].dwSize << 20)))) {
                return (LPVOID) dwAddr;
            }
        }
    }
    return NULL;
}

/* Machine dependent constants */
const DWORD cbMDStkAlign = 8;                   // stack 8 bytes aligned

void MDInitSecureStack(LPBYTE lpStack)
{
}

/* Machine dependent thread creation */
// normal thread stack: from top, TLS then PRETLS then args then free
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
MDCreateThread(
    PTHREAD pTh,
    LPVOID lpStack,
    DWORD cbStack,
    LPVOID lpBase,
    LPVOID lpStart,
    BOOL kmode,
    ulong param
    ) 
{
    DEBUGCHK ((ulong)lpStack>>VA_SECTION);

    pTh->tlsPtr = TLSPTR(lpStack, cbStack);
    KSTKBASE(pTh) = (DWORD)lpStack;

    // Leave room for arguments and TLS and PRETLS on the stack
    pTh->ctx.Sp = (ulong)pTh->tlsPtr - SIZE_PRETLS;
    KSTKBOUND(pTh) = pTh->ctx.Sp & ~(PAGE_SIZE-1);
    pTh->ctx.R0 = (ulong)lpStart;
    pTh->ctx.R1 = param;
    pTh->ctx.Lr = 4;
    pTh->ctx.Pc = (ULONG)lpBase;
    if (kmode || bAllKMode) {
        pTh->ctx.Psr = KERNEL_MODE;
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        pTh->ctx.Psr = USER_MODE;
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
    }
    pTh->ctx.Fpscr = 0;
    pTh->ctx.FpExc = 0;
    memset (pTh->ctx.FpExtra, 0, sizeof (pTh->ctx.FpExtra));
#if defined(THUMBSUPPORT)
    if ( (pTh->ctx.Pc & 0x01) != 0 ){
        pTh->ctx.Psr |= THUMB_STATE;
    }
#endif

    DEBUGMSG(ZONE_SCHEDULE, (L"MDCT: pTh=%8.8lx Pc=%8.8lx Psr=%4.4x GP=%8.8lx Sp=%8.8lx\r\n", pTh, pTh->ctx.Pc, pTh->ctx.Psr, pTh->ctx.R9, pTh->ctx.Sp));
}

// main thread stack: from top, TLS then buf then buf2 then buf2 (ascii) then args then free
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
MDSetupMainThread (
    PTHREAD pTh,
    LPBYTE pCurSP,
    LPVOID lpBase,
    LPVOID lpStart,
    BOOL kmode,
    ulong dwModCnt
    ) 
{
    LPDWORD pArgs = ((LPDWORD) pCurSP) - 4; // 4 arguments on stack
    PPROCESS pprc = pTh->pOwnerProc;

    pTh->ctx.Sp = (ulong)pArgs;
    KSTKBOUND(pTh) = pTh->ctx.Sp & ~(PAGE_SIZE-1);
    pTh->ctx.R0 = (ulong) lpStart;           // arg #0
    pTh->ctx.R1 = (ulong) pprc->hProc;       // arg #1 - hInstance
    pTh->ctx.R2 = dwModCnt;                  // arg #2
    pTh->ctx.R3 = (ulong)pprc->pcmdline;     // arg #3 - cmdline
    pArgs[0] = (DWORD) hCoreDll;             // arg #4
    pArgs[1] = pprc->e32.e32_sect14rva;      // arg #5
    pArgs[2] = pprc->e32.e32_sect14size;     // arg #6
    pArgs[3] = (DWORD) pprc->BasePtr;        // arg #7
    
    pTh->ctx.Lr = 4;
    pTh->ctx.Pc = (ULONG)lpBase;
    if (kmode || bAllKMode) {
        pTh->ctx.Psr = KERNEL_MODE;
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        pTh->ctx.Psr = USER_MODE;
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
    }
    pTh->ctx.Fpscr = 0;
    pTh->ctx.FpExc = 0;
    memset (pTh->ctx.FpExtra, 0, sizeof (pTh->ctx.FpExtra));
#if defined(THUMBSUPPORT)
    if ( (pTh->ctx.Pc & 0x01) != 0 ){
        pTh->ctx.Psr |= THUMB_STATE;
    }
#endif

    DEBUGMSG(ZONE_SCHEDULE, (L"MDCMainT: pTh=%8.8lx Pc=%8.8lx Psr=%4.4x TOC=%8.8lx Sp=%8.8lx\r\n", pTh, pTh->ctx.Pc, pTh->ctx.Psr, pTh->ctx.R9, pTh->ctx.Sp));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
DoThreadGetContext(
    HANDLE hTh,
    PCONTEXT lpContext
    ) 
{
    PTHREAD pth;
    if (!(pth = HandleToThread(hTh))) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (lpContext->ContextFlags & ~CONTEXT_FULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pth->pThrdDbg && pth->pThrdDbg->psavedctx) {
        ULONG ulOldAky = CurAKey;
        SETCURKEY((DWORD)-1);
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            lpContext->Psr = pth->pThrdDbg->psavedctx->Psr;
            lpContext->Pc = pth->pThrdDbg->psavedctx->Pc;
            lpContext->Lr = pth->pThrdDbg->psavedctx->Lr;
            lpContext->Sp = pth->pThrdDbg->psavedctx->Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            lpContext->R0 = pth->pThrdDbg->psavedctx->R0;
            lpContext->R1 = pth->pThrdDbg->psavedctx->R1;
            lpContext->R2 = pth->pThrdDbg->psavedctx->R2;
            lpContext->R3 = pth->pThrdDbg->psavedctx->R3;
            lpContext->R4 = pth->pThrdDbg->psavedctx->R4;
            lpContext->R5 = pth->pThrdDbg->psavedctx->R5;
            lpContext->R6 = pth->pThrdDbg->psavedctx->R6;
            lpContext->R7 = pth->pThrdDbg->psavedctx->R7;
            lpContext->R8 = pth->pThrdDbg->psavedctx->R8;
            lpContext->R9 = pth->pThrdDbg->psavedctx->R9;
            lpContext->R10 = pth->pThrdDbg->psavedctx->R10;
            lpContext->R11 = pth->pThrdDbg->psavedctx->R11;
            lpContext->R12 = pth->pThrdDbg->psavedctx->R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            lpContext->Fpscr = pth->pThrdDbg->psavedctx->Fpscr;
            lpContext->FpExc = pth->pThrdDbg->psavedctx->FpExc;
            memcpy (lpContext->S, pth->pThrdDbg->psavedctx->S, sizeof (lpContext->S));
        }
        SETCURKEY(ulOldAky);
    } else {
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            lpContext->Psr = pth->ctx.Psr;
            lpContext->Pc = pth->ctx.Pc;
            lpContext->Lr = pth->ctx.Lr;
            lpContext->Sp = pth->ctx.Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            lpContext->R0 = pth->ctx.R0;
            lpContext->R1 = pth->ctx.R1;
            lpContext->R2 = pth->ctx.R2;
            lpContext->R3 = pth->ctx.R3;
            lpContext->R4 = pth->ctx.R4;
            lpContext->R5 = pth->ctx.R5;
            lpContext->R6 = pth->ctx.R6;
            lpContext->R7 = pth->ctx.R7;
            lpContext->R8 = pth->ctx.R8;
            lpContext->R9 = pth->ctx.R9;
            lpContext->R10 = pth->ctx.R10;
            lpContext->R11 = pth->ctx.R11;
            lpContext->R12 = pth->ctx.R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            lpContext->Fpscr = pth->ctx.Fpscr;
            lpContext->FpExc = pth->ctx.FpExc;
            memcpy (lpContext->S, pth->ctx.S, sizeof (lpContext->S));
            memcpy (lpContext->FpExtra, pth->ctx.FpExtra, sizeof (lpContext->FpExtra));
        }
    }
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
DoThreadSetContext(
    HANDLE hTh,
    const CONTEXT *lpContext
    ) 
{
    PTHREAD pth;
    if (!(pth = HandleToThread(hTh))) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (lpContext->ContextFlags & ~CONTEXT_FULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pth->pThrdDbg && pth->pThrdDbg->psavedctx) {
        ULONG ulOldAky = CurAKey;
        SETCURKEY((DWORD)-1);
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            pth->pThrdDbg->psavedctx->Psr = (pth->pThrdDbg->psavedctx->Psr & 0xdf) | (lpContext->Psr & ~0xdf);
            pth->pThrdDbg->psavedctx->Pc = lpContext->Pc;
            pth->pThrdDbg->psavedctx->Lr = lpContext->Lr;
            pth->pThrdDbg->psavedctx->Sp = lpContext->Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            pth->pThrdDbg->psavedctx->R0 = lpContext->R0;
            pth->pThrdDbg->psavedctx->R1 = lpContext->R1;
            pth->pThrdDbg->psavedctx->R2 = lpContext->R2;
            pth->pThrdDbg->psavedctx->R3 = lpContext->R3;
            pth->pThrdDbg->psavedctx->R4 = lpContext->R4;
            pth->pThrdDbg->psavedctx->R5 = lpContext->R5;
            pth->pThrdDbg->psavedctx->R6 = lpContext->R6;
            pth->pThrdDbg->psavedctx->R7 = lpContext->R7;
            pth->pThrdDbg->psavedctx->R8 = lpContext->R8;
            pth->pThrdDbg->psavedctx->R9 = lpContext->R9;
            pth->pThrdDbg->psavedctx->R10 = lpContext->R10;
            pth->pThrdDbg->psavedctx->R11 = lpContext->R11;
            pth->pThrdDbg->psavedctx->R12 = lpContext->R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            pth->pThrdDbg->psavedctx->Fpscr = lpContext->Fpscr;
            pth->pThrdDbg->psavedctx->FpExc = lpContext->FpExc;
            memcpy (pth->pThrdDbg->psavedctx->S, lpContext->S, sizeof (lpContext->S));
        }
        SETCURKEY(ulOldAky);
    } else {
        if (lpContext->ContextFlags & CONTEXT_CONTROL) {
            pth->ctx.Psr = (pth->ctx.Psr & 0xdf) | (lpContext->Psr & ~0xdf);
            pth->ctx.Pc = lpContext->Pc;
            pth->ctx.Lr = lpContext->Lr;
            pth->ctx.Sp = lpContext->Sp;
        }
        if (lpContext->ContextFlags & CONTEXT_INTEGER) {
            pth->ctx.R0 = lpContext->R0;
            pth->ctx.R1 = lpContext->R1;
            pth->ctx.R2 = lpContext->R2;
            pth->ctx.R3 = lpContext->R3;
            pth->ctx.R4 = lpContext->R4;
            pth->ctx.R5 = lpContext->R5;
            pth->ctx.R6 = lpContext->R6;
            pth->ctx.R7 = lpContext->R7;
            pth->ctx.R8 = lpContext->R8;
            pth->ctx.R9 = lpContext->R9;
            pth->ctx.R10 = lpContext->R10;
            pth->ctx.R11 = lpContext->R11;
            pth->ctx.R12 = lpContext->R12;
        }
        if ((VFP_EXIST == vfpStat) && (lpContext->ContextFlags & CONTEXT_FLOATING_POINT)) {
            FPUFlushContext();
            pth->ctx.Fpscr = lpContext->Fpscr;
            pth->ctx.FpExc = lpContext->FpExc;
            memcpy (pth->ctx.S, lpContext->S, sizeof (lpContext->S));
            memcpy (pth->ctx.FpExtra, lpContext->FpExtra, sizeof (lpContext->FpExtra));
        }
    }
    return TRUE;
}

//
// Define procedure prototypes for exception filter and termination handler
// execution routines defined in jmpunwnd.s
//

// For ARM the compiler generated exception functions expect the following
// input arguments:
//
//  Exception Filter: r0 = exception pointers, r11 = frame pointer
//
//  Termination Handler: r0 = abnormal term flag, r11 = frame pointer



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
long 
__declspec(
    iw32
    ) __C_ExecuteExceptionFilter(
    PEXCEPTION_POINTERS ExceptionPointers,
    EXCEPTION_FILTER ExceptionFilter,
    ULONG EstablisherFrame);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
__declspec(
    iw32
    ) __C_ExecuteTerminationHandler(
    BOOLEAN AbnormalTermination,
    TERMINATION_HANDLER TerminationHandler,
    ULONG EstablisherFrame);

/*++
Routine Description:
    This function scans the scope tables associated with the specified
    procedure and calls exception and termination handlers as necessary.

Arguments:
    ExceptionRecord - Supplies a pointer to an exception record.

    EstablisherFrame - Supplies a pointer to frame of the establisher function.

    ContextRecord - Supplies a pointer to a context record.

    DispatcherContext - Supplies a pointer to the exception dispatcher or
        unwind dispatcher context.

Return Value:
    If the exception is handled by one of the exception filter routines, then
    there is no return from this routine and RtlUnwind is called. Otherwise,
    an exception disposition value of continue execution or continue search is
    returned.
--*/
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
EXCEPTION_DISPOSITION 
__C_specific_handler(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext
    ) 
{
    ULONG ControlPc;
    EXCEPTION_FILTER ExceptionFilter;
    EXCEPTION_POINTERS ExceptionPointers;
    PRUNTIME_FUNCTION FunctionEntry;
    ULONG Index;
    PSCOPE_TABLE ScopeTable;
    ULONG TargetPc;
    TERMINATION_HANDLER TerminationHandler;
    LONG Value;
    
    // Get address of where control left the establisher, the address of the
    // function table entry that describes the function, and the address of
    // the scope table.
    ControlPc = DispatcherContext->ControlPc;
    FunctionEntry = DispatcherContext->FunctionEntry;
    ScopeTable = (PSCOPE_TABLE)(FunctionEntry->HandlerData);
    // If an unwind is not in progress, then scan the scope table and call
    // the appropriate exception filter routines. Otherwise, scan the scope
    // table and call the appropriate termination handlers using the target
    // PC obtained from the context record.
    // are called.
    if (IS_DISPATCHING(ExceptionRecord->ExceptionFlags)) {
        // Scan the scope table and call the appropriate exception filter
        // routines.
        ExceptionPointers.ExceptionRecord = ExceptionRecord;
        ExceptionPointers.ContextRecord = ContextRecord;
        for (Index = 0; Index < ScopeTable->Count; Index += 1) {
            if ((ControlPc >= ScopeTable->ScopeRecord[Index].BeginAddress) &&
                    (ControlPc < ScopeTable->ScopeRecord[Index].EndAddress) &&
                    (ScopeTable->ScopeRecord[Index].JumpTarget != 0)) {
                // Call the exception filter routine.
                ExceptionFilter =
                    (EXCEPTION_FILTER)ScopeTable->ScopeRecord[Index].HandlerAddress;
                Value = __C_ExecuteExceptionFilter(&ExceptionPointers,ExceptionFilter,(ULONG)EstablisherFrame);
                // If the return value is less than zero, then dismiss the
                // exception. Otherwise, if the value is greater than zero,
                // then unwind to the target exception handler. Otherwise,
                // continue the search for an exception filter.
                if (Value < 0)
                    return ExceptionContinueExecution;
                if (Value > 0) {
                    DispatcherContext->ControlPc = ScopeTable->ScopeRecord[Index].JumpTarget;
                    return ExceptionExecuteHandler;
                }
            }
        }
    } else {
        // Scan the scope table and call the appropriate termination handler
        // routines.
        TargetPc = CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
        for (Index = 0; Index < ScopeTable->Count; Index += 1) {
            if ((ControlPc >= ScopeTable->ScopeRecord[Index].BeginAddress) &&
                    (ControlPc < ScopeTable->ScopeRecord[Index].EndAddress)) {
                // If the target PC is within the same scope the control PC
                // is within, then this is an uplevel goto out of an inner try
                // scope or a long jump back into a try scope. Terminate the
                // scan termination handlers.
                //
                // N.B. The target PC can be just beyond the end of the scope,
                //      in which case it is a leave from the scope.
                if ((TargetPc >= ScopeTable->ScopeRecord[Index].BeginAddress) &&
                    (TargetPc < ScopeTable->ScopeRecord[Index].EndAddress))
                    break;
                // If the scope table entry describes an exception filter
                // and the associated exception handler is the target of
                // the unwind, then terminate the scan for termination
                // handlers. Otherwise, if the scope table entry describes
                // a termination handler, then record the address of the
                // end of the scope as the new control PC address and call
                // the termination handler.
                if (ScopeTable->ScopeRecord[Index].JumpTarget != 0) {
                    if (TargetPc == ScopeTable->ScopeRecord[Index].JumpTarget)
                        break;
                } else {
                    DispatcherContext->ControlPc = ScopeTable->ScopeRecord[Index].EndAddress + 4;
                    TerminationHandler = (TERMINATION_HANDLER)ScopeTable->ScopeRecord[Index].HandlerAddress;
                    __C_ExecuteTerminationHandler(TRUE, TerminationHandler, (ULONG)EstablisherFrame);
                }
            }
        }
    }
    // Continue search for exception or termination handlers.
    return ExceptionContinueSearch;
}

void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    ULONG *pRegs = (ULONG *) pcstk->dwPrevSP;

    pCtx->R4  = pRegs[0];
    pCtx->R5  = pRegs[1];
    pCtx->R6  = pRegs[2];
    pCtx->R7  = pRegs[3];
    pCtx->R8  = pRegs[4];
    pCtx->R9  = pRegs[5];
    pCtx->R10 = pRegs[6];
    pCtx->R11 = pRegs[7];
}


