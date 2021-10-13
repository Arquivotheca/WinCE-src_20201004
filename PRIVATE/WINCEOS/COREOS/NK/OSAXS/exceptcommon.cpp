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

    ExceptCommon.cpp

Abstract:

    Save the exception context for use by FlexiPTMI and PackData.

--*/

#include "osaxs_p.h"

#ifdef SHx
// for SR_DSP_ENABLED and SR_FPU_DISABLED
#include "shx.h"
#endif

SAVED_THREAD_STATE *g_psvdThread = NULL;
EXCEPTION_INFO    *g_pexceptionInfo = NULL;


/*++

Routine Name:

    GetThreadProgramCounter

Argument:

    pth - pointer to thread structure / thread representation

--*/

DWORD GetThreadProgramCounter (const THREAD *pth)
{
    if (! OsAxsIsCurThread (pth))
    {
        return GetThreadIP (pth);
    }
    return CONTEXT_TO_PROGRAM_COUNTER (g_pexceptionInfo->ExceptionPointers.ContextRecord);
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
    if (! OsAxsIsCurThread (pth))
    {
        return (DWORD) ((pth)->retValue);
    }
    return CONTEXT_TO_RETURN_ADDRESS (g_pexceptionInfo->ExceptionPointers.ContextRecord);
}


#if defined(x86)
VOID FPUFlushContextx86(IN CONTEXT* pCtx)
{
    PFXSAVE_AREA FxArea;
    PFLOATING_SAVE_AREA FnArea;
    DWORD i, j;

    if (*g_OsaxsData.ppCurFPUOwner)
    {
        KCall ((PKFN) g_OsaxsData.FPUFlushContext, 0, 0, 0);
        if  ((*g_OsaxsData.pdwProcessorFeatures) & CPUID_FXSR)
        {
            FxArea = (PFXSAVE_AREA) PTH_TO_FLTSAVEAREAPTR(pCurThread);
            FnArea = &pCtx->FloatSave;

            FnArea->ControlWord = FxArea->ControlWord;
            FnArea->StatusWord = FxArea->StatusWord;
            FnArea->TagWord = 0;
            for (i = 0; i < FN_BITS_PER_TAGWORD; i+=2) {
                if (((FxArea->TagWord >> (i/2)) & FX_TAG_VALID) != FX_TAG_VALID)
                    FnArea->TagWord |= (FN_TAG_EMPTY << i);
            }
            FnArea->ErrorOffset = FxArea->ErrorOffset;
            FnArea->ErrorSelector = FxArea->ErrorSelector & 0xFFFF;
            FnArea->ErrorSelector |= (((DWORD)FxArea->ErrorOpcode) << 16);
            FnArea->DataOffset = FxArea->DataOffset;
            FnArea->DataSelector = FxArea->DataSelector;
            memset(&FnArea->RegisterArea[0], 0, SIZE_OF_80387_REGISTERS);
            for (i = 0; i < NUMBER_OF_FP_REGISTERS; i++) {
                for (j = 0; j < BYTES_PER_FP_REGISTER; j++) {
                    FnArea->RegisterArea[i*BYTES_PER_FP_REGISTER+j] =
                    FxArea->RegisterArea[i*BYTES_PER_FX_REGISTER+j];
                }
            }
        }
        else
        {
            pCtx->FloatSave = *(PTH_TO_FLTSAVEAREAPTR(pCurThread));
        }
    }
}
#endif

BOOL KdpFlushExtendedContext(CONTEXT* pCtx)
/*++

Routine Description:

    For CPUs that support it, this function retrieves the extended portion of the context
    that is only backed up when used by a thread.

Arguments:

    pCtx - Supplies the exception context to be updated.

Return Value:

    TRUE if the current thread was the owner of the extended context

--*/
{
    BOOL fOwner = FALSE;

    // FPU/DSPFlushContext code.  Calling these functions alters the state of the g_CurFPUOwner
    // thread in that it disables the FPU/DSP (ExceptionDispatch will reenable in on the next
    // access).  For SHx, the disable alters the context, so we update the exception context
    // to reflect the change.  See also: KdpTrap, KdpSetContext.
    //
    //
#if defined(SHx) && !defined(SH4) && !defined(SH3e)
    // copy over the DSP registers from the thread context
    fOwner = (pCurThread == g_CurDSPOwner);
    g_OsaxsData.DSPFlushContext ();
    // if DSPFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) pCtx->Psr &= ~SR_DSP_ENABLED;
    memcpy (&(pCtx->DSR), &(pCurThread->ctx.DSR), sizeof (DWORD) * 13);
#endif

    // Get the floating point registers from the thread context
#if defined(SH4)
    fOwner = (pCurThread == g_CurFPUOwner);
    g_OsaxsData.FPUFlushContext ();
    // if FPUFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) pCtx->Psr |= SR_FPU_DISABLED;
    memcpy (&(pCtx->Fpscr), &(pCurThread->ctx.Fpscr), sizeof (DWORD) * 34);
#elif defined(MIPS_HAS_FPU)
    g_OsaxsData.FPUFlushContext ();
    pCtx->Fsr = pCurThread->ctx.Fsr;
    memcpy (&(pCtx->FltF0), &(pCurThread->ctx.FltF0), sizeof (FREG_TYPE) * 32);
#elif defined(ARM)
    // FPUFlushContext might modify FpExc, but apparently it can't be restored, so we shouldn't bother
    // trying update our context with it
    g_OsaxsData.FPUFlushContext ();
    memcpy (&(pCtx->Fpscr), &(pCurThread->ctx.Fpscr), sizeof (DWORD) * 43);
#elif defined(x86)
    FPUFlushContextx86(pCtx);
#endif

    return fOwner;
}


/*++

Routine Name:

    SaveExceptionState

Routine Description:

    Save the pointer to the exception info and saved thread state.

--*/
HRESULT SaveExceptionState (EXCEPTION_INFO     *  pexceptionInfo,
                            SAVED_THREAD_STATE *  psvdThread,
                            EXCEPTION_INFO     ** ppexceptionInfoSave,
                            SAVED_THREAD_STATE ** ppsvdThreadSave)
{ 
    if (ppexceptionInfoSave)
    {
        *ppexceptionInfoSave = g_pexceptionInfo;
    }

    if (ppsvdThreadSave)
    {
        *ppsvdThreadSave = g_psvdThread;
    }

    g_pexceptionInfo = pexceptionInfo;
    g_psvdThread = psvdThread;

    return S_OK;
}


