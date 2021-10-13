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
/*++

Module Name:

    ExceptCommon.cpp

Abstract:

    Save the exception context for use by FlexiPTMI and PackData.

--*/

#include "osaxs_p.h"

#ifdef SHx
// for SR_DSP_ENABLED and SR_FPU_DISABLED
#include "shx.h"
#endif

#ifdef ARM
BOOL HasWMMX();
void GetWMMXRegisters(CONCAN_REGS *);
void SetWMMXRegisters(CONCAN_REGS *);
#endif

/*++

Routine Name:

    GetThreadProgramCounter

Argument:

    pth - pointer to thread structure / thread representation

--*/

DWORD GetThreadProgramCounter (const THREAD *pth)
{
    if (pth != PcbGetCurThread())
    {
        return GetThreadIP (pth);
    }
    return CONTEXT_TO_PROGRAM_COUNTER (DD_ExceptionState.context);
}


#ifdef x86
DWORD CONTEXT_TO_RETURN_ADDRESS (const CONTEXT *pCtx)
{
    LPDWORD pFrame = (LPDWORD) (pCtx->Ebp + 4);
    DWORD   dwRa = 0;
    if (   IsDwordAligned (pFrame)
        && ((DWORD)pFrame > pCtx->Esp)
        && ((KERNEL_MODE == GetContextMode (pCtx)) || IsValidUsrPtr (pFrame, sizeof(DWORD), TRUE))) {
        DWORD saveKrn = pCurThread->tlsPtr[TLSSLOT_KERNEL];
        pCurThread->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;
        __try {
            dwRa = *pFrame;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        pCurThread->tlsPtr[TLSSLOT_KERNEL] = saveKrn;
    }
    return dwRa;
}
#endif


/*++

Routine Name:

    GetThreadRetAddr

Argument:

    pth - pointer to thread structure / thread representation

--*/

DWORD GetThreadRetAddr (const THREAD *pth)
{
    if (pth != PcbGetCurThread())
    {
        return (DWORD) ((pth)->retValue);
    }
    return CONTEXT_TO_RETURN_ADDRESS (DD_ExceptionState.context);
}


DWORD MapContextFlags(DWORD ContextFlags)
{
    DWORD CeContextFlags = 0;
    
    if ((ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        CeContextFlags |= CE_CONTEXT_CONTROL;
    }
    if ((ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        CeContextFlags |= CE_CONTEXT_INTEGER;
    }
    if ((ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT)
    {
        CeContextFlags |= CE_CONTEXT_FLOATING_POINT;
    }

    return CeContextFlags;
}


//
// Grab any extra context that may need filling in. (FPU, DSP).
// WMMX is an exception, because it isn't part of the ARM CONTEXT structure.
//
void OsAxsCaptureExtraContext()
{
    DEBUGGERMSG(OXZONE_EXSTATE, (L"+OsAxsCaptureExtraContext\r\n"));

    // * Capture FPU context.
    if (IsFPUPresent())
    {
        BOOL fCaptureFPU = FALSE;

#ifdef x86
        fCaptureFPU = TRUE;
#else        
        // Make sure the FPU is not currently in the emulator (for ARM).
        // Make sure the FPU has been turned on for the current thread (for RISC).
 
        PTHREAD pCurTh = pCurThread;
        if (pCurTh->pcstkTop != NULL &&
            pCurTh->pcstkTop->dwPrcInfo & CST_EXCEPTION)
        {
            BOOL fUserMode = (pCurTh->pcstkTop->dwPrcInfo & CST_MODE_FROM_USER) != 0;
            DWORD *pTLS = fUserMode? pCurTh->tlsNonSecure : pCurTh->tlsPtr;
            
            fCaptureFPU =  (pTLS[TLSSLOT_KERNEL] & TLSKERN_FPU_USED) && 
                          !(pTLS[TLSSLOT_KERNEL] & TLSKERN_IN_FPEMUL);

            DEBUGGERMSG(OXZONE_EXSTATE, (L" OsAxsCaptureExtraContext, fCaptureFPU = %d, TLSKERN = %08X\r\n", fCaptureFPU, pTLS[TLSSLOT_KERNEL]));
        }
        else
        {
            DEBUGGERMSG(OXZONE_ALERT, (L" OsAxsCaptureExtraContext, pcstkTop unexpected.\r\n"));
        }
#endif

        if (fCaptureFPU)
        {
            MDCaptureFPUContext(DD_ExceptionState.context);
        }
    }

    DEBUGGERMSG(OXZONE_EXSTATE, (L"-OsAxsCaptureExtraContext\r\n"));
}


HRESULT OsAxsGetExceptionRecord(PCONTEXT pctx, size_t cbctx)
{
    HRESULT hr;
    CONTEXT *pCurContext = DD_ExceptionState.context;
    if (cbctx < sizeof(CONTEXT))
    {
        hr = E_FAIL;
        goto exit;
    }
    
    DWORD ContextFlags = MapContextFlags(pCurContext->ContextFlags);
    memcpy(pctx, pCurContext, sizeof(CONTEXT));
    pctx->ContextFlags = ContextFlags;

#ifdef ARM
    if ((sizeof(CONTEXT) + sizeof(CONCAN_REGS)) <= cbctx)
    {
        CONCAN_REGS *pConcanRegs = (CONCAN_REGS *) &pctx[1];
        if (HasWMMX())
        {
            GetWMMXRegisters(pConcanRegs);
            pctx->ContextFlags |= CE_CONTEXT_DSP;
        }
    }
 #endif
    
    hr = S_OK;

exit:
    return hr;
}


HRESULT OsAxsSetExceptionRecord(PCONTEXT pctx, size_t cbctx)
{
    CONTEXT *pCurContext = DD_ExceptionState.context;

    DWORD ContextFlags = pCurContext->ContextFlags;
    memcpy(pCurContext, pctx, sizeof(CONTEXT));
    pCurContext->ContextFlags = ContextFlags;

    // Sync floating point context back to pCurThread if necessary.
    if ((pCurContext->ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT)
    {
#if defined(SH4)
        memcpy(&pCurThread->ctx.Fpscr, &pCurContext->Fpscr, sizeof(DWORD) * 34);
#elif defined(MIPS_HAS_FPU)
        pCurThread->ctx.Fsr = pCurContext->Fsr;
        memcpy(&pCurThread->ctx.FltF0, &pCurContext->FltF0, sizeof(FREG_TYPE) * 32);
#elif defined(ARM)
        pCurThread->ctx.Fpscr = pCurContext->Fpscr;
        pCurThread->ctx.FpExc = pCurContext->FpExc;
        memcpy(pCurThread->ctx.S, pCurContext->S, NUM_VFP_REGS * REGSIZE);
#elif defined(x86)
        if (pCurThread->pSavedCtx != NULL)
        {
            pCurThread->pSavedCtx->FloatSave = pCurContext->FloatSave;
        }
        else if (ProcessorFeatures & CPUID_FXSR)
        {
            // Convert from fnsave format in CONTEXT to fxsave format in PCR
            PFXSAVE_AREA        FxArea  = (PFXSAVE_AREA) PTH_TO_FLTSAVEAREAPTR(pCurThread);
            PFLOATING_SAVE_AREA FnArea  = &pctx->FloatSave;
            FxArea->ControlWord         = (USHORT) FnArea->ControlWord;
            FxArea->StatusWord          = (USHORT) FnArea->StatusWord;
            FxArea->TagWord             = 0;
            for (DWORD i = 0; i < FN_BITS_PER_TAGWORD; i+=2) {
                if (((FnArea->TagWord >> i) & FN_TAG_MASK) != FN_TAG_EMPTY) 
                    FxArea->TagWord |= (FX_TAG_VALID << (i/2));
            }
            FxArea->ErrorOffset         = FnArea->ErrorOffset;
            FxArea->ErrorSelector       = FnArea->ErrorSelector & 0xffff;
            FxArea->ErrorOpcode         = (USHORT) ((FnArea->ErrorSelector >> 16) & 0xFFFF);
            FxArea->DataOffset          = FnArea->DataOffset;
            FxArea->DataSelector        = FnArea->DataSelector;
            memset(&FxArea->RegisterArea[0], 0, SIZE_OF_FX_REGISTERS);
            for (DWORD i = 0; i < NUMBER_OF_FP_REGISTERS; i++)
            {
                for (DWORD j = 0; j < BYTES_PER_FP_REGISTER; j++)
                {
                    FxArea->RegisterArea[i*BYTES_PER_FX_REGISTER+j] =
                        FnArea->RegisterArea[i*BYTES_PER_FP_REGISTER+j];
                }
            }
        }
        else
        {
            *(PTH_TO_FLTSAVEAREAPTR(pCurThread)) = pCurContext->FloatSave;
        }
#endif
    }        
    
    
#ifdef ARM
    // Handle the Intel/Marvel WMMX context if it's present.
    if ((sizeof(CONTEXT) + sizeof(CONCAN_REGS)) <= cbctx)
    {
        CONCAN_REGS *pConcanRegs = (CONCAN_REGS *) &pctx[1];
        if (HasWMMX())
        {
            SetWMMXRegisters(pConcanRegs);
        }
    }
#endif
    return S_OK;
}

void OsAxsPushExceptionState(EXCEPTION_RECORD *exr, CONTEXT *ctx, BOOL secondChance)
{
    KDASSERT(g_pDebuggerData->exceptionIdx >= EXCEPTION_STACK_UNUSED &&
            g_pDebuggerData->exceptionIdx < (EXCEPTION_STACK_SIZE - 1));

    DEBUGGERMSG(OXZONE_EXSTATE, (L"++OsAxsPushExceptionState [%d]\r\n", g_pDebuggerData->exceptionIdx));

    ++g_pDebuggerData->exceptionIdx;
    DD_ExceptionState.exception = exr;
    DD_ExceptionState.context = ctx;
    DD_ExceptionState.secondChance = secondChance;
    DEBUGGERMSG(OXZONE_EXSTATE, (L"--OsAxsPushExceptionState [%d]\r\n", g_pDebuggerData->exceptionIdx));
}

void OsAxsPopExceptionState(void)
{
    KDASSERT(g_pDebuggerData->exceptionIdx >= EXCEPTION_STACK_UNUSED &&
            g_pDebuggerData->exceptionIdx < EXCEPTION_STACK_SIZE);

    DEBUGGERMSG(OXZONE_EXSTATE, (L"++OsAxsPopExceptionState [%d]\r\n", g_pDebuggerData->exceptionIdx));
    --g_pDebuggerData->exceptionIdx;
    DEBUGGERMSG(OXZONE_EXSTATE, (L"--OsAxsPopExceptionState [%d]\r\n", g_pDebuggerData->exceptionIdx));

}


