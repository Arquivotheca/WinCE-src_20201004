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
/*
 *              NK Kernel code
 *
 *
 * Module Name:
 *
 *              msgqueue.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel message queue routines
 *
 */
#include "kernel.h"

static HANDLE           g_hMsgQHeap;    // heap for message queue
static CRITICAL_SECTION g_MsgQcs;

#define MAX_NODE_SIZE       0x100000    // max node size = 1M
#define MAX_NUM_NODES       0x100000    // max # of nodes = 1M
#define MAX_PREALLOC_SIZE   0x1000000   // max pre-alloc size = 16M

// Bit in the queue flags to denote that the message queue is invalid; any 
// attempt to open the message queue after the queue is marked invalid 
// will be an error.
// This bit is set if one end of the queue is closed and the queue is opened
// as !MSGQUE_ALLOW_BROKEN
#define MSGQUEUE_INVALID 0x80000000
#define MSGQUEUE_SFXLEN  0x2 // length, in characters, of :R or :W

// any outstanding data in the queue?
#define IS_QUEUE_EMPTY(q)   (!(q)->pHead && (!(q)->pAlert || !(q)->pAlert->dwDataSize))

#define PHD2MSGQ(phd)       (((PEVENT) (phd)->pvObj)->pMsgQ)
#define ISWRITER(lpe)       ((lpe)->manualreset & MSGQ_WRITER)

static LPVOID MsgQmalloc (DWORD dwSize)
{
    return NKHeapAlloc (g_hMsgQHeap, dwSize);
}

static BOOL MsgQfree (LPVOID pMem)
{    
    return NKHeapFree (g_hMsgQHeap, pMem);
}

static void LockQueue (PMSGQUEUE lpQ)
{
    EnterCriticalSection (&lpQ->csLock);
}


static void UnlockQueue (PMSGQUEUE lpQ)
{
    LeaveCriticalSection (&lpQ->csLock);
}

static void Enqueue(PMSGQUEUE lpQ, PMSGNODE lpN)
{
    DEBUGCHK (OwnCS (&lpQ->csLock));
    lpN->pNext = NULL;
    if (lpQ->pTail == NULL) {
        DEBUGCHK(lpQ->pHead == NULL);
        lpQ->pHead = lpN;
    } else {
        lpQ->pTail->pNext = lpN;
    }
    lpQ->pTail = lpN;
}

static PMSGNODE Dequeue(PMSGQUEUE lpQ)
{
    PMSGNODE lpN = lpQ->pHead;

    DEBUGCHK (OwnCS (&lpQ->csLock));
    if (lpN) {
        lpQ->pHead = lpN->pNext;
        if (lpQ->pHead == NULL) {
            DEBUGCHK(lpQ->pTail == lpN);
            lpQ->pTail = NULL;
        }
    }
    return lpN;
}

static BOOL SetQEvent (PHDATA phdQ)
{
    return phdQ? EVNTModify ((PEVENT) phdQ->pvObj, EVENT_SET) : FALSE;
}

static BOOL ResetQEvent (PHDATA phdQ)
{
    return phdQ? EVNTModify ((PEVENT) phdQ->pvObj, EVENT_RESET) : FALSE;
}

static DWORD WaitQEvent (PHDATA phdQ, DWORD dwTimeout)
{
    DEBUGCHK (phdQ);
    return DoWaitForObjects (1, &phdQ, dwTimeout);
}

static PMSGNODE CreateAlertNode (PMSGQUEUE lpQ)
{
    PMSGNODE lpN;

    // pOpt->cbMaxMessage is validated in NKCreateMsgQueue
    PREFAST_ASSUME (lpQ->cbMaxMessage <= MAX_NODE_SIZE);
    
    lpN = MsgQmalloc (sizeof(MSGNODE) + lpQ->cbMaxMessage);
    
    if (lpN) {
        // setup alert node
        lpN->phdTok = NULL;
        lpN->dwDataSize = 0;
    }

    return lpN;
}

static BOOL PreAllocQNodes (PMSGQUEUE lpQ)
{
    BOOL fRet = TRUE;
    if (!(lpQ->dwFlags & MSGQUEUE_NOPRECOMMIT)) {
        DWORD     cbSize = lpQ->cbMaxMessage;
        DWORD     cNode  = lpQ->dwMaxNumMsg - lpQ->dwCurNumMsg;
        ULONGLONG u64TotalSize = (ULONGLONG) cbSize * cNode;

        DEBUGCHK (cbSize);

        // pOpt->cbMaxMessage is validated in NKCreateMsgQueue
        PREFAST_ASSUME (lpQ->cbMaxMessage <= MAX_NODE_SIZE);

        if ((MAX_PREALLOC_SIZE < u64TotalSize)
            || (NULL == (lpQ->pAlert = CreateAlertNode (lpQ)))) {
            fRet = FALSE;

        } else {
            PMSGNODE lpn;
            
            // allocate message nodes
            while (cNode--) {
                lpn = MsgQmalloc (sizeof(MSGNODE) + cbSize);
                if (!lpn) {
                    return FALSE;
                }

                lpn->pNext     = lpQ->pFreeList;
                lpQ->pFreeList = lpn;
                lpn->phdTok    = NULL;
            }
        }
    }
    return fRet;
}

// allocate message queue structure
static PMSGQUEUE AllocateMsgQ (PMSGQUEUEOPTIONS pOpt)
{
    PMSGQUEUE lpQ;
    
    lpQ = MsgQmalloc (sizeof (MSGQUEUE));
    DEBUGMSG (ZONE_MAPFILE, (L"Message Queue created, lpQ = %8.8lx, cbMaxMessage = %8.8lx\r\n", lpQ, pOpt->cbMaxMessage));
    if (lpQ) {
        memset (lpQ, 0, sizeof (MSGQUEUE));
        lpQ->dwFlags      = pOpt->dwFlags;
        lpQ->cbMaxMessage = pOpt->cbMaxMessage;
        lpQ->dwMaxNumMsg  = pOpt->dwMaxMessages;
        InitializeCriticalSection (&lpQ->csLock);
    }

    return lpQ;
}

static void LinkQToPHD (PMSGQUEUE lpQ, PHDATA phdQ, BOOL fRead)
{
    PEVENT lpe = (PEVENT) phdQ->pvObj;

    DEBUGCHK (lpe->manualreset);
    
    LockQueue (lpQ);
    lpe->pMsgQ = lpQ;
    if (fRead) {
        lpQ->phdReader = phdQ;
        if (!IS_QUEUE_EMPTY (lpQ)) {
            SetQEvent (phdQ);
        }
    } else {
        lpe->manualreset |= MSGQ_WRITER;
        lpQ->phdWriter = phdQ;
    }
    UnlockQueue (lpQ);
}

static void FreeNode (PMSGNODE pNode)
{
    if (pNode) {
        UnlockHandleData (pNode->phdTok);
        MsgQfree (pNode);
    }
}

static void FreeAllNodes (PMSGNODE pHead)
{
    PMSGNODE lpN;
    while (pHead) {
        lpN = pHead;
        pHead = lpN->pNext;
        FreeNode (lpN);
    }
}

BOOL InitMsgQueue(void)
{
    InitializeCriticalSection(&g_MsgQcs);
    VERIFY (NULL != (g_hMsgQHeap = NKCreateHeap ()));

    DEBUGMSG (1, (L"Message Queue support initialized, g_hMsgQHeap = %8.8lx\r\n", g_hMsgQHeap));
    return g_hMsgQHeap != NULL;
}


//
// NOTE: caller of this funciton is responsible for unlocking *pphd, even if returning NULL.
//       the reason is that this funciton is called while holding g_MsgQcs, and calling UnlockHnadleData
//       inside can cause deadlock if it's the last ref and the object got destroyed.
//
static HANDLE CreateAndLockQEvt (BOOL fCreate, BOOL fInitSet, LPCWSTR pszName, PHDATA *pphd, LPDWORD pdwErr)
{
    PMSGQUEUE pMsgQ;
    HANDLE hEvt;

    PREFAST_SUPPRESS (25084, "CE supports only all access bits for open event");    
    hEvt = fCreate
                    ? NKCreateMsgQEvent (NULL, TRUE, fInitSet, pszName)
                    : NKOpenMsgQEvent (EVENT_ALL_ACCESS, FALSE, pszName);

    if (hEvt) {
        *pdwErr = pszName? GetLastError () : 0;
        *pphd = LockHandleData (hEvt, pActvProc);
        if (!*pphd) {
            *pdwErr = ERROR_INVALID_PARAMETER;
            hEvt = NULL;
        } else if ((ERROR_ALREADY_EXISTS == *pdwErr)
            && ((NULL == (pMsgQ = PHD2MSGQ (*pphd))) || (pMsgQ->dwFlags & MSGQUEUE_INVALID))) {
            // a named event exists, but not message queue or it points to a message queue
            // which is in a broken state; in both cases fail the call to open the message queue
            *pdwErr = ERROR_INVALID_NAME;
            // UnlockHandleData (*pphd);  -- see note above.
            HNDLCloseHandle (pActvProc, hEvt);
            hEvt = NULL;
        }
    } else {
        *pdwErr = fCreate? GetLastError() : 0;
    }
    return hEvt;
}

static HANDLE CreateUnnamedMsgQ (PMSGQUEUEOPTIONS pOpt, LPDWORD pdwErr)
{
    HANDLE    hQ   = NULL;
    PHDATA    phdQ = NULL;
    PMSGQUEUE lpQ  = AllocateMsgQ (pOpt);

    if (lpQ
        && PreAllocQNodes (lpQ)
        && (NULL != (hQ = CreateAndLockQEvt (TRUE, !pOpt->bReadAccess, NULL, &phdQ, pdwErr)))) {

        LinkQToPHD (lpQ, phdQ, pOpt->bReadAccess);
        
    } else if (lpQ) {
        // error and need cleanup
        *pdwErr = ERROR_OUTOFMEMORY;
        FreeNode (lpQ->pAlert);
        FreeAllNodes (lpQ->pFreeList);
        DeleteCriticalSection (&lpQ->csLock);
        MsgQfree (lpQ);
    }

    UnlockHandleData (phdQ);
    return hQ;
}

static HANDLE CreateNamedMsgQ (__in_ecount(len) LPWSTR pQName, DWORD len, PMSGQUEUEOPTIONS pOpt, LPDWORD pdwErr)
{
    HANDLE    hQ        = NULL;
    PHDATA    phdQ      = NULL;
    PHDATA    phdOtherQ = NULL;
    PMSGQUEUE lpQ       = NULL;
    PMSGQUEUE lpQNew    = NULL;

    DEBUGCHK (pQName && len);

    // It's possible that 2 thread calling CreateMsgQueue with the same name at the same time.
    // Need to serialize the calls.
    EnterCriticalSection (&g_MsgQcs);

    // try to find our end of the queue
    hQ = CreateAndLockQEvt (TRUE, !pOpt->bReadAccess, pQName, &phdQ, pdwErr);
    if (hQ && !*pdwErr) {

        // try the other end if this is not an existing queue.
        HANDLE hOtherQ;

        pQName[len-1] = pOpt->bReadAccess? 'W' : 'R';
        hOtherQ = CreateAndLockQEvt (FALSE, FALSE, pQName, &phdOtherQ, pdwErr);
        if (hOtherQ) {
            DEBUGCHK (ERROR_ALREADY_EXISTS == *pdwErr);
            VERIFY (NULL != (lpQ = PHD2MSGQ (phdOtherQ)));
            HNDLCloseHandle (pActvProc, hOtherQ);
        }

        // create a new queue if the other end doesn't exist either
        if (!*pdwErr && (NULL == (lpQ = AllocateMsgQ (pOpt)))) {
            *pdwErr = ERROR_OUTOFMEMORY;
        }

        if (lpQ) {
            LinkQToPHD (lpQ, phdQ, pOpt->bReadAccess);
        }

    } else if (hQ 
        && (pOpt->dwFlags & MSGQUEUE_TOKEN_SUPPORT)) {
        // Set token support flag for an existing queue.
        lpQNew = PHD2MSGQ (phdQ);
        lpQNew->dwFlags |= MSGQUEUE_TOKEN_SUPPORT;
    }

    LeaveCriticalSection (&g_MsgQcs);

    // if new queue created, pre-allocate nodes if necessary
    if (!*pdwErr) {
        DEBUGCHK (lpQ);
        LockQueue (lpQ);
        if (!PreAllocQNodes (lpQ)) {
            *pdwErr = ERROR_OUTOFMEMORY;
        }
        UnlockQueue (lpQ);
    }

    UnlockHandleData (phdOtherQ);
    UnlockHandleData (phdQ);

    if (hQ && *pdwErr && (ERROR_ALREADY_EXISTS != *pdwErr)) {
        // error, close the handle. Queue will be destroyed 
        // if this is the last reference.
        HNDLCloseHandle (pActvProc, hQ);
        hQ = NULL;
    }

    return hQ;

}

static HANDLE OpenOrCreateMsgQueue (
    __in_ecount_opt(len) LPCWSTR pQName,
    DWORD len, 
    PMSGQUEUEOPTIONS lpOptions,
    LPDWORD lpdwErr
    )
{
    HANDLE hQRtn = NULL;
    WCHAR szQName[MAX_PATH];
            
    if (pQName) {
        if (len) {
#pragma warning( push )                    
#pragma prefast(disable: 22105, "len is checked by the caller to be less than MAX_PATH - MSGQUEUE_SFXLEN");
#pragma prefast(disable: 26015, "len is checked by the caller to be less than MAX_PATH - MSGQUEUE_SFXLEN");
            memcpy (szQName, pQName, len * sizeof(WCHAR));
#pragma warning( pop )
        }
        szQName[len] = ':';                
        szQName[++len] = lpOptions->bReadAccess? 'R' : 'W';
        szQName[++len] = 0;
    }
    
    // update arguments if default is specified
    if (!lpOptions->dwMaxMessages) {
        lpOptions->dwFlags |= MSGQUEUE_NOPRECOMMIT;
        lpOptions->dwMaxMessages = MAX_NUM_NODES+1;
    }

    hQRtn = pQName
        ? CreateNamedMsgQ (szQName, len, lpOptions, lpdwErr)
        : CreateUnnamedMsgQ (lpOptions, lpdwErr);

    return hQRtn;
}

HANDLE NKCreateMsgQueue (
    LPCWSTR pQName,                 // IN only
    PMSGQUEUEOPTIONS lpOptions
    )
{
    MSGQUEUEOPTIONS opt;
    DWORD dwErr = 0;
    HANDLE hQRtn = NULL;
    DWORD len = (pQName) ? NKwcslen (pQName) : 0;

    DEBUGMSG (ZONE_MAPFILE, (L"NKCreateMsgQueue %8.8lx %8.8lx\r\n", pQName, lpOptions));
    
    // make a local copy of the arguments for security and validate arguments
    opt = *lpOptions;
    opt.dwFlags &= ~MSGQUEUE_INVALID; // filter out flags set internally

    if (   !opt.cbMaxMessage                        // max size can't be 0
        || (opt.cbMaxMessage > MAX_NODE_SIZE)       // node size too big
        || (opt.dwSize < sizeof (opt))              // invalid size field
        || (opt.dwMaxMessages > MAX_NUM_NODES) // invalid max # of message
        || (len >= MAX_PATH-MSGQUEUE_SFXLEN) // invalid name
        || (opt.dwFlags & ~(MSGQUEUE_NOPRECOMMIT | MSGQUEUE_ALLOW_BROKEN | MSGQUEUE_TOKEN_SUPPORT))) { // invalid flags
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {
        hQRtn = OpenOrCreateMsgQueue (pQName, len, &opt, &dwErr);
    }
    
    DEBUGMSG (ZONE_MAPFILE || (dwErr && (dwErr != ERROR_ALREADY_EXISTS)), (L"NKCreateMsgQueue returns %8.8lx, dwErr = %8.8lx\r\n", hQRtn, dwErr));
    SetLastError (dwErr);
    return hQRtn;
}

//
// MSGQClose - called when one end of the message queue is totally closed
//
BOOL MSGQClose (PEVENT lpe) 
{
    PMSGQUEUE lpQ = lpe->pMsgQ;
    BOOL      fDestroy = FALSE;
    PMSGNODE  pFreeList = NULL, pHead = NULL, pAlert = NULL;

    if (lpQ) {

        EnterCriticalSection (&g_MsgQcs);

        DEBUGMSG (ZONE_MAPFILE, (L"MSGQClose: lpe = %8.8lx, lpQ = %8.8lx\r\n", lpe, lpQ));
        LockQueue (lpQ);

        // signal the queue event to wake up all reader/writes
        EVNTModify (lpe, EVENT_SET);

        // reset the msgQ hdata ptr
        if (ISWRITER (lpe)) {
            lpQ->phdWriter = NULL;
        } else {
            lpQ->phdReader = NULL;
        }

        // destroy the queue if there is no reader and no write
        fDestroy = !lpQ->phdReader && !lpQ->phdWriter;

        // free nodes if queue doesn't allow to be broken
        if (fDestroy || !(MSGQUEUE_ALLOW_BROKEN & lpQ->dwFlags)) {
            pFreeList  = lpQ->pFreeList;
            pHead      = lpQ->pHead;
            pAlert     = lpQ->pAlert;
            lpQ->pHead = lpQ->pTail = lpQ->pFreeList = lpQ->pAlert = NULL;
            lpQ->dwCurNumMsg = 0;
            lpQ->dwFlags |= MSGQUEUE_INVALID;
        }

        UnlockQueue (lpQ);
        DEBUGMSG (ZONE_MAPFILE, (L"MSGQClose: returns, fDestroy = %d\r\n", fDestroy));

        LeaveCriticalSection (&g_MsgQcs);

        // Free all the nodes
        FreeNode (pAlert);
        FreeAllNodes (pHead);
        FreeAllNodes (pFreeList);

        if (fDestroy) {
            DEBUGCHK (lpQ);
            DeleteCriticalSection (&lpQ->csLock);
            MsgQfree (lpQ);
        }
    }
    
    return TRUE;
}

//
// Notified when all handles to the message queue is closed (can still be locked)
//
BOOL MSGQPreClose (PEVENT lpe)
{
    PMSGQUEUE lpQ = (PMSGQUEUE) lpe->pMsgQ;

    if (lpQ) {
        DEBUGMSG (ZONE_MAPFILE, (L"MSGQPreClose: lpe = %8.8lx, lpQ = %8.8lx\r\n", lpe, lpQ));
        LockQueue (lpQ);

        // signal the queue event to wake up all reader/writes
        if (lpQ->phdReader) {
            EVNTModify (lpQ->phdReader->pvObj, EVENT_SET);
        }

        if (lpQ->phdWriter) {
            EVNTModify (lpQ->phdWriter->pvObj, EVENT_SET);
        }

        UnlockQueue (lpQ);
    }
    return TRUE;
}

HANDLE NKOpenMsgQueueEx(
    PPROCESS pprc,
    HANDLE   hMsgQ,                // Event handle, NOT internal handle data
    HANDLE   hDstPrc,
    PMSGQUEUEOPTIONS lpOptions
    )
{
    PHDATA    phdDstPrc  = NULL;
    PPROCESS  pprcDst;
    PHDATA    phdQ = NULL;
    HANDLE    hQRtn = 0;
    PMSGQUEUE lpQ;
    DWORD     dwErr = 0;
    MSGQUEUEOPTIONS opt;

    DEBUGMSG (ZONE_MAPFILE, (L"NKOpenMsgQueueEx %8.8lx %8.8lx %8.8lx %8.8lx\r\n", pprc, hMsgQ, hDstPrc, lpOptions));

    if (GetCurrentProcess () == hDstPrc) {
        pprcDst   = pActvProc;
    } else {
        phdDstPrc = LockHandleData (hDstPrc, pActvProc);
        pprcDst   = GetProcPtr (phdDstPrc);
    }
    
    // validate parameters, including option struct size
    if (!hMsgQ
        || !pprcDst
        || !CeSafeCopyMemory (&opt, lpOptions, sizeof (opt))
        || (opt.dwSize < sizeof (opt))
        || (NULL == (phdQ = LockHandleData (hMsgQ, pprc)))
        || (NULL == (lpQ = PHD2MSGQ (phdQ)))
        || (lpQ->dwFlags & MSGQUEUE_INVALID)) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {

        // change active process, so the handle created belongs to the destination process
        PPROCESS  pprcActv = SwitchActiveProcess (pprcDst);
        
        if (phdQ->pName) {
        
            // named, just call OpenOrCreateMsgQueue to create the named queue. Skip the ":W"
            // or ":R" of the name.
            DWORD len = NKwcslen (phdQ->pName->name) - MSGQUEUE_SFXLEN;

            opt.cbMaxMessage  = lpQ->cbMaxMessage;
            opt.dwFlags       = 0;
            opt.dwMaxMessages = 0;
            hQRtn = OpenOrCreateMsgQueue (phdQ->pName->name, len, &opt, &dwErr);
            
        } else {

            PHDATA phdNewQ = NULL;

            DEBUGMSG (ZONE_MAPFILE, (L"NKOpenMsgQueueEx: opt.bReadAccess = %8.8lx\r\n", opt.bReadAccess));
            LockQueue (lpQ);
            phdNewQ = opt.bReadAccess? lpQ->phdReader : lpQ->phdWriter;
            if (phdNewQ) {

                // the requested end of the queue already exist, just dup it and done        
                hQRtn = HNDLDupWithHDATA (pprcDst, phdNewQ, &dwErr);
                
            } else {
            
                // the requested end doesn't exist, create a new one
                hQRtn = CreateAndLockQEvt (TRUE, !opt.bReadAccess, NULL, &phdNewQ, &dwErr);
                if (hQRtn) {
                    // unnamed, link the new event to the queue
                    LinkQToPHD (lpQ, phdNewQ, opt.bReadAccess);
                }

                UnlockHandleData (phdNewQ);

            }
            UnlockQueue (lpQ);
        }
        
        SwitchActiveProcess (pprcActv);
    }

    UnlockHandleData (phdQ);
    UnlockHandleData (phdDstPrc);

    DEBUGMSG (ZONE_MAPFILE, (L"NKOpenMsgQueueEx returns %8.8lx dwErr = %8.8lx\r\n", hQRtn, dwErr));
    SetLastError (dwErr);
    return hQRtn;
    
}

ERRFALSE (WAIT_OBJECT_0 == 0);

static DWORD WaitForQueueEvent (PHDATA phdQ, PMSGQUEUE lpQ, DWORD dwTimeout, DWORD dwWakeupTime)
{
    DWORD dwErr = WAIT_TIMEOUT;
    PTHREAD pCurTh = pCurThread;
    
    ResetQEvent (phdQ);

    UnlockQueue (lpQ);

    if ((INFINITE == dwTimeout)
        || ((int) (dwTimeout = dwWakeupTime - OEMGetTickCount ()) > 0)) {

        SET_USERBLOCK(pCurTh);
        dwErr = WaitQEvent (phdQ, dwTimeout);
        CLEAR_USERBLOCK(pCurTh);

        if (GET_DYING(pCurTh) && !GET_DEAD(pCurTh)) {
            // we're being terminated, while reading/writing message queue
            dwErr = WAIT_ABANDONED;
        }
    }

    LockQueue (lpQ);
    if (WAIT_TIMEOUT == dwErr)
        dwErr = ERROR_TIMEOUT; // for BC

    return dwErr;
}

// we'll do copying without release CS if packet size if under the threshold
// for perf improvemnt
#define COPY_THRESHOLD  128

BOOL  MSGQRead (
    PEVENT  lpe,                            // Event object of the message queue
    __out_bcount(cbSize) LPVOID  lpBuffer,  // OUT only
    DWORD   cbSize,
    LPDWORD lpNumberOfBytesRead,            // OUT only
    DWORD   dwTimeout,
    LPDWORD pdwFlags,                       // OUT only
    PHANDLE phTok                           // OUT only, to receive sender's token
    )
{
    PPROCESS pprc = pActvProc;
    DWORD       dwErr = ERROR_INVALID_HANDLE;
    PMSGQUEUE   lpQ;

    // cap the max timeout value
    if ((dwTimeout != INFINITE) && (dwTimeout > MAX_TIMEOUT)) {
        dwTimeout = MAX_TIMEOUT;
    }

    // validate parameters
    if (   !lpBuffer
        || ((int) cbSize <= 0)
        || !lpNumberOfBytesRead
        || !pdwFlags) {
        dwErr = ERROR_INVALID_PARAMETER;

    // make sure thisis the reader end of the queue
    } else if (!ISWRITER (lpe) && (NULL != (lpQ = lpe->pMsgQ))) {

        DWORD    dwWakeupTime = (INFINITE == dwTimeout)? INFINITE : (OEMGetTickCount () + dwTimeout);
        PMSGNODE lpN = NULL;

        LockQueue (lpQ);
        do {
        
            if (!GetHandleCountFromHDATA (lpQ->phdReader)) {
                // all handle to the read-end closed while we're reading.
                dwErr = ERROR_INVALID_HANDLE;
                break;
            }
            
            // queue broken?            
            if (!(lpQ->dwFlags & MSGQUEUE_ALLOW_BROKEN) && !GetHandleCountFromHDATA (lpQ->phdWriter)) {
                dwErr = ERROR_PIPE_NOT_CONNECTED;
                break;
            }
            
            // any data available?
            lpN = ((lpQ->pAlert && lpQ->pAlert->dwDataSize)? lpQ->pAlert : lpQ->pHead);
            if (lpN) {
                dwErr = 0;
                break;
            }
        } while (0 == (dwErr = WaitForQueueEvent (lpQ->phdReader, lpQ, dwTimeout, dwWakeupTime)));

        if (!dwErr) {
            
            HANDLE hTok = NULL;
            DEBUGCHK (lpN);

            //duplicate the sender token
            if (lpN->phdTok && phTok) {
                hTok = HNDLDupWithHDATA (pprc, lpN->phdTok, &dwErr);
            }

            // unlock the sender's token (we've already duplicated it)
            DoUnlockHDATA (lpN->phdTok, -HNDLCNT_INCR);
            lpN->phdTok = NULL;
            
            if (!dwErr) {

                BOOL   fUnlocked = FALSE;
                
                // data available, check size. Succeed even if passed in buffer is < message size (MSDN).
                if (cbSize < lpN->dwDataSize) {
                    dwErr = ERROR_INSUFFICIENT_BUFFER;
                } else {
                    cbSize = lpN->dwDataSize;
                }
                
                // unlock queue if it's not an alert message and the size exceed threshold
                //
                if (lpQ->pAlert != lpN) {

                    DEBUGCHK (lpN == lpQ->pHead);
                    Dequeue (lpQ);
                    lpQ->dwCurNumMsg --;

                    // unlock the queue if size exceeds threshold (perf consideration, don't unlock/re-lock
                    // if the data size is small enough
                    if (cbSize > COPY_THRESHOLD) {
                        UnlockQueue (lpQ);
                        fUnlocked = TRUE;
                    }
                }

                // copy data
                __try {
                    memcpy(lpBuffer, (lpN+1), cbSize);
                    *lpNumberOfBytesRead = cbSize;
                    *pdwFlags = lpN->dwFlags;
                    if (phTok) {
                        *phTok = hTok; // hTok could be NULL if this is system token
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(1 ,(TEXT("ReadMsgQueue: copy data error\r\n")));
                    dwErr = ERROR_INVALID_PARAMETER;
                    if (hTok) {
                        HNDLCloseHandle (pprc, hTok);
                    }
                }

                // re-lock the queue if necessary
                if (fUnlocked) {
                    LockQueue (lpQ);
                }

                if (lpQ->pAlert == lpN) {
                    // free the alert node
                    lpQ->pAlert->dwDataSize = 0;

                // final queue data management (node release, max # of messages, etc)
                } else if ((lpQ->dwFlags & MSGQUEUE_NOPRECOMMIT)
                    && (lpQ->pFreeList || (lpN->dwDataSize != lpQ->cbMaxMessage))) {

                    // not pre-commit, and can't cache the node -- free it
                    MsgQfree (lpN);

                } else {

                    // max specified, or cached node
                    lpN->pNext = lpQ->pFreeList;
                    lpQ->pFreeList = lpN;
                }

                // signal room available for write
                SetQEvent (lpQ->phdWriter);

                // Need to reset event if queue is empty. Otherwise, codes using 
                // "WaitForSingleObject/WaitForMultipleObjects" on queue handle will get
                // signaled, while there is no data in the queue.
                // NOTE: We don't have to do this if we don't make queue handle waitable by 
                // WaitForSingleObject/WaitForMultipleObjects. 
                if (IS_QUEUE_EMPTY (lpQ)) {
                    ResetQEvent (lpQ->phdReader);
                }
            }
        }

        UnlockQueue (lpQ);
    }

    SetLastError (dwErr);
    return (!dwErr || (ERROR_INSUFFICIENT_BUFFER == dwErr));
}

BOOL MSGQWrite (
    PEVENT  lpe,                    // Event object of the message queue
    LPCVOID lpBuffer,               // IN only
    DWORD   cbDataSize,
    DWORD   dwTimeout,
    DWORD   dwFlags
    )
{
    DWORD       dwErr = ERROR_INVALID_HANDLE;
    PMSGQUEUE   lpQ;

    // cap the max timeout value
    if ((dwTimeout != INFINITE) && (dwTimeout > MAX_TIMEOUT)) {
        dwTimeout = MAX_TIMEOUT;
    }

    // validate parameters
    if (!lpBuffer || !cbDataSize) { // invalid queue
        dwErr = ERROR_INVALID_PARAMETER;

    // valid writer end of the queue?
    } else if (ISWRITER(lpe) && (NULL != (lpQ = lpe->pMsgQ))) {

        PMSGNODE   lpN = NULL;
        DWORD       dwWakeupTime = (INFINITE == dwTimeout)? INFINITE : (OEMGetTickCount () + dwTimeout);

        LockQueue (lpQ);

        do {
            
            // check msg size limitation
            if (cbDataSize > lpQ->cbMaxMessage) {
                dwErr = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            if (!GetHandleCountFromHDATA (lpQ->phdWriter)) {
                // all handle to the write-end closed while we're writing.
                dwErr = ERROR_INVALID_HANDLE;
                break;
            }

            // queue broken?
            if (!(lpQ->dwFlags & MSGQUEUE_ALLOW_BROKEN) && !GetHandleCountFromHDATA (lpQ->phdReader)) {
                dwErr = ERROR_PIPE_NOT_CONNECTED;
                break;
            }

            // alert message?
            if (dwFlags & MSGQUEUE_MSGALERT) {
                if (!lpQ->pAlert && (NULL == (lpQ->pAlert = CreateAlertNode (lpQ)))) {
                    dwErr = ERROR_OUTOFMEMORY;
                    break;

                } else if (!lpQ->pAlert->dwDataSize) {
                    lpN = lpQ->pAlert;
                    break;
                }
            }

            // anything in the free list?
            lpN = lpQ->pFreeList;
            if (lpN) {
                lpQ->pFreeList = lpN->pNext;
                break;
            }

            // allocate memory if not pre-commit
            if ((lpQ->dwFlags & MSGQUEUE_NOPRECOMMIT)
                && (lpQ->dwCurNumMsg < lpQ->dwMaxNumMsg)) {

                lpN = (PMSGNODE) MsgQmalloc (cbDataSize + sizeof(MSGNODE));
                if (!lpN) {
                    dwErr = ERROR_OUTOFMEMORY;
                }
                break;
            }
            // when we gets here, queue is full.

        } while (0 == (dwErr = WaitForQueueEvent (lpQ->phdWriter, lpQ, dwTimeout, dwWakeupTime)));

        if (lpN) {
            BOOL fUnlocked = FALSE;
            // got space in queue

            // unlock queue if it's not an alert message and the size exceed threshold
            //
            if (lpQ->pAlert != lpN) {

                lpQ->dwCurNumMsg ++;

                // unlock the queue if size exceeds threshold
                if (cbDataSize > COPY_THRESHOLD) {
                    UnlockQueue (lpQ);
                    fUnlocked = TRUE;
                }
            }

            __try {

                // start copying data            
                memcpy (lpN+1, lpBuffer, cbDataSize);
                lpN->dwDataSize = cbDataSize;
                lpN->dwFlags = dwFlags;
                lpN->phdTok = NULL;

                dwErr = 0;

            }  __except (EXCEPTION_EXECUTE_HANDLER) {
                DEBUGMSG(1, (TEXT("WriteMsgQueue: copy data error\r\n")));
                dwErr = ERROR_INVALID_PARAMETER;
            }

            // re-lock the queue if necessary
            if (fUnlocked) {
                LockQueue (lpQ);
            }
            
            if (!dwErr) {

                if (lpQ->pAlert != lpN) {
                    Enqueue (lpQ, lpN);

                    // update high water mark if necessary
                    if (lpQ->dwMaxQueueMessages < lpQ->dwCurNumMsg) {
                        lpQ->dwMaxQueueMessages = lpQ->dwCurNumMsg;
                    }
                }

                // queue is no longer empty, signal the read event
                SetQEvent (lpQ->phdReader);

                // Need to reset event if queue is full. Otherwise, codes using 
                // "WaitForSingleObject/WaitForMultipleObjects" on queue handle will get
                // signaled, while there is no space available in the queue.
                // NOTE: We don't have to do this if we don't make queue handle waitable by 
                // WaitForSingleObject/WaitForMultipleObjects. 
                if (lpQ->dwCurNumMsg == lpQ->dwMaxNumMsg) {
                    ResetQEvent (lpQ->phdWriter);
                }


            // else - restore the queue state from error
            } else {

                // unlock the token handle
                DoUnlockHDATA (lpN->phdTok, -HNDLCNT_INCR);
                lpN->phdTok = NULL;
                
                if (lpQ->pAlert == lpN) {
                    // free the alert node
                    lpQ->pAlert->dwDataSize = 0;
                    
                } else {
                    lpQ->dwCurNumMsg --;

                    if (lpQ->dwFlags & MSGQUEUE_NOPRECOMMIT) {
                        // not pre-commit, just free it
                        MsgQfree (lpN);
                    } else {
                        // put it back to free list
                        lpN->pNext = lpQ->pFreeList;
                        lpQ->pFreeList = lpN;
                    }
                }
            }
                
        }

        UnlockQueue (lpQ);
    }

    SetLastError (dwErr);
    return NOERROR == dwErr;

}



BOOL MSGQGetInfo (
    PEVENT lpe,            // Event handle, NOT internal handle data
    PMSGQUEUEINFO lpInfo   // IN/OUT: dwSize is IN, all else is OUT
    )
{
    DWORD        dwErr = 0;
    MSGQUEUEINFO info;      // local copy of the info
    PMSGQUEUE    lpQ    = lpe->pMsgQ;

    // validate parameters
    if (!lpQ
        || !CeSafeCopyMemory (&info.dwSize, &lpInfo->dwSize, sizeof (DWORD))
        || (info.dwSize < sizeof (info))) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        LockQueue (lpQ);
        info.dwFlags             = (lpQ->dwFlags & ~MSGQUEUE_INVALID); // filter out the flags not set by user
        info.cbMaxMessage        = lpQ->cbMaxMessage;
        info.dwCurrentMessages   = lpQ->dwCurNumMsg;
        info.dwMaxQueueMessages  = lpQ->dwMaxQueueMessages;
        info.wNumReaders         = (WORD) GetHandleCountFromHDATA (lpQ->phdReader);
        info.wNumWriters         = (WORD) GetHandleCountFromHDATA (lpQ->phdWriter);
        // we modified dwMaxMessages to simplified 'full test', special care here
        info.dwMaxMessages       = (lpQ->dwMaxNumMsg <= MAX_NUM_NODES)? lpQ->dwMaxNumMsg : 0;
        UnlockQueue (lpQ);

        if (!CeSafeCopyMemory (lpInfo, &info, sizeof (info))) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    }

    if (dwErr) {
        SetLastError (dwErr);
    }

    return !dwErr;
}


