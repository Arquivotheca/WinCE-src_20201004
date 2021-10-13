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
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "kernel.h"

#define Naked void __declspec(naked)

extern FXSAVE_AREA g_InitialFPUState;

// Default processor type & revision level information.
DWORD ProcessorFeatures = 0;
DWORD ProcessorFeaturesEx = 0;

DWORD dwFPType;

const wchar_t NKCpuType [] = TEXT("x86");

void UpdateRegistrationPtr (NK_PCR *pcr);
extern void FPUFlushContext(void);

// friendly exception name. 1 based (0th entry is for exception id==1)
const LPCSTR g_ppszMDExcpId [MD_MAX_EXCP_ID+1] = {
    IDSTR_DIVIDE_BY_ZERO,           // divide by zero (0)
    IDSTR_INVALID_EXCEPTION,        // single step (1)
    IDSTR_INVALID_EXCEPTION,        // thread switch break point (2)
    IDSTR_INVALID_EXCEPTION,        // break point (3)
    IDSTR_INVALID_EXCEPTION,        // unused (4)
    IDSTR_INVALID_EXCEPTION,        // unused (5)
    IDSTR_ILLEGAL_INSTRUCTION,      // illegal instruction (6)
    IDSTR_INVALID_EXCEPTION,        // unused (7)
    IDSTR_INVALID_EXCEPTION,        // unused (8)
    IDSTR_INVALID_EXCEPTION,        // unused (9)
    IDSTR_INVALID_EXCEPTION,        // unused (10)
    IDSTR_INVALID_EXCEPTION,        // unused (11)
    IDSTR_INVALID_EXCEPTION,        // unused (12)
    IDSTR_PROTECTION_FAULT,         // Protection Fault (13)
    IDSTR_PAGE_FAULT,               // Page Fault (14)
    IDSTR_INVALID_EXCEPTION,        // unused (15)
    IDSTR_FPU_EXCEPTION,            // FPU exception (16)
    
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDDumpFrame (PTHREAD pth, DWORD dwExcpId, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, DWORD dwLevel)
{
    NKDbgPrintfW(L"Exception %a (%d): Thread-Id=%8.8lx(pth=%8.8lx), EIP=%8.8lx\r\n",
            GetExceptionString (dwExcpId), dwExcpId, dwCurThId, pth, pCtx->Eip);
    NKDbgPrintfW(L"Eax=%8.8lx Ebx=%8.8lx Ecx=%8.8lx Edx=%8.8lx\r\n",
            pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx);
    NKDbgPrintfW(L"Esi=%8.8lx Edi=%8.8lx Ebp=%8.8lx Esp=%8.8lx\r\n",
            pCtx->Esi, pCtx->Edi, pCtx->Ebp, pCtx->Esp);
    NKDbgPrintfW(L"CS=%4.4lx DS=%4.4lx ES=%4.4lx SS=%4.4lx FS=%4.4lx GS=%4.4lx\r\n",
            (WORD)pCtx->SegCs, (WORD)pCtx->SegDs, (WORD)pCtx->SegEs, (WORD)pCtx->SegSs, 
            (WORD)pCtx->SegFs, (WORD)pCtx->SegGs);
    NKDbgPrintfW(L"Flags=%8.8lx\r\n",
            pCtx->EFlags);

    return TRUE;
}



//------------------------------------------------------------------------------
// DumpThreadContext, arg1 = id, arg2 = addr
//------------------------------------------------------------------------------
BOOL MDDumpThreadContext (PTHREAD pth, DWORD id, DWORD addr, DWORD unused, DWORD level)
{
    ulong espValue, ssValue;
    if ((pth->ctx.TcxCs&0xFFFF) == KGDT_R0_CODE) {
        espValue = pth->ctx.TcxNotEsp+16;
        ssValue = KGDT_R0_DATA;
    } else {
        espValue = pth->ctx.TcxEsp;
        ssValue = pth->ctx.TcxSs;
    }
    NKDbgPrintfW(L"Exception %a (%d): Thread-Id=%8.8lx(pth=%8.8lx), Proc-Id=%8.8lx(pprc=%8.8lx) '%s', VM-active=%8.8lx(pprc=%8.8lx) '%s'\r\n",
        GetExceptionString (id), id, 
        dwCurThId, pCurThread,
        pActvProc->dwId, pActvProc, SafeGetProcName (pActvProc),
        pVMProc->dwId,   pVMProc,   SafeGetProcName (pVMProc)
        );
    NKDbgPrintfW(L"EIP=%8.8lx Flags=%8.8lx EA=%8.8lx\r\n", 
            pth->ctx.TcxEip, pth->ctx.TcxEFlags, addr);
    NKDbgPrintfW(L"Eax=%8.8lx Ebx=%8.8lx Ecx=%8.8lx Edx=%8.8lx\r\n",
            pth->ctx.TcxEax, pth->ctx.TcxEbx, pth->ctx.TcxEcx, pth->ctx.TcxEdx);
    NKDbgPrintfW(L"Esi=%8.8lx Edi=%8.8lx Ebp=%8.8lx Esp=%8.8lx\r\n",
            pth->ctx.TcxEsi, pth->ctx.TcxEdi, pth->ctx.TcxEbp, espValue);
    NKDbgPrintfW(L"CS=%4.4lx DS=%4.4lx ES=%4.4lx SS=%4.4lx FS=%4.4lx GS=%4.4lx\r\n",
            (WORD)pth->ctx.TcxCs, (WORD)pth->ctx.TcxDs, (WORD)pth->ctx.TcxEs, (WORD)ssValue,
            (WORD)pth->ctx.TcxFs, (WORD)pth->ctx.TcxGs);
    if (level > 1) {
        PDWORD pdw;
        int count = 16;
        addr = espValue;
        if (pth->ctx.TcxEbp >= espValue) {
            count = (pth->ctx.TcxEbp+16 - espValue) / 4;
            //count = ((VM_PAGE_SIZE - (espValue& 0xfff))/4) - 4;
            
        }
        pdw = (PDWORD) GetKAddr ((LPVOID) addr);
        if (pdw)
            DumpDwords((PDWORD)addr, count);
    }
    return TRUE;
}

//
// NOTE: MDRestoreCalleeSavedRegisters never restore SP, it's done in common code
//
void MDRestoreCalleeSavedRegisters (PCALLSTACK pcstk, PCONTEXT pCtx)
{
    pCtx->Ebp = pcstk->regs[REG_EBP];
    pCtx->Ebx = pcstk->regs[REG_EBX];
    pCtx->Esi = pcstk->regs[REG_ESI];
    pCtx->Edi = pcstk->regs[REG_EDI];
}

void MDClearVolatileRegs (PCONTEXT pCtx)
{
    pCtx->Eax = pCtx->Ecx = pCtx->Edx = 0;
}


void MDSkipBreakPoint (PEXCEPTION_RECORD pExr, PCONTEXT pCtx)
{
    if (*(uchar*)pCtx->Eip == 0xCC)
        ++pCtx->Eip;
    else if (*(uchar*)pCtx->Eip == 0xCD)
        pCtx->Eip += 2;
}

BOOL MDIsPageFault (DWORD dwExcpId)
{
    return (14 == dwExcpId);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SafeIdentifyCpu(void)
{
    __asm {

        pushad                          // save all registers
        pushfd
        cli

        pushfd                          // Save EFLAGS to stack
        pop     eax                     // Store EFLAGS in EAX
        mov     ecx,eax                 // Save in ECX for testing later
        xor     eax,00200000h           // Switch bit 21
        push    eax                     // Copy changed value to stack
        popfd                           // Save changed EAX to EFLAGS
        pushfd                          // Push EFLAGS to top of stack
        pop     eax                     // Store EFLAGS in EAX
        cmp     eax,ecx                 // See if bit 21 has changed
        jz      cpuid_trap              // If no change,no CPUID

        // CPUID is supported
        mov     eax, 1                  // get the family and stepping
        _emit   0fh                     // cpuid instruction
        _emit   0a2h
        mov     ebx, eax
        mov     ecx, eax
        shr     ebx, 4
        and     ebx, 0Fh                // (ebx) = model
        and     ecx, 0Fh                // (ecx) = stepping
        shr     eax, 8
        and     eax, 0Fh                // (eax) = family
        cmp     eax, 4
        jne     cpu_not_p4
        mov     eax, PROCESSOR_INTEL_486
        jmp     short cpuid_store
cpu_not_p4:
        cmp     eax, 5
        jne     short cpu_not_p5
        mov     eax, PROCESSOR_INTEL_PENTIUM
        jmp     short cpuid_store
cpu_not_p5:
        mov     eax, PROCESSOR_INTEL_PENTIUMII
cpuid_store:
        mov     esi, dword ptr [g_pNKGlobal]
        mov     (PNKGLOBAL) [esi].dwProcessorType, eax
        mov     (PNKGLOBAL) [esi].wProcessorLevel, bx
        mov     (PNKGLOBAL) [esi].wProcessorRevision, cx
        mov     ProcessorFeatures, edx

        // Let's get extended CPUID data (EAX = 0x80000001)
        mov     eax, 0x80000001
        _emit   0fh                     // cpuid instruction
        _emit   0a2h
        mov     ProcessorFeaturesEx, edx

cpuid_trap:
        popfd
        popad                           // restore all registers
    }
}


//------------------------------------------------------------------------------
//
// HandleExcepiotn - hardware exception handling
// NOTE: pth can be a faked thread structure allocated on KStack. Make sure
//       we use pth and pCurThread correct in this function.
//       USE pth: when accessing the ctx fields
//       USE pCurThread: for everything else
//
//------------------------------------------------------------------------------
BOOL 
HandleException(
    PTHREAD pth,
    int id,
    ulong addr
    ) 
{
    BOOL fRet;
    KCALLPROFON(0);
    fRet = KC_CommonHandleException (pth, id, addr, 0);
    KCALLPROFOFF(0);
    return fRet;
}

typedef struct _EXCARGS {
    DWORD dwExceptionCode;      /* exception code   */
    DWORD dwExceptionFlags;     /* continuable exception flag   */
    DWORD cArguments;           /* number of arguments in array */
    DWORD *lpArguments;         /* address of array of arguments    */
} EXCARGS;
typedef EXCARGS *PEXCARGS;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SetCPUHardwareWatch(
    LPVOID lpv,
    DWORD dwFlags
    ) 
{
    static PPROCESS pOwnerProc;
    
    if (!InPrivilegeCall()) 
        return KCall((PKFN)SetCPUHardwareWatch, lpv, dwFlags);
        
    KCALLPROFON(73);

    if (lpv == 0) {
        // dwFlags=0 means check to see if we want to take this breakpoint
        if (dwFlags == (DWORD)-1) {
            // check to see which breakpoint went off and
            // clear the debug register
            DWORD dwReason;
            DWORD dwMask=(DWORD)~0xf;
            _asm {
                mov eax, dr6
                mov dwReason, eax
                and eax, dwMask
                mov dr6, eax
            }
            // was the zeroptr breakpoint hit?
            if (dwReason&0x2 && pOwnerProc != pActvProc) {
                NKOutputDebugString(L"zero mapped breakpoint hit in wrong process!\r\n");
                KCALLPROFOFF(73);
                return FALSE;
            }
        } else {
            // dwFlags=1 means disable the data watchpoint
            dwFlags = (DWORD)~0xf;
            _asm {
                mov eax, dr7
                and eax, dwFlags
                mov dr7, eax
            }
        }
    } else {
        DWORD dwMask=(DWORD)~0xff000f;
        LPVOID lpvZero=lpv;
        if (dwFlags == HARDWARE_WATCH_READ) {
            dwFlags = 0xff000a;
        } else {
            dwFlags = 0xdd000a;
        }
        _asm {
            mov eax, lpv
            mov dr0, eax
            mov eax, lpvZero
            mov dr1, eax
            mov eax, dr7
            and eax, dwMask
            or  eax, dwFlags
            mov dr7, eax
        }
        pOwnerProc = pActvProc;
    }

    KCALLPROFOFF(73);
    return TRUE;
}
    
#ifdef DEBUG
int MDDbgPrintExceptionInfo (PEXCEPTION_RECORD pExr, PCALLSTACK pcstk)
{
    DEBUGMSG(ZONE_SEH || !IsInKVM ((DWORD) pcstk), (TEXT("ExceptionDispatch: pcstk=%8.8lx Eip=%8.8lx id=%x\r\n"),
            pcstk, pcstk->retAddr, pcstk->regs[REG_EDI]));
    return TRUE;
}

#endif
    


LPDWORD MDGetRaisedExceptionInfo (PEXCEPTION_RECORD pExr, PCONTEXT pCtx, PCALLSTACK pcstk)
{
    // RaiseException. The stack looks like this. The only difference between
    // a trusted/untrusted caller is that the stack is switched
    //
    //      |                       |
    //      |                       |
    //      -------------------------
    //      |                       |<---- pCtx
    //      |                       |
    //      |                       |
    //      -------------------------
    //      |                       |<---- pExr
    //      |                       |
    //      |                       |
    //      |                       |
    //      -------------------------
    //      |            RA         |<----- (pExr+1)
    //      -------------------------
    //      | arg of RaiseExcepiton |<----- (pExr+1) + sizeof(DWORD)
    //      |                       |
    //      -------------------------
    
    PEXCARGS pea = (PEXCARGS) ((DWORD)(pExr+1) + sizeof(DWORD));

    pExr->ExceptionCode = pea->dwExceptionCode;
    pExr->ExceptionFlags = pea->dwExceptionFlags;

    if (pea->lpArguments && pea->cArguments) {
                
        if (pea->cArguments > EXCEPTION_MAXIMUM_PARAMETERS) {
            pExr->ExceptionCode = (DWORD) STATUS_INVALID_PARAMETER;
            pExr->ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            
        } else {
            // copy arguments
            CeSafeCopyMemory (pExr->ExceptionInformation, pea->lpArguments, pea->cArguments*sizeof(DWORD));
            pExr->NumberParameters = pea->cArguments;
        }
    }

    return pea->lpArguments;
}


//
// argument id, addr, unuse are exactly the same as the value HandleException passed to KC_CommonHandleException
//
void MDSetupExcpInfo (PTHREAD pth, PCALLSTACK pcstk, DWORD id, DWORD addr, DWORD unused)
{
    pcstk->regs[REG_EXCPLIST] = (DWORD) GetRegistrationHead ();  // exception list
    pcstk->regs[REG_EDI]      = id;                              // Edi: id
    pcstk->regs[REG_ESI]      = addr;                            // Esi: addr
    pcstk->regs[REG_EBP]      = (pth->ctx.TcxEFlags & 0x0100);   // Ebp: old TFlag
    pth->ctx.TcxEFlags &= ~0x0100;                      // Unset TF while in the debugger
    if (id != 16)
        pcstk->regs[REG_EBX]  = pth->ctx.TcxError;               // Ebx: error code
    else {
        WORD status;
        _asm fnstsw status;
        pcstk->regs[REG_EBX]  = status;                          // Ebx: status
        _asm fnclex;
    }
    if (pCurThread->tlsPtr != pCurThread->tlsSecure) {
        NK_PCR *pcr = TLS2PCR (pCurThread->tlsSecure);
        UpdateRegistrationPtr (pcr);
        pcr->ExceptionList = (DWORD) REGISTRATION_RECORD_PSL_BOUNDARY;    // mark registration end
    }
    SetThreadMode (pth, KERNEL_MODE);       // Run in kernel mode
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MDCaptureFPUContext (PCONTEXT pCtx)
{
    FPUFlushContext ();

    if (ProcessorFeatures & CPUID_FXSR) {
        // Context is stored in FXSAVE format.  Convert to FNSAVE
        // format.
        PFXSAVE_AREA        FxArea = (PFXSAVE_AREA) PTH_TO_FLTSAVEAREAPTR(pCurThread);
        PFLOATING_SAVE_AREA FnArea = &pCtx->FloatSave;
        DWORD i, j;

        FnArea->ControlWord = FxArea->ControlWord;
        FnArea->StatusWord  = FxArea->StatusWord;
        FnArea->TagWord     = 0;
        for (i = 0; i < FN_BITS_PER_TAGWORD; i += 2) {
            if (((FxArea->TagWord >> (i/2)) & FX_TAG_VALID) != FX_TAG_VALID) {
                FnArea->TagWord |= (FN_TAG_EMPTY << i);
            }
        }
        FnArea->ErrorOffset     = FxArea->ErrorOffset;
        FnArea->ErrorSelector   = FxArea->ErrorSelector & 0xFFFF;
        FnArea->ErrorSelector  |= ((DWORD)FxArea->ErrorOpcode) << 16;
        FnArea->DataOffset      = FxArea->DataOffset;
        FnArea->DataSelector    = FxArea->DataSelector;
        memset(FnArea->RegisterArea, 0, SIZE_OF_80387_REGISTERS);
        for (i = 0; i < NUMBER_OF_FP_REGISTERS; ++i) {
            for (j = 0; j < BYTES_PER_FP_REGISTER; ++j) {
                FnArea->RegisterArea[i*BYTES_PER_FP_REGISTER + j] =
                    FxArea->RegisterArea[i*BYTES_PER_FX_REGISTER + j];
            }
        }

    } else {
        pCtx->FloatSave = *(PTH_TO_FLTSAVEAREAPTR(pCurThread));
    }

    pCtx->ContextFlags |= CONTEXT_FLOATING_POINT;   
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL MDHandleHardwareException (PCALLSTACK pcstk, PEXCEPTION_RECORD pExr, PCONTEXT pCtx, LPDWORD pdwExcpId)
{
    int        id = pcstk->regs[REG_EDI];       // Edi kept id
    ULONG    addr = pcstk->regs[REG_ESI];       // Esi kept addr
    DWORD   error = pcstk->regs[REG_EBX];       // Ebx is the error status
    BOOL fInKMode = !(pcstk->dwPrcInfo & CST_MODE_FROM_USER);
    BOOL fHandled = FALSE;

    pCtx->EFlags |= pcstk->regs[REG_EBP];       // restore t flag (Ebp contain the old flag)

    pExr->ExceptionInformation[0] = (error >> 1) & 1;   // indicate write

    *pdwExcpId = id;
    
    // Construct an EXCEPTION_RECORD from the EXCINFO structure
    switch (id) {
    case ID_PROTECTION_FAULT:        // General Protection Fault
        pExr->ExceptionInformation[0] = 1;

        // fall through to handle page fault
        __fallthrough;
       
    case ID_PAGE_FAULT:        // Page fault

        pExr->NumberParameters = 2;
        pExr->ExceptionInformation[1] = addr;
        pExr->ExceptionCode = (DWORD) STATUS_ACCESS_VIOLATION;

        // pExr->ExceptionInformation[0] == read or write (1 for write)
        if (!InSysCall ()                           // faulted inside kcall - not much we can do 
            && (fInKMode || !(addr & 0x80000000))   // faulted access kernel address while not in kmode
            && !IsInSharedHeap (addr)) {            // faulted while access shared heap - always fail
            fHandled = VMProcessPageFault (pVMProc, addr, pExr->ExceptionInformation[0], CONTEXT_TO_PROGRAM_COUNTER(pCtx));
        }
        break;

    case ID_BREAK_POINT:     // Breakpoint
        pExr->ExceptionInformation[0] = 1; // DEBUGBREAK_STOP_BREAKPOINT
        pExr->ExceptionCode = (DWORD) STATUS_BREAKPOINT;
        break;

    case ID_THREAD_BREAK_POINT: // Stop thread breakpoint
        pExr->ExceptionInformation[0] = 3; // DEBUG_THREAD_SWITCH_BREAKPOINT
        pExr->ExceptionCode = (DWORD) STATUS_BREAKPOINT;
        break;


    case ID_SINGLE_STEP:     // single step
        pExr->ExceptionInformation[0] = 0;
        pExr->ExceptionCode = (DWORD) STATUS_SINGLE_STEP;
        // If you are using the SetCPUHardware watch function you will probably 
        // want to uncomment the following lines so that it will clear the register
        // automatically on exception
/*          
        if(!SetCPUHardwareWatch(0, (DWORD)-1)) {
            goto continueExecution;
        }
*/          
        break;

    case ID_DIVIDE_BY_ZERO:     // Divide by zero
        pExr->ExceptionInformation[0] = 0;
        pExr->ExceptionCode = (DWORD) STATUS_INTEGER_DIVIDE_BY_ZERO;
        break;

    case ID_ILLEGAL_INSTRUCTION: // Reserved instruction
        pExr->ExceptionCode = (DWORD) STATUS_ILLEGAL_INSTRUCTION;
        break;

    case ID_FPU_EXCEPTION: {
        if (error & 0x01)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_INVALID_OPERATION;
        else if (error & 0x4)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_DIVIDE_BY_ZERO;
        else if (error & 0x8)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_OVERFLOW;
        else if (error & 0x10)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_UNDERFLOW;
        else if (error & 0x20)
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_INEXACT_RESULT;
        else
            pExr->ExceptionCode = (DWORD) STATUS_FLOAT_DENORMAL_OPERAND;
        break;
    }
    default:
        // unknown excepiton!!!
        pExr->ExceptionCode = (DWORD) STATUS_ILLEGAL_INSTRUCTION;
        DEBUGCHK (0);
    }

    return fHandled;
}

#define SANATIZE_SEG_REGS(eax, ax)       \
    _asm { cld } \
    _asm { mov eax, KGDT_R3_DATA }  \
    _asm { mov ds, ax } \
    _asm { mov es, ax } \
    _asm { mov eax, KGDT_PCR }  \
    _asm { mov fs, ax } \
    _asm { mov eax, KGDT_KPCB } \
    _asm { mov gs, ax }


//------------------------------------------------------------------------------
// Capture processor state into a full CONTEXT structure for debugging
// and exception handling.
//
//  (eax) = ptr to EXCINFO structure
//------------------------------------------------------------------------------
Naked 
CaptureContext(void) 
{
    __asm {
        lea     esp, [esp-size EXCEPTION_RECORD]    // room for exception record
        push    ss
        push    esp     // will be repaired by ExceptionDispatch
        pushfd
        push    cs
        push    eax     // Eip filled in by ExceptionDispatch
        push    ebp
        push    eax
        push    ecx
        push    edx
        push    ebx
        push    esi
        push    edi
        push    ds
        push    es
        push    fs
        push    gs
        SANATIZE_SEG_REGS(ecx, cx)
        sub     esp, size FLOATING_SAVE_AREA
        push    0       // dr7
        push    0       // dr6
        push    0       // dr3
        push    0       // dr2
        push    0       // dr1
        push    0       // dr0
        push    CONTEXT_SEH
        mov     ebx, esp        // (ebx) = ptr to context structure
        sub     esp, 4          // room for pcstk argument if need to run SEH handler
        push    ebx             // (arg1) = ptr to CONTEXT
        lea     edi, [ebx+size CONTEXT]  // (edi) = pExr
        push    edi
        call    ExceptionDispatch

        test    eax, eax        // run SEH handler?
        jnz     RunSEH

        // Reload processor state from possibly edited 
        lea     esp, [ebx].SegGs
        pop     gs
        pop     fs
        pop     es
        pop     ds
        pop     edi
        pop     esi
        pop     ebx
        pop     edx
        pop     ecx
        mov     ax, cs              // (ax) = current code selector
        xor     eax, [esp+12]       // (ax) = CS ^ target selector
        and     eax, 0000fffcH      // (eax) = selector bits that differ
        jz      short no_ring_switch
        pop     eax
        pop     ebp
        iretd

// Restore frame without a ring switch.  Special care must be taken here because
// we need to restore Esp but an IRETD without a ring switch will not restore
// the SS:ESP.  Since we are returning the same ring, all that needs to be restored
// are Esp and Eip. This is done by copying the new Eip and new Ebp values onto the
// new stack, switching to the new stack and then restoring Ebp and returning.
// The return must be via an IRETD so that single stepping works correctly.
//
// At this point the stack frame is:
//      24  ss
//      20  esp
//      16  flags
//      12  cs
//      08  eip
//      04  ebp
//      00  eax
//      ---------< esp points here

no_ring_switch:
        mov     ebp, [esp+20]       // (ebp) = new stack pointer
        mov     eax, [esp+16]       // (eax) = new EFlags
        mov     [ebp-4], eax        // may overwrite SS value
        mov     eax, [esp+8]        // (eax) = new eip
        mov     [ebp-12], eax       // may overwrite EFlags value
        mov     eax, [esp+4]        // (eax) = new ebp
        mov     [ebp-16], eax       // may overwrite CS value
        mov     [ebp-8], cs
        pop     eax                 // restore Eax
        lea     esp, [ebp-16]       // (esp) = new stack (with Ebp & IRET frame pushed on)
        pop     ebp                 // (ebp) = original Ebp
        iretd                       // continue at the desired address

//
// (ebx) = pCtx (on secure stack)
// (edi) = pExr (on secure stack)
//
RunSEH:
        mov     ecx, gs:[0] PCB.pCurThd // (ecx) = pCurThread
        mov     ecx, [ecx].pcstkTop // (ecx) = pCurThread->pcstkTop
        mov     [esp], edi          // arg0 - pExr
        mov     [esp+4], ebx        // arg1 - pCtx
        mov     [esp+8], ecx        // arg2 - pcstk
        push    0                   // return address, invalid

        mov     edx, g_pfnKrnRtlDispExcp      // (edx) = kernel mode RtlDispatchException
        
        mov     ax, cs              // dx = current cs
        xor     eax, [ebx].SegCs    // CS ^ target selector
        and     eax, 0000fffcH      // (eax) = selector bit that differ
        jz      Rtn2KMode

        // mode switch - must switch stack
        // pctx->Esp is the target Esp.

        mov     ecx, KGDT_UPCB
        mov     gs, cx
        
        mov     edx, g_pfnUsrRtlDispExcp    // (edx) = user mode RtlDispatchException
        push    [ebx].SegSs         // ss
        push    (PCONTEXT)[ebx].Esp // target esp
        
Rtn2KMode:
        // (edx) = RtlDispatchException in coredll or kcoredll
        push    [ebx].EFlags        // EFLAGS
        push    [ebx].SegCs         // cs
        push    edx                 // IP = RtlDispatchException
        iretd
    }
}


void InitFPSaveArea (NK_PCR *pcr)
{
    if (dwFPType == FPTYPE_HARDWARE) {
        if (ProcessorFeatures & CPUID_FXSR)  {
            memcpy(&pcr->tcxExtended, &g_InitialFPUState, 
                sizeof(pcr->tcxExtended));
        } else {
            memcpy(&pcr->tcxFPU, &g_InitialFPUState, (sizeof(pcr->tcxFPU)+16));
        }
    } else if (dwFPType == FPTYPE_SOFTWARE)
        memset(pcr->Emx87Data, 0, sizeof(pcr->Emx87Data));
}

//------------------------------------------------------------------------------
// normal thread stack: from top, TLS then args then free
//------------------------------------------------------------------------------
void 
MDSetupThread(
    PTHREAD pTh,
    FARPROC  lpBase,
    FARPROC  lpStart,
    BOOL    kmode,
    ulong   param
    ) 
{
    ulong *args;
    NK_PCR *pcr = TLS2PCR (pTh->tlsPtr);
    
    // Allocate space for arguments to the start function.
    pTh->ctx.TcxEsp = (ulong) pTh->tlsPtr - SECURESTK_RESERVE;

    args = (ulong*)pTh->ctx.TcxEsp;
    pcr->ExceptionList = 0;
//    pcr->InitialStack = pTh->ctx.TcxEsp;
//    pcr->StackLimit = (DWORD)lpStack;
    args[1] = (ulong)lpStart;
    args[2] = param;
    args[0] = 0;                        // set return address to fault
    pTh->ctx.TcxEip = (ULONG)lpBase;
    pTh->ctx.TcxDs = KGDT_R3_DATA;
    pTh->ctx.TcxEs = KGDT_R3_DATA;
    pTh->ctx.TcxFs = KGDT_PCR;
//    pTh->ctx.TcxGs = KGDT_KPCB;
    pTh->ctx.TcxGs = kmode? KGDT_KPCB : KGDT_UPCB;
    pTh->ctx.TcxEFlags = 0x3200;        // IOPL=3, IF=1
    if (kmode) {
        SetThreadMode (pTh, KERNEL_MODE);
    } else {
        SetThreadMode (pTh, USER_MODE);
    }

}


void MDInitStack (LPBYTE lpStack, DWORD cbSize)
{
    InitFPSaveArea (NCRPTR (lpStack, cbSize));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ZeroPage(
    void *pvPage
    ) 
{
    _asm {
        mov edi, pvPage
        mov ecx, VM_PAGE_SIZE/4
        xor eax, eax
        rep stosd
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SCHL_DoThreadGetContext(
    PTHREAD pth,
    LPCONTEXT lpContext
    ) 
{
    PFXSAVE_AREA FxArea;
    PFLOATING_SAVE_AREA FnArea;

    BOOL fRet = TRUE;
    AcquireSchedulerLock (0);

    if (!KC_IsValidThread (pth)
        || (lpContext->ContextFlags & ~(CONTEXT_FULL|CONTEXT_FLOATING_POINT|CONTEXT_DEBUG_REGISTERS))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
        
    } else if (pth->pSavedCtx) {
        if ((lpContext->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
            lpContext->Ebp = pth->pSavedCtx->Ebp;
            lpContext->Eip = pth->pSavedCtx->Eip;
            lpContext->SegCs = pth->pSavedCtx->SegCs;
            lpContext->EFlags = pth->pSavedCtx->EFlags;
            lpContext->Esp = pth->pSavedCtx->Esp;
            lpContext->SegSs = pth->pSavedCtx->SegSs;
        }
        if ((lpContext->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {
            lpContext->Edi = pth->pSavedCtx->Edi;
            lpContext->Esi = pth->pSavedCtx->Esi;
            lpContext->Ebx = pth->pSavedCtx->Ebx;
            lpContext->Edx = pth->pSavedCtx->Edx;
            lpContext->Ecx = pth->pSavedCtx->Ecx;
            lpContext->Eax = pth->pSavedCtx->Eax;
        }
        if ((lpContext->ContextFlags & CONTEXT_SEGMENTS) == CONTEXT_SEGMENTS) {
            lpContext->SegGs = pth->pSavedCtx->SegGs;
            lpContext->SegFs = pth->pSavedCtx->SegFs;
            lpContext->SegEs = pth->pSavedCtx->SegEs;
            lpContext->SegDs = pth->pSavedCtx->SegDs;
        }
        if ((lpContext->ContextFlags & CONTEXT_FLOATING_POINT) == 
            CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            lpContext->FloatSave = pth->pSavedCtx->FloatSave;
        }
        if ((lpContext->ContextFlags & CONTEXT_DEBUG_REGISTERS) == 
            CONTEXT_DEBUG_REGISTERS) {

        }
    } else {
        if ((lpContext->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
            lpContext->Ebp = pth->ctx.TcxEbp;
            lpContext->Eip = pth->ctx.TcxEip;
            lpContext->SegCs = pth->ctx.TcxCs;
            lpContext->EFlags = pth->ctx.TcxEFlags;
            lpContext->Esp = pth->ctx.TcxEsp;
            lpContext->SegSs = pth->ctx.TcxSs;
        }
        if ((lpContext->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {
            lpContext->Edi = pth->ctx.TcxEdi;
            lpContext->Esi = pth->ctx.TcxEsi;
            lpContext->Ebx = pth->ctx.TcxEbx;
            lpContext->Edx = pth->ctx.TcxEdx;
            lpContext->Ecx = pth->ctx.TcxEcx;
            lpContext->Eax = pth->ctx.TcxEax;
        }
        if ((lpContext->ContextFlags & CONTEXT_SEGMENTS) == CONTEXT_SEGMENTS) {
            lpContext->SegGs = pth->ctx.TcxGs;
            lpContext->SegFs = pth->ctx.TcxFs;
            lpContext->SegEs = pth->ctx.TcxEs;
            lpContext->SegDs = pth->ctx.TcxDs;
        }
        if ((lpContext->ContextFlags & CONTEXT_FLOATING_POINT) == 
            CONTEXT_FLOATING_POINT) {
            if (ProcessorFeatures & CPUID_FXSR)  {
                FxArea = (PFXSAVE_AREA) PTH_TO_FLTSAVEAREAPTR(pth);
                FnArea = &lpContext->FloatSave;
                __asm {
                    // We won't get here if emulating FP, so CR0.EM will be 0
                    call    FPUFlushContext     // FPUFlushContext sets CR0.TS
                    clts 
                    mov     eax, FxArea
                    FXRESTOR_EAX                // convert from fxsave format
                    mov     eax, FnArea         // in NK_PCR to fnsave format
                    fnsave  [eax]               // in CONTEXT structure
                    fwait
                    mov     eax, cr0 
                    or      eax, TS_MASK
                    mov     cr0, eax
                }
            } else {
                FPUFlushContext();
                lpContext->FloatSave = *(PTH_TO_FLTSAVEAREAPTR(pth));
            }
        }
        if ((lpContext->ContextFlags & CONTEXT_DEBUG_REGISTERS) == 
            CONTEXT_DEBUG_REGISTERS) {

        }
    }

    ReleaseSchedulerLock (0);
    return fRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SCHL_DoThreadSetContext(
    PTHREAD pth,
    const CONTEXT *lpContext
    ) 
{
    PFXSAVE_AREA FxArea;
    DWORD i, j, TagWord;
    BOOL fRet = TRUE;
    AcquireSchedulerLock (0);


    if (!KC_IsValidThread (pth)
        || (lpContext->ContextFlags & ~(CONTEXT_FULL|CONTEXT_FLOATING_POINT|CONTEXT_DEBUG_REGISTERS))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        fRet = FALSE;
        
    } else if (pth->pSavedCtx) {
        if ((lpContext->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
            pth->pSavedCtx->Ebp = lpContext->Ebp;
            pth->pSavedCtx->Eip = lpContext->Eip;
            pth->pSavedCtx->SegCs = lpContext->SegCs;
            pth->pSavedCtx->EFlags = (pth->pSavedCtx->EFlags & 0xfffff200) |
                                 (lpContext->EFlags& 0x00000dff);
            pth->pSavedCtx->Esp = lpContext->Esp;
            pth->pSavedCtx->SegSs = lpContext->SegSs;
        }
        if ((lpContext->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {
            pth->pSavedCtx->Edi = lpContext->Edi;
            pth->pSavedCtx->Esi = lpContext->Esi;
            pth->pSavedCtx->Ebx = lpContext->Ebx;
            pth->pSavedCtx->Edx = lpContext->Edx;
            pth->pSavedCtx->Ecx = lpContext->Ecx;
            pth->pSavedCtx->Eax = lpContext->Eax;
        }
        if ((lpContext->ContextFlags & CONTEXT_SEGMENTS) == CONTEXT_SEGMENTS) {
            pth->pSavedCtx->SegGs = lpContext->SegGs;
            pth->pSavedCtx->SegFs = lpContext->SegFs;
            pth->pSavedCtx->SegEs = lpContext->SegEs;
            pth->pSavedCtx->SegDs = lpContext->SegDs;
        }
        if ((lpContext->ContextFlags & CONTEXT_FLOATING_POINT) == 
            CONTEXT_FLOATING_POINT) {
            FPUFlushContext();
            pth->pSavedCtx->FloatSave = lpContext->FloatSave;
        }
        if ((lpContext->ContextFlags & CONTEXT_DEBUG_REGISTERS) == 
            CONTEXT_DEBUG_REGISTERS) {

        }
    } else {
        if ((lpContext->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
            pth->ctx.TcxEbp = lpContext->Ebp;
            pth->ctx.TcxEip = lpContext->Eip;
            pth->ctx.TcxCs = lpContext->SegCs;
            pth->ctx.TcxEFlags = (pth->ctx.TcxEFlags & 0xfffff200) |
                                 (lpContext->EFlags& 0x00000dff);
            pth->ctx.TcxEsp = lpContext->Esp;
            pth->ctx.TcxSs = lpContext->SegSs;
        }
        if ((lpContext->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER) {
            pth->ctx.TcxEdi = lpContext->Edi;
            pth->ctx.TcxEsi = lpContext->Esi;
            pth->ctx.TcxEbx = lpContext->Ebx;
            pth->ctx.TcxEdx = lpContext->Edx;
            pth->ctx.TcxEcx = lpContext->Ecx;
            pth->ctx.TcxEax = lpContext->Eax;
        }
        if ((lpContext->ContextFlags & CONTEXT_SEGMENTS) == CONTEXT_SEGMENTS) {
            pth->ctx.TcxGs = lpContext->SegGs;
            pth->ctx.TcxFs = lpContext->SegFs;
            pth->ctx.TcxEs = lpContext->SegEs;
            pth->ctx.TcxDs = lpContext->SegDs;
        }
        if ((lpContext->ContextFlags & CONTEXT_FLOATING_POINT) == 
            CONTEXT_FLOATING_POINT)  {
            FPUFlushContext();
            if  (ProcessorFeatures & CPUID_FXSR)  {
                // Convert from fnsave format in CONTEXT to fxsave format in PCR
                FxArea = (PFXSAVE_AREA) PTH_TO_FLTSAVEAREAPTR(pth);
                FxArea->ControlWord = (USHORT) lpContext->FloatSave.ControlWord;
                FxArea->StatusWord = (USHORT) lpContext->FloatSave.StatusWord;
                FxArea->TagWord = 0;
                TagWord = lpContext->FloatSave.TagWord;
                for (i = 0; i < FN_BITS_PER_TAGWORD; i+=2) {
                    if (((TagWord >> i) & FN_TAG_MASK) != FN_TAG_EMPTY) 
                        FxArea->TagWord |= (FX_TAG_VALID << (i/2));
                }
                FxArea->ErrorOffset = lpContext->FloatSave.ErrorOffset;
                FxArea->ErrorSelector = lpContext->FloatSave.ErrorSelector & 0xffff;
                FxArea->ErrorOpcode = (USHORT)
                    ((lpContext->FloatSave.ErrorSelector >> 16) & 0xFFFF);
                FxArea->DataOffset = lpContext->FloatSave.DataOffset;
                FxArea->DataSelector = lpContext->FloatSave.DataSelector;
                memset(&FxArea->RegisterArea[0], 0, SIZE_OF_FX_REGISTERS);
                for (i = 0; i < NUMBER_OF_FP_REGISTERS; i++) {
                    for (j = 0; j < BYTES_PER_FP_REGISTER; j++) {
                        FxArea->RegisterArea[i*BYTES_PER_FX_REGISTER+j] =
                        lpContext->FloatSave.RegisterArea[i*BYTES_PER_FP_REGISTER+j];
                    }
                }
            } else {
                *(PTH_TO_FLTSAVEAREAPTR(pth)) = lpContext->FloatSave;
            }
        }
        if ((lpContext->ContextFlags & CONTEXT_DEBUG_REGISTERS) == 
            CONTEXT_DEBUG_REGISTERS) {

        }
    }

    ReleaseSchedulerLock (0);
    return fRet;
}


