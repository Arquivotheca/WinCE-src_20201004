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
/***
*frame.cpp - The frame handler and everything associated with it.
*
*
*Purpose:
*       The frame handler and everything associated with it.
*
*       Entry points:
*       _CxxFrameHandler   - the frame handler.
*
****/

#include <excpt.h>

#include <corecrt.h>
#include <mtdll.h>      // CRT internal header file
#include <ehdata.h>     // Declarations of all types used for EH
#include <ehstate.h>    // Declarations of state management stuff
#include <exception>    // User-visible routines for eh
#include <ehhooks.h>    // Declarations of hook variables and callbacks
#include <ehglobs.h>    // EH global variables
#include <coredll.h>    // Define debug zone information

// WARNING: trnsctrl.h removes the __stdcall->__cdecl mapping, so don't include
//          any headers after it
#include <trnsctrl.h>   // Routines to handle transfer of control

#pragma hdrstop         // PCH is created from here

extern "C" void __FrameUnwindToState(
    EHRegistrationNode *,
    DispatcherContext *,
    FuncInfo *,
    __ehstate_t
);

static BOOLEAN FindHandler(
    EHExceptionRecord *,
    EHRegistrationNode *,
    CONTEXT *,
    DispatcherContext *,
    FuncInfo *,
    int
);

static int TypeMatch(
    HandlerType   *pCatch,         // Type of the 'catch' clause
    CatchableType *pCatchable,     // Type conversion under consideration
    ThrowInfo     *pThrow          // General information about the thrown type.
    );

static void BuildCatchObject(
    EHExceptionRecord  *pExcept,     // Original exception thrown
    EHRegistrationNode *pRN,         // Registration node of catching function
    HandlerType        *pCatch,      // The catch clause that got it
    CatchableType      *pConv        // The conversion to use
    );

#ifdef _M_ARM
#define DC_NVREGS(pDC)          ((pDC)->NonVolatileRegisters)

extern "C" DWORD __CatchIt (PCONTEXT pCtx, PCCATCHPARAMS pCatchParams);

#define __GetRangeOfTrysToCheck(a, b, c, d, e, f, g) \
                                _GetRangeOfTrysToCheck(a, b, c, d, e, f, g)

#else

extern "C" void * CallCatchBlock(
    EHExceptionRecord *,
    EHRegistrationNode *,
    CONTEXT *,
    FuncInfo *,
    void *,
    int,
    unsigned long
    );

#define DC_NVREGS(pDC)          NULL
#define __SetState(a,b,c,d)
#define _SetImageBase(ib)

#define __GetRangeOfTrysToCheck(a, b, c, d, e, f, g) \
                                _GetRangeOfTrysToCheck(b, c, d, e, f)

#define __FrameUnwindToEmptyState(pRN, pDC, pFuncInfo)  __FrameUnwindToState (pRN, pDC, pFuncInfo, EH_EMPTY_STATE)


#endif


////////////////////////////////////////////////////////////////////////////////
//
// __InternalCxxFrameHandler - the frame handler for all functions with C++ EH
// information.
//
// If exception is handled, this doesn't return; otherwise, it returns
// ExceptionContinueSearch.
//
// Note that this is called three ways:
//     From __CxxFrameHandler: primary usage, called to inspect whole function.
//         CatchDepth == 0, pMarkerRN == NULL
//     From CatchGuardHandler: If an exception occurred within a catch, this is
//         called to check for try blocks within that catch only, and does not
//         handle unwinds.
//     From TranslatorGuardHandler: Called to handle the translation of a
//         non-C++ EH exception.  Context considered is that of parent.

extern "C" EXCEPTION_DISPOSITION __cdecl __InternalCxxFrameHandler(
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    CONTEXT *pContext,                  // Context info
    DispatcherContext *pDC,             // Context within subject frame
    FuncInfo *pFuncInfo,                // Static information for this frame
    int CatchDepth                      // How deeply nested are we?
    )
{
    DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler: ENTER\r\n"));

    DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler: pExcept=%08x pFuncInfo=%08x  pRN=%08x  CatchDepth=%d\r\n",
                    pExcept, pFuncInfo, pRN, CatchDepth));
    DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler:                  ExceptionCode=%08x  ExceptionFlags=%08x\r\n",
                    pExcept->ExceptionCode, pExcept->ExceptionFlags));
    DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler:                  pDC->ControlPc=%08x\r\n",
                    pDC->ControlPc));
    DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler:                  pExceptionObject=%08x pThrowInfo= %08x\r\n",
                    PER_PEXCEPTOBJ(pExcept), PER_PTHROW(pExcept)));

    if ((PER_CODE(pExcept) != EH_EXCEPTION_NUMBER) &&
        (FUNC_MAGICNUM(*pFuncInfo) >= EH_MAGIC_NUMBER3) &&
#ifdef _M_ARM
        !cxxReThrow &&
#endif
        ((FUNC_FLAGS(*pFuncInfo) & FI_EHS_FLAG) != 0))
    {
        // This function was compiled /EHs so we don't need to do anything in
        // this handler.
         
        DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler - function compiled with /EHS, ExceptionContinueSearch\r\n"));
        return ExceptionContinueSearch;
    }

    if (IS_UNWINDING(PER_FLAGS(pExcept)))
    {
        // We're at the unwinding stage of things.  Don't care about the
        // exception itself.

        DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler: unwinding ...\r\n"));

        if ( IS_TARGET_UNWIND(PER_FLAGS(pExcept) )){

            EHRegistrationNode *pEstablisherFrame = pRN;

            DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler: IS_TARGET_UNWIND\r\n"));
#ifdef _M_ARM
            EHRegistrationNode  EstablisherFramePointers;
            pEstablisherFrame = _GetEstablisherFrame(pRN, pDC, pFuncInfo, &EstablisherFramePointers);
#endif
            __ehstate_t targetState = TBME_LOW(*TargetEntry);

            DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler:               targetState=%d from TargetEntry=%08x\r\n",
                            targetState, TargetEntry));

            __FrameUnwindToState(pEstablisherFrame, pDC, pFuncInfo, targetState);

            DEBUGMSG(DBGEH,(L"__InternalCxxFrameHandler: after unwind, calling Catch Block\r\n"));

#ifdef _M_ARM

            CATCHPARAMS         catchParam;
            
            catchParam.pNonVolatileRegs     = pDC->NonVolatileRegisters;
            catchParam.pContext             = pContext;
            catchParam.HandlerAddress       = pDC->TargetPc;
            catchParam.pExcept              = pExcept;
            catchParam.pEstablisherFrame    = pEstablisherFrame;
            catchParam.pFuncInfo            = pFuncInfo;

            pDC->TargetPc = __CatchIt (pContext, &catchParam);

            DEBUGMSG(DBGEH,(L"from __CatchIt: continuationAddress %08x\r\n",
                            pDC->TargetPc));
#else
            SetState(pRN, TBME_HIGH(*TargetEntry)+1);

            CONTEXT_TO_PROGRAM_COUNTER(pContext) = (DWORD) CallCatchBlock(pExcept,
                                                                  pRN,
                                                                  pContext,
                                                                  pFuncInfo,
                                                                  (void *)CONTEXT_TO_PROGRAM_COUNTER(pContext),
                                                                  CatchDepth,
                                                                  0x100);
            
            DEBUGMSG(DBGEH,(L"from CallCatchBlock: continuationAddress %08x\r\n",
                            CONTEXT_TO_PROGRAM_COUNTER(pContext)));

#endif  // _M_ARM
        }
        else if (FUNC_MAXSTATE(*pFuncInfo) != 0 && CatchDepth == 0) {

            // Only unwind if there's something to unwind
            DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler: Unwind to empty state\n"));

            __FrameUnwindToEmptyState(pRN, pDC, pFuncInfo);
        }

    } else if (FUNC_NTRYBLOCKS(*pFuncInfo) != 0) {

        //
        //  If try blocks are present, search for a handler:
        //

        DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler: NTRYBLOCKS=%d\r\n",
                        FUNC_NTRYBLOCKS(*pFuncInfo)));

        DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler: call FindHandler\r\n"));

        if ( FindHandler(pExcept, pRN, pContext, pDC, pFuncInfo, CatchDepth) ) {
            DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler: EXIT with pExcept(%08x): Handler Found %08x\n",
                            pExcept, pDC->ControlPc));
            return ExceptionExecuteHandler;
        }

        // If it returned FALSE, we didn't have any matches.
        DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler: No Handler Found\r\n"));
    }

    // We had nothing to do with it or it was rethrown.  Keep searching.
    DEBUGMSG(DBGEH,(L"InternalCxxFrameHandler EXIT with pExcept(%08x): ExceptionContinueSearch\r\n", pExcept));

    return ExceptionContinueSearch;

} // InternalCxxFrameHandler



////////////////////////////////////////////////////////////////////////////////
//
// FindHandler - find a matching handler on this frame, using all means
// available.
//
// Description:
//     If the exception thrown was an MSC++ EH, search handlers for match.
//     Otherwise, if we haven't already recursed, try to translate.
//     If we have recursed (ie we're handling the translator's exception), and
//         it isn't a typed exception, call _inconsistency.
//
// Returns:
//      Returns TRUE if a handler was found
//
// Assumptions:
//      Only called if there are handlers in this function.

static BOOLEAN FindHandler(
    EHExceptionRecord  *pExcept,     // Information for this (logical) exception
    EHRegistrationNode *pRN,         // Dynamic information for subject frame
    CONTEXT            *pContext,    // Context info
    DispatcherContext  *pDC,         // Context within subject frame
    FuncInfo           *pFuncInfo,   // Static information for subject frame
    int                 CatchDepth   // Level of nested catch that is being checked
    )
{
    DEBUGCHK (FUNC_NTRYBLOCKS(*pFuncInfo) > 0);

    // Get the current state (machine-dependent)
#if defined(_M_ARM) /*IFSTRIP=IGN*/
    __ehstate_t curState = __StateFromControlPc(pFuncInfo, pDC);
    EHRegistrationNode EstablisherFrame;
    /*
     * Here We find what is the actual State of current function. The way we
     * do this is first get State from ControlPc.
     *
     * Remember we have __GetUnwindTryBlock to remember the last State for which
     * Exception was handled and __GetCurrentState for retriving the current
     * state of the function. Please Note that __GetCurrentState is used
     * primarily for unwinding purpose.
     *
     * Also remember that all the catch blocks act as funclets. This means that
     * ControlPc for all the catch blocks are different from ControlPc of parent
     * catch block or function.
     *
     * take a look at this example
     * try {
     *   // STATE1 = 1
     *   try {
     *     // STATE2
     *     // THROW
     *   } catch (...) { // CatchB1
     *     // STATE3
     *     // RETHROW OR NEW THROW
     *   }
     * } catch (...) { // CatchB2
     * }
     *
     * If we have an exception comming from STATE3, the FindHandler will be
     * called for CatchB1, at this point we do the test which State is our
     * real state, curState from ControlPc or state from __GetUnwindTryBlock.
     * Since curState from ControlPc is greater, we know that real State is
     * curState from ControlPc and thus we update the UnwindTryBlockState.
     *
     * On further examination, we found out that there is no handler within
     * this catch block, we return without handling the exception. For more
     * info on how we determine if we have handler, have a look at
     * __GetRangeOfTrysToCheck.
     *
     * Now FindHandler will again be called for parent function. Here again
     * we test which is real State, state from ControlPc or State from
     * __GetUnwindTryBlock. This time state from __GetUnwindTryBlock is correct.
     *
     * Also look at code in __CxxCallCatchBlock, you will se that as soon as we get
     * out of last catch block, we reset __GetUnwindTryBlock state to -1.
     */

    _GetEstablisherFrame(pRN, pDC, pFuncInfo, &EstablisherFrame);
    if (curState > __GetUnwindTryBlock(pRN, pDC, pFuncInfo)) {
        __SetState(&EstablisherFrame, pDC, pFuncInfo, curState);
        __SetUnwindTryBlock(pRN, pDC, pFuncInfo, /*curTry*/ curState);
    } else {
        curState = __GetUnwindTryBlock(pRN, pDC, pFuncInfo);
    }
#else
    EHExceptionRecord *pNewExcept = pExcept ;
    EHExceptionRecord *pRethrowException = NULL ;

    __ehstate_t curState = GetCurrentState(pRN, pDC, pFuncInfo);
#endif
    DEBUGMSG(DBGEH,(L"FindHandler: curState=GetCurrentState=%d\r\n", curState));

    DEBUGCHK(curState >= EH_EMPTY_STATE && curState < FUNC_MAXSTATE(*pFuncInfo));

    // Check if it's a re-throw ( ie. no new object to throw ).
    // Use the exception we stashed away if it is.
    if (PER_IS_MSVC_EH(pExcept) && PER_PTHROW(pExcept) == NULL) {

        DEBUGMSG(DBGEH,(L"FindHandler: pExcept(0x%08x) is a rethrow of _pCurrentException(0x%08x)\r\n",
                             pExcept, _pCurrentException));

        if (_pCurrentException == NULL) {
            // Oops!  User re-threw a non-existant exception!  Let it propogate.
            DEBUGMSG(DBGEH,(L"FindHandler: EXIT return FALSE\r\n"));
            return FALSE;
        }

        DEBUGMSG(DBGEH && !PER_PTHROW(_pCurrentException),(L"FindHandler: ThrowInfo is NULL\n")) ;

        pExcept = _pCurrentException;
        pContext = _pCurrentExContext;

        DEBUGMSG(DBGEH,(L"FindHandler: reset pExcept=%08x pContext=%08x from _pCurrentException\r\n",
                        pExcept, pContext));

#ifdef _M_ARM
        _SetThrowImageBase ((DWORD) PER_PTHROWIB (pExcept));

#else
        pRethrowException = pExcept;
        DEBUGMSG(DBGEH,(L"FindHandler: rethrow: setting pRethrowException=%08x\r\n",
                        pRethrowException));
#endif

        DEBUGCHK(!PER_IS_MSVC_EH(pExcept) || PER_PTHROW(pExcept) != NULL);
    }


    // Determine range of try blocks to consider:
    // Only try blocks which are at the current catch depth are of interest.

    unsigned int curTryBlockIndex;    // the index of current try block.
    unsigned int endTryBlockIndex;    // the index of try block to end search, itself excluded

    TryBlockMapEntry *pEntry = __GetRangeOfTrysToCheck(pRN,
                                                      pFuncInfo,
                                                      CatchDepth,
                                                      curState,
                                                      &curTryBlockIndex,
                                                      &endTryBlockIndex,
                                                      pDC);

    DEBUGMSG(DBGEH,(L"FindHandler: TBME=%08x  curTryBlockIndex=%d  endTryBlockIndex=%d  curState=%d\r\n",
                    pEntry, curTryBlockIndex, endTryBlockIndex, curState));

    // Scan the try blocks in the function:
    for (; curTryBlockIndex < endTryBlockIndex; curTryBlockIndex++, pEntry++) {

        DEBUGMSG(DBGEH,(L"FindHandler: Scanning Try Block: pEntry=%08x curTryBlockIndex=%d  curState=%d\r\n",
                        pEntry, curTryBlockIndex, curState));
        DEBUGMSG(DBGEH,(L"FindHandler: TBME_LOW=%d  TBME_HIGH=%d\r\n",
                        TBME_LOW(*pEntry), TBME_HIGH(*pEntry)));

        if (TBME_LOW(*pEntry) > curState || curState > TBME_HIGH(*pEntry)) {
            DEBUGMSG(DBGEH,(L"FindHandler: curState(%d) not in range. Skipping Try Block(%d)\r\n",
                            curState, curTryBlockIndex));
            continue;
        }

        HandlerType *pHandlerArray = TBME_PLIST (*pEntry);
        HandlerType *pCatch   = pHandlerArray;
        int catches, ncatches = TBME_NCATCHES(*pEntry);
        int catchables;

        // Try block was in scope for current state.  Scan catches for this
        // try:
        for (catches = 0; catches < ncatches; catches++, pCatch++) {

            CatchableType *pCatchable = NULL;
            BOOLEAN Found = FALSE;

            if (!PER_IS_MSVC_EH(pExcept)){

                //
                //  Non EH exception, check for an ellipsis handler (only the last catch can be ellipsis)
                //

                DEBUGMSG(DBGEH,(L"FindHandler: Non Eh\r\n"));
                Found = HT_IS_TYPE_ELLIPSIS (pHandlerArray[catches])
                     && !HT_IS_STD_DOTDOT (pHandlerArray[catches]);

            } else {

#if _EH_RELATIVE_OFFSETS
                __int32 const *ppCatchable;
#else
                CatchableType * const *ppCatchable;
#endif

                // Scan all types that thrown object can be converted to:
                //
                DEBUGMSG(DBGEH,(L"FindHandler: to scan %d thrown objects ...\r\n",
                                THROW_COUNT(*PER_PTHROW(pExcept))));

                ppCatchable = THROW_CTLIST(*PER_PTHROW(pExcept));
                
                for (catchables = THROW_COUNT(*PER_PTHROW(pExcept)); catchables > 0; catchables--, ppCatchable++) {

#if _EH_RELATIVE_OFFSETS
                    pCatchable = (CatchableType *)(_GetThrowImageBase() + *ppCatchable);
#else
                    pCatchable = *ppCatchable;
#endif

                    if (TypeMatch(pCatch, pCatchable, PER_PTHROW(pExcept))) {
                        Found = TRUE;
                        break;
                    }

                } // Scan posible conversions
            } // Scan catch clauses

            if (Found) {
                // OK.  We finally found a match.  Activate the catch.

#ifndef _M_ARM
                DEBUGMSG(DBGEH,(L"FindHandler: pNewExcept(0x%08x) obj(0x%08x) pthrow(0x%08x)\r\n",
                                pNewExcept, PER_PEXCEPTOBJ(pNewExcept), PER_PTHROW(pNewExcept)));

                _pCurrExceptRethrow = pRethrowException;

                if (pRethrowException) {
                    DEBUGMSG(DBGEH,(L"FindHandler: set pNewExcept(0x%08x) obj|pthrow to be pExcept(0x%08x)\r\n",
                                    pNewExcept, pExcept));

                    PER_PEXCEPTOBJ(pNewExcept) = PER_PEXCEPTOBJ(pExcept);
                    PER_PTHROW(pNewExcept) = PER_PTHROW(pExcept);

                    DEBUGMSG(DBGEH,(L"FindHandler: pNewExcept(0x%08x) obj(0x%08x) pthrow(0x%08x)\r\n",
                                    pNewExcept, PER_PEXCEPTOBJ(pNewExcept), PER_PTHROW(pNewExcept)));
                }
#endif
                
                //
                //  A handler was found. Save current state and build a
                //  catch object.
                //

                TargetEntry   = pEntry;

                DEBUGMSG(DBGEH,(L"FindHandler: Catch Block Found: Handler=%08x  TargetState=%d\r\n",
                                HT_HANDLER(*pCatch), TBME_LOW(*pEntry)));
                DEBUGMSG(DBGEH,(L"FindHandler: set TargetEntry=pEntry=%08x\r\n",
                                pEntry));
                //
                //  Set the Unwind continuation address to the address of
                //  the catch handler.
                //

                pDC->ControlPc = (ULONG)HT_HANDLER(*pCatch);

                if (pCatchable) {
                    DEBUGMSG(DBGEH,(L"FindHandler: Call BuildCatchObject\r\n"));
#ifdef _M_ARM
                    BuildCatchObject(pExcept, (EHRegistrationNode  *) EstablisherFrame, pCatch, pCatchable);
#else
                    BuildCatchObject(pExcept, pRN, pCatch, pCatchable);
#endif
                }

                DEBUGMSG(DBGEH,(L"FindHandler: EXIT handler Found\r\n"));
                return TRUE;
            }
            
        } // Scan catch clauses

    } // Scan try blocks

    DEBUGMSG(DBGEH,(L"FindHandler: EXIT return search again\r\n"));

    return FALSE;

} // FindHandler

////////////////////////////////////////////////////////////////////////////////
//
// TypeMatch - Check if the catch type matches the given throw conversion.
//
// Returns:
//     TRUE if the catch can catch using this throw conversion, FALSE otherwise.

static int TypeMatch(
    HandlerType   *pCatch,         // Type of the 'catch' clause
    CatchableType *pCatchable,     // Type conversion under consideration
    ThrowInfo     *pThrow          // General information about the thrown type.
    )
{
    // First, check for match with ellipsis:
    if (HT_IS_TYPE_ELLIPSIS(*pCatch)) {
        return TRUE;
    }

    // Not ellipsis; the basic types match if it's the same record *or* the
    // names are identical.
    if (HT_PTD(*pCatch) != CT_PTD(*pCatchable)
        && strcmp(HT_NAME(*pCatch), CT_NAME(*pCatchable)) != 0) {
        return FALSE;
    }

    // Basic types match.  The actual conversion is valid if:
    //   caught by ref if ref required *and*
    //   the qualifiers are compatible *and*
    //   the alignments match *and*
    //   the volatility matches

    return (!CT_BYREFONLY(*pCatchable) || HT_ISREFERENCE(*pCatch))
        && (!THROW_ISCONST(*pThrow) || HT_ISCONST(*pCatch))
#ifdef ARM
        && (!THROW_ISUNALIGNED(*pThrow) || HT_ISUNALIGNED(*pCatch))
#endif
        && (!THROW_ISVOLATILE(*pThrow) || HT_ISVOLATILE(*pCatch));

} // TypeMatch


////////////////////////////////////////////////////////////////////////////////
//
// FrameUnwindFilter - Allows possibility of continuing through SEH during
//   unwind.
//

static int FrameUnwindFilter(
    EXCEPTION_POINTERS *pExPtrs
    )
{
    EHExceptionRecord *pExcept = (EHExceptionRecord *)pExPtrs->ExceptionRecord;

    switch (PER_CODE(pExcept)) {
    case EH_EXCEPTION_NUMBER:
        __ProcessingThrow = 0;
        std::terminate();

#ifdef ALLOW_UNWIND_ABORT
    case EH_ABORT_FRAME_UNWIND_PART:
        return EXCEPTION_EXECUTE_HANDLER;
#endif

    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// __FrameUnwindToState - Unwind this frame until specified state is reached.
//
// Returns:
//     No return value.
//
// Side Effects:
//     All objects on frame which go out of scope as a result of the unwind are
//       destructed.
//     Registration node is updated to reflect new state.
//
// Usage:
//      This function is called both to do full-frame unwind during the unwind
//      phase (targetState = -1), and to do partial unwinding when the current
//      frame has an appropriate catch.

extern "C" void __FrameUnwindToState (
    EHRegistrationNode *pRN,            // Registration node for subject function
    PDISPATCHER_CONTEXT pDC,            // Context within subject frame
    FuncInfo           *pFuncInfo,      // Static information for subject function
    __ehstate_t         targetState     // State to unwind to
    )
{
    DEBUGMSG(DBGEH,(L"__FrameUnwindToState: ENTER: pRN=%08x  pDC->ControlPc=%08x targetState=%d\r\n",
                    pRN, pDC->ControlPc, targetState));

    __ehstate_t curState = GetCurrentState(pRN, pDC, pFuncInfo);
    __ProcessingThrow++;

    TryBlockMapEntry *pSavedEntry = TargetEntry;
#ifdef _M_ARM
    ptrdiff_t unwindImageBase = _GetImageBase();
#else
    EHExceptionRecord *pSaveRethrowException = (EHExceptionRecord *)_pCurrExceptRethrow;
#endif

    DEBUGMSG(DBGEH,(L"__FrameUnwindToState: curState=GetCurrentState=%d\r\n",
                    curState));

    __try {
        while ((curState != EH_EMPTY_STATE) && (curState > targetState))
        {
            DEBUGCHK((curState > EH_EMPTY_STATE) && (curState < FUNC_MAXSTATE(*pFuncInfo)));

            // Get state after next unwind action
            __ehstate_t nxtState = UWE_TOSTATE(FUNC_UNWIND(*pFuncInfo, curState));

            DEBUGMSG(DBGEH,(L"__FrameUnwindToState: curState=%d\r\n", curState));

            __try {

                // Call the unwind action (if one exists):
                if (UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)) != NULL) {

                    DEBUGMSG(DBGEH,(L"__FrameUnwindToState: to call _CallSettingFrame with action @%08x pRN=%08x\r\n",
                                    UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)), pRN));

                    // Before calling unwind action, adjust state as if it were
                    // already completed:
                    __SetState(pRN, pDC, pFuncInfo, nxtState);

                    _CallSettingFrame(UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)),
                                      pRN,
                                      DC_NVREGS(pDC),
                                      0x103);

                    _SetImageBase(unwindImageBase);
                }
                else {
                    DEBUGMSG(DBGEH,(L"__FrameUnwindToState: no action for curState=%d\r\n",
                                    curState));
                }
            } __except(FrameUnwindFilter(exception_info())) {
            }

            // Adjust the state:
            curState = nxtState;

            DEBUGMSG(DBGEH,(L"__FrameUnwindToState: adjust curState=%d\r\n", curState));
        }
    } __finally {

        // restore EH state (in case there was a nested C++ exception during unwinding)
        //
        TargetEntry = pSavedEntry;
#ifndef _M_ARM
        _pCurrExceptRethrow = pSaveRethrowException;
#endif
        if (__ProcessingThrow > 0) {
            __ProcessingThrow--;
        }
    }

    ASSERT(curState == EH_EMPTY_STATE || curState <= targetState);

    DEBUGMSG(DBGEH,(L"__FrameUnwindToState: EXIT curState=targetState=%d\r\n", curState));

    __SetState(pRN, pDC, pFuncInfo, curState);

} // __FrameUnwindToState

////////////////////////////////////////////////////////////////////////////////
//
// AdjustPointer - Adjust the pointer to the exception object to a pointer to a
//   base instance.
//
// Output:
//     The address point of the base.
//
// Side-effects:
//     NONE.

static void *AdjustPointer(
    void *pThis,                  // Address point of exception object
    const PMD& pmd                // Generalized pointer-to-member descriptor
    )
{
    char *pRet = (char *)pThis + pmd.mdisp;

    if (pmd.pdisp >= 0) {
        pRet += *(ptrdiff_t *)((char *)*(ptrdiff_t *)((char *)pThis + pmd.pdisp)
                               + pmd.vdisp);
        pRet += pmd.pdisp;
    }

    return pRet;
}

////////////////////////////////////////////////////////////////////////////////
//
// BuildCatchObject - Copy or construct the catch object from the object thrown.
//
// Returns:
//     nothing.
//
// Side-effects:
//     A buffer in the subject function's frame is initialized.
//
// Open issues:
//     What happens if the constructor throws?  (or faults?)

static void BuildCatchObject(
    EHExceptionRecord  *pExcept,     // Original exception thrown
    EHRegistrationNode *pRN,         // Registration node of catching function
    HandlerType        *pCatch,      // The catch clause that got it
    CatchableType      *pConv        // The conversion to use
    )
{
    // If the catch is by ellipsis, then there is no object to construct.
    // If the catch is by type(No Catch Object), then leave too!
    if (HT_IS_TYPE_ELLIPSIS(*pCatch) || !HT_DISPCATCH(*pCatch)) {
        return;
    }

    void **pCatchBuffer = (void **) OffsetToAddress(HT_DISPCATCH(*pCatch), pRN);

    DEBUGMSG(DBGEH,(L"BuildCatchObject pRN=%08x\r\n",pRN));

    __try {
        if (HT_ISREFERENCE(*pCatch)) {

            // The catch is of form 'reference to T'.  At the throw point we
            // treat both 'T' and 'reference to T' the same, i.e.
            // pExceptionObject is a (machine) pointer to T.  Adjust as
            // required.

            DEBUGMSG(DBGEH,(L"Object thrown is HT_ISREFERENCE\r\n"));

            *pCatchBuffer = PER_PEXCEPTOBJ(pExcept);
            *pCatchBuffer = AdjustPointer(*pCatchBuffer, CT_THISDISP(*pConv));

        } else if (CT_ISSIMPLETYPE(*pConv)) {

            // Object thrown is of simple type (this including pointers) copy
            // specified number of bytes.  Adjust the pointer as required.  If
            // the thing is not a pointer, then this should be safe since all
            // the entries in the THISDISP are 0.
            DEBUGMSG(DBGEH,(L"Object thrown is CT_ISSIMPLETYPE\r\n"));

            memmove(pCatchBuffer, PER_PEXCEPTOBJ(pExcept), CT_SIZE(*pConv));

            if (CT_SIZE(*pConv) == sizeof(void*) && *pCatchBuffer != NULL) {
                *pCatchBuffer = AdjustPointer(*pCatchBuffer,
                                              CT_THISDISP(*pConv));
            }

        } else {

            DEBUGMSG(DBGEH,(L"BuildCatchObject: Object thrown is UDT\r\n"));

            if (CT_COPYFUNC(*pConv) == NULL) {

                // The UDT had a simple ctor.  Adjust in the thrown object,
                // then copy n bytes.

                DEBUGMSG(DBGEH,(L"BuildCatchObject: No copy ctor is provided\r\n"));

                memmove(pCatchBuffer,
                        AdjustPointer(PER_PEXCEPTOBJ(pExcept),CT_THISDISP(*pConv)),
                        CT_SIZE(*pConv));
            } else {

                // It's a UDT: make a copy using copy ctor

                DEBUGMSG(DBGEH,(L"BuildCatchObject: The copy ctor (0x%08x) provided \r\n",
                                     CT_COPYFUNC(*pConv)));

                if (CT_HASVB(*pConv)) {

                    DEBUGMSG(DBGEH,(L"BuildCatchObject: The class has virtual base 0x%08x\r\n",
                                         CT_HASVB(*pConv)));

                    _CallMemberFunction2((char *)pCatchBuffer,
                                         CT_COPYFUNC(*pConv),
                                         AdjustPointer(PER_PEXCEPTOBJ(pExcept), CT_THISDISP(*pConv)),
                                         1);
                } else {
                    DEBUGMSG(DBGEH,(L"BuildCatchObject: The class has No virtual base CT_HASVB\r\n"));

                    _CallMemberFunction1((char *)pCatchBuffer,
                                         CT_COPYFUNC(*pConv),
                                         AdjustPointer(PER_PEXCEPTOBJ(pExcept), CT_THISDISP(*pConv)));
                }
            }
        }

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(DBGEH,(L"BuildCatchObject: Something went wrong when building the catch object.\r\n"));

        std::terminate();
    }

    DEBUGMSG(DBGEH,(L"BuildCatchObject: EXIT\r\n"));

} // BuildCatchObject


////////////////////////////////////////////////////////////////////////////////
//
// _DestructExceptionObject - Call the destructor (if any) of the original
//   exception object.
//
// Returns: None.
//
// Side-effects:
//     Original exception object is destructed.
//
// Notes:
//     If destruction throws any exception, and we are destructing the exception
//       object as a result of a new exception, we give up.  If the destruction
//       throws otherwise, we let it be.

extern "C" void _DestructExceptionObject(
    EHExceptionRecord *pExcept,         // The original exception record
    BOOLEAN fThrowNotAllowed            // TRUE if destructor not allowed to throw
    )
{
    void (*pfnUnwindFunc)(void *) = NULL;
    if (pExcept
        && PER_IS_MSVC_EH(pExcept)
        && PER_PTHROW(pExcept)) {
        pfnUnwindFunc = THROW_UNWINDFUNC(*PER_PTHROW(pExcept));
    }
    DEBUGMSG(DBGEH,(L"_DestructExceptionObject: Func=%08x pExcept=%08x\r\n",
                        pfnUnwindFunc, pExcept));


    if (pfnUnwindFunc) {

        __try {

            // M00REVIEW: A destructor has additional hidden arguments, doesn't it?

            DEBUGMSG(DBGEH,(L"_DestructExceptionObject Func=%08x Obj=%08x\r\n",
                            pfnUnwindFunc, PER_PEXCEPTOBJ(pExcept)));

#pragma warning(disable:4191)

            _CallMemberFunction0(PER_PEXCEPTOBJ(pExcept),
                                 pfnUnwindFunc);

        } __except(fThrowNotAllowed
                   ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {

            // Can't have new exceptions when we're unwinding due to another
            // exception.
            std::terminate();
        }

#pragma warning(default:4191)
    }
}



///////////////////////////////////////////////////////////////////////////////
//
// std::uncaught_exception() - Returns true after completing evaluation of a
//                             throw-expression until either completing
//                             initialization of the exception-declaration in
//                             the matching handler.
//

bool __cdecl std::uncaught_exception()
{
    return (__ProcessingThrow != 0);
}


