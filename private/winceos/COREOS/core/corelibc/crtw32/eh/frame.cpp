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

// define CC_OUTERTRY for all platforms that use smd directory
#define CC_OUTERTRY


#if defined(_M_IX86)
#define __OffsetToAddress(a, b, c)  OffsetToAddress(a, b)
#define DC_CONTEXTRECORD(pDC) (NULL)
#else
#define __OffsetToAddress(a, b, c)  _OffsetToAddress(a, b, c)
#define DC_CONTEXTRECORD(pDC) (pDC->ContextRecord)
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Forward declaration of local functions:
//

// M00TODO: all these parameters should be declared const

extern "C" void __FrameUnwindToState(
    EHRegistrationNode *,
    PDISPATCHER_CONTEXT,
    FuncInfo *,
    __ehstate_t
    );

static BOOLEAN FindHandler(
    EHExceptionRecord *,
    EHRegistrationNode *,
    CONTEXT *,
    PDISPATCHER_CONTEXT,
    FuncInfo *,
    int
    );

static void * CallCatchBlock(
    EHExceptionRecord *,
    EHRegistrationNode *,
    CONTEXT *,
    FuncInfo *,
    void *,
    int,
    unsigned long
    );

static void BuildCatchObject(
    EHExceptionRecord *,
    EHRegistrationNode *,
    HandlerType *,
    CatchableType *
    );

static __inline int TypeMatch(
    HandlerType *,
    CatchableType *,
    ThrowInfo *
    );

static void * AdjustPointer(
    void *,
    const PMD&
    );

static int FrameUnwindFilter(
    EXCEPTION_POINTERS *
    );

static BOOLEAN CanThisExceptionObjectBeDestructed(
    EHExceptionRecord *pExcept,
    BOOLEAN abnormalTermination
    );

////////////////////////////////////////////////////////////////////////////////
//
// __InternalCxxFrameHandler - the frame handler for all functions with C++ EH
// information.
//
// If exception is handled, this doesn't return; otherwise, it returns
// ExceptionContinueSearch.
//
//

extern "C" EXCEPTION_DISPOSITION __cdecl __InternalCxxFrameHandler(
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    CONTEXT            *pContext,       // Context info
    PDISPATCHER_CONTEXT pDC,            // Context within subject frame
    FuncInfo           *pFuncInfo,      // Static information for this frame
    int CatchDepth                      // How deeply nested are we?
    )
{
    DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler: ENTER\r\n")));

    DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler: pExcept=%08x pFuncInfo=%08x  pRN=%08x  CatchDepth=%d\r\n"),
                    pExcept, pFuncInfo, pRN, CatchDepth));
    DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler:                  ExceptionCode=%08x  ExceptionFlags=%08x\r\n"),
                    pExcept->ExceptionCode, pExcept->ExceptionFlags));
    DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler:                  pDC->ControlPc=%08x\r\n"),
                    pDC->ControlPc));
    DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler:                  pExceptionObject=%08x pThrowInfo= %08x\r\n"),
                    PER_PEXCEPTOBJ(pExcept), PER_PTHROW(pExcept)));

    if ((PER_CODE(pExcept) != EH_EXCEPTION_NUMBER) &&
        (FUNC_MAGICNUM(*pFuncInfo) >= EH_MAGIC_NUMBER3) &&
        ((FUNC_FLAGS(*pFuncInfo) & FI_EHS_FLAG) != 0))
    {

        // This function was compiled /EHs so we don't need to do anything in
        // this handler.

        return ExceptionContinueSearch;
    }

    if (IS_UNWINDING(PER_FLAGS(pExcept)))
    {
        // We're at the unwinding stage of things.  Don't care about the
        // exception itself.

        DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler: unwinding ...\r\n")));

        if ( IS_TARGET_UNWIND(PER_FLAGS(pExcept) )){

            DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler: IS_TARGET_UNWIND\r\n")));

            __ehstate_t targetState = TBME_LOW(*TargetEntry);

            DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler:               targetState=%d from TargetEntry=%08x\r\n"),
                            targetState, TargetEntry));

            __FrameUnwindToState(pRN, pDC, pFuncInfo, targetState);
#if defined(_M_IX86)
            SetState(pRN, TBME_HIGH(*TargetEntry)+1);
#endif // _M_IX86

            DEBUGMSG(DBGEH,(TEXT("__InternalCxxFrameHandler: after unwind, call CallCatchBlock\r\n")));

            void * continuationAddress
                = CallCatchBlock(pExcept,
                                 pRN,
                                 pContext,
                                 pFuncInfo,
                                 (void *)CONTEXT_TO_PROGRAM_COUNTER(pContext),
                                 CatchDepth,  0x100);
            DEBUGMSG(DBGEH,(TEXT("from CallCatchBlock: continuationAddress %08x\r\n"),
                            continuationAddress));
            CONTEXT_TO_PROGRAM_COUNTER(pContext) = (ULONG)continuationAddress;
        } else if (
#ifdef _M_IX86
          FUNC_MAXSTATE(*pFuncInfo) != 0 && CatchDepth == 0
#else
          FUNC_MAXSTATE(*pFuncInfo) != 0
#endif // _M_IX86
            ) {
            // Only unwind if there's something to unwind
            DEBUGMSG(DBGEH,(TEXT("InternalCxxFrameHandler: Unwind to empty state\n")));

            __FrameUnwindToState(pRN, pDC, pFuncInfo, EH_EMPTY_STATE);
        }

    } else if (FUNC_NTRYBLOCKS(*pFuncInfo) != 0) {

        //
        //  If try blocks are present, search for a handler:
        //

        DEBUGMSG(DBGEH,(TEXT("InternalCxxFrameHandler: NTRYBLOCKS=%d\r\n"),
                        FUNC_NTRYBLOCKS(*pFuncInfo)));

        DEBUGMSG(DBGEH,(TEXT("InternalCxxFrameHandler: call FindHandler\r\n")));

        if ( FindHandler(pExcept, pRN, pContext, pDC, pFuncInfo, CatchDepth) ) {
            DEBUGMSG(DBGEH,(TEXT("InternalCxxFrameHandler: EXIT with pExcept(%08x): Handler Found %08x\n"),
                            pExcept, pDC->ControlPc));
            return ExceptionExecuteHandler;
        }

        // If it returned FALSE, we didn't have any matches.
        DEBUGMSG(DBGEH,(TEXT("InternalCxxFrameHandler: No Handler Found\r\n")));
    }

    // We had nothing to do with it or it was rethrown.  Keep searching.
    DEBUGMSG(DBGEH,(TEXT("InternalCxxFrameHandler EXIT with pExcept(%08x): ExceptionContinueSearch\r\n"), pExcept));

    return ExceptionContinueSearch;

} // InternalCxxFrameHandler


#ifndef _M_IX86

//
// __CxxFrameHandler3 - Real entry point to the runtime
//
extern "C" _CRTIMP EXCEPTION_DISPOSITION __CxxFrameHandler3(
    EHExceptionRecord  *pExcept,           // Information for this exception
    EHRegistrationNode *pRN,               // Dynamic information for this frame
    CONTEXT            *pContext,          // Context info
    PDISPATCHER_CONTEXT pDC                // More dynamic info for this frame
    )
{
    FuncInfo   *pFuncInfo;
    EXCEPTION_DISPOSITION result;

    pFuncInfo = (FuncInfo*)pDC->FunctionEntry->HandlerData;

    result = __InternalCxxFrameHandler(pExcept, pRN, pContext, pDC, pFuncInfo, 0);

    return result;

} // __CxxFrameHandler3

//
// __CxxFrameHandler - Old entrypoint to the runtime
//
extern "C" _CRTIMP EXCEPTION_DISPOSITION __CxxFrameHandler(
    EHExceptionRecord  *pExcept,           // Information for this exception
    EHRegistrationNode *pRN,               // Dynamic information for this frame
    CONTEXT            *pContext,          // Context info
    PDISPATCHER_CONTEXT pDC                // More dynamic info for this frame
    )
{
    return __CxxFrameHandler3(pExcept, pRN, pContext, pDC);
}

#endif // ifndef _M_IX86



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
    PDISPATCHER_CONTEXT pDC,         // Context within subject frame
    FuncInfo           *pFuncInfo,   // Static information for subject frame
    int                 CatchDepth   // Level of nested catch that is being checked
    )
{
    BOOLEAN Found = FALSE;

    EHExceptionRecord *pNewExcept = pExcept ;
    EHExceptionRecord *pRethrowException = NULL ;

    // Get the current state (machine-dependent)
    __ehstate_t curState = GetCurrentState(pRN, pDC, pFuncInfo);
    DEBUGMSG(DBGEH,(TEXT("FindHandler: curState=GetCurrentState=%d\r\n"), curState));

    DEBUGCHK(curState >= EH_EMPTY_STATE && curState < FUNC_MAXSTATE(*pFuncInfo));

    // Check if it's a re-throw ( ie. no new object to throw ).
    // Use the exception we stashed away if it is.
    if (PER_IS_MSVC_EH(pExcept) && PER_PTHROW(pExcept) == NULL) {

        DEBUGMSG(DBGEH,(TEXT("FindHandler: pExcept(0x%08x) is a rethrow of _pCurrentException(0x%08x)\r\n"),
                             pExcept, _pCurrentException));

        if (_pCurrentException == NULL) {
            // Oops!  User re-threw a non-existant exception!  Let it propogate.
            return FALSE;
        }

        if ( PER_PTHROW(_pCurrentException) == NULL) {
            DEBUGMSG(DBGEH,(TEXT("FindHandler: ThrowInfo is NULL\n"))) ;
#ifdef DBGEH
            __crtExitProcess(3);
#endif
        }

        pExcept = _pCurrentException;
        pContext = _pCurrentExContext;

        DEBUGMSG(DBGEH,(TEXT("FindHandler: reset pExcept=%08x pContext=%08x from _pCurrentException\r\n"),
                        pExcept, pContext));

        pRethrowException = pExcept;
        DEBUGMSG(DBGEH,(TEXT("FindHandler: rethrow: setting pRethrowException=%08x\r\n"),
                        pRethrowException));

        DEBUGCHK(!PER_IS_MSVC_EH(pExcept) || PER_PTHROW(pExcept) != NULL);
    }

#if !defined(_M_IX86)

    FRAMEINFO *pFrameInfo;

    if ((pFrameInfo = FindFrameInfo(pRN)) != NULL)
    {
        //
        //  This frame called a catch block (after unwinding). reset curState
        //  to the state reached when the unwind occurred. NestLevel is the
        //  nesting level of the called catch block. This frame's nest level
        //  is one less.
        //

        curState = pFrameInfo->state;
        CatchDepth = pFrameInfo->NestLevel - 1;
        DEBUGMSG(DBGEH,(TEXT("FindHandler: from FindFrameInfo : curState: %d CatchDepth: %d "),
                        curState, CatchDepth));
        DEBUGMSG(DBGEH,(TEXT(" FindHandler: pFrameInfo->pRN : %8.8x pFrameInfo->pCatchRN : %8.8lx\r\n"),
                        pFrameInfo->pRN, pFrameInfo->pCatchRN));

    } else if ((pFrameInfo = FindCatchFrameInfo(pRN)) != NULL)
    {
        //
        //  This frame is a catch block which called a nested catch block.
        //  Dispatch based on the nested block's level.
        //

        CatchDepth = pFrameInfo->NestLevel;
        DEBUGMSG(DBGEH,(TEXT("FindHandler: from FindCatchFrameInfo: curState: %d CatchDepth: %d "),
                        curState, CatchDepth));
        DEBUGMSG(DBGEH,(TEXT("FindHandler:  pFrameInfo->pRN : %8.8x pFrameInfo->pCatchRN : %8.8lx\r\n"),
                        pFrameInfo->pRN, pFrameInfo->pCatchRN));
    } else
    {
        DEBUGMSG(DBGEH,(TEXT("FindHandler: ====Couldn't find FrameInfo/CatchFrameInfo from pFrameInfoChain(%08x)\r\n"), pFrameInfoChain));
        DEBUGMSG(DBGEH,(TEXT("FindHandler: with pRN=%8.8x CatchDepth=%d curState=%d===\r\n"),
                        pRN, CatchDepth, curState));
    }

#endif

    // Determine range of try blocks to consider:
    // Only try blocks which are at the current catch depth are of interest.

    unsigned int curTryBlockIndex;    // the index of current try block.
    unsigned int endTryBlockIndex;    // the index of try block to end search, itself excluded

    TryBlockMapEntry *pEntry = _GetRangeOfTrysToCheck(pFuncInfo,
                                                      CatchDepth,
                                                      curState,
                                                      &curTryBlockIndex,
                                                      &endTryBlockIndex);

    DEBUGMSG(DBGEH,(TEXT("FindHandler: TBME=%08x  curTryBlockIndex=%d  endTryBlockIndex=%d  curState=%d\r\n"),
                    pEntry, curTryBlockIndex, endTryBlockIndex, curState));

    // Scan the try blocks in the function:
    for (; curTryBlockIndex < endTryBlockIndex; curTryBlockIndex++, pEntry++) {

        DEBUGMSG(DBGEH,(TEXT("FindHandler: Scanning Try Block: pEntry=%08x curTryBlockIndex=%d  curState=%d\r\n"),
                        pEntry, curTryBlockIndex, curState));
        DEBUGMSG(DBGEH,(TEXT("FindHandler: TBME_LOW=%d  TBME_HIGH=%d\r\n"),
                        TBME_LOW(*pEntry), TBME_HIGH(*pEntry)));

        if (TBME_LOW(*pEntry) > curState || curState > TBME_HIGH(*pEntry)) {
            DEBUGMSG(DBGEH,(TEXT("FindHandler: curState(%d) not in range. Skipping Try Block(%d)\r\n"),
                            curState, curTryBlockIndex));
            continue;
        }

#if !defined(_M_IX86)

        //
        //  Only consider catch blocks at a higher nesting level:
        //

        DEBUGMSG(DBGEH,(TEXT("FindHandler: CatchDepth=%d  HT_FRAMENEST=%d\r\n"),
                        CatchDepth, HT_FRAMENEST(TBME_CATCH(*pEntry, 0))));

        if ( CatchDepth >= (int)HT_FRAMENEST(TBME_CATCH(*pEntry, 0)) ){
            DEBUGMSG(DBGEH,(TEXT("FindHandler: Skipping Enclosing Catch Block. \r\n")));
            continue;
        }

#endif // !defined(_M_IX86)

        DEBUGMSG(DBGEH,(TEXT("FindHandler: In Try block scope, TBME_NCATCHES=%d\r\n"),
                        TBME_NCATCHES(*pEntry)));

        // Try block was in scope for current state.  Scan catches for this try:

        for (int catchIndex = 0; catchIndex < TBME_NCATCHES(*pEntry); catchIndex++){
            HandlerType *pCatch;
            CatchableType * const *ppCatchable;

            // get pCatch with catchIndex
            pCatch  = TBME_PCATCH(*pEntry, catchIndex);

            DEBUGMSG(DBGEH,(TEXT("FindHandler: catchIndex(%d) pCatch=%08x is trying to catch\r\n"),
                            catchIndex, pCatch));

            if (!PER_IS_MSVC_EH(pExcept)){

                //
                //  Non EH exception, check for an ellipsis handler
                //

                DEBUGMSG(DBGEH,(TEXT("FindHandler: Non Eh\r\n")));
                Found = (HT_IS_TYPE_ELLIPSIS(TBME_CATCH(*pEntry, catchIndex))
                     && (!HT_IS_STD_DOTDOT(TBME_CATCH(*pEntry, catchIndex))));
                ppCatchable = NULL;

            } else {

                //
                // Scan all types that thrown object can be converted to:
                //
                DEBUGMSG(DBGEH,(TEXT("FindHandler: to scan %d thrown objects ...\r\n"),
                                THROW_COUNT(*PER_PTHROW(pExcept))));

                ppCatchable = THROW_CTLIST(*PER_PTHROW(pExcept));

                for (int catchableIndex = 0;
                     catchableIndex < THROW_COUNT(*PER_PTHROW(pExcept));
                     catchableIndex++, ppCatchable++) {

                    DEBUGMSG(DBGEH,(TEXT("FindHandler: catchableIndex(%d) ppCatchable=%08x  properties=%08x pCatch=%08x\r\n"),
                                    catchableIndex, ppCatchable, CT_PROPERTIES(**ppCatchable), pCatch));

                    if (TypeMatch(pCatch, *ppCatchable, PER_PTHROW(pExcept))) {
                        DEBUGMSG(DBGEH,(TEXT("FindHandler: Found: *ppCatchable=%08x TypeMatched pCatch=%08x\r\n"),
                                        *ppCatchable, pCatch));
                        Found = TRUE;
                        break;
                    }

                } // Scan possible conversions

            } // MSVC_EH exception

            if ( Found ) {

                DEBUGMSG(DBGEH,(TEXT("FindHandler: pNewExcept(0x%08x) obj(0x%08x) pthrow(0x%08x)\r\n"),
                                pNewExcept, PER_PEXCEPTOBJ(pNewExcept), PER_PTHROW(pNewExcept)));

                _pCurrExceptRethrow = pRethrowException;

                if (pRethrowException) {
                    DEBUGMSG(DBGEH,(TEXT("FindHandler: set pNewExcept(0x%08x) obj|pthrow to be pExcept(0x%08x)\r\n"),
                                    pNewExcept, pExcept));

                    PER_PEXCEPTOBJ(pNewExcept) = PER_PEXCEPTOBJ(pExcept);
                    PER_PTHROW(pNewExcept) = PER_PTHROW(pExcept);

                    DEBUGMSG(DBGEH,(TEXT("FindHandler: pNewExcept(0x%08x) obj(0x%08x) pthrow(0x%08x)\r\n"),
                                    pNewExcept, PER_PEXCEPTOBJ(pNewExcept), PER_PTHROW(pNewExcept)));
                }

                //
                //  A handler was found. Save current state and build a
                //  catch object.
                //

                TargetEntry = pEntry;

                DEBUGMSG(DBGEH,(TEXT("FindHandler: Catch Block Found: Handler=%08x  TargetState=%d\r\n"),
                                HT_HANDLER(*pCatch), TBME_LOW(*pEntry)));
                DEBUGMSG(DBGEH,(TEXT("FindHandler: set TargetEntry=pEntry=%08x\r\n"),
                                pEntry));

                //
                //  Set the Unwind continuation address to the address of
                //  the catch handler.
                //

                pDC->ControlPc = (ULONG)HT_HANDLER(*pCatch);

                if (ppCatchable && *ppCatchable != NULL) {
#if defined(_M_ARM)
                    pRN = AdjustFrameForCatch(pRN, pFuncInfo);
#endif
                    DEBUGMSG(DBGEH,(TEXT("FindHandler: Call BuildCatchObject\r\n")));
                    BuildCatchObject(pExcept, pRN, pCatch, *ppCatchable);
                }

                DEBUGMSG(DBGEH,(TEXT("FindHandler: EXIT handler Found\r\n")));

                return TRUE;
            }

        } // Scan catch clauses

    } // Scan try blocks

    DEBUGMSG(DBGEH,(TEXT("FindHandler: EXIT return search again\r\n")));

    return FALSE;

} // FindHandler

////////////////////////////////////////////////////////////////////////////////
//
// TypeMatch - Check if the catch type matches the given throw conversion.
//
// Returns:
//     TRUE if the catch can catch using this throw conversion, FALSE otherwise.

static __inline int TypeMatch(
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
#if !defined(_M_IX86)
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
    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: ENTER: pRN=%08x  pDC->ControlPc=%08x targetState=%d\r\n"),
                    pRN, pDC->ControlPc, targetState));

    __ehstate_t curState = GetCurrentState(pRN, pDC, pFuncInfo);
    __ProcessingThrow++;

    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: curState=GetCurrentState=%d\r\n"),
                    curState));

#if !defined(_M_IX86)
    //
    //  If unwinding from a catch block which threw, use the unwind state from
    //  the catch block. (CatchExceptRN is set in CallCatchBlock)
    //

    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: CatchExceptRN=%08x CatchExceptState=%d\r\n"),
                    CatchExceptRN, CatchExceptState));

    if ( pRN == CatchExceptRN ) {
        curState = CatchExceptState;

        DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: pRN=CatchExceptRN=%08x\r\n"), pRN));
        DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: reset curState=%d\r\n"), curState));
    }
    CatchExceptRN = 0;

#endif // !defined(_M_IX86)

    __try {
        while (curState != targetState)
        {

            // The unwind-map may have a shortcut by using EH_EMPTY_STATE
            if ( curState == EH_EMPTY_STATE ){
                break;
            }

            DEBUGCHK((curState > EH_EMPTY_STATE) && (curState < FUNC_MAXSTATE(*pFuncInfo)));

            DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: curState=%d\r\n"), curState));

            __try {

                // Call the unwind action (if one exists):
                if (UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)) != NULL) {

#if defined(_M_ARM)
                    pRN = FindFrameForUnwind (pRN, pFuncInfo);
                    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: ARM adjusted pRN=%08x\r\n"),
                                    UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)), pRN));
                    // pass correct frame pointer for unwind funclet. In ARM
                    // the frame pointer is not stored in stack when catch
                    // block is called. so we need to pass the correct frame
                    // to access the locals
#endif // _M_ARM

                    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: to call _CallSettingFrame with action @%08x pRN=%08x\r\n"),
                                    UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)), pRN));

                    _CallSettingFrame(UWE_ACTION(FUNC_UNWIND(*pFuncInfo, curState)),
                                      pRN, DC_CONTEXTRECORD(pDC), NULL);
                }
                else {
                    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: no action for curState=%d\r\n"),
                                    curState));
                }
            } __except(FrameUnwindFilter(exception_info())) {
            }

            // Adjust the state:
            curState = UWE_TOSTATE(FUNC_UNWIND(*pFuncInfo, curState));

            DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: adjust curState=%d\r\n"), curState));
        }
    } __finally {
        if (__ProcessingThrow > 0) {
            __ProcessingThrow--;
        }
    }

    DEBUGCHK(curState == targetState);

    DEBUGMSG(DBGEH,(TEXT("__FrameUnwindToState: EXIT curState=targetState=%d\r\n"), curState));

} // __FrameUnwindToState



#define IS_RETHROW(pNewExcept, pExcept)  \
       ( pNewExcept && pExcept && pNewExcept!=pExcept \
       && PER_PEXCEPTOBJ(pNewExcept)==PER_PEXCEPTOBJ(pExcept) \
       && PER_PTHROW(pNewExcept)==PER_PTHROW(pExcept))


////////////////////////////////////////////////////////////////////////////////
//
//  CanThisExceptionObjectBeDestructed
//
//  This function is invoked to determine that exception object can be destroyed
//  or not:
//
//  return TRUE means that the object can be destructed; FALSE otherwise.
//
//  Note:
//  The object we are trying to delete is from the throw line, constructed by
//  copy constructor.
//  Example: throw a0; the copy of a0 is the exception object that
//  we are talking about here.
//
// _pCurrentException is actually the previous exception besides pExcept
//
static BOOLEAN CanThisExceptionObjectBeDestructed(EHExceptionRecord *pExcept,
                                                  BOOLEAN abnormalTermination)
{
    BOOLEAN ret = FALSE;

    DEBUGMSG(DBGEH,(TEXT("Checking: pExcept(%08x), obj(%08x), abnormalTermination(%d)\r\n"),
                    pExcept, PER_PEXCEPTOBJ(pExcept), abnormalTermination));
    DEBUGMSG(DBGEH,(TEXT("Checking:          _pCurrExceptRethrow(%08x), _pCurrentException(%08x)\r\n"),
                    _pCurrExceptRethrow, _pCurrentException));

    if ( abnormalTermination )
    {
        // as long as pExcept is not the same as _pCurrExceptRethrow
        if ( pExcept!=_pCurrExceptRethrow )
        {
            ret = TRUE;
        }
    }
    else
    {
        // Important point:
        // Just because we found normal exit from a catch does not imply that we can destroy
        // the source of the local exception object (original exception object). This is
        // because a catch may be nested inside another. After exiting a nested catch, we
        // may find a rethrow of the original exception object. For this reason, we can only
        // destroy the original exception object once we have left the scope of the catch which
        // caught it.
        //
        // For normal termination we destroy pExcept if any of the following are true:
        // 1) No pCurrExceptRethrow at all.
        // 2) pExcept is the same of pCurrExceptRethrow, and pExcept itself
        //    is not a rethrow of _pCurrentException.
        // 3) Exception is rethrow and _pCurrExceptRethrow is not the previous recorded exception
        //    because another rethrow might be thrown.
        if ( !_pCurrExceptRethrow ||
             ( pExcept==_pCurrExceptRethrow &&
               !IS_RETHROW(pExcept, _pCurrentException) ) ||
             ( IS_RETHROW((EHExceptionRecord *)_pCurrExceptRethrow, pExcept) &&
              _pCurrExceptRethrow!=_pCurrentException ) )
        {
            ret = TRUE;
        }
    }

    DEBUGMSG(DBGEH,(TEXT("Checking: EXIT ret:%08x\r\n"),
                    ret));

    return ret;
}


////////////////////////////////////////////////////////////////////////////////
//
//  CallCatchBlock
//
//  This function is called after the shx\target frame has been unwound (all unwind
//  actions have been performed. The catch block is now invoked.
//
//

static void *CallCatchBlock(
    EHExceptionRecord  *pExcept,        // The exception thrown
    EHRegistrationNode *pRN,            // Dynamic info of function with catch
    CONTEXT            *pContext,       // Context info
    FuncInfo           *pFuncInfo,      // Static info of function with catch
    void               *handlerAddress, // Code address of handler
    int                 CatchDepth,     // How deeply nested in catch blocks
    unsigned long       NLGCode         // NLG destination code
    )
{
    // Address where execution resumes after exception handling completed.
    // Initialized to non-NULL (value doesn't matter) to distinguish from
    // re-throw in __finally.
    void *continuationAddress = handlerAddress;

#if !defined(_M_IX86)
    FRAMEINFO NewFrame = {};
#if defined(_M_ARM)
    CATCHFRAMEINFO NewCatchFrame;
#endif // _M_ARM
#endif // !defined(_M_IX86)

    DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: ENTER, to invoke handler @%08x  pRN=%08x\r\n"),
                    handlerAddress, pRN));


    // Save the current exception in case of a rethrow.  Save the previous value
    // on the stack, to be restored when the catch exits.
    EHExceptionRecord *pSaveException ;
    CONTEXT *pSaveExContext ;
    // Save the current rethrow exception. This is because rethrows can be nested. To be
    // restored when the catch exits.
    EHExceptionRecord *pSaveRethrowException ;

    DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: save _pCurrentException=%08x _pCurrentExContext=%08x\r\n"),
                    _pCurrentException, _pCurrentExContext));

    pSaveException = _pCurrentException;
    pSaveExContext = _pCurrentExContext;

    pSaveRethrowException = (EHExceptionRecord *)_pCurrExceptRethrow;

    _pCurrentException = pExcept;
    _pCurrentExContext = pContext;

    DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: put pExcept=%08x into _pCurrentException\r\n"),
                    pExcept));
    DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: put pContext=%08x into _pCurrentExContext\r\n"),
                    pContext));

    __try {

#if defined(_M_IX86)
        // The current stack pointer is stored just ahead of the
        // registration node.  We need to add it to the context
        // structure (for the continuation) before calling the catch
        // block because it will be changed in the catch block if
        // there's a nested try/catch block.
        pContext->Esp = *((unsigned long*)pRN - 1);
        continuationAddress = _CallCatchBlock2(pRN, pFuncInfo, handlerAddress,
                                               CatchDepth, 0);

        // Get frame pointer by looking past the exception
        // registration node on the stack per __CallSettingFrame
        pContext->Ebp = (unsigned long) pRN + sizeof(EHRegistrationNode);
#else
        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: Fill NewFrame fields all but pCatchRN...\r\n")));
        // this will be set in CallSetttingFrame
        NewFrame.pCatchRN = NULL;
        NewFrame.pRN = pRN;
        NewFrame.state = (FUNC_UNWIND(*pFuncInfo,
                                      TBME_LOW(*TargetEntry))).toState;
        NewFrame.NestLevel = HT_FRAMENEST(TBME_CATCH(*TargetEntry, 0));
        NewFrame.next = pFrameInfoChain;

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: NewFrame=%08x pRN=%08x state=%d NestLevel=%d next=%08x\r\n"),
                        &NewFrame, NewFrame.pRN, NewFrame.state, NewFrame.NestLevel, NewFrame.next));

        pFrameInfoChain = &NewFrame;

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: insert NewFrame=%08x into pFrameInfoChain\r\n"),
                        &NewFrame));

#if defined(_M_ARM)
        NewCatchFrame.pRN = pRN;
        NewCatchFrame.NestLevel = HT_FRAMENEST(TBME_CATCH(*TargetEntry, 0));
        NewCatchFrame.pFuncInfo = pFuncInfo ;
        NewCatchFrame.next = pCatchChain;
        pRN = FindCatchFrame(pRN, HT_FRAMENEST(TBME_CATCH(*TargetEntry, 0)));
        pCatchChain = &NewCatchFrame;
#endif // _M_ARM

        continuationAddress = _CallSettingFrame(handlerAddress, pRN,
                                                pContext, &NewFrame );

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: from _CallSettingFrame: pCatchRN=%08x\r\n"),
                        NewFrame.pCatchRN));

#endif // _M_IX86

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: from _CallSettingFrame continuationAddress %08x\r\n"),
                        continuationAddress));

    } __finally {

        // DEBUGCHK(pExcept==_pCurrentException);

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: Restore the _pCurrentException\r\n")));

        _pCurrentException = pSaveException;
        _pCurrentExContext = pSaveExContext;

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: _pCurrentException=%08x _pCurrentExContext=%08x\r\n"),
                        _pCurrentException, _pCurrentExContext));

        // Destroy the original exception object if we're not exiting on a
        // re-throw.  Note that the catch handles destruction of its parameter.

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: pExcept(0x%08x), PER_PTHROW(pExcept):%08x, PER_PEXCEPTOBJ(pExcept):%08x _pCurrentException=%08x\r\n"),
                        pExcept, PER_PTHROW(pExcept), PER_PEXCEPTOBJ(pExcept), _pCurrentException));

#if !defined(_M_IX86)

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: abnormal_termination(%d) _pCurrExceptRethrow(0x%08x)\r\n"),
                        abnormal_termination(), _pCurrExceptRethrow));
#endif // !defined(_M_IX86)

        if (PER_IS_MSVC_EH(pExcept) && PER_PTHROW(pExcept) != NULL
            && CanThisExceptionObjectBeDestructed(pExcept, (abnormal_termination() != 0)))
        {
            DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: calling DestructExceptionObject pExcept=%08x, obj(%08x)\r\n"),
                            pExcept, PER_PEXCEPTOBJ(pExcept)));
            _DestructExceptionObject(pExcept, (abnormal_termination() != 0));
        }

        _pCurrExceptRethrow = pSaveRethrowException;

#if !defined(_M_IX86)
        //
        //  If the catch block raised an exception, save the pRN and state
        //  for unwinding through the associated try block. The lifetime of
        //  these values is from here until the next call to FrameUnwindToState
        //
        if (abnormal_termination()) {
            DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: abnormal_termination. Catch Block Raised an exception ")));
            CatchExceptRN = NewFrame.pRN;
            CatchExceptState = NewFrame.state;
            DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: save CatchExceptRN=%08x CatchExceptState=%d from NewFrame\r\n"),
                            CatchExceptRN, CatchExceptState));
        }

#if defined(_M_ARM)
        //
        // Remove current catch frame from list
        //
        CATCHFRAMEINFO *lcurFrame = pCatchChain;
        CATCHFRAMEINFO **lnextFrame = &pCatchChain;
        while ( lcurFrame != NULL ){
            if (lcurFrame == &NewCatchFrame ) {

                *lnextFrame = lcurFrame->next;
                DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: Removing %08x: pCatchChain=%08x\r\n"),
                                lcurFrame, pCatchChain));
            }
            lnextFrame = &lcurFrame->next;
            lcurFrame = lcurFrame->next;
        }
#endif // _M_ARM
#endif // !defined(_M_IX86)

    } // finally

#ifndef _M_IX86

    // to update pFrameInfoChain here.
    // we remove not only NewFrame when normal_termination occurs, but also residing FrameInfo
    // which had been created by previous CallCatchBlock while abnormal_termination happened.
    // If the state of pFrameInfo on the pFrameInfoChain is bigger than NewFrame's state,
    // we are sure that pFrameInfo is enclosed by NewFrame's state, given that try/catch
    // can not be overlapped.
    {
        FRAMEINFO *prevFrame = NULL;
        FRAMEINFO *curFrame = pFrameInfoChain;

        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: normal_termination. update pFrameChainInfo ")));
        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: remove NewFrame(%08x) from pFrameInfoChain(%08x)\r\n"),
                        &NewFrame, pFrameInfoChain));

        // iterate the pFrameInfoChain to look for NewFrame to remove

        // Assumption: stack grows while memory decreases.

        while ( curFrame != NULL ) {

            if ( curFrame->pRN <= NewFrame.pRN ) {
                // curFrame's establisher frame was established after NewFrame's.
                // curFrame can be safely removed from the chain.

                DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: Removing curFrame=%08x from pFrameInfoChain=%08x\r\n"),
                                curFrame, pFrameInfoChain));

                if ( curFrame == pFrameInfoChain ) {
                    pFrameInfoChain = curFrame->next;
                } else {
                    prevFrame->next = curFrame->next;
                }

                DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: curFrame: pRN=%08x CatchRN=%08x state=%d NestLevel=%d\r\n"),
                                curFrame->pRN, curFrame->pCatchRN,
                                curFrame->state, curFrame->NestLevel));

            }
            prevFrame = curFrame;
            curFrame = curFrame->next;
        }
        DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: After removal NewFrame(%08x), pFrameInfoChain(%08x)\r\n"),
                        &NewFrame, pFrameInfoChain));
    }
#endif // _M_IX86

    DEBUGMSG(DBGEH,(TEXT("CallCatchBlock: EXIT pExcept(%08x) with continuationAddress=%08x\r\n"),
                    pExcept, continuationAddress));

    return continuationAddress;

} // CallCatchBlock


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

    void **pCatchBuffer = (void **)__OffsetToAddress(HT_DISPCATCH(*pCatch), pRN,
                                                     HT_FRAMENEST(*pCatch));
#ifndef _M_IX86
    DEBUGMSG(DBGEH,(TEXT("BuildCatchObject pRN=%08x Level=%d\r\n"),pRN,
                    HT_FRAMENEST(*pCatch)));
#endif // _M_IX86

    __try {
        if (HT_ISREFERENCE(*pCatch)) {

            // The catch is of form 'reference to T'.  At the throw point we
            // treat both 'T' and 'reference to T' the same, i.e.
            // pExceptionObject is a (machine) pointer to T.  Adjust as
            // required.

            DEBUGMSG(DBGEH,(TEXT("Object thrown is HT_ISREFERENCE\r\n")));

            *pCatchBuffer = PER_PEXCEPTOBJ(pExcept);
            *pCatchBuffer = AdjustPointer(*pCatchBuffer, CT_THISDISP(*pConv));

        } else if (CT_ISSIMPLETYPE(*pConv)) {

            // Object thrown is of simple type (this including pointers) copy
            // specified number of bytes.  Adjust the pointer as required.  If
            // the thing is not a pointer, then this should be safe since all
            // the entries in the THISDISP are 0.
            DEBUGMSG(DBGEH,(TEXT("Object thrown is CT_ISSIMPLETYPE\r\n")));

            memmove(pCatchBuffer, PER_PEXCEPTOBJ(pExcept), CT_SIZE(*pConv));

            if (CT_SIZE(*pConv) == sizeof(void*) && *pCatchBuffer != NULL) {
                *pCatchBuffer = AdjustPointer(*pCatchBuffer,
                                              CT_THISDISP(*pConv));
            }

        } else {

            DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: Object thrown is UDT\r\n")));

            if (CT_COPYFUNC(*pConv) == NULL) {

                // The UDT had a simple ctor.  Adjust in the thrown object,
                // then copy n bytes.

                DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: No copy ctor is provided\r\n")));

                memmove(pCatchBuffer, AdjustPointer(PER_PEXCEPTOBJ(pExcept),
                                                    CT_THISDISP(*pConv)),
                        CT_SIZE(*pConv));
            } else {

                // It's a UDT: make a copy using copy ctor

                DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: The copy ctor (0x%08x) provided \r\n"),
                                     CT_COPYFUNC(*pConv)));

                if (CT_HASVB(*pConv)) {

                    DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: The class has virtual base 0x%08x\r\n"),
                                         CT_HASVB(*pConv)));

                    _CallMemberFunction2((char *)pCatchBuffer,
                                         CT_COPYFUNC(*pConv),
                                         AdjustPointer(PER_PEXCEPTOBJ(pExcept),
                                                       CT_THISDISP(*pConv)), 1);
                } else {
                    DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: The class has No virtual base CT_HASVB\r\n")));

                    _CallMemberFunction1((char *)pCatchBuffer,
                                         CT_COPYFUNC(*pConv),
                                         AdjustPointer(PER_PEXCEPTOBJ(pExcept),
                                                       CT_THISDISP(*pConv)));
                }
            }
        }

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: Something went wrong when building the catch object.\r\n")));

        std::terminate();
    }

    DEBUGMSG(DBGEH,(TEXT("BuildCatchObject: EXIT\r\n")));

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

#define __ResetException(a)

void _DestructExceptionObject(
    EHExceptionRecord *pExcept,         // The original exception record
    BOOLEAN fThrowNotAllowed            // TRUE if destructor not allowed to throw
    )
{
    DEBUGMSG(DBGEH,(TEXT("_DestructExceptionObject: Func=%08x pExcept=%08x\r\n"),
                    THROW_UNWINDFUNC(*PER_PTHROW(pExcept)), pExcept));

    if (pExcept != NULL && THROW_UNWINDFUNC(*PER_PTHROW(pExcept)) != NULL) {

        __try {

            // M00REVIEW: A destructor has additional hidden arguments, doesn't it?

            DEBUGMSG(DBGEH,(TEXT("_DestructExceptionObject Func=%08x Obj=%08x\r\n"),
                            THROW_UNWINDFUNC(*PER_PTHROW(pExcept)),
                            PER_PEXCEPTOBJ(pExcept)));

#pragma warning(disable:4191)

            _CallMemberFunction0(PER_PEXCEPTOBJ(pExcept),
                                 THROW_UNWINDFUNC(*PER_PTHROW(pExcept)));

            __ResetException(pExcept);

        } __except(fThrowNotAllowed
                   ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {

            // Can't have new exceptions when we're unwinding due to another
            // exception.
            std::terminate();
        }

#pragma warning(default:4191)
    }
}


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


#if !defined(_M_IX86)

//
// Used by BuildCatchObject to copy the thrown object. Since catch-handlers are
// nested functions, and access variables with up-level addressing, we have
// to find the frame of the outer-most parent.
//
PVOID _OffsetToAddress( ptrdiff_t offset, PULONG pBase, ULONG nesting_level )
{
#if defined(_M_ARM)
    CATCHFRAMEINFO *pCatchFrame = pCatchChain;
#endif
    while( nesting_level > 1 ) {
#if defined(_M_ARM)
        pBase = (PULONG)pCatchFrame->pRN ;
        DEBUGMSG(DBGEH,(TEXT("pBASE %08x Nest Level %d\r\n"),
                        pBase, pCatchFrame->NestLevel)) ;
        pCatchFrame = pCatchFrame->next;
#elif defined(_MIPS64) // MIPS64 in this context means 64-bit registers
        pBase = (PULONG)(*(PULONG)((char*)pBase - 8));
#else
        pBase = (PULONG)(*(PULONG)((char*)pBase - 4));
#endif
        nesting_level--;
    }

    return (PVOID)(((char*)pBase) + (int)offset);
}


//
// This routine returns a corresponding frame info structure if we are executing
// from within a catch.  Otherwise, NULL is returned.
//

FRAMEINFO *
FindFrameInfo(
    EHRegistrationNode *pRN
    )
{
    FRAMEINFO *pFrameInfo = pFrameInfoChain;

    while ( pFrameInfo != NULL ){

        if ( pFrameInfo->pRN == pRN ) {
            break;
        }
        pFrameInfo = pFrameInfo->next;
    }
    return pFrameInfo;
}

FRAMEINFO *
FindCatchFrameInfo(
    EHRegistrationNode *pRN
    )
{
    FRAMEINFO *pFrameInfo = pFrameInfoChain;

    while ( pFrameInfo != NULL ){

        if ( pFrameInfo->pCatchRN == pRN ) {
            break;
        }
        pFrameInfo = pFrameInfo->next;
    }
    return pFrameInfo;
}

#ifdef CC_OUTERTRY

// _GetRangeOfTrysToCheck - determine which try blocks are of interest, given
//  the current catch block nesting depth.  We only check the trys at a single
//      depth.
//
// Returns:
//      Address of first try block of interest is returned
//      pStart and pEnd get the indices of the range in question
// This is the version of _GetRangeOfTrysToCheck that would work if
// CC_OUTERTRY is defined in the BE. To be compatible with EH format
// output by the OLD MIPS compiler, CC_OUTERTRY causes the MIPS-UTC compiler
// to emit the outer try index in the try block table instead of the
// "high catch state".
TryBlockMapEntry* _GetRangeOfTrysToCheck(
    FuncInfo   *pFuncInfo,
    int                     CatchDepth,
    __ehstate_t curState,
    unsigned   *pStart,
    unsigned   *pEnd
    )
{
    TryBlockMapEntry *pEntry;
    unsigned num_of_try_blocks = FUNC_NTRYBLOCKS(*pFuncInfo);

    DEBUGCHK( num_of_try_blocks > 0 );
    for( unsigned int index = 0; index < num_of_try_blocks; index++ )
    {
        pEntry = FUNC_PTRYBLOCK(*pFuncInfo, index);
        if(curState >= TBME_LOW(*pEntry) && curState <= TBME_HIGH(*pEntry) )
        {
            *pStart = index;
            //
            // It would be better to return the end-index itself, but I don't
            // want to change the caller's code.
            //
            *pEnd = TBME_CATCHHIGH(*pEntry) + 1;
            DEBUGCHK( *pEnd <= num_of_try_blocks && *pStart < *pEnd );
            return pEntry;
        }
    }

    *pStart = *pEnd = 0;
    return NULL;
}

#else // CC_OUTERTRY

// _GetRangeOfTrysToCheck - determine which try blocks are of interest, given
//  the current catch block nesting depth.  We only check the trys at a single
//      depth.
//
// Returns:
//      Address of first try block of interest is returned
//      pStart and pEnd get the indices of the range in question

TryBlockMapEntry *_GetRangeOfTrysToCheck(
    FuncInfo *pFuncInfo,
    int CatchDepth,
    __ehstate_t curState,
    unsigned *pStart,
    unsigned *pEnd
    )
{
    TryBlockMapEntry *pEntry = FUNC_PTRYBLOCK(*pFuncInfo, 0);
    int start;
    int end = FUNC_NTRYBLOCKS(*pFuncInfo) - 1;

    //
    // First, find the innermost try block that contains this state.  We
    // must always check this try block.
    //
    for (start = 0; start <= end; start++)
    {
        if (TBME_LOW(pEntry[start]) <= curState &&
            TBME_HIGH(pEntry[start]) >= curState)
        {
            break;
        }
    }

    // We may not check try block if we are already in an associated catch.
    // We know our current catch depth and that the try blocks are sorted
    // innermost to outermost.  Therefore, we start with the outermost try
    // block and remove it from list of try blocks to check, if we are in
    // its catch.  We continue working inward until we have accounted for
    // our current catch depth.

    if(CatchDepth > 0 && start < end)
    {
        for (; CatchDepth > 0 && start < end; end--) {

            if (TBME_HIGH(pEntry[end]) < curState &&
                curState <= TBME_CATCHHIGH(pEntry[end]))
            {
                --CatchDepth;
            }
        }
        end++;
    }

    *pStart = start;
    *pEnd = end + 1;

    DEBUGCHK(*pEnd >= *pStart && *pEnd <= FUNC_NTRYBLOCKS(*pFuncInfo));
    return &(pEntry[start]);
}

#endif // CC_OUTERTRY

#endif // !_M_IX86
