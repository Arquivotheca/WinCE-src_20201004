//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//
//
//------------------------------------------------------------------------------
#include "kernel.h"
#include "shx.h"

extern void UnusedHandler(void);
extern void OEMInitDebugSerial(void);
extern void InitClock(void);
extern void LoadKPage(void);
extern void DumpFrame(PTHREAD pth, PCONTEXT pctx, DWORD dwExc, DWORD info);
extern void APICallReturn(void);

#ifdef SH4
void SaveFloatContext(PTHREAD);
void RestoreFloatContext(PTHREAD);
DWORD GetAndClearFloatCode(void);
DWORD GetAndClearFloatCode(void);
DWORD GetCauseFloatCode(void);
extern BOOL HandleHWFloatException(EXCEPTION_RECORD *ExceptionRecord,
                                   PCONTEXT pctx);
extern unsigned int get_fpscr();
extern void set_fpscr(unsigned int);
extern void clr_fpscr(unsigned int);
extern void DisableFPU();
#endif


#ifdef SH3
// frequency control register value
extern unsigned short SH3FQCR_Fast;
extern unsigned int SH3DSP;
void SaveSH3DSPContext(PTHREAD);
void RestoreSH3DSPContext(PTHREAD);
#endif

#if defined (SH3)
const wchar_t NKCpuType [] = TEXT("SH-3");
#elif   defined (SH4)
const wchar_t NKCpuType [] = TEXT("SH-4");
#else
#error "Unknown CPU"
#endif

// OEM definable extra bits for the Cache Control Register
unsigned long OEMExtraCCR;

extern char InterlockedAPIs[], InterlockedEnd[];

extern void (*lpNKHaltSystem)(void);
extern void FakeNKHaltSystem (void);

MEMBLOCK *MDAllocMemBlock (DWORD dwBase, DWORD ixBlock)
{
    MEMBLOCK *pmb = (MEMBLOCK *) AllocMem (HEAP_MEMBLOCK);
    if (pmb)
        memset (pmb, 0, sizeof (MEMBLOCK));
    return pmb;
}

void MDFreeMemBlock (MEMBLOCK *pmb)
{
    DEBUGCHK (pmb);
    FreeMem (pmb, HEAP_MEMBLOCK);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SH3Init(
    int cpuType
    ) 
{
    int ix;
#ifdef SH3
    /* initialize frequency control register */
    if (SH3FQCR_Fast)
        *(volatile ushort *)0xffffff80 = SH3FQCR_Fast;
#endif

    /* Disable the cpu cache & flush it. */
    CCR = 0;
    CCR = CACHE_FLUSH;

    /* Zero out kernel data page. */
    memset(&KData, 0, sizeof(KData));
    KData.handleBase = 0x80000000;
    KData.pAPIReturn = (ulong)APICallReturn;

    /* Initialize SectionTable in KPage */
    for (ix = 1 ; ix <= SECTION_MASK ; ++ix)
        SectionTable[ix] = NULL_SECTION;

    /* Copy kernel data to RAM & zero out BSS */
    KernelRelocate(pTOC);

    OEMInitDebugSerial();           // initialize serial port
    OEMWriteDebugString(TEXT("\r\nWindows CE Kernel for Hitachi SH Built on ") TEXT(__DATE__)
            TEXT(" at ") TEXT(__TIME__) TEXT("\r\n"));
#if defined(SH4)
    OEMWriteDebugString(TEXT("SH-4 Kernel\r\n"));
#else
    NKDbgPrintfW(L"SH-3 Kernel. FQCR=%x\r\n", *(volatile ushort *)0xffffff80);
#endif

    /* Initialize address translation hardware. */
    MMUTEA = 0;         /* clear transation address */
    MMUTTB = (DWORD)SectionTable; /* set translation table base address */
    MMUPTEH = 0;        /* clear ASID */
    MMUCR = TLB_FLUSH | TLB_ENABLE;
    LoadKPage();

    /* Copy interlocked api code into the kpage */
    DEBUGCHK(sizeof(KData) <= FIRST_INTERLOCK);
    DEBUGCHK((InterlockedEnd-InterlockedAPIs)+FIRST_INTERLOCK == 0x400);
    memcpy((char *)&KData+FIRST_INTERLOCK, InterlockedAPIs, InterlockedEnd-InterlockedAPIs);

    // Enable the CPU cache. Can't do this before KernelRelocate because OEMExtraCCR
    // may not be properly initialized before that point.
    CCR = CACHE_ENABLE | OEMExtraCCR;
    NKDbgPrintfW(L"CCR=%4.4x\r\n", CCR);

    OEMInit();          // initialize firmware
    KernelFindMemory();
#ifdef DEBUG
    OEMWriteDebugString(TEXT("SH3Init done.\r\n"));
#endif
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    if (hwInterruptNumber > 112)
        return FALSE;
    InterruptTable[hwInterruptNumber] = (DWORD)pfnHandler;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UnhookInterrupt(
    int hwInterruptNumber,
    FARPROC pfnHandler
    ) 
{
    if (hwInterruptNumber > 112 ||
        InterruptTable[hwInterruptNumber] != (DWORD)pfnHandler)
        return FALSE;
    InterruptTable[hwInterruptNumber] = (DWORD)UnusedHandler;
    return TRUE;
}

#ifdef SH4

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SwitchFPUOwner(
    PCONTEXT pctx
    ) 
{
    KCALLPROFON(61);
    if (g_CurFPUOwner != pCurThread) {
        if (g_CurFPUOwner)
            SaveFloatContext(g_CurFPUOwner);
        g_CurFPUOwner = pCurThread;
        RestoreFloatContext(pCurThread);
    }
    KCALLPROFOFF(61);
    pctx->Psr &= ~SR_FPU_DISABLED;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FPUFlushContext(void) 
{
    if (!InSysCall())
        KCall ((FARPROC) FPUFlushContext);
    else if (g_CurFPUOwner) {
        SaveFloatContext(g_CurFPUOwner);
        g_CurFPUOwner->ctx.Psr |= SR_FPU_DISABLED;
        DisableFPU();
        g_CurFPUOwner = 0;
    }
}

DWORD dwStoreQueueBase;
BOOL DoSetRAMMode(BOOL bEnable, LPVOID *lplpvAddress, LPDWORD lpLength);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_SetRAMMode(
    BOOL bEnable,
    LPVOID *lplpvAddress,
    LPDWORD lpLength
    ) 
{
    TRUSTED_API (L"SC_SetRAMMode", FALSE);

    return DoSetRAMMode(bEnable, lplpvAddress, lpLength);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID 
SC_SetStoreQueueBase(
    DWORD dwPhysPage
    ) 
{
    TRUSTED_API (L"SC_SetStoreQueueBase", NULL);
    
    if (dwPhysPage & (1024*1024-1)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (dwPhysPage & 0xe0000000) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return 0;
    }
    dwStoreQueueBase = dwPhysPage | PG_VALID_MASK | PG_1M_MASK | PG_PROT_WRITE | PG_DIRTY_MASK | 1;
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_ALL);
    return (LPVOID)0xE0000000;
}

#else   // !SH4



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SwitchDSPOwner(
    PCONTEXT pctx
    ) 
{
    KCALLPROFON(68);
    if (g_CurDSPOwner != pCurThread) {
        if (g_CurDSPOwner)
            SaveSH3DSPContext(g_CurDSPOwner);
        g_CurDSPOwner = pCurThread;
        RestoreSH3DSPContext(pCurThread);
    }
    pctx->Psr |= SR_DSP_ENABLED;            // enable the DSP
    KCALLPROFOFF(68);
}

void DSPFlushContext(void) {
    if (g_CurDSPOwner) {
        SaveSH3DSPContext(g_CurDSPOwner);
        g_CurDSPOwner->ctx.Psr &= ~SR_DSP_ENABLED;  // disable the DSP
        g_CurDSPOwner = 0;
    }
}



#endif



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID
NKCreateStaticMapping(
    DWORD dwPhysBase,
    DWORD dwSize
    ) 
{
    dwPhysBase <<= 8;   // Only supports 32-bit physical address.
    
    if (dwPhysBase < 0x20000000 && ((dwPhysBase + dwSize) < 0x20000000)) {
        return Phys2VirtUC(dwPhysBase);
    } else {
        return NULL;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPVOID MDValidateKVA (DWORD dwAddr)
{
    return ((dwAddr < 0xC0000000) || (dwAddr >= 0xE0000000))        // between 0x80000000 - 0xc0000000
                                                                    // or >= 0xE0000000
         ? (LPVOID) dwAddr : NULL;
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


typedef struct ExcInfo {
    DWORD   linkage;
    ULONG   oldFir;
    UINT    oldMode0;
    UCHAR   exc;
    UCHAR   lowSp;
    UCHAR   pad[2];
    ULONG   info;
} EXCINFO, *PEXCINFO;

ERRFALSE(sizeof(EXCINFO) <= sizeof(CALLSTACK));
ERRFALSE(offsetof(EXCINFO,linkage) == offsetof(CALLSTACK,pcstkNext));
ERRFALSE(offsetof(EXCINFO,oldFir) == offsetof(CALLSTACK,retAddr));
//ERRFALSE(offsetof(EXCINFO,oldMode) == offsetof(CALLSTACK,pprcLast));
ERRFALSE(64 >= sizeof(CALLSTACK));



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
HandleException(
    PTHREAD pth,
    DWORD dwExc,
    DWORD info
    ) 
{
    PEXCINFO pexi;
    DWORD stackaddr;
    BOOL fTlsOk;
    KCALLPROFON(0);
#if 0
    NKDbgPrintfW(L"Exception %03x Thread=%8.8lx(%8.8lx) PC=%8.8lx TEA=%8.8lx AKY=%8.8lx\r\n",
                 dwExc, pCurThread, pth, pth->ctx.Fir, info, CurAKey);
#endif

    if (IsInPwrHdlr ()) {
        // Calling API in Power Handler
        NKDbgPrintfW (L"!Unrecoverable Error: Exception or calling API inside Power Handler\r\n");
        NKDbgPrintfW(L"Exception %03x Thread=%8.8lx(%8.8lx) PC=%8.8lx TEA=%8.8lx AKY=%8.8lx SP=%8.8lx\r\n",
                 dwExc, pCurThread, pth, pth->ctx.Fir, info, CurAKey, pth->ctx.R15);
        lpNKHaltSystem ();
        FakeNKHaltSystem ();
    }

    if (dwExc == 0x160) pth->ctx.Fir -= 2; // TRAPA instruction was completed

    // avoid crossing page boundary in structure
    pexi = (struct ExcInfo *)((pth->ctx.R15 & ~63) - sizeof(CALLSTACK));
    // before we use pexi, we need to validate if it's valid, or we'll fault in DemandCommit and crash
    if ((KERN_TRUST_FULL != pth->pProc->bTrustLevel)    // not fully trusted
        && (pth->tlsPtr != pth->tlsSecure)              // not on secure stack
        && !ValidateStack (pth, (DWORD) pexi, &fTlsOk)) {        // not a valid SP (or TLS trashed)
        SET_STACKFAULT(pth);
        dwExc = 0x1fe0;
        info = (DWORD)pexi;

        // fix SP and TLS so we don't fault in CaptureContext
        pth->ctx.R15 = FixTlsAndSP (pth, !fTlsOk);
        pexi = (struct ExcInfo *)((pth->ctx.R15 & ~63) - sizeof(CALLSTACK));

        // set PC = 0 so NKDispatchException will fail.
        pth->ctx.Fir = 0;

        if (pth->pcstkTop) {
            // mark the return address to be the PSL boundary so we'll unwind correctly
            pth->ctx.PR = SYSCALL_RETURN;
        }
    } else if (!IsKernelVa(pexi) && DemandCommit((DWORD)pexi)) {
        stackaddr = (DWORD)pexi & ~(PAGE_SIZE-1);
        if ((stackaddr >= KSTKBOUND(pth)) || (stackaddr < KSTKBASE(pth)) ||
            ((KSTKBOUND(pth) = stackaddr) >= (KSTKBASE(pth) + MIN_STACK_RESERVE)) ||
            TEST_STACKFAULT(pth)) {
            KCALLPROFOFF(0);
            return 1; // restart instruction
        }
        SET_STACKFAULT(pth);
        dwExc = 0x1fe0;
        info = (DWORD)pexi;
    }
    // Setup to capture the exception context in kernel mode but
    // running in thread context to allow preemption and stack growth.
    if (pth->ctx.Fir != (ulong)CaptureContext+4) {
        pexi->exc = (UCHAR)(dwExc >> 5);
        pexi->lowSp = (UCHAR)(pth->ctx.R15 & 63);
        pexi->oldFir = pth->ctx.Fir;
//        pexi->oldMode = GetThreadMode(pth);
        ((PCALLSTACK) pexi)->dwPrcInfo = CST_IN_KERNEL | ((KERNEL_MODE == GetThreadMode(pth))? 0 : CST_MODE_FROM_USER);
        pexi->info = info;
        pexi->linkage = (DWORD)pCurThread->pcstkTop | 1;
        pCurThread->pcstkTop = (PCALLSTACK)pexi;
        pth->ctx.R15 = (DWORD)pexi;
        pth->ctx.Psr |= 0x40000000; // Kernel mode
        pth->ctx.Fir = (ulong)CaptureContext;
        KCALLPROFOFF(0);
        return TRUE;            // continue execution
    }
    DumpFrame(pth, (PCONTEXT)&pth->ctx, dwExc, info);
    RETAILMSG(1, (TEXT("Halting thread %8.8lx\r\n"), pCurThread));
    SurrenderCritSecs();
    DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
    SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
    RunList.pth = 0;
    SetReschedule();
    KCALLPROFOFF(0);
    return 0;
}

typedef struct _EXCARGS {
    DWORD dwExceptionCode;      /* exception code   */
    DWORD dwExceptionFlags;     /* continuable exception flag   */
    DWORD cArguments;           /* number of arguments in array */
    DWORD *lpArguments;         /* address of array of arguments    */
} EXCARGS, *PEXCARGS;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ExceptionDispatch(
    PCONTEXT pctx
    ) 
{
    EXCEPTION_RECORD er;
    int exc;
    ULONG info;
    PEXCARGS pea;
    PTHREAD pth; 
    PEXCINFO pexi;
    DWORD dwThrdInfo = KTHRDINFO (pCurThread);  // need to save it since it might get changed during exception handling

    pth = pCurThread;
    pexi = (PEXCINFO)pth->pcstkTop;
    DEBUGMSG(ZONE_SEH, (TEXT("ExceptionDispatch: pexi=%8.8lx Fir=%8.8lx RA=%8.8lx exc=%x\r\n"),
            pexi, pexi->oldFir, pctx->PR, pexi->exc << 5));

    // Update CONTEXT with infomation saved in the EXCINFO structure
    pctx->Fir = pexi->oldFir;
    // if (pexi->oldMode != KERNEL_MODE)
    if (((PCALLSTACK)pexi)->dwPrcInfo & CST_MODE_FROM_USER) {
        pctx->Psr &= ~0x40000000;
    }
    pctx->R15 = (ULONG)pctx + sizeof(CONTEXT);
    memset(&er, 0, sizeof(er));
    er.ExceptionAddress = (PVOID)pctx->Fir;

    // Check for RaiseException call versus a CPU detected exception.
    // RaiseException just becomes a call to CaptureContext as a KPSL.
    // HandleExcepion sets the LSB of the callstack linkage but ObjectCall
    // does not.
    if (!(pexi->linkage & 1)) {
        


        pea = (PEXCARGS)pctx->R15;
        exc = -1;
        pctx->Fir -= 2;     // to avoid boundary problems at the end of a try block.
        DEBUGMSG(ZONE_SEH, (TEXT("Raising exception %x flags=%x args=%d pexi=%8.8lx\r\n"),
                            pea->dwExceptionCode, pea->dwExceptionFlags, pea->cArguments, pexi));
        er.ExceptionCode = pea->dwExceptionCode;
        er.ExceptionFlags = pea->dwExceptionFlags;
        if (pea->lpArguments && pea->cArguments) {
            if (pea->cArguments > EXCEPTION_MAXIMUM_PARAMETERS) {
                er.ExceptionCode = STATUS_INVALID_PARAMETER;
                er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            } else {
                memcpy(er.ExceptionInformation, pea->lpArguments,
                       pea->cArguments*sizeof(DWORD));
                er.NumberParameters = pea->cArguments;
            }
        }
    } else {
        // CPU detected exception. Extract some additional information about
        // the cause of the exception from the EXCINFO (CALLSTACK) structure.
        pctx->R15 += pexi->lowSp + sizeof(CALLSTACK);
        exc = pexi->exc;
        info = pexi->info;

        // Construct an EXCEPTION_RECORD from the EXCINFO structure
        er.ExceptionInformation[1] = info;
        // TLB Miss on load or store. Attempt to auto-commit the page. If that fails,
        // fall through into general exception processing.
        if (((exc == 0x3) || (exc == 2)) && AutoCommit(info)) {
            pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
            goto continueExecution;
        }
        switch (exc) {
        case 8:     // Address error (store)
            er.ExceptionInformation[0] = 1;
        case 7:     // Address error (load or instruction fetch)
            if (GetContextMode(pctx) != USER_MODE || !(info&0x80000000)) {
                er.ExceptionCode = STATUS_DATATYPE_MISALIGNMENT;
                break;
            }
 #if defined(SH3)
            // (SH3DSP only)
            // Check for the exceptions caused by accessing XY memory in the range
            // between 0xA5000000 to 0xA5FFFFFF.  Please note that "info" is actually 
            // the value of TEA which is the virtual address caused the exception. 
            //
            // Please also note that this does not work exactly as it states in 7729 
            // HW manaul.  According to the manual, accessing XY memory in the user
            // mode when DSP is disabled should cause an address error.  Hitashi 
            // made a request to change this behavior.  The change is similar to the
            // way we handle the excecution caused by the first DSP instruction when
            // DSP is not enabled.  We simply turn on DSP to enable user to access 
            // the XY memory then continue the execution from the faulting
            // instruction without passing the exception to the user app.
            //
            // XY memory is in the range not mapped by TLB so we don't have worry 
            // about any TLB related exceptions.  Please note we only handle it
            // when DSP is disabled.  Treat it as a normal address error if DSP
            // is already onp
            if ( SH3DSP && info <= XYMEMORY_END && info >= XYMEMORY_BEGIN
                  && ( ( pctx->Psr & SR_DSP_ENABLED ) == 0 )){
                
                // If DSP processor and DSP not enabled, enable the DSP.  Thus user
                // can access the XY memory without causing any exceptions
                KCall((PKFN)SwitchDSPOwner,pctx);
                pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);

                // Continue the exception: we don't want to pass this exception to
                // the user app which trying to access the XY memory
                goto continueExecution;
            }
#endif

           goto accessError;

        case 3:     // TLB Miss (store)
        case 4:     // TLB modification
        case 6:     // TLB protection violation (store)
            er.ExceptionInformation[0] = 1;
        case 2:     // TLB miss (load or instruction fetch)
        case 5:     // TLB protection violation (load)
            if (!InSysCall ()) {
                BOOL fSuccess;
                //
                // since we might be calling out of kernel to perform paging and there's
                // no gurantee that the driver underneath won't touch FPU, we need to save
                // the floating point context and restoring it after paging.
                //
#ifdef SH4
                // get the contents of the floating point registers
                FPUFlushContext ();
                // save FPU context in exception context
                memcpy (pctx->FRegs, pCurThread->ctx.FRegs, sizeof(pctx->FRegs));
                memcpy (pctx->xFRegs, pCurThread->ctx.xFRegs, sizeof(pctx->xFRegs));
                pctx->Fpscr = pCurThread->ctx.Fpscr;
                pctx->Fpul = pCurThread->ctx.Fpul;
#endif
                fSuccess = ProcessPageFault ((exc==3)||(exc==4)||(exc==6), info);
                
#ifdef SH4
                // throw away what's in the floating point registers by flushing it.
                FPUFlushContext ();
                // restore FPU context from excepiton context
                memcpy (pCurThread->ctx.FRegs, pctx->FRegs, sizeof(pctx->FRegs));
                memcpy (pCurThread->ctx.xFRegs, pctx->xFRegs, sizeof(pctx->xFRegs));
                pCurThread->ctx.Fpscr = pctx->Fpscr;
                pCurThread->ctx.Fpul = pctx->Fpul;
#endif
                if (fSuccess) {
                    pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
                    goto continueExecution;
                }
            }
    accessError:            
            er.ExceptionCode = STATUS_ACCESS_VIOLATION;
            er.NumberParameters = 2;
            break;
#ifdef SH4
        case 0x40:
        case 0x41:
            KCall((PKFN)SwitchFPUOwner,pctx);
            pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
            goto continueExecution;
        case 9: {           // floating point exception
            DWORD code;
            code = GetCauseFloatCode();
#if 0
            NKDbgPrintfW(L"ExceptionAddress 0x%x\r\n", er.ExceptionAddress);
            NKDbgPrintfW(L"code 0x%x, exc 0x%x\r\n",code,exc);
#endif
            if (code & 0x10)
                er.ExceptionCode = STATUS_FLOAT_INVALID_OPERATION;
            else if (code & 0x8)
                er.ExceptionCode = STATUS_FLOAT_DIVIDE_BY_ZERO;
            else if (code & 0x4)
                er.ExceptionCode = STATUS_FLOAT_OVERFLOW;
            else if (code & 0x2)
                er.ExceptionCode = STATUS_FLOAT_UNDERFLOW;
            else if (code & 0x1)
                er.ExceptionCode = STATUS_FLOAT_INEXACT_RESULT;
            else
            {
                //
                // Reach here 
                // --if code is 0x20 (FPU error)
                // --if code is 0x0 (processor thinks an ieee
                //   exception is possible)
                //
                // both cases require that fp operation be emulated
                // for correct ieee result.
                //
                // save the fpscr before FPUFlushContext will flush it
                // to zero.
                //
                // FPUFlushContext clears fpscr
                //
                FPUFlushContext();

                //
                // Copy current thread fregs and xfreg to user context
                //
                memcpy(&pctx->FRegs[0],&pCurThread->ctx.FRegs[0],sizeof(DWORD)*16);
                memcpy(&pctx->xFRegs[0],&pCurThread->ctx.xFRegs[0],sizeof(DWORD)*16);
                pctx->Fpscr = pCurThread->ctx.Fpscr;
                pctx->Fpul = pCurThread->ctx.Fpul;

                if (HandleHWFloatException(&er,pctx)) 
                {
                    // flush float context back to thread context after exception was handled
                    FPUFlushContext();

                    //
                    // update current thread context with user context
                    //
                    memcpy(&pCurThread->ctx.FRegs[0],&pctx->FRegs[0],sizeof(DWORD)*16);
                    memcpy(&pCurThread->ctx.xFRegs[0],&pctx->xFRegs[0],sizeof(DWORD)*16);

                    pCurThread->ctx.Fpul = pctx->Fpul;
                    pCurThread->ctx.Fpscr = pctx->Fpscr;
                    pCurThread->ctx.Psr = pctx->Psr;

                    pctx->Fir+=2; // +2: return control to instruction successor
                    pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
                    goto continueExecution;
                }
            }
            //
            // Update user context fpscr and fpul
            //
            pCurThread->ctx.Fpscr = pctx->Fpscr;
            pCurThread->ctx.Fpul = pctx->Fpul;
            pCurThread->ctx.Psr = pctx->Psr;
            break;
        }
#endif
        case 11:        // Breakpoint
            er.ExceptionInformation[0] = info;
            er.ExceptionCode = STATUS_BREAKPOINT;
            break;

        case 12:    // Reserved instruction
        case 13:    // Illegal slot instruction
#ifdef SH3
            //
            // Assume DSP instruction.
            // If DSP processor and DSP not enabled, enable the DSP.
            //

            if ( SH3DSP && ((pctx->Psr & SR_DSP_ENABLED) == 0) ){
                KCall((PKFN)SwitchDSPOwner,pctx);
                pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
                goto continueExecution;
            }
#endif
            er.ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
            break;
        case 15:    // Hardware breakpoint
            er.ExceptionInformation[0] = info;
            er.ExceptionCode = STATUS_USER_BREAK;
            break;

        case 0xFF:  // Stack overflow
            er.ExceptionCode = STATUS_STACK_OVERFLOW;
            er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            break;
        }
    }

    if (exc != 11 && exc != 15 && !IsNoFaultMsgSet ()) {
        // if we faulted in DllMain doing PROCESS_ATTACH, the process name will
        // be pointing to other process's (the creator) address space. Make sure
        // we don't fault on displaying process name.
        LPWSTR pProcName = pCurProc->lpszProcName? pCurProc->lpszProcName : L"";
        if (!((DWORD) pProcName & 0x80000000)
            && (((DWORD) pProcName >> VA_SECTION) != (DWORD) (pCurProc->procnum+1)))
            pProcName = L"";
        NKDbgPrintfW(L"Exception %03x Thread=%8.8lx Proc=%8.8lx '%s'\r\n",
                     exc<<5, pth, hCurProc, pProcName);
        NKDbgPrintfW(L"AKY=%8.8lx PC=%8.8lx RA=%8.8lx TEA=%8.8lx\r\n",
                     pCurThread->aky, pctx->Fir, pctx->PR, info);
        if (IsNoFaultSet ()) {
            NKDbgPrintfW(L"TLSKERN_NOFAULT set... bypassing kernel debugger.\r\n");
        }
    }

    DEBUGMSG (ZONE_SEH, (L"ExceptionDispatch: PSR = %8.8lx\r\n", pctx->Psr));
    // Invoke the kernel debugger to attempt to debug the exception before
    // letting the program resolve the condition via SEH.
    pth->pcstkTop = (PCALLSTACK)(pexi->linkage & ~1);
    if (!(pexi->linkage & 1) && IsValidKPtr (pexi)) {
        // from RaiseException, free the callstack structure
        FreeMem (pexi, HEAP_CALLSTACK);
    }
    if (!UserDbgTrap(&er,pctx,FALSE) && (IsNoFaultSet () || !KDTrap(&er, pctx, FALSE))) {
        BOOL bHandled = FALSE;
        // don't pass a break point exception to NKDispatchException
        if (er.ExceptionCode != STATUS_BREAKPOINT) {
            // to prevent recursive exception due to user messed-up TLS
            KTHRDINFO (pth) &= ~UTLS_INKMODE;
            bHandled = NKDispatchException(pth, &er, pctx);
        }
        if (!bHandled && !UserDbgTrap(&er, pctx, TRUE) && !KDTrap(&er, pctx, TRUE)) {
            if (er.ExceptionCode == STATUS_BREAKPOINT) {
                RETAILMSG(1, (TEXT("DEBUG_BREAK @%8.8lx Ignored.\r\n"), pctx->Fir));
                pctx->Fir += 2;     // skip over the trapa instruction
            } else {
                // Terminate the process.
                RETAILMSG(1, (TEXT("\r\nUnhandled exception %8.8lx:\r\n"),
                              er.ExceptionCode));
                if (InSysCall()) {
                    DumpFrame(pth, pctx, exc<<5, info);
                    lpNKHaltSystem ();
                    FakeNKHaltSystem ();
                } else {
                    if (!GET_DEAD(pth)) {
                        PCALLSTACK pcstk = pth->pcstkTop;
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
                            pctx->R15 = (DWORD) pth->tlsPtr - SECURESTK_RESERVE - (PAGE_SIZE >> 1);
                        }
                        SET_DEAD(pth);
                        //pth->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_TRYINGTODIE;
                        pctx->Fir = (ULONG)pExcpExitThread;
                        pctx->R4 = er.ExceptionCode;     // argument: exception code
                        pctx->R5 = (ULONG)er.ExceptionAddress;  // argument: exception address
                        RETAILMSG(1, (TEXT("Terminating thread %8.8lx, PSR = %8.8lx\r\n"), pth, pctx->Psr));
                    } else {
                        DumpFrame(pth, pctx, exc<<5, info);
                        RETAILMSG(1, (TEXT("Can't terminate thread %8.8lx, sleeping forever\r\n"), pth));
                        SurrenderCritSecs();
                        Sleep(INFINITE);
                        DEBUGCHK(0);    // should never get here
                    }
                }
            }
        }
    }
    if (exc == 2 || exc == 3)
        GuardCommit(info);
 continueExecution:

    // restore ThrdInfo
    KTHRDINFO (pth) = dwThrdInfo;
    
    // If returning from handling a stack overflow, reset the thread's stack overflow
    // flag. It would be good to free the tail of the stack at this time
    // so that the thread will stack fault again if the stack gets too big. But we
    // are currently using that stack page.
    if (exc == 0xFF)
        CLEAR_STACKFAULT(pth);
    if (GET_DYING(pth) && !GET_DEAD(pth) && (pCurProc == pth->pOwnerProc)) {
        SET_DEAD(pth);
        CLEAR_USERBLOCK(pth);
        CLEAR_DEBUGWAIT(pth);
        pctx->Fir = (ULONG)pExcpExitThread;
        pctx->R4 = er.ExceptionCode;     // argument: exception code
        pctx->R5 = (ULONG)er.ExceptionAddress;  // argument: exception address
    }   
    return;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DumpFrame(
    PTHREAD pth,
    PCONTEXT pctx,
    DWORD dwExc,
    DWORD info
    ) 
{
    DWORD oldCCR;
    oldCCR = CCR;
    CCR = 0;        // disable the cache to avoid changing its state
    NKDbgPrintfW(L"Exception %03x Thread=%8.8lx AKY=%8.8lx PC=%8.8lx\r\n",
                 dwExc, pth, pCurThread->aky, pctx->Fir);
    NKDbgPrintfW(L" R0=%8.8lx  R1=%8.8lx  R2=%8.8lx  R3=%8.8lx\r\n",
                 pctx->R0, pctx->R1, pctx->R2, pctx->R3);
    NKDbgPrintfW(L" R4=%8.8lx  R5=%8.8lx  R6=%8.8lx  R7=%8.8lx\r\n",
                 pctx->R4, pctx->R5, pctx->R6, pctx->R7);
    NKDbgPrintfW(L" R8=%8.8lx  R9=%8.8lx R10=%8.8lx R11=%8.8lx\r\n",
                 pctx->R8, pctx->R9, pctx->R10, pctx->R11);
    NKDbgPrintfW(L"R12=%8.8lx R13=%8.8lx R14=%8.8lx R15=%8.8lx\r\n",
                 pctx->R12, pctx->R13, pctx->R14, pctx->R15);
    NKDbgPrintfW(L" PR=%8.8lx  SR=%8.8lx\r\n TEA/TRPA=%8.8x",
                 pctx->PR, pctx->Psr, info);
    NKDbgPrintfW(L"PTEL=%8.8lx PTEH=%8.8lx MMUCR=%8.8lx TTB=%8.8lx\r\n",
                 MMUPTEL, MMUPTEH, MMUCR, MMUTTB);
    NKDbgPrintfW(L"CCR=%8.8lx\r\n", oldCCR);
    CCR = oldCCR;   // restore original cache state
}


/* Machine dependent constants */
const DWORD cbMDStkAlign = 4;                   // stack 4 bytes aligned
// give stack and size, where is TLS
#define TLSPTR(pStk,cbStk)  ((LPDWORD)((ulong)pStk + cbStk - (TLS_MINIMUM_AVAILABLE*4)))

//------------------------------------------------------------------------------
// normal thread stack: from top, TLS, PRETLS, then args then free
//------------------------------------------------------------------------------
void 
MDCreateThread(
    PTHREAD pTh,
    LPVOID  lpStack,
    DWORD   cbStack,
    LPVOID  lpBase,
    LPVOID  lpStart,
    BOOL    kmode,
    ulong   param
    ) 
{
    DEBUGCHK ((ulong)lpStack>>VA_SECTION);

    pTh->tlsPtr = TLSPTR (lpStack, cbStack);
    KSTKBASE(pTh) = (DWORD)lpStack;
    // Clear all registers: Esp. fpu state for SH-4
    memset(&pTh->ctx, 0, sizeof(pTh->ctx));
    // Leave room for arguments and TLS and PRETLS on the stack
    pTh->ctx.R15 = (ulong) pTh->tlsPtr - SIZE_PRETLS - 4*4;
    KSTKBOUND(pTh) = pTh->ctx.R15 & ~(PAGE_SIZE-1);

    pTh->ctx.R4 = (ulong)lpStart;
    pTh->ctx.R5 = param;
    pTh->ctx.PR = 0;
#ifdef SH4
    pTh->ctx.Psr = SR_FPU_DISABLED; // disable floating point
    pTh->ctx.Fpscr = 0x40000;  // handle no exceptions
#else
    pTh->ctx.Psr = 0;           // disable DSP
#endif
    pTh->ctx.Fir = (ULONG)lpBase;
    pTh->ctx.ContextFlags = CONTEXT_FULL;
    if (kmode || bAllKMode) {
        SetThreadMode (pTh, KERNEL_MODE);
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        SetThreadMode (pTh, USER_MODE);
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
    }
}



//------------------------------------------------------------------------------
// main thread stack: from top, TLS then buf then buf2 then buf2 (ascii) then args then free
//------------------------------------------------------------------------------
LPCWSTR 
MDCreateMainThread1(
    PTHREAD pTh, 
    LPVOID  lpStack, 
    DWORD   cbStack, 
    LPBYTE  buf, 
    ulong   buflen, 
    LPBYTE  buf2, 
    ulong   buflen2
    ) 
{
    LPBYTE pbcmdline;
    
    DEBUGCHK ((ulong)lpStack>>VA_SECTION);

    pTh->tlsPtr = TLSPTR (lpStack, cbStack);
    KSTKBASE(pTh) = (DWORD)lpStack;

    pbcmdline = (LPBYTE) pTh->tlsPtr - SIZE_PRETLS - ALIGNSTK(buflen);
    memcpy (pbcmdline, buf, buflen);
    pTh->pOwnerProc->lpszProcName = (LPWSTR)(pbcmdline - ALIGNSTK(buflen2));
    memcpy(pTh->pOwnerProc->lpszProcName,buf2,buflen2);
    KPlpvTls = pTh->tlsPtr;
    return (LPCWSTR) pbcmdline;
}

void MDInitSecureStack(LPBYTE lpStack)
{
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
MDCreateMainThread2(
    PTHREAD pTh, 
    DWORD   cbStack, 
    LPVOID  lpBase, 
    LPVOID  lpStart, 
    BOOL    kmode,
    ulong   p1, 
    ulong   p2, 
    ulong   buflen, 
    ulong   buflen2, 
    ulong   p4
    ) 
{
    // Clear all registers: Esp. fpu state for SH-4
    memset(&pTh->ctx, 0, sizeof(pTh->ctx));
    // Leave room for TLS, PRETLS, and arguments on the stack
    pTh->ctx.R15 = ((ulong) pTh->tlsPtr - SIZE_PRETLS - 8*4
                            - ALIGNSTK(buflen) - ALIGNSTK(buflen2)) & ~7;
    KSTKBOUND(pTh) = pTh->ctx.R15 & ~(PAGE_SIZE-1);

    pTh->ctx.R4 = (ulong)lpStart;
    pTh->ctx.R5 = p1;
    pTh->ctx.R6 = p2;
    pTh->ctx.R7 = (ulong) pTh->tlsPtr - SIZE_PRETLS - ALIGNSTK(buflen);
    ((LPDWORD)pTh->ctx.R15)[4] = p4;
    pTh->ctx.PR = 0;
#ifdef SH4
    pTh->ctx.Psr = SR_FPU_DISABLED; // disable floating point
    pTh->ctx.Fpscr = 0x40000;  // handle no exceptions
#else
    pTh->ctx.Psr = 0;           // disable DSP
#endif
    pTh->ctx.Fir = (ULONG)lpBase;
    if (kmode || bAllKMode) {
        SetThreadMode (pTh, KERNEL_MODE);
        KTHRDINFO (pTh) |= UTLS_INKMODE;
    } else {
        SetThreadMode (pTh, USER_MODE);
        KTHRDINFO (pTh) &= ~UTLS_INKMODE;
    }
    pTh->ctx.ContextFlags = CONTEXT_FULL;
}

#ifdef XTIME
extern DWORD ExceptionTime;// tick at exception entry



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SyscallTime(
    int iSyscall
    ) 
{
    DWORD   dwTime;
    dwTime = ExceptionTime - TMUADDR->tcnt1;
    xt.dwTime[iSyscall]+= dwTime;
    if ((++xt.dwCount[iSyscall]) == 1)
        xt.dwMax[iSyscall]=xt.dwMin[iSyscall]= dwTime;
    if (xt.dwMax[iSyscall] < dwTime)
        xt.dwMax[iSyscall]= dwTime;
    if (xt.dwMin[iSyscall] > dwTime)
        xt.dwMin[iSyscall]= dwTime;
}
#endif

// For the SH3 the compiler generated exception functions expect the following input arguments:
//  Exception Filter: r4 = frame, r5 = exception pointers
//  Termination Handler: r4 = frame, r5 = abnormal term flag

#define __C_ExecuteExceptionFilter(eptr, fn, efrm) ((fn)((efrm), (eptr)))
#define __C_ExecuteTerminationHandler(bAb, fn, efrm) ((fn)((efrm), (bAb)))


//------------------------------------------------------------------------------
//  Routine Description:
//      This function scans the scope tables associated with the specified
//      procedure and calls exception and termination handlers as necessary.
//  
//  Arguments:
//      ExceptionRecord - Supplies a pointer to an exception record.
//  
//      EstablisherFrame - Supplies a pointer to frame of the establisher function.
//  
//      ContextRecord - Supplies a pointer to a context record.
//  
//      DispatcherContext - Supplies a pointer to the exception dispatcher or
//          unwind dispatcher context.
//  
//  Return Value:
//      If the exception is handled by one of the exception filter routines, then
//      there is no return from this routine and RtlUnwind is called. Otherwise,
//      an exception disposition value of continue execution or continue search is
//      returned.
//------------------------------------------------------------------------------
EXCEPTION_DISPOSITION __C_specific_handler(
    PEXCEPTION_RECORD ExceptionRecord,
    PVOID EstablisherFrame, 
    PCONTEXT ContextRecord, 
    PDISPATCHER_CONTEXT DispatcherContext
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
                ScopeTable->ScopeRecord[Index].JumpTarget) {
                // Call the exception filter routine.
                ExceptionFilter = (EXCEPTION_FILTER)ScopeTable->ScopeRecord[Index].HandlerAddress;
                Value = __C_ExecuteExceptionFilter(&ExceptionPointers,ExceptionFilter,(ULONG)EstablisherFrame);
                // If the return value is less than zero, then dismiss the
                // exception. Otherwise, if the value is greater than zero,
                // then unwind to the target exception handler. Otherwise,
                // continue the search for an exception filter.
                if (Value < 0)
                    return ExceptionContinueExecution;
                else if (Value > 0) {
                    DispatcherContext->ControlPc = ScopeTable->ScopeRecord[Index].JumpTarget;
                    return ExceptionExecuteHandler;
                }
            }
        }
    } else {
        // Scan the scope table and call the appropriate termination handler
        // routines.
        TargetPc = ContextRecord->Fir;
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
                if (ScopeTable->ScopeRecord[Index].JumpTarget) {
                    if (TargetPc == ScopeTable->ScopeRecord[Index].JumpTarget)
                        break;
                } else {
                    DispatcherContext->ControlPc = ScopeTable->ScopeRecord[Index].EndAddress + 4;
                    TerminationHandler = (TERMINATION_HANDLER)ScopeTable->ScopeRecord[Index].HandlerAddress;
                    __C_ExecuteTerminationHandler(TRUE,TerminationHandler,(ULONG)EstablisherFrame);
                }
            }
        }
    }
    // Continue search for exception or termination handlers.
    return ExceptionContinueSearch;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
DoThreadGetContext(
    HANDLE hTh, 
    LPCONTEXT lpContext
    ) 
{
    PTHREAD pth;
    ULONG   ulContextFlags = lpContext->ContextFlags; // Keep a local copy of the context flag
    if (!(pth = HandleToThread(hTh))) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (ulContextFlags & ~CONTEXT_FULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // Clear the SH3 and SH4 bits in the context flags.  These are use to differentiate
    // SH3 and SH4.  Without doing so, the masking will always be true because of these 
    // bits.  For example, (CONTEXT_CONTROL & CONTEXT_INTEGER) will be either 0x40 or 0xc0 
    // depending on the processor, but it will never be zero.  So the consequence is that
    // we always return the full context no mater what flags users specify.
    // (Please see \public\common\sdk\inc\winnt.h for details.)
    ulContextFlags &= ~CONTEXT_SH4; 

    if (pth->pThrdDbg && pth->pThrdDbg->psavedctx) {
        ACCESSKEY ulOldKey;
        SWITCHKEY(ulOldKey,0xffffffff);
        if (ulContextFlags & CONTEXT_CONTROL) {
            lpContext->PR = pth->pThrdDbg->psavedctx->PR;
            lpContext->R15 = pth->pThrdDbg->psavedctx->R15;
            lpContext->Fir = pth->pThrdDbg->psavedctx->Fir;
            lpContext->Psr = pth->pThrdDbg->psavedctx->Psr;
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            lpContext->MACH = pth->pThrdDbg->psavedctx->MACH;
            lpContext->MACL = pth->pThrdDbg->psavedctx->MACL;
            lpContext->GBR = pth->pThrdDbg->psavedctx->GBR;
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
            lpContext->R13 = pth->pThrdDbg->psavedctx->R13;
            lpContext->R14 = pth->pThrdDbg->psavedctx->R14;
        }
#ifdef SH3
        if ( SH3DSP && (ulContextFlags & CONTEXT_DSP_REGISTERS)) {
            DSPFlushContext();
            lpContext->DSR = pth->pThrdDbg->psavedctx->DSR;
            lpContext->MOD = pth->pThrdDbg->psavedctx->MOD;
            lpContext->RS = pth->pThrdDbg->psavedctx->RS;
            lpContext->RE = pth->pThrdDbg->psavedctx->RE;
            lpContext->A0 = pth->pThrdDbg->psavedctx->A0;
            lpContext->A1 = pth->pThrdDbg->psavedctx->A1;
            lpContext->M0 = pth->pThrdDbg->psavedctx->M0;
            lpContext->M1 = pth->pThrdDbg->psavedctx->M1;
            lpContext->X0 = pth->pThrdDbg->psavedctx->X0;
            lpContext->X1 = pth->pThrdDbg->psavedctx->X1;
            lpContext->Y0 = pth->pThrdDbg->psavedctx->Y0;
            lpContext->Y1 = pth->pThrdDbg->psavedctx->Y1;
            lpContext->A0G = pth->pThrdDbg->psavedctx->A0G;
            lpContext->A1G = pth->pThrdDbg->psavedctx->A1G;
        }
#endif

#ifdef SH4
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            lpContext->Fpscr = pth->pThrdDbg->psavedctx->Fpscr;
            lpContext->Fpul = pth->pThrdDbg->psavedctx->Fpul;
            memcpy(lpContext->FRegs,pth->pThrdDbg->psavedctx->FRegs,sizeof(lpContext->FRegs));
            memcpy(lpContext->xFRegs,pth->pThrdDbg->psavedctx->xFRegs,sizeof(lpContext->xFRegs));
        }
#endif
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

        }
        SETCURKEY(ulOldKey);
    } else {
        if (ulContextFlags & CONTEXT_CONTROL) {
            lpContext->PR = pth->ctx.PR;
            lpContext->R15 = pth->ctx.R15;
            lpContext->Fir = pth->ctx.Fir;
            lpContext->Psr = pth->ctx.Psr;
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            lpContext->MACH = pth->ctx.MACH;
            lpContext->MACL = pth->ctx.MACL;
            lpContext->GBR = pth->ctx.GBR;
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
            lpContext->R13 = pth->ctx.R13;
            lpContext->R14 = pth->ctx.R14;
        }
#ifdef SH3
        if ( SH3DSP && (ulContextFlags & CONTEXT_DSP_REGISTERS)) {
            DSPFlushContext();
            lpContext->DSR = pth->ctx.DSR;
            lpContext->MOD = pth->ctx.MOD;
            lpContext->RS = pth->ctx.RS;
            lpContext->RE = pth->ctx.RE;
            lpContext->A0 = pth->ctx.A0;
            lpContext->A1 = pth->ctx.A1;
            lpContext->M0 = pth->ctx.M0;
            lpContext->M1 = pth->ctx.M1;
            lpContext->X0 = pth->ctx.X0;
            lpContext->X1 = pth->ctx.X1;
            lpContext->Y0 = pth->ctx.Y0;
            lpContext->Y1 = pth->ctx.Y1;
            lpContext->A0G = pth->ctx.A0G;
            lpContext->A1G = pth->ctx.A1G;
        }
#endif
#ifdef SH4
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            lpContext->Fpscr = pth->ctx.Fpscr;
            lpContext->Fpul = pth->ctx.Fpul;
            memcpy(lpContext->FRegs,pth->ctx.FRegs,sizeof(lpContext->FRegs));
            memcpy(lpContext->xFRegs,pth->ctx.xFRegs,sizeof(lpContext->xFRegs));
        }
#endif
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

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
    ULONG   ulContextFlags = lpContext->ContextFlags; // Keep a local copy of the context flag
    if (!(pth = HandleToThread(hTh))) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (ulContextFlags & ~CONTEXT_FULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // Clear the SH3 and SH4 bits in the context flags.  These are use to differentiate
    // SH3 and SH4.  Without doing so, the masking will always be true because of these 
    // bits.  For example, (CONTEXT_CONTROL & CONTEXT_INTEGER) will be either 0x40 or 0xc0 
    // depending on the processor, but it will never be zero.  So the consequence is that
    // we always return the full context no mater what flags users specify.
    // (Please see \public\common\sdk\inc\winnt.h for details.)
    ulContextFlags &= ~CONTEXT_SH4; 

    if (pth->pThrdDbg && pth->pThrdDbg->psavedctx) {
        ACCESSKEY ulOldKey;
        SWITCHKEY(ulOldKey,0xffffffff);
        if (ulContextFlags & CONTEXT_CONTROL) {
            pth->pThrdDbg->psavedctx->PR = lpContext->PR;
            pth->pThrdDbg->psavedctx->R15 = lpContext->R15;
            pth->pThrdDbg->psavedctx->Fir = lpContext->Fir;
            pth->pThrdDbg->psavedctx->Psr = (pth->ctx.Psr & 0xfffffcfc) | (lpContext->Psr & 0x00000303);
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            pth->pThrdDbg->psavedctx->MACH = lpContext->MACH;
            pth->pThrdDbg->psavedctx->MACL = lpContext->MACL;
            pth->pThrdDbg->psavedctx->GBR = lpContext->GBR;
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
            pth->pThrdDbg->psavedctx->R13 = lpContext->R13;
            pth->pThrdDbg->psavedctx->R14 = lpContext->R14;
        }
#ifdef SH3
        if ( SH3DSP && (ulContextFlags & CONTEXT_DSP_REGISTERS)) {
            DSPFlushContext();
            pth->pThrdDbg->psavedctx->DSR = lpContext->DSR;
            pth->pThrdDbg->psavedctx->MOD = lpContext->MOD;
            pth->pThrdDbg->psavedctx->RS = lpContext->RS;
            pth->pThrdDbg->psavedctx->RE = lpContext->RE;
            pth->pThrdDbg->psavedctx->A0 = lpContext->A0;
            pth->pThrdDbg->psavedctx->A1 = lpContext->A1;
            pth->pThrdDbg->psavedctx->M0 = lpContext->M0;
            pth->pThrdDbg->psavedctx->M1 = lpContext->M1;
            pth->pThrdDbg->psavedctx->X0 = lpContext->X0;
            pth->pThrdDbg->psavedctx->X1 = lpContext->X1;
            pth->pThrdDbg->psavedctx->Y0 = lpContext->Y0;
            pth->pThrdDbg->psavedctx->Y1 = lpContext->Y1;
            pth->pThrdDbg->psavedctx->A0G = lpContext->A0G;
            pth->pThrdDbg->psavedctx->A1G = lpContext->A1G;
        }
#endif
#ifdef SH4
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            pth->pThrdDbg->psavedctx->Fpscr = lpContext->Fpscr;
            pth->pThrdDbg->psavedctx->Fpul = lpContext->Fpul;
            memcpy(pth->pThrdDbg->psavedctx->FRegs,lpContext->FRegs,sizeof(lpContext->FRegs));
            memcpy(pth->pThrdDbg->psavedctx->xFRegs,lpContext->xFRegs,sizeof(lpContext->xFRegs));
        }
#endif
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

        }
        SETCURKEY(ulOldKey);
    } else {
        if (ulContextFlags & CONTEXT_CONTROL) {
            pth->ctx.PR = lpContext->PR;
            pth->ctx.R15 = lpContext->R15;
            pth->ctx.Fir = lpContext->Fir;
            pth->ctx.Psr = (pth->ctx.Psr & 0xfffffcfc) | (lpContext->Psr & 0x00000303);
        }
        if (ulContextFlags & CONTEXT_INTEGER) {
            pth->ctx.MACH = lpContext->MACH;
            pth->ctx.MACL = lpContext->MACL;
            pth->ctx.GBR = lpContext->GBR;
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
            pth->ctx.R13 = lpContext->R13;
            pth->ctx.R14 = lpContext->R14;
        }
#ifdef SH3
        if ( SH3DSP && (ulContextFlags & CONTEXT_DSP_REGISTERS)) {
            DSPFlushContext();
            pth->ctx.DSR = lpContext->DSR;
            pth->ctx.MOD = lpContext->MOD;
            pth->ctx.RS = lpContext->RS;
            pth->ctx.RE = lpContext->RE;
            pth->ctx.A0 = lpContext->A0;
            pth->ctx.A1 = lpContext->A1;
            pth->ctx.M0 = lpContext->M0;
            pth->ctx.M1 = lpContext->M1;
            pth->ctx.X0 = lpContext->X0;
            pth->ctx.X1 = lpContext->X1;
            pth->ctx.Y0 = lpContext->Y0;
            pth->ctx.Y1 = lpContext->Y1;
            pth->ctx.A0G = lpContext->A0G;
            pth->ctx.A1G = lpContext->A1G;
        }
#endif
#ifdef SH4
        if (ulContextFlags & CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            pth->ctx.Fpscr = lpContext->Fpscr;
            pth->ctx.Fpul = lpContext->Fpul;
            memcpy(pth->ctx.FRegs,lpContext->FRegs,sizeof(lpContext->FRegs));
            memcpy(pth->ctx.xFRegs,lpContext->xFRegs,sizeof(lpContext->xFRegs));
        }
#endif
        if (ulContextFlags & CONTEXT_DEBUG_REGISTERS) {

        }
    }
    return TRUE;
}

void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    ULONG *pRegs = (ULONG *) pcstk->dwPrevSP;

    pCtx->R8  = pRegs[0];
    pCtx->R9  = pRegs[1];
    pCtx->R10 = pRegs[2];
    pCtx->R11 = pRegs[3];
    pCtx->R12 = pRegs[4];
    pCtx->R13 = pRegs[5];
    pCtx->R14 = pRegs[6];
}

