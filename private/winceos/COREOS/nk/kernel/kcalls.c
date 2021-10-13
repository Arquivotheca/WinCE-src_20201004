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
//
//    kcalls.c - implementations of all KCalls
//
#include <windows.h>

#include <kernel.h>

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

ERRFALSE (sizeof(STUBTHREAD) <= sizeof (MODULE));

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IMPORTANT NOTE FOR CRITICAL SECTION:
//
//  Kernel and user mode CS are handled differently. The hCrit member of CS is a pointer when CS is created by
//  kernel (ONLY KERNEL, NOT INCLUDING OTHER DLL LOADED INTO KERNEL). For the others, hCrit member is a handle.
//
//  Therefore, the hCrit member SHOULD NEVER BE REFERENCED WITHIN CS HANDLING CODE. An PMUTEX argument is passed
//  along all the CS functions to access the MUTEX structure.
//
//

//------------------------------------------------------------------------------
// MP support
//------------------------------------------------------------------------------
PPCB    g_ppcbs[MAX_CPU];
static volatile LONG  g_nCpuIpiDone;
volatile DWORD g_dwIpiCommand;
static volatile DWORD g_dwIpiData;

volatile LONG g_nCpuStopped = 1;
volatile LONG g_nCpuReady = 1;
volatile LONG g_fStartMP;

static PTHREAD g_pthMonitor;
static LPDWORD g_pdwSpinnerThreadId;
static LONG    g_lMaxRunnableDelay;

static DWORD   g_dwTimeInDebugger;  // time spent in debugger

//
// any thread can never be in both RunList and SleepList
//
TWO_D_QUEUE RunList;
PTHREAD     SleepList;
#define NEXT_IN_SLEEPLIST(pth)      ((pth)->pNextSleepRun)
#define PREV_IN_SLEEPLIST(pth)      ((pth)->pPrevSleepRun)


DWORD       currmaxprio = MAX_CE_PRIORITY_LEVELS - 1;
PTHREAD     pOOMThread;
volatile LONG g_PerfCounterHigh;

// get the lowest priority interrupt bit position
LONG GetHighPos (LONG);

static DWORD g_seqNumber;

SPINLOCK g_schedLock;

void MDFlushFPU (BOOL fPreempted);
void MDDiscardFPUContent (void);

// IsThreadInPSLCall - check if a thread is in the middle of a PSL call. 
FORCEINLINE BOOL IsThreadInPSLCall (PCTHREAD pth)
{
    return (pth->pprcOwner != pth->pprcActv) || (USER_MODE != GetThreadMode (pth));
}

FORCEINLINE BOOL IsThreadBeingTerminated (PCTHREAD pth)
{
    return GET_DYING(pth) && !GET_DEAD(pth)     // being terminated, but not yet exited
        && GET_USERBLOCK(pth)                   // a blocking call from user
        && (pth->pprcOwner == pth->pprcActv);   // in owner process
}

static PTWO_D_NODE FirstRunnableNode (PPCB ppcb)
{
    PTWO_D_NODE pHeadRunQ  = RunList.pHead;
    PTWO_D_NODE pHeadAfnty = ppcb->affinityQ.pHead;

    return (pHeadAfnty                                          //     affinity queue not empty,
            && (!pHeadRunQ                                      // and (   global run queue is empty,
                || (pHeadAfnty->prio < pHeadRunQ->prio)         //      or thread in affinity queue has higher priority
                || ((pHeadAfnty->prio == pHeadRunQ->prio)       //      or (threads of the 2 queues has same priority
                    && ((int) (THREAD_OF_QNODE(pHeadRunQ)->dwSeqNum - THREAD_OF_QNODE(pHeadAfnty)->dwSeqNum) > 0))))
                                                                //          and thread in affinity queue has lower sequence number)
                                                                //     )
            ? pHeadAfnty
            : pHeadRunQ;
}


static void SaveCoProcRegisters (PTHREAD pth)
{
    DEBUGCHK (OwnSchedulerLock ());
    // save the co-proc register for the thread being swap out.
    if (g_pOemGlobal->fSaveCoProcReg && pth->pCoProcSaveArea) {
        DEBUGCHK (g_pOemGlobal->pfnSaveCoProcRegs);
        g_pOemGlobal->pfnSaveCoProcRegs (pth->pCoProcSaveArea);
    }
}

void RestoreCoProcRegisters (PTHREAD pth)
{
    DEBUGCHK (InSysCall());
    DEBUGCHK (pth);
    // restore co-proc register
    if (g_pOemGlobal->fSaveCoProcReg && pth->pCoProcSaveArea) {
        DEBUGCHK (g_pOemGlobal->pfnSaveCoProcRegs);
        g_pOemGlobal->pfnRestoreCoProcRegs (pth->pCoProcSaveArea);
    }
}


//------------------------------------------------------------------------------
// PrioEnqueue - add a node to the 2-d priority queue at a given priority
//     fAtTail - insert at tail of nodes of same priority (insert at head if false)
// return TRUE if queue priority changed, FALSE otherwise.
//------------------------------------------------------------------------------
static BOOL PrioEnqueue (PTWO_D_QUEUE pQueue, PTWO_D_NODE pNode, BOOL fAtTail)
{
    DWORD       prio        = pNode->prio;
    PTWO_D_NODE pNextNode   = pQueue->pHead;
    BOOL        fRet        = (!pNextNode || (prio < pNextNode->prio));
    DWORD       idxHash     = prio/PRIORITY_LEVELS_HASHSCALE;

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    if (fRet) {
        // we're of the hightest priority

        // no previous node
        pNode->pQPrev = NULL;

        // link the next node if it exists
        pNode->pQNext = pNextNode;
        if (pNextNode) {
            pNextNode->pQPrev = pNode;
        }

        // update hash table, queue head, and up/down pointers        
        pNode->pQUp = pNode->pQDown = pQueue->Hash[idxHash] = pQueue->pHead = pNode;
        
    } else {

        // there are higher or equal priority nodes
        PTWO_D_NODE pWalkNode = pQueue->Hash[idxHash];

        // find the thread prior to us
        if (!pWalkNode) {
            // hash not exist yet. the new node become the hash node
            pQueue->Hash[idxHash] = pNode;

            // look for the previous node.
            // the loop is bounded by PRIORITY_LEVELS_HASHSIZE
            while (NULL == (pWalkNode = pQueue->Hash[--idxHash]))
                ;
        }

        if (prio < pWalkNode->prio) {
            // we're the 1st of the priority range, just replace the hash node
            pNode->pQPrev = pWalkNode->pQPrev;
            pNode->pQNext = pWalkNode;
            pNode->pQUp = pNode->pQDown = pQueue->Hash[idxHash] = pNode->pQPrev->pQNext = pWalkNode->pQPrev = pNode;

        } else {
            // need to find the node right before us.
        
            // bounded by MAX_PRIORITY_HASHSCALE
            while ((NULL != (pNextNode = pWalkNode->pQNext)) && (prio >= pNextNode->prio)) {
                pWalkNode = pNextNode;
            }

            DEBUGCHK (pWalkNode);
            DEBUGCHK (!pNextNode || (prio < pNextNode->prio));
            DEBUGCHK (pWalkNode->prio <= prio);

            if (prio != pWalkNode->prio) {

                // insert between pWalkNode and pNextNode
                pNode->pQNext = pNextNode;
                if (pNextNode) {
                    pNextNode->pQPrev = pNode;
                }
                
                pNode->pQPrev = pWalkNode;
                pWalkNode->pQNext = pNode->pQUp = pNode->pQDown = pNode;

            } else {

                // insert at the tail of the nodes of the same priority
                pNode->pQUp = pWalkNode->pQUp;
                pNode->pQDown = pWalkNode;
                pNode->pQUp->pQDown = pWalkNode->pQUp = pNode;

                if (fAtTail) {
                    // insert at tail, simply clear the next/prev of the node inserted
                    pNode->pQNext = pNode->pQPrev = NULL;

                } else {
                    // insert at head
                    // since the up-down list is a circular queue, all we need to do is to 
                    // fixup prev/next/hash to point to the new node and we're done.

                    // update hash table if necessary (if the walk node is the hash node)
                    if (pQueue->Hash[idxHash] == pWalkNode) {
                        pQueue->Hash[idxHash] = pNode;

                        // update queue head if necessary
                        if (pWalkNode == pQueue->pHead) {
                            pQueue->pHead = pNode;
                        }
                    }
                    
                    // update next/prev
                    pNode->pQNext = pNextNode;
                    if (pNextNode) {
                        pNextNode->pQPrev = pNode;
                    }
                    pNode->pQPrev = pWalkNode->pQPrev;
                    if (pNode->pQPrev) {
                        pNode->pQPrev->pQNext = pNode;
                    }

                    // clear the next/prev of the node we bumped.
                    pWalkNode->pQNext = pWalkNode->pQPrev = NULL;
                    
                }


            }
        }
    }
    return fRet;
}


//------------------------------------------------------------------------------
// PrioDequeue - remove a node form a 2-d priority queue
//------------------------------------------------------------------------------
static BOOL PrioDequeue (PTWO_D_QUEUE pQueue, PTWO_D_NODE pNode, DWORD prio) 
{
    PTWO_D_NODE pDown    = pNode->pQDown;
    PTWO_D_NODE pNext    = pNode->pQNext;
    PTWO_D_NODE pPrev    = pNode->pQPrev;
    DWORD       idxHash  = prio/PRIORITY_LEVELS_HASHSCALE;
    BOOL        fPrioChanged = FALSE;

    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (pDown);

    // update hash table if needed
    if (pQueue->Hash[idxHash] == pNode) {
        pQueue->Hash[idxHash] = ((pDown != pNode) ? pDown :
            (pNext && ((DWORD) pNext->prio/PRIORITY_LEVELS_HASHSCALE == idxHash)) ? pNext : NULL);
    }
    
    if (pDown == pNode) {
        // it's the only node of this priority
        if (pPrev) {
            // not the 1st node, link prev/next node together
            pPrev->pQNext = pNext;

        } else {
            // it's the 1st node
            DEBUGCHK (pQueue->pHead == pNode);
            pQueue->pHead = pNext;
            fPrioChanged = TRUE;
        }

        if (pNext) {
            pNext->pQPrev = pPrev;
        }
        
    } else {
        PTWO_D_NODE pUp = pNode->pQUp;

        DEBUGCHK (pUp);

        // there are other nodes of this priority
        pDown->pQUp = pUp;
        pUp->pQDown = pDown;
        
        if (pPrev) {
            // it's the top node of this priority, make the next node the top node
            pPrev->pQNext = pDown;
            pDown->pQPrev = pPrev;

        } else if (pNode == pQueue->pHead) {
            // it's the 1st node in this queue, with other node of the same priority as it
            DEBUGCHK(!pDown->pQPrev);
            pQueue->pHead = pDown;

        } else {
            DEBUGCHK (!pNext);
        }

        if (pNext) {
            pNext->pQPrev = pDown;
            pDown->pQNext = pNext;
        }
    }
    pNode->pQDown = NULL;

    return fPrioChanged;
}

//------------------------------------------------------------------------------
// PrioDequeueHead - remove the head node form a 2-d priority queue
//------------------------------------------------------------------------------
static PTWO_D_NODE PrioDequeueHead (PTWO_D_QUEUE pQueue)
{
    PTWO_D_NODE pHead = pQueue->pHead;

    DEBUGCHK (OwnSchedulerLock ());

    if (pHead) {
        PrioDequeue (pQueue, pHead, pHead->prio);
    }
    return pHead;
}


//------------------------------------------------------------------------------
// FlatEnqueue - add a proxy to a flat queue
//------------------------------------------------------------------------------
static void FlatEnqueue (PTWO_D_NODE *ppQHead, PTWO_D_NODE pNode) 
{
    PTWO_D_NODE pHead = *ppQHead;

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    if (!pHead) {
        pNode->pQUp     = pNode->pQDown = pNode;
        pNode->pQPrev   = (PTWO_D_NODE) ppQHead;
        *ppQHead        = pNode;
    } else {
        pNode->pQPrev   = NULL;
        pNode->pQUp     = pHead->pQUp;
        pNode->pQDown   = pHead;
        pHead->pQUp     = pNode->pQUp->pQDown = pNode;
    }
}

//------------------------------------------------------------------------------
// FlatDequeue - remove a proxy from a flat queue (not a 2-d priority queue)
//------------------------------------------------------------------------------
static void FlatDequeue (PTWO_D_NODE pNode) 
{
    PTWO_D_NODE pDown    = pNode->pQDown;
    PTWO_D_NODE *ppQHead = (PTWO_D_NODE *) pNode->pQPrev;

    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (pDown);
    
    if (ppQHead) { // we're first
        if (pDown == pNode) { // we're alone
            *ppQHead = NULL;
        } else {
            pDown->pQUp = pNode->pQUp;
            pNode->pQUp->pQDown = *ppQHead = pDown;
            pDown->pQPrev = (PTWO_D_NODE) ppQHead;
        }
    } else {
        pDown->pQUp = pNode->pQUp;
        pNode->pQUp->pQDown = pDown;
    }
    pNode->pQDown = 0;
}


//------------------------------------------------------------------------------
// put running thread back to run queue (preempted by higher prio thread)
//------------------------------------------------------------------------------
static void PutRunningThreadToRunQ (PTHREAD pth)
{
    PSTUBTHREAD pthStub = GetStubThread (pth);
    BOOL        fAtTail = pth->dwQuantum && !pth->dwQuantLeft;
    
    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (!IsStubThread (pth));
    DEBUGCHK (RUNSTATE_RUNNING == GET_RUNSTATE (pth));
    DEBUGCHK (pth != &dummyThread);

    SET_RUNSTATE (pth, RUNSTATE_RUNNABLE);
    
    PrioEnqueue (pth->pRunQ, QNODE_OF_THREAD (pth), fAtTail);
    pth->dwSeqNum = g_seqNumber ++;
    g_pNKGlobal->dwCntRunnableThreads ++;
    
    SaveCoProcRegisters (pth);
    MDFlushFPU (TRUE);

    SoftLog (0x66660000, pth->dwId + PcbGetCurCpu() - 1);
    if (pthStub) {
        DEBUGCHK (pth->pRunQ->pHead);

        pth = THREAD_OF_QNODE (pth->pRunQ->pHead);
        SoftLog (0x66660001, GET_CPRIO (pth));
        
        DEBUGCHK (RUNSTATE_RUNNING == GET_RUNSTATE (pthStub));
        DEBUGCHK (&pthStub->ProcReadyQ == pth->pRunQ);

        // add the stub to its run queue with the correct priority
        SET_CPRIO (pthStub, GET_CPRIO (pth));
        SET_RUNSTATE (pthStub, RUNSTATE_RUNNABLE);
        PrioEnqueue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), fAtTail);
        pthStub->dwSeqNum = g_seqNumber ++;
    }
}

//------------------------------------------------------------------------------
// make a thread runnable
//------------------------------------------------------------------------------
static VOID MakeRun (PTHREAD pth, BOOL fAtTail) 
{
    PPCB        ppcb    = GetPCB ();
    PSTUBTHREAD pthStub = GetStubThread (pth);
    DWORD       cprio   = GET_CPRIO (pth);

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (!IsStubThread (pth));

    DEBUGCHK (pth != &dummyThread);

    if (!pth->bSuspendCnt) {

        // check to see if the CPU had already been powered off
        if ((pth->dwAffinity > 1)
            && (CE_PROCESSOR_STATE_POWERED_ON != g_ppcbs[pth->dwAffinity-1]->dwCpuState)) {
            pth->dwAffinity = 1;
            pth->pRunQ      = &(g_ppcbs[0]->affinityQ);
        }

        if (!fAtTail) {
            DEBUGCHK (pth->dwQuantLeft);

            fAtTail = ! --pth->dwQuantLeft;     // take aways 1ms everytime inserted at the front. Otherwise
                                                // two threads mutually signaling each other can potentially
                                                // starve threads of the same priority.
        }

        SET_RUNSTATE (pth, RUNSTATE_RUNNABLE);
        pth->dwTimeWhenRunnable = CurMSec;        
        PrioEnqueue (pth->pRunQ, QNODE_OF_THREAD (pth), fAtTail);
        pth->dwSeqNum = g_seqNumber ++;

        g_pNKGlobal->dwCntRunnableThreads ++;

        SoftLog (0x88880000, pth->dwId + ppcb->dwCpuId - 1);
        SoftLog (0x88890000, pth->dwQuantLeft);
        
        if (pthStub) {
            DWORD stubprio = GET_CPRIO (pthStub);

            DEBUGCHK (&pthStub->ProcReadyQ == pth->pRunQ);
            DEBUGCHK (pth->pRunQ->pHead);

            switch (GET_RUNSTATE (pthStub)) {
            case RUNSTATE_RUNNING:
                // A thread in the process is running, preempt if needed
                SoftLog (0x88880001, cprio);
                if (cprio < stubprio) {
                    SetReschedule (ppcb);
                }
                break;
            case RUNSTATE_RUNNABLE:
                SoftLog (0x88880002, stubprio);
                if (cprio >= stubprio) {
                    // stub is queued at the right priority, do nothing
                    break;
                }
                SoftLog (0x88880003, stubprio);
                // stub is queued at a lower priority, dequeue first
                PrioDequeue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), stubprio);

                // fall through to enqueue the stub
                __fallthrough;
                
            case RUNSTATE_BLOCKED:
                // add the stub to global run queue with the correct priority
                SoftLog (0x88880004, pth->dwId + ppcb->dwCpuId - 1);
                SET_CPRIO (pthStub, pth->pRunQ->pHead->prio);
                SET_RUNSTATE (pthStub, RUNSTATE_RUNNABLE);
                PrioEnqueue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), TRUE);
                pthStub->dwSeqNum = g_seqNumber++;
                break;
            default:
                DEBUGCHK (0);
            }

        }

        if (!ppcb->pthSched || (cprio < GET_CPRIO (ppcb->pthSched))) {
            SetReschedule (ppcb);
        }

        // tell the other CPU that a new thread is runnable. reschedule if needed.
        if (!ppcb->wIPIPending) {
            ppcb->wIPIPending = 1;
        }
    }
}

static void HandleThreadWakeupAtTheSameTime (PTHREAD pth)
{
    PTHREAD pDown = pth->pDownSleep;

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (!GET_SLEEPING (pth));
    DEBUGCHK (!pth->pUpSleep);

    // check if there is a hanging tail of the thread...
    if (pDown) {
        DEBUGCHK (!GET_NEEDSLEEP(pDown) && GET_SLEEPING (pDown));
        DEBUGCHK (!pDown->bSuspendCnt);
        pDown->wCount ++;
        pDown->wCount2 ++;
        pDown->lpProxy = NULL;
        CLEAR_SLEEPING (pDown);
        pDown->pUpSleep = pth->pDownSleep = NULL;

        // don't worry about it if NEEDRUN flag is set
        if (GET_RUNSTATE(pDown) != RUNSTATE_NEEDSRUN) {
            MakeRun (pDown, TRUE);
        } 
    }
}

//------------------------------------------------------------------------------
// remove a thread from Run Queue
//------------------------------------------------------------------------------
static void RunqDequeue (PTHREAD pth, DWORD cprio)
{
    PSTUBTHREAD pthStub = GetStubThread (pth);
    
    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (!IsStubThread (pth));

    HandleThreadWakeupAtTheSameTime (pth);
    PrioDequeue (pth->pRunQ, QNODE_OF_THREAD (pth), cprio);
    
    g_pNKGlobal->dwCntRunnableThreads --;

    SoftLog (0x77770001, pth->dwId + PcbGetCurCpu () - 1);
    
    if (pthStub && (RUNSTATE_RUNNABLE == GET_RUNSTATE (pthStub))) {
        PTWO_D_NODE pHead = pth->pRunQ->pHead;

        DEBUGCHK (&pthStub->ProcReadyQ == pth->pRunQ);
        
        cprio = GET_CPRIO (pthStub);
        SoftLog (0x77770002, cprio);

        if (!pHead || (cprio != pHead->prio)) {
            SoftLog (0x77770003, (DWORD)pHead);
            PrioDequeue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), cprio);

            if (pHead) {
                SoftLog (0x77770004, pHead->prio);
                SET_CPRIO (pthStub, pHead->prio);
                PrioEnqueue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), TRUE);
            } else {
                SET_RUNSTATE (pthStub, RUNSTATE_BLOCKED);
            }
        }
    }
}

FORCEINLINE void SetThreadToExit (PTHREAD pth)
{
    DWORD dwIp = pth->pcstkTop
               ? SYSCALL_RETURN
               : ((pth->pprcOwner == g_pprcNK)? (DWORD) NKExitThread
                                              : TrapAddrExitThread);
    
    DEBUGMSG (ZONE_SCHEDULE, (L"Set Thread %8.8lx to eixt, IP was at %8.8lx\r\n", pth->dwId, THRD_CTX_TO_PC (pth)));

    SetThreadIP(pth, dwIp);
#ifdef ARM
    pth->ctx.Psr &= ~EXEC_STATE_MASK;
#endif
}


//------------------------------------------------------------------------------
// calculate quantum left for the current running thread
//------------------------------------------------------------------------------
static void CalcQuantumLeft (PTHREAD pth, DWORD dwCurrentTime)
{
    PPCB ppcb = GetPCB ();
    long nTickElapsed = dwCurrentTime - ppcb->dwPrevReschedTime;

    DEBUGCHK (OwnSchedulerLock ());
    DEBUGMSG (nTickElapsed < 0, (L"Tick going backward %8.8lx -> %8.8lx\r\n", ppcb->dwPrevReschedTime, dwCurrentTime));

    if (pth->dwQuantum && (nTickElapsed > 0)) {
        pth->dwQuantLeft -= nTickElapsed;
        if ((int) pth->dwQuantLeft <= 0) {
            pth->dwQuantLeft = 0;
            CELOG_KCThreadQuantumExpire();
        }
    }

    nTickElapsed = dwCurrentTime - ppcb->dwPrevTimeModeTime;
    DEBUGMSG (nTickElapsed < 0, (L"Tick going backward %8.8lx -> %8.8lx\r\n", ppcb->dwPrevTimeModeTime, dwCurrentTime));
    if (nTickElapsed > 0) {
        if (GET_TIMEMODE(pth) == TIMEMODE_USER) {
            pth->dwUTime += nTickElapsed;
        } else {
            pth->dwKTime += nTickElapsed;
        }
    }
    ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = dwCurrentTime;
}

//
// Make stub thread runnable if there is a thread in the process runnable.
//
static void MakeRunStubThread (PTHREAD pth)
{
    PSTUBTHREAD pthStub = GetStubThread (pth);

    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (pth == pCurThread);

    if (pthStub) {
        PTWO_D_NODE pHead = pthStub->ProcReadyQ.pHead;
        if (pHead) {
            SoftLog (0x99990001, pHead->prio);
            SET_CPRIO (pthStub, pHead->prio);
            SET_RUNSTATE (pthStub, RUNSTATE_RUNNABLE);
            PrioEnqueue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), TRUE);
            pthStub->dwSeqNum = g_seqNumber++;
        } else {
            SoftLog (0x99990002, pth->dwId + PcbGetCurCpu () - 1);
            SET_RUNSTATE (pthStub, RUNSTATE_BLOCKED);
        }
    }
}

//------------------------------------------------------------------------------
// block the current running thread
//------------------------------------------------------------------------------
static void BlockCurThread (BOOL fPreempt)
{
    PPCB        ppcb    = GetPCB ();
    PTHREAD     pCurTh  = ppcb->pCurThd;
    
    DEBUGCHK (OwnSchedulerLock ());

    CalcQuantumLeft (pCurTh, GETCURRTICK ());

    //DEBUGMSG (ZONE_SCHEDULE, (L"BlockCurThread: pCurTh->dwId = %8.8lx\r\n", pCurTh->dwId));

    SoftLog (0x99990000, pCurTh->dwId + ppcb->dwCpuId - 1);

    MakeRunStubThread (pCurTh);
    
    // save the co-proc register for the thread being swap out.
    SaveCoProcRegisters (pCurTh);
    MDFlushFPU (fPreempt);

    SET_RUNSTATE (pCurTh, RUNSTATE_BLOCKED);
    pCurTh->dwTimeWhenBlocked = CurMSec;
    SetReschedule (ppcb);
    ppcb->pthSched = NULL;
}


//------------------------------------------------------------------------------
// remove a thread from sleep list
//------------------------------------------------------------------------------
static void SleepqDequeue (PTHREAD pth) 
{
    PTHREAD pth2 = pth->pUpSleep, pth3;


    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    DEBUGCHK(pth && GET_SLEEPING(pth));
    pth->wCount2++;

    if (pth2) {
        // in second dimension of sleep queue (thread)
        DEBUGCHK (pth != SleepList);
        DEBUGCHK (!NEXT_IN_SLEEPLIST (pth) && !PREV_IN_SLEEPLIST (pth));
        pth2->pDownSleep = pth3 = pth->pDownSleep;
        if (pth3) {
            pth3->pUpSleep = pth2;
            pth->pDownSleep = 0;
        }
        pth->pUpSleep = 0;
    } else if (NULL != (pth2 = pth->pDownSleep)) {
        // at intersection of 1st and 2nd dimension
        DEBUGCHK (!NEXT_IN_SLEEPLIST (pth2) && !PREV_IN_SLEEPLIST (pth2));

        NEXT_IN_SLEEPLIST (pth2) = pth3 = NEXT_IN_SLEEPLIST (pth);
        if (pth3) {
            PREV_IN_SLEEPLIST (pth3) = pth2;
        }

        PREV_IN_SLEEPLIST (pth2) = pth3 = PREV_IN_SLEEPLIST (pth);
        if (pth3) {
            NEXT_IN_SLEEPLIST (pth3) = pth2;
        } else {
            DEBUGCHK (pth == SleepList);
            SleepList = pth2;
        }
        pth2->pUpSleep = pth->pDownSleep = 0;

    } else {
        // in 1st dimension, with no 2nd dimenstion node below it.
        pth2 = PREV_IN_SLEEPLIST (pth);
        
        if (pth2) {
            // not 1st node
            NEXT_IN_SLEEPLIST (pth2) = pth3 = NEXT_IN_SLEEPLIST (pth);
            if (pth3) {
                PREV_IN_SLEEPLIST (pth3) = pth2;
            }
        } else {
            // 1st node in sleep queue
            DEBUGCHK (pth == SleepList);
            // update SleepList
            SleepList = pth->pNextSleepRun;
            if (SleepList) {
                SleepList->pPrevSleepRun = 0;
            }
        }
    }

    CLEAR_SLEEPING(pth);
}


//------------------------------------------------------------------------------
// simple signaling of thread while thread is in WAITSTATE_PROCESSING
//------------------------------------------------------------------------------
static void SignalThread (PTHREAD pth)
{

    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (pth->bWaitState == WAITSTATE_PROCESSING);

    pth->lpProxy = NULL;
    pth->wCount++;
    pth->bWaitState = WAITSTATE_SIGNALLED;
}


//------------------------------------------------------------------------------
// SleepOneMore
// Returns:
//        0    thread is put to sleep, or no need to sleep
//        1    continue search
//------------------------------------------------------------------------------
#pragma prefast(disable: 11, "pth cannot be NULL when wDirection is non-zero")
static int SleepOneMore (SLEEPSTRUCT *pSleeper) 
{
    PPCB      ppcb = GetPCB ();
    PTHREAD pCurTh = ppcb->pCurThd;

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    if ((int) (GETCURRTICK () - pCurTh->dwWakeupTime) >= 0) {
        // time to wakeup already. Signal the thread if we're waiting with timeout.
        if (pCurTh->lpProxy) {
            SignalThread (pCurTh);
        }

        // as we didn't put the thread in Sleep Queue, we have not yielded to other thread,
        // while the semantic for Sleep should always yield regardless the timeout. So we 
        // manually re-queue ourself into the end of the run queue to yield to other threads.
        BlockCurThread (FALSE);
        MakeRun (pCurTh, TRUE);
        ppcb->wIPIPending = 0;

    } else {

        PTHREAD pth = pSleeper->pth, pth2;
        int nDiff;

        if (pth && (pth->wCount2 != pSleeper->wCount2)) {
            // restart if pth changes its position in the sleepq, or removed from sleepq
            pSleeper->pth = 0;
            pSleeper->wDirection = 0;
            return 1;
        }

        if (pSleeper->wDirection) {
            // veritcal
            DEBUGCHK (pth && GET_SLEEPING(pth));
            DEBUGCHK (pCurTh->dwWakeupTime == pth->dwWakeupTime);
            DEBUGCHK((GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) || (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN));

            if (GET_CPRIO (pCurTh) < GET_CPRIO (pth)) {
                // rare case when we're priority inverted while traversing down, restart
                pSleeper->pth = 0;
                pSleeper->wDirection = 0;
                return 1;
            }
            pth2 = pth->pDownSleep;
            if (pth2 && (GET_CPRIO (pCurTh) >= GET_CPRIO (pth2))) {
                // continue vertical search
                pSleeper->pth = pth2;
                pSleeper->wCount2 = pth2->wCount2;
                return 1;
            }
            // insert between pth and pth2 vertically
            pCurTh->pNextSleepRun = pCurTh->pPrevSleepRun = 0;
            pCurTh->pUpSleep = pth;
            pCurTh->pDownSleep = pth2;
            pth->pDownSleep = pCurTh;
            if (pth2) {
                pth2->pUpSleep = pCurTh;
            }

        } else {
            // horizontal
            pth2 = pth? pth->pNextSleepRun : SleepList;
            pCurTh->pNextSleepRun = pth2;
            if (pth2) {
                nDiff = pth2->dwWakeupTime - pCurTh->dwWakeupTime;
                if (0 > nDiff) {
                    // continue search horizontally
                    pSleeper->pth = pth2;
                    pSleeper->wCount2 = pth2->wCount2;
                    return 1;
                }
                if (0 == nDiff) {
                    if (GET_CPRIO (pCurTh) >= GET_CPRIO (pth2)) {
                        // start vertical search
                        pSleeper->wDirection = 1;
                        pSleeper->pth = pth2;
                        pSleeper->wCount2 = pth2->wCount2;
                        return 1;
                    }
                    // replace pth2 as head of vertical list
                    pth2->wCount2 ++;
                    pCurTh->pDownSleep = pth2;
                    pCurTh->pNextSleepRun = pth2->pNextSleepRun;
                    pth2->pUpSleep = pCurTh;
                    pth2->pNextSleepRun = pth2->pPrevSleepRun = 0;
                    pth2 = pCurTh->pNextSleepRun;
                    if (pth2)
                        pth2->pPrevSleepRun = pCurTh;
                } else {
                    // insert between pth and pth2
                    pth2->pPrevSleepRun = pCurTh;
                }
            }

            // common part
            pCurTh->pPrevSleepRun = pth;
            if (pth) {
                DEBUGCHK((GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) || (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN));
                pth->pNextSleepRun = pCurTh;
            } else {

                // update reschedule time if need to wake up earlier
                if ((int) (dwReschedTime - pCurTh->dwWakeupTime) > 0) {
                    // if the platform support variable tick scheduling, tell OAL to 
                    // update the next interrupt tick
                    dwReschedTime = pCurTh->dwWakeupTime;
                    if (g_pOemGlobal->pfnUpdateReschedTime) {
                        g_pOemGlobal->pfnUpdateReschedTime (dwReschedTime);
                    }
                }
                // update SleepList
                SleepList = pCurTh;

            }
        }

        pSleeper->pth = 0;
        pSleeper->wDirection = 0;
        SET_SLEEPING(pCurTh);
        pCurTh->bWaitState = WAITSTATE_BLOCKED;
        
        BlockCurThread (FALSE);

    }
    return 0;

}
#pragma prefast(pop)

//------------------------------------------------------------------------------
// DoLinkCritMut - Worker function to add a CS/Mutex to the 'owned object list' a thread
//------------------------------------------------------------------------------
static BOOL DoLinkCritMut (PMUTEX pMutex, PTHREAD pth) 
{
    PTWO_D_NODE pHead = pMutex->proxyqueue.pHead;
    
    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    pMutex->bListedPrio = pHead? pHead->prio : (MAX_CE_PRIORITY_LEVELS - 1);
    pMutex->bListed     = CRIT_STATE_OWNED;

    return PrioEnqueue (&pth->ownedlist, QNODE_OF_MUTEX (pMutex), TRUE);
}


//------------------------------------------------------------------------------
// UnlinkCritMut - remove a CS/Mutex from the 'owned object list' a thread
//------------------------------------------------------------------------------
static void UnlinkCritMut (PMUTEX pMutex, PTHREAD pth) 
{ 
    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    if (CRIT_STATE_OWNED == pMutex->bListed) {
        
        pMutex->bListed = CRIT_STATE_NOT_OWNED;
        PrioDequeue (&pth->ownedlist, QNODE_OF_MUTEX (pMutex), pMutex->bListedPrio);
    }
}

//------------------------------------------------------------------------------
// PreReleaseMutex - step 1 of releasing the ownership of a mutex/cs,
//                   Remove the CS from the thread's "owned object list".
//------------------------------------------------------------------------------

static BOOL PreReleaseMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs) 
{
    PTHREAD pCurTh = GetPCB ()->pCurThd;
    DWORD   dwThId = pCurTh->dwId;
    BOOL    fRet   = (pCurTh == pMutex->pOwner);

    DEBUGCHK (OwnSchedulerLock ());

    if (lpcs) {

        if (fRet) {
            // pMutex->owner is always correct. We own the CS in this case. Simply update Owner to id+2
            lpcs->OwnerThread = (HANDLE) (dwThId + 2);

        } else {
            // pMutex->owner is not current thread, only success if it's null, and ownerthread is the id of current thread
            DWORD dwCsOwner = (DWORD) lpcs->OwnerThread;
            
            fRet = !pMutex->pOwner
                && ((dwCsOwner & ~1) == dwThId)
                && (InterlockedCompareExchange ((PLONG)&lpcs->OwnerThread, dwThId+2, dwCsOwner) == (LONG) dwCsOwner);

            if (fRet) {
                pMutex->pOwner = pCurTh;
            }
        } 
    }

    if (fRet) {
        SET_NOPRIOCALC (pCurTh);
        UnlinkCritMut (pMutex, pCurTh);
        pMutex->bListed = CRIT_STATE_LEAVING;
    }

    return fRet;
}

static void RecalcCPrio (PTHREAD pth, BOOL fLogInvert)
{
    PPCB   ppcb      = GetPCB ();
    DWORD  dwPrio    = GET_BPRIO (pth);
    DWORD  dwOldPrio = GET_CPRIO (pth);
    PMUTEX pMutex    = MUTEX_OF_QNODE (pth->ownedlist.pHead);

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());

    if (pMutex && (pMutex->bListedPrio < dwPrio)) {
        dwPrio = pMutex->bListedPrio;
    }
    SET_CPRIO (pth, dwPrio);
    if (dwPrio != dwOldPrio) {
        if (fLogInvert) {
            CELOG_KCThreadPriorityInvert (pth);
        }
        if (PriorityLowerThanNode (RunList.pHead, dwPrio)
            || PriorityLowerThanNode (ppcb->affinityQ.pHead, dwPrio)
            || PriorityLowerThanNode (pth->pRunQ->pHead, dwPrio)) {
            SetReschedule (ppcb);
        }
    }
}

static PTHREAD SignalThreadWithProxy (PPROXY pprox)
{
    PTHREAD pth = pprox->pTh;

    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (pth);
    DEBUGCHK (pth->wCount == pprox->wCount);

    // wakeup the thread
    pth->lpProxy = NULL;
    pth->wCount ++;
    pth->dwWaitResult = pprox->dwRetVal;
    
    if (pth->bWaitState == WAITSTATE_BLOCKED) {
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
        SET_RUNSTATE (pth, RUNSTATE_NEEDSRUN);
    } else {

        DEBUGCHK(!GET_SLEEPING(pth));
        DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
        pth->bWaitState = WAITSTATE_SIGNALLED;
        pth = NULL;             // thread not blocked, no need to "MakeRun" this thread
    }
    return pth;
}

//------------------------------------------------------------------------------
// WakeupIST - wake up the thread blocked on an interrupt event
//------------------------------------------------------------------------------
static PTHREAD WakeupIST (PEVENT pEvt) 
{
    PTHREAD pth;
    PPROXY  pprox = PROXY_OF_QNODE (pEvt->proxyqueue.pHead);

    // must own scheduler spinlock
    DEBUGCHK (OwnSchedulerLock ());
    DEBUGCHK (pprox);
    DEBUGCHK (pprox->pObject == (LPBYTE)pEvt);
    DEBUGCHK (pprox->bType   == HT_MANUALEVENT);
    DEBUGCHK (pprox->pQDown  == pprox);
    DEBUGCHK (pprox->pQPrev  == (PPROXY)&pEvt->proxyqueue.pHead);
    DEBUGCHK (!pEvt->state);
    DEBUGCHK (!pEvt->manualreset);
    DEBUGCHK (pEvt->phdIntr);

    // Dequeue proxy. Since there can only be 1 thread waiting, simply set 
    // pEvt->proxyqueue.pHead and set pprox->pQDown to NULL
    pprox->pQDown = NULL;
    pEvt->proxyqueue.pHead = NULL;
    
    pth = pprox->pTh;
    if (pth->wCount != pprox->wCount) {
        // The thread had already been signaled (timeout or other object signaled),
        // and in the middle of return from WaitForXXXObjects. As there cannot be another
        // thread waiting on this event, just set the event and done. The thread will
        // clean up its proxy list before returning from Wait function.
        pEvt->state = 1;
        pth        = NULL;
        
    } else {

        // signal the waiting thread
        pth = SignalThreadWithProxy (pprox);
    }

    return pth;
}


//------------------------------------------------------------------------------
// link proxy to mutex
//------------------------------------------------------------------------------
static void QueueProxyToMutex (PMUTEX pMutex, PPROXY pProx, PMUTEX *ppCritMut)
{
    DEBUGCHK (OwnSchedulerLock ());
    
    if (PrioEnqueue (&pMutex->proxyqueue, QNODE_OF_PROXY (pProx), TRUE)) {
        PTHREAD pOwner = pMutex->pOwner;
        
        // mutex priority changed
        switch (pMutex->bListed) {
        case CRIT_STATE_OWNED:
            // remove from the owner's owned list
            PrioDequeue (&pOwner->ownedlist, QNODE_OF_MUTEX (pMutex), pMutex->bListedPrio);
        
            // fall through
            __fallthrough;
            
        case CRIT_STATE_NOT_OWNED:
            // add to owner's owned list.
            DoLinkCritMut (pMutex, pOwner);
            break;
        default:
            // owner calling LeaveCriticalSection, do nothing
            break;
        }
    
        *ppCritMut = pMutex;
    }
}


//------------------------------------------------------------------------------
// check if a sync object is signaled. preform necessary operation if the object is not signaled.
//------------------------------------------------------------------------------
static DWORD CheckOneSyncObject (PPROXY pProx, PWAITSTRUCT pws)
{
    PTHREAD pCurTh          = GetPCB ()->pCurThd;
    DWORD   dwSignaleState  = SIGNAL_STATE_SIGNALED;
    LPVOID  pSyncObj        = pProx->pObject;
    BYTE    prio;

    DEBUGCHK (OwnSchedulerLock ());

    pCurTh->dwWaitResult = pProx->dwRetVal;
    
    if (pSyncObj) {
        switch (pProx->bType) {
            case SH_CURTHREAD: {
                PTHREAD pthWait = (PTHREAD)pSyncObj;

                if (!pthWait->ftExit.dwLowDateTime) {
                    // thread still alive, not signaled
                    if (pws->fEnqueue) {
                        FlatEnqueue (&pthWait->pBlockers, QNODE_OF_PROXY (pProx));
                        dwSignaleState = SIGNAL_STATE_NOT_SIGNALED;
                    } else {
                        pCurTh->dwWaitResult = WAIT_TIMEOUT;
                    }
                }
                break;
            }

            case HT_EVENT: {
                PEVENT pEvt = (PEVENT)pSyncObj;

                if (pEvt->state) {    // event is already signalled
                    DEBUGCHK(!pEvt->phdIntr || !pEvt->manualreset);
                    pEvt->state = pEvt->manualreset;
                    
                } else if (pws->fEnqueue) {

                    prio = pProx->prio;

                    dwSignaleState = SIGNAL_STATE_NOT_SIGNALED;

                    if (pEvt->phdIntr) {
                        // interrupt event
                        if (pEvt->proxyqueue.pHead) {
                            DEBUGCHK (0);
                            dwSignaleState = SIGNAL_STATE_BAD_WAIT;
                        } else {
                            // fast enqueue for interrupt event (only 1 thread waiting on it)
                            pProx->bType = HT_MANUALEVENT;
                            pProx->pQUp = pProx->pQDown = pProx;
                            pProx->pQPrev = (PPROXY) &pEvt->proxyqueue.pHead;
                            pEvt->proxyqueue.pHead = QNODE_OF_PROXY (pProx);
                        }
                        
                    } else if (pEvt->manualreset) {
                        // manual reset event
                        pProx->bType = HT_MANUALEVENT;
                        FlatEnqueue (&pEvt->proxyqueue.pHead, QNODE_OF_PROXY (pProx));
                        if (prio < pEvt->bMaxPrio)
                            pEvt->bMaxPrio = prio;
                        
                    } else {
                        // auto-reset event
                        PrioEnqueue (&pEvt->proxyqueue, QNODE_OF_PROXY (pProx), TRUE);
                    }
                } else {
                    pCurTh->dwWaitResult = WAIT_TIMEOUT;
                }
                break;
            }
            case HT_SEMAPHORE: {
                PSEMAPHORE psem = (PSEMAPHORE)pSyncObj;

                if (psem->lCount) {
                    // count remaining, signaled
                    psem->lCount--;
                } else if (pws->fEnqueue) {
                    PrioEnqueue (&psem->proxyqueue, QNODE_OF_PROXY(pProx), TRUE);
                    dwSignaleState = SIGNAL_STATE_NOT_SIGNALED;
                } else {
                    pCurTh->dwWaitResult = WAIT_TIMEOUT;
                }
                break;
            }
            case HT_MUTEX: {
                PMUTEX pMutex  = (PMUTEX) pSyncObj;
                PTHREAD pOwner = pMutex->pOwner;

                if (!pOwner || GET_BURIED(pOwner)) {
                    // no owner, grab the mutex (signaled)
                    pMutex->LockCount = 1;
                    pMutex->pOwner    = pCurTh;
                    
                } else if (pOwner == pCurTh) {
                    // already own the mutex, signaled
                    DEBUGCHK (!pMutex->proxyqueue.pHead || (CRIT_STATE_NOT_OWNED != pMutex->bListed));
                    if (pMutex->LockCount == MUTEX_MAXLOCKCNT) {
                        dwSignaleState = SIGNAL_STATE_BAD_WAIT;
                    } else {
                        pMutex->LockCount++;
                    }
                        
                } else if (pws->fEnqueue) {
                    // queue us up to block on the mutex
                    QueueProxyToMutex (pMutex, pProx, &pws->pMutex);
                    dwSignaleState = SIGNAL_STATE_NOT_SIGNALED;

                } else {
                    pCurTh->dwWaitResult = WAIT_TIMEOUT;
                }
                break;
            }
            default:
                dwSignaleState = SIGNAL_STATE_BAD_WAIT;
        }

    // pSyncObj == NULL: singled if it's a thread, bad otherwise
    } else if (SH_CURTHREAD != pProx->bType) {
        dwSignaleState = SIGNAL_STATE_BAD_WAIT;
    }


    return dwSignaleState;
    
}


//------------------------------------------------------------------------------
// in case of OOM, find a runnable OOM handler thread
//------------------------------------------------------------------------------
static PTHREAD FindOOMHandlerThread (PPCB ppcb)
{
    PTHREAD pOwner = pOOMThread;

    SoftLog (0xaaaa0000, (DWORD) pOwner);
    while (!PendEvents1(ppcb) && !PendEvents2(ppcb)) {
        switch (GET_RUNSTATE (pOwner)) {
        case RUNSTATE_NEEDSRUN:
            if (pOwner->bSuspendCnt) {
                return NULL;
            }
            if (GET_SLEEPING (pOwner)) {
                SleepqDequeue(pOwner);
                pOwner->wCount ++;
            }
            MakeRun (pOwner, !pOwner->dwQuantLeft);
            DEBUGCHK (GET_RUNSTATE (pOwner) == RUNSTATE_RUNNABLE);
            // fall thru
            __fallthrough;
            
        case RUNSTATE_RUNNABLE: {

            PSTUBTHREAD pthStub = GetStubThread (pOwner);
            
            DEBUGCHK (OwnSchedulerLock ());
            DEBUGCHK (!IsStubThread (pOwner));
            
            HandleThreadWakeupAtTheSameTime (pOwner);

            PrioDequeue (pOwner->pRunQ, QNODE_OF_THREAD (pOwner), GET_CPRIO (pOwner));
            SET_RUNSTATE (pOwner, RUNSTATE_RUNNING);
            SET_CPRIO (pOwner, currmaxprio);
            
            if (pthStub) {
                SoftLog (0xaaaa0001, currmaxprio);
                PrioDequeue (pthStub->pRunQ, QNODE_OF_THREAD (pthStub), GET_CPRIO (pthStub));
                SET_RUNSTATE (pthStub, RUNSTATE_RUNNING);
                SET_CPRIO (pthStub, currmaxprio);

            }
            
            return pOwner;
        }

        case RUNSTATE_BLOCKED:
            if (   !GET_SLEEPING(pOwner)
                && pOwner->lpProxy
                && (pOwner->lpProxy->bType == HT_CRITSEC)) {
                PMUTEX pMutex = (PMUTEX) pOwner->lpProxy->pObject;
                DEBUGCHK (pOwner->lpProxy->pQDown);
                DEBUGCHK (pMutex && pMutex->pOwner);
                pOwner = pMutex->pOwner;
                break;
            }
            return NULL;

        default:
            // OOM handler is already running (can only occur on multi-core), do nothing
            return NULL;
        }
    }
    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
//
// KCalls exposed to external
//
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// NextThread - part I of scheduler. Set interrupt event on interrupt, 
//              and make sleeping thread runnable when sleep time expired.
//------------------------------------------------------------------------------
void NextThread (void) 
{
    PPCB    ppcb    = GetPCB ();
    PTHREAD pCurTh  = ppcb->pCurThd;
    PTHREAD pth;
    PEVENT  lpe;
    LONG    pend1, pend2, intr;

    DEBUGCHK (OwnSchedulerLock ());
    KCALLPROFON(10);
    /* Handle interrupts */
    do {

        pend1 = InterlockedExchange ((PLONG)&PendEvents1(ppcb), 0);
        pend2 = InterlockedExchange ((PLONG)&PendEvents2(ppcb), 0);

        while (pend1 || pend2) {
            if (pend1) {
                intr = GetHighPos(pend1);
                pend1 &= ~(1<<intr);
            } else {
                intr = GetHighPos(pend2);
                pend2 &= ~(1<<intr);
                intr += 32;
            }
            lpe = IntrEvents[intr];
            if (lpe) {
                if (!lpe->proxyqueue.pHead) {
                    // no one waiting, set the event
                    lpe->state = 1;

                } else {
                    pth = WakeupIST (lpe);
                    if (pth) {
                        // need to wakeup the IST
                        if (GET_SLEEPING(pth)) {
                            SleepqDequeue(pth);
                        }
                        MakeRun (pth, !pth->dwQuantLeft);
                    }
                }
            }
        }
    } while (PendEvents1(ppcb) || PendEvents2(ppcb));

///////////////////////////////////////////////////////////
// We can not return here or the real-time SleepQueue
// will not work as expected. i.e. if timer tick and 
// other interrupts come at the same time, we'll lose 
// the timer tick and the thread at the beginning of
// the SleepList will not be waked up until the next
// tick.

    /* Handle timeouts */
    if ((int) (CurMSec - dwReschedTime) >= 0) {
        SetReschedule (ppcb);
        ppcb->wIPIPending = 1;
    }
    pth = SleepList;
    if (pth && ((int) (CurMSec - pth->dwWakeupTime) >= 0)) {

        DEBUGCHK((int) (CurMSec - dwReschedTime) >= 0);
        DEBUGCHK(GET_SLEEPING(pth));
        // We only update CurMSec on timer interrupt or in/out of Idle.
        // Sinc the timer ISR should return SYSINTR_RESCHED in this case, and we 
        // always reschedule after Idle, there is no need to update dwReschedTime
        // It'll be updated in KCNextThread
        CLEAR_SLEEPING(pth);
        pth->wCount++;
        pth->wCount2 ++;

        SleepList = pth->pNextSleepRun;
        if (SleepList) {
            SleepList->pPrevSleepRun = 0;
        }
        if (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN) {
            DEBUGCHK(GET_RUNSTATE(pth) == RUNSTATE_BLOCKED);
            pth->lpProxy = 0;
            MakeRun (pth, !pth->dwQuantLeft);
        } 
    }

    if (!GetReschedule (ppcb) && pCurTh && (&dummyThread != pCurTh)) {
        g_pOemGlobal->pfnNotifyReschedule (pCurTh->dwId, GET_CPRIO(pCurTh), pCurTh->dwQuantum, 0);
    }

    KCALLPROFOFF (10);
}

// check if we're being suspended/terminated asynchronously
static PTHREAD HandleIPIOperation (PPCB ppcb)
{
    DWORD dwIPIOperation = ppcb->wIPIOperation;
    PTHREAD pth = ppcb->pthSched;

    if (pth && dwIPIOperation) {
        ppcb->wIPIOperation = 0;

        DEBUGCHK (pth == ppcb->pCurThd);

        if (!IsThreadInPSLCall (pth)) {
            switch (dwIPIOperation) {
            case IPI_TERMINATE_THREAD:
                SetThreadToExit (pth);
                break;

            case IPI_SUSPEND_THREAD:
                if (pth->bPendSusp) {
                    pth->bSuspendCnt = pth->bPendSusp;
                    pth->bPendSusp   = 0;
                    BlockCurThread (TRUE);
                    pth = NULL;
                }
                break;

            default:
                DEBUGCHK (0);
            }
        }
    }

    return pth;
}

DWORD dwLastKCTime;


//------------------------------------------------------------------------------
// KCNextThread - part 2 of scheduler, find the highest prioity runnable thread
//
// NOTE: Reference to pCurThread is NOT allowed in this function, for pCurThread
//       can already be in run queue (yield, or blocked and signaled before we get
//       here). If the other CPUs picked up the thread and start running it, we're
//       in trouble.
//
//------------------------------------------------------------------------------
void KCNextThread (void) 
{
    PPCB        ppcb = GetPCB ();
    PTHREAD     pth  = HandleIPIOperation (ppcb);
    DWORD       dwCurrentTime, NewReschedTime, pcbix;
    PSTUBTHREAD pthStub;
    PTWO_D_NODE pRunnable = NULL;
    BOOL        fCpuOn = (CE_PROCESSOR_STATE_POWERED_ON == ppcb->dwCpuState);

    DEBUGCHK (OwnSchedulerLock ());

    KCALLPROFON (44);

    dwCurrentTime = GETCURRTICK ();

    SoftLog (0x55550000, (pth? pth->dwId : 0) + ppcb->dwCpuId - 1);
    if (pth) {

        // current running thread is preempted
        DWORD prio = GET_CPRIO (pth);

        CalcQuantumLeft (pth, dwCurrentTime);
        if (!pth->dwQuantLeft && pth->dwQuantum) {
            prio ++;       // add 1 to prio if quantum expired, to simplify priority comparison
        }

        if (fCpuOn) {

            pthStub = GetStubThread (pth);

            SoftLog (0x55550001, THRD_CTX_TO_PC(pth));

            // if the process is running in BC mode, check the per-proc
            // run queue to see if there is a thread of higher prio runnable.
            if (pthStub && PriorityLowerThanNode (pthStub->ProcReadyQ.pHead, prio)) {
                // DEBUGMSG (ZONE_SCHEDULE, (L"KCNextThread (thread in proc queue higher prio): pHead->prio = %d\r\n", pthStub->ProcReadyQ.pHead->prio));
                SoftLog (0x55550002, prio);
                PutRunningThreadToRunQ (pth);
                pth = NULL;
            }

            pRunnable = FirstRunnableNode (ppcb);

            if (pth && PriorityLowerThanNode (pRunnable, prio)) {
                SoftLog (0x55550003, pRunnable->prio);
                PutRunningThreadToRunQ (pth);
                pth = NULL;
            }
        } else {
            // CPU being powered off
            SoftLog (0x55551001, ppcb->dwCpuId);
            PutRunningThreadToRunQ (pth);
            pth = NULL;
        }

    } else if (fCpuOn) {
        // current running thread yield/block
        pRunnable = FirstRunnableNode (ppcb);
        ppcb->dwPrevTimeModeTime = ppcb->dwPrevReschedTime = dwCurrentTime;
    }

    if (!pth) {
        
        if (pRunnable && fCpuOn) {

            // context switch occurs

            pth = THREAD_OF_QNODE (pRunnable);
            
            // check "max runable priority" (set by OOM)
            if ((pRunnable->prio > currmaxprio) && pOOMThread) {

                pth = FindOOMHandlerThread (ppcb);
                // no need to recalculate priority if we get the thread from OOM
                // handling. FindOOMHandlerThread had make the new runnable thread running

            } else {

                // make the new thread the running thread
                VERIFY (PrioDequeueHead (pth->pRunQ) == pRunnable);
                SET_RUNSTATE (pth, RUNSTATE_RUNNING);

                if (IsStubThread (pth)) {
                    pthStub = (PSTUBTHREAD) pth;
                    pRunnable = PrioDequeueHead (&pthStub->ProcReadyQ);

                    DEBUGCHK (pRunnable);

                    pth = THREAD_OF_QNODE (pRunnable);
                    SET_RUNSTATE (pth, RUNSTATE_RUNNING);

                    SoftLog (0x55550004, pth->dwId + ppcb->dwCpuId - 1);

                    // DEBUGMSG (ZONE_SCHEDULE, (L"KCNextThread (make stub running) pth->dwId = %8.8lx\r\n", pth->dwId));
                }

                HandleThreadWakeupAtTheSameTime (pth);

                // reculate priority based on sync object owned.
                if (!GET_NOPRIOCALC(pth)) {
                    
                    // recalculate priority based on sync object pth owns
                    RecalcCPrio (pth, TRUE);

                }
            }

            if(pth) {
                g_pNKGlobal->dwCntRunnableThreads --;
            }
        }
        
    } else if ((ppcb->dwCpuId==1) && (g_pthMonitor)) {                 
    
        // monitor thread registered - runs only on main core
        if (pth == g_pthMonitor) {
            // monitor thread scheduled, reset the counters
            g_pthMonitor->dwTimeWhenRunnable = dwCurrentTime;
            g_dwTimeInDebugger = 0;
            
        } else if (GET_RUNSTATE (g_pthMonitor) == RUNSTATE_RUNNABLE) {

            // monitor thread is runnable, calculate how long had the thread being runnable
            LONG lTimeElapsed = dwCurrentTime - g_pthMonitor->dwTimeWhenRunnable - g_dwTimeInDebugger;

            if (lTimeElapsed > g_lMaxRunnableDelay) {

                // monitor thread hasn't run for a long time. i.e.
                // High prio thread spinning, run the monitor thread instead.
                if (g_pdwSpinnerThreadId) {
                    *g_pdwSpinnerThreadId = pth->dwId;
                }
                g_pthMonitor->dwTimeWhenRunnable = dwCurrentTime;   // update time when runnable
                g_pthMonitor->dwQuantLeft        = 0;               // monitor thread got full quantum
                g_dwTimeInDebugger = 0;

                RunqDequeue (g_pthMonitor, GET_CPRIO (g_pthMonitor));
                if (pth == ppcb->pthSched) {
                    // the same thread got rescheduled
                    PutRunningThreadToRunQ (pth);
                } else {
                    // new runnable thread rescheduled
                    pth->dwQuantLeft ++; // Make run will decrement the quantum by 1                    
                    MakeRun (pth, FALSE);
                }
                pth = g_pthMonitor;
                SET_CPRIO (pth, 0);
            }
        }
    }
    
    ppcb->pthSched = pth;
    
    DEBUGCHK (!pth || !IsStubThread (pth));

    randdw2 = ((randdw2<<5) | (randdw2>>27)) ^ ((dwCurrentTime>>5) & 0x1f);

    // update perf counter high in case tick count wrapped around
    if (dwLastKCTime > dwCurrentTime) {
        g_PerfCounterHigh++;
    }
    dwLastKCTime = dwCurrentTime;

    // update reschedule time
    NewReschedTime = SleepList? SleepList->dwWakeupTime : (dwCurrentTime + 0x7fffffff);

    if (pth) {
        if (!pth->dwQuantLeft) {
            pth->dwQuantLeft = pth->dwQuantum;
        }
        // update NewReschedTime
        if (pth->dwQuantum && ((int) ((dwCurrentTime += pth->dwQuantLeft) - NewReschedTime) < 0)) {
            NewReschedTime = dwCurrentTime;
        }

        CELOG_KCThreadSwitch(pth);
        g_pOemGlobal->pfnNotifyReschedule (pth->dwId, GET_CPRIO(pth), pth->dwQuantum, 0);

    } else {
        CELOG_KCThreadSwitch(NULL);
        g_pOemGlobal->pfnNotifyReschedule (0, 0, 0, 0);
    }

    pcbix = g_pKData->nCpus;
    // compare NewReschedTime against other cores
    do {
        pcbix--;

        // skip current PCB or pthSched is absent
        if ((g_ppcbs[pcbix] != ppcb) && g_ppcbs[pcbix]->pthSched) {
            DWORD CpuNReschedTime = g_ppcbs[pcbix]->pthSched->dwQuantLeft + g_ppcbs[pcbix]->dwPrevReschedTime;

            // find minimal ReschedTime from all cores.
            // NOTE: we must Ignore the reshedule time before CurMSec, otherwise, we can get into IPI storm due to master core not idling:
            //      1. Master core got to the reschedule code, and the new thread runnable has affinity set to other core.
            //      2. Master core have no thread to run because the new thread cannot run on master core.
            //      3. We calculate the new "reschedule time" before getting into idle, which will be updated after the other cores reschedule.
            //         i.e. the "reschedule time" calculated has already passed unless the other cores finish reschedule.
            //      4. Master core send IPI to other cores to ask them to reschedule.
            //      5. Master core get into idle (call OEMIdleEx), but because the reschedule time has already passed, idle returns right away.
            //      6. Repeat from (1).
            // Since quantum expiration before current time will be taken care of after we send the reshedule IPI to the other cores when we
            // released the scheduler spinlock, we can safely exclude it from reschedule time calculation.
            if (((int) (CpuNReschedTime - CurMSec) > 0)
                && ((int) (NewReschedTime - CpuNReschedTime) > 0)) {
                NewReschedTime = CpuNReschedTime;
            }
        }
    } while (pcbix > 0);

    // if the platform support variable tick scheduling, tell OAL to 
    // update the next interrupt tick
    if (dwReschedTime != NewReschedTime) {
        dwReschedTime = NewReschedTime;
        if (g_pOemGlobal->pfnUpdateReschedTime) {
            g_pOemGlobal->pfnUpdateReschedTime (NewReschedTime);
        }
    }

#ifdef SH4
    if (pth) {
        if (ppcb->pCurFPUOwner != pth)
            pth->ctx.Psr |= SR_FPU_DISABLED;
        else
            pth->ctx.Psr &= ~SR_FPU_DISABLED;
    }
#endif

    if (PendEvents1(ppcb) || PendEvents2(ppcb)) {
        SetReschedule (ppcb);
        ppcb->bResched = 1;
    }
    SoftLog (0x5555ffff, (pth? pth->dwId : 0) + ppcb->dwCpuId - 1);

    // DEBUGMSG (ZONE_SCHEDULE, (L"KCNextThread pth = %8.8lx\r\n", pth));
    KCALLPROFOFF (44);
}

//------------------------------------------------------------------------------
// SCHL_BlockCurThread: block current running thread and reschedule.
//------------------------------------------------------------------------------
void SCHL_BlockCurThread (void)
{
    AcquireSchedulerLock (0);
    BlockCurThread (FALSE);
    ReleaseSchedulerLock (0);
}

//------------------------------------------------------------------------------
// SCHL_RegisterMonitorThread: register current thread as monitor thread.
//------------------------------------------------------------------------------
void SCHL_RegisterMonitorThread (LPDWORD pdwSpinnerThreadId, DWORD lMaxDelay)
{
    AcquireSchedulerLock (0);
    g_lMaxRunnableDelay  = lMaxDelay;
    g_pdwSpinnerThreadId = pdwSpinnerThreadId;
    g_dwTimeInDebugger   = 0;
    g_pthMonitor         = pCurThread;
    ReleaseSchedulerLock (0);

    /* in multicore case monitor should be locked to master CPU */
    if (g_pKData->nCpus > 1)    
        SCHL_SetThreadAffinity(g_pthMonitor, 1);
}

#define SUSPEND_THREAD_ACCESS_DENIED    0xfffffffe
#define SUSPEND_THREAD_INVALID_ARG      0xffffffff

//------------------------------------------------------------------------------
// SCHL_ThreadSuspend: suspend a thread. We'll mark the thread
//                structure and the thread would suspend itself when exiting PSL.
//------------------------------------------------------------------------------
DWORD SCHL_ThreadSuspend (PTHREAD pth) 
{
    DWORD retval = 0;

    AcquireSchedulerLock (5);

    // validate parameter 
    if (   !KC_IsValidThread (pth)
        || (pth->pprcOwner == g_pprcNK)
        || (GET_DYING(pth) && !GET_DEAD(pth))) {
        // fail - invalid thread, kernel thread, or thread is dying
        retval = SUSPEND_THREAD_ACCESS_DENIED;

    // increment suspend count if already suspended
    } else if (pth->bSuspendCnt) {
        DEBUGCHK (RUNSTATE_BLOCKED == GET_RUNSTATE (pth));

        retval = (MAX_SUSPEND_COUNT > pth->bSuspendCnt)? pth->bSuspendCnt ++ : SUSPEND_THREAD_INVALID_ARG;
        
    // increment pend-suspend count if already pend-suspended
    } else if (pth->bPendSusp) {
    
        retval = (MAX_SUSPEND_COUNT > pth->bPendSusp)? pth->bPendSusp ++ : SUSPEND_THREAD_INVALID_ARG;
        
    // thread not suspended, suspend/pend-suspend the thread
    } else {

        PPCB ppcb = GetPCB ();

        switch (GET_RUNSTATE (pth)) {
        case RUNSTATE_RUNNING:
            if (ppcb->pthSched == pth) {
                // current running thread, suspend on the spot
                pth->bSuspendCnt = 1;
                BlockCurThread (FALSE);
            } else {
                // thread running on another CPU, send IPI
                pth->bPendSusp = 1;
                SendIPI (IPI_SUSPEND_THREAD, (DWORD)pth);
            }
            break;
        case RUNSTATE_RUNNABLE:
            // runnable thread
            if (IsThreadInPSLCall (pth)) {
                // in kernel or PSL, late suspend
                pth->bPendSusp = 1;
            } else {
                // in owner process, suspend on the spot
                pth->bSuspendCnt = 1;
                RunqDequeue (pth, GET_CPRIO(pth));
                SET_RUNSTATE (pth,RUNSTATE_BLOCKED);
            }
            break;

        case RUNSTATE_NEEDSRUN:
        case RUNSTATE_BLOCKED:
            // blocked/need-run, pend suspend
            pth->bPendSusp = 1;
            break;

        default:
            DEBUGCHK (0);
            break;
        }
    }
    
    if ((int)retval >= 0) {
        CELOG_KCThreadSuspend(pth);
    }
    
    ReleaseSchedulerLock (5);

    return retval;
}


void SCHL_SuspendSelfIfNeeded (void)
{
    PPCB      ppcb;
    PTHREAD pCurTh;

    AcquireSchedulerLock (64);

    ppcb = GetPCB ();
    pCurTh = ppcb->pCurThd;
    DEBUGCHK (!pCurTh->bSuspendCnt);

    if (pCurTh->bPendSusp) {
        pCurTh->bSuspendCnt = pCurTh->bPendSusp;
        pCurTh->bPendSusp = 0;
        SET_USERBLOCK (pCurTh);     // okay to terminate the thread
        BlockCurThread (FALSE);
    }
    
    ReleaseSchedulerLock (64);
}



//------------------------------------------------------------------------------
// resume a thread
//------------------------------------------------------------------------------
DWORD SCHL_ThreadResume (PTHREAD pth) 
{
    DWORD retval = 0;
    AcquireSchedulerLock (47);

    if (KC_IsValidThread (pth)) {
        retval = pth->bSuspendCnt;
        if (retval) {
            if (!--pth->bSuspendCnt && (GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) && (!pth->lpProxy || GET_NEEDSLEEP(pth))) {
                DEBUGCHK (!GET_SLEEPING(pth));
                MakeRun (pth, TRUE);
            }
        } else {
            retval = pth->bPendSusp;
            if (retval) {
                // if the suspend is pending, just decrement the count and return
                pth->bPendSusp --;
            }
        }
        if (retval) {
            CELOG_KCThreadResume(pth);
        }
    }

    ReleaseSchedulerLock (47);
    return retval;
}



//------------------------------------------------------------------------------
// current thread yield
//------------------------------------------------------------------------------
void SCHL_ThreadYield (void) 
{
    PPCB    ppcb;
    PTHREAD pCurTh;

    AcquireSchedulerLock (35);
    ppcb   = GetPCB ();
    pCurTh = ppcb->pCurThd;

    DEBUGCHK (GET_RUNSTATE(pCurTh) == RUNSTATE_RUNNING);

    BlockCurThread (FALSE);
    MakeRun (pCurTh, TRUE);
    ppcb->wIPIPending = 0;

    ReleaseSchedulerLock (35);
    
}

//------------------------------------------------------------------------------
// change base priority of a thread
//------------------------------------------------------------------------------
BOOL SCHL_SetThreadBasePrio (PTHREAD pth, DWORD nPriority)
{
    PTHREAD pCurTh;
    BOOL bRet;
    DWORD oldb, oldc;
    bRet = FALSE;

    AcquireSchedulerLock (28);

    pCurTh = GetPCB ()->pCurThd;

    if (KC_IsValidThread (pth)) {
        oldb = GET_BPRIO(pth);
        if (oldb != nPriority) {
            oldc = GET_CPRIO(pth);
            SET_BPRIO(pth,(WORD)nPriority);

            CELOG_KCThreadSetPriority(pth, nPriority);

            // update process priority watermark
            if (nPriority < pth->pprcOwner->bPrio)
                pth->pprcOwner->bPrio = (BYTE) nPriority;
                
            if (pCurTh == pth) {
                // calculate CPrio
                RecalcCPrio (pth, FALSE);

            } else if (nPriority < oldc) {
                SET_CPRIO (pth,nPriority);
                if (GET_RUNSTATE(pth) == RUNSTATE_RUNNABLE) {
                    RunqDequeue (pth, oldc);
                    MakeRun (pth, TRUE);
                } else if (GET_SLEEPING (pth) && pth->pUpSleep && (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN)) {
                    SleepqDequeue (pth);
                    SET_NEEDSLEEP (pth);
                    if (pth->lpProxy) {
                        pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
                    }
                    MakeRun (pth, TRUE);
                }
            }
        }
        bRet = TRUE;
    }

    ReleaseSchedulerLock (28);
    return bRet;
}

//------------------------------------------------------------------------------
// terminate a thread
//------------------------------------------------------------------------------
BOOL SCHL_SetThreadToDie (PTHREAD pth, DWORD dwExitCode, sttd_t *psttd) 
{
    BOOL fRet;

    AcquireSchedulerLock (17);


    // terminating a kernel thread is not allowed.
    fRet = KC_IsValidThread (pth) && (pth->pprcOwner != g_pprcNK);

    if (fRet) {

        if (!GET_DYING(pth) && !GET_DEAD(pth)) {

            PTHREAD pCurTh  = pCurThread;

            SET_DYING(pth);
            pth->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_TRYINGTODIE;
            pth->dwExitCode = dwExitCode;
            pth->bPendSusp = 0;     // clear pending suspend bit, no longer need to suspend since the thread is dying

            if (pth != pCurTh) {

                if (GET_CPRIO (pCurTh) < GET_BPRIO (pth)) {
                    psttd->pThNeedBoost = pth;
                }

                switch (GET_RUNSTATE(pth)) {
                case RUNSTATE_RUNNABLE:
    
                    if (!IsThreadInPSLCall (pth)) {
                        // thread not across PSL
                        SetThreadToExit (pth);
                    }
                    break;
                    
                case RUNSTATE_BLOCKED:
    
                    if (pth->pprcOwner == pth->pprcActv) {

                        if (USER_MODE == GetThreadMode (pth)) {
                            // blocked in user mode -> jump to exit code path
                            SetThreadToExit (pth);

                        } else if (GET_USERBLOCK (pth)) {
                            // thread blocked on sync object from blocking call
                            pth->lpProxy = 0;
                            pth->bWaitState   = WAITSTATE_SIGNALLED;
                            pth->dwWaitResult = WAIT_FAILED;

                        } else {
                            // blocked on kernel internal sync object
                            // can't terminate on the spot
                            break;
                        }
                        
                        pth->bSuspendCnt = 0;
                        pth->wCount++;
                        SET_RUNSTATE (pth, RUNSTATE_NEEDSRUN);
                        psttd->pThNeedRun = pth;
                    }
                    break;
                    
                case RUNSTATE_RUNNING:
                    // it's a thread running on another CPU.
                    DEBUGCHK (g_pKData->nCpus > 1);
                    SendIPI (IPI_TERMINATE_THREAD, (DWORD)pth);
                    break;
                    
                case RUNSTATE_NEEDSRUN:
                    // can't kill on the spot
                    break;
                    
                default:
                    DEBUGCHK(0);
                    break;
                            }
            }
        }
    }

    ReleaseSchedulerLock (17);

    return fRet;
}


//------------------------------------------------------------------------------
// change thread quantum
//------------------------------------------------------------------------------
BOOL SCHL_SetThreadQuantum (PTHREAD pth, DWORD dwQuantum)
{
    BOOL fRet;
    AcquireSchedulerLock (17);
    fRet = KC_IsValidThread (pth);
    if (fRet) {
        pth->dwQuantum = dwQuantum;
    }

    ReleaseSchedulerLock (17);
    return fRet;
}


//------------------------------------------------------------------------------
// get kernel time of current running thread
//------------------------------------------------------------------------------
DWORD SCHL_GetCurThreadKTime (void) 
{
    PPCB    ppcb;
    PTHREAD pCurTh;
    DWORD retval;
    AcquireSchedulerLock (48);

    ppcb   = GetPCB ();
    pCurTh = ppcb->pCurThd;
    retval = pCurTh->dwKTime;
    if (GET_TIMEMODE(pCurTh))
        retval += (GETCURRTICK () - ppcb->dwPrevTimeModeTime);

    ReleaseSchedulerLock (48);
    
    return retval;
}


//------------------------------------------------------------------------------
// get user time of current running thread
//------------------------------------------------------------------------------
DWORD SCHL_GetCurThreadUTime (void) 
{
    PPCB    ppcb;
    PTHREAD pCurTh;
    DWORD retval;
    AcquireSchedulerLock (65);
    ppcb   = GetPCB ();
    pCurTh = ppcb->pCurThd;
    retval = pCurTh->dwUTime;
    if (!GET_TIMEMODE(pCurTh))
        retval += (GETCURRTICK () - ppcb->dwPrevTimeModeTime);
    ReleaseSchedulerLock (65);
    return retval;
}

//------------------------------------------------------------------------------
// Set thread affinity
//------------------------------------------------------------------------------
void SCHL_SetThreadAffinity (PTHREAD pth, DWORD dwAffinity)
{
    if (g_pKData->nCpus > 1) {
        PTWO_D_QUEUE pNewRunQ;
        AcquireSchedulerLock (65);

        if (dwAffinity) {
            PPCB ppcb = g_ppcbs[dwAffinity-1];
            if (CE_PROCESSOR_STATE_POWERED_ON == ppcb->dwCpuState) {
                pNewRunQ = &ppcb->affinityQ;
            } else {
                // we can only get into this case when CPU got powered off in between the check 
                // in NKSetThreadAffinity and acquiring the spinlock. In which case, it'll behave
                // as if the call to NKSetThreadAffinity succeed and CPU got powered of right away.
                pNewRunQ   = &g_ppcbs[0]->affinityQ;
                dwAffinity = 1;
            }
        } else {
            pNewRunQ = &RunList;
        }

        if (pNewRunQ != pth->pRunQ) {

            pth->dwAffinity = dwAffinity;

            if (RUNSTATE_RUNNABLE != GET_RUNSTATE (pth)) {
                // not in any queue

                // update thread's run queue
                pth->pRunQ = pNewRunQ;

                if ((RUNSTATE_RUNNING == GET_RUNSTATE (pth)) && dwAffinity) {
                    // the thread is currently running (can be on any CPU).
                    PPCB ppcb = GetPCB ();
                    if (pth != ppcb->pCurThd) {
                        // force other CPU to reschedule
                        ppcb->wIPIPending = 2;
                        
                    } else if (dwAffinity != ppcb->dwCpuId) {
                        // thread running on current CPU changed affinity
                        BlockCurThread (FALSE);
                        pth->dwQuantLeft ++; // Make run will decrement the quantum by 1                        
                        MakeRun (pth, FALSE);
                    }
                }
                
                
            } else {
                // was in a run queue and affinity changed. 
                
                // remove from current runq
                RunqDequeue (pth, GET_CPRIO (pth));

                // update thread's run queue
                pth->pRunQ = pNewRunQ;

                // insert back to run queue
                MakeRun (pth, TRUE);
            }
        }
        
        ReleaseSchedulerLock (65);
    }
}


//------------------------------------------------------------------------------
// SCHL_FinalRemoveThread - final stage of thread/process cleanup.
//------------------------------------------------------------------------------
void SCHL_FinalRemoveThread (PHDATA phdThrd, PSTUBTHREAD pStubThread, BOOL fIsMainThread)
{
    PPCB   ppcb = GetPCB ();
    PTHREAD pth = GetThreadPtr (phdThrd);

    // SCHL_FinalRemoveThread MUST be running on KStack, for we're going to release the thread stack in this function
    DEBUGCHK (InPrivilegeCall ());
    
    AcquireSchedulerLock (36);

    DEBUGCHK (pth == ppcb->pCurThd);
    DEBUGCHK (!pth->ownedlist.pHead);
    DEBUGCHK (GetPCB ()->pthSched == pth);
    DEBUGCHK (pth->tlsPtr == pth->tlsSecure);
    DEBUGCHK (!pth->lpProxy);
    DEBUGCHK (!pth->pBlockers);

    if (g_pthMonitor == pth) {
        g_pthMonitor = NULL;
    }

    MDDiscardFPUContent ();

    // cache the thread's secure stack in kernel's stack list
    VMCacheStack (pth->pStkForExit, (DWORD) pth->tlsSecure & ~VM_BLOCK_OFST_MASK, KRN_STACK_SIZE);

    // the thread handle can no longer get to the thread
    phdThrd->pvObj  = NULL;

    if (fIsMainThread) {
        // main thread - free the per-process ready queue
        if (pStubThread) {
            DEBUGCHK (!pStubThread->ProcReadyQ.pHead);
            FreeMem (pStubThread, HEAP_STUBTHREAD);
        }
        SetReschedule (ppcb);
        ppcb->pthSched = NULL;
    } else {
        // secondary thread - mark current thread blocked, and make the next thread
        //                    in the proc queue runnable, if any.
        BlockCurThread (FALSE);
    }

    SoftLog (0x44440001, pth->dwId + PcbGetCurCpu () - 1);

    DEBUGMSG (ZONE_SCHEDULE, (L"Thread %8.8lx complete terminated\r\n", pth->dwId));

    ppcb->pCurThd = &dummyThread;

    // Add the HDATA to delay free list. As it can be freed by other CPUs once added to the list,
    //     we need to update pCurThd to dummy such that reference to it won't cause fault.
    KC_AddDelayFreeThread (phdThrd);

    ReleaseSchedulerLock (36);
    
}

//------------------------------------------------------------------------------
// SCHL_SwitchOwnerToKernel - thread exiting path, thread exiting path, make kernel owner of a thread  
//                            to kernel. handle stubthread if there is
//------------------------------------------------------------------------------
void SCHL_SwitchOwnerToKernel (PTHREAD pth)
{
    AcquireSchedulerLock (13);
    
    DEBUGCHK (pth == pCurThread);

    MakeRunStubThread (pth);

    pth->pprcOwner      = g_pprcNK;
    pth->pRunQ          = &RunList;
    pth->dwAffinity     = 0;

    ReleaseSchedulerLock (13);
}


//------------------------------------------------------------------------------
// SCHL_MakeRunIfNeeded - make a thread runnable if needed
//------------------------------------------------------------------------------
void SCHL_MakeRunIfNeeded (PTHREAD pth)
{
    AcquireSchedulerLock (39);
    if (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN) {
        if (GET_SLEEPING(pth)) {
            SleepqDequeue(pth);
            pth->wCount ++;
        }
        MakeRun (pth, !pth->dwQuantLeft);
    }
    ReleaseSchedulerLock (39);
}

//------------------------------------------------------------------------------
// SCHL_DequeueFlatProxy - remove a proxy from a flat queue (not a 2-d priority queue)
//------------------------------------------------------------------------------
BOOL SCHL_DequeueFlatProxy (PPROXY pprox) 
{
    AcquireSchedulerLock (54);
    if (pprox->pQDown) {
        FlatDequeue (QNODE_OF_PROXY (pprox));
    }
    ReleaseSchedulerLock (54);
    return FALSE;
}


//------------------------------------------------------------------------------
// SCHL_DequeuePrioProxy - remove a proxy form a 2-d priority queue
//------------------------------------------------------------------------------
BOOL SCHL_DequeuePrioProxy (PPROXY pprox) 
{
    BOOL fRet = FALSE;
    AcquireSchedulerLock (31);
    if (pprox->pQDown) {
        PEVENT pEvt = (PEVENT) pprox->pObject;
        fRet = PrioDequeue (&pEvt->proxyqueue, QNODE_OF_PROXY (pprox), pprox->prio);
    }
    ReleaseSchedulerLock (31);
    return fRet;
}

//------------------------------------------------------------------------------
// SCHL_PostUnlinkCritMut - final step of releasing the ownership of a CS/Mutex.
//                     Adjust thread priority based on CS/mutex still owned.
//------------------------------------------------------------------------------
void SCHL_PostUnlinkCritMut (PMUTEX pMutex, LPCRITICAL_SECTION lpcs) 
{
    PTHREAD pCurTh;

    AcquireSchedulerLock (52);
    pCurTh = GetPCB ()->pCurThd;

    DEBUGCHK (pCurTh != pMutex->pOwner);
    // Though I'd like to debugchk here for to make sure we have everything in sync, 
    // we cannot reference lpcs here, for the CS can be delelted once it's released.
    // DEBUGCHK (!lpcs || ((pCurTh->dwId|1) != (((DWORD) lpcs->OwnerThread - 2) | 3)));

    CLEAR_NOPRIOCALC (pCurTh);
    RecalcCPrio (pCurTh, TRUE);
    ReleaseSchedulerLock (52);
}


//------------------------------------------------------------------------------
// SCHL_DoReprioCritMut - re-prio a CS in the owner thread's 'owned object list'
//------------------------------------------------------------------------------
void SCHL_DoReprioCritMut (PMUTEX pMutex)
{
    PTHREAD pOwner;
    AcquireSchedulerLock (4);
    pOwner = pMutex->pOwner;
    
    if (pOwner && (CRIT_STATE_OWNED == pMutex->bListed)) {
        UnlinkCritMut (pMutex, pOwner);
        if (pMutex->proxyqueue.pHead || !pMutex->lpcs) {
            // it's a mutex or a cs with threads waiting for it,
            // add it to the owner thread's "owned object list"
            DoLinkCritMut (pMutex, pOwner);
        } else {
            // no one is blocked on the mutex, change the state to "not owned"
            pMutex->bListed = CRIT_STATE_NOT_OWNED;
        }
    }
    ReleaseSchedulerLock (4);
}

//------------------------------------------------------------------------------
// SCHL_PreReleaseMutex - step 1 of releasing the ownership of a Mutex
//------------------------------------------------------------------------------
BOOL SCHL_PreReleaseMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs)
{
    BOOL fRet;
    AcquireSchedulerLock (51);
    fRet = PreReleaseMutex (pMutex, lpcs);
    ReleaseSchedulerLock (51);
    return fRet;
}

//------------------------------------------------------------------------------
// PreAbandonCrit - thread is giving up ownership of all of it's owned CS/mutex
//                  in the thread exit path.
//------------------------------------------------------------------------------
PMUTEX SCHL_PreAbandonMutex (PTHREAD pth)
{
    PMUTEX pMutex;

    AcquireSchedulerLock (53);

    DEBUGCHK (pth == GetPCB ()->pCurThd);
    pMutex = MUTEX_OF_QNODE (pth->ownedlist.pHead);

    if (pMutex) {
        PreReleaseMutex (pMutex, IsKModeAddr ((DWORD) pMutex->lpcs)? pMutex->lpcs : 0);
    } else {
        SET_BURIED (pth);
    }
    ReleaseSchedulerLock (53);
    return pMutex;
}

//------------------------------------------------------------------------------
// PostBoostCrit12 - part 2 of priority inheritance handling of CS.
//                  (boost the priority of the owner thread)
//------------------------------------------------------------------------------
void SCHL_BoostMutexOwnerPriority (PTHREAD pCurTh, PMUTEX pMutex)
{
    AcquireSchedulerLock (60);
    
    if (WAITSTATE_PROCESSING == pCurTh->bWaitState) {
        PTWO_D_NODE pBlocker    = pMutex->proxyqueue.pHead;
        PTHREAD     pOwner      = pMutex->pOwner;
        DWORD       prio        = pBlocker->prio;
        DWORD       oldcprio    = GET_CPRIO (pOwner);

        DEBUGCHK ( pOwner 
                && pBlocker
                && (   (CRIT_STATE_OWNED != pMutex->bListed)
                    || (pMutex->bListedPrio == pBlocker->prio)));

        if (prio < oldcprio) {

            SET_CPRIO (pOwner,prio);

            if (GET_RUNSTATE (pOwner) == RUNSTATE_RUNNABLE) {
                RunqDequeue (pOwner, oldcprio);
                MakeRun (pOwner, TRUE);

            } else if (GET_SLEEPING (pOwner) && pOwner->pUpSleep && (GET_RUNSTATE(pOwner) != RUNSTATE_NEEDSRUN)) {
                SleepqDequeue (pOwner);
                SET_NEEDSLEEP (pOwner);
                if (pOwner->lpProxy) {
                    pOwner->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
                }
                MakeRun (pOwner, TRUE);
            }

            CELOG_KCThreadPriorityInvert (pOwner);
        }
    }
    ReleaseSchedulerLock (60);
}

//------------------------------------------------------------------------------
// SCHL_LinkMutexOwner - link mutex to owner thread and handle priority inheritance
//------------------------------------------------------------------------------
void SCHL_LinkMutexOwner (PTHREAD pCurTh, PMUTEX pMutex)
{
    PTWO_D_NODE pBlocker;
    DWORD       prio;

    AcquireSchedulerLock (59);

    pBlocker = pMutex->proxyqueue.pHead;
    prio     = pBlocker? pBlocker->prio : (MAX_CE_PRIORITY_LEVELS - 1);

    if (CRIT_STATE_OWNED != pMutex->bListed) {

        pMutex->bListedPrio = (BYTE) prio;
        pMutex->bListed     = CRIT_STATE_OWNED;

        PrioEnqueue (&pCurTh->ownedlist, QNODE_OF_MUTEX (pMutex), TRUE);
    }

    if (prio < GET_CPRIO (pCurTh)) {
        SET_CPRIO (pCurTh, prio);
        CELOG_KCThreadPriorityInvert (pCurTh);
    }

    ReleaseSchedulerLock (59);
}

//------------------------------------------------------------------------------
// SCHL_UnlinkMutexOwner - unlink mutex from its owner thread
// NOTE: This function should only be called when a mutex object is to be destroyed.
//------------------------------------------------------------------------------
void SCHL_UnlinkMutexOwner (PMUTEX pMutex)
{
    PTHREAD pOwner;
    DEBUGMSG (pMutex->pOwner, (L"Mutex 0x%8.8lx is destroyed while thread 0x%8.8lx still owns the mutex\r\n", pMutex, pMutex->pOwner));
    AcquireSchedulerLock (59);
    pOwner = pMutex->pOwner;
    if (pOwner) {
        UnlinkCritMut (pMutex, pOwner);
        pMutex->pOwner = NULL;
    }
    ReleaseSchedulerLock (59);
}

//------------------------------------------------------------------------------
// SCHL_MutexFinalBoost - final step of priority inheritance handling of CS.
//                  (boost the priority of the new owner)
//------------------------------------------------------------------------------
void SCHL_MutexFinalBoost (PMUTEX pMutex, LPCRITICAL_SECTION lpcs)
{
    PTHREAD     pCurTh;
    PTWO_D_NODE pBlocker;

    AcquireSchedulerLock (59);

    pCurTh = GetPCB ()->pCurThd;
    
    DEBUGCHK (!IsOwnerLeaving (pMutex));
    DEBUGCHK (pMutex->pOwner == pCurTh);

    if (lpcs) {
        //
        // We need to make sure owner id is the right one, for we can be given the CS due to 
        // a thread exiting. In which case, it'll update pMutex->pCurTh, but won't change lpcs->OwnerThread.
        //
        lpcs->OwnerThread = (HANDLE) pCurTh->dwId;
    }

    pBlocker = pMutex->proxyqueue.pHead;
    if (!pBlocker) {
        // no one blocking on this mutex anymore
        UnlinkCritMut (pMutex, pCurTh);

        // uncontented CS does not have a owner. For LeaveCriticalSection won't trap into kernel if not contented
        if (lpcs) {
            pMutex->pOwner = NULL;
        }
        
    } else {
        // someone is blocked on the mutex. claim ownership
        DWORD prio;

        if (CRIT_STATE_OWNED != pMutex->bListed) {
            DoLinkCritMut (pMutex, pCurTh);
        }

        if ((prio = pBlocker->prio) < GET_CPRIO (pCurTh)) {
            SET_CPRIO (pCurTh, prio);
            CELOG_KCThreadPriorityInvert (pCurTh);
        }
    }
    
    DEBUGCHK (!lpcs || !pMutex->pOwner || pMutex->proxyqueue.pHead);

    ReleaseSchedulerLock (59);
}

//------------------------------------------------------------------------------
// SCHL_ReleaseMutex - final stop to leave a CS/mutex.
//          give the CS/mutex to new owner or set owner to 0 if there is no one waiting.
//          Return TRUE if needs to keep searching.
//------------------------------------------------------------------------------
BOOL SCHL_ReleaseMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs, PTHREAD *ppTh)
{
    PTHREAD pCurTh;

    AcquireSchedulerLock (20);

    pCurTh = GetPCB ()->pCurThd;
    if (pMutex->pOwner == pCurTh) {  // the test will fail if the CS is claimed by other threads while we're leaving

        DWORD dwNewOwnerId = 0;
        
        DEBUGCHK (CRIT_STATE_LEAVING == pMutex->bListed);
        DEBUGCHK (!lpcs || (lpcs->OwnerThread == (HANDLE) (pCurTh->dwId+2)));

        if (!pMutex->proxyqueue.pHead) {
            // no one is waiting on the CS/Mutex - no longer owned
            pMutex->pOwner  = NULL;
            pMutex->bListed = CRIT_STATE_NOT_OWNED;
            
            if (lpcs) {
                lpcs->OwnerThread = NULL;
            }
        } else {
            // someone is waiting, try to wakeup the 1st thread
            PPROXY  pprox = PROXY_OF_QNODE (PrioDequeueHead (&pMutex->proxyqueue));
            PTHREAD pNewOwner;
            
            DEBUGCHK (pprox);
            
            pNewOwner = pprox->pTh;
            DEBUGCHK(pNewOwner);
            
            if (pNewOwner->wCount != pprox->wCount) {
                // thread terminated/signaled while blocking on the CS/mutex - keep searching
                ReleaseSchedulerLock(20);
                return TRUE;
            }
            
            DEBUGCHK (ISMUTEX (pMutex) || !GET_SLEEPING (pNewOwner));
            DEBUGCHK (pNewOwner != pCurTh);
            
            // wake up new owner
            *ppTh = SignalThreadWithProxy (pprox);

            if (lpcs) {
                dwNewOwnerId = pNewOwner->dwId;
                lpcs->OwnerThread = (HANDLE) dwNewOwnerId;
            }
            pMutex->pOwner = pNewOwner;
            pMutex->LockCount = 1;

            if (pMutex->proxyqueue.pHead) {
                // add to new owner's "owned list" if there is still anyone waiting
                DoLinkCritMut (pMutex, pNewOwner);
            } else {
                // no one waiting, don't add to any thread's owned object list
                pMutex->bListed = CRIT_STATE_NOT_OWNED;
            }
        
        }

        if (lpcs) {
            // Log that a different thread was unblocked and now owns the CS
            CELOG_CriticalSectionLeave (dwNewOwnerId, lpcs);
        }
    }
    ReleaseSchedulerLock(20);
    return FALSE;
}

//------------------------------------------------------------------------------
// SCHL_YieldToNewMutexOwner - upon release of mutex, yield to the new owner if 
//          it's of priority equal to or higher than the running thread.
//------------------------------------------------------------------------------
void SCHL_YieldToNewMutexOwner (PTHREAD pNewOwner)
{
    AcquireSchedulerLock (1);
    if (GET_RUNSTATE(pNewOwner) == RUNSTATE_NEEDSRUN) {
        if (GET_SLEEPING(pNewOwner)) {
            SleepqDequeue(pNewOwner);
            pNewOwner->wCount ++;
        }
        if ((g_nCpuReady == 1) && (pNewOwner->dwQuantLeft > 1)) {
            //
            // Lock Convoy reduction - one single core, make owner of mutex/cs running as soon as it
            //      get the mutex/cs on single core.
            //
            // Yield to the new owner if new owner of priority equal or higher.
            // we do this by putting the current running thread into the head of the run queue and 
            // force reschedule. When we make the new owner runnable, it'll be add to the head of 
            // run queue ahead of us and become the new running thread right away after reschedule.
            // 
            PTHREAD pCurTh = pCurThread;
            if (GET_CPRIO(pNewOwner) <= GET_CPRIO(pCurTh)) {
                BlockCurThread (FALSE);
                pCurTh->dwQuantLeft ++;     // Make run will decrement the quantum by 1
                                            // And the thread leaving CS shouldn't be paying
                                            // the quantum panelty.
                MakeRun (pCurTh, FALSE);
            }
        }
        MakeRun (pNewOwner, !pNewOwner->dwQuantLeft);
    }
    ReleaseSchedulerLock (1);
}


//------------------------------------------------------------------------------
// SCHL_WakeOneThreadFlat - wake up a thread that is blocked on thread/manual-event
//                      (object linked on a flat-queue)
//------------------------------------------------------------------------------
PTWO_D_NODE SCHL_WakeOneThreadFlat (PTWO_D_NODE *ppQHead, PTHREAD *ppTh) 
{
    PTWO_D_NODE pHead;
    AcquireSchedulerLock (41);
    pHead = *ppQHead;
    if (pHead) {
        PPROXY pprox = PROXY_OF_QNODE (pHead);
        DEBUGCHK ((pprox->bType == SH_CURTHREAD) || (pprox->bType == HT_MANUALEVENT));
        DEBUGCHK (pprox->pQPrev == (PPROXY) ppQHead);
        DEBUGCHK (pprox->pQDown);
        DEBUGCHK (pprox->pTh);

        // dequeue the proxy
        FlatDequeue (pHead);

        // wakeup the thread
        if (pprox->pTh->wCount == pprox->wCount) {
            *ppTh = SignalThreadWithProxy (pprox);
        }
    }
    ReleaseSchedulerLock (41);
    return pHead;
}



//------------------------------------------------------------------------------
// SCHL_EventModMan - set/pulse a manual reset event
//------------------------------------------------------------------------------
DWORD SCHL_EventModMan (PEVENT pEvt, PTWO_D_NODE *ppStubQHead, DWORD action)
{
    DWORD prio;
    AcquireSchedulerLock (15);
    prio = pEvt->bMaxPrio;
    pEvt->bMaxPrio = THREAD_RT_PRIORITY_IDLE;
    *ppStubQHead = pEvt->proxyqueue.pHead;
    if (*ppStubQHead) {
        (*ppStubQHead)->pQPrev = (PTWO_D_NODE) ppStubQHead;
        DEBUGCHK ((*ppStubQHead)->pQDown);
        pEvt->proxyqueue.pHead = NULL;
    }
    pEvt->state = (action == EVENT_SET);
    ReleaseSchedulerLock (15);
    return prio;
}

//------------------------------------------------------------------------------
// SCHL_EventModAuto - set/pulse a auto-reset event
//------------------------------------------------------------------------------
BOOL SCHL_EventModAuto (PEVENT pEvt, DWORD action, PTHREAD *ppTh)
{
    BOOL    bRet = FALSE;
    PPROXY  pprox;

    AcquireSchedulerLock(16);

    pprox = PROXY_OF_QNODE (PrioDequeueHead (&pEvt->proxyqueue));
    if (!pprox) {
        pEvt->state = (action == EVENT_SET);

    } else {
        PTHREAD pth = pprox->pTh;

        DEBUGCHK (!pEvt->phdIntr);
        DEBUGCHK(pth);

        if (pth->wCount != pprox->wCount) {
            bRet = TRUE;
        } else {
            // wakeup the thread
            *ppTh = SignalThreadWithProxy (pprox);
            pEvt->state = 0;
        }
    }

    ReleaseSchedulerLock (16);
    return bRet;
}

//------------------------------------------------------------------------------
// SCHL_ResetProcEvent -- reset a process event if the process hasn't exited yet
//------------------------------------------------------------------------------
void SCHL_ResetProcEvent (PEVENT pEvt)
{
    AcquireSchedulerLock (16);
    if (!(pEvt->manualreset & PROCESS_EXITED)) {
        pEvt->state = 0;
    }
    ReleaseSchedulerLock (16);
}



//------------------------------------------------------------------------------
// SCHL_EventModIntr -- set/pulse a interrupt event
//------------------------------------------------------------------------------
PTHREAD SCHL_EventModIntr (PEVENT pEvt, DWORD action) 
{
    PTHREAD pth = NULL;
    AcquireSchedulerLock (42);
    if (!pEvt->proxyqueue.pHead) {
        pEvt->state = (action == EVENT_SET);
    } else {
        pEvt->state = 0;
        pth = WakeupIST (pEvt);
    }
    ReleaseSchedulerLock (42);
    return pth;
}



//------------------------------------------------------------------------------
// SCHL_AdjustPrioDown -- adjust the priority of current running thread while set/pulse 
//                   manual-reset events.
//------------------------------------------------------------------------------
void SCHL_AdjustPrioDown (void) 
{
    AcquireSchedulerLock (66);
    RecalcCPrio (GetPCB ()->pCurThd, FALSE);
    ReleaseSchedulerLock (66);
}

//------------------------------------------------------------------------------
// SamAdd - increment Semaphore count by lReleaseCount,
//          returns previous count (-1 if exceed max)
//------------------------------------------------------------------------------
LONG SCHL_SemAdd (PSEMAPHORE lpsem, LONG lReleaseCount) 
{
    LONG prev;
    AcquireSchedulerLock (3);
    if ((lReleaseCount <= 0)
        || (lpsem->lCount + lpsem->lPending + lReleaseCount > lpsem->lMaxCount)
        || (lpsem->lCount + lpsem->lPending + lReleaseCount < lpsem->lCount + lpsem->lPending)) {
        prev = -1;
    } else {
        prev = lpsem->lCount + lpsem->lPending;
        lpsem->lPending += lReleaseCount;
    }
    ReleaseSchedulerLock(3);
    return prev;
}

//------------------------------------------------------------------------------
// relase all threads, up to the current semaphore count, blocked on the semaphore
//------------------------------------------------------------------------------
BOOL SCHL_SemPop (PSEMAPHORE lpsem, LPLONG pRemain, PTHREAD *ppTh) 
{
    BOOL        bRet = FALSE;
    AcquireSchedulerLock (37);

    if (*pRemain) {
        PPROXY pprox = PROXY_OF_QNODE (PrioDequeueHead (&lpsem->proxyqueue));

        DEBUGCHK (*pRemain <= lpsem->lPending);
        
        if (!pprox) {
            lpsem->lCount += *pRemain;
            lpsem->lPending -= *pRemain;
        } else {
            PTHREAD pth = pprox->pTh;
            DEBUGCHK(pth);

            bRet = TRUE;
            if (pth->wCount == pprox->wCount) {
                // wake up thread        
                *ppTh = SignalThreadWithProxy (pprox);
                lpsem->lPending--;
                (*pRemain)--;
            }
        }
    }
    ReleaseSchedulerLock(37);
    return bRet;
}


//------------------------------------------------------------------------------
// SCHL_PutThreadToSleep - put a thread into the sleep queue
//------------------------------------------------------------------------------
BOOL SCHL_PutThreadToSleep (SLEEPSTRUCT *pSleeper) 
{
    PTHREAD pCurTh;
    BOOL fKeepTrying = FALSE;
    
    AcquireSchedulerLock (2);

    pCurTh = GetPCB ()->pCurThd;

    DEBUGCHK(!GET_SLEEPING(pCurTh));
    DEBUGCHK(!pCurTh->pUpSleep && !pCurTh->pDownSleep);

    CLEAR_NEEDSLEEP (pCurTh);

    if (!(GET_DYING(pCurTh) && !GET_DEAD(pCurTh) && GET_USERBLOCK(pCurTh))) {      // thread not dying
        fKeepTrying = SleepOneMore (pSleeper);
    }

    ReleaseSchedulerLock (2);
    return fKeepTrying;
}



//------------------------------------------------------------------------------
// SCHL_WaitOneMore - process one synchronization object in Wait funciton,
//               return continue search or not
//------------------------------------------------------------------------------
BOOL SCHL_WaitOneMore (PWAITSTRUCT pws) 
{
    PPCB    ppcb;
    PTHREAD pCurTh;
    PPROXY pProx;
    BOOL   fContinue = FALSE;

    AcquireSchedulerLock (18);

    ppcb   = GetPCB ();
    pCurTh = ppcb->pCurThd;
    CLEAR_NEEDSLEEP (pCurThread);

    // if the thread is signaled, do nothing (signaling process would have setup everything)
    if (WAITSTATE_SIGNALLED != pCurTh->bWaitState) {
    
        // if the thread is terminated, fail
        if (IsThreadBeingTerminated (pCurTh)) {
            KSetLastError (pCurTh, ERROR_OPERATION_ABORTED);
            pCurTh->dwWaitResult = WAIT_FAILED;
            SignalThread (pCurTh);

        // considered signaled if user funciton return TRUE
        } else if (pws->pfnWait && pws->pfnWait (pws->dwUserData)) {
            pCurTh->dwWaitResult = WAIT_OBJECT_0;
            SignalThread (pCurTh);

        // any more object to wait?
        } else if (NULL != (pProx = pws->pProxyPending)) {
            DEBUGCHK(pCurTh->bWaitState == WAITSTATE_PROCESSING);
            DEBUGCHK(pCurTh == pProx->pTh);
            DEBUGCHK(pProx->pTh == pCurTh);
            DEBUGCHK(pProx->wCount == pCurTh->wCount);

            switch (CheckOneSyncObject (pProx, pws)) {
            case SIGNAL_STATE_NOT_SIGNALED:
                // not signaled, update pending/checked proxy list
                pws->pProxyPending = pProx->pThLinkNext;
                pProx->pThLinkNext = pCurTh->lpProxy;
                pCurTh->lpProxy = pws->pProxyEnqueued = pProx;
                fContinue = TRUE;
                break;
                
            case SIGNAL_STATE_BAD_WAIT:
                pCurTh->dwWaitResult = WAIT_FAILED;
                KSetLastError (pCurTh,ERROR_INVALID_HANDLE);
            
                // fall through to signal thread
            
                __fallthrough;
            
            case SIGNAL_STATE_SIGNALED:
            
                SignalThread (pCurTh);
                break;
            default:
                DEBUGCHK (0);
            }


        // no more object to wait, check timeout
        } else if (INFINITE != pws->dwTimeout) {

            pCurTh->dwWaitResult = WAIT_TIMEOUT;    // default timeout. If object signaled, it'll be changed by the 
                                                    // signaling funciton

            if (pws->dwTimeout) {
                fContinue = SleepOneMore (&pws->sleeper);
            } else {
                // handle it like Sleep (0), such that it'll yield before polling
                SignalThread (pCurTh);
                BlockCurThread (FALSE);
                MakeRun (pCurTh, TRUE);
                ppcb->wIPIPending = 0;
            }

        // infinit timeout, just block
        } else {

            pCurTh->bWaitState = WAITSTATE_BLOCKED;
            BlockCurThread (FALSE);
        }

    }

    ReleaseSchedulerLock (18);
    return fContinue;
}


//------------------------------------------------------------------------------
// SCHL_CSWaitPart1 - part I of grabbing a CS.
//------------------------------------------------------------------------------
DWORD SCHL_CSWaitPart1 (PMUTEX *ppCritMut, PPROXY pProx, PMUTEX pMutex, LPCRITICAL_SECTION lpcs) 
{
    PPCB    ppcb;
    PTHREAD pCurTh;
    BOOL    dwSignalState = SIGNAL_STATE_SIGNALED;

    AcquireSchedulerLock (19);

    ppcb = GetPCB ();
    pCurTh = ppcb->pCurThd;

    DEBUGCHK (!*ppCritMut);
    DEBUGCHK (ppcb->pthSched == pCurTh);
    DEBUGCHK (GET_RUNSTATE(pCurTh) != RUNSTATE_BLOCKED);
    DEBUGCHK (WAITSTATE_PROCESSING == pCurTh->bWaitState);

    if ((!IsKernelVa (lpcs) && IsThreadBeingTerminated (pCurTh))
        || (pMutex->pOwner == pCurTh)) {

        //
        // if we hit this debugchk, we're trapping into kernel to grab a CS we already owned...
        //
        DEBUGCHK (pMutex->pOwner != pCurTh);
        
        dwSignalState = SIGNAL_STATE_BAD_WAIT;
        SignalThread (pCurTh);

    } else {

        PTWO_D_NODE pBlocker    = pMutex->proxyqueue.pHead;
        BYTE        prio        = pProx->prio = GET_CPRIO (pCurTh);
        PTHREAD     pOwner      = pMutex->pOwner;
        
        
        DEBUGCHK(pProx->pTh == pCurTh);
        DEBUGCHK(pProx->wCount == pCurTh->wCount);
        DEBUGCHK(pProx->bType == HT_CRITSEC);
        DEBUGCHK(!pCurTh->lpProxy);
        DEBUGCHK(pMutex == (PMUTEX)pProx->pObject);
        DEBUGCHK(prio == GET_CPRIO(pCurTh));

        if (pBlocker && (pBlocker->prio <= prio)) {
            // other thread of higher or equal already acquiring the CS, block.
            dwSignalState = SIGNAL_STATE_NOT_SIGNALED;
            DEBUGCHK (pOwner);
        } else {

            
            DEBUGCHK (!pOwner || lpcs->OwnerThread);

            if (!pOwner) {

                DWORD    dwIdCompare = 0, dwCurOwnerId;
                
                // try to grab the CS if possible
                for ( ; ; ) {
                    
                    dwCurOwnerId = InterlockedCompareExchange ((PLONG)&lpcs->OwnerThread, pCurTh->dwId, dwIdCompare);

                    if (dwCurOwnerId == dwIdCompare) {
                        // CS claimed
                        break;
                    }
                    
                    if (dwCurOwnerId && !(dwCurOwnerId & 1)) {

                        // CS is owned by a thread (id can be either thread_id or thread_id + 2)
                        // owner to id+2.
                        pMutex->pOwner = pOwner = KC_PthFromId ((dwCurOwnerId - 2) | 2);
                        break;
                    }
                    dwIdCompare = dwCurOwnerId;
                }
            }

            if (   pOwner                                               // owner id valid
                && !IsOwnerLeaving (pMutex)                             // owner not leaving the CS
                && (IsKernelVa (lpcs) || !GET_BURIED (pOwner))) {       // owner not at last stage of exiting
                dwSignalState = SIGNAL_STATE_NOT_SIGNALED;
            }
        }

        DEBUGCHK (lpcs->OwnerThread);

        // determine if we need to block or grab the CS
        if (SIGNAL_STATE_SIGNALED == dwSignalState) {

            // got the CS, update owner and mutex state

            // if the CS is in the previous owner's "owned list", remove it.
            DEBUGCHK ((CRIT_STATE_OWNED != pMutex->bListed) || pOwner);
            UnlinkCritMut (pMutex, pOwner);

            lpcs->OwnerThread = (HANDLE) pCurTh->dwId;
            pMutex->pOwner    = pCurTh;

            pMutex->bListed = CRIT_STATE_NOT_OWNED;

            SignalThread (pCurTh);

        } else {
            DEBUGCHK (pOwner);

            // not signaled, queue ourself into the CS's waiting queue.
            pCurTh->lpProxy = pProx;
            QueueProxyToMutex (pMutex, pProx, ppCritMut);
        }

    }

    DEBUGCHK(GET_RUNSTATE(pCurTh) != RUNSTATE_BLOCKED);

    ReleaseSchedulerLock(19);
    return dwSignalState;
}



//------------------------------------------------------------------------------
// SCHL_CSWaitPart2 - part 2 of grabbing a CS.
//------------------------------------------------------------------------------
void SCHL_CSWaitPart2 (PMUTEX pMutex, LPCRITICAL_SECTION lpcs) 
{
    PTHREAD pCurTh;

    AcquireSchedulerLock (58);

    pCurTh = GetPCB ()->pCurThd;

    if (pCurTh->bWaitState != WAITSTATE_SIGNALLED) {
        if (!IsKernelVa (lpcs) && IsThreadBeingTerminated (pCurTh)) {
            SignalThread (pCurTh);

        } else {
            // Block
            pCurTh->bWaitState = WAITSTATE_BLOCKED;
            BlockCurThread (FALSE);
        }
    }
    ReleaseSchedulerLock (58);
}

//---------------------------------------------------------------------------------
//
// Kernel fast (reader/writer) lock implementation
//
//---------------------------------------------------------------------------------

static void SetupProxy (PTHREAD pCurTh, PFAST_LOCK pFastLock, PPROXY pprox, BYTE bType)
{
    // setup proxy
    pprox->bType        = bType;
    pprox->pQDown       = NULL;
    pprox->pObject      = (LPVOID) pFastLock;
    pprox->pTh          = pCurTh;
    pprox->wCount       = pCurTh->wCount;
}

static void BlockOnFastLock (PFAST_LOCK pFastLock, PPROXY pprox)
{
    PTHREAD pOwner = pFastLock->pOwner;
    BOOL    fBoost = PrioEnqueue (&pFastLock->proxyqueue, QNODE_OF_PROXY (pprox), TRUE);

    DEBUGCHK (OwnSchedulerLock ());


    if (pOwner) {
        DWORD newprio;
        DWORD oldprio;

        DEBUGCHK ((CRIT_STATE_OWNED == pFastLock->bListed)
                ||(CRIT_STATE_NOT_OWNED == pFastLock->bListed));
        
        // queue the fast lock the the owner's owned object list
        if (fBoost || (CRIT_STATE_NOT_OWNED == pFastLock->bListed)) {

            // if CRITSTATE_OWNED is set, it means that priority of the fastlock has changed, and
            // we need to remove and re-insert the fastlock
            if (CRIT_STATE_OWNED == pFastLock->bListed) {
                // remove from the owner's owned list
                PrioDequeue (&pOwner->ownedlist, QNODE_OF_MUTEX (pFastLock), pFastLock->bListedPrio);
            }
            // add to owner's owned list.
            DoLinkCritMut ((PMUTEX) pFastLock, pOwner);
        }

        DEBUGCHK (pOwner->ownedlist.pHead);

        // adjust owner priority if needed
        newprio  = pOwner->ownedlist.pHead->prio;
        oldprio  = GET_CPRIO (pOwner);

        if (newprio < oldprio) {
            SET_CPRIO (pOwner, newprio);
        
            if (GET_RUNSTATE (pOwner) == RUNSTATE_RUNNABLE) {
                RunqDequeue (pOwner, oldprio);
                MakeRun (pOwner, TRUE);
            }
        
            CELOG_KCThreadPriorityInvert (pOwner);
        }
    }
    pCurThread->bWaitState  = WAITSTATE_BLOCKED;
    BlockCurThread (FALSE);
}



// wake up the next thread blocked on reader/writer lock if possible
PTHREAD SCHL_UnblockNextThread (PFAST_LOCK pFastLock)
{
    PTHREAD pth = NULL;
    
    AcquireSchedulerLock (0);

    if (!(pFastLock->lLock & RWL_XBIT)) {
    
        PPROXY pprox = PROXY_OF_QNODE (pFastLock->proxyqueue.pHead);

        DEBUGCHK (CRIT_STATE_NOT_OWNED == pFastLock->bListed);
        
        if (pprox) {
            DEBUGCHK (pFastLock->lLock & RWL_WBIT);
            
            if (HT_READLOCK == pprox->bType) {
                // acquiring reader lock
                InterlockedIncrement (&pFastLock->lLock);

            // acquiring writer lock
            } else if (InterlockedCompareExchange (&pFastLock->lLock, RWL_XBIT|RWL_WBIT, RWL_WBIT) != RWL_WBIT) {
                // we've already woken up readers, can't wakeup the writer
                pprox = NULL;
            }

            if (pprox) {
                VERIFY (PROXY_OF_QNODE (PrioDequeueHead (&pFastLock->proxyqueue)) == pprox);
                pth = SignalThreadWithProxy (pprox);
                DEBUGCHK (pth);
                if (pFastLock->lLock & RWL_XBIT) {
                    // the lock is given to a writer, update the owner
                    pFastLock->pOwner = pth;
                }
            }
        }

    }
    if (!pFastLock->proxyqueue.pHead) {
        // no more waiting threads, clear Wait bit
        // NOTE: this must be done last, as any reader/writer can potentially claim the lock from here on.
        InterlockedAnd (&pFastLock->lLock, ~RWL_WBIT);
    }
    
    ReleaseSchedulerLock (0);
    return pth;
}

// try acquire read lock, block if can't grab it outright
void SCHL_WaitForReadLock (PFAST_LOCK pFastLock)
{
    PTHREAD pCurTh = pCurThread;
    PTWO_D_NODE pBlocker;
    PROXY       prox;
    DWORD       dwOldLockValue;

    // setup proxy
    SetupProxy (pCurTh, pFastLock, &prox, HT_READLOCK);

    AcquireSchedulerLock (0);

    pFastLock->dwContention ++;         // debug info - increment contention count
    
    // set Wait bit, any lock acquizition/relase after this loop will trap.
    dwOldLockValue = InterlockedOr (&pFastLock->lLock, RWL_WBIT);

    pBlocker = pFastLock->proxyqueue.pHead;

    if ((!pBlocker || (GET_CPRIO(pCurTh) < pBlocker->prio))
        && !(dwOldLockValue & RWL_XBIT)) {
        // our priority is higher than any waiter, and no one has the excluesive lock.

        // claim the lock
        InterlockedIncrement (&pFastLock->lLock);
        if (!pBlocker) {
            // clear wait-bit if there are no blockers
            InterlockedAnd (&pFastLock->lLock, ~RWL_WBIT);
        }
    } else {
        prox.prio = GET_CPRIO (pCurTh);
        BlockOnFastLock (pFastLock, &prox);
    }
    
    ReleaseSchedulerLock (0);

    DEBUGCHK (!prox.pQDown);

}

// try acquire write lock, block if can't grab it outright
void SCHL_WaitForWriteLock (PFAST_LOCK pFastLock)
{
    PTHREAD     pCurTh = pCurThread;
    PTWO_D_NODE pBlocker;
    PROXY       prox;

    // setup proxy
    SetupProxy (pCurTh, pFastLock, &prox, HT_WRITELOCK);

    AcquireSchedulerLock (0);

    pFastLock->dwContention ++;         // debug info - increment contention count

    // set Wait bit, any lock acquizition/relase after this loop will trap.
    InterlockedOr (&pFastLock->lLock, RWL_WBIT);

    pBlocker = pFastLock->proxyqueue.pHead;

    if ((!pBlocker || (GET_CPRIO(pCurTh) < pBlocker->prio))
        && (InterlockedCompareExchange (&pFastLock->lLock, RWL_XBIT|RWL_WBIT, RWL_WBIT) == RWL_WBIT)) {
        // our priority is higher than any waiter, and we claimed the lock successfully.
        pFastLock->pOwner = pCurTh;
        if (!pBlocker) {
            // clear wait-bit if there are no blockers
            InterlockedAnd (&pFastLock->lLock, ~RWL_WBIT);
        }
    } else {

        prox.prio = GET_CPRIO (pCurTh);
        BlockOnFastLock (pFastLock, &prox);
    }
    
    ReleaseSchedulerLock (0);

    DEBUGCHK (!prox.pQDown);

}

//
// begin releasing a write lock.
//
void SCHL_PreReleaseWriteLock (PFAST_LOCK pFastLock)
{
    AcquireSchedulerLock (0);
    DEBUGCHK ((RWL_XBIT|RWL_WBIT) == pFastLock->lLock);
    DEBUGCHK (!pFastLock->pOwner);
    InterlockedAnd (&pFastLock->lLock, ~RWL_XBIT);
    UnlinkCritMut ((PMUTEX) pFastLock, pCurThread);
    ReleaseSchedulerLock (0);
}


//------------------------------------------------------------------------------
// MP related functions.
//------------------------------------------------------------------------------
static void DoSendIPI (DWORD dwType, DWORD dwTarget, DWORD dwCommand, DWORD dwData)
{
    LONG nCpus = 0;

    switch (dwType) {
    case IPI_TYPE_ALL_BUT_SELF:
        nCpus = g_nCpuReady - 1;
        break;
    case IPI_TYPE_ALL_INCLUDE_SELF:
        nCpus = g_nCpuReady;
        break;
    case IPI_TYPE_SPECIFIC_CPU:
        nCpus = 1;
        break;
    default:
        DEBUGCHK (0);
    }

    if (nCpus) {
        g_dwIpiCommand = dwCommand;
        g_dwIpiData    = dwData;
        g_nCpuIpiDone  = 0;

        DataSyncBarrier ();
        g_pOemGlobal->pfnSendIpi (dwType, dwTarget);
        DataMemoryBarrier ();

        // wait till all CPUs done with the IPI
        while (nCpus != g_nCpuIpiDone) {
            MDSpin ();
        }
    }
}

void NKSendInterProcessorInterrupt (DWORD dwType, DWORD dwTarget, DWORD dwCommand, DWORD dwData)
{
    if (g_nCpuReady > 1) {
        AcquireSchedulerLock (29);

        if (!g_nCpuStopped) 
        {
            DoSendIPI (dwType, dwTarget, dwCommand, dwData);
        }  

        ReleaseSchedulerLock (29);
    }
}

void SendIPI (DWORD dwCommand, DWORD dwData)
{
    NKSendInterProcessorInterrupt (IPI_TYPE_ALL_BUT_SELF, 0, dwCommand, dwData);
}

void SendIPIAndReleaseSchedulerLock (void)
{
    DEBUGCHK (OwnSchedulerLock ());
    
    if ((g_nCpuReady > 1) && !g_nCpuStopped) {

        PPCB  ppcb  = GetPCB ();
        DWORD dwIPIPending = ppcb->wIPIPending;

        if (dwIPIPending) {
            
            ppcb->wIPIPending = 0;
            
            DoSendIPI (IPI_TYPE_ALL_BUT_SELF, 0, IPI_RESCHEDULE, dwIPIPending-1);

        }
    }
    ReleaseSpinLock (&g_schedLock);
}

static DWORD g_dwTickDebuggerStart;

void StopAllOtherCPUs (void)
{
    AcquireSchedulerLock (0);
    g_dwTickDebuggerStart = CurMSec;
    if (g_nCpuReady > 1) {

        if (1==InterlockedIncrement(&g_nCpuStopped)) {

            DoSendIPI (IPI_TYPE_ALL_BUT_SELF, 0, IPI_STOP_CPU, 0);
        }
    }
}

void ResumeAllOtherCPUs (void)
{
    if (g_nCpuReady > 1) {
        
        if (0==InterlockedDecrement(&g_nCpuStopped)) {
            LONG idx;
            for (idx = 1; idx < g_nCpuReady; idx ++) {
                MDSignal ();
            }
            // wait till all resumed
            while (g_nCpuIpiDone) {
                MDSpin ();
            }
        }
    }
    g_dwTimeInDebugger += CurMSec - g_dwTickDebuggerStart;
    ReleaseSchedulerLock (0);
}


DWORD HandleIpi (void)
{
    DWORD   sysintr = SYSINTR_NOP;
    PPCB    ppcb = GetPCB ();
    DWORD   curIPICommand;
    PTHREAD pth;

    // if we get IPI after the cpu is powered off, and before it's powered on, it'll be comeing from
    // OAL doing "simulated power off". Ignore IPI if that is the case.

    switch (ppcb->dwCpuState) {
    case CE_PROCESSOR_STATE_POWERED_OFF:
    case PROCESSOR_STATE_POWERING_ON:
        break;

    case PROCESSOR_STATE_READY_TO_POWER_OFF:
        // cpu ready to be powered off - ignore all IPI and return SYSINTR_NOP
        InterlockedIncrement (&g_nCpuIpiDone);
        MDSignal ();
        break;
        
    default:
        DataMemoryBarrier ();

        pth  = ppcb->pthSched;   // NOTE: Don't use pCurThread for it can be pointing to 
                                 //       a thread that had already exited
        switch (g_dwIpiCommand) {
        case IPI_RESCHEDULE:
            if (!pth                        // idle?
                || g_dwIpiData              // forced reschedule? 
                || PriorityLowerThanNode (FirstRunnableNode(ppcb), GET_CPRIO (pth))   // higher prio thread runnable?
                || ((int) (pth->dwQuantLeft + ppcb->dwPrevReschedTime - CurMSec) <= 0)) {   // quantum expired?
                SetReschedule (ppcb);
                sysintr = SYSINTR_RESCHED;
            }
            break;

        case IPI_INVALIDATE_VM:
            if ((PPROCESS) g_dwIpiData == ppcb->pVMPrc) {
                PcbSetVMProc (g_pprcNK);
                MDSwitchVM (g_pprcNK);
            }
            break;

        case IPI_SUSPEND_THREAD:
        case IPI_TERMINATE_THREAD:
            if ((PTHREAD) g_dwIpiData == pth) {
                if (IPI_TERMINATE_THREAD != ppcb->wIPIOperation) {  // terminate supercede suspend
                    ppcb->wIPIOperation = (WORD) g_dwIpiCommand;
                }
                SetReschedule (ppcb);
                sysintr = SYSINTR_RESCHED;
            }
            break;
            
        default:
            g_pOemGlobal->pfnIpiHandler (g_dwIpiCommand, g_dwIpiData);
            break;
        }

        // copy command to local storage in case it is changed by another IPI
        // between here and point (*) below
        curIPICommand = g_dwIpiCommand;

        InterlockedIncrement (&g_nCpuIpiDone);

        MDSignal ();
        
        // point (*) referenced above
        if (IPI_STOP_CPU == curIPICommand) 
        {
            NKCacheRangeFlush(NULL, 0, CACHE_SYNC_ALL | CSF_CURR_CPU_ONLY);
            // CPU "stopped" in the following spinning loop
            while (g_nCpuStopped) {
                MDSpin ();
            }
            HDStopAllCpuCB();
            InterlockedDecrement (&g_nCpuIpiDone);
            MDSignal ();
            NKCacheRangeFlush(NULL, 0, CACHE_SYNC_ALL | CSF_CURR_CPU_ONLY);
        }
    }
    return sysintr;
}

BOOL SCHL_PowerOffCPU (DWORD dwProcessor, DWORD dwHint)
{
    PTHREAD pCurTh              = pCurThread;
    DWORD   dwSaveAffinity      = pCurTh->dwAffinity;
    PPCB    ppcb                = g_ppcbs[dwProcessor-1];
    PPCB    pCurPCB             = g_ppcbs[0];               // run on master CPU while powering off other CPUs.
    PTWO_D_QUEUE pNewRunQ       = &pCurPCB->affinityQ;
    BOOL    fRet;

    DEBUGCHK ((dwProcessor > 1) && (dwProcessor <= g_pKData->nCpus));
    DEBUGCHK (CE_PROCESSOR_STATE_POWERED_ON == ppcb->dwCpuState);
    
    // set affinity to master CPU
    SCHL_SetThreadAffinity (pCurTh, 1);

    // change the CPU state of the destination CPU to PROCESSOR_STATE_POWERING_OFF and force
    // a reschedule on all cores. 
    // NOTE: 1) we could theoretically send IPI to the target CPU only. However, as this is a very 
    //       rare operation and performance isn't a concern here, we do this for simplicity.
    //       2) Sending IPI to specific CPU doesn't seem to work reliably, using ALL_BUT_SELF seems
    //       to work all the time.
    AcquireSchedulerLock (0);
    ppcb->dwCpuState = PROCESSOR_STATE_POWERING_OFF;
    pCurPCB->wIPIPending = 2;
    ReleaseSchedulerLock (0);

    // wait till the CPU is ready to be powered off
    while (PROCESSOR_STATE_READY_TO_POWER_OFF != ((volatile PCB *)ppcb)->dwCpuState) {
        MDSpin ();
    }

    // tell OAL to power off CPU
    AcquireSchedulerLock (0);
    fRet = g_pOemGlobal->pfnMpCpuPowerFunc (ppcb->dwHardwareCPUId, FALSE, dwHint);

    if (fRet) {
        // CPU is powered off, update # of CPU ready
        InterlockedDecrement (&g_nCpuReady);
        ppcb->dwCpuState = CE_PROCESSOR_STATE_POWERED_OFF;
    } else {
        // failed to power off the CPU, resume it. At this point,
        // the CPU is spinning on dwCpuState, set it to CE_PROCESSOR_STATE_POWERED_ON
        // to resume the CPU
        ppcb->dwCpuState = CE_PROCESSOR_STATE_POWERED_ON;
    }
    ReleaseSchedulerLock (0);

    if (fRet) {
        PTWO_D_QUEUE pAffinityQ = &ppcb->affinityQ;
        DWORD       cprio = GET_CPRIO(pCurTh);
        PTWO_D_NODE pNode;
        PTHREAD     pth;

        // CPU is successfully powered off, 
        // move all threads in the affinity queue of the CPU to be power off to another run queue
        AcquireSchedulerLock (0);
        pNode = pAffinityQ->pHead;
        if (pNode) {
            if (cprio > pNode->prio) {
                SetReschedule (pCurPCB);
            }
            do {
                PrioDequeue (pAffinityQ, pNode, pNode->prio);
                pth             = THREAD_OF_QNODE (pNode);
                pth->dwAffinity = 1;
                pth->pRunQ      = pNewRunQ;
                PrioEnqueue (pNewRunQ, pNode, TRUE);
                pNode = pAffinityQ->pHead;
            } while (pNode);
        }
        ReleaseSchedulerLock (0);
    }

    // restore thread affinity of current thread
    if (dwProcessor != dwSaveAffinity) {
        SCHL_SetThreadAffinity (pCurTh, dwSaveAffinity);
    }
    return fRet;
}

BOOL SCHL_PowerOnCPU (DWORD dwProcessor) 
{
    BOOL fRet;
    PPCB ppcb = g_ppcbs[dwProcessor-1];
    DEBUGCHK ((dwProcessor > 1) && (dwProcessor <= g_pKData->nCpus));
    DEBUGCHK (CE_PROCESSOR_STATE_POWERED_OFF == ppcb->dwCpuState);
    
    AcquireSchedulerLock (0);
    g_fStartMP = dwProcessor;
    ppcb->dwCpuState = PROCESSOR_STATE_POWERING_ON;
#if defined (ARM) || defined (x86)
    NKPowerOnWriteBarrier();
#else
    DataSyncBarrier();
#endif
    DEBUGMSG (ZONE_SCHEDULE, (L"CPU %d is powering on\r\n", ppcb->dwCpuId));
    fRet = g_pOemGlobal->pfnMpCpuPowerFunc (ppcb->dwHardwareCPUId, TRUE, 0);
    if (!fRet) {
        ppcb->dwCpuState = CE_PROCESSOR_STATE_POWERED_OFF;
    } else {
        // wait until the CPU to be powered on gets out of idle.
        while (((volatile PCB *)ppcb)->fIdle) {
            MDSpin ();
        }
        ppcb->dwCpuState = CE_PROCESSOR_STATE_POWERED_ON;
        InterlockedIncrement (&g_nCpuReady);
    }

    ReleaseSchedulerLock (0);

    return fRet;
}

void NKIdle (DWORD dwIdleParam)
{
    PPCB ppcb = GetPCB ();
    DEBUGCHK (InPrivilegeCall () && !ppcb->ownspinlock);

    ppcb->fIdle = TRUE;
    if (PROCESSOR_STATE_POWERING_OFF == ppcb->dwCpuState) {
        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_INSTRUCTIONS|CSF_CURR_CPU_ONLY);
        SetCPUASID (&dummyThread);
        ppcb->dwCpuState = PROCESSOR_STATE_READY_TO_POWER_OFF;
        MDSignal ();

        DEBUGMSG (ZONE_SCHEDULE, (L"CPU %d is powering off\r\n", ppcb->dwCpuId));
        // wait till OAL is notified that the CPU needs to be powered off

        // NOTE: need to turn interrupts on before start spinning, otherwise we will not be able to respond
        //       to IPI and cause the system to hang.
        INTERRUPTS_ON ();
        while (PROCESSOR_STATE_READY_TO_POWER_OFF == ((volatile PCB *)ppcb)->dwCpuState) {
            DataMemoryBarrier();
        }
        INTERRUPTS_OFF ();
        DEBUGMSG (ZONE_SCHEDULE, (L"CPU %d is ready to be powered off\r\n", ppcb->dwCpuId));
    }
    // If this core spends too much time in while (PROCESSOR_STATE_READY_TO_POWER_OFF == ...
    // (the while loop above) processing IPIs; wake up IPI (from OAL) could have occurred already.
    // If that occurs, we do not want to go into the idle state.
    if (PROCESSOR_STATE_POWERING_ON != ((volatile PCB *)ppcb)->dwCpuState) {
        do {
            if (g_pOemGlobal->pfnIdleEx) {
                g_pOemGlobal->pfnIdleEx (&ppcb->liIdleTime);
            } else {
                OEMIdle (dwIdleParam);
                ppcb->liIdleTime.QuadPart = g_pNKGlobal->liIdle.QuadPart;
            }
            DataMemoryBarrier();
        } while (CE_PROCESSOR_STATE_POWERED_OFF == ((volatile PCB *)ppcb)->dwCpuState);
    }

    ppcb->fIdle = FALSE;
    if (PROCESSOR_STATE_POWERING_ON == ppcb->dwCpuState) {
        
        // power on is done by returning from OEMIdle
        // we must acquire the scheduler lock around setting g_nCpuReady
        // since changing it in the middle of an IPI by another processor 
        // will hang the system.
        MDSignal ();
    }
}


